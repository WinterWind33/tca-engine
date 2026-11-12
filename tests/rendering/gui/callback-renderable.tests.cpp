// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/callback-renderable.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("callback_renderable class tests", "[tca][engine][rendering][gui][callback_renderable]") {
    using namespace tca::engine::widgets;

    GIVEN("A callback_renderable constructed with a callback") {
        std::size_t callCount{};
        callback_renderable renderable{[&callCount]() {
            ++callCount;
        }};

        WHEN("It is rendered") {
            REQUIRE_NOTHROW(renderable.render());

            THEN("The callback is invoked") {
                REQUIRE(callCount == 1);
            }
        }

        WHEN("It is rendered multiple times") {
            renderable.render();
            renderable.render();
            renderable.render();

            THEN("The callback is invoked once per render() call") {
                REQUIRE(callCount == 3);
            }
        }
    }

    GIVEN("A callback_renderable constructed with an empty callback") {
        callback_renderable renderable{{}};

        WHEN("It is rendered") {
            THEN("Nothing throws, since an empty callback is simply skipped") {
                REQUIRE_NOTHROW(renderable.render());
            }
        }
    }
}
