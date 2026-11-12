// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/texture-atlas.hpp"

// C++ STL
#include <format>
#include <stdexcept>
#include <utility>

#include "rendering/scene/sprite.hpp"

namespace tca::engine::scene {

namespace {

bool is_within_texture_bounds(const sprite_texture& texture, const pixel_rect& rect) noexcept {
    // Compare the size against the room left after the start rather than adding start and size:
    // the subtraction can't underflow once the start is known to be inside, and nothing wraps.
    return rect.x < texture.width && rect.width <= texture.width - rect.x &&
           rect.y < texture.height && rect.height <= texture.height - rect.y;
}

} // namespace

texture_atlas::texture_atlas(sprite_texture texture) : m_texture{texture} {
    if (m_texture.empty()) {
        throw std::invalid_argument{"Sprite texture is empty"};
    }
}

const sprite_texture& texture_atlas::texture() const noexcept {
    return m_texture;
}

sprite_region texture_atlas::add_region(std::string name, const pixel_rect& pixels) {
    if (name.empty()) {
        throw std::invalid_argument{"Adding a sprite region without a valid name"};
    }

    if (pixels.width == 0 || pixels.height == 0) {
        throw std::invalid_argument{
            std::format("Sprite region '{}' must have a non-zero width and height", name)};
    }

    // If the region tries to reach out of the texture bounds, it's not valid.
    if (!is_within_texture_bounds(m_texture, pixels)) {
        throw std::invalid_argument{
            std::format("Sprite region '{}' reaches outside the {}x{} texture", name,
                        m_texture.width, m_texture.height)};
    }

    // Every check that can reject the region runs before this, so a rejection leaves the atlas
    // unchanged. A failed insertion means the name is taken, and the first region is kept.
    const auto [it, inserted]{m_regions.try_emplace(std::move(name), pixels)};
    if (!inserted) {
        throw std::invalid_argument{
            std::format("A sprite region named '{}' already exists", it->first)};
    }

    return sprite_region{
        .texture = m_texture,
        .pixels = it->second,
    };
}

sprite_region texture_atlas::region(const std::string_view name) const {
    const auto it{m_regions.find(name)};
    if (it == m_regions.end()) {
        throw std::out_of_range{
            std::format("Could not find a sprite region with the given name '{}'", name)};
    }

    return sprite_region{.texture = m_texture, .pixels = it->second};
}

bool texture_atlas::contains(const std::string_view name) const {
    return m_regions.contains(name);
}

std::size_t texture_atlas::size() const noexcept {
    return m_regions.size();
}

bool texture_atlas::empty() const noexcept {
    return m_regions.empty();
}

} // namespace tca::engine::scene
