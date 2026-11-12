// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <array>
#include <cstdint>
#include <optional>

// Engine
#include "input/mouse-button.hpp"

namespace tca::engine::input {

/**
 * @brief Where the pointer is, in physical pixels of the window's client area (the unit the window
 *  size is reported in), with the origin at its top-left corner, x to the right and y down.
 *
 *  It can lie outside the client area, negative or past its size: while a button is held the
 *  window keeps receiving the pointer after it leaves, so a drag can be followed to its end.
 */
struct pointer_position final {
    std::int32_t x{};
    std::int32_t y{};

    constexpr bool operator==(const pointer_position&) const noexcept = default;
};

/**
 * @brief How far the mouse wheel turned, in notches: one notch of a standard wheel is 1, whatever
 *  the platform reports for it, so platform layers convert before feeding it in. Smooth-scrolling
 *  wheels and touchpads report fractions of a notch, which is why it is not a whole number: a game
 *  that wants discrete steps rounds or accumulates them itself.
 *
 *  Vertical is positive when the wheel turns away from the player (scrolling up), horizontal is
 *  positive to the right.
 */
struct wheel_delta final {
    float vertical{};
    float horizontal{};

    constexpr bool operator==(const wheel_delta&) const noexcept = default;
};

/**
 * @brief The state of every mouse button for one frame: its level (down or up) and the edges
 *  (presses and releases) seen since the last clear_edges(). It follows keyboard_state's rules:
 *  platform layers feed it with button_down() and button_up() as events arrive, the application
 *  clears the edges once the frame's fixed steps have all read them, and edges are counted, so a
 *  click that starts and ends within one frame still reads as one press and one release.
 *
 *  A button can be consumed by whoever handled it first (e.g. a UI panel eating a click meant for
 *  it): from then on it reads as untouched (up, no edges) to every later reader. Its edges stay
 *  hidden until the next clear_edges(); if it was held, it stays hidden until it is physically
 *  released, and that release is not reported. A new press after it is reported normally.
 *
 *  The pointer (where it is, whether it is inside the window) is a level that nobody can consume.
 *  The wheel is summed over the frame like the edges, cleared with them, and can be consumed.
 *
 *  Games never get this class: they are handed a mouse_view, which has the readers and
 *  consumption but none of the writers.
 */
class mouse_state final {
public:
    /**
     * @brief Counter type for presses and releases within one frame.
     */
    using edge_count = std::uint32_t;

    /**
     * @brief Records that the given button went down.
     */
    void button_down(mouse_button button) noexcept;

    /**
     * @brief Records that the given button went up. Ignored if the button is not held: a
     *  duplicate release, a button that was already down when the window got focus, or a consumed
     *  button (whose release is swallowed).
     */
    void button_up(mouse_button button) noexcept;

    /**
     * @brief Forgets every press and release seen so far and resets the wheel, keeping held buttons
     *  down. Called once per frame, after every fixed step has read the state. Consumed held
     *  buttons stay hidden.
     */
    void clear_edges() noexcept;

    /**
     * @brief Hides the given button from every later reader: its edges until the next
     *  clear_edges(), its held level until it is released (without reporting that release).
     */
    void consume(mouse_button button) noexcept;

    /**
     * @brief consume() for every button with input this frame (held, or with edges), and
     *  consume_wheel(). Untouched buttons have nothing to hide. The pointer is left alone.
     */
    void consume_all() noexcept;

    /**
     * @brief Whether the given button is currently held down and not consumed.
     */
    [[nodiscard]] bool is_down(mouse_button button) const noexcept;

    /**
     * @brief Whether the given button went down at least once since the last clear_edges() and is
     *  not consumed.
     */
    [[nodiscard]] bool was_pressed(mouse_button button) const noexcept;

    /**
     * @brief Whether the given button went up at least once since the last clear_edges() and is
     *  not consumed.
     */
    [[nodiscard]] bool was_released(mouse_button button) const noexcept;

    /**
     * @brief How many times the given button went down since the last clear_edges(), or 0 if it is
     *  consumed.
     */
    [[nodiscard]] edge_count press_count(mouse_button button) const noexcept;

    /**
     * @brief How many times the given button went up since the last clear_edges(), or 0 if it is
     *  consumed.
     */
    [[nodiscard]] edge_count release_count(mouse_button button) const noexcept;

    /**
     * @brief Releases every held button, as on focus loss when the button-ups will never arrive:
     *  each reports one release, except consumed buttons, which go up silently.
     */
    void release_all() noexcept;

    /**
     * @brief Records that the pointer moved to the given position. Whether it is inside the client
     *  area is reported separately, by pointer_entered() and pointer_left().
     */
    void move_pointer(pointer_position position) noexcept;

    /**
     * @brief Records that the pointer entered the window's client area.
     */
    void pointer_entered() noexcept;

    /**
     * @brief Records that the pointer left the window's client area. Its last position is kept.
     */
    void pointer_left() noexcept;

    /**
     * @brief The last position the pointer moved to, or nothing if it hasn't moved over the window
     *  yet. Neither clear_edges() nor consuming buttons forget it.
     */
    [[nodiscard]] std::optional<pointer_position> pointer() const noexcept;

    /**
     * @brief Whether the pointer is inside the window's client area.
     */
    [[nodiscard]] bool is_pointer_inside() const noexcept;

    /**
     * @brief Records that the wheel turned by the given amount, added to what it turned since the
     *  last clear_edges().
     */
    void scroll(wheel_delta delta) noexcept;

    /**
     * @brief How far the wheel turned since the last clear_edges(), or zero on both axes if it was
     *  consumed since. Turns are summed per axis, so opposite turns cancel out.
     */
    [[nodiscard]] wheel_delta wheel() const noexcept;

    /**
     * @brief Hides the wheel from every later reader (e.g. a scrolling panel that used it): it
     *  reads as zero until it turns again.
     */
    void consume_wheel() noexcept;

    /**
     * @brief Folds two consecutive states into one, for when no fixed step read the older one:
     *  edge counts and wheel turns add up and the held level comes from the newer state. The
     *  pointer comes from the newer state too, or from the older one if the newer state has no
     *  position yet. Neither input is modified. Neither has been read by a step yet, so neither
     *  holds consumed buttons.
     */
    [[nodiscard]] static mouse_state merge(const mouse_state& older, const mouse_state& newer);

    /**
     * @brief Folds the newer state into a fixed step's pending state, which steps read and may
     *  have consumed from. Like merge(), except that a button the pending state doesn't have
     *  (consumed by a step, or never seen) is only taken if the newer state has a press for it: a
     *  button a step consumed stays hidden while it is held, and its release isn't reported. The
     *  pointer and the wheel are folded as in merge(): a wheel a step consumed is already zero, so
     *  only the newer turns are reported. Neither input is modified.
     */
    [[nodiscard]] static mouse_state merge_into_pending(const mouse_state& pending,
                                                        const mouse_state& incoming);

private:
    struct button_state final {
        edge_count presses{};
        edge_count releases{};
        bool down{};
    };

    struct pointer_state final {
        bool inside{};
        std::optional<pointer_position> position{};
    };

    /**
     * @brief The pointer position of two consecutive states folded together: the newer one's, or
     *  the older one's if the newer state has no position yet.
     */
    [[nodiscard]] static std::optional<pointer_position> merged_pointer_position(
        const mouse_state& older, const mouse_state& newer) noexcept;

    /**
     * @brief The stored state of the given button, or nullptr if it saw no input (or was
     *  consumed).
     */
    [[nodiscard]] const button_state* find_state(mouse_button button) const noexcept;

    // An empty slot is a button nobody has seen input for, or one that was consumed.
    std::array<std::optional<button_state>, MOUSE_BUTTONS_COUNT> m_buttonStates{};
    pointer_state m_pointerState{};
    wheel_delta m_wheel{};
};

/**
 * @brief What a game sees of a mouse_state: the readers and consumption, never the writers
 *  (button_down(), move_pointer(), scroll(), clear_edges()...), which belong to the platform
 *  layers and the application.
 *
 *  A view is a cheap, copyable handle: it does not own the state it looks at, so it must not
 *  outlive it. Every call forwards to that state.
 */
class mouse_view final {
public:
    /**
     * @brief Creates a view over the given state.
     */
    explicit mouse_view(mouse_state& state) noexcept;

    /**
     * @brief Hides the given button from every later reader. See mouse_state::consume().
     */
    void consume(mouse_button button) const noexcept;

    /**
     * @brief Hides every button and the wheel from every later reader. See
     *  mouse_state::consume_all().
     */
    void consume_all() const noexcept;

    /**
     * @brief See mouse_state::is_down().
     */
    [[nodiscard]] bool is_down(mouse_button button) const noexcept;

    /**
     * @brief See mouse_state::was_pressed().
     */
    [[nodiscard]] bool was_pressed(mouse_button button) const noexcept;

    /**
     * @brief See mouse_state::was_released().
     */
    [[nodiscard]] bool was_released(mouse_button button) const noexcept;

    /**
     * @brief See mouse_state::press_count().
     */
    [[nodiscard]] mouse_state::edge_count press_count(mouse_button button) const noexcept;

    /**
     * @brief See mouse_state::release_count().
     */
    [[nodiscard]] mouse_state::edge_count release_count(mouse_button button) const noexcept;

    /**
     * @brief See mouse_state::pointer().
     */
    [[nodiscard]] std::optional<pointer_position> pointer() const noexcept;

    /**
     * @brief See mouse_state::is_pointer_inside().
     */
    [[nodiscard]] bool is_pointer_inside() const noexcept;

    /**
     * @brief See mouse_state::wheel().
     */
    [[nodiscard]] wheel_delta wheel() const noexcept;

    /**
     * @brief Hides the wheel from every later reader. See mouse_state::consume_wheel().
     */
    void consume_wheel() const noexcept;

private:
    mouse_state* m_state{};
};

} // namespace tca::engine::input
