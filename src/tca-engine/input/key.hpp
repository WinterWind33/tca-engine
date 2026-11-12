// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace tca::engine::input {

/**
 * @brief A physical key *position* on the keyboard, never the character printed on it or produced
 *  by it. Each enumerator is named after what sits at that position on a US QWERTY keyboard:
 *  key::w is "the key where W is on US QWERTY", which types 'z' on AZERTY and 'ص' on an Arabic
 *  layout. That is what makes controls survive any layout: a game bound to WASD keeps the same
 *  hand position whatever the player's layout, input method or language is, and switching layout
 *  mid-game changes nothing.
 *
 *  The set and the naming follow the W3C UI Events KeyboardEvent.code specification
 *  (https://www.w3.org/TR/uievents-code/), so it covers the physical keys of ANSI, ISO, JIS, Korean
 *  and ABNT (Brazilian) keyboards, including the numeric keypad, as distinct keys. Platform layers
 *  map hardware scan codes to it, never layout-dependent key codes (e.g. Win32 virtual keys). A key
 *  the platform reports and this enum has no position for is ignored for controls; it can still
 *  produce text.
 *
 *  Typed text is a separate channel: games read characters (any script, input-method composition,
 *  dead keys already resolved) from the frame's text, not from keys. Use keys for controls and text
 *  for anything the player writes.
 *
 * @note The enumerators are contiguous from zero, so a key can index an array of key_count elements
 *  (see key_index()). Their numeric values are not stable across engine versions: persist a key by
 *  key_name(), never by its value.
 * @note Left and right modifiers are distinct keys: a game wanting "either shift" checks both.
 */
enum class key : std::uint8_t {
    // Writing system keys: letters
    a,
    b,
    c,
    d,
    e,
    f,
    g,
    h,
    i,
    j,
    k,
    l,
    m,
    n,
    o,
    p,
    q,
    r,
    s,
    t,
    u,
    v,
    w,
    x,
    y,
    z,

    // Writing system keys: the digit row (not the numeric keypad)
    digit_0,
    digit_1,
    digit_2,
    digit_3,
    digit_4,
    digit_5,
    digit_6,
    digit_7,
    digit_8,
    digit_9,

    // Writing system keys: punctuation positions, named after their US QWERTY legend
    backquote,     // `~ on US, the key left of 1 (Zenkaku/Hankaku on JIS)
    minus,         // -_
    equal,         // =+
    bracket_left,  // [{
    bracket_right, // ]}
    backslash,     // \| on US (ANSI), the key left of Enter on ISO
    semicolon,     // ;:
    quote,         // '"
    comma,         // ,<
    period,        // .>
    slash,         // /?

    // Writing system keys found only on non-US hardware
    intl_backslash, // ISO: the extra key between left shift and Z
    intl_ro,        // JIS/ABNT: the extra key left of right shift
    intl_yen,       // JIS: the extra key left of backspace

    // Functional keys
    backspace,
    tab,
    enter,
    space,
    caps_lock,
    shift_left,
    shift_right,
    control_left,
    control_right,
    alt_left,
    alt_right, // AltGr on layouts that have it
    meta_left, // Windows / Command key
    meta_right,
    context_menu,

    // Input method keys (Japanese and Korean keyboards)
    convert,     // JIS Henkan
    non_convert, // JIS Muhenkan
    kana_mode,   // JIS Katakana/Hiragana/Romaji
    lang_1,      // Korean Hangul/English toggle
    lang_2,      // Korean Hanja

    // Control pad
    insert,
    delete_key, // "delete" is a C++ keyword
    home,
    end,
    page_up,
    page_down,

    // Arrow pad
    arrow_up,
    arrow_down,
    arrow_left,
    arrow_right,

    // Numeric keypad: the same positions whether Num Lock is on or off
    num_lock,
    numpad_0,
    numpad_1,
    numpad_2,
    numpad_3,
    numpad_4,
    numpad_5,
    numpad_6,
    numpad_7,
    numpad_8,
    numpad_9,
    numpad_add,
    numpad_subtract,
    numpad_multiply,
    numpad_divide,
    numpad_decimal,
    numpad_enter,
    numpad_comma, // ABNT/JIS keypads
    numpad_equal,

    // Function section
    escape,
    f1,
    f2,
    f3,
    f4,
    f5,
    f6,
    f7,
    f8,
    f9,
    f10,
    f11,
    f12,
    f13,
    f14,
    f15,
    f16,
    f17,
    f18,
    f19,
    f20,
    f21,
    f22,
    f23,
    f24,
    print_screen,
    scroll_lock,
    pause,
};

/**
 * @brief Number of keys in the key enum, for sizing per-key arrays and bitsets.
 */
inline constexpr std::size_t key_count{static_cast<std::size_t>(key::pause) + 1};

/**
 * @brief Position of the given key in an array of key_count elements.
 */
[[nodiscard]] constexpr std::size_t key_index(const key k) noexcept {
    return static_cast<std::size_t>(std::to_underlying(k));
}

/**
 * @brief Stable, lowercase name of the given key, spelled like its enumerator (e.g. "space",
 *  "digit_1", "shift_left"), except key::delete_key which is "delete". Meant for logs and for
 *  persisting bindings, so a name never changes once released.
 *
 * @note This is not the label to show a player: on their layout key::q may read 'A'. Labels come
 *  from the platform, which knows the active layout.
 */
[[nodiscard]] std::string_view key_name(key k) noexcept;

/**
 * @brief The key whose key_name() is the given name, so that saved bindings can be read back.
 *  Matched exactly: a name in another case, with surrounding spaces or of no key is not one.
 *
 * @return The key, or nothing if no key has that name.
 */
[[nodiscard]] std::optional<key> key_from_name(std::string_view name) noexcept;

} // namespace tca::engine::input
