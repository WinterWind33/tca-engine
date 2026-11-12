// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>

// Engine
#include "rendering/scene/sprite.hpp"

namespace tca::engine::scene {

/**
 * @brief Names regions of a single texture, so a game draws "floor" or "wall" instead of juggling
 *  pixel coordinates.
 *
 * The atlas only names regions of a texture it does not own: it copies freely, and a copy grows
 * independently of the original. Regions are handed out by value, so a region stays valid however
 * the atlas changes afterwards. Sprites drawn from one atlas share a texture, so consecutive ones
 * on the same layer need no texture switch.
 *
 * Names are case-sensitive and may overlap: two names can address the same pixels.
 */
class texture_atlas {
public:
    /**
     * @brief Constructs an atlas with no regions over a texture.
     *
     * @param texture The texture the regions are cut from. It must outlive every draw of a region
     *  handed out by this atlas.
     * @throws std::invalid_argument if the texture is empty (see sprite_texture::empty()).
     */
    explicit texture_atlas(sprite_texture texture);

    /**
     * @brief Retrieves the texture the regions are cut from.
     *
     * @return const sprite_texture& The atlas's texture.
     */
    const sprite_texture& texture() const noexcept;

    /**
     * @brief Names a region of the atlas's texture.
     *
     * The region may touch the texture's right and bottom edges, but not reach past them.
     *
     * @param name The region's name. Must not be empty or already used in this atlas.
     * @param pixels The region's bounds within the texture, in texels.
     * @return sprite_region The region just added.
     * @throws std::invalid_argument if the name is empty or already used (the existing region is
     *  kept), if the region has a zero width or height, or if it does not lie entirely within the
     *  texture. Bounds that would overflow when added up are rejected, not wrapped around.
     */
    sprite_region add_region(std::string name, const pixel_rect& pixels);

    /**
     * @brief Looks a region up by name.
     *
     * @param name The region's name, as given to add_region(). Case-sensitive.
     * @return sprite_region The region, by value.
     * @throws std::out_of_range if no region has that name.
     */
    sprite_region region(std::string_view name) const;

    /**
     * @brief Tells whether a region has a given name.
     *
     * @param name The name to look for. Case-sensitive.
     * @return true if add_region() added a region with that name.
     */
    bool contains(std::string_view name) const;

    /**
     * @brief Retrieves the number of named regions.
     *
     * @return std::size_t The number of regions added so far.
     */
    std::size_t size() const noexcept;

    /**
     * @brief Tells whether the atlas has no regions yet.
     *
     * @return true if no region was added.
     */
    bool empty() const noexcept;

private:
    sprite_texture m_texture{};

    // Transparent comparator: lookups by std::string_view build no temporary std::string.
    std::map<std::string, pixel_rect, std::less<>> m_regions{};
};

} // namespace tca::engine::scene
