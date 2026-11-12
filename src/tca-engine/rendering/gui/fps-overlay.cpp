// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/fps-overlay.hpp"

// C++ STL
#include <format>
#include <string_view>

// Third-party
#include <imgui.h>
#include <gsl/gsl>

#include "rendering/gui/imgui/imgui-math-helpers.hpp"
#include "rendering/theming/theme-roles.hpp"

namespace tca::engine::widgets {

namespace {

// Using double here because using std::chrono::duration would be semantically wrong, since ImGui
// doesn't use std::chrono::duration for its framerate and frame time values, but rather uses a
// simple scalar value.
using fps_t = double;
using frame_time_t = std::chrono::duration<double, std::milli>;

constexpr ImGuiWindowFlags WINDOW_FLAGS{
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
    ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove};
constexpr std::string_view WINDOW_TITLE{"FPSOverlay"};
constexpr std::string_view FPS_TEXT_FORMAT{"FPS: {:.1f}"};
constexpr std::string_view FRAME_TIME_TEXT_FORMAT{"Frame Time: {:.2f} ms"};

constexpr std::string_view FPS_NOT_AVAILABLE_TEXT{"FPS: N/A"};
constexpr std::string_view FRAME_TIME_NOT_AVAILABLE_TEXT{"Frame Time: N/A"};

// A diagnostics overlay is secondary information laid over the game, so it takes the theme's
// small, quiet text rather than a size and a grey of its own.
constexpr theming::typography_role LABEL_TYPOGRAPHY_ROLE{theming::typography_role::caption};
constexpr theming::color_role LABEL_COLOR_ROLE{theming::color_role::on_surface_muted};
} // namespace

fps_overlay::fps_overlay(const anchor_position pos, const math::vector2 offsetFromEdges)
    : m_anchorPosition{pos},
      m_offsetFromEdges{offsetFromEdges},
      m_fpsLabel{FPS_NOT_AVAILABLE_TEXT, LABEL_TYPOGRAPHY_ROLE, LABEL_COLOR_ROLE},
      m_frameTimeLabel{FRAME_TIME_NOT_AVAILABLE_TEXT, LABEL_TYPOGRAPHY_ROLE, LABEL_COLOR_ROLE} {
    if (offsetFromEdges.x() < 0 || offsetFromEdges.y() < 0) {
        throw std::invalid_argument{"Offset from edges must be non-negative"};
    }

    m_layout.add_renderable(m_fpsLabel);
    m_layout.add_renderable(m_frameTimeLabel);
    add_renderable(m_layout);
}

void fps_overlay::update(rendering::update_context& /*unused context*/) {
    const gsl::not_null<const ImGuiViewport*> viewport{ImGui::GetMainViewport()};
    const ImVec2 workPos{viewport->WorkPos};
    const ImVec2 workSize{viewport->WorkSize};

    m_positionPivot = compute_anchor_pivot(m_anchorPosition);
    const math::vector2 anchorPos{
        compute_anchor_position(m_anchorPosition, math::vector2{workSize.x, workSize.y})};

    // Before computing the position in window-coordinates, we need to adjust the offsets from edges
    // based on the anchor position, so that the offsets are applied in the correct direction.
    // We do this using the anchor position pivot, which indicates the direction in which the
    // offsets should be applied.
    // For example:
    // - Top left: anchor pivot = (0, 0), offsets are applied as is.
    // - Top right: anchor pivot = (1, 0), offsets should be (-offset.x, offset.y).
    // - Bottom left: anchor pivot = (0, 1), offsets should be (offset.x, -offset.y).
    // - Bottom right: anchor pivot = (1, 1), offsets should be (-offset.x, -offset.y).
    // - Center: anchor pivot = (0.5, 0.5), offsets should be (0, 0) since they are applied in both
    // directions.
    // For center anchors (which one of the two components are 0.5, we don't need to apply offsets
    // in the direction of the one that is 0.5).
    const math::vector2 adjustedOffsetFromEdges{
        m_offsetFromEdges.x() * (1.0 - 2.0 * m_positionPivot.x()),
        m_offsetFromEdges.y() * (1.0 - 2.0 * m_positionPivot.y())};

    m_position = math::vector2{workPos.x, workPos.y} + anchorPos + adjustedOffsetFromEdges;

    // Set the text of the labels.
    auto& imguiIo{ImGui::GetIO()};

    if (const fps_t fps{imguiIo.Framerate}; fps > 0.0) {
        const frame_time_t frameTime{1000.0 / fps};
        m_fpsLabel.set_text(std::format(FPS_TEXT_FORMAT, fps));
        m_frameTimeLabel.set_text(std::format(FRAME_TIME_TEXT_FORMAT, frameTime.count()));
    } else {
        m_fpsLabel.set_text(FPS_NOT_AVAILABLE_TEXT);
        m_frameTimeLabel.set_text(FRAME_TIME_NOT_AVAILABLE_TEXT);
    }
}

void fps_overlay::render() {
    ImGui::SetNextWindowPos(imgui::to_imvec2(m_position), ImGuiCond_Always,
                            imgui::to_imvec2(m_positionPivot));

    ImGui::Begin(WINDOW_TITLE.data(), nullptr, WINDOW_FLAGS);
    renderable_view::render();
    ImGui::End();
}

} // namespace tca::engine::widgets
