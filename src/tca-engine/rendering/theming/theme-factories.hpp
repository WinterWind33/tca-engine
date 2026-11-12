// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Engine
#include "rendering/colors.hpp"
#include "rendering/theming/theme.hpp"

namespace tca::engine::theming {

/**
 * @brief The engine's built-in dark theme: a near-black backdrop with a steel blue accent, with
 * every built-in role populated. This is what a default-constructed theme holds, and what the
 * application starts with when a game never sets a theme of its own.
 *
 * @return theme The engine's default dark theme.
 */
theme default_dark_theme();

/**
 * @brief Derives a whole theme from a single accent colour, so an application can take its look
 * from something sampled at runtime (typically the colour dominating the player's wallpaper).
 *
 * Backgrounds and outlines keep the accent's hue and saturation but take fixed brightness levels
 * rather than a shade proportional to the accent's own, so the result reads consistently dark
 * regardless of how dark the sampled colour happens to be, while still being visibly tinted by
 * its hue. Interactive elements use the accent itself, never dropping below a minimum brightness,
 * so an accent sampled from a night sky still yields legible, colourful buttons rather than a
 * near-black theme; the hover and active variants brighten it further, clamped so they cannot
 * overshoot white.
 *
 * The result is an ordinary theme: it can be derived from, stored, compared and applied to any
 * backend exactly like a hand-written one. Roles the accent says nothing about (the feedback
 * colours, the dimensions, the typography) keep their default_dark_theme() values.
 *
 * @param accentColor The colour driving the derived palette.
 * @return theme A complete theme tinted by accentColor.
 */
theme theme_from_accent_color(const rendering::color& accentColor);

} // namespace tca::engine::theming
