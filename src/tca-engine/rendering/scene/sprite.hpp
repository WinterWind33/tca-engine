// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <optional>

// Third-party
#include <Eigen/Geometry>

// Engine
#include "core/math/vector.hpp"
#include "core/utilities/ranged-value.hpp"
#include "rendering/colors.hpp"
#include "rendering/theming/theme-roles.hpp"

namespace tca::engine::scene {

/**
 * @brief An opaque, backend-neutral identifier of a texture uploaded to the GPU.
 *
 * Wide enough to hold any ImTextureID, so a game can name textures without including an ImGui
 * header. 0 means "no texture".
 */
using texture_id = std::uint64_t;

/**
 * @brief Invalid texture ID.
 */
constexpr texture_id NO_TEXTURE_ID{};

/**
 * @brief An axis-aligned rectangle in normalised texture coordinates: (0, 0) is the texture's
 *  top-left corner and (1, 1) its bottom-right one.
 */
using uv_rect = Eigen::AlignedBox2d;

/**
 * @brief How a surface samples its sprites' textures when they are drawn larger or smaller than
 *  their pixel size.
 *
 * Only affects sampling: a sprite's geometry and UVs are the same whatever the filter. Shapes, the
 * background and the grid are always sampled linearly, since anti-aliased lines rely on it.
 */
enum class texture_filter : std::uint8_t {
    /**
     * @brief Each screen pixel takes the colour of the closest texel: crisp, blocky pixel art.
     *  The default.
     */
    nearest,

    /**
     * @brief Each screen pixel blends the closest texels: smooth, for non-pixel-art sprites.
     *
     * Blending reaches half a texel past a region's edge, so a sprite cut from an atlas picks up
     * the neighbouring region's texels along its border. Leave a gap between regions, or extrude
     * each region's edge texels into it, when drawing an atlas with this filter.
     */
    linear,
};

/**
 * @brief How what a scene draws combines with what is already on screen.
 */
enum class blend_mode : std::uint8_t {
    /**
     * @brief Alpha blending: the colour covers what is underneath by its alpha. The default.
     */
    normal,

    /**
     * @brief Additive blending: the colour, weighted by its alpha, is added to what is underneath,
     *  so it only ever brightens it and overlapping draws accumulate. For fire, light and glows.
     */
    additive,
};

/**
 * @brief A plain view of a texture uploaded to the GPU: its identifier and its size in pixels.
 *
 * Owns nothing: the texture lives as long as whatever uploaded it (typically a rendering::image,
 * see rendering::image::as_sprite_texture()), which must outlive every draw that uses this view.
 */
struct sprite_texture {
    /**
     * @brief The texture's identifier, or 0 if there is no texture.
     */
    texture_id id{};

    /**
     * @brief The texture's width, in pixels.
     */
    std::uint32_t width{};

    /**
     * @brief The texture's height, in pixels.
     */
    std::uint32_t height{};

    /**
     * @brief Tells whether there is nothing to draw from this texture.
     *
     * @return true if the texture has no identifier, or a zero width or height.
     */
    constexpr bool empty() const noexcept {
        return id == NO_TEXTURE_ID || width == 0 || height == 0;
    }
};

/**
 * @brief An axis-aligned rectangle in whole texels, measured from a texture's top-left corner,
 *  x to the right and y down.
 */
struct pixel_rect {
    /**
     * @brief The column of the rectangle's left edge.
     */
    std::uint32_t x{};

    /**
     * @brief The row of the rectangle's top edge.
     */
    std::uint32_t y{};

    /**
     * @brief The rectangle's width, in texels.
     */
    std::uint32_t width{};

    /**
     * @brief The rectangle's height, in texels.
     */
    std::uint32_t height{};
};

/**
 * @brief A sub-rectangle of a texture to draw as a sprite: a single tile or frame of an atlas, or
 *  the whole texture.
 *
 * A plain value, like sprite_texture: it owns nothing and is safe to keep after the texture_atlas
 * that handed it out changes or goes away, as long as the texture itself is still alive.
 */
struct sprite_region {
    /**
     * @brief The texture the region is cut from.
     */
    sprite_texture texture{};

    /**
     * @brief The region's bounds within the texture, in texels.
     */
    pixel_rect pixels{};

    /**
     * @brief The region's bounds in texture coordinates: its pixel bounds divided by the texture's
     *  size.
     *
     * Not flipped: the region's top-left texel maps to the minimum corner. A region touching the
     * texture's right and bottom edges ends at exactly (1, 1).
     *
     * @return uv_rect The region's UV rectangle.
     */
    uv_rect uv() const;

    /**
     * @brief Makes the region covering a whole texture, spanning UV (0, 0) to (1, 1).
     *
     * @param texture The texture to cover.
     * @return sprite_region The region at (0, 0), as large as the texture.
     */
    static sprite_region whole(const sprite_texture& texture);
};

/**
 * @brief An opacity multiplier: 0 is fully transparent, 1 leaves the colour's own alpha as it is.
 */
using alpha_value = core::ranged_value<double, 0.0, 1.0>;

/**
 * @brief How a single sprite is drawn. The defaults draw it untinted, unflipped and unrotated,
 *  exactly over its world rectangle, on the layer shapes draw on.
 */
struct sprite_options {
    /**
     * @brief The layer the sprite draws on. Higher layers draw over lower ones, whatever the order
     *  they are asked for in; within one layer, sprites draw in call order.
     *
     * Shapes draw on layer 0, so a sprite on a negative layer draws beneath them and one on a
     * positive layer over them. The surface's background and grid stay beneath every layer.
     * Layers only order drawing within the current frame.
     */
    std::int32_t layer{};

    /**
     * @brief The theme colour role the texture's colours are multiplied by, or nullopt to draw
     *  the texture's own colours.
     *
     * Resolved through the active theme when the sprite is drawn, so it follows a theme swap. Its
     * alpha carries through: a translucent role draws a see-through sprite, e.g. a ghosted
     * placement preview. Only the colour changes: geometry and UVs are those of the untinted
     * sprite.
     */
    std::optional<theming::color_role> tint{};

    /**
     * @brief An explicit colour the texture's colours are multiplied by, or nullopt to fall back on
     *  tint.
     *
     * Game data rather than a theme role (red because the enemy was just hit), so a theme swap
     * leaves it alone. An alternative to tint: when both are set, this one is used. Its alpha
     * carries through like a role's.
     */
    std::optional<rendering::color> tint_color{};

    /**
     * @brief Multiplies the alpha of whatever colour the sprite is drawn with (white when
     *  untinted), rounded to the nearest 8-bit value. 0 draws nothing visible.
     */
    alpha_value alpha{1.0};

    /**
     * @brief The pivot the sprite is scaled and rotated about, in world units from the world
     *  rectangle's minimum (top-left) corner. Need not lie inside the rectangle.
     *
     * On its own it moves nothing: the pivot's world point is the one point a flip, a scale or a
     * rotation keeps fixed on screen, e.g. a character's feet or a weapon's grip.
     */
    math::vector2 origin{};

    /**
     * @brief Scales the sprite about its origin, per axis. A negative component flips the sprite on
     *  that axis (by swapping its UVs); a zero one, or a non-finite one, draws nothing.
     */
    math::vector2 scale{1.0, 1.0};

    /**
     * @brief Turns the sprite about its origin, in degrees, applied after the scale. Positive
     *  angles turn clockwise on screen, since y points down. A non-finite angle draws nothing.
     *
     * Whole quarter turns keep the sprite axis-aligned and snapped to whole pixels like an
     * unrotated one; any other angle draws a rotated quad.
     */
    double rotation{};
};

} // namespace tca::engine::scene
