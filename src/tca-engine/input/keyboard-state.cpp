// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/keyboard-state.hpp"

// C++ STL
#include <ranges>

namespace tca::engine::input {

void keyboard_state::press(const key k) noexcept {
    auto& keyState{m_keyStates[k]};
    ++(keyState.presses);
    keyState.down = true;
}

void keyboard_state::release(const key k) noexcept {
    const auto it{m_keyStates.find(k)};
    if ((it == std::ranges::cend(m_keyStates)) || !(it->second.down)) {
        // If there is nothing to release, skip it.
        return;
    }

    auto& keyState{it->second};
    ++(keyState.releases);
    keyState.down = false;
}

void keyboard_state::clear_edges() noexcept {
    for (auto& keyState : m_keyStates | std::views::values) {
        keyState.presses = 0;
        keyState.releases = 0;
    }
}

void keyboard_state::consume(const key k) noexcept {
    // Completely remove the key from the state map
    // since no one should ever see that that key has been an input.
    m_keyStates.erase(k);
}

void keyboard_state::consume_all() noexcept {
    m_keyStates.clear();
}

bool keyboard_state::is_down(const key k) const noexcept {
    const auto* const keyState{find_state(k)};
    return (keyState != nullptr) && (keyState->down);
}

bool keyboard_state::was_pressed(const key k) const noexcept {
    return press_count(k) > 0;
}

bool keyboard_state::was_released(const key k) const noexcept {
    return release_count(k) > 0;
}

keyboard_state::edge_count keyboard_state::press_count(const key k) const noexcept {
    const auto* const keyState{find_state(k)};
    return (keyState != nullptr) ? (keyState->presses) : 0;
}

keyboard_state::edge_count keyboard_state::release_count(const key k) const noexcept {
    const auto* const keyState{find_state(k)};
    return (keyState != nullptr) ? (keyState->releases) : 0;
}

void keyboard_state::release_all() noexcept {
    for (const key k : m_keyStates | std::views::keys) {
        release(k);
    }
}

keyboard_state keyboard_state::merge(const keyboard_state& older, const keyboard_state& newer) {
    // Start from the newer state so its held levels win, then add the older edges on top. A key
    // only seen in the older state gets an entry that is up, with the older edges.
    keyboard_state merged{newer};
    for (const auto& [k, olderKeyState] : older.m_keyStates) {
        auto& mergedKeyState{merged.m_keyStates[k]};
        mergedKeyState.presses += olderKeyState.presses;
        mergedKeyState.releases += olderKeyState.releases;
    }

    return merged;
}

keyboard_state keyboard_state::merge_into_pending(const keyboard_state& pending,
                                                  const keyboard_state& incoming) {
    // Start from the pending state so the keys it doesn't have stay out unless pressed again. A key
    // only seen in the pending state reads as up with its edges kept, as in merge().
    keyboard_state merged{pending};
    for (auto& mergedKeyState : merged.m_keyStates | std::views::values) {
        mergedKeyState.down = false;
    }

    for (const auto& [k, incomingKeyState] : incoming.m_keyStates) {
        const auto it{merged.m_keyStates.find(k)};
        if (it == std::ranges::end(merged.m_keyStates)) {
            if (incomingKeyState.presses > 0) {
                merged.m_keyStates.emplace(k, incomingKeyState);
            }

            continue;
        }

        auto& mergedKeyState{it->second};
        mergedKeyState.presses += incomingKeyState.presses;
        mergedKeyState.releases += incomingKeyState.releases;
        mergedKeyState.down = incomingKeyState.down;
    }

    return merged;
}

const keyboard_state::key_state* keyboard_state::find_state(const key k) const noexcept {
    const auto it{m_keyStates.find(k)};
    return (it != std::ranges::cend(m_keyStates)) ? &(it->second) : nullptr;
}

keyboard_view::keyboard_view(keyboard_state& state) noexcept : m_state{&state} {}

void keyboard_view::consume(const key k) const noexcept {
    m_state->consume(k);
}

void keyboard_view::consume_all() const noexcept {
    m_state->consume_all();
}

bool keyboard_view::is_down(const key k) const noexcept {
    return m_state->is_down(k);
}

bool keyboard_view::was_pressed(const key k) const noexcept {
    return m_state->was_pressed(k);
}

bool keyboard_view::was_released(const key k) const noexcept {
    return m_state->was_released(k);
}

keyboard_state::edge_count keyboard_view::press_count(const key k) const noexcept {
    return m_state->press_count(k);
}

keyboard_state::edge_count keyboard_view::release_count(const key k) const noexcept {
    return m_state->release_count(k);
}

} // namespace tca::engine::input
