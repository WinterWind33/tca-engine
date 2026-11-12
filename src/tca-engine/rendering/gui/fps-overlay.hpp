// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <chrono>

#include "core/math/vector.hpp"

#include "rendering/tickables.hpp"

#include "rendering/gui/label.hpp"
#include "rendering/gui/layout-widgets.hpp"

namespace tca::engine::widgets {

/**
 * @brief Simple widget displaying the current frames per second (FPS) and frame time in
 * milliseconds.
 * This widget can be useful for debugging and performance monitoring, as it provides real-time
 * information about the rendering performance of the application.
 *
 * FPS and frame time are displayed in two labels, horizontally aligned and anchored to a corner of
 * the screen, as specified by the constructor parameters.
 */
class fps_overlay : public rendering::renderable_view, public rendering::updatable {
public:
    /**
     * @brief Construct a new fps overlay object with the given anchor position (default=top right)
     * and offset from the edges (default=0,0).
     *
     * The anchor position determines where the overlay will be anchored on the screen, while the
     * offset allows to specify an additional offset from the edges of the screen, which can be
     * useful to avoid overlapping with other UI elements or to position the overlay more precisely.
     *
     * @param pos The anchor position for the overlay. Default is top right.
     * @param offsetFromEdges The offset from the edges of the screen. Default is (0, 0).
     */
    explicit fps_overlay(anchor_position pos = anchor_position::top_right,
                         math::vector2 offsetFromEdges = {});

private:
    anchor_position m_anchorPosition{};
    math::vector2 m_offsetFromEdges{};
    same_line_layout m_layout{};
    label m_fpsLabel{};
    label m_frameTimeLabel{};

    math::vector2 m_position{};
    math::vector2 m_positionPivot{};

    void render() override;
    void update(rendering::update_context& context) override;
};

} // namespace tca::engine::widgets
