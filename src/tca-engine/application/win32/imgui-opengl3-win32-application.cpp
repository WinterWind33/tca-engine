// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/imgui-opengl3-win32-application.hpp"

// Third-party
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>

// Engine
#include "application/application-logger.hpp"
#include "application/win32/imgui-win32-utilities.hpp"
#include "rendering/render-targets/win32/imgui-win32-window.hpp"
#include "rendering/win32/opengl3/opengl3-additive-blending.hpp"

namespace tca::engine::application::win32 {

void imgui_opengl3_win32_application::initialize() {
    get_application_logger()->info(
        "[ {} ] Initializing ImGui OpenGL 3 application",
        platform::win32::type_traits::from_win32_string_view(get_app_name()));

    // Enable DPI awareness for the application, so that the ImGui style scaling works properly on
    // high DPI displays.
    ImGui_ImplWin32_EnableDpiAwareness();

    // Let's create an imgui window before calling OpenGL 3 initialization, since the OpenGL 3
    // rendering manager needs a valid window to create the OpenGL 3 context, and
    // the ImGui OpenGL 3 backend needs a valid OpenGL 3 context to initialize properly, so we
    // need to create the ImGui window before initializing the OpenGL 3 resources.
    {
        auto window = platform::win32::win32_application_window::two_phase_window_creation<
            rendering::win32::imgui_win32_window>(
            get_app_name(),
            [this](const platform::win32::details::wparam_t wparam,
                   const platform::win32::details::lparam_t lparam) {
                if (!m_renderingManager) {
                    get_application_logger()->warn(
                        "[ {} ] Received WM_SIZE message, but rendering manager is not initialized "
                        "yet. Ignoring the message.",
                        platform::win32::type_traits::from_win32_string_view(get_app_name()));
                    return true;
                }

                if (wparam == SIZE_MINIMIZED) {
                    // Nothing to do here, we can just skip the resize handling since the window is
                    // minimized.
                    // No logging is needed.
                    return true;
                }

                m_renderingManager->on_window_resize(
                    // Update the rendering manager with the new
                    // window size when the window is resized.
                    LOWORD(lparam), HIWORD(lparam));
                return true;
            },
            [this](const float newDpiScale) {
                m_mainScale = on_dpi_scale_changed(get_app_name(), newDpiScale);
            },
            CS_OWNDC, // TODO: this is wrong, applications SHOULD NOT access these low level window
                      // styles.
            make_window_creation_params());

        window->set_input_collector(&get_input_collector());
        m_window = std::move(window);
    }

    m_mainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        ::MonitorFromWindow(m_window->get_native_handle().get(), MONITOR_DEFAULTTONEAREST));

    // Then we can create OpenGL 3 resources.
    opengl3_win32_application::initialize();

    m_window->show();

    initialize_imgui_context(get_app_name(), m_mainScale);
    initialize_rendering_backends();
}

bool imgui_opengl3_win32_application::pre_logic_tick() {
    if (!opengl3_win32_application::pre_logic_tick()) {
        return false;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    consume_input_captured_by_imgui(get_input());
    return true;
}

void imgui_opengl3_win32_application::post_logic_tick() {
    ImGui::Render();
    // Call OpenGL 3 rendering only after ImGui rendering, since the ImGui rendering needs to be
    // done before the OpenGL 3 rendering, so that the ImGui draw data is available when the OpenGL
    // 3 rendering is done.
    opengl3_win32_application::post_logic_tick();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Present
    m_renderingManager->present();
}

void imgui_opengl3_win32_application::initialize_rendering_backends() {
    ImGui_ImplWin32_InitForOpenGL(m_window->get_native_handle().get());
    ImGui_ImplOpenGL3_Init();
    rendering::win32::opengl3::install_additive_blending();

    m_imguiBackendInitialized = true;
}

void imgui_opengl3_win32_application::close() {
    if (!m_imguiBackendInitialized) {
        // initialize() failed before the ImGui backends were set up, so there is nothing to tear
        // down here.
        return;
    }

    get_application_logger()->info(
        "[ {} ] Shutting down ImGui OpenGL 3 application",
        platform::win32::type_traits::from_win32_string_view(get_app_name()));
    rendering::win32::opengl3::uninstall_additive_blending();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_imguiBackendInitialized = false;
}

} // namespace tca::engine::application::win32
