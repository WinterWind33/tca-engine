// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/callback-renderable.hpp"

// C++ STL
#include <utility>

namespace tca::engine::widgets {
callback_renderable::callback_renderable(render_callback callback) noexcept
    : m_onRender{std::move(callback)} {}

void callback_renderable::render() {
    if (m_onRender) {
        m_onRender();
    }
}

} // namespace tca::engine::widgets
