// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/win32/win32-keyboard.hpp"

// C++ STL
#include <optional>

// Third-party
#include <gsl/util>

// Platform
#include "platform/win32/win32-application-window.hpp"

namespace tca::engine::platform::win32 {

namespace {
constexpr auto EXTENDED_KEY_BIT{0x01000000};
constexpr auto SCAN_CODE_EXTENDED_KEY_PREFIX{0xE000};
} // namespace

scan_code scan_code_from_lparam(const details::lparam_t param) noexcept {
    auto shifted{(param >> 16) & 0xff};
    // If the 24th bit is set, we need to add the 0xE0 to the result.
    // 00000001 00000000 00000000 00000000
    // 01 00 00 00 -> 00 E0 00 00
    // With E = 1110
    shifted |= ((param & EXTENDED_KEY_BIT) ? SCAN_CODE_EXTENDED_KEY_PREFIX : 0);
    return gsl::narrow_cast<scan_code>(shifted);
}

std::optional<input::key> key_from_scan_code(const scan_code scanCode) noexcept {
    using input::key;

    // Set 1 scan codes, as Windows reports them, with the extended (0xE0) prefix in the high
    // byte. The extended bit is what tells twins apart (numpad Enter, right Ctrl/Alt, the control
    // pad vs the numpad), so both halves of the code are always matched together.
    switch (scanCode) {
        // Letters
        case 0x0010:
            return key::q;
        case 0x0011:
            return key::w;
        case 0x0012:
            return key::e;
        case 0x0013:
            return key::r;
        case 0x0014:
            return key::t;
        case 0x0015:
            return key::y;
        case 0x0016:
            return key::u;
        case 0x0017:
            return key::i;
        case 0x0018:
            return key::o;
        case 0x0019:
            return key::p;
        case 0x001E:
            return key::a;
        case 0x001F:
            return key::s;
        case 0x0020:
            return key::d;
        case 0x0021:
            return key::f;
        case 0x0022:
            return key::g;
        case 0x0023:
            return key::h;
        case 0x0024:
            return key::j;
        case 0x0025:
            return key::k;
        case 0x0026:
            return key::l;
        case 0x002C:
            return key::z;
        case 0x002D:
            return key::x;
        case 0x002E:
            return key::c;
        case 0x002F:
            return key::v;
        case 0x0030:
            return key::b;
        case 0x0031:
            return key::n;
        case 0x0032:
            return key::m;

        // Digit row
        case 0x0002:
            return key::digit_1;
        case 0x0003:
            return key::digit_2;
        case 0x0004:
            return key::digit_3;
        case 0x0005:
            return key::digit_4;
        case 0x0006:
            return key::digit_5;
        case 0x0007:
            return key::digit_6;
        case 0x0008:
            return key::digit_7;
        case 0x0009:
            return key::digit_8;
        case 0x000A:
            return key::digit_9;
        case 0x000B:
            return key::digit_0;

        // Punctuation
        case 0x0029:
            return key::backquote;
        case 0x000C:
            return key::minus;
        case 0x000D:
            return key::equal;
        case 0x001A:
            return key::bracket_left;
        case 0x001B:
            return key::bracket_right;
        case 0x002B:
            return key::backslash;
        case 0x0027:
            return key::semicolon;
        case 0x0028:
            return key::quote;
        case 0x0033:
            return key::comma;
        case 0x0034:
            return key::period;
        case 0x0035:
            return key::slash;

        // ISO, JIS, Korean and ABNT extras
        case 0x0056:
            return key::intl_backslash;
        case 0x0073:
            return key::intl_ro;
        case 0x007D:
            return key::intl_yen;
        case 0x0079:
            return key::convert;
        case 0x007B:
            return key::non_convert;
        case 0x0070:
            return key::kana_mode;
        case 0x0072:
            return key::lang_1;
        case 0x0071:
            return key::lang_2;

        // Editing and modifiers
        case 0x000E:
            return key::backspace;
        case 0x000F:
            return key::tab;
        case 0x001C:
            return key::enter;
        case 0x0039:
            return key::space;
        case 0x003A:
            return key::caps_lock;
        case 0x002A:
            return key::shift_left;
        case 0x0036:
            return key::shift_right;
        case 0xE036:
            return key::shift_right; // Some CJK IMEs set the extended bit on it.
        case 0x001D:
            return key::control_left;
        case 0xE01D:
            return key::control_right;
        case 0x0038:
            return key::alt_left;
        case 0xE038:
            return key::alt_right;
        case 0xE05B:
            return key::meta_left;
        case 0xE05C:
            return key::meta_right;
        case 0xE05D:
            return key::context_menu;

        // Control pad and arrows: the extended twins of the numpad keys
        case 0xE052:
            return key::insert;
        case 0xE053:
            return key::delete_key;
        case 0xE047:
            return key::home;
        case 0xE04F:
            return key::end;
        case 0xE049:
            return key::page_up;
        case 0xE051:
            return key::page_down;
        case 0xE048:
            return key::arrow_up;
        case 0xE050:
            return key::arrow_down;
        case 0xE04B:
            return key::arrow_left;
        case 0xE04D:
            return key::arrow_right;

        // Numpad
        case 0xE045:
            return key::num_lock;
        case 0x0052:
            return key::numpad_0;
        case 0x004F:
            return key::numpad_1;
        case 0x0050:
            return key::numpad_2;
        case 0x0051:
            return key::numpad_3;
        case 0x004B:
            return key::numpad_4;
        case 0x004C:
            return key::numpad_5;
        case 0x004D:
            return key::numpad_6;
        case 0x0047:
            return key::numpad_7;
        case 0x0048:
            return key::numpad_8;
        case 0x0049:
            return key::numpad_9;
        case 0x004E:
            return key::numpad_add;
        case 0x004A:
            return key::numpad_subtract;
        case 0x0037:
            return key::numpad_multiply;
        case 0xE035:
            return key::numpad_divide;
        case 0x0053:
            return key::numpad_decimal;
        case 0xE01C:
            return key::numpad_enter;
        case 0x007E:
            return key::numpad_comma;
        case 0x0059:
            return key::numpad_equal;

        // Function section
        case 0x0001:
            return key::escape;
        case 0x003B:
            return key::f1;
        case 0x003C:
            return key::f2;
        case 0x003D:
            return key::f3;
        case 0x003E:
            return key::f4;
        case 0x003F:
            return key::f5;
        case 0x0040:
            return key::f6;
        case 0x0041:
            return key::f7;
        case 0x0042:
            return key::f8;
        case 0x0043:
            return key::f9;
        case 0x0044:
            return key::f10;
        case 0x0057:
            return key::f11;
        case 0x0058:
            return key::f12;
        case 0x0064:
            return key::f13;
        case 0x0065:
            return key::f14;
        case 0x0066:
            return key::f15;
        case 0x0067:
            return key::f16;
        case 0x0068:
            return key::f17;
        case 0x0069:
            return key::f18;
        case 0x006A:
            return key::f19;
        case 0x006B:
            return key::f20;
        case 0x006C:
            return key::f21;
        case 0x006D:
            return key::f22;
        case 0x006E:
            return key::f23;
        case 0x0076:
            return key::f24;
        case 0xE037:
            return key::print_screen;
        case 0x0054:
            return key::print_screen; // Alt+Print Screen reports SysRq's scan code.
        case 0x0046:
            return key::scroll_lock;
        case 0x0045:
            return key::pause;
        case 0xE046:
            return key::pause; // Ctrl+Pause reports Break's scan code.

        default:
            return std::nullopt;
    }
}

} // namespace tca::engine::platform::win32
