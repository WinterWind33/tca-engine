// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <functional>
#include <optional>
#include <stop_token>

// Third-party
#include <GLFW/glfw3.h>

// Engine
#include "application/application.hpp"
#include "rendering/theming/theme.hpp"

namespace tca::engine::application::posix_linux {

namespace detail {

/**
 * @brief A rectangle in GLFW screen coordinates.
 */
struct screen_rect final {
    int x{};
    int y{};
    int width{};
    int height{};

    [[nodiscard]] constexpr int right() const noexcept {
        return x + width;
    }

    [[nodiscard]] constexpr int bottom() const noexcept {
        return y + height;
    }
};

} // namespace detail

/**
 * @brief The application controller of GLFW-backed (Linux) applications, the counterpart of the
 * Win32 one: the same requests give the same results on both platforms.
 *
 * It does not own the window. It reaches it, the display settings and the theme through the
 * functions it is given, so it works with whatever window the application has at the time, or
 * none.
 *
 * - exit() asks the window to close (glfwSetWindowShouldClose()), or, with no window, requests a
 *   stop on the token handed out by the constructor.
 * - go_fullscreen() puts the window on the monitor it is on (the one whose work area it overlaps
 *   most, or the primary monitor if it overlaps none), at that monitor's current video mode.
 * - go_windowed() restores the window's position and content size from before going fullscreen.
 *   If there is none (the window was created fullscreen) or it is no longer on any monitor, the
 *   window gets window_display_settings::content_size, centred.
 */
class linux_application_controller final : public application_controller {
public:
    /**
     * @brief Constructs the controller.
     *
     * @param windowGetter Returns the application's window, or null if it has none.
     * @param settingsGetter Returns the application's window display settings.
     * @param themeSetter Applies a new theme to the application.
     * @param stopToken Set to a token that exit() requests a stop on when there is no window.
     */
    explicit linux_application_controller(
        std::function<GLFWwindow*()> windowGetter,
        std::function<const window_display_settings&()> settingsGetter,
        std::function<void(theming::theme)> themeSetter, std::stop_token& stopToken);

    void exit() override;
    void go_fullscreen() override;
    void go_windowed() override;
    void use_theme(theming::theme newTheme) override;
    void set_content_size(window_content_size contentSize,
                          content_size_options options = {}) override;
    [[nodiscard]] bool is_fullscreen() const noexcept override;
    void center_window() override;

private:
    std::function<GLFWwindow*()> m_windowGetter{};
    std::function<const window_display_settings&()> m_settingsGetter{};
    std::function<void(theming::theme)> m_themeSetter{};
    std::stop_source m_stopSource{};

    // The window's content area, in screen coordinates, the last time go_fullscreen() was called
    // while windowed. go_windowed() restores it.
    std::optional<detail::screen_rect> m_windowedRect{};

    struct deferred_content_size final {
        window_content_size contentSize{};
        window_placement placement{};
    };

    // A set_content_size() request made while fullscreen with
    // fullscreen_resize::defer_until_windowed, applied (once) by the next go_windowed().
    std::optional<deferred_content_size> m_deferredContentSize{};
};

} // namespace tca::engine::application::posix_linux
