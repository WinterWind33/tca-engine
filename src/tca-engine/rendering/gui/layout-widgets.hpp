// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <optional>
#include <string>

#include "core/math/vector.hpp"
#include "rendering/gui/widget.hpp"
#include "rendering/theming/theme-roles.hpp"
#include "rendering/tickables.hpp"

namespace tca::engine::widgets {

/**
 * @brief Enum class representing the possible anchor positions for a widget.
 *
 * This enum is used to specify where a widget should be anchored on the screen (or the parent,
 * depending on the implementation), which can be useful for positioning widgets in a consistent
 * manner regardless of the screen size or resolution. The anchor position determines how the
 * widget's position is calculated relative to the edges of the screen or parent container.
 */
enum class anchor_position {
    top_left,
    top_center,
    top_right,
    center_left,
    center,
    center_right,
    bottom_left,
    bottom_center,
    bottom_right,
};

/**
 * @brief Computes the position of a widget based on the specified anchor position and the size of
 * the parent container. The returned position is relative to the top-left corner of the parent
 * container.
 *
 * @note Throws invalid_argument if the parent size is zero or negative, or if the anchor position
 * is invalid.
 *
 * @param anchorPos The anchor position for the widget.
 * @param parentSize The size of the parent container.
 * @return math::vector2 The computed position of the widget.
 */
math::vector2 compute_anchor_position(anchor_position anchorPos, const math::vector2& parentSize);

/**
 * @brief Computes the pivot point of a widget based on the specified anchor position. The pivot
 * point is used to determine how the widget is positioned relative to its anchor point. For
 * example, if the anchor position is top-left, the pivot point will be (0, 0), meaning that
 * the widget will be positioned such that its top-left corner is at the anchor point.
 *
 * Values are in the range [0, 1], where (0, 0) corresponds to the top-left corner of the widget,
 * and (1, 1) corresponds to the bottom-right corner of the widget.
 *
 * @note Throws invalid_argument if the anchor position is invalid.
 *
 * @param anchorPos The anchor position for the widget.
 * @return math::vector2 The computed pivot point of the widget.
 */
math::vector2 compute_anchor_pivot(anchor_position anchorPos);

/**
 * @brief A layout widget that arranges its child widgets in a single horizontal line.
 */
class same_line_layout : public rendering::renderable_view {
private:
    void render() final;
};

/**
 * @brief Widget representing the entire viewport, which can be used as a parent for other widgets
 * to ensure that they are always positioned relative to the viewport, regardless of the screen size
 * or resolution.
 *
 * Its backdrop is named by a theme role rather than by a colour, so an application that restyles
 * itself restyles this too, with nothing to update here.
 */
class viewport_layout_widget : public rendering::pausable_renderables_view,
                               public rendering::pausable_updatables_view {
public:
    using id_t = std::string;
    using id_view_t = std::string_view;

    /**
     * @brief Construct a new viewport layout widget object with the given ID.
     *
     * @param id The widget's ID, unique among the ImGui windows of the application.
     * @param backgroundRole The theme colour role painting the viewport's backdrop, normally
     * theming::color_role::window_background. std::nullopt (the default) paints no backdrop at
     * all, leaving whatever is behind it visible.
     */
    explicit viewport_layout_widget(
        id_view_t id, std::optional<theming::color_role> backgroundRole = std::nullopt) noexcept;

private:
    void render() final;

    id_t m_id{};
    std::optional<theming::color_role> m_backgroundRole{};
};

} // namespace tca::engine::widgets
