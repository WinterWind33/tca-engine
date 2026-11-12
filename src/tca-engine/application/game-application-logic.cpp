// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/game-application-logic.hpp"

namespace tca::engine::application {

bool game_application_logic::tick() {
    const auto now{std::chrono::steady_clock::now()};

    // On the first tick there is no previous frame to measure from, so the delta time passed to
    // the updatables is 0.
    const bool isFirstTick{m_lastUpdateTime.time_since_epoch().count() == 0};
    const duration deltaTime{isFirstTick
                                 ? duration::zero()
                                 : std::chrono::duration_cast<duration>(now - m_lastUpdateTime)};
    m_lastUpdateTime = now;

    // The whole update pass completes before the render pass starts, so every renderable draws
    // the game state of this frame rather than a mix of this frame's and the previous one's.
    rendering::update_context context{deltaTime, get_input()};
    m_updatables.update(context);
    m_renderables.render();

    return true;
}

} // namespace tca::engine::application
