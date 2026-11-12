// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>

// Engine
#include "core/utilities/ranged-value.hpp"
#include "rendering/colors.hpp"

namespace tca::engine::rendering {

/**
 * @brief A colour component already normalized to [0.0, 1.0]. Ranged rather than a bare double so
 * that a derivation that overshoots (brightening an accent past white, say) is rejected where it
 * is computed instead of quietly producing a colour nobody intended.
 */
using normalized_component = core::ranged_value<double, 0.0, 1.0>;

/**
 * @brief A colour expressed in the hue/saturation/value cylindrical model, which is the useful
 * one when a palette has to be *derived* from a colour: brightening a button on hover, or keeping
 * a hue while forcing a fixed lightness, are single-component edits here and messy ones in RGB.
 */
struct hsv_color final {
    /**
     * @brief Hue, where 0.0 and 1.0 are both red.
     */
    normalized_component hue{};

    /**
     * @brief Saturation. 0.0 is a shade of grey.
     */
    normalized_component saturation{};

    /**
     * @brief Value (brightness). 0.0 is black.
     */
    normalized_component value{};

    /**
     * @brief Alpha, in the range [0, 255], carried through the conversions unchanged.
     */
    std::uint8_t alpha{255};

    constexpr bool operator==(const hsv_color& other) const noexcept = default;
};

/**
 * @brief Converts an 8-bit RGBA colour to its hue/saturation/value representation.
 *
 * @param sourceColor The colour to convert. Its alpha is carried over untouched.
 * @return hsv_color The same colour expressed in HSV.
 */
hsv_color to_hsv(const color& sourceColor) noexcept;

/**
 * @brief Converts a hue/saturation/value colour back to an 8-bit RGBA colour.
 *
 * @param sourceColor The colour to convert. Its alpha is carried over untouched.
 * @return color The same colour expressed as 8-bit RGBA.
 */
color from_hsv(const hsv_color& sourceColor) noexcept;

} // namespace tca::engine::rendering
