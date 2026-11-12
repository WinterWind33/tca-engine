// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/layout-widgets.hpp"

// C++ STL
#include <stdexcept>

// Third-party
#include <catch2/catch_test_macros.hpp>

#include "core/math/vector.hpp"

TEST_CASE("compute_anchor_position() function tests",
          "[tca][engine][rendering][gui][layout-widgets][compute_anchor_position]") {
    using namespace tca::engine;

    GIVEN("A parent container with a valid size (100,100)") {
        const math::vector2 containerSize{100, 100};
        math::vector2 pos{};

        SECTION("top left anchor position") {
            REQUIRE_NOTHROW(pos = widgets::compute_anchor_position(
                                widgets::anchor_position::top_left, containerSize));
            REQUIRE(pos.x() == 0.0);
            REQUIRE(pos.y() == 0.0);
        }

        SECTION("top center anchor position") {
            REQUIRE_NOTHROW(pos = widgets::compute_anchor_position(
                                widgets::anchor_position::top_center, containerSize));
            REQUIRE(pos.x() == 50.0);
            REQUIRE(pos.y() == 0.0);
        }

        SECTION("top right anchor position") {
            REQUIRE_NOTHROW(pos = widgets::compute_anchor_position(
                                widgets::anchor_position::top_right, containerSize));
            REQUIRE(pos.x() == 100.0);
            REQUIRE(pos.y() == 0.0);
        }

        SECTION("center left anchor position") {
            REQUIRE_NOTHROW(pos = widgets::compute_anchor_position(
                                widgets::anchor_position::center_left, containerSize));
            REQUIRE(pos.x() == 0.0);
            REQUIRE(pos.y() == 50.0);
        }

        SECTION("center anchor position") {
            REQUIRE_NOTHROW(pos = widgets::compute_anchor_position(widgets::anchor_position::center,
                                                                   containerSize));
            REQUIRE(pos.x() == 50.0);
            REQUIRE(pos.y() == 50.0);
        }

        SECTION("center right anchor position") {
            REQUIRE_NOTHROW(pos = widgets::compute_anchor_position(
                                widgets::anchor_position::center_right, containerSize));
            REQUIRE(pos.x() == 100.0);
            REQUIRE(pos.y() == 50.0);
        }

        SECTION("bottom left anchor position") {
            REQUIRE_NOTHROW(pos = widgets::compute_anchor_position(
                                widgets::anchor_position::bottom_left, containerSize));
            REQUIRE(pos.x() == 0.0);
            REQUIRE(pos.y() == 100.0);
        }

        SECTION("bottom center anchor position") {
            REQUIRE_NOTHROW(pos = widgets::compute_anchor_position(
                                widgets::anchor_position::bottom_center, containerSize));
            REQUIRE(pos.x() == 50.0);
            REQUIRE(pos.y() == 100.0);
        }

        SECTION("bottom right anchor position") {
            REQUIRE_NOTHROW(pos = widgets::compute_anchor_position(
                                widgets::anchor_position::bottom_right, containerSize));
            REQUIRE(pos.x() == 100.0);
            REQUIRE(pos.y() == 100.0);
        }

        SECTION("invalid anchor position") {
            REQUIRE_THROWS_AS(widgets::compute_anchor_position(
                                  static_cast<widgets::anchor_position>(-1), containerSize),
                              std::invalid_argument);
        }
    }

    GIVEN("A parent container with an invalid size (negative values)") {
        const math::vector2 containerSize{-100, -100};

        SECTION("top left anchor position") {
            REQUIRE_THROWS_AS(
                widgets::compute_anchor_position(widgets::anchor_position::top_left, containerSize),
                std::invalid_argument);
        }
    }

    GIVEN("A parent container with an invalid size (zero values)") {
        const math::vector2 containerSize{0, 0};

        SECTION("center anchor position") {
            REQUIRE_THROWS_AS(
                widgets::compute_anchor_position(widgets::anchor_position::center, containerSize),
                std::invalid_argument);
        }
    }
}

TEST_CASE("compute_anchor_pivot() function tests",
          "[tca][engine][rendering][gui][layout-widgets][compute_anchor_pivot]") {
    using namespace tca::engine;

    math::vector2 pivot{};

    SECTION("top left anchor position") {
        REQUIRE_NOTHROW(pivot = widgets::compute_anchor_pivot(widgets::anchor_position::top_left));
        REQUIRE(pivot.x() == 0.0);
        REQUIRE(pivot.y() == 0.0);
    }

    SECTION("top center anchor position") {
        REQUIRE_NOTHROW(pivot =
                            widgets::compute_anchor_pivot(widgets::anchor_position::top_center));
        REQUIRE(pivot.x() == 0.5);
        REQUIRE(pivot.y() == 0.0);
    }

    SECTION("top right anchor position") {
        REQUIRE_NOTHROW(pivot = widgets::compute_anchor_pivot(widgets::anchor_position::top_right));
        REQUIRE(pivot.x() == 1.0);
        REQUIRE(pivot.y() == 0.0);
    }

    SECTION("center left anchor position") {
        REQUIRE_NOTHROW(pivot =
                            widgets::compute_anchor_pivot(widgets::anchor_position::center_left));
        REQUIRE(pivot.x() == 0.0);
        REQUIRE(pivot.y() == 0.5);
    }

    SECTION("center anchor position") {
        REQUIRE_NOTHROW(pivot = widgets::compute_anchor_pivot(widgets::anchor_position::center));
        REQUIRE(pivot.x() == 0.5);
        REQUIRE(pivot.y() == 0.5);
    }

    SECTION("center right anchor position") {
        REQUIRE_NOTHROW(pivot =
                            widgets::compute_anchor_pivot(widgets::anchor_position::center_right));
        REQUIRE(pivot.x() == 1.0);
        REQUIRE(pivot.y() == 0.5);
    }

    SECTION("bottom left anchor position") {
        REQUIRE_NOTHROW(pivot =
                            widgets::compute_anchor_pivot(widgets::anchor_position::bottom_left));
        REQUIRE(pivot.x() == 0.0);
        REQUIRE(pivot.y() == 1.0);
    }

    SECTION("bottom center anchor position") {
        REQUIRE_NOTHROW(pivot =
                            widgets::compute_anchor_pivot(widgets::anchor_position::bottom_center));
        REQUIRE(pivot.x() == 0.5);
        REQUIRE(pivot.y() == 1.0);
    }

    SECTION("bottom right anchor position") {
        REQUIRE_NOTHROW(pivot =
                            widgets::compute_anchor_pivot(widgets::anchor_position::bottom_right));
        REQUIRE(pivot.x() == 1.0);
        REQUIRE(pivot.y() == 1.0);
    }

    SECTION("invalid anchor position") {
        REQUIRE_THROWS_AS(widgets::compute_anchor_pivot(static_cast<widgets::anchor_position>(-1)),
                          std::invalid_argument);
    }
}
