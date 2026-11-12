// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/mouse-button.hpp"

// C++ STL
#include <cstddef>
#include <set>
#include <string_view>

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("mouse_button enum and names", "[tca][engine][input][mouse-button]") {
    using namespace tca::engine::input;

    SECTION("mouse_button_index() spans [0, MOUSE_BUTTONS_COUNT)") {
        REQUIRE(MOUSE_BUTTONS_COUNT == 5);
        REQUIRE(mouse_button_index(mouse_button::left) == 0);
        REQUIRE(mouse_button_index(mouse_button::x2) == MOUSE_BUTTONS_COUNT - 1);
    }

    SECTION("Every mouse button has a non-empty, unique name") {
        std::set<std::string_view> names{};
        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto name{mouse_button_name(static_cast<mouse_button>(index))};
            REQUIRE_FALSE(name.empty());
            REQUIRE(names.insert(name).second);
        }
    }

    SECTION("Names are spelled like the enumerators") {
        REQUIRE(mouse_button_name(mouse_button::left) == "left");
        REQUIRE(mouse_button_name(mouse_button::right) == "right");
        REQUIRE(mouse_button_name(mouse_button::middle) == "middle");
        REQUIRE(mouse_button_name(mouse_button::x1) == "x1");
        REQUIRE(mouse_button_name(mouse_button::x2) == "x2");
    }
}

TEST_CASE("mouse_button_from_name", "[tca][engine][input][mouse-button]") {
    using namespace tca::engine::input;

    SECTION("Every mouse button is found back from its name") {
        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            const auto found{mouse_button_from_name(mouse_button_name(b))};
            REQUIRE(found.has_value());
            REQUIRE(found.value() == b);
        }
    }

    SECTION("A name of no mouse button is not one") {
        REQUIRE_FALSE(mouse_button_from_name("").has_value());
        REQUIRE_FALSE(mouse_button_from_name("not_a_button").has_value());
        REQUIRE_FALSE(mouse_button_from_name("mouse:left").has_value());
    }

    SECTION("Key names are not mouse button names") {
        REQUIRE_FALSE(mouse_button_from_name("space").has_value());
        REQUIRE_FALSE(mouse_button_from_name("a").has_value());
    }

    SECTION("Names are matched exactly") {
        REQUIRE_FALSE(mouse_button_from_name("Left").has_value());
        REQUIRE_FALSE(mouse_button_from_name("LEFT").has_value());
        REQUIRE_FALSE(mouse_button_from_name(" left").has_value());
        REQUIRE_FALSE(mouse_button_from_name("left ").has_value());
        REQUIRE_FALSE(mouse_button_from_name("lef").has_value());
        REQUIRE_FALSE(mouse_button_from_name("X1").has_value());
    }
}
