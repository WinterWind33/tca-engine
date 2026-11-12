// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <memory>

// Engine
#include "application/win32/win32-application.hpp"
#include "rendering/win32/opengl3/opengl3-image-render-queue.hpp"
#include "rendering/win32/opengl3/opengl3-rendering-manager.hpp"

namespace tca::engine::application::win32 {

/**
 * @brief An application class that integrates an OpenGL 3 rendering backend on the Win32 platform.
 * This class extends the basic Win32 application to include OpenGL 3 rendering manager.
 * It also handles window resizing events to ensure that the OpenGL viewport
 * is updated accordingly.
 */
class opengl3_win32_application : public win32_application {
protected:
    std::unique_ptr<rendering::win32::opengl3::opengl3_rendering_manager> m_renderingManager{};

    void initialize() override;
    bool pre_logic_tick() override;
    void post_logic_tick() override;

    /**
     * @brief Pushes the theme's window background role into the rendering manager's clear color,
     * so that re-theming the application at runtime also changes what the window is cleared to.
     */
    void on_theme_changed() override;

    /**
     * @brief Lazily constructs (on first call) and returns an OpenGL 3-backed image render queue.
     * Must only be called after initialize() has completed.
     */
    rendering::image_render_queue& get_or_create_image_render_queue() override;

private:
    std::unique_ptr<rendering::win32::opengl3::opengl3_image_render_queue> m_imageRenderQueue{};
};

} // namespace tca::engine::application::win32
