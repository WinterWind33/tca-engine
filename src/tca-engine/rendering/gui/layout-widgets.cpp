// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/layout-widgets.hpp"

// C++ STL
#include <optional>
#include <ranges>
#include <stdexcept>

// Third-party
#include <imgui.h>
#include <gsl/gsl>

#include "core/math/math.hpp"
#include "rendering/colors.hpp"
#include "rendering/gui/imgui/imgui-color-helpers.hpp"
#include "rendering/theming/active-theme.hpp"

namespace tca::engine::widgets {

math::vector2 compute_anchor_position(const anchor_position anchorPos,
                                      const math::vector2& parentSize) {
    // Sanity check: check that the parent size is valid (non-zero and non-negative).
    if (parentSize.x() < 0 || parentSize.y() < 0 || math::is_nearly_zero(parentSize.x()) ||
        math::is_nearly_zero(parentSize.y())) {
        throw std::invalid_argument{"Parent size must be positive and non-zero"};
    }

    switch (anchorPos) {
        case anchor_position::top_left:
            return math::vector2::zero();
        case anchor_position::top_center:
            return math::vector2{math::vector2::x_axis_vector() * parentSize.x() * 0.5};
        case anchor_position::top_right:
            return math::vector2{math::vector2::x_axis_vector() * parentSize.x()};
        case anchor_position::center_left:
            return math::vector2{math::vector2::y_axis_vector() * parentSize.y() * 0.5};
        case anchor_position::center:
            return math::vector2{parentSize.x() * 0.5, parentSize.y() * 0.5};
        case anchor_position::center_right:
            return math::vector2{parentSize.x(), parentSize.y() * 0.5};
        case anchor_position::bottom_left:
            return math::vector2{math::vector2::y_axis_vector() * parentSize.y()};
        case anchor_position::bottom_center:
            return math::vector2{parentSize.x() * 0.5, parentSize.y()};
        case anchor_position::bottom_right:
            return math::vector2{parentSize.x(), parentSize.y()};
    }

    throw std::invalid_argument{"Invalid anchor position"};
}

math::vector2 compute_anchor_pivot(const anchor_position anchorPos) {
    switch (anchorPos) {
        case anchor_position::top_left:
            return math::vector2::zero();
        case anchor_position::top_center:
            return math::vector2{0.5, 0.0};
        case anchor_position::top_right:
            return math::vector2::x_axis_vector();
        case anchor_position::center_left:
            return math::vector2{math::vector2::y_axis_vector() * 0.5};
        case anchor_position::center:
            return math::vector2{0.5, 0.5};
        case anchor_position::center_right:
            return math::vector2{1.0, 0.5};
        case anchor_position::bottom_left:
            return math::vector2::y_axis_vector();
        case anchor_position::bottom_center:
            return math::vector2{0.5, 1.0};
        case anchor_position::bottom_right:
            return math::vector2{1.0, 1.0};
    }

    throw std::invalid_argument{"Invalid anchor position"};
}

void same_line_layout::render() {
    if (m_renderables.empty()) {
        return;
    }

    // We render all the child renderables on the same line, so we need to set the cursor position
    // to the right of the last rendered child renderable before rendering the next one.
    // We take only the first n-1 renderables, where n is the total number of renderables, because
    // we don't want to call ImGui::SameLine() after rendering the last renderable, since it would
    // cause an extra empty line to be rendered after the last renderable.
    for (const gsl::not_null<rendering::renderable*>& renderable :
         std::views::all(m_renderables) | std::views::take(m_renderables.size() - 1)) {
        renderable->render();
        ImGui::SameLine();
    }

    // Render the last renderable without calling ImGui::SameLine() after it.
    m_renderables.back()->render();
}

// =============== viewport_layout_widget implementation ===============

viewport_layout_widget::viewport_layout_widget(
    const id_view_t id, const std::optional<theming::color_role> backgroundRole) noexcept
    : m_id{id},
      m_backgroundRole{backgroundRole} {}

void viewport_layout_widget::render() {
    static constexpr auto IMGUI_WINDOW_FLAGS{ImGuiWindowFlags_NoTitleBar |
                                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove};

    const gsl::not_null<const ImGuiViewport*> mainViewport{ImGui::GetMainViewport()};
    ImGui::SetNextWindowPos(ImVec2{mainViewport->Pos.x, mainViewport->Pos.y});
    ImGui::SetNextWindowSize(ImVec2{mainViewport->Size.x, mainViewport->Size.y});

    // Resolved at render time rather than cached, so that a theme applied after this widget was
    // built still reaches its backdrop.
    const rendering::color backgroundColor{m_backgroundRole ? theming::color_for(*m_backgroundRole)
                                                            : rendering::colors::Transparent};

    auto actualFlags{IMGUI_WINDOW_FLAGS};
    if (backgroundColor.a == 0) {
        actualFlags |= ImGuiWindowFlags_NoBackground;
    }

    {
        // Scoped so the override is undone as soon as Begin() has read it, rather than lasting for
        // the whole of this render(): it must not reach the child renderables below, which would
        // otherwise inherit it for any window of their own.
        std::optional<imgui::scoped_style_color> backgroundOverride{};
        if (backgroundColor.a != 0) {
            backgroundOverride.emplace(ImGuiCol_WindowBg, backgroundColor);
        }

        ImGui::SetNextWindowBgAlpha(rendering::normalize_color_component(backgroundColor.a));
        ImGui::Begin(m_id.c_str(), nullptr, actualFlags);
    }

    // Render child renderables of the viewport layout widget.
    rendering::pausable_renderables_view::render();

    ImGui::End();
}

} // namespace tca::engine::widgets
