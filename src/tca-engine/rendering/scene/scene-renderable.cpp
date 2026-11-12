// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/scene-renderable.hpp"

// C++ STL
#include <utility>

namespace tca::engine::scene {

render_context::render_context(scene_painter& painter) noexcept : m_painter{painter} {}

scene_painter& render_context::painter() const noexcept {
    return m_painter;
}

callback_scene_renderable::callback_scene_renderable(render_callback callback) noexcept
    : m_onRender{std::move(callback)} {}

void callback_scene_renderable::render(render_context& context) {
    if (m_onRender) {
        m_onRender(context);
    }
}

} // namespace tca::engine::scene
