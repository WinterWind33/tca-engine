// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/imgui/imgui-color-helpers.hpp"

// Engine
#include "rendering/theming/active-theme.hpp"

namespace tca::engine::imgui {

ImVec4 to_imvec4(const rendering::color& sourceColor) noexcept {
    const auto normalized{rendering::to_normalized_float4(sourceColor)};
    return ImVec4{normalized[0], normalized[1], normalized[2], normalized[3]};
}

scoped_style_color::scoped_style_color(const ImGuiCol target, const theming::color_role role)
    : scoped_style_color{target, theming::color_for(role)} {}

scoped_style_color::scoped_style_color(const ImGuiCol target,
                                       const rendering::color& explicitColor) {
    ImGui::PushStyleColor(target, to_imvec4(explicitColor));
}

scoped_style_color::~scoped_style_color() {
    ImGui::PopStyleColor();
}

} // namespace tca::engine::imgui
