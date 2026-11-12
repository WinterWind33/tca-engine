// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/input-collector.hpp"

// Third-party
#include <gsl/assert>

namespace tca::engine::input {

void append_utf8(std::string& utf8Text, const char32_t codePoint) {
    const auto byte = [](const char32_t bits) {
        return static_cast<char>(bits);
    };

    if (codePoint < 0x80) {
        utf8Text += byte(codePoint);
    } else if (codePoint < 0x800) {
        utf8Text += byte(0xC0 | (codePoint >> 6));
        utf8Text += byte(0x80 | (codePoint & 0x3F));
    } else if (codePoint < 0x10000) {
        if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
            return; // Surrogates are not code points.
        }
        utf8Text += byte(0xE0 | (codePoint >> 12));
        utf8Text += byte(0x80 | ((codePoint >> 6) & 0x3F));
        utf8Text += byte(0x80 | (codePoint & 0x3F));
    } else if (codePoint <= 0x10FFFF) {
        utf8Text += byte(0xF0 | (codePoint >> 18));
        utf8Text += byte(0x80 | ((codePoint >> 12) & 0x3F));
        utf8Text += byte(0x80 | ((codePoint >> 6) & 0x3F));
        utf8Text += byte(0x80 | (codePoint & 0x3F));
    }
    // Anything above 0x10FFFF is not a code point: append nothing.
}

input_collector::input_collector(input_frame& frame) noexcept : m_frame{&frame} {}

void input_collector::on_key_down(const key k, const bool isRepeat) noexcept {
    if (!isRepeat) {
        // When a key is repeated, it should not be treated as
        // a new press at all.
        Expects(m_frame != nullptr);
        m_frame->keyboard().press(k);
    }
}

void input_collector::on_key_up(const key k) noexcept {
    Expects(m_frame != nullptr);
    m_frame->keyboard().release(k);
}

void input_collector::on_char(const char32_t codePoint) {
    Expects(m_frame != nullptr);
    if (codePoint < 0x20 // control code
        || (codePoint >= 0x7F && codePoint <= 0x9f)) {
        return;
    }
    std::string utf8Str{};
    append_utf8(utf8Str, codePoint);
    m_frame->append_text(utf8Str);
}

void input_collector::on_mouse_button_down(const mouse_button button) noexcept {
    Expects(m_frame != nullptr);
    m_frame->mouse().button_down(button);
}

void input_collector::on_mouse_button_up(const mouse_button button) noexcept {
    Expects(m_frame != nullptr);
    m_frame->mouse().button_up(button);
}

void input_collector::on_pointer_moved(const pointer_position position) noexcept {
    Expects(m_frame != nullptr);
    m_frame->mouse().move_pointer(position);
}

void input_collector::on_pointer_entered() noexcept {
    Expects(m_frame != nullptr);
    m_frame->mouse().pointer_entered();
}

void input_collector::on_pointer_left() noexcept {
    Expects(m_frame != nullptr);
    m_frame->mouse().pointer_left();
}

void input_collector::on_scroll(const wheel_delta delta) noexcept {
    Expects(m_frame != nullptr);
    m_frame->mouse().scroll(delta);
}

void input_collector::on_focus_lost() noexcept {
    Expects(m_frame != nullptr);
    // When the window loses focus we need to release all
    // the pressed keys, otherwise we won't be able to detect
    // a release and this can cause big bugs.
    m_frame->keyboard().release_all();
    m_frame->mouse().release_all();
}

void input_collector::clear() noexcept {
    m_frame->consume_all();
}

} // namespace tca::engine::input
