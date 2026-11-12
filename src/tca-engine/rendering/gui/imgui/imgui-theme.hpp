// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Engine
#include "rendering/theming/theme.hpp"

namespace tca::engine::imgui {

/**
 * @brief Mirrors a theme into the current ImGui context's style: every ImGuiCol_ entry and every
 * style size that has a role is written from the theme.
 *
 * The application is exhaustive by construction. The style is first reset to ImGui's own defaults,
 * so no value can silently survive from whatever style happened to be applied before, and the
 * colour table is written through a mapping that is checked at compile time to cover every single
 * ImGuiCol_ entry - adding an entry in a future Dear ImGui version breaks the build rather than
 * quietly leaving that element unthemed.
 *
 * @note This only writes into the ImGui style. It does not change which theme
 * theming::get_active_theme() returns, so widgets resolving roles themselves would still see the
 * old one - use use_theme() to change both at once.
 *
 * @param themeToApply The theme to mirror into the ImGui style.
 * @param sizeScale Factor applied to every size the theme contributes, so that a DPI-scaled
 * application gets DPI-scaled paddings and roundings. 1.0f leaves the theme's sizes as authored.
 */
void apply_theme(const theming::theme& themeToApply, float sizeScale = 1.0f);

/**
 * @brief Makes a theme the application's active one and mirrors it into the ImGui style in one
 * step. This is what a game offering a theme setting calls: it can be called at any point after
 * startup, and takes effect from the next frame with no restart.
 *
 * The size scale is taken from the style's current DPI scale, so re-theming at runtime keeps
 * whatever DPI scaling the application was already running at.
 *
 * @param newTheme The theme to wear from now on.
 */
void use_theme(theming::theme newTheme);

} // namespace tca::engine::imgui
