// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Third-party
#include <imgui.h>

// Engine
#include "rendering/colors.hpp"
#include "rendering/theming/theme-roles.hpp"

namespace tca::engine::imgui {

/**
 * @brief Converts an engine color to the normalized float representation ImGui takes.
 *
 * @param sourceColor The color to convert.
 * @return ImVec4 The same color, with every component normalized to [0.0f, 1.0f].
 */
ImVec4 to_imvec4(const rendering::color& sourceColor) noexcept;

/**
 * @brief Overrides one ImGui style color for the duration of a scope, restoring the previous one
 * when the scope ends.
 *
 * This is the supported way to paint a single element with a role the applied theme did not put
 * into that ImGui slot - most often a text variant, since ImGui has exactly one ImGuiCol_Text and
 * roles like theming::color_role::on_surface_muted therefore cannot live in the style table:
 *
 * ```cpp
 * const imgui::scoped_style_color muted{ImGuiCol_Text, theming::color_role::on_surface_muted};
 * ImGui::Text("%s", hint.c_str());
 * ```
 *
 * It works for any ImGuiCol_ entry, not only text: pushing theming::color_role::error onto
 * ImGuiCol_Button paints one destructive button without the theme having to know about it.
 *
 * @note Neither copyable nor movable, so the push and its pop cannot be separated: ImGui's style
 * stack is strictly last-in-first-out, and nesting these correctly is exactly what that
 * guarantees.
 */
class scoped_style_color final {
public:
    /**
     * @brief Pushes the color the *active* theme gives a role. This is the overload to reach for:
     * what is pushed still has a name and a meaning, so it moves with the theme like everything
     * else.
     *
     * @param target The ImGui style color to override for this scope.
     * @param role The theme role supplying the color.
     */
    scoped_style_color(ImGuiCol target, theming::color_role role);

    /**
     * @brief Pushes an explicit color, for the cases where there is no role to name because the
     * color was computed (a data-driven tint, a colour sampled at runtime).
     *
     * @note If you are about to pass a constant here, you want a custom role instead - see
     * theming::theme::set_custom_color(). A literal written at a call site is precisely the
     * per-widget colour the theme database exists to abolish.
     *
     * @param target The ImGui style color to override for this scope.
     * @param explicitColor The color to push.
     */
    scoped_style_color(ImGuiCol target, const rendering::color& explicitColor);

    /**
     * @brief Restores the style color that was in place before this object was constructed.
     */
    ~scoped_style_color();

    scoped_style_color(const scoped_style_color&) = delete;
    scoped_style_color& operator=(const scoped_style_color&) = delete;
    scoped_style_color(scoped_style_color&&) = delete;
    scoped_style_color& operator=(scoped_style_color&&) = delete;
};

} // namespace tca::engine::imgui
