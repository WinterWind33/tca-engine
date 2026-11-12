// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/custom-tickables/fixed-step-tickables.hpp"

// C++ STL
#include <chrono>

// Engine
#include "input/input-frame.hpp"

namespace tca::engine::rendering {

// Step context implementation

step_context::step_context(const input::input_view input) noexcept : m_input{input} {}

auto step_context::input() const noexcept -> input::input_view {
    return m_input;
}

// Fixed step updatable implementation

fixed_step_updatable::fixed_step_updatable(const step_duration duration,
                                           const max_steps_per_frame maxStepsPerFrame)
    : m_stepDuration{duration},
      m_maxStepPerFrame{maxStepsPerFrame} {}

void fixed_step_updatable::update(update_context& context) {
    // Fold the input in first, so a frame whose delta is ignored doesn't lose its edges.
    m_pendingInput = input::merge_into_pending(m_pendingInput, context.input().snapshot());

    const delta_time delta{context.delta()};
    if (delta.count() < 0) {
        return;
    }

    const std::chrono::nanoseconds stepDuration{m_stepDuration.get()};
    m_accumulator += std::chrono::duration_cast<std::chrono::nanoseconds>(delta);

    for (step_count tempCounter{}; tempCounter < m_maxStepPerFrame && m_accumulator >= stepDuration;
         ++tempCounter) {
        m_accumulator -= stepDuration;
        ++m_stepCount;

        // And call the step() method since a new step has happened.
        step_context stepContext{input::input_view{m_pendingInput}};
        step(stepContext);

        // The edges and text were this step's: later steps only see the held keys.
        m_pendingInput.end_tick();
    }

    // Whole steps still left couldn't run this frame: drop them and keep only the remainder.
    m_droppedSteps += static_cast<step_count::value_type>(m_accumulator / stepDuration);
    m_accumulator %= stepDuration;
}

auto fixed_step_updatable::get_step_count() const noexcept -> step_count {
    return m_stepCount;
}

auto fixed_step_updatable::get_dropped_steps() const noexcept -> step_count {
    return m_droppedSteps;
}

auto fixed_step_updatable::get_interpolation_alpha() const noexcept -> alpha_type {
    // The interpolation alpha specifies how much we are close to one step done:
    // 0.0 means no steps, 0.5 means we're halfway there. It can't reach 1.0, since the
    // accumulator is always less than one step.
    return static_cast<alpha_type::value_type>(m_accumulator.count()) /
           static_cast<alpha_type::value_type>(m_stepDuration.get().count());
}

} // namespace tca::engine::rendering
