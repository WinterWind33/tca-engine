// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <algorithm> // for std::clamp
#include <array>
#include <cstdint>

namespace tca::engine::rendering {

/**
 * @brief Represents a color in RGBA format, where each component is an 8-bit unsigned integer. The
 * alpha component (a) defaults to 255 (fully opaque) if not specified. This struct can be used to
 * represent colors in various rendering operations, such as clearing render targets or specifying
 * colors for drawing.
 */
struct color final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};

    constexpr bool operator==(const color& other) const noexcept = default;
};

/**
 * @brief Constructs a color from the given RGB components.
 *
 * @param r The red component of the color, in the range [0, 255].
 * @param g The green component of the color, in the range [0, 255].
 * @param b The blue component of the color, in the range [0, 255].
 * @return color The constructed color with the specified RGB components and an alpha value of 255
 * (fully opaque).
 */
constexpr color rgb(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b) {
    return color{r, g, b};
}

constexpr color rgba(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b,
                     const std::uint8_t a) {
    return color{r, g, b, a};
}

/**
 * @brief Normalizes an 8-bit color component to the range [0.0f, 1.0f] by dividing it by 255.0f.
 * This is useful for converting color components to a format suitable for APIs like DirectX 12,
 * which expect color components to be in the range [0.0f, 1.0f].
 *
 * @param component The 8-bit color component to normalize, in the range [0, 255].
 * @return constexpr float The normalized color component in the range [0.0f, 1.0f].
 */
constexpr float normalize_color_component(const std::uint8_t component) noexcept {
    return static_cast<float>(component) / 255.0f;
}

constexpr std::array<float, 4> to_normalized_float4(const color& color) noexcept {
    return {normalize_color_component(color.r), normalize_color_component(color.g),
            normalize_color_component(color.b), normalize_color_component(color.a)};
}

/**
 * @brief Converts a color to an array of four normalized floats in the range [0.0f, 1.0f], where
 * the RGB components are premultiplied by the alpha component. This format is suitable for use as a
 * clear color in DirectX 12, which expects clear colors to be specified in premultiplied alpha
 * format.
 *
 * @param color The color to convert.
 * @return constexpr std::array<float, 4> An array containing the normalized and premultiplied RGBA
 * components of the color.
 */
constexpr std::array<float, 4> to_premultiplied_normalized_float4(const color& color) noexcept {
    // We need to normalize the color components to the range [0.0f, 1.0f] and also premultiply the
    // RGB components by the alpha value, since DirectX 12 expects premultiplied alpha for clear
    // colors.
    return {
        normalize_color_component(color.r) * normalize_color_component(color.a),
        normalize_color_component(color.g) * normalize_color_component(color.a),
        normalize_color_component(color.b) * normalize_color_component(color.a),
        normalize_color_component(color.a),
    };
}

/**
 * @brief Applies a shading factor to the given color, effectively darkening or lightening it based
 * on the value of shadeFactor. A shadeFactor less than 1.0 will darken the color, while a
 * shadeFactor greater than 1.0 will lighten it. The alpha component of the color is preserved and
 * not affected by the shading factor.
 *
 * @param baseColor The original color to which the shading factor will be applied.
 * @param shadeFactor The factor by which to shade the color. A value less than 1.0 will darken the
 * color, while a value greater than 1.0 will lighten it.
 *
 * @return constexpr color The resulting color after applying the shading factor, with the same
 * alpha component as the original color.
 */
constexpr color shade(const color& baseColor, const double shadeFactor) noexcept {
    return color{
        static_cast<std::uint8_t>(std::clamp(baseColor.r * shadeFactor, 0.0, 255.0)),
        static_cast<std::uint8_t>(std::clamp(baseColor.g * shadeFactor, 0.0, 255.0)),
        static_cast<std::uint8_t>(std::clamp(baseColor.b * shadeFactor, 0.0, 255.0)),
        baseColor.a, // Preserve the alpha component
    };
}

namespace colors {
// Lower case because Winzoz API uses UPPER_CASE MACROS for colors, so we need to use a different
// naming convention to avoid compilers going crazy. Fuck this.
inline constexpr color Transparent{rgba(0, 0, 0, 0)};
inline constexpr color BLACK{};
inline constexpr color WHITE{rgb(255, 255, 255)};
inline constexpr color RED{rgb(255, 0, 0)};
inline constexpr color GREEN{rgb(0, 255, 0)};
inline constexpr color BLUE{rgb(0, 0, 255)};
inline constexpr color YELLOW{rgb(255, 255, 0)};
inline constexpr color CYAN{rgb(0, 255, 255)};
inline constexpr color MAGENTA{rgb(255, 0, 255)};
inline constexpr color SLATE_GRAY{rgb(112, 128, 144)};
inline constexpr color DIM_GRAY{rgb(105, 105, 105)};
inline constexpr color ROYAL_BLUE{rgb(65, 105, 225)};
inline constexpr color STEEL_BLUE{rgb(70, 130, 180)};
inline constexpr color DODGER_BLUE(rgb(30, 144, 255));
inline constexpr color MIDNIGHT_BLUE(rgb(25, 25, 112));
} // namespace colors

static_assert(shade(colors::RED, 0.5).r == 127);
static_assert(shade(colors::RED, 0.5).g == 0);
static_assert(shade(colors::RED, 0.5).b == 0);
static_assert(shade(colors::RED, 0.5).a == 255);

// Clamping should prevent the red component from exceeding 255, even with a shade factor greater
// than 1.0.
static_assert(shade(colors::RED, 1.0).r == 255);
static_assert(shade(colors::RED, 1.5).r == 255);

// Negative shade factors should darken the color, but the clamping should prevent the components
// from going below 0.
static_assert(shade(colors::RED, -0.5).r == 0);

} // namespace tca::engine::rendering
