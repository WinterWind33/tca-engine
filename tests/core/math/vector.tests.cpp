// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/math/vector.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("vector2 unit tests", "[tca][engine][math][vectors][vector2]") {
    using namespace tca::engine;

    SECTION("Default constructor initializes to zero") {
        const math::vector2 vec{};
        REQUIRE(vec.x() == 0.0);
        REQUIRE(vec.y() == 0.0);
    }

    SECTION("Constructor with parameters initializes correctly") {
        const math::vector2 vec{3.0, 4.0};
        REQUIRE(vec.x() == 3.0);
        REQUIRE(vec.y() == 4.0);
    }

    SECTION("Static zero() method returns a zero vector") {
        const math::vector2 vec{math::vector2::zero()};
        REQUIRE(vec.x() == 0.0);
        REQUIRE(vec.y() == 0.0);
    }

    SECTION("Static x_axis_vector() method returns the correct unit vector") {
        const math::vector2 vec{math::vector2::x_axis_vector()};
        REQUIRE(vec.x() == 1.0);
        REQUIRE(vec.y() == 0.0);
    }

    SECTION("Static y_axis_vector() method returns the correct unit vector") {
        const math::vector2 vec{math::vector2::y_axis_vector()};
        REQUIRE(vec.x() == 0.0);
        REQUIRE(vec.y() == 1.0);
    }

    SECTION("Operators") {
        SECTION("Addition") {
            const math::vector2 vec1{1.0, 2.0};
            const math::vector2 vec2{3.0, 4.0};
            const math::vector2 result{vec1 + vec2};
            REQUIRE(result.x() == 4.0);
            REQUIRE(result.y() == 6.0);
        }

        SECTION("Subtraction") {
            const math::vector2 vec1{5.0, 6.0};
            const math::vector2 vec2{3.0, 4.0};
            const math::vector2 result{vec1 - vec2};
            REQUIRE(result.x() == 2.0);
            REQUIRE(result.y() == 2.0);
        }

        SECTION("Scalar multiplication") {
            const math::vector2 vec{1.0, 2.0};
            const math::vector2 result{vec * 3.0};
            REQUIRE(result.x() == 3.0);
            REQUIRE(result.y() == 6.0);
        }
    }

    SECTION("is_nearly_equal()") {
        SECTION("Nearly equal vectors compare equal") {
            const math::vector2 vec1{0.1 + 0.2, 1.0};
            const math::vector2 vec2{0.3, 1.0};
            REQUIRE(math::is_nearly_equal(vec1, vec2));
        }

        SECTION("Vectors differing in the x component compare different") {
            const math::vector2 vec1{5.0, 1.0};
            const math::vector2 vec2{4.99, 1.0};
            REQUIRE_FALSE(math::is_nearly_equal(vec1, vec2));
        }

        SECTION("Vectors differing in the y component compare different") {
            const math::vector2 vec1{1.0, 5.0};
            const math::vector2 vec2{1.0, 4.99};
            REQUIRE_FALSE(math::is_nearly_equal(vec1, vec2));
        }

        SECTION("Custom tolerance is applied to each component") {
            const math::vector2 vec1{5.0, 5.0};
            const math::vector2 vec2{4.99, 5.01};
            REQUIRE(math::is_nearly_equal(vec1, vec2, 0.1));
            REQUIRE_FALSE(math::is_nearly_equal(vec1, vec2, 0.001));
        }
    }

    SECTION("is_nearly_zero()") {
        SECTION("The zero vector is nearly zero") {
            REQUIRE(math::is_nearly_zero(math::vector2::zero()));
        }

        SECTION("A vector with a non-zero x component is not nearly zero") {
            REQUIRE_FALSE(math::is_nearly_zero(math::vector2{0.01, 0.0}));
        }

        SECTION("A vector with a non-zero y component is not nearly zero") {
            REQUIRE_FALSE(math::is_nearly_zero(math::vector2{0.0, 0.01}));
        }

        SECTION("Custom tolerance is applied to each component") {
            const math::vector2 vec{0.01, -0.01};
            REQUIRE(math::is_nearly_zero(vec, 0.1));
            REQUIRE_FALSE(math::is_nearly_zero(vec, 0.001));
        }
    }
}
