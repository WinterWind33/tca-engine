// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Engine
#include "rendering/colors.hpp"
#include "rendering/theming/theme.hpp"

namespace tca::engine::theming {

/**
 * @brief Returns the theme the application is currently wearing.
 *
 * This is the ambient access widgets use: a widget resolves its roles against this at render time
 * rather than being handed a theme through its constructor, so adding a themed element to a
 * widget does not ripple through every call site that builds it. Before anything sets one, this
 * is the engine's default dark theme, so it is never absent and widget code never has a null
 * theme to handle.
 *
 * @note Read it at render time rather than caching the resolved values in the widget: that is what
 * makes swapping the theme at runtime reach everything without a widget having to be told.
 *
 * @return const theme& The active theme.
 */
const theme& get_active_theme() noexcept;

/**
 * @brief Makes the given theme the one the application wears from now on. Every widget resolving
 * roles picks it up on its next frame.
 *
 * @note This only changes the *data*. A rendering backend that mirrors the theme into a style of
 * its own (as ImGui does) has to be told as well - imgui::use_theme() does both, and is what a
 * game offering a theme setting should call.
 *
 * @param newTheme The theme to make active.
 */
void set_active_theme(theme newTheme);

/**
 * @brief Resolves a colour role against the active theme. Shorthand for
 * get_active_theme().resolve(role), which is what widget code overwhelmingly does.
 *
 * @param role The role to resolve.
 * @return rendering::color The colour the active theme gives that role.
 */
rendering::color color_for(color_role role) noexcept;

/**
 * @brief Resolves a dimension role against the active theme.
 *
 * @param role The role to resolve.
 * @return double The value the active theme gives that role.
 */
double dimension_for(dimension_role role) noexcept;

/**
 * @brief Resolves a typography role against the active theme.
 *
 * @param role The role to resolve.
 * @return typography The typography the active theme gives that role.
 */
typography typography_for(typography_role role) noexcept;

/**
 * @brief RAII helper that makes a theme active for as long as it is alive and puts the previous
 * one back afterwards. Meant for tests, and for any code that has to render a subtree under a
 * different theme without leaking it into the rest of the frame.
 */
class scoped_active_theme final {
public:
    /**
     * @brief Makes the given theme active, remembering the one it replaced.
     *
     * @param newTheme The theme to make active for this scope.
     */
    explicit scoped_active_theme(theme newTheme);

    /**
     * @brief Restores the theme that was active when this object was constructed.
     */
    ~scoped_active_theme();

    scoped_active_theme(const scoped_active_theme&) = delete;
    scoped_active_theme& operator=(const scoped_active_theme&) = delete;
    scoped_active_theme(scoped_active_theme&&) = delete;
    scoped_active_theme& operator=(scoped_active_theme&&) = delete;

private:
    theme m_previousTheme;
};

} // namespace tca::engine::theming
