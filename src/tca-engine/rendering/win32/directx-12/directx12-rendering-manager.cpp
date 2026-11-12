// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/directx-12/directx12-rendering-manager.hpp"

// C++ STL
#include <array>
#include <memory>
#include <span>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <d3d12.h>
#include <dxgi1_5.h>
#include <dxgidebug.h>

#include <wil/result.h>
#include <gsl/gsl>

#include "rendering/rendering-logger.hpp"

namespace tca::engine::rendering::win32::directx12 {

namespace {

#ifdef _DEBUG
constexpr bool ENABLE_DEBUG_LAYER{true};
#else
constexpr bool ENABLE_DEBUG_LAYER{false};
#endif

// [[maybe_unused]]: its only call sits behind `if constexpr (ENABLE_DEBUG_LAYER)`, so release
// builds discard it and clang would otherwise fail them with -Wunneeded-internal-declaration.
[[maybe_unused]] void enable_debug_layer() {
    wil::com_ptr<ID3D12Debug> debugInterface{};
    THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();

    get_rendering_logger()->info("DirectX 12 debug layer enabled");
}

DXGI_SWAP_CHAIN_DESC1 create_swap_chain_desc(const rendering_config& config) {
    return DXGI_SWAP_CHAIN_DESC1{
        .Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc =
            {
                         .Count = 1,
                         },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = gsl::narrow_cast<std::uint32_t>(config.back_buffers_count),
        .Scaling = DXGI_SCALING::DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE::DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT,
    };
}

void create_rtv_descriptor_heap(
    const wil::com_ptr<ID3D12Device>& device, const rendering_config& config,
    wil::com_ptr<ID3D12DescriptorHeap>& rtvDescriptorHeap,
    const std::span<D3D12_CPU_DESCRIPTOR_HANDLE> rtvCpuDescriptorHandles) {
    const D3D12_DESCRIPTOR_HEAP_DESC desc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = gsl::narrow_cast<std::uint32_t>(config.back_buffers_count),
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 1,
    };

    THROW_IF_FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvDescriptorHeap)));

    const std::size_t rtvDescriptorSize{
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)};

    for (D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{
             rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart()};
         auto& rtvCpuDescriptorHandle : rtvCpuDescriptorHandles) {
        rtvCpuDescriptorHandle = rtvHandle;
        rtvHandle.ptr += rtvDescriptorSize;
    }
}

void create_srv_descriptor_heap(
    const wil::com_ptr<ID3D12Device>& device, const rendering_config& config,
    wil::com_ptr<ID3D12DescriptorHeap>& srvDescriptorHeap,
    std::unique_ptr<descriptor_heap_allocator>& descriptorHeapAllocator) {
    const D3D12_DESCRIPTOR_HEAP_DESC desc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = gsl::narrow_cast<std::uint32_t>(config.srv_heap_size),
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 1,
    };

    THROW_IF_FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvDescriptorHeap)));

    heap_allocator_config heapAllocatorConfig{
        .heap_type = desc.Type,
        .heap_handle_increment = device->GetDescriptorHandleIncrementSize(desc.Type),
        .num_descriptors = gsl::narrow_cast<std::size_t>(desc.NumDescriptors),
        .heap_start_cpu = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        .heap_start_gpu = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
    };

    descriptorHeapAllocator =
        std::make_unique<descriptor_heap_allocator>(std::move(heapAllocatorConfig));
}

void create_command_queue(const wil::com_ptr<ID3D12Device>& device,
                          wil::com_ptr<ID3D12CommandQueue>& commandQueue) {
    const D3D12_COMMAND_QUEUE_DESC desc{
        .Type = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Flags = D3D12_COMMAND_QUEUE_FLAGS::D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 1,
    };

    THROW_IF_FAILED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));
}

void create_command_allocators(const wil::com_ptr<ID3D12Device>& device,
                               const std::span<frame_context> frameContexts) {
    for (auto& frameContext : frameContexts) {
        THROW_IF_FAILED(
            device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           IID_PPV_ARGS(&frameContext.command_allocator)));
    }
}

bool check_tearing_support(const wil::com_ptr<IDXGIFactory5>& dxgiFactory) {
    BOOL allowTearing{};
    THROW_IF_FAILED(dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                     &allowTearing, sizeof(allowTearing)));
    return allowTearing;
}

void create_swap_chain(const wil::com_ptr<ID3D12CommandQueue>& commandQueue,
                       const rendering_config& config,
                       const directx12_rendering_manager::window_type& window,
                       bool& isTearingSupported, wil::com_ptr<IDXGISwapChain4>& swapChain,
                       wil::unique_handle& swapChainWaitableObject) {
    wil::com_ptr<IDXGIFactory5> dxgiFactory{};
    THROW_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));

    // Check for tearing support. Tearing support is required for variable refresh rate displays,
    // but we will use it if it's available regardless of the display's capabilities, as it allows
    // for potentially smoother presentation and reduced latency.
    isTearingSupported = check_tearing_support(dxgiFactory);
    auto swapChainDesc{create_swap_chain_desc(config)};
    if (isTearingSupported) {
        swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    wil::com_ptr<IDXGISwapChain1> swapChain1{};
    THROW_IF_FAILED(
        dxgiFactory->CreateSwapChainForHwnd(commandQueue.get(), window.get_native_handle().get(),
                                            &swapChainDesc, nullptr, nullptr, &swapChain1));
    THROW_IF_FAILED(swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain)));
    THROW_IF_FAILED(dxgiFactory->MakeWindowAssociation(window.get_native_handle().get(),
                                                       DXGI_MWA_NO_ALT_ENTER));

    THROW_IF_FAILED(swapChain->SetMaximumFrameLatency(
        gsl::narrow_cast<std::uint32_t>(config.back_buffers_count)));
    swapChainWaitableObject.reset(swapChain->GetFrameLatencyWaitableObject());
    THROW_LAST_ERROR_IF_NULL(swapChainWaitableObject.get());
}

} // namespace

directx12_rendering_manager::directx12_rendering_manager(
    const gsl::not_null<const window_type*> window, const rendering_config config)
    : m_window{window},
      m_config{config} {
    get_rendering_logger()->info("Initializing DirectX 12 resources");
    get_rendering_logger()->info(
        "DirectX 12 rendering configuration: frames_in_flight_count={}, back_buffers_count={}, "
        "srv_heap_size={}",
        config.frames_in_flight_count, config.back_buffers_count, config.srv_heap_size);
    // Allocate graphics resources and objects.
    // May throw on failure.
    create_device();
}

directx12_rendering_manager::~directx12_rendering_manager() noexcept {
    // Wait for any pending GPU operations to complete before releasing resources, to ensure that
    // all resources are no longer in use by the GPU.
    wait_for_pending_operations();

    // After this, all resources are automatically released in the correct order thanks to the use
    // of smart pointers and RAII.
}

void directx12_rendering_manager::create_device() {
    constexpr D3D_FEATURE_LEVEL FEATURE_LEVEL{D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_0};

    if constexpr (ENABLE_DEBUG_LAYER) {
        // We need to call this before creating the device otherwise the debug layer won't be
        // enabled for the device creation itself, and it might throw because it does not support
        // ID3D12InfoQueue, which is required for our debug layer setup.
        enable_debug_layer();
    }

    THROW_IF_FAILED(D3D12CreateDevice(nullptr, FEATURE_LEVEL, IID_PPV_ARGS(&m_device)));

    if constexpr (ENABLE_DEBUG_LAYER) {
        wil::com_ptr<ID3D12InfoQueue> infoQueue{};
        THROW_IF_FAILED(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

        // Setup debug interface to break on any warnings/errors.
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY::D3D12_MESSAGE_SEVERITY_ERROR, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY::D3D12_MESSAGE_SEVERITY_CORRUPTION,
                                      true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY::D3D12_MESSAGE_SEVERITY_WARNING, true);
    }

    // Create render target view (RTV) descriptor heap and descriptor handles.
    m_renderTargetCpuDescriptorHandles.resize(m_config.back_buffers_count);
    create_rtv_descriptor_heap(m_device, m_config, m_rtvDescriptorHeap,
                               m_renderTargetCpuDescriptorHandles);

    create_srv_descriptor_heap(m_device, m_config, m_srvDescriptorHeap, m_descriptorHeapAllocator);
    // The SRV descriptor heap allocator should be initialized after the previous call.
    Ensures(m_descriptorHeapAllocator);

    create_command_queue(m_device, m_commandQueue);

    m_frameContexts.resize(m_config.frames_in_flight_count);
    create_command_allocators(m_device, m_frameContexts);

    THROW_IF_FAILED(m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_frameContexts[0].command_allocator.get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    THROW_IF_FAILED(m_commandList->Close());

    THROW_IF_FAILED(
        m_device->CreateFence(0, D3D12_FENCE_FLAGS::D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

    m_fenceEvent.reset(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    THROW_LAST_ERROR_IF_NULL(m_fenceEvent);

    create_swap_chain(m_commandQueue, m_config, *m_window, m_isTearingSupported, m_swapChain,
                      m_swapChainWaitableObject);

    create_render_target();
}

void directx12_rendering_manager::create_render_target() {
    for (std::size_t i{}; i < m_config.back_buffers_count; ++i) {
        wil::com_ptr<ID3D12Resource> backBuffer{};
        THROW_IF_FAILED(
            m_swapChain->GetBuffer(gsl::narrow_cast<std::uint32_t>(i), IID_PPV_ARGS(&backBuffer)));
        m_device->CreateRenderTargetView(backBuffer.get(), nullptr,
                                         m_renderTargetCpuDescriptorHandles[i]);
        m_renderTargetResources.push_back(std::move(backBuffer));
    }
}

void directx12_rendering_manager::wait_for_pending_operations() noexcept {
    Expects(m_fence);
    Expects(m_commandQueue);

    // Cannot throw because it may be used during objects destruction.
    try {
        const auto logger{get_rendering_logger()};

        const auto nextFenceValue{m_fenceLastSignaledValue + 1};

        if (const auto hr{m_commandQueue->Signal(m_fence.get(), nextFenceValue)}; FAILED(hr)) {
            logger->error("Failed to signal the command queue. HR={:#X}", hr);
            return;
        }

        m_fenceLastSignaledValue = nextFenceValue;

        if (const auto hr{m_fence->SetEventOnCompletion(nextFenceValue, m_fenceEvent.get())};
            FAILED(hr)) {
            logger->error("Failed to set event on fence completion. HR={:#X}", hr);
            return;
        }

        if (const auto res{WaitForSingleObject(m_fenceEvent.get(), INFINITE)};
            res != WAIT_OBJECT_0) {
            logger->error("Failed to wait for fence event. Result={}, LastError={}", res,
                          GetLastError());
            return;
        }
    } catch (...) {}
}

bool directx12_rendering_manager::is_window_occluded() const {
    Expects(m_swapChain);

    return (m_swapChainOccluded &&
            m_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) ||
           ::IsIconic(m_window->get_native_handle().get());
}

void directx12_rendering_manager::render(
    const std::function<void(const wil::com_ptr<ID3D12GraphicsCommandList>&)>&
        fillCommandListFunc) {
    auto& frameContext{wait_next_frame_context()};
    const auto backBufferIndex{m_swapChain->GetCurrentBackBufferIndex()};
    THROW_IF_FAILED(frameContext.command_allocator->Reset());

    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition{
                    .pResource = m_renderTargetResources[backBufferIndex].get(),
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
                    .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
                    },
    };

    THROW_IF_FAILED(m_commandList->Reset(frameContext.command_allocator.get(), nullptr));
    m_commandList->ResourceBarrier(1, &barrier);

    const auto clearColorFloat4{to_premultiplied_normalized_float4(m_clearColor)};
    m_commandList->ClearRenderTargetView(m_renderTargetCpuDescriptorHandles[backBufferIndex],
                                         clearColorFloat4.data(), 0, nullptr);
    m_commandList->OMSetRenderTargets(1, &m_renderTargetCpuDescriptorHandles[backBufferIndex],
                                      FALSE, nullptr);
    const std::array<ID3D12DescriptorHeap*, 1> descriptorHeaps{m_srvDescriptorHeap.get()};
    // Cannot pass &m_srvDescriptorHeap because the & operator sets the pointer to nullptr,
    // which causes DX12 to crash.
    m_commandList->SetDescriptorHeaps(1, descriptorHeaps.data());

    // ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
    if (fillCommandListFunc) {
        fillCommandListFunc(m_commandList);
    }
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);
    THROW_IF_FAILED(m_commandList->Close());

    const std::array<ID3D12CommandList*, 1> commandLists{
        static_cast<ID3D12CommandList*>(m_commandList.get())};

    // Same reason as above.
    m_commandQueue->ExecuteCommandLists(1, commandLists.data());
    THROW_IF_FAILED(m_commandQueue->Signal(m_fence.get(), ++m_fenceLastSignaledValue));
    frameContext.fence_value = m_fenceLastSignaledValue;

    // Present
    const auto hr{m_swapChain->Present(1, 0)}; // Present with vsync
    // HRESULT hr = m_swapChain->Present(0, m_isTearingSupported ? DXGI_PRESENT_ALLOW_TEARING
    // : 0); // Present without vsync
    m_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    m_currentFrameIndex++;
}

frame_context& directx12_rendering_manager::wait_next_frame_context() {
    frame_context& frameContext{
        m_frameContexts[m_currentFrameIndex % m_config.frames_in_flight_count]};

    if (m_fence->GetCompletedValue() < frameContext.fence_value) {
        if (const auto hr{
                m_fence->SetEventOnCompletion(frameContext.fence_value, m_fenceEvent.get())};
            FAILED(hr)) {
            get_rendering_logger()->error("Failed to set event on fence completion. HR={:#X}", hr);
        }

        const std::array<HANDLE, 2> waitableObjects{m_swapChainWaitableObject.get(),
                                                    m_fenceEvent.get()};
        if (const auto res{::WaitForMultipleObjects(2, waitableObjects.data(), TRUE, INFINITE)};
            res == WAIT_FAILED) {
            get_rendering_logger()->error("Failed to wait for frame context objects. LastError={}",
                                          ::GetLastError());
        }
    } else {
        if (const auto res{::WaitForSingleObject(m_swapChainWaitableObject.get(), INFINITE)};
            res == WAIT_FAILED) {
            get_rendering_logger()->error(
                "Failed to wait for swap chain waitable object. LastError={}", ::GetLastError());
        }
    }

    return frameContext;
}

void directx12_rendering_manager::on_window_size(const platform::win32::details::wparam_t wparam,
                                                 const platform::win32::details::lparam_t lparam) {
    if (m_device && wparam != SIZE_MINIMIZED) {
        // Reset render targets and wait for any pending GPU operations to complete before resizing
        // the swap chain, to ensure that all resources are no longer in use by the GPU.
        wait_for_pending_operations();
        m_renderTargetResources.clear();

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        std::ignore = m_swapChain->GetDesc1(&swapChainDesc);

        if (const auto hr{m_swapChain->ResizeBuffers(0, static_cast<std::uint32_t>(LOWORD(lparam)),
                                                     static_cast<std::uint32_t>(HIWORD(lparam)),
                                                     swapChainDesc.Format, swapChainDesc.Flags)};
            FAILED(hr)) {
            get_rendering_logger()->error("Failed to resize swap chain buffers. HR={:#X}", hr);
            return;
        }

        create_render_target();
    }
}
} // namespace tca::engine::rendering::win32::directx12
