// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <functional>

#include "rendering/gui/widget.hpp"
#include "rendering/tickables.hpp"

namespace tca::engine::widgets {

/**
 * @brief Adapts a simple std::function<void()> callback into a renderable, so callers that just
 * need to draw some immediate-mode content aren't forced to define their own renderable class for
 * it.
 */
class callback_renderable final : public rendering::renderable {
public:
    using render_callback = std::function<void()>;

    explicit callback_renderable(render_callback callback) noexcept;

    void render() override;

private:
    render_callback m_onRender{};
};

static_assert(widget<callback_renderable>);

} // namespace tca::engine::widgets
