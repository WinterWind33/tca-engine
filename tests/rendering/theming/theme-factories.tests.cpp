// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/theming/theme-factories.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "rendering/color-conversions.hpp"
#include "rendering/colors.hpp"

namespace {

// The brightness below which an accent-derived interactive element would stop being legible. The
// factory clamps to it, so nothing it produces for those roles may sit under it.
constexpr double MIN_ACCENT_VALUE{0.55};

double brightness_of(const tca::engine::rendering::color& sourceColor) {
    return tca::engine::rendering::to_hsv(sourceColor).value.get();
}

} // namespace

TEST_CASE("theme_from_accent_color() function tests",
          "[tca][engine][rendering][theming][theme_factories]") {
    using namespace tca::engine;
    using namespace tca::engine::theming;

    SECTION("default_dark_theme() is what a default-constructed theme holds") {
        REQUIRE(default_dark_theme() == theme{});
    }

    GIVEN("An accent color that is very dark, as one sampled from a night sky would be") {
        constexpr rendering::color DARK_ACCENT{rendering::rgb(8, 10, 40)};
        const theme derived{theme_from_accent_color(DARK_ACCENT)};

        THEN("Interactive elements are still brightened to a legible level") {
            REQUIRE(brightness_of(derived.resolve(color_role::accent)) >= MIN_ACCENT_VALUE - 0.01);
            REQUIRE(brightness_of(derived.resolve(color_role::accent_hovered)) >=
                    brightness_of(derived.resolve(color_role::accent)));
            REQUIRE(brightness_of(derived.resolve(color_role::accent_active)) >=
                    brightness_of(derived.resolve(color_role::accent_hovered)));
        }

        THEN("Backgrounds stay dark regardless of how dark the accent was") {
            REQUIRE(brightness_of(derived.resolve(color_role::window_background)) < 0.3);
            REQUIRE(brightness_of(derived.resolve(color_role::surface_variant)) < 0.4);
        }
    }

    GIVEN("An accent color that is already at full brightness") {
        constexpr rendering::color BRIGHT_ACCENT{rendering::colors::YELLOW};
        const theme derived{theme_from_accent_color(BRIGHT_ACCENT)};

        THEN("Brightening for hover and active clamps at white instead of overshooting") {
            REQUIRE_THAT(brightness_of(derived.resolve(color_role::accent_hovered)),
                         Catch::Matchers::WithinAbs(1.0, 0.01));
            REQUIRE_THAT(brightness_of(derived.resolve(color_role::accent_active)),
                         Catch::Matchers::WithinAbs(1.0, 0.01));
        }

        THEN("Text laid on the accent goes dark, since the accent itself is light") {
            REQUIRE(derived.resolve(color_role::on_accent) == rendering::colors::BLACK);
        }
    }

    SECTION("The derived theme keeps the hue of the accent it came from") {
        constexpr rendering::color ACCENT{rendering::colors::MAGENTA};
        const theme derived{theme_from_accent_color(ACCENT)};

        REQUIRE_THAT(rendering::to_hsv(derived.resolve(color_role::accent)).hue.get(),
                     Catch::Matchers::WithinAbs(rendering::to_hsv(ACCENT).hue.get(), 0.01));
        REQUIRE_THAT(rendering::to_hsv(derived.resolve(color_role::surface)).hue.get(),
                     Catch::Matchers::WithinAbs(rendering::to_hsv(ACCENT).hue.get(), 0.01));
    }

    SECTION("Roles the accent says nothing about keep their defaults") {
        const theme derived{theme_from_accent_color(rendering::colors::STEEL_BLUE)};
        const theme defaultTheme{};

        REQUIRE(derived.resolve(color_role::error) == defaultTheme.resolve(color_role::error));
        REQUIRE(derived.resolve(typography_role::heading) ==
                defaultTheme.resolve(typography_role::heading));
        REQUIRE_THAT(
            derived.resolve(dimension_role::corner_radius),
            Catch::Matchers::WithinAbs(defaultTheme.resolve(dimension_role::corner_radius), 0.001));
    }
}
