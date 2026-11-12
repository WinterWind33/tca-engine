// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/math/math.hpp"

// C++ STL
#include <concepts>

// Third-party
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

template <typename T>
    requires std::floating_point<T>
struct test_num_generator {};

template <>
struct test_num_generator<double> {
    static constexpr double VALUE_1{0.1 + 0.2};
    static constexpr double VALUE_2{0.3};
    static constexpr double VALUE_3{5.0};
    static constexpr double VALUE_4{4.99};
    static constexpr double CUSTOM_TOLERANCE{0.1};
};

template <>
struct test_num_generator<float> {
    static constexpr float VALUE_1{0.1f + 0.2f};
    static constexpr float VALUE_2{0.3f};
    static constexpr float VALUE_3{5.0f};
    static constexpr float VALUE_4{4.99f};
    static constexpr float CUSTOM_TOLERANCE{0.1f};
};

} // namespace

TEMPLATE_TEST_CASE("is_nearly_equal() function tests", "[tca][engine][core][math][is_nearly_equal]",
                   double, float) {
    using namespace tca::engine::math;

    SECTION("Machine epsilon as tolerance") {
        SECTION("Comparing two nearly equal floating-point numbers") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_1};
            constexpr TestType B{test_num_generator<TestType>::VALUE_2};

            REQUIRE(is_nearly_equal(A, B));
        }

        SECTION("Comparing two different floating-point numbers") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_1};
            constexpr TestType B{test_num_generator<TestType>::VALUE_3};

            REQUIRE_FALSE(is_nearly_equal(A, B));
        }
    }

    SECTION("Custom tolerance") {
        SECTION("Comparing two nearly equal floating-point numbers with custom tolerance") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_1};
            constexpr TestType B{test_num_generator<TestType>::VALUE_2};
            constexpr TestType TOLERANCE{test_num_generator<TestType>::CUSTOM_TOLERANCE};

            REQUIRE(is_nearly_equal(A, B, TOLERANCE));
        }

        SECTION(
            "Comparing two different floating-point numbers with custom tolerance but inside the "
            "tolerance range") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_3};
            constexpr TestType B{test_num_generator<TestType>::VALUE_4};
            constexpr TestType TOLERANCE{test_num_generator<TestType>::CUSTOM_TOLERANCE};

            REQUIRE(is_nearly_equal(A, B, TOLERANCE));
        }

        SECTION(
            "Comparing two different floating-point numbers with custom tolerance and outside the "
            "tolerance range") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_1};
            constexpr TestType B{test_num_generator<TestType>::VALUE_4};
            constexpr TestType TOLERANCE{test_num_generator<TestType>::CUSTOM_TOLERANCE};

            REQUIRE_FALSE(is_nearly_equal(A, B, TOLERANCE));
        }
    }
}

TEMPLATE_TEST_CASE("is_nearly_zero() function tests", "[tca][engine][core][math][is_nearly_zero]",
                   double, float) {
    using namespace tca::engine::math;

    SECTION("Machine epsilon as tolerance") {
        SECTION("Comparing a nearly zero floating-point number") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_1 -
                                 test_num_generator<TestType>::VALUE_2};

            REQUIRE(is_nearly_zero(A));
        }

        SECTION("Comparing a non-zero floating-point number") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_3};

            REQUIRE_FALSE(is_nearly_zero(A));
        }
    }

    SECTION("Custom tolerance") {
        SECTION("Comparing a nearly zero floating-point number with custom tolerance") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_1 -
                                 test_num_generator<TestType>::VALUE_2};
            constexpr TestType TOLERANCE{test_num_generator<TestType>::CUSTOM_TOLERANCE};

            REQUIRE(is_nearly_zero(A, TOLERANCE));
        }

        SECTION(
            "Comparing a non-zero floating-point number with custom tolerance but inside the "
            "tolerance range") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_3 -
                                 test_num_generator<TestType>::VALUE_4};
            constexpr TestType TOLERANCE{test_num_generator<TestType>::CUSTOM_TOLERANCE};

            REQUIRE(is_nearly_zero(A, TOLERANCE));
        }

        SECTION(
            "Comparing a non-zero floating-point number with custom tolerance and outside the "
            "tolerance range") {
            constexpr TestType A{test_num_generator<TestType>::VALUE_1 -
                                 test_num_generator<TestType>::VALUE_4};
            constexpr TestType TOLERANCE{test_num_generator<TestType>::CUSTOM_TOLERANCE};

            REQUIRE_FALSE(is_nearly_zero(A, TOLERANCE));
        }
    }
}
