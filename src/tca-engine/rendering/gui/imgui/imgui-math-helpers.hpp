// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Third-party
#include <imgui.h>

#include "core/math/vector.hpp"

namespace tca::engine::imgui {

/**
 * @brief Converts a math::vector2 to an ImVec2.
 *
 * @param vec The math::vector2 to convert.
 * @return ImVec2 The converted ImVec2.
 */
ImVec2 to_imvec2(const math::vector2 vec) noexcept(noexcept(ImVec2{1.0f, 1.0f}) &&
                                                   noexcept(math::vector2{}.x()) &&
                                                   noexcept(math::vector2{}.y()));

/**
 * @brief Converts an ImVec2 to a math::vector2.
 *
 * @param vec The ImVec2 to convert.
 * @return math::vector2 The converted math::vector2.
 */
math::vector2 to_vector2(const ImVec2 vec) noexcept(noexcept(math::vector2{1.0, 1.0}));
} // namespace tca::engine::imgui
