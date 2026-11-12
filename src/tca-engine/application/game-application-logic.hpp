// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <chrono>

#include "application/application.hpp"
#include "rendering/tickables.hpp"

namespace tca::engine::application {

/**
 * @brief Application logic for games: it drives the game's objects through two separate passes
 * per frame. Every tick() first calls update() on every registered updatable, then render() on
 * every registered renderable, so no object is ever drawn before the whole game state for that
 * frame has been updated.
 *
 * Objects are registered through get_updatables() and get_renderables(). Both hold references only:
 * the game owns its objects and must keep each one alive for as long as it stays registered. An
 * object that both updates and renders is registered in both. A widget nested inside another widget
 * is rendered by its parent, so only the top-level widgets go into get_renderables(), while an
 * updatable anywhere in the tree is registered in get_updatables() itself.
 *
 * Logic that belongs to the game as a whole, rather than to one object, is written as an updatable
 * and registered like any other; the class deriving from this one may itself be that updatable.
 */
class game_application_logic : public application_logic_base {
public:
    /**
     * @brief Type of the duration used to represent the time elapsed between two frames, i.e., the
     * delta time passed to every updatable.
     */
    using duration = rendering::updatable::delta_time;

protected:
    /**
     * @brief The objects updated at each frame, in the order they were added, all with the same
     * update_context. Its delta time is 0 on the first frame and the time elapsed since the
     * previous frame afterwards. Updatables can be paused and resumed through this view.
     *
     * @return rendering::pausable_updatables_view& The view of the registered updatables.
     */
    rendering::pausable_updatables_view& get_updatables() noexcept {
        return m_updatables;
    }

    /**
     * @brief The objects rendered at each frame, in the order they were added, once every
     * updatable has been updated. Renderables can be paused (not drawn) and resumed through this
     * view.
     *
     * @return rendering::pausable_renderables_view& The view of the registered renderables.
     */
    rendering::pausable_renderables_view& get_renderables() noexcept {
        return m_renderables;
    }

private:
    /**
     * @brief Type of the time point used to save the last update time.
     */
    using time_point = std::chrono::steady_clock::time_point;
    bool tick() final;

    rendering::pausable_updatables_view m_updatables{};
    rendering::pausable_renderables_view m_renderables{};
    time_point m_lastUpdateTime{};
};

} // namespace tca::engine::application
