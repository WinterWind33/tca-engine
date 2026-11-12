// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/virtual-resolution.hpp"

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tca::engine::scene {

namespace {

// The largest whole number of times `extent` fits in `usable`, at least 1.
double fitting_scale(const double usable, const std::uint32_t extent) noexcept {
    return std::max(1.0, std::floor(usable / static_cast<double>(extent)));
}

// The parts of `available` the content leaves uncovered: full-width bands above and below what is
// visible of it, and the bands left and right of it in between.
std::vector<screen_rect> letterbox_bars(const screen_rect& available, const screen_rect& content) {
    const Eigen::Vector2d low{content.min().cwiseMax(available.min())};
    const Eigen::Vector2d high{content.max().cwiseMin(available.max())};

    const std::array<screen_rect, 4> candidates{
        screen_rect{available.min(),                                Eigen::Vector2d{available.max().x(), low.y()}},
        screen_rect{Eigen::Vector2d{available.min().x(), high.y()}, available.max()                              },
        screen_rect{Eigen::Vector2d{available.min().x(), low.y()},
                    Eigen::Vector2d{low.x(), high.y()}                                                           },
        screen_rect{Eigen::Vector2d{high.x(), low.y()},
                    Eigen::Vector2d{available.max().x(), high.y()}                                               },
    };

    std::vector<screen_rect> bars{};
    for (const screen_rect& bar : candidates) {
        if ((bar.sizes().array() > 0.0).all()) {
            bars.push_back(bar);
        }
    }
    return bars;
}

} // namespace

pixel_perfect_layout compute_pixel_perfect_layout(const screen_rect& available,
                                                  const virtual_size resolution) {
    if (resolution.width == 0 || resolution.height == 0) {
        throw std::invalid_argument{"The virtual resolution must be at least one pixel each way."};
    }
    if (!available.min().allFinite() || !available.max().allFinite()) {
        throw std::invalid_argument{"The available area must be finite."};
    }
    if ((available.min().array() > available.max().array()).any()) {
        throw std::invalid_argument{
            "The available area must not have its minimum past its maximum."};
    }

    // Only the whole pixels count, so the content always starts on one.
    const Eigen::Vector2d usableMin{available.min().array().ceil().matrix()};
    const Eigen::Vector2d usableMax{available.max().array().floor().matrix().cwiseMax(usableMin)};
    const Eigen::Vector2d usableSize{usableMax - usableMin};

    const double scale{std::min({fitting_scale(usableSize.x(), resolution.width),
                                 fitting_scale(usableSize.y(), resolution.height),
                                 static_cast<double>(std::numeric_limits<std::uint32_t>::max())})};
    const Eigen::Vector2d contentSize{static_cast<double>(resolution.width) * scale,
                                      static_cast<double>(resolution.height) * scale};

    // Rounding the half down leaves the odd pixel, if any, to the right and bottom.
    const Eigen::Vector2d leftover{usableSize - contentSize};
    const Eigen::Vector2d contentMin{usableMin + (leftover / 2.0).array().floor().matrix()};
    const screen_rect content{contentMin, contentMin + contentSize};

    return pixel_perfect_layout{.resolution = resolution,
                                .scale = static_cast<std::uint32_t>(scale),
                                .content = content,
                                .letterbox = letterbox_bars(available, content)};
}

math::vector2 virtual_to_screen(const pixel_perfect_layout& layout,
                                const math::vector2& virtualPoint) noexcept {
    return math::vector2{layout.content.min() + virtualPoint * static_cast<double>(layout.scale)};
}

std::optional<math::vector2> screen_to_virtual(const pixel_perfect_layout& layout,
                                               const math::vector2& screenPoint) noexcept {
    // Written so a NaN fails every comparison and lands outside.
    const bool inside{(screenPoint.array() >= layout.content.min().array()).all() &&
                      (screenPoint.array() < layout.content.max().array()).all()};
    if (!inside) {
        return std::nullopt;
    }
    return math::vector2{(screenPoint - layout.content.min()) / static_cast<double>(layout.scale)};
}

} // namespace tca::engine::scene
