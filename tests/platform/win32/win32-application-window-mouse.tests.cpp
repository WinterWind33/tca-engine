// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/win32/win32-application-window.hpp"

// C++ STL
#include <cstddef>
#include <cstdint>
#include <memory>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

// Engine
#include "input/input-collector.hpp"
#include "input/input-frame.hpp"
#include "input/key.hpp"
#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"

// Utilities
#include "utilities/input/mouse-string-makers.hpp"
#include "utilities/win32/mouse-messages.hpp"
#include "utilities/win32/wm-quit-drain.hpp"

namespace {

namespace epw32n = tca::engine::platform::win32;
namespace ein = tca::engine::input;
using ein::mouse_button;
using ein::pointer_position;
using ein::wheel_delta;

using tests::utilities::mouse_lparam;
using tests::utilities::wheel_wparam;
using tests::utilities::x_button_wparam;

/**
 * @brief A hidden window feeding a collector, and the frame it writes into: what an application
 * owns, without the application. Mouse messages are sent straight into the window procedure.
 */
class mouse_window final {
public:
    mouse_window()
        : m_window{epw32n::win32_application_window::two_phase_window_creation<
              epw32n::win32_application_window>(TEXT("Mouse test window"))} {
        REQUIRE(hwnd() != nullptr);
        m_window->set_input_collector(&m_collector);
    }

    mouse_window(const mouse_window&) = delete;
    mouse_window& operator=(const mouse_window&) = delete;

    ~mouse_window() {
        // A test that failed mid-drag must not leave the capture to the next one.
        if (::GetCapture() == hwnd()) {
            ::ReleaseCapture();
        }
    }

    [[nodiscard]] HWND hwnd() const noexcept {
        return m_window->get_native_handle().get();
    }

    [[nodiscard]] const ein::mouse_state& mouse() const noexcept {
        return m_frame.mouse();
    }

    [[nodiscard]] const ein::keyboard_state& keyboard() const noexcept {
        return m_frame.keyboard();
    }

    void end_tick() noexcept {
        m_frame.end_tick();
    }

    void send(const UINT message, const WPARAM wparam, const LPARAM lparam) const {
        ::SendMessage(hwnd(), message, wparam, lparam);
    }

    void move(const std::int16_t x, const std::int16_t y) const {
        send(WM_MOUSEMOVE, 0, mouse_lparam(x, y));
    }

    // A point well inside the client area, whatever size the window was given.
    [[nodiscard]] pointer_position inside_point() const {
        RECT client{};
        REQUIRE(::GetClientRect(hwnd(), &client) != 0);
        REQUIRE(client.right > 2);
        REQUIRE(client.bottom > 2);
        return {.x = static_cast<std::int32_t>(client.right / 2),
                .y = static_cast<std::int32_t>(client.bottom / 2)};
    }

    void detach() noexcept {
        m_window->set_input_collector(nullptr);
    }

private:
    // Declared before the window, so the window (and its procedure) goes first.
    ein::input_frame m_frame{};
    ein::input_collector m_collector{m_frame};
    std::unique_ptr<epw32n::win32_application_window> m_window{};
};

[[nodiscard]] std::int16_t to_word(const std::int32_t value) {
    return static_cast<std::int16_t>(value);
}

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

/**
 * @brief One way a button goes down and up in Win32: the messages and their WPARAMs.
 */
struct button_messages final {
    mouse_button button{};
    UINT down{};
    UINT up{};
    UINT doubleClick{};
    WPARAM downWparam{};
    WPARAM upWparam{};
};

} // namespace

TEST_CASE("win32_application_window mouse without a collector",
          "[tca][engine][platform][win32][win32_application_window][input][mouse]") {
    tests::utilities::drain_wm_quit_messages();

    SECTION("Mouse messages are harmless") {
        auto window{epw32n::win32_application_window::two_phase_window_creation<
            epw32n::win32_application_window>(TEXT("Mouse test window"))};
        const HWND hwnd{window->get_native_handle().get()};
        REQUIRE(hwnd != nullptr);

        REQUIRE_NOTHROW(::SendMessage(hwnd, WM_MOUSEMOVE, 0, mouse_lparam(1, 1)));
        REQUIRE_NOTHROW(::SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, mouse_lparam(1, 1)));
        REQUIRE_NOTHROW(::SendMessage(hwnd, WM_LBUTTONUP, 0, mouse_lparam(1, 1)));
        REQUIRE_NOTHROW(::SendMessage(hwnd, WM_MOUSEWHEEL, wheel_wparam(WHEEL_DELTA), 0));
        REQUIRE_NOTHROW(::SendMessage(hwnd, WM_MOUSELEAVE, 0, 0));
        REQUIRE(::GetCapture() != hwnd);
    }

    SECTION("A detached collector receives nothing more") {
        mouse_window window{};
        window.send(WM_LBUTTONDOWN, MK_LBUTTON, mouse_lparam(1, 1));
        REQUIRE(window.mouse().is_down(mouse_button::left));

        window.detach();
        window.send(WM_LBUTTONUP, 0, mouse_lparam(2, 2));
        window.send(WM_RBUTTONDOWN, MK_RBUTTON, mouse_lparam(3, 3));
        window.send(WM_MOUSEWHEEL, wheel_wparam(WHEEL_DELTA), 0);

        THEN("The frame keeps what it had before detach") {
            REQUIRE(window.mouse().is_down(mouse_button::left));
            REQUIRE_FALSE(window.mouse().was_released(mouse_button::left));
            REQUIRE_FALSE(window.mouse().is_down(mouse_button::right));
            REQUIRE(window.mouse().pointer() == pointer_position{.x = 1, .y = 1});
            REQUIRE(window.mouse().wheel() == wheel_delta{});
        }
    }
}

TEST_CASE("win32_application_window mouse buttons",
          "[tca][engine][platform][win32][win32_application_window][input][mouse]") {
    tests::utilities::drain_wm_quit_messages();

    mouse_window window{};

    SECTION("Each button's down and up messages press and release it") {
        const auto messages = GENERATE(values<button_messages>({
            {mouse_button::left, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK, MK_LBUTTON, 0},
            {mouse_button::right, WM_RBUTTONDOWN, WM_RBUTTONUP, WM_RBUTTONDBLCLK, MK_RBUTTON, 0},
            {mouse_button::middle, WM_MBUTTONDOWN, WM_MBUTTONUP, WM_MBUTTONDBLCLK, MK_MBUTTON, 0},
            {mouse_button::x1, WM_XBUTTONDOWN, WM_XBUTTONUP, WM_XBUTTONDBLCLK,
             x_button_wparam(XBUTTON1, MK_XBUTTON1), x_button_wparam(XBUTTON1)},
            {mouse_button::x2, WM_XBUTTONDOWN, WM_XBUTTONUP, WM_XBUTTONDBLCLK,
             x_button_wparam(XBUTTON2, MK_XBUTTON2), x_button_wparam(XBUTTON2)},
        }));

        CAPTURE(ein::mouse_button_name(messages.button));

        window.send(messages.down, messages.downWparam, mouse_lparam(5, 6));

        THEN("The button is down and pressed once, and no other button is touched") {
            REQUIRE(window.mouse().is_down(messages.button));
            REQUIRE(window.mouse().press_count(messages.button) == 1);
            for (std::size_t i{}; i < ein::MOUSE_BUTTONS_COUNT; ++i) {
                const auto other{static_cast<mouse_button>(i)};
                if (other != messages.button) {
                    CAPTURE(ein::mouse_button_name(other));
                    REQUIRE_FALSE(window.mouse().is_down(other));
                    REQUIRE(window.mouse().press_count(other) == 0);
                }
            }
        }

        AND_WHEN("It goes up") {
            window.send(messages.up, messages.upWparam, mouse_lparam(5, 6));

            THEN("It is up, pressed once and released once") {
                REQUIRE_FALSE(window.mouse().is_down(messages.button));
                REQUIRE(window.mouse().press_count(messages.button) == 1);
                REQUIRE(window.mouse().release_count(messages.button) == 1);
            }
        }

        AND_WHEN("It is double-clicked (the double-click message replaces the second down)") {
            window.send(messages.up, messages.upWparam, mouse_lparam(5, 6));
            window.send(messages.doubleClick, messages.downWparam, mouse_lparam(5, 6));
            window.send(messages.up, messages.upWparam, mouse_lparam(5, 6));

            THEN("Both clicks count") {
                REQUIRE(window.mouse().press_count(messages.button) == 2);
                REQUIRE(window.mouse().release_count(messages.button) == 2);
                REQUIRE_FALSE(window.mouse().is_down(messages.button));
            }
        }
    }

    SECTION("A button message moves the pointer to where it happened") {
        window.send(WM_RBUTTONDOWN, MK_RBUTTON, mouse_lparam(42, 17));

        REQUIRE(window.mouse().pointer() == pointer_position{.x = 42, .y = 17});
    }

    SECTION("A button up for a button that isn't held reports nothing") {
        window.send(WM_LBUTTONUP, 0, mouse_lparam(1, 1));

        REQUIRE(buttons_untouched(window.mouse()));
    }

    SECTION("An X button message with no X button in it is dropped") {
        window.send(WM_XBUTTONDOWN, x_button_wparam(0), mouse_lparam(1, 1));

        REQUIRE(buttons_untouched(window.mouse()));
    }

    SECTION("Clicks on the title bar and the frame (non-client) are not buttons") {
        window.send(WM_NCLBUTTONDOWN, HTCAPTION, mouse_lparam(1, 1));
        window.send(WM_NCLBUTTONUP, HTCAPTION, mouse_lparam(1, 1));
        window.send(WM_NCRBUTTONDOWN, HTCAPTION, mouse_lparam(1, 1));

        REQUIRE(buttons_untouched(window.mouse()));
    }

    SECTION("Buttons leave the keyboard alone") {
        window.send(WM_LBUTTONDOWN, MK_LBUTTON, mouse_lparam(1, 1));

        for (std::size_t i{}; i < ein::key_count; ++i) {
            REQUIRE_FALSE(window.keyboard().is_down(static_cast<ein::key>(i)));
        }
    }
}

TEST_CASE("win32_application_window mouse capture",
          "[tca][engine][platform][win32][win32_application_window][input][mouse]") {
    tests::utilities::drain_wm_quit_messages();

    mouse_window window{};

    SECTION("A button down captures the mouse, so a drag that leaves the window still ends") {
        window.send(WM_LBUTTONDOWN, MK_LBUTTON, mouse_lparam(10, 10));

        REQUIRE(::GetCapture() == window.hwnd());

        AND_WHEN("The drag leaves the client area and the button goes up out there") {
            window.send(WM_MOUSEMOVE, MK_LBUTTON, mouse_lparam(-30, -40));
            window.send(WM_LBUTTONUP, 0, mouse_lparam(-35, -45));

            THEN("The release is reported where it happened, and the capture is let go") {
                REQUIRE_FALSE(window.mouse().is_down(mouse_button::left));
                REQUIRE(window.mouse().release_count(mouse_button::left) == 1);
                REQUIRE(window.mouse().pointer() == pointer_position{.x = -35, .y = -45});
                REQUIRE(::GetCapture() != window.hwnd());
            }
        }
    }

    SECTION("The capture is kept until the last held button goes up") {
        window.send(WM_LBUTTONDOWN, MK_LBUTTON, mouse_lparam(1, 1));
        window.send(WM_RBUTTONDOWN, MK_LBUTTON | MK_RBUTTON, mouse_lparam(1, 1));

        window.send(WM_LBUTTONUP, MK_RBUTTON, mouse_lparam(1, 1));
        REQUIRE(::GetCapture() == window.hwnd());

        window.send(WM_RBUTTONUP, 0, mouse_lparam(1, 1));
        REQUIRE(::GetCapture() != window.hwnd());
    }

    SECTION("Losing the capture to another window releases the held buttons") {
        // Their button-ups go to the window that took the capture, never to this one.
        auto thief{epw32n::win32_application_window::two_phase_window_creation<
            epw32n::win32_application_window>(TEXT("Capture thief window"))};
        REQUIRE(thief->get_native_handle().get() != nullptr);

        window.send(WM_LBUTTONDOWN, MK_LBUTTON, mouse_lparam(1, 1));
        window.send(WM_MBUTTONDOWN, MK_LBUTTON | MK_MBUTTON, mouse_lparam(1, 1));
        window.end_tick();

        ::SetCapture(thief->get_native_handle().get());

        THEN("Each held button reports one release") {
            REQUIRE_FALSE(window.mouse().is_down(mouse_button::left));
            REQUIRE_FALSE(window.mouse().is_down(mouse_button::middle));
            REQUIRE(window.mouse().release_count(mouse_button::left) == 1);
            REQUIRE(window.mouse().release_count(mouse_button::middle) == 1);
        }

        ::ReleaseCapture();
    }
}

TEST_CASE("win32_application_window pointer",
          "[tca][engine][platform][win32][win32_application_window][input][mouse]") {
    tests::utilities::drain_wm_quit_messages();

    mouse_window window{};
    const auto inside{window.inside_point()};

    SECTION("WM_MOUSEMOVE sets the position, in client pixels") {
        window.move(to_word(inside.x), to_word(inside.y));

        REQUIRE(window.mouse().pointer() == inside);
    }

    SECTION("The first move over the client area enters it") {
        REQUIRE_FALSE(window.mouse().is_pointer_inside());

        window.move(to_word(inside.x), to_word(inside.y));

        REQUIRE(window.mouse().is_pointer_inside());
    }

    SECTION("WM_MOUSELEAVE leaves it, keeping the last position") {
        window.move(to_word(inside.x), to_word(inside.y));
        window.send(WM_MOUSELEAVE, 0, 0);

        REQUIRE_FALSE(window.mouse().is_pointer_inside());
        REQUIRE(window.mouse().pointer() == inside);

        AND_WHEN("It moves over the client area again") {
            window.move(to_word(inside.x + 1), to_word(inside.y));

            THEN("It is inside again") {
                REQUIRE(window.mouse().is_pointer_inside());
            }
        }
    }

    SECTION("A captured move outside the client area is outside, with a negative position") {
        window.move(to_word(inside.x), to_word(inside.y));
        window.send(WM_LBUTTONDOWN, MK_LBUTTON, mouse_lparam(to_word(inside.x), to_word(inside.y)));
        window.send(WM_MOUSEMOVE, MK_LBUTTON, mouse_lparam(-5, -7));

        REQUIRE(window.mouse().pointer() == pointer_position{.x = -5, .y = -7});
        REQUIRE_FALSE(window.mouse().is_pointer_inside());

        AND_WHEN("The drag comes back in") {
            window.send(WM_MOUSEMOVE, MK_LBUTTON,
                        mouse_lparam(to_word(inside.x), to_word(inside.y)));

            THEN("It is inside again") {
                REQUIRE(window.mouse().is_pointer_inside());
            }
        }
    }

    SECTION("Moves on the title bar and the frame (non-client) don't move the pointer") {
        window.send(WM_NCMOUSEMOVE, HTCAPTION, mouse_lparam(300, 300));

        REQUIRE_FALSE(window.mouse().pointer().has_value());
    }

    SECTION("The pointer survives end_tick()") {
        window.move(to_word(inside.x), to_word(inside.y));
        window.end_tick();

        REQUIRE(window.mouse().pointer() == inside);
        REQUIRE(window.mouse().is_pointer_inside());
    }
}

TEST_CASE("win32_application_window mouse wheel",
          "[tca][engine][platform][win32][win32_application_window][input][mouse]") {
    tests::utilities::drain_wm_quit_messages();

    mouse_window window{};

    SECTION("WM_MOUSEWHEEL is vertical, in notches of WHEEL_DELTA") {
        const auto [delta, expected] = GENERATE(table<std::int16_t, float>({
            {WHEEL_DELTA,      1.0F  },
            {-WHEEL_DELTA,     -1.0F },
            {2 * WHEEL_DELTA,  2.0F  },
            // Smooth-scrolling wheels and touchpads send fractions of a notch.
            {WHEEL_DELTA / 2,  0.5F  },
            {-WHEEL_DELTA / 4, -0.25F},
        }));

        CAPTURE(delta);

        window.send(WM_MOUSEWHEEL, wheel_wparam(delta), 0);

        REQUIRE(window.mouse().wheel() == wheel_delta{.vertical = expected});
    }

    SECTION("WM_MOUSEHWHEEL is horizontal, positive to the right") {
        window.send(WM_MOUSEHWHEEL, wheel_wparam(WHEEL_DELTA), 0);
        window.send(WM_MOUSEHWHEEL, wheel_wparam(WHEEL_DELTA / 2), 0);

        REQUIRE(window.mouse().wheel() == wheel_delta{.horizontal = 1.5F});
    }

    SECTION("Turns within one tick are summed per axis") {
        window.send(WM_MOUSEWHEEL, wheel_wparam(WHEEL_DELTA), 0);
        window.send(WM_MOUSEWHEEL, wheel_wparam(WHEEL_DELTA), 0);
        window.send(WM_MOUSEHWHEEL, wheel_wparam(-WHEEL_DELTA), 0);

        REQUIRE(window.mouse().wheel() == wheel_delta{.vertical = 2.0F, .horizontal = -1.0F});
    }

    SECTION("The wheel's position is in screen coordinates, so it doesn't move the pointer") {
        window.move(3, 4);
        window.send(WM_MOUSEWHEEL, wheel_wparam(WHEEL_DELTA), mouse_lparam(900, 700));

        REQUIRE(window.mouse().pointer() == pointer_position{.x = 3, .y = 4});
    }

    SECTION("Held buttons in the wheel message are not presses") {
        window.send(WM_MOUSEWHEEL, wheel_wparam(WHEEL_DELTA, MK_LBUTTON | MK_SHIFT), 0);

        REQUIRE(buttons_untouched(window.mouse()));
    }
}

TEST_CASE("win32_application_window focus loss and the mouse",
          "[tca][engine][platform][win32][win32_application_window][input][mouse]") {
    tests::utilities::drain_wm_quit_messages();

    mouse_window window{};
    window.send(WM_LBUTTONDOWN, MK_LBUTTON, mouse_lparam(1, 1));
    window.send(WM_XBUTTONDOWN, x_button_wparam(XBUTTON2, MK_LBUTTON | MK_XBUTTON2),
                mouse_lparam(1, 1));
    window.send(WM_MOUSEWHEEL, wheel_wparam(WHEEL_DELTA), 0);
    window.end_tick();
    window.send(WM_MOUSEWHEEL, wheel_wparam(WHEEL_DELTA), 0);

    const auto [message, wparam] = GENERATE(table<UINT, WPARAM>({
        {WM_KILLFOCUS,   0    },
        {WM_ACTIVATEAPP, FALSE},
    }));

    CAPTURE(message);

    window.send(message, wparam, 0);

    THEN("Every held button is released, once") {
        REQUIRE_FALSE(window.mouse().is_down(mouse_button::left));
        REQUIRE_FALSE(window.mouse().is_down(mouse_button::x2));
        REQUIRE(window.mouse().release_count(mouse_button::left) == 1);
        REQUIRE(window.mouse().release_count(mouse_button::x2) == 1);
    }

    THEN("The pointer and the wheel are left alone") {
        REQUIRE(window.mouse().pointer() == pointer_position{.x = 1, .y = 1});
        REQUIRE(window.mouse().wheel() == wheel_delta{.vertical = 1.0F});
    }

    THEN("The capture is let go") {
        REQUIRE(::GetCapture() != window.hwnd());
    }

    AND_WHEN("The button-ups that never came arrive late") {
        window.end_tick();
        window.send(WM_LBUTTONUP, 0, mouse_lparam(1, 1));

        THEN("They are ignored") {
            REQUIRE_FALSE(window.mouse().was_released(mouse_button::left));
        }
    }
}
