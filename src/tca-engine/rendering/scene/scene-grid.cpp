// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/scene-grid.hpp"

// C++ STL
#include <cmath>
#include <stdexcept>
#include <vector>

#include "core/math/math.hpp"
#include "rendering/scene/camera-2d.hpp"

namespace tca::engine::scene {
namespace {

// Every line sits on a whole multiple of the spacing, so the lines within [minimum, maximum] are
// the multiples k * spacing with k in [ceil(minimum / spacing), floor(maximum / spacing)]. The
// indices get a small tolerance so an edge sitting exactly on a multiple is not lost to rounding,
// and each line is computed from its index rather than accumulated, so no error builds up.
std::vector<double> lines_within(const double minimum, const double maximum, const double spacing) {
    constexpr double INDEX_TOLERANCE{1e-9};

    const double first{std::ceil(minimum / spacing - INDEX_TOLERANCE)};
    const double last{std::floor(maximum / spacing + INDEX_TOLERANCE)};

    std::vector<double> lines{};
    for (double index{first}; index <= last; index += 1.0) {
        lines.push_back(index * spacing);
    }
    return lines;
}

} // namespace

grid_lines visible_grid_lines(const camera_2d& camera, const double spacing) {
    if (!std::isfinite(spacing) || math::is_nearly_zero(spacing) || spacing < 0.0) {
        throw std::invalid_argument{"Spacing must have a non-zero positive value."};
    }

    const world_rect worldRect{camera.visible_world_rect()};
    if (math::is_nearly_zero(worldRect.sizes().x()) ||
        math::is_nearly_zero(worldRect.sizes().y())) {
        return {};
    }

    return grid_lines{
        .vertical = lines_within(worldRect.min().x(), worldRect.max().x(), spacing),
        .horizontal = lines_within(worldRect.min().y(), worldRect.max().y(), spacing),
    };
}

} // namespace tca::engine::scene
