// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/color-conversions.hpp"

// C++ STL
#include <algorithm>
#include <cmath>
#include <utility>

namespace tca::engine::rendering {

namespace {

// Guards the two divisions below against a division by zero for pure black (value 0) and for a
// grey (chroma 0), without needing a branch for either. Small enough not to move any 8-bit
// component by one step.
constexpr double DIVISION_GUARD{1e-20};

// Number of hue sectors the [0.0, 1.0] hue range is split into when going back to RGB.
constexpr double HUE_SECTORS{6.0};

constexpr double to_normalized(const std::uint8_t component) noexcept {
    return static_cast<double>(component) / 255.0;
}

constexpr std::uint8_t to_component(const double normalized) noexcept {
    // + 0.5 so the conversion rounds instead of truncating: without it a round trip through HSV
    // moves most components down by one step.
    return static_cast<std::uint8_t>(std::clamp(normalized, 0.0, 1.0) * 255.0 + 0.5);
}

// The arithmetic below is exact in theory but not in floating point, so results are pinned to
// [0.0, 1.0] before they reach a normalized_component, which would otherwise throw on a value
// that overshot by an ulp.
constexpr normalized_component to_normalized_component(const double value) noexcept {
    return normalized_component{std::clamp(value, 0.0, 1.0)};
}

} // namespace

hsv_color to_hsv(const color& sourceColor) noexcept {
    double red{to_normalized(sourceColor.r)};
    double green{to_normalized(sourceColor.g)};
    double blue{to_normalized(sourceColor.b)};

    // Sorts the three components so that red ends up holding the largest one, tracking in
    // hueOffset which sector of the colour wheel that reordering corresponds to. This is the
    // branch-light formulation Dear ImGui uses, kept identical here so that a palette derived from
    // an accent colour comes out exactly as it did when this derivation lived in the ImGui theme.
    double hueOffset{0.0};
    if (green < blue) {
        std::swap(green, blue);
        hueOffset = -1.0;
    }
    if (red < green) {
        std::swap(red, green);
        hueOffset = -2.0 / HUE_SECTORS - hueOffset;
    }

    const double chroma{red - std::min(green, blue)};

    return hsv_color{
        .hue = to_normalized_component(
            std::abs(hueOffset + (green - blue) / (HUE_SECTORS * chroma + DIVISION_GUARD))),
        .saturation = to_normalized_component(chroma / (red + DIVISION_GUARD)),
        .value = to_normalized_component(red),
        .alpha = sourceColor.a,
    };
}

color from_hsv(const hsv_color& sourceColor) noexcept {
    const double saturation{sourceColor.saturation};
    const double value{sourceColor.value};

    if (saturation <= 0.0) {
        // A fully desaturated colour is a grey, and the hue carries no information at that point,
        // so the sector arithmetic below would only introduce rounding noise.
        const std::uint8_t grey{to_component(value)};
        return color{grey, grey, grey, sourceColor.alpha};
    }

    const double hue{static_cast<double>(sourceColor.hue) * HUE_SECTORS};
    const double sector{std::floor(hue)};
    const double sectorFraction{hue - sector};

    // A hue of exactly 1.0 lands on a seventh sector that is really the first one again (1.0 is
    // the same red as 0.0), so the sector index wraps rather than falling through to the last
    // branch below.
    const int sectorIndex{static_cast<int>(sector) % static_cast<int>(HUE_SECTORS)};

    const double descending{value * (1.0 - saturation)};
    const double falling{value * (1.0 - saturation * sectorFraction)};
    const double rising{value * (1.0 - saturation * (1.0 - sectorFraction))};

    switch (sectorIndex) {
        case 0:
            return color{to_component(value), to_component(rising), to_component(descending),
                         sourceColor.alpha};
        case 1:
            return color{to_component(falling), to_component(value), to_component(descending),
                         sourceColor.alpha};
        case 2:
            return color{to_component(descending), to_component(value), to_component(rising),
                         sourceColor.alpha};
        case 3:
            return color{to_component(descending), to_component(falling), to_component(value),
                         sourceColor.alpha};
        case 4:
            return color{to_component(rising), to_component(descending), to_component(value),
                         sourceColor.alpha};
        default:
            // Sector 5, the last one.
            return color{to_component(value), to_component(descending), to_component(falling),
                         sourceColor.alpha};
    }
}

} // namespace tca::engine::rendering
