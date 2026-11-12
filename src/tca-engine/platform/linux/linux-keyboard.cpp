// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/linux/linux-keyboard.hpp"

// Third-party
#include <GLFW/glfw3.h>
#include <linux/input-event-codes.h>

// C++ STL
#include <limits>
#include <optional>

namespace tca::engine::platform::posix_linux {

namespace {
// X11 keycodes are evdev codes shifted by 8, a leftover of the X protocol reserving 0-7.
constexpr int X11_KEYCODE_OFFSET{8};
} // namespace

std::optional<input::key> key_from_glfw_key(const int glfwKey) noexcept {
    using input::key;

    // GLFW derives its tokens from the physical key and names them after the US layout, so each
    // token is a position, whatever layout is active.
    switch (glfwKey) {
        // Letters
        case GLFW_KEY_A:
            return key::a;
        case GLFW_KEY_B:
            return key::b;
        case GLFW_KEY_C:
            return key::c;
        case GLFW_KEY_D:
            return key::d;
        case GLFW_KEY_E:
            return key::e;
        case GLFW_KEY_F:
            return key::f;
        case GLFW_KEY_G:
            return key::g;
        case GLFW_KEY_H:
            return key::h;
        case GLFW_KEY_I:
            return key::i;
        case GLFW_KEY_J:
            return key::j;
        case GLFW_KEY_K:
            return key::k;
        case GLFW_KEY_L:
            return key::l;
        case GLFW_KEY_M:
            return key::m;
        case GLFW_KEY_N:
            return key::n;
        case GLFW_KEY_O:
            return key::o;
        case GLFW_KEY_P:
            return key::p;
        case GLFW_KEY_Q:
            return key::q;
        case GLFW_KEY_R:
            return key::r;
        case GLFW_KEY_S:
            return key::s;
        case GLFW_KEY_T:
            return key::t;
        case GLFW_KEY_U:
            return key::u;
        case GLFW_KEY_V:
            return key::v;
        case GLFW_KEY_W:
            return key::w;
        case GLFW_KEY_X:
            return key::x;
        case GLFW_KEY_Y:
            return key::y;
        case GLFW_KEY_Z:
            return key::z;

        // Digit row
        case GLFW_KEY_0:
            return key::digit_0;
        case GLFW_KEY_1:
            return key::digit_1;
        case GLFW_KEY_2:
            return key::digit_2;
        case GLFW_KEY_3:
            return key::digit_3;
        case GLFW_KEY_4:
            return key::digit_4;
        case GLFW_KEY_5:
            return key::digit_5;
        case GLFW_KEY_6:
            return key::digit_6;
        case GLFW_KEY_7:
            return key::digit_7;
        case GLFW_KEY_8:
            return key::digit_8;
        case GLFW_KEY_9:
            return key::digit_9;

        // Punctuation
        case GLFW_KEY_GRAVE_ACCENT:
            return key::backquote;
        case GLFW_KEY_MINUS:
            return key::minus;
        case GLFW_KEY_EQUAL:
            return key::equal;
        case GLFW_KEY_LEFT_BRACKET:
            return key::bracket_left;
        case GLFW_KEY_RIGHT_BRACKET:
            return key::bracket_right;
        case GLFW_KEY_BACKSLASH:
            return key::backslash;
        case GLFW_KEY_SEMICOLON:
            return key::semicolon;
        case GLFW_KEY_APOSTROPHE:
            return key::quote;
        case GLFW_KEY_COMMA:
            return key::comma;
        case GLFW_KEY_PERIOD:
            return key::period;
        case GLFW_KEY_SLASH:
            return key::slash;

        // X11 reports the ISO key as WORLD_1, Wayland as WORLD_2.
        case GLFW_KEY_WORLD_1:
        case GLFW_KEY_WORLD_2:
            return key::intl_backslash;

        // Editing and modifiers
        case GLFW_KEY_BACKSPACE:
            return key::backspace;
        case GLFW_KEY_TAB:
            return key::tab;
        case GLFW_KEY_ENTER:
            return key::enter;
        case GLFW_KEY_SPACE:
            return key::space;
        case GLFW_KEY_CAPS_LOCK:
            return key::caps_lock;
        case GLFW_KEY_LEFT_SHIFT:
            return key::shift_left;
        case GLFW_KEY_RIGHT_SHIFT:
            return key::shift_right;
        case GLFW_KEY_LEFT_CONTROL:
            return key::control_left;
        case GLFW_KEY_RIGHT_CONTROL:
            return key::control_right;
        case GLFW_KEY_LEFT_ALT:
            return key::alt_left;
        case GLFW_KEY_RIGHT_ALT:
            return key::alt_right;
        case GLFW_KEY_LEFT_SUPER:
            return key::meta_left;
        case GLFW_KEY_RIGHT_SUPER:
            return key::meta_right;
        case GLFW_KEY_MENU:
            return key::context_menu;

        // Control pad and arrows
        case GLFW_KEY_INSERT:
            return key::insert;
        case GLFW_KEY_DELETE:
            return key::delete_key;
        case GLFW_KEY_HOME:
            return key::home;
        case GLFW_KEY_END:
            return key::end;
        case GLFW_KEY_PAGE_UP:
            return key::page_up;
        case GLFW_KEY_PAGE_DOWN:
            return key::page_down;
        case GLFW_KEY_UP:
            return key::arrow_up;
        case GLFW_KEY_DOWN:
            return key::arrow_down;
        case GLFW_KEY_LEFT:
            return key::arrow_left;
        case GLFW_KEY_RIGHT:
            return key::arrow_right;

        // Numpad
        case GLFW_KEY_NUM_LOCK:
            return key::num_lock;
        case GLFW_KEY_KP_0:
            return key::numpad_0;
        case GLFW_KEY_KP_1:
            return key::numpad_1;
        case GLFW_KEY_KP_2:
            return key::numpad_2;
        case GLFW_KEY_KP_3:
            return key::numpad_3;
        case GLFW_KEY_KP_4:
            return key::numpad_4;
        case GLFW_KEY_KP_5:
            return key::numpad_5;
        case GLFW_KEY_KP_6:
            return key::numpad_6;
        case GLFW_KEY_KP_7:
            return key::numpad_7;
        case GLFW_KEY_KP_8:
            return key::numpad_8;
        case GLFW_KEY_KP_9:
            return key::numpad_9;
        case GLFW_KEY_KP_ADD:
            return key::numpad_add;
        case GLFW_KEY_KP_SUBTRACT:
            return key::numpad_subtract;
        case GLFW_KEY_KP_MULTIPLY:
            return key::numpad_multiply;
        case GLFW_KEY_KP_DIVIDE:
            return key::numpad_divide;
        case GLFW_KEY_KP_DECIMAL:
            return key::numpad_decimal;
        case GLFW_KEY_KP_ENTER:
            return key::numpad_enter;
        case GLFW_KEY_KP_EQUAL:
            return key::numpad_equal;

        // Function row
        case GLFW_KEY_ESCAPE:
            return key::escape;
        case GLFW_KEY_F1:
            return key::f1;
        case GLFW_KEY_F2:
            return key::f2;
        case GLFW_KEY_F3:
            return key::f3;
        case GLFW_KEY_F4:
            return key::f4;
        case GLFW_KEY_F5:
            return key::f5;
        case GLFW_KEY_F6:
            return key::f6;
        case GLFW_KEY_F7:
            return key::f7;
        case GLFW_KEY_F8:
            return key::f8;
        case GLFW_KEY_F9:
            return key::f9;
        case GLFW_KEY_F10:
            return key::f10;
        case GLFW_KEY_F11:
            return key::f11;
        case GLFW_KEY_F12:
            return key::f12;
        case GLFW_KEY_F13:
            return key::f13;
        case GLFW_KEY_F14:
            return key::f14;
        case GLFW_KEY_F15:
            return key::f15;
        case GLFW_KEY_F16:
            return key::f16;
        case GLFW_KEY_F17:
            return key::f17;
        case GLFW_KEY_F18:
            return key::f18;
        case GLFW_KEY_F19:
            return key::f19;
        case GLFW_KEY_F20:
            return key::f20;
        case GLFW_KEY_F21:
            return key::f21;
        case GLFW_KEY_F22:
            return key::f22;
        case GLFW_KEY_F23:
            return key::f23;
        case GLFW_KEY_F24:
            return key::f24;
        case GLFW_KEY_PRINT_SCREEN:
            return key::print_screen;
        case GLFW_KEY_SCROLL_LOCK:
            return key::scroll_lock;
        case GLFW_KEY_PAUSE:
            return key::pause;

        default:
            return std::nullopt;
    }
}

std::optional<input::key> key_from_evdev_code(const evdev_code code) noexcept {
    using input::key;

    switch (code) {
        // Letters, by row
        case KEY_Q:
            return key::q;
        case KEY_W:
            return key::w;
        case KEY_E:
            return key::e;
        case KEY_R:
            return key::r;
        case KEY_T:
            return key::t;
        case KEY_Y:
            return key::y;
        case KEY_U:
            return key::u;
        case KEY_I:
            return key::i;
        case KEY_O:
            return key::o;
        case KEY_P:
            return key::p;
        case KEY_A:
            return key::a;
        case KEY_S:
            return key::s;
        case KEY_D:
            return key::d;
        case KEY_F:
            return key::f;
        case KEY_G:
            return key::g;
        case KEY_H:
            return key::h;
        case KEY_J:
            return key::j;
        case KEY_K:
            return key::k;
        case KEY_L:
            return key::l;
        case KEY_Z:
            return key::z;
        case KEY_X:
            return key::x;
        case KEY_C:
            return key::c;
        case KEY_V:
            return key::v;
        case KEY_B:
            return key::b;
        case KEY_N:
            return key::n;
        case KEY_M:
            return key::m;

        // Digit row
        case KEY_1:
            return key::digit_1;
        case KEY_2:
            return key::digit_2;
        case KEY_3:
            return key::digit_3;
        case KEY_4:
            return key::digit_4;
        case KEY_5:
            return key::digit_5;
        case KEY_6:
            return key::digit_6;
        case KEY_7:
            return key::digit_7;
        case KEY_8:
            return key::digit_8;
        case KEY_9:
            return key::digit_9;
        case KEY_0:
            return key::digit_0;

        // Punctuation
        case KEY_GRAVE:
            return key::backquote;
        case KEY_MINUS:
            return key::minus;
        case KEY_EQUAL:
            return key::equal;
        case KEY_LEFTBRACE:
            return key::bracket_left;
        case KEY_RIGHTBRACE:
            return key::bracket_right;
        case KEY_BACKSLASH:
            return key::backslash;
        case KEY_SEMICOLON:
            return key::semicolon;
        case KEY_APOSTROPHE:
            return key::quote;
        case KEY_COMMA:
            return key::comma;
        case KEY_DOT:
            return key::period;
        case KEY_SLASH:
            return key::slash;

        // ISO, JIS, Korean and ABNT extras: the keys GLFW has no token for
        case KEY_102ND:
            return key::intl_backslash;
        case KEY_RO:
            return key::intl_ro;
        case KEY_YEN:
            return key::intl_yen;
        case KEY_HENKAN:
            return key::convert;
        case KEY_MUHENKAN:
            return key::non_convert;
        case KEY_KATAKANAHIRAGANA:
            return key::kana_mode;
        case KEY_HANGEUL:
            return key::lang_1;
        case KEY_HANJA:
            return key::lang_2;

        // Editing and modifiers
        case KEY_BACKSPACE:
            return key::backspace;
        case KEY_TAB:
            return key::tab;
        case KEY_ENTER:
            return key::enter;
        case KEY_SPACE:
            return key::space;
        case KEY_CAPSLOCK:
            return key::caps_lock;
        case KEY_LEFTSHIFT:
            return key::shift_left;
        case KEY_RIGHTSHIFT:
            return key::shift_right;
        case KEY_LEFTCTRL:
            return key::control_left;
        case KEY_RIGHTCTRL:
            return key::control_right;
        case KEY_LEFTALT:
            return key::alt_left;
        case KEY_RIGHTALT:
            return key::alt_right;
        case KEY_LEFTMETA:
            return key::meta_left;
        case KEY_RIGHTMETA:
            return key::meta_right;
        // evdev calls the PC Menu key "Compose"; KEY_MENU is a different, media-style key.
        case KEY_COMPOSE:
            return key::context_menu;

        // Control pad and arrows
        case KEY_INSERT:
            return key::insert;
        case KEY_DELETE:
            return key::delete_key;
        case KEY_HOME:
            return key::home;
        case KEY_END:
            return key::end;
        case KEY_PAGEUP:
            return key::page_up;
        case KEY_PAGEDOWN:
            return key::page_down;
        case KEY_UP:
            return key::arrow_up;
        case KEY_DOWN:
            return key::arrow_down;
        case KEY_LEFT:
            return key::arrow_left;
        case KEY_RIGHT:
            return key::arrow_right;

        // Numpad
        case KEY_NUMLOCK:
            return key::num_lock;
        case KEY_KP0:
            return key::numpad_0;
        case KEY_KP1:
            return key::numpad_1;
        case KEY_KP2:
            return key::numpad_2;
        case KEY_KP3:
            return key::numpad_3;
        case KEY_KP4:
            return key::numpad_4;
        case KEY_KP5:
            return key::numpad_5;
        case KEY_KP6:
            return key::numpad_6;
        case KEY_KP7:
            return key::numpad_7;
        case KEY_KP8:
            return key::numpad_8;
        case KEY_KP9:
            return key::numpad_9;
        case KEY_KPPLUS:
            return key::numpad_add;
        case KEY_KPMINUS:
            return key::numpad_subtract;
        case KEY_KPASTERISK:
            return key::numpad_multiply;
        case KEY_KPSLASH:
            return key::numpad_divide;
        case KEY_KPDOT:
            return key::numpad_decimal;
        case KEY_KPENTER:
            return key::numpad_enter;
        case KEY_KPCOMMA:
            return key::numpad_comma;
        case KEY_KPEQUAL:
            return key::numpad_equal;

        // Function row
        case KEY_ESC:
            return key::escape;
        case KEY_F1:
            return key::f1;
        case KEY_F2:
            return key::f2;
        case KEY_F3:
            return key::f3;
        case KEY_F4:
            return key::f4;
        case KEY_F5:
            return key::f5;
        case KEY_F6:
            return key::f6;
        case KEY_F7:
            return key::f7;
        case KEY_F8:
            return key::f8;
        case KEY_F9:
            return key::f9;
        case KEY_F10:
            return key::f10;
        case KEY_F11:
            return key::f11;
        case KEY_F12:
            return key::f12;
        case KEY_F13:
            return key::f13;
        case KEY_F14:
            return key::f14;
        case KEY_F15:
            return key::f15;
        case KEY_F16:
            return key::f16;
        case KEY_F17:
            return key::f17;
        case KEY_F18:
            return key::f18;
        case KEY_F19:
            return key::f19;
        case KEY_F20:
            return key::f20;
        case KEY_F21:
            return key::f21;
        case KEY_F22:
            return key::f22;
        case KEY_F23:
            return key::f23;
        case KEY_F24:
            return key::f24;
        case KEY_SYSRQ:
            return key::print_screen;
        case KEY_SCROLLLOCK:
            return key::scroll_lock;
        case KEY_PAUSE:
            return key::pause;

        default:
            return std::nullopt;
    }
}

std::optional<evdev_code> evdev_code_from_glfw_scancode(const int scancode,
                                                        const int glfwPlatform) noexcept {
    int code{};
    switch (glfwPlatform) {
        case GLFW_PLATFORM_X11:
            code = scancode - X11_KEYCODE_OFFSET;
            break;
        case GLFW_PLATFORM_WAYLAND:
            code = scancode;
            break;
        default:
            // Every other platform, the null one included, has scancodes of its own.
            return std::nullopt;
    }

    if (code < 0 || code > std::numeric_limits<evdev_code>::max()) {
        return std::nullopt;
    }

    return static_cast<evdev_code>(code);
}

std::optional<input::key> key_from_glfw_key_event(const int glfwKey, const int scancode,
                                                  const int glfwPlatform) noexcept {
    if (const auto k{key_from_glfw_key(glfwKey)}) {
        return k;
    }

    // GLFW_KEY_UNKNOWN (or a token with no position): only the scancode can say which key it is.
    if (const auto code{evdev_code_from_glfw_scancode(scancode, glfwPlatform)}) {
        return key_from_evdev_code(*code);
    }

    return std::nullopt;
}

} // namespace tca::engine::platform::posix_linux
