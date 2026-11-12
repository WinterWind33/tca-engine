// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/color-conversions.hpp"

// C++ STL
#include <array>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

using tca::engine::rendering::color;
using tca::engine::rendering::colors::BLACK;
using tca::engine::rendering::colors::WHITE;

} // namespace

TEST_CASE("to_hsv() and from_hsv() functions tests",
          "[tca][engine][rendering][colors][color_conversions]") {
    using namespace tca::engine::rendering;

    SECTION("Primary colors convert to the hue they sit at on the color wheel") {
        // Each primary is a sixth of the way around the wheel, fully saturated and fully bright.
        REQUIRE_THAT(to_hsv(colors::RED).hue.get(), Catch::Matchers::WithinAbs(0.0, 0.001));
        REQUIRE_THAT(to_hsv(colors::GREEN).hue.get(), Catch::Matchers::WithinAbs(1.0 / 3.0, 0.001));
        REQUIRE_THAT(to_hsv(colors::BLUE).hue.get(), Catch::Matchers::WithinAbs(2.0 / 3.0, 0.001));

        REQUIRE_THAT(to_hsv(colors::RED).saturation.get(), Catch::Matchers::WithinAbs(1.0, 0.001));
        REQUIRE_THAT(to_hsv(colors::RED).value.get(), Catch::Matchers::WithinAbs(1.0, 0.001));
    }

    SECTION("A grey has no saturation, and black has no value either") {
        const hsv_color grey{to_hsv(rgb(128, 128, 128))};
        REQUIRE_THAT(grey.saturation.get(), Catch::Matchers::WithinAbs(0.0, 0.001));

        const hsv_color black{to_hsv(BLACK)};
        REQUIRE_THAT(black.value.get(), Catch::Matchers::WithinAbs(0.0, 0.001));
        REQUIRE_THAT(black.saturation.get(), Catch::Matchers::WithinAbs(0.0, 0.001));
    }

    SECTION("Alpha is carried through untouched, in both directions") {
        constexpr color TRANSLUCENT{rgba(10, 200, 60, 42)};
        REQUIRE(to_hsv(TRANSLUCENT).alpha == 42);
        REQUIRE(from_hsv(to_hsv(TRANSLUCENT)).a == 42);
    }

    SECTION("Converting to HSV and back returns the original color") {
        // Spread over every sector of the color wheel, plus the achromatic ends, since the sector
        // arithmetic in from_hsv() takes a different branch for each.
        constexpr std::array ROUND_TRIPPED{
            colors::RED,
            colors::GREEN,
            colors::BLUE,
            colors::YELLOW,
            colors::CYAN,
            colors::MAGENTA,
            colors::STEEL_BLUE,
            colors::MIDNIGHT_BLUE,
            colors::SLATE_GRAY,
            colors::DIM_GRAY,
            WHITE,
            BLACK,
        };

        for (const color& original : ROUND_TRIPPED) {
            REQUIRE(from_hsv(to_hsv(original)) == original);
        }
    }

    SECTION("A hue of exactly 1.0 is the same red as a hue of 0.0") {
        const hsv_color fullyRound{.hue = 1.0, .saturation = 1.0, .value = 1.0, .alpha = 255};
        REQUIRE(from_hsv(fullyRound) == colors::RED);
    }

    SECTION("Components outside [0, 1] are rejected where they are built") {
        REQUIRE_THROWS_AS((hsv_color{.hue = 1.5}), tca::engine::core::ranged_value_exception);
        REQUIRE_THROWS_AS((hsv_color{.value = -0.1}), tca::engine::core::ranged_value_exception);
    }
}
