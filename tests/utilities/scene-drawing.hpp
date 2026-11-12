// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <functional>
#include <utility>

// Engine
#include "rendering/scene/scene-renderable.hpp"
#include "rendering/scene/scene-surface.hpp"

namespace tca::tests::utilities {

/**
 * @brief Adapts a callback taking the painter into a callback_scene_renderable's callback, for
 * tests that only draw and never look at the rest of the render_context.
 */
inline engine::scene::callback_scene_renderable::render_callback painting(
    std::function<void(engine::scene::scene_painter&)> draw) {
    if (!draw) {
        return {};
    }

    return [draw = std::move(draw)](engine::scene::render_context& context) {
        draw(context.painter());
    };
}

/**
 * @brief Draws into a surface with a callback for as long as it is alive: it adds a
 *  callback_scene_renderable to the surface when built and removes it when destroyed, so a test
 *  can draw with a lambda without managing a renderable's lifetime itself.
 */
class scoped_scene_drawing final {
public:
    scoped_scene_drawing(engine::scene::scene_surface& surface,
                         std::function<void(engine::scene::scene_painter&)> draw)
        : m_surface{surface},
          m_renderable{painting(std::move(draw))} {
        m_surface.add_renderable(m_renderable);
    }

    ~scoped_scene_drawing() noexcept {
        m_surface.remove_renderable(m_renderable);
    }

    scoped_scene_drawing(const scoped_scene_drawing&) = delete;
    scoped_scene_drawing& operator=(const scoped_scene_drawing&) = delete;
    scoped_scene_drawing(scoped_scene_drawing&&) = delete;
    scoped_scene_drawing& operator=(scoped_scene_drawing&&) = delete;

private:
    engine::scene::scene_surface& m_surface;
    engine::scene::callback_scene_renderable m_renderable;
};

} // namespace tca::tests::utilities
