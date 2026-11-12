// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/input-frame.hpp"

namespace tca::engine::input {

keyboard_state& input_frame::keyboard() noexcept {
    return m_keyboard;
}

const keyboard_state& input_frame::keyboard() const noexcept {
    return m_keyboard;
}

mouse_state& input_frame::mouse() noexcept {
    return m_mouse;
}

const mouse_state& input_frame::mouse() const noexcept {
    return m_mouse;
}

std::string_view input_frame::text() const noexcept {
    return m_appendedText;
}

void input_frame::append_text(const std::string_view utf8Text) {
    m_appendedText += utf8Text;
}

void input_frame::consume(const key k) noexcept {
    m_keyboard.consume(k);
}

void input_frame::consume_all() noexcept {
    m_keyboard.consume_all();
    m_appendedText.clear();
}

void input_frame::end_tick() noexcept {
    m_keyboard.clear_edges();
    m_mouse.clear_edges();
    m_appendedText.clear();
}

input_frame merge(const input_frame& older, const input_frame& newer) {
    input_frame merged{};
    merged.keyboard() = keyboard_state::merge(older.keyboard(), newer.keyboard());
    merged.mouse() = mouse_state::merge(older.mouse(), newer.mouse());
    merged.append_text(older.text());
    merged.append_text(newer.text());

    return merged;
}

input_frame merge_into_pending(const input_frame& pending, const input_frame& incoming) {
    input_frame merged{};
    merged.keyboard() = keyboard_state::merge_into_pending(pending.keyboard(), incoming.keyboard());
    merged.mouse() = mouse_state::merge_into_pending(pending.mouse(), incoming.mouse());
    merged.append_text(pending.text());
    merged.append_text(incoming.text());

    return merged;
}

input_view::input_view(input_frame& frame) noexcept : m_frame{&frame} {}

keyboard_view input_view::keyboard() const noexcept {
    return keyboard_view{m_frame->keyboard()};
}

mouse_view input_view::mouse() const noexcept {
    return mouse_view{m_frame->mouse()};
}

std::string_view input_view::text() const noexcept {
    return m_frame->text();
}

void input_view::consume(const key k) const noexcept {
    m_frame->consume(k);
}

void input_view::consume_all() const noexcept {
    m_frame->consume_all();
}

input_frame input_view::snapshot() const {
    return input_frame{*m_frame};
}

} // namespace tca::engine::input
