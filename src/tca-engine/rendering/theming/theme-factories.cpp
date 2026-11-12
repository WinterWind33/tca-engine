// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/theming/theme-factories.hpp"

// C++ STL
#include <algorithm>

// Engine
#include "rendering/color-conversions.hpp"

namespace tca::engine::theming {

namespace {

using rendering::normalized_component;

// Backgrounds and outlines use the accent's hue and saturation as-is (only brightness is
// overridden), at fixed, deliberately dark levels rather than a shade proportional to the accent's
// own brightness, so the theme reads consistently dark regardless of how dark the source
// wallpaper happens to be, while still staying visibly tinted by its hue.
constexpr normalized_component BACKGROUND_VALUE{0.22};
constexpr normalized_component SECONDARY_BACKGROUND_VALUE{0.34};
constexpr normalized_component OUTLINE_VALUE{0.5};

// Interactive elements use the accent's own hue/saturation, but never fall below this brightness,
// so a dark accent (e.g. sampled from a night sky wallpaper) still yields legible, colorful
// buttons instead of a near-black theme.
constexpr normalized_component MIN_ACCENT_VALUE{0.55};
constexpr double HOVERED_VALUE_SCALE{1.15};
constexpr double ACTIVE_VALUE_SCALE{1.3};

// Above this brightness the accent is light enough that dark text on top of it reads better than
// light text.
constexpr normalized_component DARK_TEXT_ON_ACCENT_THRESHOLD{0.6};

/**
 * @brief Brightens a value by the given factor, clamped so it cannot overshoot white. The clamp is
 * the reason this exists rather than a bare multiplication: normalized_component would otherwise
 * throw on a bright accent scaled past 1.0.
 *
 * @param value The brightness to scale.
 * @param scale The factor to scale it by.
 * @return normalized_component The scaled brightness, never above 1.0.
 */
constexpr normalized_component brightened(const normalized_component value,
                                          const double scale) noexcept {
    return normalized_component{std::min(value.get() * scale, normalized_component::MAX())};
}

/**
 * @brief Rebuilds a colour from the accent's hue and saturation at the given brightness.
 *
 * @param accent The accent colour, in HSV.
 * @param value The brightness the result should have.
 * @return rendering::color The accent's hue and saturation at that brightness.
 */
rendering::color tinted(const rendering::hsv_color& accent,
                        const normalized_component value) noexcept {
    return rendering::from_hsv(rendering::hsv_color{
        .hue = accent.hue,
        .saturation = accent.saturation,
        .value = value,
    });
}

} // namespace

theme default_dark_theme() {
    // The engine's defaults *are* the dark theme: they live in one place (theming::defaults) so
    // that nothing has to restate them, this factory included.
    return theme{};
}

theme theme_from_accent_color(const rendering::color& accentColor) {
    const rendering::hsv_color accent{rendering::to_hsv(accentColor)};

    const normalized_component accentValue{std::max(accent.value.get(), MIN_ACCENT_VALUE.get())};
    const normalized_component hoveredValue{brightened(accentValue, HOVERED_VALUE_SCALE)};
    const normalized_component activeValue{brightened(accentValue, ACTIVE_VALUE_SCALE)};

    // Derived from the default theme rather than built from nothing, so the roles an accent colour
    // says nothing about (feedback colours, dimensions, typography) keep a sensible value instead
    // of being left unset.
    return default_dark_theme()
        .set(color_role::window_background, tinted(accent, BACKGROUND_VALUE))
        .set(color_role::surface, tinted(accent, BACKGROUND_VALUE))
        .set(color_role::surface_variant, tinted(accent, SECONDARY_BACKGROUND_VALUE))
        .set(color_role::surface_hovered, tinted(accent, hoveredValue))
        .set(color_role::surface_active, tinted(accent, activeValue))
        .set(color_role::accent, tinted(accent, accentValue))
        .set(color_role::accent_hovered, tinted(accent, hoveredValue))
        .set(color_role::accent_active, tinted(accent, activeValue))
        .set(color_role::on_accent, accentValue > DARK_TEXT_ON_ACCENT_THRESHOLD
                                        ? rendering::colors::BLACK
                                        : rendering::colors::WHITE)
        .set(color_role::outline, tinted(accent, OUTLINE_VALUE))
        .set(color_role::outline_variant, tinted(accent, SECONDARY_BACKGROUND_VALUE));
}

} // namespace tca::engine::theming
