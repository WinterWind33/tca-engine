// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/linux/linux-application-window.hpp"

// Third-party
#include <GLFW/glfw3.h>
#include <gsl/assert>
#include <gsl/gsl>
#include <gsl/narrow>

// C++ STL
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

// Engine
#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"
#include "platform/images/image-decoder.hpp"
#include "platform/linux/linux-keyboard.hpp"
#include "platform/platform-logger.hpp"

namespace tca::engine::platform::posix_linux {

namespace {

linux_application_window* get_app_window_from_glfw_window(GLFWwindow* const window) noexcept {
    if (!window) {
        return nullptr;
    }

    // First thing we do is to route the message back to the engine's window instance so that
    // it can properly handle the input.
    void* const userPointer{::glfwGetWindowUserPointer(window)};
    if (!userPointer) {
        // Nothing to do here.
        return nullptr;
    }

    return static_cast<linux_application_window*>(userPointer);
}

/**
 * @brief The engine button of a GLFW_MOUSE_BUTTON_*, or nothing for the buttons past the fifth,
 *  which have no engine counterpart. GLFW numbers the side buttons 4 (back) and 5 (forward).
 */
[[nodiscard]] std::optional<input::mouse_button> mouse_button_from_glfw_button(
    const int button) noexcept {
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            return input::mouse_button::left;
        case GLFW_MOUSE_BUTTON_RIGHT:
            return input::mouse_button::right;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            return input::mouse_button::middle;
        case GLFW_MOUSE_BUTTON_4:
            return input::mouse_button::x1;
        case GLFW_MOUSE_BUTTON_5:
            return input::mouse_button::x2;
        default:
            return std::nullopt;
    }
}

/**
 * @brief The pointer position of a GLFW cursor position. GLFW reports it in the same screen
 *  coordinates as the window size, but as a double: sub-pixel positions round down, so a pointer
 *  just left of or above the content area reads as negative rather than as 0.
 */
[[nodiscard]] input::pointer_position pointer_position_from_glfw_cursor(const double x,
                                                                        const double y) noexcept {
    return {.x = static_cast<std::int32_t>(std::floor(x)),
            .y = static_cast<std::int32_t>(std::floor(y))};
}

/**
 * @brief The wheel delta of a GLFW scroll. Vertical matches already (positive away from the
 *  player), but GLFW's horizontal offset is positive to the left (X11 maps button 6, scroll left,
 *  to +1, and Wayland's axis is negated to match), so it is flipped.
 */
[[nodiscard]] input::wheel_delta wheel_delta_from_glfw_scroll(const double xOffset,
                                                              const double yOffset) noexcept {
    return {.vertical = static_cast<float>(yOffset), .horizontal = static_cast<float>(-xOffset)};
}

} // namespace

unique_glfw_window::unique_glfw_window(unique_glfw_window&& other) noexcept
    : m_windowPtr{other.release()} {}

unique_glfw_window& unique_glfw_window::operator=(unique_glfw_window&& other) noexcept {
    if (this != std::addressof(other)) {
        reset();
        m_windowPtr = other.release();
    }
    return (*this);
}

unique_glfw_window::~unique_glfw_window() noexcept {
    reset();
}

void unique_glfw_window::swap(unique_glfw_window& other) noexcept {
    unique_glfw_window self{std::move(*this)};
    operator=(std::move(other));
    other = std::move(self);
}

void unique_glfw_window::reset(const pointer newPtr) noexcept {
    if (m_windowPtr != nullptr && m_windowPtr != newPtr) {
        ::glfwDestroyWindow(m_windowPtr);
    }

    m_windowPtr = newPtr;
}
linux_application_window::linux_application_window(const platform_preferred_string_view_t title,
                                                   const window_creation_params& creationParams)
    : platform_specific_window<unique_glfw_window>(title) {
    create_window(creationParams);
}

void linux_application_window::create_window(const window_creation_params& creationParams) {
    instantiate_window(creationParams);
    const auto& nativeHandle{get_native_handle()};
    Expects(nativeHandle.is_valid());

    // Every callback finds this instance through the window user pointer, so a move has to
    // re-point it (see the move operations).
    ::glfwSetWindowUserPointer(nativeHandle.get(), this);
    std::ignore = ::glfwSetWindowSizeCallback(
        nativeHandle.get(),
        [](GLFWwindow* const windowPtr, const int width, const int height) noexcept {
            auto* const self{get_app_window_from_glfw_window(windowPtr)};
            if (!self) {
                return;
            }

            // We accept ints in the callback so that it properly aligns with GLFW
            // signature but here we cast them to std::int32_t which are platform independent.
            self->m_resizeEvent.execute(static_cast<std::int32_t>(width),
                                        static_cast<std::int32_t>(height));
        });
    std::ignore =
        ::glfwSetKeyCallback(nativeHandle.get(), &linux_application_window::window_key_callback);
    std::ignore = ::glfwSetCharCallback(nativeHandle.get(),
                                        &linux_application_window::window_unicode_char_callback);
    std::ignore = ::glfwSetWindowFocusCallback(
        nativeHandle.get(), &linux_application_window::window_focus_changed_callback);

    std::ignore =
        ::glfwSetWindowCloseCallback(nativeHandle.get(), [](GLFWwindow* const windowPtr) noexcept {
            auto* const self{get_app_window_from_glfw_window(windowPtr)};
            if (!self) {
                return;
            }

            // GLFW has already raised the should-close flag by now: a veto lowers it again.
            if (self->on_close_requested()) {
                get_platform_logger()->info("The window with title '{}' was asked to close",
                                            self->get_window_title());
            } else {
                ::glfwSetWindowShouldClose(windowPtr, GLFW_FALSE);
                get_platform_logger()->info("The window with title '{}' vetoed a close request",
                                            self->get_window_title());
            }
        });

    // The mouse feeds the input collector, then the native event hooks, as GLFW reports it.
    std::ignore = ::glfwSetMouseButtonCallback(
        nativeHandle.get(), [](GLFWwindow* const windowPtr, const int button, const int action,
                               const int mods) noexcept {
            if (auto* const self{get_app_window_from_glfw_window(windowPtr)}; self) {
                self->routed_mouse_button_handler(button, action);
                self->on_mouse_button(button, action, mods);
            }
        });
    std::ignore = ::glfwSetCursorPosCallback(
        nativeHandle.get(),
        [](GLFWwindow* const windowPtr, const double x, const double y) noexcept {
            if (auto* const self{get_app_window_from_glfw_window(windowPtr)}; self) {
                self->routed_cursor_position_handler(x, y);
                self->on_cursor_moved(x, y);
            }
        });
    std::ignore = ::glfwSetCursorEnterCallback(
        nativeHandle.get(), [](GLFWwindow* const windowPtr, const int entered) noexcept {
            if (auto* const self{get_app_window_from_glfw_window(windowPtr)}; self) {
                self->routed_cursor_enter_handler(entered == GLFW_TRUE);
            }
        });
    std::ignore = ::glfwSetScrollCallback(
        nativeHandle.get(),
        [](GLFWwindow* const windowPtr, const double xOffset, const double yOffset) noexcept {
            if (auto* const self{get_app_window_from_glfw_window(windowPtr)}; self) {
                self->routed_scroll_handler(xOffset, yOffset);
                self->on_scroll(xOffset, yOffset);
            }
        });
    std::ignore = ::glfwSetWindowIconifyCallback(
        nativeHandle.get(), [](GLFWwindow* const windowPtr, const int iconified) noexcept {
            if (auto* const self{get_app_window_from_glfw_window(windowPtr)}; self) {
                self->on_iconify(iconified == GLFW_TRUE);
            }
        });
    std::ignore = ::glfwSetFramebufferSizeCallback(
        nativeHandle.get(),
        [](GLFWwindow* const windowPtr, const int width, const int height) noexcept {
            if (auto* const self{get_app_window_from_glfw_window(windowPtr)}; self) {
                self->on_framebuffer_resized(static_cast<std::int32_t>(width),
                                             static_cast<std::int32_t>(height));
            }
        });
    std::ignore = ::glfwSetWindowContentScaleCallback(
        nativeHandle.get(),
        [](GLFWwindow* const windowPtr, const float xScale, const float yScale) noexcept {
            if (auto* const self{get_app_window_from_glfw_window(windowPtr)}; self) {
                self->on_content_scale_changed(xScale, yScale);
            }
        });

    get_platform_logger()->info("Created a new window with title '{}'", get_window_title());
}

linux_application_window::linux_application_window(linux_application_window&& other) noexcept
    : platform_specific_window<unique_glfw_window>(std::move(other)),
      m_resizeEvent{std::move(other.m_resizeEvent)},
      m_inputCollector{std::exchange(other.m_inputCollector, nullptr)} {
    rebind_native_window();
}

linux_application_window& linux_application_window::operator=(
    linux_application_window&& other) noexcept {
    if (this != std::addressof(other)) {
        // Destroys the GLFW window this instance owned, if any, before taking other's.
        platform_specific_window<unique_glfw_window>::operator=(std::move(other));
        m_resizeEvent = std::move(other.m_resizeEvent);
        m_inputCollector = std::exchange(other.m_inputCollector, nullptr);
        rebind_native_window();
    }

    return (*this);
}

void linux_application_window::rebind_native_window() noexcept {
    // GLFW still routes the window's callbacks to the object it was created by.
    if (const auto& nativeHandle{get_native_handle()}; nativeHandle.is_valid()) {
        ::glfwSetWindowUserPointer(nativeHandle.get(), this);
    }
}

void linux_application_window::instantiate_window(const window_creation_params& creationParams) {
    // GLFW window hints are global and persist until reset, so every hint this function cares
    // about is set on every call, including the "not requested" cases. Otherwise a previous
    // creation (ours or anyone else's) would leak into this one.
    if (creationParams.initial_position) {
        ::glfwWindowHint(GLFW_POSITION_X, creationParams.initial_position->x);
        ::glfwWindowHint(GLFW_POSITION_Y, creationParams.initial_position->y);
    } else {
        ::glfwWindowHint(GLFW_POSITION_X, GLFW_ANY_POSITION);
        ::glfwWindowHint(GLFW_POSITION_Y, GLFW_ANY_POSITION);
    }

    ::glfwWindowHint(GLFW_RESIZABLE, creationParams.resizable);
    ::glfwWindowHint(GLFW_DECORATED, creationParams.decorated);
    ::glfwWindowHint(GLFW_VISIBLE, creationParams.visible);
    ::glfwWindowHint(GLFW_MAXIMIZED, creationParams.maximized);
    ::glfwWindowHint(GLFW_FOCUSED, creationParams.focused);
    ::glfwWindowHint(GLFW_FOCUS_ON_SHOW, creationParams.focus_on_show);

    GLFWmonitor* const monitor{creationParams.fullscreen ? ::glfwGetPrimaryMonitor() : nullptr};
    if (creationParams.fullscreen && monitor == nullptr) {
        get_platform_logger()->warn(
            "The window with title '{}' was asked to start fullscreen, but no monitor is "
            "connected. Creating it windowed instead.",
            get_window_title());
    }

    std::int32_t width{creationParams.initial_size.width};
    std::int32_t height{creationParams.initial_size.height};

    if (monitor != nullptr) {
        // Passing a monitor makes GLFW switch the display to the closest video mode to the
        // requested size. Requesting exactly the monitor's current mode instead gives a
        // "windowed full screen" window that leaves the display mode alone.
        const GLFWvidmode* const currentMode{::glfwGetVideoMode(monitor)};
        Expects(currentMode != nullptr);

        ::glfwWindowHint(GLFW_RED_BITS, currentMode->redBits);
        ::glfwWindowHint(GLFW_GREEN_BITS, currentMode->greenBits);
        ::glfwWindowHint(GLFW_BLUE_BITS, currentMode->blueBits);
        ::glfwWindowHint(GLFW_REFRESH_RATE, currentMode->refreshRate);
        width = currentMode->width;
        height = currentMode->height;
    } else {
        // GLFW's own defaults, so a previous fullscreen creation doesn't leak in.
        ::glfwWindowHint(GLFW_RED_BITS, 8);
        ::glfwWindowHint(GLFW_GREEN_BITS, 8);
        ::glfwWindowHint(GLFW_BLUE_BITS, 8);
        ::glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE);
    }

    unique_glfw_window wnd{
        ::glfwCreateWindow(width, height, get_window_title().data(), monitor, nullptr)};
    if (!wnd) {
        // No display to connect to, GLFW not initialized, an unsupported context request...
        // GLFW reports the reason itself, through its error callback.
        get_platform_logger()->error("Failed to create the window with title '{}'",
                                     get_window_title());
        throw std::runtime_error{"Failed to create the GLFW window."};
    }

    if (!creationParams.icons.empty()) {
        std::vector<GLFWimage> iconImages{};
        iconImages.reserve(creationParams.icons.size());
        std::ranges::transform(
            creationParams.icons, std::back_inserter(iconImages),
            [](const images::decoded_image& image) -> GLFWimage {
                // A hand-built decoded_image can be short; GLFW would read past its end.
                Expects(image.pixels.size() ==
                        std::size_t{image.width} * std::size_t{image.height} * 4U);

                return GLFWimage{
                    .width = gsl::narrow<int>(image.width),
                    .height = gsl::narrow<int>(image.height),
                    // GLFWimage::pixels is non-const, but glfwSetWindowIcon() only reads it: the
                    // image data is copied before the call returns, so dropping const is safe.
                    .pixels = const_cast<unsigned char*>(
                        reinterpret_cast<const unsigned char*>(image.pixels.data()))};
            });

        ::glfwSetWindowIcon(wnd.get(), gsl::narrow<int>(iconImages.size()), iconImages.data());
    }

    assign_native_handle(std::move(wnd));
}

void linux_application_window::show() {
    auto& nativeHandle{get_native_handle()};
    Expects(nativeHandle.is_valid());

    glfwShowWindow(nativeHandle.get());
}

bool linux_application_window::should_close() const noexcept {
    auto& nativeHandle{get_native_handle()};
    Expects(nativeHandle.is_valid());

    return glfwWindowShouldClose(nativeHandle.get()) == GLFW_TRUE;
}

void linux_application_window::request_close() noexcept {
    auto& nativeHandle{get_native_handle()};
    Expects(nativeHandle.is_valid());

    glfwSetWindowShouldClose(nativeHandle.get(), GLFW_TRUE);
}

linux_application_window::resize_event_t::subscriber&
linux_application_window::on_resize() noexcept {
    return m_resizeEvent.get_subscriber();
}

void linux_application_window::on_create() {}

bool linux_application_window::on_close_requested() {
    return true;
}

void linux_application_window::on_mouse_button([[maybe_unused]] const int button,
                                               [[maybe_unused]] const int action,
                                               [[maybe_unused]] const int mods) {}

void linux_application_window::on_cursor_moved([[maybe_unused]] const double x,
                                               [[maybe_unused]] const double y) {}

void linux_application_window::on_scroll([[maybe_unused]] const double xOffset,
                                         [[maybe_unused]] const double yOffset) {}

void linux_application_window::on_iconify([[maybe_unused]] const bool iconified) {}

void linux_application_window::on_framebuffer_resized([[maybe_unused]] const std::int32_t width,
                                                      [[maybe_unused]] const std::int32_t height) {}

void linux_application_window::on_content_scale_changed([[maybe_unused]] const float xScale,
                                                        [[maybe_unused]] const float yScale) {}

void linux_application_window::window_key_callback(GLFWwindow* const window, const int key,
                                                   const int scanCode, const int action,
                                                   const int mods) noexcept {
    auto* const appWindow{get_app_window_from_glfw_window(window)};
    if (!appWindow) {
        return;
    }

    appWindow->routed_key_event_handler(key, scanCode, action, mods);
}

void linux_application_window::routed_key_event_handler(const int key, const int scanCode,
                                                        const int action,
                                                        [[maybe_unused]] const int mods) noexcept {
    if (!m_inputCollector) {
        return;
    }

    auto optPressedKey{key_from_glfw_key_event(key, scanCode, ::glfwGetPlatform())};
    if (!optPressedKey) {
        return;
    }

    switch (action) {
        case GLFW_PRESS:
            m_inputCollector->on_key_down(optPressedKey.value(), false);
            break;
        case GLFW_REPEAT:
            m_inputCollector->on_key_down(optPressedKey.value(), true);
            break;
        case GLFW_RELEASE:
            m_inputCollector->on_key_up(optPressedKey.value());
            break;
        default:
            break;
    }
}

void linux_application_window::window_unicode_char_callback(GLFWwindow* const window,
                                                            const unsigned int codepoint) noexcept {
    auto* const appWindow{get_app_window_from_glfw_window(window)};
    if (!appWindow) {
        return;
    }

    appWindow->routed_unicode_char_handler(static_cast<char32_t>(codepoint));
}

void linux_application_window::routed_unicode_char_handler(const char32_t codepoint) noexcept {
    if (!m_inputCollector) {
        return;
    }

    m_inputCollector->on_char(codepoint);
}

void linux_application_window::window_focus_changed_callback(GLFWwindow* const window,
                                                             const int focusState) noexcept {
    auto* const appWindow{get_app_window_from_glfw_window(window)};
    if (!appWindow) {
        return;
    }

    if (focusState == GLFW_FALSE) {
        appWindow->on_focus_lost();
    } else {
        appWindow->on_focus_acquired();
    }
}

void linux_application_window::on_focus_lost() noexcept {
    if (!m_inputCollector) {
        return;
    }

    m_inputCollector->on_focus_lost();
}

void linux_application_window::on_focus_acquired() noexcept {}

void linux_application_window::routed_mouse_button_handler(const int button,
                                                           const int action) noexcept {
    if (!m_inputCollector) {
        return;
    }

    const auto optButton{mouse_button_from_glfw_button(button)};
    if (!optButton) {
        return;
    }

    // No capture to take, unlike Win32: while a button is held, X11 and Wayland keep sending the
    // pointer to the window it went down in, so a drag that leaves the window still ends here.
    switch (action) {
        case GLFW_PRESS:
            m_inputCollector->on_mouse_button_down(optButton.value());
            break;
        case GLFW_RELEASE:
            m_inputCollector->on_mouse_button_up(optButton.value());
            break;
        default:
            break;
    }
}

void linux_application_window::routed_cursor_position_handler(const double x,
                                                              const double y) noexcept {
    if (!m_inputCollector) {
        return;
    }

    m_inputCollector->on_pointer_moved(pointer_position_from_glfw_cursor(x, y));
}

void linux_application_window::routed_cursor_enter_handler(const bool entered) noexcept {
    if (!m_inputCollector) {
        return;
    }

    if (entered) {
        m_inputCollector->on_pointer_entered();
    } else {
        m_inputCollector->on_pointer_left();
    }
}

void linux_application_window::routed_scroll_handler(const double xOffset,
                                                     const double yOffset) noexcept {
    if (!m_inputCollector) {
        return;
    }

    m_inputCollector->on_scroll(wheel_delta_from_glfw_scroll(xOffset, yOffset));
}

} // namespace tca::engine::platform::posix_linux
