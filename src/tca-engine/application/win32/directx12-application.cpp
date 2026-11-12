// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/directx12-application.hpp"

// C++ STL
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

// Third-party
#include <gsl/gsl>

#include "application/application-logger.hpp"
#include "core/utilities/pointers.hpp"

namespace tca::engine::application::win32 {

namespace {

/**
 * @brief Default rendering configuration for a DirectX 12 application with this engine.
 *
 */
constexpr rendering::win32::directx12::rendering_config RENDERING_CONFIG{
    .frames_in_flight_count = 2,
    .back_buffers_count = 2,
    .srv_heap_size = 64,
};

} // namespace

directx12_application::directx12_application(fill_command_list_callback_t fillCommandListFunc)
    : m_fillCommandListFunc{std::move(fillCommandListFunc)} {}

void directx12_application::initialize() {
    win32_application::initialize();

    if (!m_window || !m_window->get_native_handle().is_valid()) {
        throw std::runtime_error{
            "Failed to initialize DirectX 12 application: application window is not set."};
    }

    get_application_logger()->info(
        "[ {} ] Initializing DirectX 12 rendering manager with the provided application window",
        platform::win32::type_traits::from_win32_string_view(get_app_name()));

    // The following will automatically create all the DirectX 12 resources.
    // If something goes wrong, it throws an exception.
    m_renderingManager = std::make_unique<rendering::win32::directx12::directx12_rendering_manager>(
        m_window.get(), RENDERING_CONFIG);
    m_renderingManager->set_clear_color(get_background_color());
}

void directx12_application::on_theme_changed() {
    // The rendering manager only exists from initialize() onwards, and set_theme() may well be
    // called before that as part of configuring the application; initialize() reads the theme
    // itself, so there is nothing to do yet in that case.
    if (m_renderingManager) {
        m_renderingManager->set_clear_color(get_background_color());
    }
}

bool directx12_application::pre_logic_tick() {
    static bool windowOccludedAlreadyLogged{};
    if (!win32_application::pre_logic_tick() || !m_renderingManager) {
        get_application_logger()->critical(
            "[ {} ] Failed to perform pre-logic tick for DirectX 12 application: base pre-logic "
            "tick failed or rendering manager is not initialized.",
            platform::win32::type_traits::from_win32_string_view(get_app_name()));
        return false;
    }

    if (m_renderingManager->is_window_occluded()) {
        if (!windowOccludedAlreadyLogged) {
            get_application_logger()->info(
                "[ {} ] Window is occluded, skipping rendering and logic tick",
                platform::win32::type_traits::from_win32_string_view(get_app_name()));
            windowOccludedAlreadyLogged = true;
        }

        // If the window is occluded, we skip the logic tick and we just wait for a short amount of
        // time before checking again, to avoid consuming too much CPU resources while the window is
        // not visible.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return false;
    }

    windowOccludedAlreadyLogged = false;

    // Reset the occlusion state of the swap chain, since this will be properly updated
    // during the post tick logic.
    m_renderingManager->reset_occlusion_state();

    return true;
}

void directx12_application::post_logic_tick() {
    win32_application::post_logic_tick();
    // Just render the frame.
    m_renderingManager->render(m_fillCommandListFunc);
}

void directx12_application::on_window_size(const platform::win32::details::wparam_t wparam,
                                           const platform::win32::details::lparam_t lparam) {
    if (m_renderingManager) {
        m_renderingManager->on_window_size(wparam, lparam);
    }
}

rendering::image_render_queue& directx12_application::get_or_create_image_render_queue() {
    Expects(m_renderingManager != nullptr &&
            "Rendering manager must be initialized before requesting the image render queue.");

    return core::get_or_create(m_imageRenderQueue, [this] {
        return std::make_unique<rendering::win32::directx12::dx12_image_render_queue>(
            *m_renderingManager);
    });
}

} // namespace tca::engine::application::win32
