// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/utilities/non-zero-values.hpp"

// C++ STL
#include <stdexcept>

// Third-party
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace tca::engine::core;

#ifndef NDEBUG

TEMPLATE_TEST_CASE("non_zero_value integral construction tests",
                   "[tca][engine][core][utilities][non_zero_value]", int, long, short) {
    SECTION("Constructing with a non-zero value") {
        REQUIRE_NOTHROW(non_zero_value<TestType>{TestType{5}});
    }

    SECTION("Constructing with zero throws") {
        REQUIRE_THROWS_AS(non_zero_value<TestType>{TestType{0}}, std::invalid_argument);
    }
}

TEMPLATE_TEST_CASE("non_zero_value floating point construction tests (no tolerance)",
                   "[tca][engine][core][utilities][non_zero_value]", float, double) {
    SECTION("Constructing with a non-zero value") {
        REQUIRE_NOTHROW(non_zero_value<TestType>{TestType{5.0}});
    }

    SECTION("Constructing with exactly zero throws") {
        REQUIRE_THROWS_AS(non_zero_value<TestType>{TestType{0.0}}, std::invalid_argument);
    }
}

TEMPLATE_TEST_CASE("non_zero_value floating point construction tests (with tolerance)",
                   "[tca][engine][core][utilities][non_zero_value]", float, double) {
    constexpr TestType TOLERANCE{static_cast<TestType>(0.1)};

    SECTION("Constructing with a value outside the tolerance range") {
        REQUIRE_NOTHROW((non_zero_value<TestType, TOLERANCE>{TestType{5.0}}));
    }

    SECTION("Constructing with exactly zero throws") {
        REQUIRE_THROWS_AS((non_zero_value<TestType, TOLERANCE>{TestType{0.0}}),
                           std::invalid_argument);
    }

    SECTION("Constructing with a value inside the tolerance range throws") {
        constexpr TestType VALUE{TOLERANCE - static_cast<TestType>(0.01)};

        REQUIRE_THROWS_AS((non_zero_value<TestType, TOLERANCE>{VALUE}), std::invalid_argument);
    }
}

#endif

TEMPLATE_TEST_CASE("non_zero_value conversion tests",
                   "[tca][engine][core][utilities][non_zero_value]", int, float, double) {
    SECTION("Converting back to the underlying value") {
        const non_zero_value<TestType> value{TestType{5}};

        REQUIRE(static_cast<TestType>(value) == TestType{5});
    }
}

TEMPLATE_TEST_CASE("non_zero_value arithmetic operators tests",
                   "[tca][engine][core][utilities][non_zero_value]", int, float, double) {
    const non_zero_value<TestType> A{TestType{6}};
    const non_zero_value<TestType> B{TestType{2}};

    SECTION("Adding two non_zero_value instances") {
        REQUIRE(A + B == TestType{8});
    }

    SECTION("Adding a non_zero_value and a raw value") {
        REQUIRE(A + TestType{2} == TestType{8});
        REQUIRE(TestType{2} + A == TestType{8});
    }

    SECTION("Subtracting two non_zero_value instances") {
        REQUIRE(A - B == TestType{4});
    }

    SECTION("Subtracting a non_zero_value and a raw value") {
        REQUIRE(A - TestType{2} == TestType{4});
        REQUIRE(TestType{10} - A == TestType{4});
    }

    SECTION("Multiplying two non_zero_value instances") {
        REQUIRE(A * B == TestType{12});
    }

    SECTION("Multiplying a non_zero_value and a raw value") {
        REQUIRE(A * TestType{2} == TestType{12});
        REQUIRE(TestType{2} * A == TestType{12});
    }

    SECTION("Dividing two non_zero_value instances") {
        REQUIRE(A / B == TestType{3});
    }

    SECTION("Dividing a non_zero_value and a raw value") {
        REQUIRE(A / TestType{2} == TestType{3});
        REQUIRE(TestType{12} / A == TestType{2});
    }

    SECTION("Negating a non_zero_value") {
        REQUIRE(-A == -TestType{6});
    }
}
