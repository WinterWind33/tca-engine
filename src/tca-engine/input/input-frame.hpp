// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <string>
#include <string_view>

// Engine
#include "input/key.hpp"
#include "input/keyboard-state.hpp"
#include "input/mouse-state.hpp"

namespace tca::engine::input {

/**
 * @brief Everything the player did during one frame: the keyboard state (levels and edges), the
 *  text typed, as UTF-8, and the mouse state (buttons, pointer and wheel). Platform layers fill it
 *  as events arrive; games read it.
 *
 *  Keys are for controls and text is for anything the player writes: the two channels are
 *  independent, except that consume_all() takes both (whoever grabs the whole keyboard, e.g. a
 *  focused text box, also takes what is typed). The mouse is a channel of its own: grabbing the
 *  keyboard doesn't take it, so it is consumed through mouse() instead.
 *
 *  Games never get this class: they are handed an input_view, which has no append_text() and no
 *  end_tick().
 */
class input_frame final {
public:
    /**
     * @brief The keyboard state of this frame.
     */
    [[nodiscard]] keyboard_state& keyboard() noexcept;

    /**
     * @brief The keyboard state of this frame.
     */
    [[nodiscard]] const keyboard_state& keyboard() const noexcept;

    /**
     * @brief The mouse state of this frame.
     */
    [[nodiscard]] mouse_state& mouse() noexcept;

    /**
     * @brief The mouse state of this frame.
     */
    [[nodiscard]] const mouse_state& mouse() const noexcept;

    /**
     * @brief The text typed during this frame, as UTF-8, or empty if it has been consumed.
     */
    [[nodiscard]] std::string_view text() const noexcept;

    /**
     * @brief Appends the given UTF-8 text to what was typed during this frame.
     */
    void append_text(std::string_view utf8Text);

    /**
     * @brief Hides the given key from every later reader. The typed text is left alone.
     */
    void consume(key k) noexcept;

    /**
     * @brief Hides every key and the typed text from every later reader. The mouse is left alone.
     */
    void consume_all() noexcept;

    /**
     * @brief Closes the tick: forgets the key and button edges, the wheel and the typed text,
     *  keeping held keys and buttons down and the pointer where it is. Called by the application
     *  at the end of every tick() on its persistent frame. Consumed held keys and buttons stay
     *  hidden until released.
     */
    void end_tick() noexcept;

private:
    keyboard_state m_keyboard{};
    mouse_state m_mouse{};
    std::string m_appendedText{};
};

/**
 * @brief Folds two consecutive frames into one, for when no fixed step ran in between so no edge
 *  is lost: edge counts add up, the held state comes from the newer frame and text is appended,
 *  older first. Mice are folded with mouse_state::merge(). Neither input is modified.
 */
[[nodiscard]] input_frame merge(const input_frame& older, const input_frame& newer);

/**
 * @brief Folds the newer frame into a fixed step's pending frame, which steps read and may have
 *  consumed from: keyboards as keyboard_state::merge_into_pending() (a key a step consumed stays
 *  hidden until pressed again), mice as mouse_state::merge_into_pending(), text appended,
 *  pending first. Neither input is modified.
 */
[[nodiscard]] input_frame merge_into_pending(const input_frame& pending,
                                             const input_frame& incoming);

/**
 * @brief What a game sees of an input_frame: the keyboard through a keyboard_view, the mouse
 *  through a mouse_view, the typed text and consumption, never append_text() or end_tick(), which
 *  belong to the platform layers and the application.
 *
 *  A view is a cheap, copyable handle: it does not own the frame it looks at, so it must not
 *  outlive it. Every call forwards to that frame.
 */
class input_view final {
public:
    /**
     * @brief Creates a view over the given frame.
     */
    explicit input_view(input_frame& frame) noexcept;

    /**
     * @brief The keyboard of the frame, read-and-consume only.
     */
    [[nodiscard]] keyboard_view keyboard() const noexcept;

    /**
     * @brief The mouse of the frame, read-and-consume only.
     */
    [[nodiscard]] mouse_view mouse() const noexcept;

    /**
     * @brief See input_frame::text().
     */
    [[nodiscard]] std::string_view text() const noexcept;

    /**
     * @brief Hides the given key from every later reader. See input_frame::consume().
     */
    void consume(key k) const noexcept;

    /**
     * @brief Hides every key and the typed text from every later reader, leaving the mouse alone.
     *  See input_frame::consume_all().
     */
    void consume_all() const noexcept;

    /**
     * @brief A copy of the frame as this view sees it now, detached from it: levels, edge counts,
     *  typed text and the mouse, with consumed keys, buttons, wheel and text already hidden.
     *  Writing to the copy doesn't reach the frame and later changes to the frame (end_tick()
     *  included) don't reach the copy. Used by readers that keep input across ticks, such as
     *  fixed_step_updatable's pending frame.
     */
    [[nodiscard]] input_frame snapshot() const;

private:
    input_frame* m_frame{};
};

} // namespace tca::engine::input
