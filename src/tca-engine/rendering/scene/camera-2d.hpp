// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Third-party
#include <Eigen/Geometry>

// Engine
#include "core/math/vector.hpp"
#include "core/utilities/ranged-value.hpp"

namespace tca::engine::scene {

/**
 * @brief An axis-aligned rectangle in world units, as returned by camera_2d::visible_world_rect().
 */
using world_rect = Eigen::AlignedBox2d;

/**
 * @brief A 2D camera: maps world coordinates to screen (viewport) coordinates and back.
 *
 * Pure data and maths, with no renderer behind it, so it can be created and tested before any
 * surface exists.
 *
 * Conventions:
 * - Screen coordinates are pixels from the viewport's top-left corner, x to the right and y down.
 *   World coordinates follow the same orientation: one world unit down is zoom pixels down.
 * - The camera position is the world point shown at the viewport centre.
 * - The zoom is the number of pixels one world unit spans, limited to [MIN_ZOOM, MAX_ZOOM].
 *
 * @note No rotation: the transform is a uniform scale and a translation only.
 */
class camera_2d {
public:
    /**
     * @brief The zoom, in pixels per world unit. Constructing one outside [MIN_ZOOM, MAX_ZOOM]
     *  (or from NaN) throws core::ranged_value_exception: use clamp_zoom() to bring an arbitrary
     *  value, such as the current zoom times a scroll step, into range first.
     */
    using zoom_type = core::ranged_value<double, 1.0 / 16.0, 16.0>;

    constexpr static zoom_type::value_type MAX_ZOOM{zoom_type::MAX()};
    constexpr static zoom_type::value_type MIN_ZOOM{zoom_type::MIN()};

    /**
     * @brief Brings a zoom into [MIN_ZOOM, MAX_ZOOM], clamping it to the nearest bound.
     *
     * @param zoom The zoom to clamp.
     * @return zoom_type The clamped zoom.
     * @throws core::ranged_value_exception if zoom is NaN, which has no nearest bound.
     */
    static zoom_type clamp_zoom(double zoom);

    /**
     * @brief Constructs an unzoomed camera at the world origin, with an empty viewport: a camera
     *  can be created before the surface it draws to knows its size.
     */
    camera_2d() noexcept = default;

    /**
     * @brief Constructs an unzoomed camera at the world origin.
     *
     * @param viewportSize The viewport size in pixels.
     * @throws std::invalid_argument if either component is negative or not finite.
     */
    explicit camera_2d(const math::vector2& viewportSize);

    /**
     * @brief The world point shown at the viewport centre.
     */
    const math::vector2& position() const noexcept {
        return m_position;
    }

    /**
     * @brief Moves the camera so the given world point is shown at the viewport centre.
     */
    void set_position(const math::vector2& position) noexcept {
        m_position = position;
    }

    /**
     * @brief Moves the camera by a world-space offset, whatever the zoom.
     */
    void pan(const math::vector2& offset) noexcept;

    /**
     * @brief The viewport size in pixels.
     */
    const math::vector2& viewport_size() const noexcept {
        return m_viewportSize;
    }

    /**
     * @brief Replaces the viewport size. The position and zoom are kept, so the position stays at
     *  the (new) viewport centre and a bigger viewport shows more world at the same scale.
     *
     * @param viewportSize The viewport size in pixels. Zero is allowed.
     * @throws std::invalid_argument if either component is negative or not finite, leaving the
     *  camera untouched.
     */
    void set_viewport_size(const math::vector2& viewportSize);

    /**
     * @brief The zoom, in pixels per world unit.
     */
    zoom_type zoom() const noexcept {
        return m_zoom;
    }

    /**
     * @brief Sets the zoom, keeping the position (so zooming happens about the viewport centre).
     *  See zoom_about() to zoom about any other point.
     */
    void set_zoom(const zoom_type zoom) noexcept {
        m_zoom = zoom;
    }

    /**
     * @brief Sets the zoom, moving the camera so the world point under the given screen point
     *  stays under it, e.g. to zoom at the cursor.
     *
     * @param anchor The screen point to zoom about. It may lie outside the viewport.
     * @param zoom The new zoom.
     */
    void zoom_about(const math::vector2& anchor, zoom_type zoom) noexcept;

    /**
     * @brief Maps a world point to the screen.
     */
    math::vector2 world_to_screen(const math::vector2& world) const noexcept;

    /**
     * @brief Maps a screen point to the world. The exact inverse of world_to_screen().
     */
    math::vector2 screen_to_world(const math::vector2& screen) const noexcept;

    /**
     * @brief The world-to-screen transform as an Eigen affine transform, for callers mapping many
     *  points (or composing it with transforms of their own).
     */
    Eigen::Affine2d world_to_screen_transform() const noexcept;

    /**
     * @brief The screen-to-world transform, the inverse of world_to_screen_transform().
     */
    Eigen::Affine2d screen_to_world_transform() const noexcept;

    /**
     * @brief The world rectangle the viewport shows, for culling: exactly the world points that
     *  world_to_screen() maps into [0, viewport_size()]. It collapses to the position when the
     *  viewport is empty.
     */
    world_rect visible_world_rect() const noexcept;

private:
    math::vector2 m_position{};
    math::vector2 m_viewportSize{};
    zoom_type m_zoom{1.0};
};

} // namespace tca::engine::scene
