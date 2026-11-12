// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <chrono>
#include <cstddef>
#include <limits>

// Engine
#include "core/utilities/ranged-value.hpp"
#include "input/input-frame.hpp"
#include "rendering/tickables.hpp"

namespace tca::engine::rendering {

/**
 * @brief Everything a fixed_step_updatable's step() is handed: the input the step should react to.
 *
 *  The first step run after a frame gets every edge (presses, releases) and the typed text that
 *  arrived since the previous step ran, even across frames that ran no step. Later steps in the
 *  same frame see held keys only, so an edge is reported exactly once.
 *
 *  The input is an input::input_view: a step can read and consume input, not write it. A context
 *  doesn't own the frame its input looks at, so it must not outlive the step it was given to.
 */
class step_context final {
public:
    /**
     * @brief Creates a context for one step.
     *
     * @param input The input the step reacts to.
     */
    explicit step_context(input::input_view input) noexcept;

    /**
     * @brief The input of this step, read-and-consume only.
     */
    [[nodiscard]] input::input_view input() const noexcept;

private:
    input::input_view m_input;
};

/**
 * @brief A fixed_step_updatable is an updatable that runs its simulation at a fixed rate, whatever
 * the rate its update() method is called at. This is useful for games whose rules are written in
 * fixed steps ("gravity 0.25 per step", "fire every 3 steps"), which would otherwise run faster on
 * a 144 Hz display than on a 60 Hz one.
 *
 * Every update() adds the frame delta to an internal accumulator, then calls step() once for each
 * whole step duration the accumulator holds. A frame can therefore run zero, one or several steps.
 * The time left over, less than one step, is carried over to the next frame. It is kept in whole
 * nanoseconds, so no drift builds up over a long session.
 *
 * At most get_max_steps_per_frame() steps run in a single frame. The whole steps a frame couldn't
 * run are dropped rather than carried over, so a stall (a window drag, a breakpoint, a slow load)
 * doesn't cause a burst of catch-up steps afterwards. They are counted by get_dropped_steps(). If
 * frames keep arriving slower than step_rate / max_steps_per_frame, the simulation slows down
 * instead of skipping.
 *
 * To draw smooth motion at any refresh rate, a game can blend its previous and current state with
 * get_interpolation_alpha(). The class only supplies the factor: which state is safe to blend is up
 * to the game.
 *
 * Input is delivered per step, not per frame: see step_context. The edges of a frame that runs no
 * step are kept for the next step that runs, and a frame running several steps reports its edges to
 * the first one only. A held key a step consumes stays hidden from later steps until it is pressed
 * again, and a key already held before the first update() reads as up until pressed again.
 *
 * Derived classes implement step(). update() is final, so the stepping logic can't be bypassed.
 *
 * @note Pausing a fixed_step_updatable through a pausable_updatables_view stops it from stepping,
 * and since it doesn't receive the paused frames' deltas, resuming doesn't replay the paused time.
 */
class fixed_step_updatable : public updatable {
public:
    /**
     * @brief The duration of a single step. It must be strictly positive.
     */
    using step_duration =
        core::ranged_value<std::chrono::nanoseconds, 0,
                           std::numeric_limits<std::chrono::nanoseconds::rep>::max(),
                           core::rv_bound_policy::min_open>;

    /**
     * @brief A number of steps, such as the steps run or dropped so far.
     */
    using step_count = core::ranged_value<std::size_t, 0, std::numeric_limits<std::size_t>::max()>;

    /**
     * @brief The maximum number of steps a single frame can run. It must be at least one, since a
     * cap of zero would never step.
     */
    using max_steps_per_frame =
        core::ranged_value<std::size_t, 0, std::numeric_limits<std::size_t>::max(),
                           core::rv_bound_policy::min_open>;

    /**
     * @brief The interpolation factor between the previous and the current step, in [0, 1].
     */
    using alpha_type = core::ranged_value<double, 0.0, 1.0>;

    /**
     * @brief Constructs a fixed_step_updatable with a one-nanosecond step and at most one step per
     * frame.
     */
    fixed_step_updatable() noexcept = default;

    /**
     * @brief Constructs a fixed_step_updatable with the given step duration and cap on the steps
     * run in a single frame.
     *
     * @param stepDuration The duration of a single step, e.g. 20ms for a 50 Hz simulation.
     * @param maxStepsPerFrame The maximum number of steps a single update() can run. The whole
     * steps beyond it are dropped.
     */
    explicit fixed_step_updatable(step_duration stepDuration, max_steps_per_frame maxStepsPerFrame);

    /**
     * @brief Accumulates the context's delta time and calls step() once for each whole step it
     * completes, up to get_max_steps_per_frame() times. The whole steps beyond the cap are dropped
     * and counted by get_dropped_steps(). Only the remainder, less than one step, is carried over
     * to the next update. A negative delta is ignored.
     *
     * A snapshot of the context's input is folded into the pending input handed to the steps
     * (input::merge_into_pending()), even when the delta is negative or no step runs.
     *
     * @param context The context of this update; its delta() is the time elapsed since the last
     * update.
     */
    void update(update_context& context) final;

    /**
     * @brief Returns the number of steps run so far. When called from inside step(), the count
     * includes the step currently running.
     *
     * @return The number of steps run so far.
     */
    step_count get_step_count() const noexcept;

    /**
     * @brief Returns the number of whole steps dropped so far because a frame exceeded
     * get_max_steps_per_frame(). This is useful for diagnostics: a steadily growing count means the
     * frames arrive too slowly for the simulation to keep up.
     *
     * @return The number of steps dropped so far.
     */
    step_count get_dropped_steps() const noexcept;

    /**
     * @brief Returns how far the accumulated time is towards the next step, as a fraction of the
     * step duration. It's 0 right after an exact multiple of the step, and always less than 1 after
     * an update(). A game can use it to draw lerp(previousState, currentState, alpha) for smooth
     * motion at any refresh rate.
     *
     * @return The interpolation factor, in [0, 1).
     */
    alpha_type get_interpolation_alpha() const noexcept;

    /**
     * @brief Returns the maximum number of steps a single update() can run.
     *
     * @return The maximum number of steps per frame.
     */
    max_steps_per_frame get_max_steps_per_frame() const noexcept {
        return m_maxStepPerFrame;
    }

    /**
     * @brief Returns the duration of a single step.
     *
     * @return The step duration.
     */
    step_duration get_step_duration() const noexcept {
        return m_stepDuration;
    }

protected:
    /**
     * @brief Advances the simulation by one fixed step. It's called by update() zero or more times
     * per frame, and always simulates exactly get_step_duration() of time, whatever the frame rate.
     *
     * @param context The input this step reacts to.
     */
    virtual void step(step_context& context) = 0;

private:
    const step_duration m_stepDuration{std::chrono::nanoseconds{1}};
    const max_steps_per_frame m_maxStepPerFrame{1};
    step_count m_stepCount{};
    step_count m_droppedSteps{};
    // Time accumulated towards the next step. Kept in whole nanoseconds rather than as a fraction
    // of a step so that it never drifts: always in [0, m_stepDuration) between updates.
    std::chrono::nanoseconds m_accumulator{};
    // The input the next step reacts to: every frame's input folded in since the last step ran,
    // edges and text cleared once a step has seen them.
    input::input_frame m_pendingInput{};
};

} // namespace tca::engine::rendering
