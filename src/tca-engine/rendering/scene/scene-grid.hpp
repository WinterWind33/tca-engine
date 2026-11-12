// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <vector>

// Engine
#include "rendering/scene/camera-2d.hpp"

namespace tca::engine::scene {

/**
 * @brief The world coordinates of the grid lines a camera shows, in ascending order.
 */
struct grid_lines {
    /**
     * @brief The world x of every vertical line.
     */
    std::vector<double> vertical;

    /**
     * @brief The world y of every horizontal line.
     */
    std::vector<double> horizontal;
};

/**
 * @brief Computes the grid lines, one every spacing world units, that fall within the camera's
 *  visible world rectangle, edges included.
 *
 * @param camera The camera whose visible world rectangle the lines must fall within.
 * @param spacing The distance between two lines, in world units.
 * @return grid_lines The visible lines. Empty if the camera's viewport is empty.
 * @throws std::invalid_argument if spacing is not finite and positive.
 */
grid_lines visible_grid_lines(const camera_2d& camera, double spacing);

} // namespace tca::engine::scene
