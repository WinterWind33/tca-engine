// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/camera-2d.hpp"

// C++ STL
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tca::engine::scene {

namespace {
// Written so NaN fails too: it compares false against everything.
bool is_valid_viewport_extent(const double extent) noexcept {
    return std::isfinite(extent) && extent >= 0.0;
}
} // namespace

camera_2d::zoom_type camera_2d::clamp_zoom(const double zoom) {
    // std::clamp passes NaN through unchanged, which zoom_type then rejects.
    return zoom_type{std::clamp(zoom, MIN_ZOOM, MAX_ZOOM)};
}

camera_2d::camera_2d(const math::vector2& viewportSize) {
    set_viewport_size(viewportSize);
}

void camera_2d::pan(const math::vector2& offset) noexcept {
    m_position += offset;
}

void camera_2d::set_viewport_size(const math::vector2& viewportSize) {
    if (!is_valid_viewport_extent(viewportSize.x()) ||
        !is_valid_viewport_extent(viewportSize.y())) {
        throw std::invalid_argument{
            "Invalid viewport size: components must be finite and non negative."};
    }

    m_viewportSize = viewportSize;
}

math::vector2 camera_2d::world_to_screen(const math::vector2& world) const noexcept {
    return math::vector2{world_to_screen_transform() * world};
}

math::vector2 camera_2d::screen_to_world(const math::vector2& screen) const noexcept {
    return math::vector2{screen_to_world_transform() * screen};
}

Eigen::Affine2d camera_2d::world_to_screen_transform() const noexcept {
    // screen = zoom * (world - position) + viewport / 2
    return Eigen::Translation2d{m_viewportSize * 0.5} * Eigen::Scaling(m_zoom.get()) *
           Eigen::Translation2d{-m_position};
}

Eigen::Affine2d camera_2d::screen_to_world_transform() const noexcept {
    // world = (screen - viewport / 2) / zoom + position, built directly rather than through a
    // general matrix inverse.
    return Eigen::Translation2d{m_position} * Eigen::Scaling(1.0 / m_zoom.get()) *
           Eigen::Translation2d{-m_viewportSize * 0.5};
}

void camera_2d::zoom_about(const math::vector2& anchor, const zoom_type zoom) noexcept {
    // The world point under the anchor must still be under it after the zoom, so solve
    // zoom * (anchoredWorld - position) + viewport / 2 = anchor for the new position.
    const math::vector2 anchoredWorld{screen_to_world(anchor)};

    m_zoom = zoom;
    m_position = anchoredWorld - (anchor - m_viewportSize * 0.5) / m_zoom.get();
}

world_rect camera_2d::visible_world_rect() const noexcept {
    return world_rect{screen_to_world(math::vector2::zero()), screen_to_world(m_viewportSize)};
}

} // namespace tca::engine::scene
