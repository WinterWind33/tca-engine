// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>    // for std::size_t
#include <functional> // for std::function
#include <memory>
#include <vector>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <gsl/gsl>

// Core
#include "core/utilities/pointers.hpp"

// Platform
#include "platform/application-window.hpp"
#include "platform/win32/win32-application-window.hpp" // for lparam and wparam types

// Rendering
#include "rendering/colors.hpp"

#include "rendering/win32/directx-12/descriptor-heap-allocator.hpp"
#include "rendering/win32/directx-12/frame-context.hpp"

namespace tca::engine::rendering::win32::directx12 {

/**
 * @brief Configuration struct for the DirectX 12 rendering manager. This struct contains various
 * parameters that control the behavior of the rendering manager, such as the number of frames in
 * flight, the number of back buffers, and the size of the shader resource view (SRV) descriptor
 * heap.
 */
struct rendering_config final {
    /**
     * @brief Number of frames that can be in flight at the same time. This determines how many
     * command lists can be recorded and executed concurrently, and how many render targets are
     * needed for double or triple buffering. A common value for this is 2 or 3, depending on the
     * desired latency and performance characteristics.
     */
    std::size_t frames_in_flight_count{};

    /**
     * @brief Number of back buffers in the swap chain. This determines how many render targets are
     * used for presenting frames to the screen. A common value for this is 2 (double buffering) or
     * 3 (triple buffering). Note that the number of back buffers should be at least equal to the
     * frames in flight count to avoid stalling the GPU when waiting for a back buffer to become
     * available.
     */
    std::size_t back_buffers_count{};

    /**
     * @brief Size of the shader resource view (SRV) descriptor heap, in number of descriptors. This
     * determines how many SRV descriptors can be allocated from the heap for binding resources to
     * the pipeline. The required size of the SRV descriptor heap depends on the number of resources
     * that need to be bound at the same time, such as textures, buffers, and samplers. A common
     * value for this is 100 or more, depending on the complexity of the scene and the number of
     * resources used.
     */
    std::size_t srv_heap_size{};
};

/**
 * @brief The DirectX 12 rendering manager is responsible for managing the DirectX 12 device, swap
 * chain, command queue, and other related resources needed for rendering frames to a window. It
 * provides an interface for rendering frames, setting the clear color, and handling window resizing
 * events. The rendering manager also manages a descriptor heap allocator for shader resource views
 * (SRVs) and keeps track of the occlusion state of the swap chain to optimize rendering when the
 * window is not visible.
 */
class directx12_rendering_manager {
public:
    /**
     * @brief The type of the application window that the DirectX 12 rendering manager will render
     * to.
     */
    using window_type = platform::platform_specific_window<wil::unique_hwnd>;

    /**
     * @brief Construct a new directx12 rendering manager object. This will initialize the DirectX
     * 12 device, swap chain, command queue, and other related resources needed for rendering frames
     * to the specified application window.
     *
     * @note If something goes wrong during initialization, an exception will be thrown.
     *
     * @param window The application window to render to.
     * @param config The configuration for the rendering manager.
     */
    explicit directx12_rendering_manager(gsl::not_null<const window_type*> window,
                                         rendering_config config);

    ~directx12_rendering_manager() noexcept;

    /**
     * @brief Sets the clear color used to clear the render target before rendering each frame.
     *
     * @param clearColor The color to use for clearing the render target.
     */
    void set_clear_color(const color& clearColor) noexcept {
        m_clearColor = clearColor;
    }

    /**
     * @brief Checks if the swap chain is currently occluded, which means that the window is not
     * visible on the screen. This can happen when the window is minimized, or when it is covered by
     * another window. If the swap chain is occluded, it may be more efficient to skip rendering and
     * presenting frames until it becomes visible again.
     *
     * @return true if the swap chain is currently occluded, false otherwise.
     */
    bool is_window_occluded() const;

    /**
     * @brief Resets the occlusion state of the swap chain.
     */
    void reset_occlusion_state() noexcept {
        m_swapChainOccluded = false;
    }

    /**
     * @brief Renders a frame by executing the provided command list filling function. This function
     * will handle the synchronization of frames in flight, the presentation of the swap chain, and
     * the clearing of the render target using the configured clear color. The provided command list
     * filling function will be called with a reference to the current command list, allowing the
     * caller to record rendering commands for the frame.
     *
     * @param fillCommandListFunc A function that takes a reference to the current command list and
     * fills it with rendering commands. This function will be called for each frame that is
     * rendered. If no function is provided, the frame will be rendered with only the clear
     * operation and no additional rendering commands.
     */
    void render(const std::function<void(const wil::com_ptr<ID3D12GraphicsCommandList>&)>&
                    fillCommandListFunc = {});

    /**
     * @brief Retrieves a copy of the rendering configuration used by this rendering manager.
     *
     * This can be useful for querying the current configuration parameters, such as the number
     * of frames in flight, the number of back buffers, and the size of the SRV descriptor heap.
     *
     * @return rendering_config A copy of the rendering configuration used by this rendering
     * manager.
     */
    rendering_config get_rendering_config() const noexcept {
        return m_config;
    }

    /**
     * @brief Retrieves a reference to the DirectX 12 device used by this rendering manager.
     *
     * @return const wil::com_ptr<ID3D12Device>& A reference to the DirectX 12 device used by this
     * rendering manager.
     */
    const wil::com_ptr<ID3D12Device>& get_device() const noexcept {
        return m_device;
    }

    /**
     * @brief Retrieves a reference to the DirectX 12 command queue used by this rendering manager.
     *
     * @return const wil::com_ptr<ID3D12CommandQueue>& A reference to the DirectX 12 command queue
     * used by this rendering manager.
     */
    const wil::com_ptr<ID3D12CommandQueue>& get_command_queue() const noexcept {
        return m_commandQueue;
    }

    /**
     * @brief Get the srv descriptor heap allocator object, which is responsible for managing the
     * allocation and deallocation of descriptors from the shader resource view (SRV) descriptor
     * heap.
     *
     * @return descriptor_heap_allocator& A reference to the SRV descriptor heap allocator.
     */
    descriptor_heap_allocator& get_srv_descriptor_heap_allocator() noexcept {
        return *m_descriptorHeapAllocator;
    }

    /**
     * @brief Get the srv descriptor heap object
     *
     * @return const wil::com_ptr<ID3D12DescriptorHeap>& A reference to the SRV descriptor heap.
     */
    const wil::com_ptr<ID3D12DescriptorHeap>& get_srv_descriptor_heap() const noexcept {
        return m_srvDescriptorHeap;
    }

    /**
     * @brief Should be called when the application window is resized. This will update the swap
     * chain and render target resources to match the new window size, and it will also update the
     * occlusion state of the swap chain to optimize rendering when the window is not visible.
     *
     * @todo: Consider hiding this method, adding a bindable event inside the window class and
     * subscribing to it from the rendering manager, so that the user of the rendering manager
     * doesn't have to worry about calling this method when resizing the window.
     *
     * @param wparam The WPARAM parameter from the window resize event, which contains information
     * about the type of resize event (e.g., minimize, maximize, restore).
     * @param lparam The LPARAM parameter from the window resize event, which contains the new width
     * and height of the window in its low-order and high-order words, respectively.
     */
    void on_window_size(platform::win32::details::wparam_t wparam,
                        platform::win32::details::lparam_t lparam);

    /**
     * @brief Waits for all pending GPU operations to complete. Callers that own resources
     * referenced by in-flight command lists (e.g. an ImGui rendering backend) must call this
     * before releasing those resources, since the GPU may still be processing the last submitted
     * frame.
     */
    void wait_for_pending_operations() noexcept;

private:
    core::ptr_ref_to<const window_type> m_window;
    rendering_config m_config{};
    color m_clearColor{colors::WHITE};

    std::vector<frame_context> m_frameContexts{};
    std::size_t m_currentFrameIndex{};
    std::unique_ptr<descriptor_heap_allocator> m_descriptorHeapAllocator{};

    wil::com_ptr<ID3D12Device> m_device{};
    wil::com_ptr<ID3D12DescriptorHeap> m_rtvDescriptorHeap{};
    wil::com_ptr<ID3D12DescriptorHeap> m_srvDescriptorHeap{};
    wil::com_ptr<ID3D12CommandQueue> m_commandQueue{};
    wil::com_ptr<ID3D12GraphicsCommandList> m_commandList{};
    wil::com_ptr<ID3D12Fence> m_fence{};
    wil::unique_handle m_fenceEvent{};
    wil::unique_handle m_swapChainWaitableObject{};
    std::uint64_t m_fenceLastSignaledValue{};
    wil::com_ptr<IDXGISwapChain4> m_swapChain{};
    bool m_isTearingSupported{};
    bool m_swapChainOccluded{};
    std::vector<wil::com_ptr<ID3D12Resource>> m_renderTargetResources{};
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_renderTargetCpuDescriptorHandles{};

    // === Methods ===

    void create_device();
    void create_render_target();

    frame_context& wait_next_frame_context();
};

} // namespace tca::engine::rendering::win32::directx12
