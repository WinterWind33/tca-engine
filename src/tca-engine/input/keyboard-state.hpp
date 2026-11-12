// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <map>

// Engine
#include "input/key.hpp"

namespace tca::engine::input {

/**
 * @brief The state of every key for one frame: its level (down or up) and the edges (presses and
 *  releases) seen since the last clear_edges(). Platform layers feed it with press() and release()
 *  as events arrive; the application clears the edges once the frame's fixed steps have all read
 *  them, so a press is never lost between two steps nor reported twice.
 *
 *  Edges are counted rather than flagged, so a tap that starts and ends within one frame still
 *  reads as one press and one release, and two taps read as two.
 *
 *  A key can be consumed by whoever handled it first (e.g. a pause menu eating escape): from then
 *  on it reads as untouched (up, no edges) to every later reader, which cannot tell it apart from
 *  a key nobody pressed. Its edges stay hidden until the next clear_edges(); if it was held, it
 *  stays hidden until it is physically released, and that release is not reported. A new press
 *  after it is reported normally.
 *
 *  Games never get this class: they are handed a keyboard_view, which has the readers and
 *  consumption but none of the writers.
 */
class keyboard_state final {
public:
    /**
     * @brief Counter type for presses and releases within one frame.
     */
    using edge_count = std::uint32_t;

    /**
     * @brief Records that the given key went down.
     */
    void press(key k) noexcept;

    /**
     * @brief Records that the given key went up. Ignored if the key is not held: a duplicate
     *  release, a key that was already down when the window got focus, or a consumed key (whose
     *  release is swallowed).
     */
    void release(key k) noexcept;

    /**
     * @brief Forgets every press and release seen so far, keeping held keys down. Called once per
     *  frame, after every fixed step has read the state. Consumed held keys stay hidden.
     */
    void clear_edges() noexcept;

    /**
     * @brief Hides the given key from every later reader: its edges until the next clear_edges(),
     *  its held level until it is released (without reporting that release).
     */
    void consume(key k) noexcept;

    /**
     * @brief consume() for every key with input this frame (held, or with edges). Untouched keys
     *  have nothing to hide.
     */
    void consume_all() noexcept;

    /**
     * @brief Whether the given key is currently held down and not consumed.
     */
    [[nodiscard]] bool is_down(key k) const noexcept;

    /**
     * @brief Whether the given key went down at least once since the last clear_edges() and is
     *  not consumed.
     */
    [[nodiscard]] bool was_pressed(key k) const noexcept;

    /**
     * @brief Whether the given key went up at least once since the last clear_edges() and is not
     *  consumed.
     */
    [[nodiscard]] bool was_released(key k) const noexcept;

    /**
     * @brief How many times the given key went down since the last clear_edges(), or 0 if it is
     *  consumed.
     */
    [[nodiscard]] edge_count press_count(key k) const noexcept;

    /**
     * @brief How many times the given key went up since the last clear_edges(), or 0 if it is
     *  consumed.
     */
    [[nodiscard]] edge_count release_count(key k) const noexcept;

    /**
     * @brief Releases every held key, as on focus loss when the key-ups will never arrive: each
     *  reports one release, except consumed keys, which go up silently.
     */
    void release_all() noexcept;

    /**
     * @brief Folds two consecutive states into one, for when no fixed step read the older one:
     *  edge counts add up and the held level comes from the newer state. Neither input is
     *  modified. Neither has been read by a step yet, so neither holds consumed keys.
     */
    [[nodiscard]] static keyboard_state merge(const keyboard_state& older,
                                              const keyboard_state& newer);

    /**
     * @brief Folds the newer state into a fixed step's pending state, which steps read and may
     *  have consumed from. Like merge(), except that a key the pending state doesn't have
     *  (consumed by a step, or never seen) is only taken if the newer state has a press for it: a
     *  key a step consumed stays hidden while it is held, and its release isn't reported. Neither
     *  input is modified.
     */
    [[nodiscard]] static keyboard_state merge_into_pending(const keyboard_state& pending,
                                                           const keyboard_state& incoming);

private:
    struct key_state final {
        edge_count presses{};
        edge_count releases{};
        bool down{};
    };

    /**
     * @brief The stored state of the given key, or nullptr if it saw no input (or was consumed).
     */
    [[nodiscard]] const key_state* find_state(key k) const noexcept;

    std::map<key, key_state> m_keyStates{};
};

/**
 * @brief What a game sees of a keyboard_state: the readers and consumption, never press(),
 *  release() or clear_edges(), which belong to the platform layers and the application.
 *
 *  A view is a cheap, copyable handle: it does not own the state it looks at, so it must not
 *  outlive it. Every call forwards to that state.
 */
class keyboard_view final {
public:
    /**
     * @brief Creates a view over the given state.
     */
    explicit keyboard_view(keyboard_state& state) noexcept;

    /**
     * @brief Hides the given key from every later reader. See keyboard_state::consume().
     */
    void consume(key k) const noexcept;

    /**
     * @brief Hides every key from every later reader. See keyboard_state::consume_all().
     */
    void consume_all() const noexcept;

    /**
     * @brief See keyboard_state::is_down().
     */
    [[nodiscard]] bool is_down(key k) const noexcept;

    /**
     * @brief See keyboard_state::was_pressed().
     */
    [[nodiscard]] bool was_pressed(key k) const noexcept;

    /**
     * @brief See keyboard_state::was_released().
     */
    [[nodiscard]] bool was_released(key k) const noexcept;

    /**
     * @brief See keyboard_state::press_count().
     */
    [[nodiscard]] keyboard_state::edge_count press_count(key k) const noexcept;

    /**
     * @brief See keyboard_state::release_count().
     */
    [[nodiscard]] keyboard_state::edge_count release_count(key k) const noexcept;

private:
    keyboard_state* m_state{};
};

} // namespace tca::engine::input
