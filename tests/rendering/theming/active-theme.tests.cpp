// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/theming/active-theme.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "rendering/colors.hpp"

TEST_CASE("Active theme access tests", "[tca][engine][rendering][theming][active_theme]") {
    using namespace tca::engine;
    using namespace tca::engine::theming;

    // Every case below installs its own theme, so the surrounding one is restored on the way out
    // and test cases cannot leak a theme into each other.
    const scoped_active_theme isolation{get_active_theme()};

    SECTION("There is always an active theme, even before anything sets one") {
        REQUIRE(color_for(color_role::window_background) ==
                get_active_theme().resolve(color_role::window_background));
    }

    SECTION("Setting a theme changes what every role resolves to") {
        set_active_theme(theme{}
                             .set(color_role::accent, rendering::colors::MAGENTA)
                             .set(dimension_role::indent, 42.0)
                             .set(typography_role::body, typography{.scale = 1.5}));

        REQUIRE(color_for(color_role::accent) == rendering::colors::MAGENTA);
        REQUIRE_THAT(dimension_for(dimension_role::indent),
                     Catch::Matchers::WithinAbs(42.0, 0.001));
        REQUIRE(typography_for(typography_role::body) == typography{.scale = 1.5});
    }

    GIVEN("A theme installed for a scope only") {
        set_active_theme(theme{}.set(color_role::accent, rendering::colors::GREEN));

        WHEN("A different theme is made active for that scope") {
            {
                const scoped_active_theme scoped{
                    theme{}.set(color_role::accent, rendering::colors::RED)};

                THEN("Roles resolve against the scoped theme") {
                    REQUIRE(color_for(color_role::accent) == rendering::colors::RED);
                }
            }

            THEN("The previous theme is back once the scope ends") {
                REQUIRE(color_for(color_role::accent) == rendering::colors::GREEN);
            }
        }
    }
}
