// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/sprite.hpp"
#include "core/math/vector.hpp"

namespace tca::engine::scene {

uv_rect sprite_region::uv() const {
    return uv_rect{
        math::vector2{static_cast<double>(pixels.x) / texture.width,
                      static_cast<double>(pixels.y) / texture.height                  },
        math::vector2{(static_cast<double>(pixels.x) + pixels.width) / texture.width,
                      (static_cast<double>(pixels.y) + pixels.height) / texture.height}
    };
}

sprite_region sprite_region::whole(const sprite_texture& texture) {
    return sprite_region{
        .texture = texture,
        .pixels =
            pixel_rect{
                       .width = texture.width,
                       .height = texture.height,
                       },
    };
}

} // namespace tca::engine::scene
