// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <wil/resource.h>

#include "platform/application-window.hpp"
#include "rendering/colors.hpp"

namespace tca::engine::rendering::win32::opengl3 {

namespace details {

/**
 * @brief Unique handle type for OpenGL rendering contexts (HGLRC). This type is a specialization of
 *  wil::unique_any, which is a type-erased unique handle that can be used to manage any resource
 *  that has a corresponding close function.
 */
using unique_hglrc = wil::unique_any<HGLRC, decltype(&::wglDeleteContext), ::wglDeleteContext>;

} // namespace details

/**
 * @brief Class owning and managing the OpenGL 3 rendering context for a given application window.
 * This class is responsible for creating and managing the OpenGL context, handling window resizing
 * events, and performing rendering operations.
 */
class opengl3_rendering_manager final {
public:
    /**
     * @brief The type of the window that this rendering manager will manage.
     */
    using window_type = platform::platform_specific_window<wil::unique_hwnd>;

    /**
     * @brief RAII. Creates an OpenGL 3 rendering manager for the given application window,
     * initializing the OpenGL context and any necessary resources. The rendering manager will be
     * responsible for managing the OpenGL context and performing rendering operations for the
     * specified window.
     *
     * @param window The application window that this rendering manager will manage.
     */
    explicit opengl3_rendering_manager(const window_type& window);
    ~opengl3_rendering_manager() noexcept;

    /**
     * @brief Handles window resize events. This method should be called whenever the application
     * window is resized, and it will update the OpenGL viewport and any necessary resources to
     * accommodate the new window size.
     * // TODO: this call should be hidden from the public API. Once the windows have events, this
     * method should be called internally by the rendering manager whenever a window resize event is
     * received, so that the user of the rendering manager does not have to worry about calling it
     * manually.
     *
     * @param newWidth The new width of the application window.
     * @param newHeight The new height of the application window.
     */
    void on_window_resize(std::uint32_t newWidth, std::uint32_t newHeight) noexcept;

    /**
     * @brief Sets the clear color for the OpenGL context. This color will be used to clear the
     * screen before rendering a new frame.
     *
     * @param color The color to use for clearing the screen.
     */
    void set_clear_color(const rendering::color& color) noexcept {
        m_clearColor = color;
    }

    /**
     * @brief Performs the rendering operations for the current frame. This method should be called
     * once per frame to render the scene using the OpenGL context. It will clear the screen using
     * the specified clear color, and then perform any necessary rendering operations to draw the
     * scene.
     *
     * @note DOES NOT present the render frame. The present() method should be called after this
     * method to present the rendered frame to the screen.
     */
    void render();

    /**
     * @brief Presents the rendered frame to the screen. This method should be called after the
     * render() method to present the rendered frame to the screen. It will swap the front and back
     * buffers of the OpenGL context to display the rendered frame on the screen.
     */
    void present();

    /**
     * @brief Checks if the application window is currently occluded. An occluded window is a window
     * that is not visible on the screen, either because it is minimized, covered by another window,
     * or otherwise not visible.
     *
     * @note This actually checks if the window is iconic, does not check if it is covered by
     * another window.
     * TODO: (Future refactors) this should not belong here. Move to the application window class.
     *
     * @return true if the application window is occluded, or false if it is visible on the screen.
     */
    bool is_window_occluded() const noexcept;

private:
    const window_type& m_window;
    details::unique_hglrc m_openglContext{};
    wil::unique_hdc m_windowHdc{};
    std::uint32_t m_windowWidth{};
    std::uint32_t m_windowHeight{};
    rendering::color m_clearColor{};

    void initialize(const window_type& window);
    void cleanup() noexcept;
};

} // namespace tca::engine::rendering::win32::opengl3
