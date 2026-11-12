// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Engine
#include "input/input-frame.hpp"
#include "rendering/tickables.hpp"

namespace tca::tests::utilities {

/**
 * @brief Updates the given updatable once with the given delta time and an empty input frame, for
 * tests that only care about time.
 */
inline void update_with_delta(engine::rendering::updatable& target,
                              const engine::rendering::update_context::delta_time delta) {
    engine::input::input_frame frame{};
    engine::rendering::update_context context{delta, engine::input::input_view{frame}};
    target.update(context);
}

} // namespace tca::tests::utilities
