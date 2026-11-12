// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/mouse-state.hpp"

// C++ STL
#include <cstddef>
#include <optional>

namespace tca::engine::input {

namespace {

[[nodiscard]] constexpr wheel_delta add(const wheel_delta lhs, const wheel_delta rhs) noexcept {
    return {.vertical = lhs.vertical + rhs.vertical, .horizontal = lhs.horizontal + rhs.horizontal};
}

} // namespace

void mouse_state::button_down(const mouse_button button) noexcept {
    auto& slot{m_buttonStates[mouse_button_index(button)]};
    if (!slot.has_value()) {
        // Not emplace(): clang decides whether button_state is default-constructible while
        // mouse_state is still incomplete, gets it wrong and remembers it, so emplace() with no
        // arguments matches nothing.
        slot = button_state{};
    }

    ++(slot->presses);
    slot->down = true;
}

void mouse_state::button_up(const mouse_button button) noexcept {
    auto& slot{m_buttonStates[mouse_button_index(button)]};
    if (!slot.has_value() || !(slot->down)) {
        // If there is nothing to release, skip it.
        return;
    }

    ++(slot->releases);
    slot->down = false;
}

void mouse_state::clear_edges() noexcept {
    for (auto& slot : m_buttonStates) {
        if (slot.has_value()) {
            slot->presses = 0;
            slot->releases = 0;
        }
    }

    m_wheel = {};
}

void mouse_state::consume(const mouse_button button) noexcept {
    // Empty the slot entirely, since no one should ever see that the button has been an input.
    m_buttonStates[mouse_button_index(button)].reset();
}

void mouse_state::consume_all() noexcept {
    m_buttonStates.fill(std::nullopt);
    consume_wheel();
}

bool mouse_state::is_down(const mouse_button button) const noexcept {
    const auto* const buttonState{find_state(button)};
    return (buttonState != nullptr) && (buttonState->down);
}

bool mouse_state::was_pressed(const mouse_button button) const noexcept {
    return press_count(button) > 0;
}

bool mouse_state::was_released(const mouse_button button) const noexcept {
    return release_count(button) > 0;
}

mouse_state::edge_count mouse_state::press_count(const mouse_button button) const noexcept {
    const auto* const buttonState{find_state(button)};
    return (buttonState != nullptr) ? (buttonState->presses) : 0;
}

mouse_state::edge_count mouse_state::release_count(const mouse_button button) const noexcept {
    const auto* const buttonState{find_state(button)};
    return (buttonState != nullptr) ? (buttonState->releases) : 0;
}

void mouse_state::release_all() noexcept {
    for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
        button_up(static_cast<mouse_button>(index));
    }
}

void mouse_state::move_pointer(const pointer_position position) noexcept {
    m_pointerState.position = position;
}

void mouse_state::pointer_entered() noexcept {
    m_pointerState.inside = true;
}

void mouse_state::pointer_left() noexcept {
    m_pointerState.inside = false;
}

std::optional<pointer_position> mouse_state::pointer() const noexcept {
    return m_pointerState.position;
}

bool mouse_state::is_pointer_inside() const noexcept {
    return m_pointerState.inside;
}

void mouse_state::scroll(const wheel_delta delta) noexcept {
    m_wheel = add(m_wheel, delta);
}

wheel_delta mouse_state::wheel() const noexcept {
    return m_wheel;
}

void mouse_state::consume_wheel() noexcept {
    // Turning it back to zero is enough: a turn after this is new input, reported as usual.
    m_wheel = {};
}

mouse_state mouse_state::merge(const mouse_state& older, const mouse_state& newer) {
    // Start from the newer state so its held levels win, then add the older edges on top. A button
    // only seen in the older state gets a slot that is up, with the older edges.
    mouse_state merged{newer};
    for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
        const auto& olderSlot{older.m_buttonStates[index]};
        if (!olderSlot.has_value()) {
            continue;
        }

        auto& mergedSlot{merged.m_buttonStates[index]};
        if (!mergedSlot.has_value()) {
            mergedSlot = button_state{};
        }

        mergedSlot->presses += olderSlot->presses;
        mergedSlot->releases += olderSlot->releases;
    }

    merged.m_pointerState.position = merged_pointer_position(older, newer);
    merged.m_wheel = add(older.m_wheel, newer.m_wheel);

    return merged;
}

mouse_state mouse_state::merge_into_pending(const mouse_state& pending,
                                            const mouse_state& incoming) {
    // Start from the pending state so the buttons it doesn't have stay out unless pressed again. A
    // button only seen in the pending state reads as up with its edges kept, as in merge().
    mouse_state merged{pending};
    for (auto& slot : merged.m_buttonStates) {
        if (slot.has_value()) {
            slot->down = false;
        }
    }

    for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
        const auto& incomingSlot{incoming.m_buttonStates[index]};
        if (!incomingSlot.has_value()) {
            continue;
        }

        auto& mergedSlot{merged.m_buttonStates[index]};
        if (!mergedSlot.has_value()) {
            if (incomingSlot->presses > 0) {
                mergedSlot = incomingSlot;
            }

            continue;
        }

        mergedSlot->presses += incomingSlot->presses;
        mergedSlot->releases += incomingSlot->releases;
        mergedSlot->down = incomingSlot->down;
    }

    // Steps can't consume the pointer, so it folds exactly as in merge().
    merged.m_pointerState.inside = incoming.m_pointerState.inside;
    merged.m_pointerState.position = merged_pointer_position(pending, incoming);
    merged.m_wheel = add(pending.m_wheel, incoming.m_wheel);

    return merged;
}

std::optional<pointer_position> mouse_state::merged_pointer_position(
    const mouse_state& older, const mouse_state& newer) noexcept {
    // The position is a level, like a held button, so the newer one wins; the older one only
    // stands in while the newer state has never seen the pointer move.
    return newer.m_pointerState.position.has_value() ? newer.m_pointerState.position
                                                     : older.m_pointerState.position;
}

const mouse_state::button_state* mouse_state::find_state(const mouse_button button) const noexcept {
    const auto& slot{m_buttonStates[mouse_button_index(button)]};
    return slot.has_value() ? &(*slot) : nullptr;
}

mouse_view::mouse_view(mouse_state& state) noexcept : m_state{&state} {}

void mouse_view::consume(const mouse_button button) const noexcept {
    m_state->consume(button);
}

void mouse_view::consume_all() const noexcept {
    m_state->consume_all();
}

bool mouse_view::is_down(const mouse_button button) const noexcept {
    return m_state->is_down(button);
}

bool mouse_view::was_pressed(const mouse_button button) const noexcept {
    return m_state->was_pressed(button);
}

bool mouse_view::was_released(const mouse_button button) const noexcept {
    return m_state->was_released(button);
}

mouse_state::edge_count mouse_view::press_count(const mouse_button button) const noexcept {
    return m_state->press_count(button);
}

mouse_state::edge_count mouse_view::release_count(const mouse_button button) const noexcept {
    return m_state->release_count(button);
}

std::optional<pointer_position> mouse_view::pointer() const noexcept {
    return m_state->pointer();
}

bool mouse_view::is_pointer_inside() const noexcept {
    return m_state->is_pointer_inside();
}

wheel_delta mouse_view::wheel() const noexcept {
    return m_state->wheel();
}

void mouse_view::consume_wheel() const noexcept {
    m_state->consume_wheel();
}

} // namespace tca::engine::input
