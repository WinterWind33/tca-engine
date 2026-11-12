// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <mutex>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <d3d12.h>

// Rendering
#include "rendering/rendering-queues/image-render-queue-base.hpp"
#include "rendering/win32/directx-12/directx12-rendering-manager.hpp"

namespace tca::engine::rendering::win32::directx12 {

/**
 * @brief DirectX 12 implementation of image_render_queue. Uploads decoded images as GPU textures
 * using the shader resource view (SRV) descriptor heap already owned by the
 * directx12_rendering_manager (the very same heap ImGui's own font texture is allocated from), and
 * hands back the texture identifier needed to draw them (drawing itself is the image class's job,
 * not this queue's - see image.hpp).
 *
 * @note enqueue_image()/dequeue_image() are safe to call concurrently from multiple threads
 * (including off the thread that owns this queue). The actual GPU upload (device/command queue
 * calls) runs unsynchronized, as ID3D12Device and ID3D12CommandQueue are free-threaded per the
 * D3D12 contract; a mutex only guards the base class's texture map/handle counter (see
 * image_render_queue_base) and every call into descriptor_heap_allocator (a plain
 * std::stack-based free list, see descriptor-heap-allocator.hpp, with no synchronization of its
 * own).
 */
class dx12_image_render_queue final : public image_render_queue_base {
public:
    /**
     * @brief Construct a new dx12 image render queue object bound to the given rendering manager.
     * The rendering manager must outlive this queue.
     *
     * @param renderingManager The DirectX 12 rendering manager owning the device, command queue,
     * and SRV descriptor heap allocator used to upload and register textures.
     */
    explicit dx12_image_render_queue(directx12_rendering_manager& renderingManager) noexcept;

    enqueued_image enqueue_image(platform::images::decoded_image image) override;
    void dequeue_image(image_handle handle) noexcept override;

private:
    directx12_rendering_manager& m_renderingManager;

    // Guards the base class's texture map/handle counter and every call into this queue's
    // descriptor_heap_allocator (see the class-level @note above). Not held across the actual GPU
    // upload/wait calls, which are free-threaded on their own and would otherwise serialize
    // uploads from different threads for no reason.
    std::mutex m_bookkeepingMutex{};
};

} // namespace tca::engine::rendering::win32::directx12
