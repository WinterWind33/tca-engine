// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

// Engine
#include "rendering/colors.hpp"
#include "rendering/theming/theme-roles.hpp"

namespace tca::engine::theming {

namespace defaults {

/**
 * @brief Builds the colour table of the engine's default theme.
 *
 * Every entry starts as fully transparent and every role is then assigned by name, so a role
 * added to color_role without a value here is caught by the static_assert below at compile time
 * instead of silently shipping as transparent black.
 *
 * @return constexpr std::array<rendering::color, COLOR_ROLE_COUNT> The default colour table.
 */
constexpr std::array<rendering::color, COLOR_ROLE_COUNT> make_default_colors() noexcept {
    std::array<rendering::color, COLOR_ROLE_COUNT> colors{};
    colors.fill(rendering::colors::Transparent);

    colors[to_role_index(color_role::window_background)] = rendering::rgb(18, 18, 20);
    colors[to_role_index(color_role::surface)] = rendering::rgb(30, 31, 36);
    colors[to_role_index(color_role::surface_variant)] = rendering::rgb(44, 46, 54);
    colors[to_role_index(color_role::surface_hovered)] = rendering::rgb(58, 61, 72);
    colors[to_role_index(color_role::surface_active)] = rendering::rgb(72, 76, 90);
    colors[to_role_index(color_role::overlay_scrim)] = rendering::rgba(0, 0, 0, 168);
    colors[to_role_index(color_role::on_surface)] = rendering::rgb(235, 236, 240);
    colors[to_role_index(color_role::on_surface_muted)] = rendering::rgb(150, 153, 163);
    colors[to_role_index(color_role::on_surface_disabled)] = rendering::rgb(100, 102, 110);
    colors[to_role_index(color_role::on_accent)] = rendering::rgb(12, 13, 16);
    colors[to_role_index(color_role::accent)] = rendering::rgb(70, 130, 180);
    colors[to_role_index(color_role::accent_hovered)] = rendering::rgb(90, 155, 208);
    colors[to_role_index(color_role::accent_active)] = rendering::rgb(112, 178, 229);
    colors[to_role_index(color_role::outline)] = rendering::rgb(70, 73, 84);
    colors[to_role_index(color_role::outline_variant)] = rendering::rgb(52, 54, 62);
    colors[to_role_index(color_role::success)] = rendering::rgb(76, 175, 80);
    colors[to_role_index(color_role::warning)] = rendering::rgb(255, 167, 38);
    colors[to_role_index(color_role::error)] = rendering::rgb(229, 84, 81);

    return colors;
}

/**
 * @brief Builds the dimension table of the engine's default theme. Entries start negative and
 * every role is assigned by name, so a role left without a value is caught at compile time.
 *
 * @return constexpr std::array<double, DIMENSION_ROLE_COUNT> The default dimension table.
 */
constexpr std::array<double, DIMENSION_ROLE_COUNT> make_default_dimensions() noexcept {
    std::array<double, DIMENSION_ROLE_COUNT> dimensions{};
    dimensions.fill(-1.0);

    dimensions[to_role_index(dimension_role::window_padding)] = 8.0;
    dimensions[to_role_index(dimension_role::frame_padding)] = 4.0;
    dimensions[to_role_index(dimension_role::item_spacing)] = 8.0;
    dimensions[to_role_index(dimension_role::item_inner_spacing)] = 4.0;
    dimensions[to_role_index(dimension_role::indent)] = 21.0;
    dimensions[to_role_index(dimension_role::scrollbar_size)] = 14.0;
    dimensions[to_role_index(dimension_role::grab_min_size)] = 12.0;
    dimensions[to_role_index(dimension_role::border_thickness)] = 1.0;
    dimensions[to_role_index(dimension_role::corner_radius)] = 4.0;
    dimensions[to_role_index(dimension_role::content_width_fraction)] = 0.8;
    dimensions[to_role_index(dimension_role::content_height_fraction)] = 0.8;

    return dimensions;
}

/**
 * @brief Builds the typography table of the engine's default theme. Entries start at a scale of
 * zero - the one value no role would ever be given, since it renders nothing - and every role is
 * assigned by name, so a role left without a value is caught at compile time.
 *
 * @return constexpr std::array<typography, TYPOGRAPHY_ROLE_COUNT> The default typography table.
 */
constexpr std::array<typography, TYPOGRAPHY_ROLE_COUNT> make_default_typography() noexcept {
    std::array<typography, TYPOGRAPHY_ROLE_COUNT> typographies{};
    typographies.fill(typography{.scale = 0.0});

    typographies[to_role_index(typography_role::body)] = typography{.scale = 1.0};
    typographies[to_role_index(typography_role::caption)] = typography{.scale = 0.85};
    typographies[to_role_index(typography_role::subheading)] = typography{.scale = 1.15};
    typographies[to_role_index(typography_role::heading)] = typography{.scale = 1.35};
    typographies[to_role_index(typography_role::display)] = typography{.scale = 1.75};

    return typographies;
}

/**
 * @brief The engine's default colour table: a dark palette with a steel blue accent.
 */
inline constexpr std::array<rendering::color, COLOR_ROLE_COUNT> DEFAULT_COLORS{
    make_default_colors()};

/**
 * @brief The engine's default dimension table.
 */
inline constexpr std::array<double, DIMENSION_ROLE_COUNT> DEFAULT_DIMENSIONS{
    make_default_dimensions()};

/**
 * @brief The engine's default typography table.
 */
inline constexpr std::array<typography, TYPOGRAPHY_ROLE_COUNT> DEFAULT_TYPOGRAPHY{
    make_default_typography()};

// Exhaustiveness of the defaults, checked at compile time: every built-in role must have been
// given a value above, otherwise it would still hold the sentinel the table was filled with.
// Adding a role to one of the enums and forgetting to give it a default breaks the build here
// rather than shipping as transparent black or as a negative size.
static_assert(std::ranges::none_of(DEFAULT_COLORS,
                                   [](const rendering::color& roleColor) {
                                       return roleColor == rendering::colors::Transparent;
                                   }),
              "Every built-in color_role must have a default value in make_default_colors().");
static_assert(
    std::ranges::none_of(DEFAULT_DIMENSIONS,
                         [](const double dimension) {
                             return dimension < 0.0;
                         }),
    "Every built-in dimension_role must have a default value in make_default_dimensions().");
static_assert(
    std::ranges::none_of(DEFAULT_TYPOGRAPHY,
                         [](const typography& text) {
                             return text.scale <= 0.0;
                         }),
    "Every built-in typography_role must have a default value in make_default_typography().");

} // namespace defaults

/**
 * @brief The theme resource database: the single artifact holding what an application looks like.
 *
 * A theme maps every built-in role (see theme-roles.hpp) to a value, plus any number of custom
 * roles a game adds under names of its own. Widgets and rendering backends ask it for a *role*
 * and never for a value, so changing one entry here changes every element resolving that role,
 * across game widgets, engine widgets and the window clear colour alike.
 *
 * It deliberately knows nothing about ImGui, or about any other renderer: it is plain data,
 * constructible and testable with no graphics context at all. Applying it to a concrete backend
 * is a separate step (see rendering/gui/imgui/imgui-theme.hpp for the ImGui one), so a second
 * backend can be themed from the very same database without restating it.
 *
 * ### Derivation
 *
 * A variant (high contrast, colour-blind friendly, a seasonal skin) is defined by deriving from
 * an existing theme and overriding only what differs - the equivalent of style inheritance:
 *
 * ```cpp
 * const theming::theme highContrast{
 *     base.derive()
 *         .set(theming::color_role::on_surface, rendering::colors::WHITE)
 *         .set(theming::color_role::window_background, rendering::colors::BLACK)};
 * ```
 *
 * Derivation copies rather than keeping a pointer to the base on purpose: a derived theme keeps
 * working after the theme it came from is gone, and resolving never has to walk a chain.
 *
 * @note Resolving a *built-in* role is total: it always returns a value, so call sites have no
 * failure path and no std::optional to unwrap. Only custom roles, which the engine cannot
 * guarantee anyone defined, resolve to an optional.
 */
class theme final {
public:
    /**
     * @brief Constructs the engine's default theme: a dark palette with a steel blue accent, with
     * every built-in role populated.
     */
    theme() = default;

    /**
     * @brief Returns a copy of this theme, to be used as the base of a variant. Overrides are then
     * applied to the result through the chainable set() overloads.
     *
     * @return theme A copy of this theme, custom roles included.
     */
    theme derive() const {
        return *this;
    }

    /**
     * @brief Overrides a built-in colour role.
     *
     * @param role The role to override. Must not be color_role::count.
     * @param value The colour the role resolves to from now on.
     * @return theme& This theme, so overrides can be chained.
     */
    theme& set(color_role role, rendering::color value);

    /**
     * @brief Overrides a built-in dimension role.
     *
     * @param role The role to override. Must not be dimension_role::count.
     * @param value The value the role resolves to from now on.
     * @return theme& This theme, so overrides can be chained.
     */
    theme& set(dimension_role role, double value);

    /**
     * @brief Overrides a built-in typography role.
     *
     * @param role The role to override. Must not be typography_role::count.
     * @param value The typography the role resolves to from now on.
     * @return theme& This theme, so overrides can be chained.
     */
    theme& set(typography_role role, typography value);

    /**
     * @brief Defines (or redefines) a colour role the engine has never heard of, so a game can
     * extend the vocabulary without forking the engine or waiting on a release.
     *
     * @param name The role's name. Chosen by the game; it only has to be unique among its own
     * custom colour roles.
     * @param value The colour the role resolves to.
     * @return theme& This theme, so overrides can be chained.
     */
    theme& set_custom_color(std::string_view name, rendering::color value);

    /**
     * @brief Defines (or redefines) a custom dimension role. See set_custom_color().
     *
     * @param name The role's name.
     * @param value The value the role resolves to.
     * @return theme& This theme, so overrides can be chained.
     */
    theme& set_custom_dimension(std::string_view name, double value);

    /**
     * @brief Defines (or redefines) a custom typography role. See set_custom_color().
     *
     * @param name The role's name.
     * @param value The typography the role resolves to.
     * @return theme& This theme, so overrides can be chained.
     */
    theme& set_custom_typography(std::string_view name, typography value);

    /**
     * @brief Resolves a built-in colour role. Total: there is no failure path.
     *
     * @param role The role to resolve. Must not be color_role::count.
     * @return rendering::color The colour this theme gives that role.
     */
    rendering::color resolve(color_role role) const noexcept;

    /**
     * @brief Resolves a built-in dimension role. Total: there is no failure path.
     *
     * @param role The role to resolve. Must not be dimension_role::count.
     * @return double The value this theme gives that role.
     */
    double resolve(dimension_role role) const noexcept;

    /**
     * @brief Resolves a built-in typography role. Total: there is no failure path.
     *
     * @param role The role to resolve. Must not be typography_role::count.
     * @return typography The typography this theme gives that role.
     */
    typography resolve(typography_role role) const noexcept;

    /**
     * @brief Resolves a custom colour role.
     *
     * @param name The role's name, as passed to set_custom_color().
     * @return std::optional<rendering::color> The colour, or std::nullopt if neither this theme
     * nor the one it was derived from ever defined that role.
     */
    std::optional<rendering::color> resolve_custom_color(std::string_view name) const;

    /**
     * @brief Resolves a custom dimension role.
     *
     * @param name The role's name, as passed to set_custom_dimension().
     * @return std::optional<double> The value, or std::nullopt if the role is not defined.
     */
    std::optional<double> resolve_custom_dimension(std::string_view name) const;

    /**
     * @brief Resolves a custom typography role.
     *
     * @param name The role's name, as passed to set_custom_typography().
     * @return std::optional<typography> The typography, or std::nullopt if the role is not
     * defined.
     */
    std::optional<typography> resolve_custom_typography(std::string_view name) const;

    /**
     * @brief Resolves a custom colour role, falling back to a built-in one when the game never
     * defined it. This is what widget code should normally use, so that a custom role stays an
     * override of something the engine already knows how to draw rather than a new failure path.
     *
     * @param name The custom role's name.
     * @param fallback The built-in role to resolve instead when the custom one is not defined.
     * @return rendering::color The resolved colour.
     */
    rendering::color resolve_custom_color_or(std::string_view name, color_role fallback) const;

    /**
     * @brief Resolves a custom dimension role, falling back to a built-in one. See
     * resolve_custom_color_or().
     *
     * @param name The custom role's name.
     * @param fallback The built-in role to resolve instead when the custom one is not defined.
     * @return double The resolved value.
     */
    double resolve_custom_dimension_or(std::string_view name, dimension_role fallback) const;

    /**
     * @brief Resolves a custom typography role, falling back to a built-in one. See
     * resolve_custom_color_or().
     *
     * @param name The custom role's name.
     * @param fallback The built-in role to resolve instead when the custom one is not defined.
     * @return typography The resolved typography.
     */
    typography resolve_custom_typography_or(std::string_view name, typography_role fallback) const;

    bool operator==(const theme& other) const = default;

private:
    // Built-in roles live in fixed-size tables indexed by the enumerator, which is what makes
    // resolving them total: there is no entry that can be missing.
    std::array<rendering::color, COLOR_ROLE_COUNT> m_colors{defaults::DEFAULT_COLORS};
    std::array<double, DIMENSION_ROLE_COUNT> m_dimensions{defaults::DEFAULT_DIMENSIONS};
    std::array<typography, TYPOGRAPHY_ROLE_COUNT> m_typography{defaults::DEFAULT_TYPOGRAPHY};

    // Custom roles a game added. std::less<> so that they can be looked up by std::string_view
    // without allocating a std::string on every resolve.
    std::map<std::string, rendering::color, std::less<>> m_customColors{};
    std::map<std::string, double, std::less<>> m_customDimensions{};
    std::map<std::string, typography, std::less<>> m_customTypography{};
};

} // namespace tca::engine::theming
