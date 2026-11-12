// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/texture-atlas.hpp"
#include "rendering/scene/sprite.hpp"

// A game names atlas regions and hands sprites to a painter without including an ImGui header,
// exactly like it places a scene surface: checked right after the sprite headers, before this file
// includes anything that pulls ImGui in.
#if defined(IMGUI_VERSION)
#error "rendering/scene/sprite.hpp and texture-atlas.hpp must not include an ImGui header"
#endif

// C++ STL
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Third-party
#include <Eigen/Geometry>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "core/math/vector.hpp"
#include "core/utilities/ranged-value.hpp"
#include "platform/images/image-decoder.hpp"
#include "rendering/colors.hpp"
#include "rendering/image-factory.hpp"
#include "rendering/image.hpp"
#include "rendering/theming/theme-roles.hpp"

// Test doubles
#include "test-doubles/rendering/rendering-queues/image-render-queue.test-double.hpp"

namespace {

using tca::engine::math::vector2;
using tca::engine::rendering::color;
using tca::engine::scene::alpha_value;
using tca::engine::scene::pixel_rect;
using tca::engine::scene::sprite_options;
using tca::engine::scene::sprite_region;
using tca::engine::scene::sprite_texture;
using tca::engine::scene::texture_atlas;
using tca::engine::scene::texture_id;
using tca::engine::scene::uv_rect;
using tca::engine::theming::color_role;

using Catch::Matchers::WithinAbs;

// UVs are ratios of small whole numbers, so they are exact to well within this.
constexpr double UV_EPSILON{1e-12};

constexpr texture_id ATLAS_TEXTURE_ID{0xA71A5};
constexpr std::uint32_t ATLAS_WIDTH{256};
constexpr std::uint32_t ATLAS_HEIGHT{128};

constexpr sprite_texture ATLAS_TEXTURE{
    .id = ATLAS_TEXTURE_ID,
    .width = ATLAS_WIDTH,
    .height = ATLAS_HEIGHT,
};

void require_uv(const uv_rect& actual, const double minU, const double minV, const double maxU,
                const double maxV) {
    REQUIRE_THAT(actual.min().x(), WithinAbs(minU, UV_EPSILON));
    REQUIRE_THAT(actual.min().y(), WithinAbs(minV, UV_EPSILON));
    REQUIRE_THAT(actual.max().x(), WithinAbs(maxU, UV_EPSILON));
    REQUIRE_THAT(actual.max().y(), WithinAbs(maxV, UV_EPSILON));
}

bool same_pixels(const pixel_rect& lhs, const pixel_rect& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width && lhs.height == rhs.height;
}

bool same_texture(const sprite_texture& lhs, const sprite_texture& rhs) {
    return lhs.id == rhs.id && lhs.width == rhs.width && lhs.height == rhs.height;
}

tca::engine::platform::images::decoded_image make_blank_image(const std::uint32_t width,
                                                              const std::uint32_t height) {
    return tca::engine::platform::images::decoded_image{
        .pixels = std::vector<std::byte>(static_cast<std::size_t>(width) * height * 4),
        .width = width,
        .height = height,
    };
}

} // namespace

// ================================= Compile-time contract =================================

static_assert(std::same_as<texture_id, std::uint64_t>,
              "an opaque, backend-neutral texture identifier wide enough for any ImTextureID");

static_assert(std::same_as<uv_rect, Eigen::AlignedBox2d>,
              "UV rectangles reuse Eigen's box type, like world_rect and screen_rect");

static_assert(std::is_aggregate_v<sprite_texture> && std::is_trivially_copyable_v<sprite_texture>,
              "a sprite texture is a plain view of an uploaded image: it owns nothing");
static_assert(std::is_aggregate_v<pixel_rect>);
static_assert(std::is_aggregate_v<sprite_region>);
static_assert(std::is_aggregate_v<sprite_options>);

static_assert(std::is_enum_v<tca::engine::scene::texture_filter>,
              "how sprites are sampled is a backend-neutral choice, named by the engine");

static_assert(std::is_copy_constructible_v<texture_atlas> &&
                  std::is_move_constructible_v<texture_atlas>,
              "an atlas only names regions of a texture it does not own, so it copies freely");

static_assert(
    std::same_as<decltype(std::declval<const texture_atlas&>().region(std::string_view{})),
                 sprite_region>,
    "regions are handed out by value, so nothing dangles when the atlas grows");

static_assert(
    std::same_as<decltype(std::declval<const tca::engine::rendering::image&>().as_sprite_texture()),
                 sprite_texture>,
    "an uploaded image is where a game gets a sprite texture from");

// ===================================== sprite_texture =====================================

TEST_CASE("sprite_texture", "[tca][engine][rendering][scene][sprites][sprite_texture]") {
    SECTION("A default sprite texture is empty") {
        REQUIRE(sprite_texture{}.empty());
    }

    SECTION("A texture with an identifier and a size is not empty") {
        REQUIRE_FALSE(ATLAS_TEXTURE.empty());
    }

    SECTION("A texture with no identifier is empty, whatever its size") {
        REQUIRE(sprite_texture{.id = 0, .width = 16, .height = 16}.empty());
    }

    SECTION("A texture with a zero width or height is empty") {
        REQUIRE(sprite_texture{.id = 7, .width = 0, .height = 16}.empty());
        REQUIRE(sprite_texture{.id = 7, .width = 16, .height = 0}.empty());
    }
}

// ====================================== sprite_region ======================================

TEST_CASE("sprite_region UVs", "[tca][engine][rendering][scene][sprites][sprite_region]") {
    SECTION("A region's UVs are its pixel bounds divided by the texture size") {
        const sprite_region region{
            .texture = ATLAS_TEXTURE,
            .pixels = pixel_rect{.x = 32, .y = 16, .width = 16, .height = 32},
        };

        // 32/256, 16/128, 48/256, 48/128.
        require_uv(region.uv(), 0.125, 0.125, 0.1875, 0.375);
    }

    SECTION("A region covering the whole texture spans UV 0 to 1") {
        const sprite_region region{
            .texture = ATLAS_TEXTURE,
            .pixels = pixel_rect{.x = 0, .y = 0, .width = ATLAS_WIDTH, .height = ATLAS_HEIGHT},
        };

        require_uv(region.uv(), 0.0, 0.0, 1.0, 1.0);
    }

    SECTION("A one-pixel region at the far corner ends exactly at UV 1") {
        const sprite_region region{
            .texture = ATLAS_TEXTURE,
            .pixels =
                pixel_rect{.x = ATLAS_WIDTH - 1, .y = ATLAS_HEIGHT - 1, .width = 1, .height = 1},
        };

        require_uv(region.uv(), 255.0 / 256.0, 127.0 / 128.0, 1.0, 1.0);
    }

    SECTION("A whole-texture region can be made straight from a texture") {
        const sprite_region region{sprite_region::whole(ATLAS_TEXTURE)};

        REQUIRE(same_texture(region.texture, ATLAS_TEXTURE));
        REQUIRE(
            same_pixels(region.pixels,
                        pixel_rect{.x = 0, .y = 0, .width = ATLAS_WIDTH, .height = ATLAS_HEIGHT}));
    }
}

// ====================================== texture_atlas ======================================

TEST_CASE("texture_atlas construction", "[tca][engine][rendering][scene][sprites][texture_atlas]") {
    SECTION("An atlas starts with no regions and keeps its texture") {
        const texture_atlas atlas{ATLAS_TEXTURE};

        REQUIRE(atlas.size() == 0);
        REQUIRE(atlas.empty());
        REQUIRE(same_texture(atlas.texture(), ATLAS_TEXTURE));
    }

    SECTION("An atlas over an empty texture is rejected") {
        const sprite_texture emptyTexture =
            GENERATE(sprite_texture{}, sprite_texture{.id = 0, .width = 8, .height = 8},
                     sprite_texture{.id = 1, .width = 0, .height = 8},
                     sprite_texture{.id = 1, .width = 8, .height = 0});
        CAPTURE(emptyTexture.id, emptyTexture.width, emptyTexture.height);

        REQUIRE_THROWS_AS(texture_atlas{emptyTexture}, std::invalid_argument);
    }
}

TEST_CASE("texture_atlas regions", "[tca][engine][rendering][scene][sprites][texture_atlas]") {
    texture_atlas atlas{ATLAS_TEXTURE};

    SECTION("An added region can be looked up by name") {
        const pixel_rect floorPixels{.x = 0, .y = 0, .width = 32, .height = 32};

        atlas.add_region("floor", floorPixels);

        REQUIRE(atlas.contains("floor"));
        REQUIRE(atlas.size() == 1);
        REQUIRE_FALSE(atlas.empty());

        const sprite_region floor{atlas.region("floor")};
        REQUIRE(same_pixels(floor.pixels, floorPixels));
        REQUIRE(same_texture(floor.texture, ATLAS_TEXTURE));
    }

    SECTION("add_region() hands back the region it added") {
        const pixel_rect wallPixels{.x = 32, .y = 0, .width = 32, .height = 64};

        const sprite_region wall{atlas.add_region("wall", wallPixels)};

        REQUIRE(same_pixels(wall.pixels, wallPixels));
        REQUIRE(same_texture(wall.texture, ATLAS_TEXTURE));
    }

    SECTION("A region draws the correct sub-rectangle: its UVs match its known pixel bounds") {
        // A 4x2 tileset of 64x64 tiles; the tile at column 2, row 1.
        atlas.add_region("desk", pixel_rect{.x = 128, .y = 64, .width = 64, .height = 64});

        require_uv(atlas.region("desk").uv(), 0.5, 0.5, 0.75, 1.0);
    }

    SECTION("Many regions share the atlas's one texture") {
        atlas.add_region("floor", pixel_rect{.x = 0, .y = 0, .width = 16, .height = 16});
        atlas.add_region("wall", pixel_rect{.x = 16, .y = 0, .width = 16, .height = 16});
        atlas.add_region("door", pixel_rect{.x = 32, .y = 0, .width = 16, .height = 16});

        REQUIRE(atlas.size() == 3);
        REQUIRE(atlas.region("floor").texture.id == ATLAS_TEXTURE_ID);
        REQUIRE(atlas.region("wall").texture.id == ATLAS_TEXTURE_ID);
        REQUIRE(atlas.region("door").texture.id == ATLAS_TEXTURE_ID);
    }

    SECTION("Regions may overlap: two names can address the same pixels") {
        atlas.add_region("bed", pixel_rect{.x = 0, .y = 0, .width = 32, .height = 16});
        atlas.add_region("bed-highlighted", pixel_rect{.x = 0, .y = 0, .width = 32, .height = 16});

        REQUIRE(atlas.size() == 2);
    }

    SECTION("A region may touch the texture's right and bottom edges") {
        const pixel_rect corner{
            .x = ATLAS_WIDTH - 16, .y = ATLAS_HEIGHT - 16, .width = 16, .height = 16};

        REQUIRE_NOTHROW(atlas.add_region("corner", corner));
        require_uv(atlas.region("corner").uv(), 240.0 / 256.0, 112.0 / 128.0, 1.0, 1.0);
    }

    SECTION("Lookup accepts any string-like name") {
        atlas.add_region(std::string{"floor"}, pixel_rect{.x = 0, .y = 0, .width = 8, .height = 8});

        const std::string owned{"floor"};
        constexpr std::string_view view{"floor"};

        REQUIRE(atlas.contains(owned));
        REQUIRE(atlas.contains(view));
        REQUIRE(atlas.contains("floor"));
        REQUIRE(same_pixels(atlas.region(view).pixels, atlas.region(owned).pixels));
    }

    SECTION("Names are case-sensitive") {
        atlas.add_region("Floor", pixel_rect{.x = 0, .y = 0, .width = 8, .height = 8});

        REQUIRE(atlas.contains("Floor"));
        REQUIRE_FALSE(atlas.contains("floor"));
    }

    SECTION("An unknown name is not contained and looking it up throws") {
        atlas.add_region("floor", pixel_rect{.x = 0, .y = 0, .width = 8, .height = 8});

        REQUIRE_FALSE(atlas.contains("ceiling"));
        REQUIRE_THROWS_AS(atlas.region("ceiling"), std::out_of_range);
    }

    SECTION("A region handed out stays valid after the atlas grows") {
        const sprite_region floor{
            atlas.add_region("floor", pixel_rect{.x = 0, .y = 0, .width = 8, .height = 8})};

        for (std::uint32_t index{0}; index < 64; ++index) {
            atlas.add_region(
                "tile-" + std::to_string(index),
                pixel_rect{.x = (index % 32) * 8, .y = (index / 32) * 8, .width = 8, .height = 8});
        }

        REQUIRE(same_pixels(floor.pixels, pixel_rect{.x = 0, .y = 0, .width = 8, .height = 8}));
        REQUIRE(same_pixels(atlas.region("floor").pixels, floor.pixels));
    }

    SECTION("A copied atlas keeps its regions and grows independently") {
        atlas.add_region("floor", pixel_rect{.x = 0, .y = 0, .width = 8, .height = 8});

        texture_atlas copy{atlas};
        copy.add_region("wall", pixel_rect{.x = 8, .y = 0, .width = 8, .height = 8});

        REQUIRE(copy.contains("floor"));
        REQUIRE(copy.contains("wall"));
        REQUIRE_FALSE(atlas.contains("wall"));
    }
}

TEST_CASE("texture_atlas rejects invalid regions",
          "[tca][engine][rendering][scene][sprites][texture_atlas]") {
    texture_atlas atlas{ATLAS_TEXTURE};
    atlas.add_region("floor", pixel_rect{.x = 0, .y = 0, .width = 16, .height = 16});

    // A rejected region must leave the atlas exactly as it was.
    auto requireUnchanged{[&atlas] {
        REQUIRE(atlas.size() == 1);
        REQUIRE(same_pixels(atlas.region("floor").pixels,
                            pixel_rect{.x = 0, .y = 0, .width = 16, .height = 16}));
    }};

    SECTION("A duplicate name is rejected, keeping the first region") {
        REQUIRE_THROWS_AS(
            atlas.add_region("floor", pixel_rect{.x = 16, .y = 16, .width = 8, .height = 8}),
            std::invalid_argument);
        requireUnchanged();
    }

    SECTION("An empty name is rejected") {
        REQUIRE_THROWS_AS(atlas.add_region("", pixel_rect{.x = 0, .y = 0, .width = 8, .height = 8}),
                          std::invalid_argument);
        requireUnchanged();
    }

    SECTION("A zero width or height is rejected") {
        REQUIRE_THROWS_AS(
            atlas.add_region("flat", pixel_rect{.x = 0, .y = 0, .width = 0, .height = 8}),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            atlas.add_region("thin", pixel_rect{.x = 0, .y = 0, .width = 8, .height = 0}),
            std::invalid_argument);
        requireUnchanged();
    }

    SECTION("A region reaching past the texture's right or bottom edge is rejected") {
        REQUIRE_THROWS_AS(
            atlas.add_region("wide",
                             pixel_rect{.x = ATLAS_WIDTH - 8, .y = 0, .width = 9, .height = 8}),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            atlas.add_region("tall",
                             pixel_rect{.x = 0, .y = ATLAS_HEIGHT - 8, .width = 8, .height = 9}),
            std::invalid_argument);
        requireUnchanged();
    }

    SECTION("A region starting outside the texture is rejected") {
        REQUIRE_THROWS_AS(
            atlas.add_region("outside",
                             pixel_rect{.x = ATLAS_WIDTH, .y = 0, .width = 1, .height = 1}),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            atlas.add_region("below",
                             pixel_rect{.x = 0, .y = ATLAS_HEIGHT, .width = 1, .height = 1}),
            std::invalid_argument);
        requireUnchanged();
    }

    SECTION("Bounds that would overflow when added up are rejected, not wrapped around") {
        constexpr std::uint32_t LARGEST{std::numeric_limits<std::uint32_t>::max()};

        REQUIRE_THROWS_AS(
            atlas.add_region("wrap-x", pixel_rect{.x = 8, .y = 0, .width = LARGEST, .height = 8}),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            atlas.add_region("wrap-y", pixel_rect{.x = 0, .y = LARGEST, .width = 8, .height = 2}),
            std::invalid_argument);
        requireUnchanged();
    }
}

// ================================ From an uploaded image ================================

TEST_CASE("rendering::image hands out a sprite_texture",
          "[tca][engine][rendering][scene][sprites][image]") {
    tca::engine::rendering::test_doubles::image_render_queue_fake queue{};
    const tca::engine::rendering::image_factory factory{queue};

    SECTION("It carries the uploaded texture's identifier and the image's size") {
        const tca::engine::rendering::image atlasImage{
            factory.create_from_pixels(make_blank_image(64, 32))};

        const sprite_texture texture{atlasImage.as_sprite_texture()};

        REQUIRE_FALSE(texture.empty());
        REQUIRE(texture.width == 64);
        REQUIRE(texture.height == 32);
    }

    SECTION("Two images hand out two different textures") {
        const tca::engine::rendering::image first{
            factory.create_from_pixels(make_blank_image(8, 8))};
        const tca::engine::rendering::image second{
            factory.create_from_pixels(make_blank_image(8, 8))};

        REQUIRE(first.as_sprite_texture().id != second.as_sprite_texture().id);
    }

    SECTION("An empty image hands out an empty texture") {
        const tca::engine::rendering::image emptyImage{};

        REQUIRE(emptyImage.as_sprite_texture().empty());
    }

    SECTION("A moved-from image hands out an empty texture, the moved-to one keeps it") {
        tca::engine::rendering::image source{factory.create_from_pixels(make_blank_image(8, 4))};
        const sprite_texture before{source.as_sprite_texture()};

        const tca::engine::rendering::image destination{std::move(source)};

        REQUIRE(source.as_sprite_texture().empty()); // NOLINT(bugprone-use-after-move)
        REQUIRE(same_texture(destination.as_sprite_texture(), before));
    }

    SECTION("An atlas can be built straight over an uploaded image") {
        const tca::engine::rendering::image atlasImage{
            factory.create_from_pixels(make_blank_image(128, 64))};
        texture_atlas atlas{atlasImage.as_sprite_texture()};

        atlas.add_region("chair", pixel_rect{.x = 64, .y = 32, .width = 32, .height = 32});

        require_uv(atlas.region("chair").uv(), 0.5, 0.5, 0.75, 1.0);
    }
}

// ===================================== sprite_options =====================================

TEST_CASE("sprite_options defaults", "[tca][engine][rendering][scene][sprites][sprite_options]") {
    SECTION("A sprite is untinted by default") {
        REQUIRE_FALSE(sprite_options{}.tint.has_value());
    }

    SECTION("A sprite draws on layer 0 by default, the layer shapes draw on") {
        REQUIRE(sprite_options{}.layer == 0);
    }

    SECTION("A tint is a theme colour role, not a raw colour") {
        static_assert(std::same_as<decltype(sprite_options::tint), std::optional<color_role>>);
        REQUIRE(sprite_options{.tint = color_role::error}.tint == color_role::error);
    }
    SECTION("A sprite has no explicit tint colour by default") {
        static_assert(std::same_as<decltype(sprite_options::tint_color), std::optional<color>>);
        REQUIRE_FALSE(sprite_options{}.tint_color.has_value());
    }

    SECTION("A sprite is fully opaque by default") {
        REQUIRE(static_cast<double>(sprite_options{}.alpha) == 1.0);
    }

    SECTION("A sprite is unflipped, unscaled and unrotated about its top-left corner by default") {
        const sprite_options defaults{};
        REQUIRE(defaults.origin == vector2::zero());
        REQUIRE(defaults.scale == vector2(1.0, 1.0));
        REQUIRE(defaults.rotation == 0.0);
    }
}

TEST_CASE("alpha_value", "[tca][engine][rendering][scene][sprites][sprite_options][alpha]") {
    SECTION("Alpha is limited to [0, 1], both ends included") {
        static_assert(std::same_as<alpha_value, tca::engine::core::ranged_value<double, 0.0, 1.0>>);
        REQUIRE(static_cast<double>(alpha_value{0.0}) == 0.0);
        REQUIRE(static_cast<double>(alpha_value{1.0}) == 1.0);
    }

    SECTION("An alpha outside [0, 1], or NaN, is rejected where it is made") {
        REQUIRE_THROWS_AS(alpha_value{1.5}, tca::engine::core::ranged_value_exception);
        REQUIRE_THROWS_AS(alpha_value{-0.1}, tca::engine::core::ranged_value_exception);
        REQUIRE_THROWS_AS(alpha_value{std::numeric_limits<double>::quiet_NaN()},
                          tca::engine::core::ranged_value_exception);
        REQUIRE_THROWS_AS(sprite_options{.alpha = 2.0}, tca::engine::core::ranged_value_exception);
    }
}
