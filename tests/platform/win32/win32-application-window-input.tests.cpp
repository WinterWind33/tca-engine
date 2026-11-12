// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/win32/win32-application-window.hpp"

// C++ STL
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

// Engine
#include "input/input-collector.hpp"
#include "input/input-frame.hpp"
#include "input/key.hpp"

// Utilities
#include "utilities/win32/keyboard-messages.hpp"
#include "utilities/win32/mouse-messages.hpp"
#include "utilities/win32/wm-quit-drain.hpp"

namespace {

namespace epn = tca::engine::platform;
namespace epw32n = epn::win32;
namespace ein = tca::engine::input;
using ein::key;
using epw32n::scan_code;

using tests::utilities::key_down_lparam;
using tests::utilities::key_repeat_lparam;
using tests::utilities::key_up_lparam;
using tests::utilities::mouse_lparam;
using tests::utilities::wheel_wparam;
using tests::utilities::x_button_wparam;

/**
 * @brief A hidden window feeding a collector, and the frame it writes into: what an application
 * owns, without the application. Messages are either sent (straight into the window procedure,
 * nothing queued) or posted and pumped (when the window procedure must be able to peek at what
 * comes next).
 */
class keyboard_window final {
public:
    keyboard_window()
        : m_window{epw32n::win32_application_window::two_phase_window_creation<
              epw32n::win32_application_window>(TEXT("Keyboard test window"))} {
        REQUIRE(hwnd() != nullptr);
        m_window->set_input_collector(&m_collector);
    }

    [[nodiscard]] HWND hwnd() const noexcept {
        return m_window->get_native_handle().get();
    }

    [[nodiscard]] const ein::keyboard_state& keyboard() const noexcept {
        return m_frame.keyboard();
    }

    [[nodiscard]] std::string_view text() const noexcept {
        return m_frame.text();
    }

    void end_tick() noexcept {
        m_frame.end_tick();
    }

    void send(const UINT message, const WPARAM wparam, const LPARAM lparam) const {
        ::SendMessage(hwnd(), message, wparam, lparam);
    }

    void post(const UINT message, const WPARAM wparam, const LPARAM lparam) const {
        REQUIRE(::PostMessage(hwnd(), message, wparam, lparam) != 0);
    }

    void pump() const {
        tests::utilities::pump_window_messages(hwnd());
    }

    void key_down(const WPARAM virtualKey, const scan_code scanCode) const {
        send(WM_KEYDOWN, virtualKey, key_down_lparam(scanCode));
    }

    void key_up(const WPARAM virtualKey, const scan_code scanCode) const {
        send(WM_KEYUP, virtualKey, key_up_lparam(scanCode));
    }

    void chars(const std::u16string_view utf16) const {
        for (const char16_t unit : utf16) {
            send(WM_CHAR, static_cast<WPARAM>(unit), 1);
        }
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

TEST_CASE("win32_application_window input collector",
          "[tca][engine][platform][win32][win32_application_window][input]") {
    tests::utilities::drain_wm_quit_messages();

    SECTION("Without a collector, keyboard messages are harmless") {
        auto window{epw32n::win32_application_window::two_phase_window_creation<
            epw32n::win32_application_window>(TEXT("Keyboard test window"))};
        const HWND hwnd{window->get_native_handle().get()};
        REQUIRE(hwnd != nullptr);

        REQUIRE_NOTHROW(::SendMessage(hwnd, WM_KEYDOWN, VK_SPACE, key_down_lparam(0x39)));
        REQUIRE_NOTHROW(::SendMessage(hwnd, WM_CHAR, L'a', 1));
        REQUIRE_NOTHROW(::SendMessage(hwnd, WM_KEYUP, VK_SPACE, key_up_lparam(0x39)));
        REQUIRE_NOTHROW(::SendMessage(hwnd, WM_KILLFOCUS, 0, 0));
    }

    SECTION("A detached collector receives nothing more") {
        keyboard_window window{};
        window.key_down(VK_SPACE, 0x39);
        REQUIRE(window.keyboard().is_down(key::space));

        window.detach();
        window.key_up(VK_SPACE, 0x39);
        window.key_down('A', 0x1E);
        window.chars(u"a");

        THEN("The frame keeps what it had before detach") {
            REQUIRE(window.keyboard().is_down(key::space));
            REQUIRE_FALSE(window.keyboard().was_released(key::space));
            REQUIRE_FALSE(window.keyboard().is_down(key::a));
            REQUIRE(window.text().empty());
        }
    }
}

TEST_CASE("win32_application_window keyboard messages",
          "[tca][engine][platform][win32][win32_application_window][input]") {
    tests::utilities::drain_wm_quit_messages();

    keyboard_window window{};

    SECTION("WM_KEYDOWN presses, WM_KEYUP releases") {
        window.key_down(VK_SPACE, 0x39);

        THEN("The key is down and pressed once") {
            REQUIRE(window.keyboard().is_down(key::space));
            REQUIRE(window.keyboard().press_count(key::space) == 1);
            REQUIRE(window.keyboard().release_count(key::space) == 0);
        }

        AND_WHEN("It goes up") {
            window.key_up(VK_SPACE, 0x39);

            THEN("It is up, pressed once and released once") {
                REQUIRE_FALSE(window.keyboard().is_down(key::space));
                REQUIRE(window.keyboard().press_count(key::space) == 1);
                REQUIRE(window.keyboard().release_count(key::space) == 1);
            }
        }
    }

    SECTION("Auto-repeat (previous state bit) is not a press") {
        window.key_down(VK_SPACE, 0x39);
        window.send(WM_KEYDOWN, VK_SPACE, key_repeat_lparam(0x39));
        window.send(WM_KEYDOWN, VK_SPACE, key_repeat_lparam(0x39));

        REQUIRE(window.keyboard().is_down(key::space));
        REQUIRE(window.keyboard().press_count(key::space) == 1);

        AND_WHEN("The tick ends and the key keeps repeating") {
            window.end_tick();
            window.send(WM_KEYDOWN, VK_SPACE, key_repeat_lparam(0x39));

            THEN("It is still down, with no new press") {
                REQUIRE(window.keyboard().is_down(key::space));
                REQUIRE_FALSE(window.keyboard().was_pressed(key::space));
            }
        }
    }

    SECTION("A repeat of a key that isn't held (already down when focus came) is ignored") {
        window.send(WM_KEYDOWN, VK_SPACE, key_repeat_lparam(0x39));

        REQUIRE(keyboard_untouched(window.keyboard()));
    }

    SECTION("WM_SYSKEYDOWN / WM_SYSKEYUP (Alt held, or F10) are keys too") {
        window.send(WM_SYSKEYDOWN, VK_MENU, key_down_lparam(0x38, true));
        window.send(WM_SYSKEYDOWN, VK_F4, key_down_lparam(0x3E, true));
        window.send(WM_SYSKEYDOWN, VK_F10, key_down_lparam(0x44));

        THEN("They are pressed") {
            REQUIRE(window.keyboard().is_down(key::alt_left));
            REQUIRE(window.keyboard().is_down(key::f4));
            REQUIRE(window.keyboard().is_down(key::f10));
        }

        AND_WHEN("They go up") {
            window.send(WM_SYSKEYUP, VK_F4, key_up_lparam(0x3E, true));
            window.send(WM_SYSKEYUP, VK_MENU, key_up_lparam(0x38));
            window.send(WM_SYSKEYUP, VK_F10, key_up_lparam(0x44));

            THEN("They are released") {
                REQUIRE(window.keyboard().was_released(key::alt_left));
                REQUIRE(window.keyboard().was_released(key::f4));
                REQUIRE(window.keyboard().was_released(key::f10));
                REQUIRE(held_keys(window.keyboard()).empty());
            }
        }
    }

    SECTION("The key comes from the scan code, never the virtual key") {
        // AZERTY: the key at US 'Q' sends VK 'A', the key at US 'W' sends VK 'Z'.
        window.key_down('A', 0x10);
        window.key_down('Z', 0x11);

        REQUIRE(window.keyboard().is_down(key::q));
        REQUIRE(window.keyboard().is_down(key::w));
        REQUIRE_FALSE(window.keyboard().is_down(key::a));
        REQUIRE_FALSE(window.keyboard().is_down(key::z));
    }

    SECTION("The extended bit tells twins apart, whatever Num Lock says") {
        const auto [virtualKey, scanCode, expected] = GENERATE(table<WPARAM, scan_code, key>({
            {VK_RETURN,  0x1C,   key::enter         },
            {VK_RETURN,  0xE01C, key::numpad_enter  },
            {VK_CONTROL, 0x1D,   key::control_left  },
            {VK_CONTROL, 0xE01D, key::control_right },
            {VK_MENU,    0x38,   key::alt_left      },
            {VK_MENU,    0xE038, key::alt_right     },
            {VK_SHIFT,   0x2A,   key::shift_left    },
            {VK_SHIFT,   0x36,   key::shift_right   },
            {VK_OEM_2,   0x35,   key::slash         },
            {VK_DIVIDE,  0xE035, key::numpad_divide },
            // Num Lock off: the numpad sends navigation virtual keys, still numpad keys.
            {VK_UP,      0x48,   key::numpad_8      },
            {VK_HOME,    0x47,   key::numpad_7      },
            {VK_INSERT,  0x52,   key::numpad_0      },
            {VK_DELETE,  0x53,   key::numpad_decimal},
            // Num Lock on: same keys, numpad virtual keys.
            {VK_NUMPAD8, 0x48,   key::numpad_8      },
            {VK_DECIMAL, 0x53,   key::numpad_decimal},
            // The control pad and arrows are always the extended ones.
            {VK_UP,      0xE048, key::arrow_up      },
            {VK_HOME,    0xE047, key::home          },
            {VK_INSERT,  0xE052, key::insert        },
            {VK_DELETE,  0xE053, key::delete_key    },
        }));

        CAPTURE(virtualKey, scanCode, ein::key_name(expected));

        window.key_down(virtualKey, scanCode);
        THEN("Only that key is down") {
            REQUIRE(window.keyboard().is_down(expected));
            REQUIRE(held_keys(window.keyboard()) ==
                    std::vector<std::string_view>{ein::key_name(expected)});
        }
    }

    SECTION("An unmapped scan code is dropped") {
        // A non-zero scan code is never second-guessed from the virtual key.
        window.key_down('A', 0x7F);
        window.key_up('A', 0x7F);

        REQUIRE(keyboard_untouched(window.keyboard()));
    }

    SECTION("Scan code 0 with a virtual key that has no scan code is dropped") {
        // 0xFF is reserved: MapVirtualKeyW() gives no scan code for it.
        window.key_down(0xFF, 0x00);
        window.key_up(0xFF, 0x00);

        REQUIRE(keyboard_untouched(window.keyboard()));
    }
}

TEST_CASE("win32_application_window keyboard quirks",
          "[tca][engine][platform][win32][win32_application_window][input]") {
    tests::utilities::drain_wm_quit_messages();

    keyboard_window window{};

    SECTION("Scan code 0 (synthetic input) is recovered from the virtual key") {
        // Only virtual keys whose scan code doesn't depend on the layout.
        const auto [virtualKey, expected] = GENERATE(table<WPARAM, key>({
            {VK_SPACE,    key::space        },
            {VK_RETURN,   key::enter        },
            {VK_ESCAPE,   key::escape       },
            {VK_LSHIFT,   key::shift_left   },
            {VK_RCONTROL, key::control_right},
            {VK_LEFT,     key::arrow_left   },
            {VK_DELETE,   key::delete_key   },
            {VK_F5,       key::f5           },
        }));

        CAPTURE(virtualKey, ein::key_name(expected));

        window.key_down(virtualKey, 0x00);
        REQUIRE(window.keyboard().is_down(expected));
        REQUIRE(window.keyboard().press_count(expected) == 1);

        window.key_up(virtualKey, 0x00);
        REQUIRE_FALSE(window.keyboard().is_down(expected));
        REQUIRE(window.keyboard().release_count(expected) == 1);
    }

    SECTION("Pause and Num Lock share scan code 0x45") {
        SECTION("Pause arrives as VK_PAUSE") {
            window.key_down(VK_PAUSE, 0x45);
            REQUIRE(window.keyboard().is_down(key::pause));
            REQUIRE_FALSE(window.keyboard().is_down(key::num_lock));
        }

        SECTION("Num Lock with the extended bit") {
            window.key_down(VK_NUMLOCK, 0xE045);
            REQUIRE(window.keyboard().is_down(key::num_lock));
            REQUIRE_FALSE(window.keyboard().is_down(key::pause));
        }

        SECTION("Num Lock without the extended bit is still Num Lock") {
            window.key_down(VK_NUMLOCK, 0x45);
            REQUIRE(window.keyboard().is_down(key::num_lock));
            REQUIRE_FALSE(window.keyboard().is_down(key::pause));
        }

        SECTION("Scan code 0: both recovered from the virtual key") {
            window.key_down(VK_PAUSE, 0x00);
            window.key_down(VK_NUMLOCK, 0x00);
            REQUIRE(window.keyboard().is_down(key::pause));
            REQUIRE(window.keyboard().is_down(key::num_lock));
        }

        SECTION("Ctrl+Pause (VK_CANCEL, Break's scan code) is Pause") {
            window.key_down(VK_CANCEL, 0xE046);
            REQUIRE(window.keyboard().is_down(key::pause));
            REQUIRE_FALSE(window.keyboard().is_down(key::scroll_lock));
        }
    }

    SECTION("AltGr: the fake left Ctrl Windows sends before right Alt is dropped") {
        // Posted, not sent: the window procedure has to peek at the right Alt queued behind the
        // fake left Ctrl. Posted back to back, both carry the same message time.
        window.post(WM_KEYDOWN, VK_CONTROL, key_down_lparam(0x1D));
        window.post(WM_KEYDOWN, VK_MENU, key_down_lparam(0xE038));
        window.pump();

        THEN("Right Alt is down, left Ctrl was never pressed") {
            REQUIRE(window.keyboard().is_down(key::alt_right));
            REQUIRE(window.keyboard().press_count(key::alt_right) == 1);
            REQUIRE_FALSE(window.keyboard().is_down(key::control_left));
            REQUIRE(window.keyboard().press_count(key::control_left) == 0);
        }

        AND_WHEN("AltGr repeats while held") {
            window.end_tick();
            window.post(WM_KEYDOWN, VK_CONTROL, key_repeat_lparam(0x1D));
            window.post(WM_KEYDOWN, VK_MENU, key_repeat_lparam(0xE038));
            window.pump();

            THEN("Still only right Alt, with no new press") {
                REQUIRE(held_keys(window.keyboard()) ==
                        std::vector<std::string_view>{ein::key_name(key::alt_right)});
                REQUIRE_FALSE(window.keyboard().was_pressed(key::alt_right));
            }
        }

        AND_WHEN("AltGr goes up (the fake left Ctrl goes up too)") {
            window.end_tick();
            window.post(WM_KEYUP, VK_CONTROL, key_up_lparam(0x1D));
            window.post(WM_KEYUP, VK_MENU, key_up_lparam(0xE038));
            window.pump();

            THEN("Right Alt is released, left Ctrl reports nothing") {
                REQUIRE(window.keyboard().was_released(key::alt_right));
                REQUIRE(window.keyboard().release_count(key::control_left) == 0);
                REQUIRE(held_keys(window.keyboard()).empty());
            }
        }
    }

    SECTION("A real left Ctrl is kept") {
        SECTION("Alone") {
            window.post(WM_KEYDOWN, VK_CONTROL, key_down_lparam(0x1D));
            window.pump();

            REQUIRE(window.keyboard().is_down(key::control_left));
        }

        SECTION("Followed by another key (Ctrl+C)") {
            window.post(WM_KEYDOWN, VK_CONTROL, key_down_lparam(0x1D));
            window.post(WM_KEYDOWN, 'C', key_down_lparam(0x2E));
            window.pump();

            REQUIRE(window.keyboard().is_down(key::control_left));
            REQUIRE(window.keyboard().is_down(key::c));
        }

        SECTION("Sent, with nothing queued behind it") {
            window.key_down(VK_CONTROL, 0x1D);

            REQUIRE(window.keyboard().is_down(key::control_left));
        }

        SECTION("Then right Alt pressed while left Ctrl is held (Ctrl+Alt)") {
            window.key_down(VK_CONTROL, 0x1D);
            window.end_tick();
            window.post(WM_KEYDOWN, VK_CONTROL, key_repeat_lparam(0x1D));
            window.post(WM_KEYDOWN, VK_MENU, key_down_lparam(0xE038));
            window.pump();

            THEN("Dropping the repeat in front of right Alt doesn't release left Ctrl") {
                REQUIRE(window.keyboard().is_down(key::control_left));
                REQUIRE(window.keyboard().is_down(key::alt_right));
                REQUIRE(window.keyboard().release_count(key::control_left) == 0);
            }
        }
    }

    SECTION("Both Shifts held, then one Shift key-up") {
        tests::utilities::scoped_keyboard_state keyState{};
        keyState.set_down(VK_SHIFT, false);
        keyState.set_down(VK_LSHIFT, false);
        keyState.set_down(VK_RSHIFT, false);

        window.key_down(VK_SHIFT, 0x2A);
        window.key_down(VK_SHIFT, 0x36);
        REQUIRE(window.keyboard().is_down(key::shift_left));
        REQUIRE(window.keyboard().is_down(key::shift_right));

        SECTION("Both are up for Windows: the one with no key-up of its own is released too") {
            // The user let go of left Shift (no WM_KEYUP for it), then of right Shift.
            window.key_up(VK_SHIFT, 0x36);

            REQUIRE_FALSE(window.keyboard().is_down(key::shift_left));
            REQUIRE_FALSE(window.keyboard().is_down(key::shift_right));
            REQUIRE(window.keyboard().release_count(key::shift_left) == 1);
            REQUIRE(window.keyboard().release_count(key::shift_right) == 1);
        }

        SECTION("The other one is still down for Windows: it stays held") {
            keyState.set_down(VK_SHIFT, true);
            keyState.set_down(VK_LSHIFT, true);
            window.key_up(VK_SHIFT, 0x36);

            REQUIRE(window.keyboard().is_down(key::shift_left));
            REQUIRE(window.keyboard().release_count(key::shift_left) == 0);
            REQUIRE_FALSE(window.keyboard().is_down(key::shift_right));
            REQUIRE(window.keyboard().release_count(key::shift_right) == 1);
        }

        SECTION("Left Shift's own key-up releases it once, not twice") {
            keyState.set_down(VK_SHIFT, true);
            keyState.set_down(VK_RSHIFT, true);
            window.key_up(VK_SHIFT, 0x2A);

            REQUIRE_FALSE(window.keyboard().is_down(key::shift_left));
            REQUIRE(window.keyboard().release_count(key::shift_left) == 1);
            REQUIRE(window.keyboard().is_down(key::shift_right));
        }
    }

    SECTION("A Shift key-up with only that Shift held releases nothing else") {
        tests::utilities::scoped_keyboard_state keyState{};
        keyState.set_down(VK_SHIFT, false);
        keyState.set_down(VK_LSHIFT, false);
        keyState.set_down(VK_RSHIFT, false);

        window.key_down(VK_SHIFT, 0x36);
        window.key_up(VK_SHIFT, 0x36);

        REQUIRE(window.keyboard().release_count(key::shift_right) == 1);
        REQUIRE(window.keyboard().press_count(key::shift_left) == 0);
        REQUIRE(window.keyboard().release_count(key::shift_left) == 0);
    }

    SECTION("Print Screen sends only a key-up: the press is synthesized") {
        window.key_up(VK_SNAPSHOT, 0xE037);

        REQUIRE(window.keyboard().press_count(key::print_screen) == 1);
        REQUIRE(window.keyboard().release_count(key::print_screen) == 1);
        REQUIRE_FALSE(window.keyboard().is_down(key::print_screen));

        AND_WHEN("It is tapped again in the same tick") {
            window.key_up(VK_SNAPSHOT, 0xE037);

            THEN("Both taps count") {
                REQUIRE(window.keyboard().press_count(key::print_screen) == 2);
                REQUIRE(window.keyboard().release_count(key::print_screen) == 2);
            }
        }
    }

    SECTION("Alt+Print Screen (SysRq's scan code, as WM_SYSKEYUP) is Print Screen") {
        window.send(WM_SYSKEYUP, VK_SNAPSHOT, key_up_lparam(0x54, true));

        REQUIRE(window.keyboard().press_count(key::print_screen) == 1);
        REQUIRE(window.keyboard().release_count(key::print_screen) == 1);
    }

    SECTION("A Print Screen that does send its key-down isn't pressed twice") {
        window.key_down(VK_SNAPSHOT, 0xE037);
        window.key_up(VK_SNAPSHOT, 0xE037);

        REQUIRE(window.keyboard().press_count(key::print_screen) == 1);
        REQUIRE(window.keyboard().release_count(key::print_screen) == 1);
    }
}

TEST_CASE("win32_application_window focus loss",
          "[tca][engine][platform][win32][win32_application_window][input]") {
    tests::utilities::drain_wm_quit_messages();

    keyboard_window window{};
    window.key_down(VK_SPACE, 0x39);
    window.key_down('W', 0x11);
    window.chars(u"x");
    REQUIRE(window.keyboard().is_down(key::space));
    REQUIRE(window.keyboard().is_down(key::w));

    SECTION("WM_KILLFOCUS releases every held key and keeps the text") {
        window.send(WM_KILLFOCUS, 0, 0);

        REQUIRE(held_keys(window.keyboard()).empty());
        REQUIRE(window.keyboard().was_released(key::space));
        REQUIRE(window.keyboard().was_released(key::w));
        REQUIRE(window.text() == "x");
    }

    SECTION("WM_ACTIVATEAPP (deactivated) releases every held key") {
        window.send(WM_ACTIVATEAPP, FALSE, 0);

        REQUIRE(held_keys(window.keyboard()).empty());
        REQUIRE(window.keyboard().was_released(key::space));
        REQUIRE(window.keyboard().was_released(key::w));
    }

    SECTION("WM_ACTIVATEAPP (activated) releases nothing") {
        window.send(WM_ACTIVATEAPP, TRUE, 0);

        REQUIRE(window.keyboard().is_down(key::space));
        REQUIRE(window.keyboard().is_down(key::w));
        REQUIRE_FALSE(window.keyboard().was_released(key::space));
    }

    SECTION("The key-up that never came (Win key, Alt+Tab) arriving late is ignored") {
        window.send(WM_KILLFOCUS, 0, 0);
        window.end_tick();
        window.key_up(VK_SPACE, 0x39);

        REQUIRE_FALSE(window.keyboard().was_released(key::space));
    }
}

TEST_CASE("win32_application_window typed text",
          "[tca][engine][platform][win32][win32_application_window][input]") {
    tests::utilities::drain_wm_quit_messages();

    keyboard_window window{};

    SECTION("WM_CHAR code units become UTF-8 text") {
        SECTION("ASCII") {
            window.chars(u"Hi!");
            REQUIRE(window.text() == "Hi!");
        }

        SECTION("Two-byte UTF-8 (é)") {
            window.chars(u"é");
            REQUIRE(window.text() == "\xC3\xA9");
        }

        SECTION("Three-byte UTF-8 (中, an IME result)") {
            window.chars(u"中");
            REQUIRE(window.text() == "\xE4\xB8\xAD");
        }

        SECTION("A surrogate pair becomes one four-byte code point (U+1F600)") {
            window.chars(u"\U0001F600");
            REQUIRE(window.text() == "\xF0\x9F\x98\x80");
        }

        SECTION("Text keeps arriving in order across pairs") {
            window.chars(u"a\U0001F600b");
            REQUIRE(window.text() ==
                    "a\xF0\x9F\x98\x80"
                    "b");
        }
    }

    SECTION("Broken UTF-16 is dropped, never turned into text") {
        SECTION("A lone high surrogate followed by a character") {
            window.send(WM_CHAR, 0xD83D, 1);
            window.chars(u"a");
            REQUIRE(window.text() == "a");
        }

        SECTION("A lone low surrogate") {
            window.send(WM_CHAR, 0xDE00, 1);
            REQUIRE(window.text().empty());
        }

        SECTION("Two high surrogates, then a low one: only the second pair") {
            window.send(WM_CHAR, 0xD83D, 1);
            window.send(WM_CHAR, 0xD83D, 1);
            window.send(WM_CHAR, 0xDE00, 1);
            REQUIRE(window.text() == "\xF0\x9F\x98\x80");
        }

        SECTION("A high surrogate left pending at the end of a tick") {
            window.send(WM_CHAR, 0xD83D, 1);
            window.end_tick();
            window.send(WM_CHAR, 0xDE00, 1);

            THEN("The pair still completes in the next tick") {
                REQUIRE(window.text() == "\xF0\x9F\x98\x80");
            }
        }

        SECTION("A high surrogate pending when focus is lost is forgotten") {
            window.send(WM_CHAR, 0xD83D, 1);
            window.send(WM_KILLFOCUS, 0, 0);
            window.send(WM_CHAR, 0xDE00, 1);
            REQUIRE(window.text().empty());
        }
    }

    SECTION("Control characters from WM_CHAR are not text") {
        window.send(WM_CHAR, 0x08, 1); // Backspace
        window.send(WM_CHAR, 0x0D, 1); // Enter
        window.send(WM_CHAR, 0x1B, 1); // Escape
        window.send(WM_CHAR, 0x7F, 1); // Ctrl+Backspace

        REQUIRE(window.text().empty());
    }

    SECTION("WM_DEADCHAR is ignored: the composed character arrives as WM_CHAR") {
        window.send(WM_DEADCHAR, L'^', 1);
        REQUIRE(window.text().empty());

        window.chars(u"ê");
        REQUIRE(window.text() == "\xC3\xAA");
    }

    SECTION("WM_SYSCHAR (Alt+key menu mnemonics) is not text") {
        window.send(WM_SYSCHAR, L'f', 1);
        REQUIRE(window.text().empty());
    }

    SECTION("Text and keys are separate channels") {
        window.key_down('A', 0x1E);
        REQUIRE(window.text().empty());

        window.chars(u"a");
        REQUIRE(window.text() == "a");
        REQUIRE(window.keyboard().press_count(key::a) == 1);
    }
}

namespace {

/**
 * @brief Records what the base routed window procedure returns for each message, so tests can
 * check the input feed never swallows a message DefWindowProc needs (Alt+F4 and Alt+Space go
 * through WM_SYSKEYDOWN, IMEs through WM_IME_*).
 */
class recording_window_tss final : public epw32n::win32_application_window {
public:
    using win32_application_window::win32_application_window;

    [[nodiscard]] std::optional<route_result_t> last_result_for(const UINT message) const {
        for (auto it{m_results.rbegin()}; it != m_results.rend(); ++it) {
            if (it->first == message) {
                return it->second;
            }
        }

        return std::nullopt;
    }

protected:
    route_result_t routed_window_procedure(
        const HWND hwnd, const epw32n::windows_messages message,
        const epw32n::details::wparam_t wparam,
        const epw32n::details::lparam_t lparam) noexcept override {
        const auto result{
            win32_application_window::routed_window_procedure(hwnd, message, wparam, lparam)};
        m_results.emplace_back(epw32n::to_underlying(message), result);
        return result;
    }

private:
    std::vector<std::pair<UINT, route_result_t>> m_results{};
};

} // namespace

TEST_CASE("win32_application_window input messages still reach DefWindowProc",
          "[tca][engine][platform][win32][win32_application_window][input]") {
    tests::utilities::drain_wm_quit_messages();

    ein::input_frame frame{};
    ein::input_collector collector{frame};
    auto window{epw32n::win32_application_window::two_phase_window_creation<recording_window_tss>(
        TEXT("Keyboard test window"))};
    const HWND hwnd{window->get_native_handle().get()};
    REQUIRE(hwnd != nullptr);
    window->set_input_collector(&collector);

    const auto [message, wparam, lparam] = GENERATE(table<UINT, WPARAM, LPARAM>({
        {WM_KEYDOWN, VK_SPACE, key_down_lparam(0x39)},
        {WM_KEYUP, VK_SPACE, key_up_lparam(0x39)},
        {WM_SYSKEYDOWN, VK_MENU, key_down_lparam(0x38, true)},
        {WM_SYSKEYUP, VK_MENU, key_up_lparam(0x38)},
        {WM_CHAR, L'a', 1},
        {WM_DEADCHAR, L'^', 1},
        {WM_SYSCHAR, L'f', 1},
        {WM_KILLFOCUS, 0, 0},
        {WM_ACTIVATEAPP, FALSE, 0},
        {WM_IME_SETCONTEXT, FALSE, 0},
        {WM_IME_NOTIFY, 0, 0},
        {WM_IME_STARTCOMPOSITION, 0, 0},
        {WM_IME_COMPOSITION, 0, 0},
        {WM_IME_ENDCOMPOSITION, 0, 0},
        {WM_IME_CHAR, L'a', 1},
        // The mouse: DefWindowProc turns X buttons into WM_APPCOMMAND (browser back/forward) and
        // forwards unhandled wheel turns to the parent.
        {WM_MOUSEMOVE, 0, mouse_lparam(1, 1)},
        {WM_LBUTTONDOWN, MK_LBUTTON, mouse_lparam(1, 1)},
        {WM_LBUTTONUP, 0, mouse_lparam(1, 1)},
        {WM_LBUTTONDBLCLK, MK_LBUTTON, mouse_lparam(1, 1)},
        {WM_XBUTTONDOWN, x_button_wparam(XBUTTON1, MK_XBUTTON1), mouse_lparam(1, 1)},
        {WM_XBUTTONUP, x_button_wparam(XBUTTON1), mouse_lparam(1, 1)},
        {WM_MOUSEWHEEL, wheel_wparam(WHEEL_DELTA), 0},
        {WM_MOUSEHWHEEL, wheel_wparam(WHEEL_DELTA), 0},
        {WM_MOUSELEAVE, 0, 0},
        {WM_CAPTURECHANGED, 0, 0},
    }));

    CAPTURE(message);

    ::SendMessage(hwnd, message, wparam, lparam);

    const auto result{window->last_result_for(message)};
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->has_value());

    // Destroy the window before the collector it points at.
    window.reset();
}
