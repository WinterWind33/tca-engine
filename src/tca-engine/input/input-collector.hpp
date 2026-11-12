// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <string>

// Engine
#include "input/input-frame.hpp"
#include "input/key.hpp"
#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"

namespace tca::engine::input {

/**
 * @brief Appends the UTF-8 encoding of the given code point (1 to 4 bytes) to the given string.
 *  Values that are not code points (UTF-16 surrogates, anything above 0x10FFFF) append nothing.
 *  Every code point is encoded, control characters included: deciding what counts as text is
 *  the collector's job.
 */
void append_utf8(std::string& utf8Text, char32_t codePoint);

/**
 * @brief Turns platform events into the application's persistent input_frame: platform layers
 *  call it as messages arrive, it writes straight into the frame.
 *
 *  It keeps no key state of its own, so it never brings back a key a reader consumed: a consumed
 *  held key stays hidden through its repeats and its release, until it is pressed again.
 *
 *  It filters what every backend would otherwise have to: key repeats are not presses, and
 *  only printable code points become text.
 */
class input_collector final {
public:
    /**
     * @brief Creates a collector writing into the given frame, which must outlive it.
     */
    explicit input_collector(input_frame& frame) noexcept;

    /**
     * @brief A key went down. A repeat (the key auto-repeating while held, or a key that was
     *  already down when the window got focus) is not a new press and is ignored.
     */
    void on_key_down(key k, bool isRepeat) noexcept;

    /**
     * @brief A key went up. Ignored if the key is not held (see keyboard_state::release()).
     */
    void on_key_up(key k) noexcept;

    /**
     * @brief A character was typed: appended to the frame's text as UTF-8. Values that are not
     *  code points and control characters (below 0x20, and 0x7F to 0x9F) are dropped.
     */
    void on_char(char32_t codePoint);

    /**
     * @brief A mouse button went down.
     */
    void on_mouse_button_down(mouse_button button) noexcept;

    /**
     * @brief A mouse button went up. Ignored if the button is not held (see
     *  mouse_state::button_up()).
     */
    void on_mouse_button_up(mouse_button button) noexcept;

    /**
     * @brief The pointer moved to the given position, in client-area pixels.
     */
    void on_pointer_moved(pointer_position position) noexcept;

    /**
     * @brief The pointer entered the window's client area.
     */
    void on_pointer_entered() noexcept;

    /**
     * @brief The pointer left the window's client area. Its last position is kept.
     */
    void on_pointer_left() noexcept;

    /**
     * @brief The wheel turned by the given amount, in notches (see wheel_delta).
     */
    void on_scroll(wheel_delta delta) noexcept;

    /**
     * @brief The window lost focus, so the key-ups of held keys and the button-ups of held mouse
     *  buttons will never arrive: every held key and button is released (see
     *  keyboard_state::release_all() and mouse_state::release_all()). The typed text, the pointer
     *  and the wheel are left alone.
     */
    void on_focus_lost() noexcept;

    /**
     * @brief Forgets every held key, every edge and the typed text, without reporting releases.
     *  Keys held through it are ignored until they are released and pressed again.
     */
    void clear() noexcept;

private:
    input_frame* m_frame{};
};

} // namespace tca::engine::input
