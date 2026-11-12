// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/linux/linux-application-window.hpp"

// Third-party
#include <GLFW/glfw3.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

// C++ STL
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

// Engine
#include "input/input-collector.hpp"
#include "input/input-frame.hpp"
#include "input/key.hpp"
#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"
#include "testing/linux/windowing-utilities.hpp"

// Utilities
#include "utilities/input/mouse-string-makers.hpp"

namespace {

namespace eplxn = tca::engine::platform::posix_linux;
namespace ein = tca::engine::input;
using ein::key;
using ein::mouse_button;
using ein::pointer_position;
using ein::wheel_delta;
using tca::engine::testing::apply_headless_glfw_window_hints;
using tca::engine::testing::scoped_null_glfw_context;

/**
 * @brief The callback GLFW currently has installed on the window, read through the setter's
 * return value and put straight back. The null platform has no way to inject keyboard events, so
 * tests call the installed callbacks themselves, exactly as GLFW would.
 */
template <typename Callback>
[[nodiscard]] Callback installed_callback(Callback (*const setter)(GLFWwindow*, Callback),
                                          GLFWwindow* const window) {
    const Callback callback{setter(window, nullptr)};
    std::ignore = setter(window, callback);
    return callback;
}

[[nodiscard]] std::unique_ptr<eplxn::linux_application_window> make_window(
    const eplxn::window_creation_params& creationParams = {}) {
    apply_headless_glfw_window_hints();
    return eplxn::linux_application_window::create<eplxn::linux_application_window>(
        "Keyboard test window", creationParams);
}

// Invokes the window's installed callbacks. Each REQUIREs the callback to be there, so a window
// that never wired one fails here rather than silently testing nothing.
void send_key(GLFWwindow* const window, const int glfwKey, const int action, const int mods = 0) {
    const GLFWkeyfun callback{installed_callback(&::glfwSetKeyCallback, window)};
    REQUIRE(callback != nullptr);
    callback(window, glfwKey, ::glfwGetKeyScancode(glfwKey), action, mods);
}

void send_raw_key(GLFWwindow* const window, const int glfwKey, const int scancode,
                  const int action) {
    const GLFWkeyfun callback{installed_callback(&::glfwSetKeyCallback, window)};
    REQUIRE(callback != nullptr);
    callback(window, glfwKey, scancode, action, 0);
}

void send_char(GLFWwindow* const window, const char32_t codePoint) {
    const GLFWcharfun callback{installed_callback(&::glfwSetCharCallback, window)};
    REQUIRE(callback != nullptr);
    callback(window, static_cast<unsigned int>(codePoint));
}

void send_focus(GLFWwindow* const window, const bool focused) {
    const GLFWwindowfocusfun callback{installed_callback(&::glfwSetWindowFocusCallback, window)};
    REQUIRE(callback != nullptr);
    callback(window, focused ? GLFW_TRUE : GLFW_FALSE);
}

void send_mouse_button(GLFWwindow* const window, const int glfwButton, const int action,
                       const int mods = 0) {
    const GLFWmousebuttonfun callback{installed_callback(&::glfwSetMouseButtonCallback, window)};
    REQUIRE(callback != nullptr);
    callback(window, glfwButton, action, mods);
}

void send_cursor_position(GLFWwindow* const window, const double x, const double y) {
    const GLFWcursorposfun callback{installed_callback(&::glfwSetCursorPosCallback, window)};
    REQUIRE(callback != nullptr);
    callback(window, x, y);
}

void send_cursor_enter(GLFWwindow* const window, const bool entered) {
    const GLFWcursorenterfun callback{installed_callback(&::glfwSetCursorEnterCallback, window)};
    REQUIRE(callback != nullptr);
    callback(window, entered ? GLFW_TRUE : GLFW_FALSE);
}

void send_scroll(GLFWwindow* const window, const double xOffset, const double yOffset) {
    const GLFWscrollfun callback{installed_callback(&::glfwSetScrollCallback, window)};
    REQUIRE(callback != nullptr);
    callback(window, xOffset, yOffset);
}

/**
 * @brief A hidden window feeding a collector, and the frame it writes into: what an application
 * owns, without the application.
 */
class keyboard_window final {
public:
    explicit keyboard_window(const eplxn::window_creation_params& creationParams = {})
        : m_window{make_window(creationParams)} {
        REQUIRE(handle() != nullptr);
        m_window->set_input_collector(&m_collector);
    }

    [[nodiscard]] GLFWwindow* handle() const noexcept {
        return m_window->get_native_handle().get();
    }

    [[nodiscard]] eplxn::linux_application_window& window() noexcept {
        return *m_window;
    }

    [[nodiscard]] const ein::keyboard_state& keyboard() const noexcept {
        return m_frame.keyboard();
    }

    [[nodiscard]] std::string_view text() const noexcept {
        return m_frame.text();
    }

    [[nodiscard]] const ein::mouse_state& mouse() const noexcept {
        return m_frame.mouse();
    }

    void end_tick() noexcept {
        m_frame.end_tick();
    }

    void press(const int glfwKey, const int mods = 0) const {
        send_key(handle(), glfwKey, GLFW_PRESS, mods);
    }

    void repeat(const int glfwKey) const {
        send_key(handle(), glfwKey, GLFW_REPEAT);
    }

    void release(const int glfwKey, const int mods = 0) const {
        send_key(handle(), glfwKey, GLFW_RELEASE, mods);
    }

    void chars(const std::u32string_view codePoints) const {
        for (const char32_t codePoint : codePoints) {
            send_char(handle(), codePoint);
        }
    }

    void focus(const bool focused) const {
        send_focus(handle(), focused);
    }

    void press_button(const int glfwButton) const {
        send_mouse_button(handle(), glfwButton, GLFW_PRESS);
    }

    void release_button(const int glfwButton) const {
        send_mouse_button(handle(), glfwButton, GLFW_RELEASE);
    }

    void detach() noexcept {
        m_window->set_input_collector(nullptr);
    }

private:
    // Declared before the window, so the window goes first, and GLFW is terminated last.
    scoped_null_glfw_context m_glfwContext{};
    ein::input_frame m_frame{};
    ein::input_collector m_collector{m_frame};
    std::unique_ptr<eplxn::linux_application_window> m_window{};
};

// True if no key is down and no key has an edge: nothing reached the keyboard.
[[nodiscard]] bool keyboard_untouched(const ein::keyboard_state& keyboard) {
    for (std::size_t i{}; i < ein::key_count; ++i) {
        const auto k{static_cast<key>(i)};
        if (keyboard.is_down(k) || keyboard.press_count(k) != 0 || keyboard.release_count(k) != 0) {
            return false;
        }
    }

    return true;
}

// Keys that are down, in key order: for readable failures.
[[nodiscard]] std::vector<std::string_view> held_keys(const ein::keyboard_state& keyboard) {
    std::vector<std::string_view> held{};
    for (std::size_t i{}; i < ein::key_count; ++i) {
        if (const auto k{static_cast<key>(i)}; keyboard.is_down(k)) {
            held.push_back(ein::key_name(k));
        }
    }

    return held;
}

} // namespace

TEST_CASE("linux_application_window tests - input callbacks are wired",
          "[tca][engine][platform][linux][linux_application_window][input]") {
    scoped_null_glfw_context glfwContext{};

    GIVEN("A window constructed directly, with no collector") {
        apply_headless_glfw_window_hints();
        eplxn::linux_application_window window{"Keyboard test window",
                                               eplxn::window_creation_params{}};
        GLFWwindow* const handle{window.get_native_handle().get()};
        REQUIRE(handle != nullptr);

        THEN("The key, character and focus callbacks are installed") {
            REQUIRE(installed_callback(&::glfwSetKeyCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetCharCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetWindowFocusCallback, handle) != nullptr);
        }

        THEN("The mouse button, cursor, cursor enter and scroll callbacks are installed") {
            REQUIRE(installed_callback(&::glfwSetMouseButtonCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetCursorPosCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetCursorEnterCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetScrollCallback, handle) != nullptr);
        }
    }
}

TEST_CASE("linux_application_window tests - input collector",
          "[tca][engine][platform][linux][linux_application_window][input]") {
    SECTION("Without a collector, keyboard events are harmless") {
        scoped_null_glfw_context glfwContext{};
        const auto window{make_window()};
        GLFWwindow* const handle{window->get_native_handle().get()};
        REQUIRE(handle != nullptr);

        REQUIRE_NOTHROW(send_key(handle, GLFW_KEY_SPACE, GLFW_PRESS));
        REQUIRE_NOTHROW(send_key(handle, GLFW_KEY_SPACE, GLFW_REPEAT));
        REQUIRE_NOTHROW(send_char(handle, U'a'));
        REQUIRE_NOTHROW(send_key(handle, GLFW_KEY_SPACE, GLFW_RELEASE));
        REQUIRE_NOTHROW(send_focus(handle, false));
        REQUIRE_NOTHROW(send_focus(handle, true));
    }

    SECTION("A detached collector receives nothing more") {
        keyboard_window window{};
        window.press(GLFW_KEY_SPACE);
        REQUIRE(window.keyboard().is_down(key::space));

        window.detach();
        window.release(GLFW_KEY_SPACE);
        window.press(GLFW_KEY_A);
        window.chars(U"a");
        window.focus(false);

        THEN("The frame keeps what it had before detach") {
            REQUIRE(window.keyboard().is_down(key::space));
            REQUIRE_FALSE(window.keyboard().was_released(key::space));
            REQUIRE_FALSE(window.keyboard().is_down(key::a));
            REQUIRE(window.text().empty());
        }
    }

    SECTION("Switching collectors feeds only the new one") {
        keyboard_window window{};
        ein::input_frame otherFrame{};
        ein::input_collector otherCollector{otherFrame};

        window.window().set_input_collector(&otherCollector);
        window.press(GLFW_KEY_W);
        window.chars(U"w");

        THEN("The new collector's frame gets the input") {
            REQUIRE(otherFrame.keyboard().is_down(key::w));
            REQUIRE(otherFrame.text() == "w");
        }

        THEN("The old collector's frame gets nothing") {
            REQUIRE(keyboard_untouched(window.keyboard()));
            REQUIRE(window.text().empty());
        }

        // Detach before otherCollector goes out of scope.
        window.detach();
    }

    SECTION("The resize event still fires once the input callbacks are wired") {
        // Both share the window user pointer: wiring input must not break resize.
        keyboard_window window{};

        bool resized{};
        window.window().on_resize().subscribe([&resized](const std::int32_t, const std::int32_t) {
            resized = true;
        });
        ::glfwSetWindowSize(window.handle(), 800, 600);
        window.press(GLFW_KEY_SPACE);

        REQUIRE(resized);
        REQUIRE(window.keyboard().is_down(key::space));
    }
}

TEST_CASE("linux_application_window tests - key events",
          "[tca][engine][platform][linux][linux_application_window][input]") {
    keyboard_window window{};

    SECTION("GLFW_PRESS presses, GLFW_RELEASE releases") {
        window.press(GLFW_KEY_SPACE);

        THEN("The key is down and pressed once") {
            REQUIRE(window.keyboard().is_down(key::space));
            REQUIRE(window.keyboard().press_count(key::space) == 1);
            REQUIRE(window.keyboard().release_count(key::space) == 0);
        }

        AND_WHEN("It goes up") {
            window.release(GLFW_KEY_SPACE);

            THEN("It is up, pressed once and released once") {
                REQUIRE_FALSE(window.keyboard().is_down(key::space));
                REQUIRE(window.keyboard().press_count(key::space) == 1);
                REQUIRE(window.keyboard().release_count(key::space) == 1);
            }
        }
    }

    SECTION("GLFW_REPEAT is not a press") {
        window.press(GLFW_KEY_SPACE);
        window.repeat(GLFW_KEY_SPACE);
        window.repeat(GLFW_KEY_SPACE);

        REQUIRE(window.keyboard().is_down(key::space));
        REQUIRE(window.keyboard().press_count(key::space) == 1);

        AND_WHEN("The tick ends and the key keeps repeating") {
            window.end_tick();
            window.repeat(GLFW_KEY_SPACE);

            THEN("It is still down, with no new press") {
                REQUIRE(window.keyboard().is_down(key::space));
                REQUIRE_FALSE(window.keyboard().was_pressed(key::space));
            }
        }
    }

    SECTION("A repeat of a key that isn't held (already down when focus came) is ignored") {
        window.repeat(GLFW_KEY_SPACE);

        REQUIRE(keyboard_untouched(window.keyboard()));
    }

    SECTION("A release of a key that isn't held is ignored") {
        window.release(GLFW_KEY_SPACE);

        REQUIRE(keyboard_untouched(window.keyboard()));
    }

    SECTION("Each token reaches its own key, twins included") {
        const auto [glfwKey, expected] = GENERATE(table<int, key>({
            {GLFW_KEY_Q,             key::q             },
            {GLFW_KEY_W,             key::w             },
            {GLFW_KEY_ENTER,         key::enter         },
            {GLFW_KEY_KP_ENTER,      key::numpad_enter  },
            {GLFW_KEY_LEFT_CONTROL,  key::control_left  },
            {GLFW_KEY_RIGHT_CONTROL, key::control_right },
            {GLFW_KEY_LEFT_ALT,      key::alt_left      },
            {GLFW_KEY_RIGHT_ALT,     key::alt_right     },
            {GLFW_KEY_LEFT_SHIFT,    key::shift_left    },
            {GLFW_KEY_RIGHT_SHIFT,   key::shift_right   },
            {GLFW_KEY_SLASH,         key::slash         },
            {GLFW_KEY_KP_DIVIDE,     key::numpad_divide },
            {GLFW_KEY_UP,            key::arrow_up      },
            {GLFW_KEY_KP_8,          key::numpad_8      },
            {GLFW_KEY_DELETE,        key::delete_key    },
            {GLFW_KEY_KP_DECIMAL,    key::numpad_decimal},
            {GLFW_KEY_PRINT_SCREEN,  key::print_screen  },
            {GLFW_KEY_PAUSE,         key::pause         },
            {GLFW_KEY_NUM_LOCK,      key::num_lock      },
            {GLFW_KEY_WORLD_1,       key::intl_backslash},
        }));

        CAPTURE(glfwKey, ein::key_name(expected));

        window.press(glfwKey);
        THEN("Only that key is down") {
            REQUIRE(held_keys(window.keyboard()) ==
                    std::vector<std::string_view>{ein::key_name(expected)});
        }
    }

    SECTION("Modifier flags don't change the key") {
        window.press(GLFW_KEY_A, GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT);

        THEN("Only the key itself is down: modifiers are keys of their own") {
            REQUIRE(held_keys(window.keyboard()) ==
                    std::vector<std::string_view>{ein::key_name(key::a)});
        }

        AND_WHEN("It goes up with other modifier flags") {
            window.release(GLFW_KEY_A, GLFW_MOD_SUPER | GLFW_MOD_CAPS_LOCK);

            THEN("It is released") {
                REQUIRE(window.keyboard().was_released(key::a));
                REQUIRE(held_keys(window.keyboard()).empty());
            }
        }
    }

    SECTION("Left and right modifiers are held independently") {
        window.press(GLFW_KEY_LEFT_SHIFT);
        window.press(GLFW_KEY_RIGHT_SHIFT);
        window.release(GLFW_KEY_RIGHT_SHIFT);

        // GLFW reports each Shift's own release, unlike Win32: no "other Shift" release.
        REQUIRE(window.keyboard().is_down(key::shift_left));
        REQUIRE(window.keyboard().release_count(key::shift_left) == 0);
        REQUIRE_FALSE(window.keyboard().is_down(key::shift_right));
        REQUIRE(window.keyboard().release_count(key::shift_right) == 1);
    }

    SECTION("Print Screen is a plain press and release") {
        // No Win32-style key-up-only workaround: GLFW sends both.
        window.press(GLFW_KEY_PRINT_SCREEN);
        window.release(GLFW_KEY_PRINT_SCREEN);

        REQUIRE(window.keyboard().press_count(key::print_screen) == 1);
        REQUIRE(window.keyboard().release_count(key::print_screen) == 1);
    }

    SECTION("On the null platform, a key GLFW has no token for is dropped") {
        // Null platform scancodes are not evdev codes, so there is nothing to fall back to.
        send_raw_key(window.handle(), GLFW_KEY_UNKNOWN, 89, GLFW_PRESS);
        send_raw_key(window.handle(), GLFW_KEY_UNKNOWN, 89, GLFW_RELEASE);

        REQUIRE(keyboard_untouched(window.keyboard()));
    }

    SECTION("A token with no position and no scancode is dropped") {
        send_raw_key(window.handle(), GLFW_KEY_F25, -1, GLFW_PRESS);
        send_raw_key(window.handle(), GLFW_KEY_F25, -1, GLFW_RELEASE);

        REQUIRE(keyboard_untouched(window.keyboard()));
    }

    SECTION("An action GLFW doesn't define is dropped") {
        send_raw_key(window.handle(), GLFW_KEY_SPACE, ::glfwGetKeyScancode(GLFW_KEY_SPACE), 42);

        REQUIRE(keyboard_untouched(window.keyboard()));
    }
}

TEST_CASE("linux_application_window tests - focus loss",
          "[tca][engine][platform][linux][linux_application_window][input]") {
    keyboard_window window{};
    window.press(GLFW_KEY_SPACE);
    window.press(GLFW_KEY_W);
    window.chars(U"x");
    REQUIRE(window.keyboard().is_down(key::space));
    REQUIRE(window.keyboard().is_down(key::w));

    SECTION("Losing focus releases every held key and keeps the text") {
        window.focus(false);

        REQUIRE(held_keys(window.keyboard()).empty());
        REQUIRE(window.keyboard().was_released(key::space));
        REQUIRE(window.keyboard().was_released(key::w));
        REQUIRE(window.text() == "x");
    }

    SECTION("Gaining focus releases nothing") {
        window.focus(true);

        REQUIRE(window.keyboard().is_down(key::space));
        REQUIRE(window.keyboard().is_down(key::w));
        REQUIRE_FALSE(window.keyboard().was_released(key::space));
    }

    SECTION("The key-up that never came, arriving late, is ignored") {
        window.focus(false);
        window.end_tick();
        window.release(GLFW_KEY_SPACE);

        REQUIRE_FALSE(window.keyboard().was_released(key::space));
    }
}

TEST_CASE("linux_application_window tests - focus loss from GLFW itself",
          "[tca][engine][platform][linux][linux_application_window][input]") {
    // Real focus changes, no hand-called callback: the null platform moves focus between visible
    // windows and tells the one losing it.
    constexpr eplxn::window_creation_params visibleAndFocused{.visible = true, .focused = true};

    keyboard_window window{visibleAndFocused};
    REQUIRE(::glfwGetWindowAttrib(window.handle(), GLFW_FOCUSED) == GLFW_TRUE);
    window.press(GLFW_KEY_SPACE);

    WHEN("Another window takes focus") {
        const auto other{make_window(visibleAndFocused)};
        ::glfwFocusWindow(other->get_native_handle().get());
        REQUIRE(::glfwGetWindowAttrib(window.handle(), GLFW_FOCUSED) == GLFW_FALSE);

        THEN("The first window's held keys are released") {
            REQUIRE(held_keys(window.keyboard()).empty());
            REQUIRE(window.keyboard().was_released(key::space));
        }
    }
}

TEST_CASE("linux_application_window tests - typed text",
          "[tca][engine][platform][linux][linux_application_window][input]") {
    keyboard_window window{};

    SECTION("Code points become UTF-8 text") {
        SECTION("ASCII") {
            window.chars(U"Hi!");
            REQUIRE(window.text() == "Hi!");
        }

        SECTION("Two-byte UTF-8 (é)") {
            window.chars(U"é");
            REQUIRE(window.text() == "\xC3\xA9");
        }

        SECTION("Three-byte UTF-8 (中, an IME result)") {
            window.chars(U"中");
            REQUIRE(window.text() == "\xE4\xB8\xAD");
        }

        SECTION("Four-byte UTF-8 arrives as one code point, no surrogate pairing (U+1F600)") {
            window.chars(U"\U0001F600");
            REQUIRE(window.text() == "\xF0\x9F\x98\x80");
        }

        SECTION("Text keeps arriving in order") {
            window.chars(U"a\U0001F600b");
            REQUIRE(window.text() ==
                    "a\xF0\x9F\x98\x80"
                    "b");
        }

        SECTION("Text accumulates across calls within a tick and starts over on the next") {
            window.chars(U"ab");
            window.chars(U"c");
            REQUIRE(window.text() == "abc");

            window.end_tick();
            window.chars(U"d");
            REQUIRE(window.text() == "d");
        }
    }

    SECTION("Values that are not text are dropped") {
        SECTION("Control characters") {
            window.chars(U"\x08\x0D\x1B\x7F\x9F");
            REQUIRE(window.text().empty());
        }

        SECTION("Surrogates and values beyond U+10FFFF") {
            send_char(window.handle(), char32_t{0xD83D});
            send_char(window.handle(), char32_t{0xDE00});
            send_char(window.handle(), char32_t{0x110000});
            REQUIRE(window.text().empty());
        }
    }

    SECTION("Text and keys are separate channels") {
        window.press(GLFW_KEY_A);
        REQUIRE(window.text().empty());

        window.chars(U"a");
        REQUIRE(window.text() == "a");
        REQUIRE(window.keyboard().press_count(key::a) == 1);
    }
}

TEST_CASE("linux_application_window tests - a moved window keeps receiving its events",
          "[tca][engine][platform][linux][linux_application_window][input]") {
    scoped_null_glfw_context glfwContext{};
    ein::input_frame frame{};
    ein::input_collector collector{frame};

    // The source lives on the heap so it can be destroyed right after the move: any callback
    // still routed to it would then reach freed memory.
    auto source{make_window()};
    GLFWwindow* const handle{source->get_native_handle().get()};
    REQUIRE(handle != nullptr);
    source->set_input_collector(&collector);

    bool resized{};
    source->on_resize().subscribe([&resized](const std::int32_t, const std::int32_t) {
        resized = true;
    });

    // The GLFW window, the collector and the resize subscribers all go with the move.
    const auto requireEventsReach = [&](eplxn::linux_application_window& target) {
        THEN("GLFW routes the window's callbacks to the new object") {
            REQUIRE(target.get_native_handle().get() == handle);
            REQUIRE(::glfwGetWindowUserPointer(handle) == &target);
        }

        THEN("Keys, text and focus loss reach the collector") {
            send_key(handle, GLFW_KEY_SPACE, GLFW_PRESS);
            send_char(handle, U'a');
            REQUIRE(frame.keyboard().is_down(key::space));
            REQUIRE(frame.text() == "a");

            send_focus(handle, false);
            REQUIRE(frame.keyboard().was_released(key::space));
        }

        THEN("The mouse reaches the collector") {
            send_mouse_button(handle, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS);
            send_cursor_position(handle, 4.0, 5.0);
            send_scroll(handle, 0.0, 1.0);
            REQUIRE(frame.mouse().is_down(mouse_button::left));
            REQUIRE(frame.mouse().pointer() == pointer_position{.x = 4, .y = 5});
            REQUIRE(frame.mouse().wheel() == wheel_delta{.vertical = 1.0F});
        }

        THEN("The resize subscribers are still notified") {
            ::glfwSetWindowSize(handle, 800, 600);
            REQUIRE(resized);
        }
    };

    WHEN("It is move-constructed into another window and the source is destroyed") {
        eplxn::linux_application_window target{std::move(*source)};
        source.reset();

        requireEventsReach(target);
    }

    WHEN("It is move-assigned over another window and the source is destroyed") {
        const auto target{make_window()};
        target->set_input_collector(nullptr);
        *target = std::move(*source);
        source.reset();

        requireEventsReach(*target);
    }
}

namespace {

// True if no button is down and no button has an edge: nothing reached the buttons.
[[nodiscard]] bool buttons_untouched(const ein::mouse_state& mouse) {
    for (std::size_t i{}; i < ein::MOUSE_BUTTONS_COUNT; ++i) {
        const auto button{static_cast<mouse_button>(i)};
        if (mouse.is_down(button) || mouse.press_count(button) != 0 ||
            mouse.release_count(button) != 0) {
            return false;
        }
    }

    return true;
}

} // namespace

TEST_CASE("linux_application_window tests - mouse without a collector",
          "[tca][engine][platform][linux][linux_application_window][input][mouse]") {
    SECTION("Mouse events are harmless") {
        scoped_null_glfw_context glfwContext{};
        const auto window{make_window()};
        GLFWwindow* const handle{window->get_native_handle().get()};
        REQUIRE(handle != nullptr);

        REQUIRE_NOTHROW(send_mouse_button(handle, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS));
        REQUIRE_NOTHROW(send_cursor_position(handle, 1.0, 1.0));
        REQUIRE_NOTHROW(send_cursor_enter(handle, true));
        REQUIRE_NOTHROW(send_scroll(handle, 0.0, 1.0));
        REQUIRE_NOTHROW(send_mouse_button(handle, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE));
    }

    SECTION("A detached collector receives nothing more") {
        keyboard_window window{};
        window.press_button(GLFW_MOUSE_BUTTON_LEFT);
        REQUIRE(window.mouse().is_down(mouse_button::left));

        window.detach();
        window.release_button(GLFW_MOUSE_BUTTON_LEFT);
        window.press_button(GLFW_MOUSE_BUTTON_RIGHT);
        send_cursor_position(window.handle(), 3.0, 3.0);
        send_scroll(window.handle(), 0.0, 1.0);

        THEN("The frame keeps what it had before detach") {
            REQUIRE(window.mouse().is_down(mouse_button::left));
            REQUIRE_FALSE(window.mouse().was_released(mouse_button::left));
            REQUIRE_FALSE(window.mouse().is_down(mouse_button::right));
            REQUIRE_FALSE(window.mouse().pointer().has_value());
            REQUIRE(window.mouse().wheel() == wheel_delta{});
        }
    }
}

TEST_CASE("linux_application_window tests - mouse buttons",
          "[tca][engine][platform][linux][linux_application_window][input][mouse]") {
    keyboard_window window{};

    SECTION("GLFW_PRESS presses, GLFW_RELEASE releases, each button in its own slot") {
        const auto [glfwButton, expected] = GENERATE(table<int, mouse_button>({
            {GLFW_MOUSE_BUTTON_LEFT,   mouse_button::left  },
            {GLFW_MOUSE_BUTTON_RIGHT,  mouse_button::right },
            {GLFW_MOUSE_BUTTON_MIDDLE, mouse_button::middle},
            {GLFW_MOUSE_BUTTON_4,      mouse_button::x1    },
            {GLFW_MOUSE_BUTTON_5,      mouse_button::x2    },
        }));

        CAPTURE(glfwButton, ein::mouse_button_name(expected));

        window.press_button(glfwButton);

        THEN("Only that button is down, pressed once") {
            REQUIRE(window.mouse().is_down(expected));
            REQUIRE(window.mouse().press_count(expected) == 1);
            for (std::size_t i{}; i < ein::MOUSE_BUTTONS_COUNT; ++i) {
                const auto other{static_cast<mouse_button>(i)};
                if (other != expected) {
                    CAPTURE(ein::mouse_button_name(other));
                    REQUIRE_FALSE(window.mouse().is_down(other));
                }
            }
        }

        AND_WHEN("It goes up") {
            window.release_button(glfwButton);

            THEN("It is up, pressed once and released once") {
                REQUIRE_FALSE(window.mouse().is_down(expected));
                REQUIRE(window.mouse().press_count(expected) == 1);
                REQUIRE(window.mouse().release_count(expected) == 1);
            }
        }
    }

    SECTION("Buttons past the fifth have no engine button and are dropped") {
        for (const int glfwButton :
             {GLFW_MOUSE_BUTTON_6, GLFW_MOUSE_BUTTON_7, GLFW_MOUSE_BUTTON_8}) {
            window.press_button(glfwButton);
            window.release_button(glfwButton);
        }

        REQUIRE(buttons_untouched(window.mouse()));
    }

    SECTION("A release of a button that isn't held is ignored") {
        window.release_button(GLFW_MOUSE_BUTTON_LEFT);

        REQUIRE(buttons_untouched(window.mouse()));
    }

    SECTION("An action GLFW doesn't define is dropped") {
        send_mouse_button(window.handle(), GLFW_MOUSE_BUTTON_LEFT, 42);

        REQUIRE(buttons_untouched(window.mouse()));
    }

    SECTION("Modifier flags don't change the button, and press no key") {
        send_mouse_button(window.handle(), GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS,
                          GLFW_MOD_SHIFT | GLFW_MOD_CONTROL);

        REQUIRE(window.mouse().is_down(mouse_button::left));
        REQUIRE(keyboard_untouched(window.keyboard()));
    }

    SECTION("Two clicks within one tick are both counted") {
        window.press_button(GLFW_MOUSE_BUTTON_LEFT);
        window.release_button(GLFW_MOUSE_BUTTON_LEFT);
        window.press_button(GLFW_MOUSE_BUTTON_LEFT);
        window.release_button(GLFW_MOUSE_BUTTON_LEFT);

        REQUIRE(window.mouse().press_count(mouse_button::left) == 2);
        REQUIRE(window.mouse().release_count(mouse_button::left) == 2);
    }
}

TEST_CASE("linux_application_window tests - pointer",
          "[tca][engine][platform][linux][linux_application_window][input][mouse]") {
    keyboard_window window{};

    SECTION("The cursor position is the pointer position") {
        send_cursor_position(window.handle(), 120.0, 45.0);

        REQUIRE(window.mouse().pointer() == pointer_position{.x = 120, .y = 45});
    }

    SECTION("Sub-pixel positions round down, so just outside the left edge is negative") {
        send_cursor_position(window.handle(), 10.75, 3.25);
        REQUIRE(window.mouse().pointer() == pointer_position{.x = 10, .y = 3});

        send_cursor_position(window.handle(), -0.5, -12.25);
        REQUIRE(window.mouse().pointer() == pointer_position{.x = -1, .y = -13});
    }

    SECTION("A drag that leaves the window keeps being followed") {
        window.press_button(GLFW_MOUSE_BUTTON_LEFT);
        send_cursor_enter(window.handle(), false);
        send_cursor_position(window.handle(), -30.0, -40.0);
        window.release_button(GLFW_MOUSE_BUTTON_LEFT);

        THEN("The release is reported, with the pointer outside where it ended") {
            REQUIRE(window.mouse().release_count(mouse_button::left) == 1);
            REQUIRE(window.mouse().pointer() == pointer_position{.x = -30, .y = -40});
            REQUIRE_FALSE(window.mouse().is_pointer_inside());
        }
    }

    SECTION("Cursor enter and leave tell whether it is over the content area") {
        send_cursor_enter(window.handle(), true);
        REQUIRE(window.mouse().is_pointer_inside());

        send_cursor_position(window.handle(), 7.0, 8.0);
        send_cursor_enter(window.handle(), false);

        REQUIRE_FALSE(window.mouse().is_pointer_inside());
        REQUIRE(window.mouse().pointer() == pointer_position{.x = 7, .y = 8});
    }

    SECTION("The pointer survives end_tick()") {
        send_cursor_enter(window.handle(), true);
        send_cursor_position(window.handle(), 7.0, 8.0);
        window.end_tick();

        REQUIRE(window.mouse().is_pointer_inside());
        REQUIRE(window.mouse().pointer() == pointer_position{.x = 7, .y = 8});
    }
}

TEST_CASE("linux_application_window tests - mouse wheel",
          "[tca][engine][platform][linux][linux_application_window][input][mouse]") {
    keyboard_window window{};

    SECTION("The vertical offset is vertical, positive scrolling up") {
        send_scroll(window.handle(), 0.0, 1.0);
        send_scroll(window.handle(), 0.0, 0.5);

        REQUIRE(window.mouse().wheel() == wheel_delta{.vertical = 1.5F});
    }

    SECTION("The horizontal offset is flipped: GLFW's is positive to the left") {
        send_scroll(window.handle(), -1.0, 0.0);
        REQUIRE(window.mouse().wheel() == wheel_delta{.horizontal = 1.0F});

        send_scroll(window.handle(), 2.0, 0.0);
        REQUIRE(window.mouse().wheel() == wheel_delta{.horizontal = -1.0F});
    }

    SECTION("end_tick() resets the wheel") {
        send_scroll(window.handle(), 0.0, 1.0);
        window.end_tick();

        REQUIRE(window.mouse().wheel() == wheel_delta{});
    }

    SECTION("Scrolling doesn't move the pointer") {
        send_cursor_position(window.handle(), 3.0, 4.0);
        send_scroll(window.handle(), 0.0, 1.0);

        REQUIRE(window.mouse().pointer() == pointer_position{.x = 3, .y = 4});
    }
}

TEST_CASE("linux_application_window tests - focus loss and the mouse",
          "[tca][engine][platform][linux][linux_application_window][input][mouse]") {
    keyboard_window window{};
    window.press_button(GLFW_MOUSE_BUTTON_LEFT);
    window.press_button(GLFW_MOUSE_BUTTON_5);
    send_cursor_position(window.handle(), 1.0, 1.0);
    window.end_tick();
    send_scroll(window.handle(), 0.0, 1.0);

    SECTION("Losing focus releases every held button, once, and leaves the pointer and wheel") {
        window.focus(false);

        REQUIRE_FALSE(window.mouse().is_down(mouse_button::left));
        REQUIRE_FALSE(window.mouse().is_down(mouse_button::x2));
        REQUIRE(window.mouse().release_count(mouse_button::left) == 1);
        REQUIRE(window.mouse().release_count(mouse_button::x2) == 1);
        REQUIRE(window.mouse().pointer() == pointer_position{.x = 1, .y = 1});
        REQUIRE(window.mouse().wheel() == wheel_delta{.vertical = 1.0F});
    }

    SECTION("The button-up that never came, arriving late, is ignored") {
        window.focus(false);
        window.end_tick();
        window.release_button(GLFW_MOUSE_BUTTON_LEFT);

        REQUIRE_FALSE(window.mouse().was_released(mouse_button::left));
    }
}
