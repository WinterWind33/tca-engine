// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/colors.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("Colors tests", "[tca][engine][rendering][colors]") {
    using namespace tca::engine::rendering;

    SECTION("Color constructor with RGBA values") {
        constexpr color c{255, 128, 0, 128};
        REQUIRE(c.r == 255);
        REQUIRE(c.g == 128);
        REQUIRE(c.b == 0);
        REQUIRE(c.a == 128);
    }

    SECTION("Color constructor with helper rgb function") {
        constexpr color c{rgb(76, 67, 0)};
        REQUIRE(c.r == 76);
        REQUIRE(c.g == 67);
        REQUIRE(c.b == 0);
        REQUIRE(c.a == 255); // Default alpha value
    }

    SECTION("Normalization of components") {
        SECTION("Normalize a color component") {
            REQUIRE_THAT(normalize_color_component(0), Catch::Matchers::WithinRel(0.0f));
            REQUIRE_THAT(normalize_color_component(128), Catch::Matchers::WithinRel(0.5019608f));
            REQUIRE_THAT(normalize_color_component(255), Catch::Matchers::WithinRel(1.0f));
        }
    }

    SECTION("Conversion to premultiplied normalized float4") {
        SECTION("Full opaque color") {
            constexpr color c{255, 128, 0, 255};
            const auto float4Color{to_premultiplied_normalized_float4(c)};
            REQUIRE_THAT(float4Color[0], Catch::Matchers::WithinRel(1.0f));
            REQUIRE_THAT(float4Color[1], Catch::Matchers::WithinRel(0.5019608f));
            REQUIRE_THAT(float4Color[2], Catch::Matchers::WithinRel(0.0f));
            REQUIRE_THAT(float4Color[3], Catch::Matchers::WithinRel(1.0f));
        }

        SECTION("Full transparent color") {
            constexpr color c{255, 128, 0, 0};
            const auto float4Color{to_premultiplied_normalized_float4(c)};
            REQUIRE_THAT(float4Color[0], Catch::Matchers::WithinRel(0.0f));
            REQUIRE_THAT(float4Color[1], Catch::Matchers::WithinRel(0.0f));
            REQUIRE_THAT(float4Color[2], Catch::Matchers::WithinRel(0.0f));
            REQUIRE_THAT(float4Color[3], Catch::Matchers::WithinRel(0.0f));
        }

        SECTION("Semi-transparent color") {
            constexpr color c{255, 128, 0, 128};
            const auto float4Color{to_premultiplied_normalized_float4(c)};
            REQUIRE_THAT(float4Color[0], Catch::Matchers::WithinRel(0.5019608f));
            REQUIRE_THAT(float4Color[1], Catch::Matchers::WithinRel(0.251964f));
            REQUIRE_THAT(float4Color[2], Catch::Matchers::WithinRel(0.0f));
            REQUIRE_THAT(float4Color[3], Catch::Matchers::WithinRel(0.5019608f));
        }
    }
}
