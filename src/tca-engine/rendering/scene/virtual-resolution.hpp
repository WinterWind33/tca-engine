// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <optional>
#include <vector>

// Third-party
#include <Eigen/Geometry>

// Engine
#include "core/math/vector.hpp"

namespace tca::engine::scene {

/**
 * @brief An axis-aligned rectangle in screen pixels.
 */
using screen_rect = Eigen::AlignedBox2d;

/**
 * @brief A fixed resolution a pixel-art scene is authored at, such as 320x180, in whole virtual
 *  pixels.
 */
struct virtual_size {
    /**
     * @brief The width, in virtual pixels.
     */
    std::uint32_t width{};

    /**
     * @brief The height, in virtual pixels.
     */
    std::uint32_t height{};

    constexpr bool operator==(const virtual_size&) const noexcept = default;
};

/**
 * @brief Where a virtual resolution lands in an area of the screen: scaled by a whole number,
 *  centred, with letterbox bars filling the rest.
 */
struct pixel_perfect_layout {
    /**
     * @brief The virtual resolution laid out.
     */
    virtual_size resolution{};

    /**
     * @brief How many screen pixels one virtual pixel spans on each axis. At least 1.
     */
    std::uint32_t scale{1};

    /**
     * @brief Where the virtual screen lands, in screen pixels: resolution times scale, with its
     *  corners on whole pixels. Larger than the available area when even a scale of 1 does not
     *  fit, in which case it overflows it evenly on that axis.
     */
    screen_rect content{};

    /**
     * @brief The parts of the available area the content leaves uncovered, as non-empty,
     *  non-overlapping rectangles. Empty when the content covers the whole area.
     */
    std::vector<screen_rect> letterbox{};
};

/**
 * @brief Lays a virtual resolution out in an area of the screen, at the largest whole-number scale
 *  that fits.
 *
 * Only the whole pixels of the available area count: its corners are rounded inwards before the
 * scale is chosen, so the content always starts on a whole pixel. The content is centred in them;
 * when the leftover space is odd, the extra pixel goes to the right or bottom bar.
 *
 * @param available The area to lay it out in, in physical screen pixels. May be empty (zero
 *  sized), e.g. a minimised window.
 * @param resolution The virtual resolution.
 * @return pixel_perfect_layout The layout.
 * @throws std::invalid_argument if the area is not finite, has its minimum past its maximum, or
 *  the resolution is zero on either axis.
 */
pixel_perfect_layout compute_pixel_perfect_layout(const screen_rect& available,
                                                  virtual_size resolution);

/**
 * @brief Maps a point in virtual pixels to the screen.
 */
math::vector2 virtual_to_screen(const pixel_perfect_layout& layout,
                                const math::vector2& virtualPoint) noexcept;

/**
 * @brief Maps a screen point to virtual pixels, e.g. the pointer.
 *
 * @return std::optional<math::vector2> The virtual point, not rounded, or nullopt if the screen
 *  point lies outside the content: in a letterbox bar or past the area altogether. The content's
 *  right and bottom edges are outside it.
 */
std::optional<math::vector2> screen_to_virtual(const pixel_perfect_layout& layout,
                                               const math::vector2& screenPoint) noexcept;

} // namespace tca::engine::scene
