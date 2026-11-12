// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/theming/theme.hpp"

// C++ STL
#include <cstddef>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "rendering/colors.hpp"

TEST_CASE("theme class tests", "[tca][engine][rendering][theming][theme]") {
    using namespace tca::engine;
    using namespace tca::engine::theming;

    SECTION("Resolving a built-in role is total") {
        const theme defaultTheme{};

        THEN("Every built-in color role resolves to something visible") {
            for (std::size_t roleIndex{}; roleIndex < COLOR_ROLE_COUNT; ++roleIndex) {
                // Nothing is left at the transparent sentinel the default table is built from, so
                // no widget can resolve a role and get nothing at all.
                REQUIRE(defaultTheme.resolve(static_cast<color_role>(roleIndex)) !=
                        rendering::colors::Transparent);
            }
        }

        THEN("Every built-in dimension role resolves to a non-negative value") {
            for (std::size_t roleIndex{}; roleIndex < DIMENSION_ROLE_COUNT; ++roleIndex) {
                REQUIRE(defaultTheme.resolve(static_cast<dimension_role>(roleIndex)) >= 0.0);
            }
        }

        THEN("Every built-in typography role resolves to a positive scale") {
            for (std::size_t roleIndex{}; roleIndex < TYPOGRAPHY_ROLE_COUNT; ++roleIndex) {
                REQUIRE(defaultTheme.resolve(static_cast<typography_role>(roleIndex)).scale > 0.0);
            }
        }
    }

    SECTION("set() overrides a role, and nothing else") {
        theme customized{};
        const theme original{};

        customized.set(color_role::accent, rendering::colors::MAGENTA);

        REQUIRE(customized.resolve(color_role::accent) == rendering::colors::MAGENTA);
        REQUIRE(customized.resolve(color_role::on_surface) ==
                original.resolve(color_role::on_surface));
    }

    SECTION("set() overloads are chainable") {
        const theme customized{theme{}
                                   .set(color_role::accent, rendering::colors::MAGENTA)
                                   .set(dimension_role::corner_radius, 12.0)
                                   .set(typography_role::heading, typography{.scale = 2.0})};

        REQUIRE(customized.resolve(color_role::accent) == rendering::colors::MAGENTA);
        REQUIRE_THAT(customized.resolve(dimension_role::corner_radius),
                     Catch::Matchers::WithinAbs(12.0, 0.001));
        REQUIRE(customized.resolve(typography_role::heading) == typography{.scale = 2.0});
    }

    SECTION("A typography scale that is not a scale is rejected") {
        REQUIRE_THROWS_AS((typography{.scale = -1.0}), core::ranged_value_exception);
    }

    GIVEN("A theme derived from a base one") {
        const theme base{theme{}
                             .set(color_role::accent, rendering::colors::MAGENTA)
                             .set(dimension_role::corner_radius, 12.0)};

        const theme variant{
            base.derive().set(color_role::window_background, rendering::colors::BLACK)};

        THEN("The variant only restates what actually differs") {
            REQUIRE(variant.resolve(color_role::window_background) == rendering::colors::BLACK);
            REQUIRE(variant.resolve(color_role::accent) == rendering::colors::MAGENTA);
            REQUIRE_THAT(variant.resolve(dimension_role::corner_radius),
                         Catch::Matchers::WithinAbs(12.0, 0.001));
        }

        THEN("Deriving does not touch the theme it was derived from") {
            REQUIRE(base.resolve(color_role::window_background) ==
                    theme{}.resolve(color_role::window_background));
        }

        THEN("A theme derived with no override compares equal to its base") {
            REQUIRE(base.derive() == base);
        }
    }

    GIVEN("A theme carrying roles the engine has never heard of") {
        const theme extended{theme{}
                                 .set_custom_color("ecg_trace", rendering::colors::GREEN)
                                 .set_custom_dimension("chart_line_width", 2.5)
                                 .set_custom_typography("patient_name", typography{.scale = 1.6})};

        THEN("They resolve to what the game defined") {
            REQUIRE(extended.resolve_custom_color("ecg_trace") == rendering::colors::GREEN);
            REQUIRE(extended.resolve_custom_dimension("chart_line_width") == 2.5);
            REQUIRE(extended.resolve_custom_typography("patient_name") == typography{.scale = 1.6});
        }

        THEN("A role nobody defined resolves to nothing rather than to a wrong value") {
            REQUIRE_FALSE(extended.resolve_custom_color("never_defined").has_value());
        }

        THEN("A custom role can fall back to a built-in one, keeping call sites total") {
            REQUIRE(extended.resolve_custom_color_or("ecg_trace", color_role::accent) ==
                    rendering::colors::GREEN);
            REQUIRE(extended.resolve_custom_color_or("never_defined", color_role::accent) ==
                    extended.resolve(color_role::accent));
            REQUIRE_THAT(
                extended.resolve_custom_dimension_or("never_defined", dimension_role::indent),
                Catch::Matchers::WithinAbs(extended.resolve(dimension_role::indent), 0.001));
            REQUIRE(extended.resolve_custom_typography_or("never_defined", typography_role::body) ==
                    extended.resolve(typography_role::body));
        }

        THEN("Custom roles are inherited by a derived theme") {
            const theme variant{
                extended.derive().set_custom_color("ecg_trace", rendering::colors::RED)};

            REQUIRE(variant.resolve_custom_dimension("chart_line_width") == 2.5);
            REQUIRE(variant.resolve_custom_color("ecg_trace") == rendering::colors::RED);
            REQUIRE(extended.resolve_custom_color("ecg_trace") == rendering::colors::GREEN);
        }
    }
}
