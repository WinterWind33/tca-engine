// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/directx-12/dx12-image-render-queue.hpp"

// C++ STL
#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <gsl/gsl>

namespace tca::engine::rendering::win32::directx12 {

namespace {

constexpr DXGI_FORMAT TEXTURE_FORMAT{DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM};
constexpr std::uint32_t BYTES_PER_PIXEL{4};

// --- Background for readers unfamiliar with DirectX 12 ---
//
// DX12 never lets you touch the GPU "directly" from the CPU. Instead, everything works like
// this:
//   1. You describe work you want the GPU to do by recording it into a "command list" (think of
//      it as a to-do list of GPU instructions: "copy this data here", "transition this resource
//      to that state", etc.). Recording does NOT execute anything yet.
//   2. Command lists are allocated out of a "command allocator" (the actual memory backing the
//      recorded commands lives there).
//   3. You submit ("execute") a closed command list on a "command queue". Only then does the GPU
//      actually start doing the work - and it does so asynchronously, in parallel with the CPU.
//   4. Because the GPU runs independently of the CPU, you need a "fence" to know when the GPU has
//      actually finished a piece of work: you ask the queue to "signal" the fence to some value
//      once it reaches that point in the command stream, and the CPU can wait until the fence
//      reaches that value.
//
// A GPU texture must also live in a specific kind of memory ("heap"):
//   - a DEFAULT heap is fast for the GPU to read from (what shaders use), but the CPU cannot
//     write into it directly.
//   - an UPLOAD heap is CPU-writable (you can memcpy into it), but slower for the GPU to read
//     repeatedly.
// So uploading pixel data to a texture is a two-step dance: write the pixels into a temporary
// UPLOAD heap resource from the CPU, then ask the GPU to copy that into the real DEFAULT heap
// texture (a GPU-side copy, recorded as a command and executed like any other GPU work).
//
// Resources also have a "state" (e.g. "this is a copy destination", "this is being read by a
// shader") that must be explicitly declared and transitioned via "resource barriers" before using
// a resource a different way, so the GPU can synchronize memory access correctly.

// Waits, synchronously, for every command previously submitted to commandQueue to finish
// executing on the GPU, using a one-off fence (see the background comment above). Used to make
// texture uploads (a rare, user-triggered operation) simple and safe, at the cost of a small
// stall, rather than threading them through the rendering manager's per-frame fence bookkeeping.
void wait_for_command_queue_idle(const wil::com_ptr<ID3D12Device>& device,
                                 const wil::com_ptr<ID3D12CommandQueue>& commandQueue) {
    // A fence starts at 0. We'll ask the queue to set it to SIGNAL_VALUE (1) once all commands
    // submitted so far have finished executing on the GPU.
    wil::com_ptr<ID3D12Fence> fence{};
    THROW_IF_FAILED(
        device->CreateFence(0, D3D12_FENCE_FLAGS::D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    // A plain Win32 event object: the fence will wake this event up once it reaches the value we
    // ask it to wait for below, so the CPU can block on it instead of busy-polling the fence.
    wil::unique_handle fenceEvent{::CreateEvent(nullptr, FALSE, FALSE, nullptr)};
    THROW_LAST_ERROR_IF_NULL(fenceEvent.get());

    constexpr std::uint64_t SIGNAL_VALUE{1};
    // Tells the GPU: "once every command submitted to this queue so far has actually finished,
    // set the fence to SIGNAL_VALUE". This is queued as GPU work too - it doesn't happen the
    // instant this call returns, the GPU still has to catch up to it.
    THROW_IF_FAILED(commandQueue->Signal(fence.get(), SIGNAL_VALUE));
    // "Wake fenceEvent up once the fence reaches SIGNAL_VALUE" (this registers the wait; it does
    // not block by itself).
    THROW_IF_FAILED(fence->SetEventOnCompletion(SIGNAL_VALUE, fenceEvent.get()));
    // Actually blocks the CPU here until the GPU has caught up and signaled the fence.
    THROW_LAST_ERROR_IF(::WaitForSingleObject(fenceEvent.get(), INFINITE) != WAIT_OBJECT_0);
}

// Uploads a decoded image's pixels into a newly created default-heap D3D12 texture resource,
// executing the upload on the given command queue and waiting for it to complete before
// returning. See the background comment above for the overall "upload heap -> GPU copy ->
// default heap" idea this function implements.
[[nodiscard]] wil::com_ptr<ID3D12Resource> create_and_upload_texture(
    const wil::com_ptr<ID3D12Device>& device, const wil::com_ptr<ID3D12CommandQueue>& commandQueue,
    const platform::images::decoded_image& image) {
    // Step 1: describe the texture we want (a plain 2D texture, one mip level, RGBA8, no
    // multisampling) and create it in the fast, GPU-only DEFAULT heap. Its initial state is
    // COPY_DEST because the very first thing that will happen to it is receiving a copy from the
    // upload buffer below - every D3D12 resource must start in a state that matches how it's about
    // to be used.
    const D3D12_RESOURCE_DESC textureDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = image.width,
        .Height = image.height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = TEXTURE_FORMAT,
        .SampleDesc{.Count = 1},
        .Layout = D3D12_TEXTURE_LAYOUT::D3D12_TEXTURE_LAYOUT_UNKNOWN,
    };

    constexpr D3D12_HEAP_PROPERTIES DEFAULT_HEAP_PROPERTIES{
        .Type = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT};

    wil::com_ptr<ID3D12Resource> textureResource{};
    THROW_IF_FAILED(device->CreateCommittedResource(
        &DEFAULT_HEAP_PROPERTIES, D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&textureResource)));

    // Step 2: ask the device how the texture's data needs to be laid out for a CPU-to-GPU copy.
    // GPUs require each row of a texture to start at an address aligned to
    // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256 bytes) inside the upload buffer - which is usually
    // NOT the same as our tightly-packed decoded_image row width (image.width * 4 bytes). This
    // call computes the padded "footprint" (including the required, possibly-larger, per-row byte
    // count, i.e. RowPitch) and the total upload buffer size we need to allocate to hold it.
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT numRows{};
    UINT64 rowSizeInBytes{};
    UINT64 totalUploadBufferSize{};
    device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes,
                                  &totalUploadBufferSize);

    // Step 3: create the CPU-writable UPLOAD heap buffer that will temporarily hold the pixel
    // data before the GPU copies it into the real texture. Its initial state is GENERIC_READ,
    // which is the state D3D12 requires for anything the GPU will read from an upload heap.
    constexpr D3D12_HEAP_PROPERTIES UPLOAD_HEAP_PROPERTIES{
        .Type = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_UPLOAD};
    const D3D12_RESOURCE_DESC uploadBufferDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = totalUploadBufferSize,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .SampleDesc{.Count = 1},
        .Layout = D3D12_TEXTURE_LAYOUT::D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    };

    wil::com_ptr<ID3D12Resource> uploadResource{};
    THROW_IF_FAILED(device->CreateCommittedResource(
        &UPLOAD_HEAP_PROPERTIES, D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE, &uploadBufferDesc,
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&uploadResource)));

    // Step 4: this is the one and only point where we're allowed to use plain CPU memcpy - into
    // the upload buffer, which is CPU-visible. Map() gives us a raw CPU pointer to it.
    // We copy row by row instead of one big memcpy, because the destination's row stride
    // (footprint.Footprint.RowPitch, GPU-aligned, from step 2) can be wider than our source's
    // tightly-packed row stride (sourceRowPitch): the extra padding bytes at the end of each
    // destination row are simply left uninitialized, which is fine since the GPU won't read past
    // each row's real pixel data when sampling the texture.
    {
        void* mappedData{nullptr};
        constexpr D3D12_RANGE NO_READ_RANGE{};
        THROW_IF_FAILED(uploadResource->Map(0, &NO_READ_RANGE, &mappedData));

        gsl::not_null<std::byte*> destination{static_cast<std::byte*>(mappedData)};
        const gsl::not_null<const std::byte*> source{image.pixels.data()};
        const std::size_t sourceRowPitch{static_cast<std::size_t>(image.width) * BYTES_PER_PIXEL};

        for (UINT row{}; row < numRows; ++row) {
            std::memcpy(destination.get() + (row * footprint.Footprint.RowPitch),
                        source.get() + (row * sourceRowPitch), sourceRowPitch);
        }

        uploadResource->Unmap(0, nullptr);
    }

    // Step 5: create a one-off command allocator (the memory backing our command list) and a
    // command list (the actual "to-do list" of GPU commands we're about to record). These are
    // separate from the rendering manager's own per-frame command list/allocators used to draw
    // every frame, since a texture upload is a rare, out-of-band operation, not part of the
    // regular per-frame rendering loop.
    wil::com_ptr<ID3D12CommandAllocator> commandAllocator{};
    THROW_IF_FAILED(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));

    wil::com_ptr<ID3D12GraphicsCommandList> commandList{};
    THROW_IF_FAILED(
        device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  commandAllocator.get(), nullptr, IID_PPV_ARGS(&commandList)));

    // Step 6: record a "copy this region from the upload buffer into the texture" command. This
    // is the actual GPU-side copy that moves the pixel data from the slow-but-CPU-writable upload
    // buffer into the fast-but-CPU-invisible default-heap texture created in step 1. Nothing runs
    // yet - we're still just recording instructions into commandList.
    const D3D12_TEXTURE_COPY_LOCATION copyDestination{
        .pResource = textureResource.get(),
        .Type = D3D12_TEXTURE_COPY_TYPE::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0,
    };
    const D3D12_TEXTURE_COPY_LOCATION copySource{
        .pResource = uploadResource.get(),
        .Type = D3D12_TEXTURE_COPY_TYPE::D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .PlacedFootprint = footprint,
    };
    commandList->CopyTextureRegion(&copyDestination, 0, 0, 0, &copySource, nullptr);

    // Step 7: record a "resource barrier". The texture was created in the COPY_DEST state (it was
    // about to receive the copy above); now that the copy is recorded, we must transition it to
    // PIXEL_SHADER_RESOURCE, the state a texture needs to be in to be read by a shader (which is
    // exactly what happens when ImGui draws it later). Forgetting this step, or getting the
    // before/after states wrong, is a very common source of GPU validation errors/crashes in
    // D3D12 code.
    const D3D12_RESOURCE_BARRIER barrier{
        .Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Transition{
                    .pResource = textureResource.get(),
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
                    .StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    },
    };
    commandList->ResourceBarrier(1, &barrier);
    // Command lists must be "closed" before they can be submitted; this locks in the recording.
    THROW_IF_FAILED(commandList->Close());

    // Step 8: submit ("execute") the recorded command list on the queue. This is the moment the
    // GPU actually starts doing the copy and the state transition - everything before this line
    // was just building a list of instructions, not doing any GPU work.
    const std::array<ID3D12CommandList*, 1> commandLists{
        static_cast<ID3D12CommandList*>(commandList.get())};
    commandQueue->ExecuteCommandLists(1, commandLists.data());

    // Step 9: wait for the GPU to actually finish that copy before returning. This matters
    // because uploadResource, commandAllocator and commandList are all local variables that are
    // about to be destroyed when this function returns (their COM ref-counts drop to zero) - we
    // must be certain the GPU is done reading/using them first, otherwise we'd free memory the
    // GPU might still be actively copying from.
    wait_for_command_queue_idle(device, commandQueue);

    return textureResource;
}

} // namespace

dx12_image_render_queue::dx12_image_render_queue(
    directx12_rendering_manager& renderingManager) noexcept
    : m_renderingManager{renderingManager} {}

enqueued_image dx12_image_render_queue::enqueue_image(const platform::images::decoded_image image) {
    const auto& device{m_renderingManager.get_device()};

    // Uploads the pixels and gets back the finished, GPU-resident texture (see
    // create_and_upload_texture above for the full step-by-step upload process).
    wil::com_ptr<ID3D12Resource> textureResource{
        create_and_upload_texture(device, m_renderingManager.get_command_queue(), image)};

    // A shader (and ImGui's rendering backend, which is really just a shader under the hood)
    // cannot bind a texture resource directly - it reads it through a "descriptor": a small,
    // fixed-size chunk of GPU-readable metadata describing how to interpret and access the
    // resource, living in a "descriptor heap" (essentially an array of these descriptors). Here we
    // reserve one free slot in the rendering manager's shared descriptor heap - the very same heap
    // ImGui's own font texture descriptor lives in.
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
    {
        // descriptor_heap_allocator's free list has no synchronization of its own (see
        // descriptor-heap-allocator.hpp), so concurrent allocate()/free() calls from different
        // threads must be serialized here.
        const std::scoped_lock lock{m_bookkeepingMutex};
        m_renderingManager.get_srv_descriptor_heap_allocator().allocate(cpuHandle, gpuHandle);
    }

    // Fills that descriptor slot with a "Shader Resource View" (SRV): the specific kind of
    // descriptor that says "this is a 2D texture, in this pixel format, readable by a shader".
    // cpuHandle is where the device writes the descriptor's data (a CPU-side address into the
    // heap); gpuHandle is the address a shader uses at draw time to find that same descriptor
    // from the GPU's point of view - both point at the same slot, just from each side.
    const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
        .Format = TEXTURE_FORMAT,
        .ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D{.MipLevels = 1},
    };
    device->CreateShaderResourceView(textureResource.get(), &srvDesc, cpuHandle);

    // ImGui's DirectX 12 backend represents a drawable texture as an ImTextureID - in practice
    // just the numeric value of the GPU descriptor handle above, reinterpreted as an opaque ID.
    // This is the exact same convention ImGui's own font texture uses internally.
    const ImTextureID textureId{static_cast<ImTextureID>(gpuHandle.ptr)};

    // gpu_texture (see gpu-texture.hpp) is a simple RAII wrapper: it stores the texture id/size,
    // plus a "releaser" callback that undoes everything this function just did, invoked exactly
    // once when the texture is no longer needed (see image_render_queue_base::dequeue_image()).
    // The lambda captures
    // the rendering manager (to reach the same descriptor allocator again to free the slot) and
    // takes ownership of the ID3D12Resource com_ptr itself, so the GPU resource stays alive for as
    // long as this gpu_texture (and thus this lambda) does, and gets released exactly when the
    // lambda runs.
    // The releaser below is invoked from dequeue_image() (see the override further down), which
    // already holds m_bookkeepingMutex around the whole erase - so free() here runs already
    // synchronized and must not try to re-acquire the mutex itself.
    const gsl::not_null<directx12_rendering_manager*> renderingManager{&m_renderingManager};
    gpu_texture texture{
        textureId, image.width, image.height,
        [renderingManager, cpuHandle, gpuHandle, resource = std::move(textureResource)]() mutable {
            // Uploads only happen on rare, user-triggered action, so a synchronous stall here to
            // guarantee the GPU is no longer referencing this texture before releasing it is an
            // acceptable trade-off for simplicity.
            renderingManager->wait_for_pending_operations();
            renderingManager->get_srv_descriptor_heap_allocator().free(cpuHandle, gpuHandle);
            resource.reset();
        }};

    // register_texture() touches the base class's texture map/handle counter, which is also
    // unsynchronized on its own - see the class-level @note.
    const std::scoped_lock lock{m_bookkeepingMutex};
    return register_texture(std::move(texture));
}

void dx12_image_render_queue::dequeue_image(const image_handle handle) noexcept {
    const std::scoped_lock lock{m_bookkeepingMutex};
    image_render_queue_base::dequeue_image(handle);
}

} // namespace tca::engine::rendering::win32::directx12
