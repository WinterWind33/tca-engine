// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "web/gui/web-view.hpp"

// Third-party
#include <imgui.h>
#include <gsl/gsl>

#include "rendering/colors.hpp"

namespace tca::engine::web::gui {

namespace {

constexpr rendering::color PLACEHOLDER_RECT_COLOR{rendering::colors::SLATE_GRAY};

} // namespace

void web_view::render() {
    // Stage 0 stub: no document has been parsed yet, so we just draw a placeholder rectangle
    // filling the available content region of the current ImGui window. This will be replaced by
    // the actual display list painter once the rendering pipeline exists.
    const ImVec2 topLeft{ImGui::GetCursorScreenPos()};
    const ImVec2 availableSize{ImGui::GetContentRegionAvail()};
    const ImVec2 bottomRight{topLeft.x + availableSize.x, topLeft.y + availableSize.y};

    const gsl::not_null<ImDrawList*> drawList{ImGui::GetWindowDrawList()};
    drawList->AddRectFilled(topLeft, bottomRight,
                            ImGui::ColorConvertFloat4ToU32(ImVec4{
                                rendering::normalize_color_component(PLACEHOLDER_RECT_COLOR.r),
                                rendering::normalize_color_component(PLACEHOLDER_RECT_COLOR.g),
                                rendering::normalize_color_component(PLACEHOLDER_RECT_COLOR.b),
                                rendering::normalize_color_component(PLACEHOLDER_RECT_COLOR.a)}));

    ImGui::Dummy(availableSize);
}

} // namespace tca::engine::web::gui
