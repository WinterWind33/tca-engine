// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/imgui/imgui-math-helpers.hpp"

// Third-party
#include <gsl/gsl>

namespace tca::engine::imgui {

ImVec2 to_imvec2(const math::vector2 vec) noexcept(noexcept(ImVec2{1.0f, 1.0f}) &&
                                                   noexcept(math::vector2{}.x()) &&
                                                   noexcept(math::vector2{}.y())) {
    return ImVec2{gsl::narrow_cast<float>(vec.x()), gsl::narrow_cast<float>(vec.y())};
}

math::vector2 to_vector2(const ImVec2 vec) noexcept(noexcept(math::vector2{1.0, 1.0})) {
    return math::vector2{vec.x, vec.y};
}

} // namespace tca::engine::imgui
