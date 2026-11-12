// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <cstdint>

// Engine
#include "core/utilities/ranged-value.hpp"

namespace tca::engine::theming {

/**
 * @brief The built-in colour roles. A role names what something *is* in the interface ("the muted
 * text drawn on a surface"), never what colour it happens to be, so that two elements sharing a
 * role are incapable of drifting apart: changing the role's value in the theme changes both.
 *
 * Widgets and rendering backends resolve roles against a theme (see theming::theme); nothing in
 * the engine holds a colour literal of its own.
 *
 * @note The role set here is the vocabulary the *engine* speaks. A game needing a role the engine
 * has never heard of does not have to fork it: theme::set_custom_color() adds one under a name of
 * its own choosing.
 */
enum class color_role : std::uint8_t {
    /**
     * @brief The application's backdrop: what the window is cleared to before a frame is drawn,
     * and what the viewport-filling layout widget paints.
     */
    window_background,

    /**
     * @brief A panel, popup or child region sitting on top of the backdrop.
     */
    surface,

    /**
     * @brief A raised or recessed area within a surface: input frames, buttons at rest, headers.
     */
    surface_variant,

    /**
     * @brief surface_variant while the pointer is over it.
     */
    surface_hovered,

    /**
     * @brief surface_variant while it is being pressed or dragged.
     */
    surface_active,

    /**
     * @brief The dimming laid over everything behind a modal. Normally partially transparent.
     */
    overlay_scrim,

    /**
     * @brief Primary text and icons drawn on a surface.
     */
    on_surface,

    /**
     * @brief Secondary, less important text on a surface: hints, captions, timestamps.
     */
    on_surface_muted,

    /**
     * @brief Text and icons of something that cannot currently be interacted with.
     */
    on_surface_disabled,

    /**
     * @brief Text and icons drawn on top of the accent colour, where on_surface would not have
     * enough contrast.
     */
    on_accent,

    /**
     * @brief The colour carrying the application's identity, used for the elements the eye should
     * land on first: selections, checkmarks, slider grabs, links.
     */
    accent,

    /**
     * @brief The accent while the pointer is over it.
     */
    accent_hovered,

    /**
     * @brief The accent while it is being pressed or dragged.
     */
    accent_active,

    /**
     * @brief Borders and separators that should read as a deliberate line.
     */
    outline,

    /**
     * @brief The quieter line: table gridlines, inner separators, tree hierarchy lines.
     */
    outline_variant,

    /**
     * @brief Something completed or valid.
     */
    success,

    /**
     * @brief Something that needs attention but is not a failure.
     */
    warning,

    /**
     * @brief Something that failed or is invalid.
     */
    error,

    /**
     * @brief Not a role: the number of built-in colour roles. Always keep it last.
     */
    count,
};

/**
 * @brief The built-in dimension roles, all expressed in unscaled pixels except the two content
 * fractions, which are fractions of the available space in the range [0.0, 1.0].
 */
enum class dimension_role : std::uint8_t {
    /**
     * @brief Padding between a window's edge and its content.
     */
    window_padding,

    /**
     * @brief Padding inside a framed element (button, input box) around its label.
     */
    frame_padding,

    /**
     * @brief Spacing between two stacked elements.
     */
    item_spacing,

    /**
     * @brief Spacing between an element and its own label or icon.
     */
    item_inner_spacing,

    /**
     * @brief Horizontal step by which nested content is indented.
     */
    indent,

    /**
     * @brief Thickness of a scrollbar.
     */
    scrollbar_size,

    /**
     * @brief Smallest a draggable grab is allowed to become.
     */
    grab_min_size,

    /**
     * @brief Thickness of borders and separators.
     */
    border_thickness,

    /**
     * @brief Radius by which corners are rounded.
     */
    corner_radius,

    /**
     * @brief Fraction of the available width a page of content should occupy, in [0.0, 1.0].
     */
    content_width_fraction,

    /**
     * @brief Fraction of the available height a page of content should occupy, in [0.0, 1.0].
     */
    content_height_fraction,

    /**
     * @brief Not a role: the number of built-in dimension roles. Always keep it last.
     */
    count,
};

/**
 * @brief Multiplier a typography role applies to the application's body font size. Ranged rather
 * than a bare double because a negative or absurd scale is not a look, it is a bug: a theme
 * carrying one would silently render text inside out or fill the screen with a single word, and
 * the range check catches it where the theme is defined instead.
 */
using typography_scale = core::ranged_value<double, 0.0, 8.0>;

/**
 * @brief How text of a given role is sized, relative to the application's body text. Kept as a
 * struct rather than a bare scale so that a face, weight or letter spacing can be added later
 * without every call site having to change.
 */
struct typography final {
    /**
     * @brief Multiplier applied to the current font size. 1.0 is body text.
     */
    typography_scale scale{1.0};

    constexpr bool operator==(const typography& other) const noexcept = default;
};

/**
 * @brief The built-in typography roles, again naming a piece of text's job rather than its size.
 */
enum class typography_role : std::uint8_t {
    /**
     * @brief Ordinary prose: the size everything else is relative to.
     */
    body,

    /**
     * @brief Small, secondary text: hints, diagnostics overlays, footnotes.
     */
    caption,

    /**
     * @brief The heading of a section within a page.
     */
    subheading,

    /**
     * @brief The heading of a page.
     */
    heading,

    /**
     * @brief The largest text in the application: title screens, splash text.
     */
    display,

    /**
     * @brief Not a role: the number of built-in typography roles. Always keep it last.
     */
    count,
};

/**
 * @brief Number of built-in colour roles, i.e. the size of a theme's colour table.
 */
inline constexpr std::size_t COLOR_ROLE_COUNT{static_cast<std::size_t>(color_role::count)};

/**
 * @brief Number of built-in dimension roles, i.e. the size of a theme's dimension table.
 */
inline constexpr std::size_t DIMENSION_ROLE_COUNT{static_cast<std::size_t>(dimension_role::count)};

/**
 * @brief Number of built-in typography roles, i.e. the size of a theme's typography table.
 */
inline constexpr std::size_t TYPOGRAPHY_ROLE_COUNT{
    static_cast<std::size_t>(typography_role::count)};

/**
 * @brief Turns a built-in role into its index in the corresponding theme table.
 *
 * @tparam RoleType One of color_role, dimension_role or typography_role.
 * @param role The role to index. Must not be the enum's `count` sentinel.
 * @return constexpr std::size_t The role's table index.
 */
template <typename RoleType>
constexpr std::size_t to_role_index(const RoleType role) noexcept {
    return static_cast<std::size_t>(role);
}

} // namespace tca::engine::theming
