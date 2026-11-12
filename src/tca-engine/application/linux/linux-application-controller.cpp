// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/linux/linux-application-controller.hpp"

// C++ STL
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>

// Third-party
#include <GLFW/glfw3.h>
#include <gsl/assert>

// Engine
#include "application/application-logger.hpp"

namespace tca::engine::application::posix_linux {

namespace {

using detail::screen_rect;

/**
 * @brief How far the window's frame sticks out of its content area on each side. All 0 for
 * undecorated and fullscreen windows (and possibly on Wayland, when the decorations are drawn by
 * the client).
 */
struct frame_extents final {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

[[nodiscard]] frame_extents frame_extents_of(GLFWwindow* const window) noexcept {
    frame_extents extents{};
    ::glfwGetWindowFrameSize(window, &extents.left, &extents.top, &extents.right, &extents.bottom);
    return extents;
}

/**
 * @brief The window's content area. GLFW positions and sizes windows by it, so this is what
 * glfwSetWindowMonitor() takes back when leaving fullscreen.
 */
[[nodiscard]] screen_rect content_rect_of(GLFWwindow* const window) noexcept {
    screen_rect content{};
    ::glfwGetWindowPos(window, &content.x, &content.y);
    ::glfwGetWindowSize(window, &content.width, &content.height);
    return content;
}

/**
 * @brief The window's outer rectangle, frame included: the counterpart of Win32's GetWindowRect(),
 * and what is measured against monitor work areas.
 */
[[nodiscard]] screen_rect frame_rect_of(GLFWwindow* const window) noexcept {
    const screen_rect content{content_rect_of(window)};
    const frame_extents extents{frame_extents_of(window)};

    return screen_rect{.x = content.x - extents.left,
                       .y = content.y - extents.top,
                       .width = content.width + extents.left + extents.right,
                       .height = content.height + extents.top + extents.bottom};
}

[[nodiscard]] screen_rect work_area_of(GLFWmonitor* const monitor) noexcept {
    screen_rect workArea{};
    ::glfwGetMonitorWorkarea(monitor, &workArea.x, &workArea.y, &workArea.width, &workArea.height);
    return workArea;
}

/**
 * @brief The monitor whose work area the rectangle overlaps most.
 *
 * @return The monitor, or null if the rectangle overlaps none (or no monitor is connected).
 */
[[nodiscard]] GLFWmonitor* monitor_overlapping(const screen_rect& rect) noexcept {
    int monitorsCount{};
    GLFWmonitor** const monitors{::glfwGetMonitors(&monitorsCount)};
    if (monitors == nullptr) {
        return nullptr;
    }

    GLFWmonitor* bestMonitor{};
    std::int64_t bestArea{};
    for (GLFWmonitor* const monitor :
         std::span{monitors, static_cast<std::size_t>(monitorsCount)}) {
        const screen_rect workArea{work_area_of(monitor)};

        // A negative overlap on either axis means the rectangles do not touch.
        const int overlapWidth{
            std::max(0, std::min(rect.right(), workArea.right()) - std::max(rect.x, workArea.x))};
        const int overlapHeight{
            std::max(0, std::min(rect.bottom(), workArea.bottom()) - std::max(rect.y, workArea.y))};

        // Widened before multiplying, so two large screen sizes cannot overflow an int.
        if (const std::int64_t area{std::int64_t{overlapWidth} * overlapHeight}; area > bestArea) {
            bestMonitor = monitor;
            bestArea = area;
        }
    }

    return bestMonitor;
}

/**
 * @brief The monitor the window is on. GLFW has no such call, since glfwGetWindowMonitor() only
 * reports the monitor of a fullscreen window, so it is the one whose work area the window
 * overlaps most.
 *
 * @return That monitor, the primary monitor if the window overlaps none, or null if no monitor is
 * connected.
 */
[[nodiscard]] GLFWmonitor* monitor_of(GLFWwindow* const window) noexcept {
    if (GLFWmonitor* const monitor{monitor_overlapping(frame_rect_of(window))};
        monitor != nullptr) {
        return monitor;
    }

    return ::glfwGetPrimaryMonitor();
}

/**
 * @brief Resizes a windowed window's content area and places it, keeping the whole window, frame
 * included, inside the work area of the monitor it is on. A size too large for that work area is
 * clamped, leaving the content area smaller than requested.
 */
void apply_content_size(GLFWwindow* const window, const window_content_size contentSize,
                        const window_placement placement) {
    GLFWmonitor* const monitor{monitor_of(window)};
    if (monitor == nullptr) {
        get_application_logger()->warn(
            "Cannot set the content size to {}x{} because no monitor is connected. Ignoring the "
            "request.",
            contentSize.width, contentSize.height);
        return;
    }

    const screen_rect workArea{work_area_of(monitor)};
    const frame_extents extents{frame_extents_of(window)};
    const int outerWidth{
        std::min(contentSize.width + extents.left + extents.right, workArea.width)};
    const int outerHeight{
        std::min(contentSize.height + extents.top + extents.bottom, workArea.height)};

    int outerX{};
    int outerY{};
    switch (placement) {
        case window_placement::keep_top_left: {
            // The top-left corner only moves if the resized window would otherwise hang off the
            // work area.
            const screen_rect frameRect{frame_rect_of(window)};
            outerX = std::clamp(frameRect.x, workArea.x, workArea.right() - outerWidth);
            outerY = std::clamp(frameRect.y, workArea.y, workArea.bottom() - outerHeight);
            break;
        }
        case window_placement::center:
            outerX = workArea.x + ((workArea.width - outerWidth) / 2);
            outerY = workArea.y + ((workArea.height - outerHeight) / 2);
            break;
    }

    ::glfwSetWindowSize(window, outerWidth - extents.left - extents.right,
                        outerHeight - extents.top - extents.bottom);
    ::glfwSetWindowPos(window, outerX + extents.left, outerY + extents.top);
}

} // namespace

linux_application_controller::linux_application_controller(
    std::function<GLFWwindow*()> windowGetter,
    std::function<const window_display_settings&()> settingsGetter,
    std::function<void(theming::theme)> themeSetter, std::stop_token& stopToken)
    : m_windowGetter{std::move(windowGetter)},
      m_settingsGetter{std::move(settingsGetter)},
      m_themeSetter{std::move(themeSetter)} {
    stopToken = m_stopSource.get_token();
}

void linux_application_controller::exit() {
    // The architecture of the application doesn't guarantee that the application has a window, so
    // with none there is nothing to close and the stop is requested directly.
    if (GLFWwindow* const windowHandle{m_windowGetter()}; windowHandle != nullptr) {
        get_application_logger()->info(
            "Manual exit signal received. Asking the application window to close.");
        ::glfwSetWindowShouldClose(windowHandle, GLFW_TRUE);
    } else {
        get_application_logger()->info(
            "Manual exit signal received. Requesting application with no window to exit.");
        m_stopSource.request_stop();
    }
}

void linux_application_controller::go_fullscreen() {
    GLFWwindow* const windowHandle{m_windowGetter()};
    if (windowHandle == nullptr) {
        get_application_logger()->warn(
            "Cannot go fullscreen because the application window is not provided. Ignoring the "
            "request.");
        return;
    }

    // Already fullscreen: going fullscreen again must not overwrite the saved windowed rectangle
    // with the fullscreen one.
    if (is_fullscreen()) {
        return;
    }

    GLFWmonitor* const monitor{monitor_of(windowHandle)};
    if (monitor == nullptr) {
        get_application_logger()->warn(
            "Cannot go fullscreen because no monitor is connected. Ignoring the request.");
        return;
    }

    const GLFWvidmode* const mode{::glfwGetVideoMode(monitor)};
    if (mode == nullptr) {
        get_application_logger()->warn(
            "Cannot go fullscreen because the video mode of the window's monitor could not be "
            "retrieved. Ignoring the request.");
        return;
    }

    // The content area, not the frame rectangle: glfwSetWindowMonitor() takes it back that way.
    m_windowedRect = content_rect_of(windowHandle);

    get_application_logger()->info("Setting the application to fullscreen.");

    // Requesting exactly the monitor's current video mode keeps GLFW from switching the display
    // mode.
    ::glfwSetWindowMonitor(windowHandle, monitor, 0, 0, mode->width, mode->height,
                           mode->refreshRate);
}

void linux_application_controller::go_windowed() {
    GLFWwindow* const windowHandle{m_windowGetter()};
    if (windowHandle == nullptr) {
        get_application_logger()->warn(
            "Cannot go windowed because the application window is not provided. Ignoring the "
            "request.");
        return;
    }

    // Already windowed: restoring the saved rectangle would only move the window around.
    if (!is_fullscreen()) {
        return;
    }

    get_application_logger()->info("Setting the application to windowed.");

    // The window goes back where it was before going fullscreen, as long as that is still on a
    // monitor (it may have been disconnected in the meantime). Otherwise it gets the configured
    // content size, centred on the monitor it was fullscreen on.
    if (m_windowedRect.has_value() && monitor_overlapping(*m_windowedRect) != nullptr) {
        const screen_rect& rect{*m_windowedRect};
        ::glfwSetWindowMonitor(windowHandle, nullptr, rect.x, rect.y, rect.width, rect.height, 0);
    } else {
        const window_display_settings& settings{m_settingsGetter()};
        get_application_logger()->warn(
            "The window's position from before going fullscreen is not available. Using the "
            "configured window display settings' content size ({}x{}), centred.",
            settings.content_size.width, settings.content_size.height);

        // Leaving fullscreen at the work area's origin keeps the window on the monitor it was
        // fullscreen on, so that apply_content_size() centres it there.
        const screen_rect workArea{work_area_of(::glfwGetWindowMonitor(windowHandle))};
        ::glfwSetWindowMonitor(windowHandle, nullptr, workArea.x, workArea.y,
                               settings.content_size.width, settings.content_size.height, 0);
        apply_content_size(windowHandle, settings.content_size, window_placement::center);
    }

    // A content size requested while fullscreen with fullscreen_resize::defer_until_windowed is
    // applied once, now that the window has a content size of its own again. Applying it after the
    // restore means keep_top_left keeps the top-left corner from before going fullscreen.
    if (const auto deferred{std::exchange(m_deferredContentSize, std::nullopt)};
        deferred.has_value()) {
        apply_content_size(windowHandle, deferred->contentSize, deferred->placement);
    }
}

void linux_application_controller::use_theme(theming::theme newTheme) {
    Expects(m_themeSetter != nullptr && "Theme setter function cannot be null.");

    // Routed back through the application rather than only setting the ambient theme here: the
    // application is the one place that knows everything a theme decides (the ImGui style and the
    // colour the window is cleared to, as well as what widgets resolve against).
    m_themeSetter(std::move(newTheme));
}

void linux_application_controller::set_content_size(const window_content_size contentSize,
                                                    const content_size_options options) {
    GLFWwindow* const windowHandle{m_windowGetter()};
    if (windowHandle == nullptr) {
        get_application_logger()->warn(
            "Cannot set the content size because the application window is not provided. "
            "Ignoring the request.");
        return;
    }

    if (contentSize.is_empty()) {
        get_application_logger()->warn(
            "Cannot set the content size to {}x{} because it is empty. Ignoring the request.",
            contentSize.width, contentSize.height);
        return;
    }

    // A fullscreen window covers its whole monitor, so there is no content size to change: the
    // request is either dropped or kept for the next go_windowed(). An ignored request leaves an
    // earlier deferred one in place.
    if (is_fullscreen()) {
        if (options.fullscreen == fullscreen_resize::defer_until_windowed) {
            get_application_logger()->info(
                "The application is fullscreen. Deferring the content size {}x{} until it goes "
                "windowed.",
                contentSize.width, contentSize.height);
            m_deferredContentSize = deferred_content_size{contentSize, options.placement};
        } else {
            get_application_logger()->info(
                "The application is fullscreen. Ignoring the content size {}x{}.",
                contentSize.width, contentSize.height);
        }
        return;
    }

    apply_content_size(windowHandle, contentSize, options.placement);
}

bool linux_application_controller::is_fullscreen() const noexcept {
    if (auto* const windowHandle{m_windowGetter()}; windowHandle) {
        // getWindowMonitor returns a valid monitor only if the window uses it
        // when in fullscreen mode.
        return ::glfwGetWindowMonitor(windowHandle) != nullptr;
    }

    return false;
}

void linux_application_controller::center_window() {
    GLFWwindow* const windowHandle{m_windowGetter()};
    if (windowHandle == nullptr) {
        get_application_logger()->warn(
            "Cannot center the window because the application window is not provided. Ignoring "
            "the request.");
        return;
    }

    if (is_fullscreen()) {
        get_application_logger()->warn(
            "Skipping window centering request since the window is fullscreen.");
        return;
    }

    const screen_rect content{content_rect_of(windowHandle)};
    apply_content_size(windowHandle,
                       window_content_size{.width = content.width, .height = content.height},
                       window_placement::center);
}

} // namespace tca::engine::application::posix_linux
