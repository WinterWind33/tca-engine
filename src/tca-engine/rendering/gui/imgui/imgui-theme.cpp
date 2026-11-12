// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/imgui/imgui-theme.hpp"

// C++ STL
#include <array>
#include <cstddef>
#include <utility>

// Third-party
#include <imgui.h>

// Engine
#include "core/utilities/ranged-value.hpp"
#include "rendering/colors.hpp"
#include "rendering/gui/imgui/imgui-color-helpers.hpp"
#include "rendering/theming/active-theme.hpp"

namespace tca::engine::imgui {

namespace {

/**
 * @brief Says which theme role one ImGuiCol_ entry takes its colour from.
 */
struct imgui_color_binding final {
    /**
     * @brief The ImGui colour this binding covers.
     */
    ImGuiCol imgui_color{};

    /**
     * @brief The theme role it resolves against.
     */
    theming::color_role role{};

    /**
     * @brief Multiplier applied to the role's alpha. Entries that exist to *tint* something
     * already drawn (row stripes, selection highlights, drop targets) need the role's colour at a
     * fraction of its opacity; this keeps that in the mapping instead of forcing a separate role
     * per opacity level. Ranged, since a multiplier outside [0, 1] would make an element more
     * opaque than the role it claims to be showing.
     */
    core::ranged_value<float, 0.0f, 1.0f> alpha_scale{1.0f};
};

// Every ImGuiCol_ entry, in enum order, mapped to the role it takes its colour from. Kept in enum
// order and checked below, so this table is the exhaustiveness guarantee: no ImGui element can be
// left holding a colour from a previously applied style.
constexpr std::array COLOR_BINDINGS{
    imgui_color_binding{ImGuiCol_Text, theming::color_role::on_surface},
    imgui_color_binding{ImGuiCol_TextDisabled, theming::color_role::on_surface_disabled},
    imgui_color_binding{ImGuiCol_WindowBg, theming::color_role::surface},
    imgui_color_binding{ImGuiCol_ChildBg, theming::color_role::surface},
    imgui_color_binding{ImGuiCol_PopupBg, theming::color_role::surface},
    imgui_color_binding{ImGuiCol_Border, theming::color_role::outline},
    // ImGui draws this *behind* the border as a drop shadow; the engine's themes are flat, so it
    // is bound to a role at zero opacity rather than left to ImGui's default.
    imgui_color_binding{ImGuiCol_BorderShadow, theming::color_role::outline_variant, 0.0f},
    imgui_color_binding{ImGuiCol_FrameBg, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_FrameBgHovered, theming::color_role::surface_hovered},
    imgui_color_binding{ImGuiCol_FrameBgActive, theming::color_role::surface_active},
    imgui_color_binding{ImGuiCol_TitleBg, theming::color_role::surface},
    imgui_color_binding{ImGuiCol_TitleBgActive, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_TitleBgCollapsed, theming::color_role::surface, 0.75f},
    imgui_color_binding{ImGuiCol_MenuBarBg, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_ScrollbarBg, theming::color_role::surface},
    imgui_color_binding{ImGuiCol_ScrollbarGrab, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_ScrollbarGrabHovered, theming::color_role::surface_hovered},
    imgui_color_binding{ImGuiCol_ScrollbarGrabActive, theming::color_role::surface_active},
    imgui_color_binding{ImGuiCol_CheckMark, theming::color_role::accent},
    imgui_color_binding{ImGuiCol_CheckboxSelectedBg, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_SliderGrab, theming::color_role::accent},
    imgui_color_binding{ImGuiCol_SliderGrabActive, theming::color_role::accent_active},
    imgui_color_binding{ImGuiCol_Button, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_ButtonHovered, theming::color_role::surface_hovered},
    imgui_color_binding{ImGuiCol_ButtonActive, theming::color_role::surface_active},
    imgui_color_binding{ImGuiCol_Header, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_HeaderHovered, theming::color_role::surface_hovered},
    imgui_color_binding{ImGuiCol_HeaderActive, theming::color_role::surface_active},
    imgui_color_binding{ImGuiCol_Separator, theming::color_role::outline},
    imgui_color_binding{ImGuiCol_SeparatorHovered, theming::color_role::accent_hovered},
    imgui_color_binding{ImGuiCol_SeparatorActive, theming::color_role::accent_active},
    imgui_color_binding{ImGuiCol_ResizeGrip, theming::color_role::outline_variant},
    imgui_color_binding{ImGuiCol_ResizeGripHovered, theming::color_role::accent_hovered},
    imgui_color_binding{ImGuiCol_ResizeGripActive, theming::color_role::accent_active},
    imgui_color_binding{ImGuiCol_InputTextCursor, theming::color_role::on_surface},
    imgui_color_binding{ImGuiCol_TabHovered, theming::color_role::surface_hovered},
    imgui_color_binding{ImGuiCol_Tab, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_TabSelected, theming::color_role::accent},
    imgui_color_binding{ImGuiCol_TabSelectedOverline, theming::color_role::accent},
    imgui_color_binding{ImGuiCol_TabDimmed, theming::color_role::surface},
    imgui_color_binding{ImGuiCol_TabDimmedSelected, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_TabDimmedSelectedOverline, theming::color_role::outline_variant},
    imgui_color_binding{ImGuiCol_PlotLines, theming::color_role::accent},
    imgui_color_binding{ImGuiCol_PlotLinesHovered, theming::color_role::accent_hovered},
    imgui_color_binding{ImGuiCol_PlotHistogram, theming::color_role::accent},
    imgui_color_binding{ImGuiCol_PlotHistogramHovered, theming::color_role::accent_hovered},
    imgui_color_binding{ImGuiCol_TableHeaderBg, theming::color_role::surface_variant},
    imgui_color_binding{ImGuiCol_TableBorderStrong, theming::color_role::outline},
    imgui_color_binding{ImGuiCol_TableBorderLight, theming::color_role::outline_variant},
    // Even rows are left showing the surface they sit on, odd ones are barely tinted by the text
    // colour, which is how a striped table reads without introducing two more roles.
    imgui_color_binding{ImGuiCol_TableRowBg, theming::color_role::surface, 0.0f},
    imgui_color_binding{ImGuiCol_TableRowBgAlt, theming::color_role::on_surface, 0.06f},
    imgui_color_binding{ImGuiCol_TextLink, theming::color_role::accent},
    imgui_color_binding{ImGuiCol_TextSelectedBg, theming::color_role::accent, 0.35f},
    imgui_color_binding{ImGuiCol_TreeLines, theming::color_role::outline_variant},
    imgui_color_binding{ImGuiCol_DragDropTarget, theming::color_role::accent},
    imgui_color_binding{ImGuiCol_DragDropTargetBg, theming::color_role::accent, 0.25f},
    imgui_color_binding{ImGuiCol_UnsavedMarker, theming::color_role::warning},
    imgui_color_binding{ImGuiCol_NavCursor, theming::color_role::accent},
    imgui_color_binding{ImGuiCol_NavWindowingHighlight, theming::color_role::on_surface, 0.7f},
    imgui_color_binding{ImGuiCol_NavWindowingDimBg, theming::color_role::overlay_scrim, 0.8f},
    imgui_color_binding{ImGuiCol_ModalWindowDimBg, theming::color_role::overlay_scrim},
};

/**
 * @brief Checks that COLOR_BINDINGS lists every ImGuiCol_ entry exactly once, by requiring the
 * table to be in enum order: entry i must bind ImGuiCol i.
 *
 * @return constexpr bool True when the table is complete and in order.
 */
constexpr bool bindings_cover_every_imgui_color() noexcept {
    for (std::size_t index{}; index < COLOR_BINDINGS.size(); ++index) {
        if (COLOR_BINDINGS[index].imgui_color != static_cast<ImGuiCol>(index)) {
            return false;
        }
    }

    return true;
}

static_assert(COLOR_BINDINGS.size() == static_cast<std::size_t>(ImGuiCol_COUNT) &&
                  bindings_cover_every_imgui_color(),
              "COLOR_BINDINGS must map every ImGuiCol_ entry, in enum order. A Dear ImGui update "
              "that adds, removes or reorders a color lands here: give the new entry a theme role "
              "rather than letting it keep whatever the previously applied style left behind.");

ImVec4 with_scaled_alpha(const rendering::color& sourceColor, const float alphaScale) noexcept {
    ImVec4 converted{to_imvec4(sourceColor)};
    converted.w *= alphaScale;
    return converted;
}

float to_size(const double dimension) noexcept {
    return static_cast<float>(dimension);
}

void apply_sizes(ImGuiStyle& style, const theming::theme& themeToApply,
                 const float sizeScale) noexcept {
    const auto scaled{[sizeScale, &themeToApply](const theming::dimension_role role) {
        return to_size(themeToApply.resolve(role)) * sizeScale;
    }};

    const float windowPadding{scaled(theming::dimension_role::window_padding)};
    const float framePadding{scaled(theming::dimension_role::frame_padding)};
    const float itemSpacing{scaled(theming::dimension_role::item_spacing)};
    const float itemInnerSpacing{scaled(theming::dimension_role::item_inner_spacing)};
    const float borderThickness{scaled(theming::dimension_role::border_thickness)};
    const float cornerRadius{scaled(theming::dimension_role::corner_radius)};

    style.WindowPadding = ImVec2{windowPadding, windowPadding};
    style.FramePadding = ImVec2{framePadding, framePadding};
    style.ItemSpacing = ImVec2{itemSpacing, itemSpacing};
    style.ItemInnerSpacing = ImVec2{itemInnerSpacing, itemInnerSpacing};
    style.IndentSpacing = scaled(theming::dimension_role::indent);
    style.ScrollbarSize = scaled(theming::dimension_role::scrollbar_size);
    style.GrabMinSize = scaled(theming::dimension_role::grab_min_size);

    style.WindowBorderSize = borderThickness;
    style.ChildBorderSize = borderThickness;
    style.PopupBorderSize = borderThickness;
    style.FrameBorderSize = borderThickness;
    style.TabBorderSize = borderThickness;
    style.SeparatorTextBorderSize = borderThickness;

    style.WindowRounding = cornerRadius;
    style.ChildRounding = cornerRadius;
    style.PopupRounding = cornerRadius;
    style.FrameRounding = cornerRadius;
    style.ScrollbarRounding = cornerRadius;
    style.GrabRounding = cornerRadius;
    style.TabRounding = cornerRadius;
    style.ImageRounding = cornerRadius;
}

} // namespace

void apply_theme(const theming::theme& themeToApply, const float sizeScale) {
    ImGuiStyle& style{ImGui::GetStyle()};

    // The font scaling fields describe the *display*, not the theme: they are set once from the
    // window's DPI and must survive a theme swap, so they are carried across the reset below.
    const float fontSizeBase{style.FontSizeBase};
    const float fontScaleMain{style.FontScaleMain};
    const float fontScaleDpi{style.FontScaleDpi};

    // Start from ImGui's own defaults rather than from whatever style is currently installed. This
    // is what makes applying a theme exhaustive even for the style fields no role covers (cell
    // padding, text alignments, tessellation): none of them can carry a value over from the
    // previous theme.
    style = ImGuiStyle{};
    style.FontSizeBase = fontSizeBase;
    style.FontScaleMain = fontScaleMain;
    style.FontScaleDpi = fontScaleDpi;

    // Scaled before the theme's own sizes are written, so that the fields no role covers end up
    // DPI-scaled too, while the ones a role does cover are written already scaled by apply_sizes()
    // instead of being scaled twice.
    style.ScaleAllSizes(sizeScale);

    for (const imgui_color_binding& binding : COLOR_BINDINGS) {
        style.Colors[binding.imgui_color] =
            with_scaled_alpha(themeToApply.resolve(binding.role), binding.alpha_scale);
    }

    apply_sizes(style, themeToApply, sizeScale);
}

void use_theme(theming::theme newTheme) {
    // A theme can legitimately be chosen before the ImGui context exists - configuring the
    // application is the normal case - so there may be no style to write into yet. The theme still
    // becomes the active one, and the context picks it up when it is created.
    if (ImGui::GetCurrentContext() != nullptr) {
        // The DPI scale the application is already running at, so re-theming at runtime does not
        // silently undo it.
        apply_theme(newTheme, ImGui::GetStyle().FontScaleDpi);
    }

    theming::set_active_theme(std::move(newTheme));
}

} // namespace tca::engine::imgui
