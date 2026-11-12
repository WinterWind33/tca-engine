// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/imgui-dx12-application.hpp"

// Third-party
#include <gsl/gsl>

#include <imgui.h>

// Windows
#include "platform/win32/windows.hpp"

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

// Engine
#include "platform/win32/windows-type-traits.hpp"

#include "rendering/render-targets/win32/imgui-win32-window.hpp"

#include "application/application-logger.hpp"
#include "application/win32/imgui-win32-utilities.hpp"

namespace tca::engine::application::win32 {

imgui_dx12_application::imgui_dx12_application()
    : directx12_application{[this](const wil::com_ptr<ID3D12GraphicsCommandList>& commandList) {
          fill_command_list(commandList);
      }} {}

void imgui_dx12_application::initialize() {
    get_application_logger()->info(
        "[ {} ] Initializing ImGui DirectX 12 application",
        platform::win32::type_traits::from_win32_string_view(get_app_name()));

    // Enable DPI awareness for the application, so that the ImGui style scaling works properly on
    // high DPI displays.
    ImGui_ImplWin32_EnableDpiAwareness();

    // Let's create an imgui window before calling DirectX 12 initialization, since the DirectX 12
    // rendering manager needs a valid window to create the DirectX 12 device and swap chain, and
    // the ImGui DirectX 12 backend needs a valid DirectX 12 device to initialize properly, so we
    // need to create the ImGui window before initializing the DirectX 12 resources.
    {
        auto window = platform::win32::win32_application_window::two_phase_window_creation<
            rendering::win32::imgui_win32_window>(
            get_app_name(),
            [this](const platform::win32::details::wparam_t wparam,
                   const platform::win32::details::lparam_t lparam) {
                on_window_size(wparam, lparam);
                return true;
            },
            [this](const float newDpiScale) {
                m_mainScale = on_dpi_scale_changed(get_app_name(), newDpiScale);
            },
            CS_CLASSDC, make_window_creation_params());

        window->set_input_collector(&get_input_collector());
        m_window = std::move(window);
    }

    m_mainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        ::MonitorFromWindow(m_window->get_native_handle().get(), MONITOR_DEFAULTTONEAREST));

    // Then we can create DirectX 12 resources.
    directx12_application::initialize();

    // Show the window.
    m_window->show();

    initialize_imgui_context(get_app_name(), m_mainScale);
    initialize_imgui_rendering_backend();
}

void imgui_dx12_application::fill_command_list(
    const wil::com_ptr<ID3D12GraphicsCommandList>& commandList) {
    // Delegate the rendering of the ImGui draw data to the ImGui DirectX 12 backend, which will
    // take care of rendering all the ImGui draw lists using the provided command list.
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.get());
}

void imgui_dx12_application::initialize_imgui_rendering_backend() {
    Expects(m_window != nullptr);
    Expects(m_renderingManager != nullptr);

    get_application_logger()->info(
        "[ {} ] Initializing ImGui DirectX 12 rendering backend for application",
        platform::win32::type_traits::from_win32_string_view(get_app_name()));

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_window->get_native_handle().get());

    // This structure is non-aggregate type (it has a default constructor) so we cannot use
    // designated initializers.
    m_imguiInitInfo.Device = m_renderingManager->get_device().get();
    m_imguiInitInfo.CommandQueue = m_renderingManager->get_command_queue().get();
    m_imguiInitInfo.NumFramesInFlight =
        static_cast<int>(m_renderingManager->get_rendering_config().frames_in_flight_count);
    m_imguiInitInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_imguiInitInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    // Set the user data to point to the descriptor heap allocator, so that we can use it in the
    // callbacks to allocate and free descriptors for the ImGui textures.
    m_imguiInitInfo.UserData =
        static_cast<void*>(&m_renderingManager->get_srv_descriptor_heap_allocator());

    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need
    // to allocate more)
    m_imguiInitInfo.SrvDescriptorHeap = m_renderingManager->get_srv_descriptor_heap().get();
    m_imguiInitInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* const init_info,
                                              D3D12_CPU_DESCRIPTOR_HANDLE* const out_cpu_handle,
                                              D3D12_GPU_DESCRIPTOR_HANDLE* const out_gpu_handle) {
        gsl::not_null descriptorHeapAllocator{
            static_cast<rendering::win32::directx12::descriptor_heap_allocator*>(
                init_info->UserData)};
        return descriptorHeapAllocator->allocate(*out_cpu_handle, *out_gpu_handle);
    };
    m_imguiInitInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* const initInfo,
                                             D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
                                             D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
        gsl::not_null descriptorHeapAllocator{
            static_cast<rendering::win32::directx12::descriptor_heap_allocator*>(
                initInfo->UserData)};
        return descriptorHeapAllocator->free(cpu_handle, gpu_handle);
    };
    ImGui_ImplDX12_Init(&m_imguiInitInfo);
    m_imguiBackendInitialized = true;

    m_additiveBlending = std::make_unique<rendering::win32::directx12::additive_blend_pipelines>(
        *m_renderingManager->get_device(), m_imguiInitInfo.RTVFormat);
}

bool imgui_dx12_application::pre_logic_tick() {
    if (!directx12_application::pre_logic_tick()) {
        return false;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    consume_input_captured_by_imgui(get_input());

    return true;
}

void imgui_dx12_application::post_logic_tick() {
    ImGui::Render();

    // Call DirectX 12 rendering only after ImGui rendering.
    directx12_application::post_logic_tick();
}

void imgui_dx12_application::close() {
    if (!m_imguiBackendInitialized) {
        // initialize() failed before the ImGui backends were set up, so there is nothing to tear
        // down here.
        return;
    }

    get_application_logger()->info(
        "[ {} ] Shutting down ImGui DirectX 12 application",
        platform::win32::type_traits::from_win32_string_view(get_app_name()));

    // The GPU may still be processing the last submitted/presented frame, which references
    // ImGui-owned descriptors and buffers, so we must wait for it to be idle before releasing
    // those resources below.
    if (m_renderingManager) {
        m_renderingManager->wait_for_pending_operations();
    }

    m_additiveBlending.reset();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_imguiBackendInitialized = false;
}

} // namespace tca::engine::application::win32
