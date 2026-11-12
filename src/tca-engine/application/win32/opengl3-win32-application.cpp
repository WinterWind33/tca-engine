// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/opengl3-win32-application.hpp"

// C++ STL
#include <memory>
#include <stdexcept>
#include <thread>

// Engine
#include "application/application-logger.hpp"
#include "core/utilities/pointers.hpp"

namespace tca::engine::application::win32 {

void opengl3_win32_application::initialize() {
    win32_application::initialize();

    if (!m_window || !m_window->get_native_handle().is_valid()) {
        throw std::runtime_error{
            "Failed to initialize OpenGL 3 application: application window is not set."};
    }

    get_application_logger()->info(
        "[ {} ] Initializing OpenGL 3 rendering manager with the provided application window",
        platform::win32::type_traits::from_win32_string_view(get_app_name()));

    // The following will automatically create all the OpenGL 3 resources.
    // If something goes wrong, it throws an exception.
    m_renderingManager =
        std::make_unique<rendering::win32::opengl3::opengl3_rendering_manager>(*m_window);
    m_renderingManager->set_clear_color(get_background_color());
}

void opengl3_win32_application::on_theme_changed() {
    // The rendering manager only exists from initialize() onwards, and set_theme() may well be
    // called before that as part of configuring the application; initialize() reads the theme
    // itself, so there is nothing to do yet in that case.
    if (m_renderingManager) {
        m_renderingManager->set_clear_color(get_background_color());
    }
}

bool opengl3_win32_application::pre_logic_tick() {
    if (!win32_application::pre_logic_tick()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return false;
    }

    if (m_renderingManager->is_window_occluded()) {
        // If the window is occluded, we skip the logic tick and we just wait for a short amount
        // of time before checking again, to avoid consuming too much CPU resources while the
        // window is not visible.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return false;
    }

    return true;
}

void opengl3_win32_application::post_logic_tick() {
    win32_application::post_logic_tick();

    if (m_renderingManager) {
        m_renderingManager->render();
        // m_renderingManager->present() should be called after rendering, so it is delegated to the
        // derived class, since the base class doesn't know when the rendering is done.
    }
}

rendering::image_render_queue& opengl3_win32_application::get_or_create_image_render_queue() {
    return core::get_or_create(m_imageRenderQueue, [] {
        return std::make_unique<rendering::win32::opengl3::opengl3_image_render_queue>();
    });
}

} // namespace tca::engine::application::win32
