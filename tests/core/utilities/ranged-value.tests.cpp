// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/utilities/ranged-value.hpp"

// Third-party
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

// C++ STL
#include <chrono>
#include <compare>
#include <complex>
#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>

using namespace tca::engine::core;

namespace {

// A structural, totally ordered type with no std::formatter, standing in for a game's own
// value types.
struct level {
    int value{};

    constexpr auto operator<=>(const level&) const = default;
};

// Whether a ranged_value offers the given operator. Concepts rather than inline requires
// expressions, which are a hard error (not false) for non-dependent types outside a template.
template <typename RangedValue>
concept pre_incrementable = requires(RangedValue rv) { ++rv; };

template <typename RangedValue>
concept post_incrementable = requires(RangedValue rv) { rv++; };

template <typename RangedValue, typename Delta>
concept add_assignable = requires(RangedValue rv, Delta delta) { rv += delta; };

template <typename RangedValue, typename Delta>
concept subtract_assignable = requires(RangedValue rv, Delta delta) { rv -= delta; };

} // namespace

TEMPLATE_TEST_CASE("ranged_value bounds tests", "[tca][engine][core][utilities][ranged_value]", int,
                   float) {
    // Named as same-typed constexpr locals (rather than inline int literals) so the bounds
    // are of type TestType: bounds are part of the type as given, so `0` and `0.f` would
    // make the int and float instantiations name different kinds of ranged_value.
    constexpr TestType RANGE_MIN{0};
    constexpr TestType RANGE_MAX{5};

    SECTION("MIN and MAX return the template arguments") {
        REQUIRE((ranged_value<TestType, RANGE_MIN, RANGE_MAX>::MIN()) == TestType{0});
        REQUIRE((ranged_value<TestType, RANGE_MIN, RANGE_MAX>::MAX()) == TestType{5});
    }

    SECTION("The bound policy defaults to closed") {
        REQUIRE((ranged_value<TestType, RANGE_MIN, RANGE_MAX>::BOUND_POLICY()) ==
                rv_bound_policy::closed);
    }

    SECTION("BOUND_POLICY returns the requested policy") {
        REQUIRE((ranged_value<TestType, RANGE_MIN, RANGE_MAX,
                              rv_bound_policy::max_open>::BOUND_POLICY()) ==
                rv_bound_policy::max_open);
        REQUIRE((ranged_value<TestType, RANGE_MIN, RANGE_MAX,
                              rv_bound_policy::min_open>::BOUND_POLICY()) ==
                rv_bound_policy::min_open);
        REQUIRE(
            (ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::open>::BOUND_POLICY()) ==
            rv_bound_policy::open);
    }

    SECTION("The bound policy is part of the type") {
        REQUIRE_FALSE((
            std::same_as<ranged_value<TestType, RANGE_MIN, RANGE_MAX>,
                         ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::max_open>>));
        REQUIRE_FALSE((
            std::same_as<ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::max_open>,
                         ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::min_open>>));
        REQUIRE_FALSE(
            (std::same_as<ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::min_open>,
                          ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::open>>));
    }
}

TEMPLATE_TEST_CASE("ranged_value construction tests (closed range, zero inside range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the previous TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{0};
    constexpr TestType RANGE_MAX{5};

    SECTION("Default construction yields zero") {
        REQUIRE((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{}) == TestType{0});
    }

    SECTION("Constructing with a value inside the range does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{3}}));
    }

    SECTION("Constructing with a value inside the range round-trips") {
        const ranged_value<TestType, RANGE_MIN, RANGE_MAX> value{TestType{3}};

        REQUIRE(static_cast<TestType>(value) == TestType{3});
    }

    SECTION("Constructing with the lower bound does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{0}}));
    }

    SECTION("Constructing with the upper bound (inclusive) round-trips") {
        const ranged_value<TestType, RANGE_MIN, RANGE_MAX> value{TestType{5}};

        REQUIRE(value.get() == TestType{5});
    }

    SECTION("Constructing with a value above the range throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{6}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with a value below the range throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{-1}}),
                          ranged_value_exception);
    }
}

TEMPLATE_TEST_CASE("ranged_value construction tests (max_open range, zero inside range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{0};
    constexpr TestType RANGE_MAX{5};
    constexpr auto POLICY{rv_bound_policy::max_open};

    SECTION("Default construction yields zero") {
        REQUIRE((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{}) == TestType{0});
    }

    SECTION("Constructing with a value inside the range does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{3}}));
    }

    SECTION("Constructing with the lower bound does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{0}}));
    }

    SECTION("Constructing with the upper bound (exclusive) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{5}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with a value below the range throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{-1}}),
                          ranged_value_exception);
    }
}

TEMPLATE_TEST_CASE("ranged_value construction tests (closed range, zero outside range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{5};
    constexpr TestType RANGE_MAX{10};

    SECTION("Constructing with a value inside the range does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{7}}));
    }

    SECTION("Constructing with a value inside the range round-trips") {
        const ranged_value<TestType, RANGE_MIN, RANGE_MAX> value{TestType{7}};

        REQUIRE(static_cast<TestType>(value) == TestType{7});
    }

    SECTION("Constructing with zero (outside the range) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{0}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with a value below the range throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{4}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with the upper bound (inclusive) does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{10}}));
    }

    SECTION("Constructing with a value above the range throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{11}}),
                          ranged_value_exception);
    }
}

TEMPLATE_TEST_CASE("ranged_value construction tests (max_open range, zero outside range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{5};
    constexpr TestType RANGE_MAX{10};
    constexpr auto POLICY{rv_bound_policy::max_open};

    SECTION("Constructing with a value inside the range does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{7}}));
    }

    SECTION("Constructing with zero (outside the range) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{0}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with the upper bound (exclusive) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{10}}),
                          ranged_value_exception);
    }
}

TEMPLATE_TEST_CASE("ranged_value construction tests (min_open range, zero inside range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{-5};
    constexpr TestType RANGE_MAX{5};
    constexpr auto POLICY{rv_bound_policy::min_open};

    SECTION("Default construction yields zero") {
        REQUIRE((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{}) == TestType{0});
    }

    SECTION("Constructing with a value inside the range round-trips") {
        const ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY> value{TestType{3}};

        REQUIRE(value.get() == TestType{3});
    }

    SECTION("Constructing with the lower bound (exclusive) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{-5}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with the upper bound (inclusive) round-trips") {
        const ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY> value{TestType{5}};

        REQUIRE(value.get() == TestType{5});
    }

    SECTION("Constructing with a value above the range throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{6}}),
                          ranged_value_exception);
    }
}

TEMPLATE_TEST_CASE("ranged_value construction tests (min_open range, zero outside range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{5};
    constexpr TestType RANGE_MAX{10};
    constexpr auto POLICY{rv_bound_policy::min_open};

    SECTION("Constructing with a value inside the range does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{7}}));
    }

    SECTION("Constructing with zero (outside the range) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{0}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with the lower bound (exclusive) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{5}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with the upper bound (inclusive) does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{10}}));
    }
}

TEMPLATE_TEST_CASE("ranged_value construction tests (open range, zero inside range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{-5};
    constexpr TestType RANGE_MAX{5};
    constexpr auto POLICY{rv_bound_policy::open};

    SECTION("Default construction yields zero") {
        REQUIRE((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{}) == TestType{0});
    }

    SECTION("Constructing with a value inside the range round-trips") {
        const ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY> value{TestType{-4}};

        REQUIRE(value.get() == TestType{-4});
    }

    SECTION("Constructing with the lower bound (exclusive) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{-5}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with the upper bound (exclusive) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{5}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with a value below the range throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{-6}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with a value above the range throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{6}}),
                          ranged_value_exception);
    }
}

TEMPLATE_TEST_CASE("ranged_value construction tests (open range, zero outside range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{5};
    constexpr TestType RANGE_MAX{10};
    constexpr auto POLICY{rv_bound_policy::open};

    SECTION("Constructing with a value inside the range does not throw") {
        REQUIRE_NOTHROW((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{7}}));
    }

    SECTION("Constructing with zero (outside the range) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{0}}),
                          ranged_value_exception);
    }

    SECTION("Constructing with either bound (both exclusive) throws") {
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{5}}),
                          ranged_value_exception);
        REQUIRE_THROWS_AS((ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY>{TestType{10}}),
                          ranged_value_exception);
    }
}

TEST_CASE("ranged_value default construction depends on the bound policy",
          "[tca][engine][core][utilities][ranged_value]") {
    SECTION("Zero as the upper bound belongs to a closed range") {
        STATIC_REQUIRE(std::default_initializable<ranged_value<int, -5, 0>>);
        REQUIRE((ranged_value<int, -5, 0>{}) == 0);
    }

    SECTION("Zero as the upper bound is outside a max_open range") {
        STATIC_REQUIRE_FALSE(
            (std::default_initializable<ranged_value<int, -5, 0, rv_bound_policy::max_open>>));
        REQUIRE_THROWS_AS((ranged_value<int, -5, 0, rv_bound_policy::max_open>{0}),
                          ranged_value_exception);
        REQUIRE_NOTHROW((ranged_value<int, -5, 0, rv_bound_policy::max_open>{-1}));
    }

    SECTION("Zero as the lower bound belongs to a max_open range") {
        STATIC_REQUIRE(
            std::default_initializable<ranged_value<int, 0, 5, rv_bound_policy::max_open>>);
        REQUIRE((ranged_value<int, 0, 5, rv_bound_policy::max_open>{}) == 0);
    }

    SECTION("Zero as the lower bound is outside a min_open range") {
        STATIC_REQUIRE_FALSE(
            (std::default_initializable<ranged_value<int, 0, 5, rv_bound_policy::min_open>>));
        REQUIRE_THROWS_AS((ranged_value<int, 0, 5, rv_bound_policy::min_open>{0}),
                          ranged_value_exception);
        REQUIRE_NOTHROW((ranged_value<int, 0, 5, rv_bound_policy::min_open>{1}));
    }

    SECTION("Zero as the upper bound belongs to a min_open range") {
        STATIC_REQUIRE(
            std::default_initializable<ranged_value<int, -5, 0, rv_bound_policy::min_open>>);
        REQUIRE((ranged_value<int, -5, 0, rv_bound_policy::min_open>{}) == 0);
    }

    SECTION("Zero as either bound is outside an open range") {
        STATIC_REQUIRE_FALSE(
            (std::default_initializable<ranged_value<int, 0, 5, rv_bound_policy::open>>));
        STATIC_REQUIRE_FALSE(
            (std::default_initializable<ranged_value<int, -5, 0, rv_bound_policy::open>>));
        STATIC_REQUIRE(std::default_initializable<ranged_value<int, -5, 5, rv_bound_policy::open>>);
    }
}

TEST_CASE("ranged_value with Min == Max", "[tca][engine][core][utilities][ranged_value]") {
    SECTION("A closed range holds exactly that one value") {
        REQUIRE_NOTHROW((ranged_value<int, 3, 3>{3}));
        REQUIRE_THROWS_AS((ranged_value<int, 3, 3>{2}), ranged_value_exception);
        REQUIRE_THROWS_AS((ranged_value<int, 3, 3>{4}), ranged_value_exception);
    }

    SECTION("A max_open range is empty, so even the bound itself is rejected") {
        REQUIRE_THROWS_AS((ranged_value<int, 3, 3, rv_bound_policy::max_open>{3}),
                          ranged_value_exception);
    }

    SECTION("A min_open range is empty, so even the bound itself is rejected") {
        REQUIRE_THROWS_AS((ranged_value<int, 3, 3, rv_bound_policy::min_open>{3}),
                          ranged_value_exception);
    }

    SECTION("An open range is empty, so even the bound itself is rejected") {
        REQUIRE_THROWS_AS((ranged_value<int, 3, 3, rv_bound_policy::open>{3}),
                          ranged_value_exception);
    }
}

TEST_CASE("ranged_value open range between adjacent integers is empty",
          "[tca][engine][core][utilities][ranged_value]") {
    REQUIRE_THROWS_AS((ranged_value<int, 3, 4, rv_bound_policy::open>{3}), ranged_value_exception);
    REQUIRE_THROWS_AS((ranged_value<int, 3, 4, rv_bound_policy::open>{4}), ranged_value_exception);
}

TEMPLATE_TEST_CASE("ranged_value comparison operators",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{0};
    constexpr TestType RANGE_MAX{10};

    const ranged_value<TestType, RANGE_MIN, RANGE_MAX> low{TestType{3}};
    const ranged_value<TestType, RANGE_MIN, RANGE_MAX> sameAsLow{TestType{3}};
    const ranged_value<TestType, RANGE_MIN, RANGE_MAX> high{TestType{7}};

    SECTION("Two ranged_value instances compare by their underlying value") {
        REQUIRE(low == sameAsLow);
        REQUIRE_FALSE(low == high);
        REQUIRE(low != high);
        REQUIRE_FALSE(low != sameAsLow);

        REQUIRE(low < high);
        REQUIRE(high > low);
        REQUIRE(low <= sameAsLow);
        REQUIRE(low >= sameAsLow);
        REQUIRE(low <= high);
        REQUIRE(high >= low);
    }

    SECTION("A ranged_value compares against a raw value_type on either side") {
        REQUIRE(low == TestType{3});
        REQUIRE(TestType{3} == low);
        REQUIRE(low != TestType{7});
        REQUIRE(TestType{7} != low);

        REQUIRE(low < TestType{7});
        REQUIRE(TestType{7} > low);
        REQUIRE(high > TestType{3});
        REQUIRE(TestType{3} < high);

        REQUIRE(low <= TestType{3});
        REQUIRE(TestType{3} <= low);
        REQUIRE(low >= TestType{3});
        REQUIRE(TestType{3} >= low);
    }

    SECTION("Ranged values differing only in bound policy still compare by their value") {
        const ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::max_open> maxOpenLow{
            TestType{3}};

        REQUIRE(low == maxOpenLow.get());
        REQUIRE(maxOpenLow < high.get());
    }
}

TEMPLATE_TEST_CASE("ranged_value assignment operator (closed range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{0};
    constexpr TestType RANGE_MAX{10};

    ranged_value<TestType, RANGE_MIN, RANGE_MAX> value{TestType{3}};

    SECTION("Assigning a value inside the range stores the new value") {
        value = TestType{7};

        REQUIRE(value.get() == TestType{7});
        REQUIRE(static_cast<TestType>(value) == TestType{7});
    }

    SECTION("Assigning the lower bound stores it") {
        value = RANGE_MIN;

        REQUIRE(value.get() == RANGE_MIN);
    }

    SECTION("Assigning the upper bound (inclusive) stores it") {
        value = RANGE_MAX;

        REQUIRE(value.get() == RANGE_MAX);
    }

    SECTION("Assigning a value above the range throws and leaves the value untouched") {
        REQUIRE_THROWS_AS((value = TestType{11}), ranged_value_exception);
        REQUIRE(value.get() == TestType{3});
    }

    SECTION("Assigning a value below the range throws and leaves the value untouched") {
        REQUIRE_THROWS_AS((value = TestType{-1}), ranged_value_exception);
        REQUIRE(value.get() == TestType{3});
    }

    SECTION("Copy assignment from another ranged_value stores the new value") {
        const ranged_value<TestType, RANGE_MIN, RANGE_MAX> other{TestType{9}};

        value = other;

        REQUIRE(value.get() == TestType{9});
    }
}

TEMPLATE_TEST_CASE("ranged_value assignment operator (max_open range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{0};
    constexpr TestType RANGE_MAX{10};
    constexpr auto POLICY{rv_bound_policy::max_open};

    ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY> value{TestType{3}};

    SECTION("Assigning a value inside the range stores the new value") {
        value = TestType{9};

        REQUIRE(value.get() == TestType{9});
    }

    SECTION("Assigning the lower bound stores it") {
        value = RANGE_MIN;

        REQUIRE(value.get() == RANGE_MIN);
    }

    SECTION("Assigning the upper bound (exclusive) throws and leaves the value untouched") {
        REQUIRE_THROWS_AS((value = RANGE_MAX), ranged_value_exception);
        REQUIRE(value.get() == TestType{3});
    }

    SECTION("Assigning a value below the range throws and leaves the value untouched") {
        REQUIRE_THROWS_AS((value = TestType{-1}), ranged_value_exception);
        REQUIRE(value.get() == TestType{3});
    }
}

TEMPLATE_TEST_CASE("ranged_value assignment operator (min_open range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{0};
    constexpr TestType RANGE_MAX{10};
    constexpr auto POLICY{rv_bound_policy::min_open};

    ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY> value{TestType{3}};

    SECTION("Assigning a value inside the range stores the new value") {
        value = TestType{1};

        REQUIRE(value.get() == TestType{1});
    }

    SECTION("Assigning the lower bound (exclusive) throws and leaves the value untouched") {
        REQUIRE_THROWS_AS((value = RANGE_MIN), ranged_value_exception);
        REQUIRE(value.get() == TestType{3});
    }

    SECTION("Assigning the upper bound (inclusive) stores it") {
        value = RANGE_MAX;

        REQUIRE(value.get() == RANGE_MAX);
    }

    SECTION("Assigning a value above the range throws and leaves the value untouched") {
        REQUIRE_THROWS_AS((value = TestType{11}), ranged_value_exception);
        REQUIRE(value.get() == TestType{3});
    }
}

TEMPLATE_TEST_CASE("ranged_value assignment operator (open range)",
                   "[tca][engine][core][utilities][ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{0};
    constexpr TestType RANGE_MAX{10};
    constexpr auto POLICY{rv_bound_policy::open};

    ranged_value<TestType, RANGE_MIN, RANGE_MAX, POLICY> value{TestType{3}};

    SECTION("Assigning a value inside the range stores the new value") {
        value = TestType{9};

        REQUIRE(value.get() == TestType{9});
    }

    SECTION("Assigning the lower bound (exclusive) throws and leaves the value untouched") {
        REQUIRE_THROWS_AS((value = RANGE_MIN), ranged_value_exception);
        REQUIRE(value.get() == TestType{3});
    }

    SECTION("Assigning the upper bound (exclusive) throws and leaves the value untouched") {
        REQUIRE_THROWS_AS((value = RANGE_MAX), ranged_value_exception);
        REQUIRE(value.get() == TestType{3});
    }
}

TEST_CASE("ranged_value pre/post decrement operators",
          "[tca][engine][core][utilities][ranged_value]") {
    SECTION("Pre-decrement stores the decremented value and returns it") {
        ranged_value<int, 0, 10> value{3};

        const auto& result{--value};

        REQUIRE(value.get() == 2);
        REQUIRE(result.get() == 2);
    }

    SECTION("Post-decrement stores the decremented value and returns the previous one") {
        ranged_value<int, 0, 10> value{3};

        const auto previous{value--};

        REQUIRE(value.get() == 2);
        REQUIRE(previous.get() == 3);
    }

    SECTION("Decrementing below the lower bound throws") {
        ranged_value<int, 0, 10> value{0};

        REQUIRE_THROWS_AS(--value, ranged_value_exception);
    }

    SECTION("Decrementing from the upper bound of a closed range is allowed") {
        ranged_value<int, 0, 10> value{10};

        --value;

        REQUIRE(value.get() == 9);
    }

    SECTION("Decrementing onto the lower bound of a min_open range throws") {
        ranged_value<int, 0, 10, rv_bound_policy::min_open> value{1};

        REQUIRE_THROWS_AS(--value, ranged_value_exception);
        REQUIRE(value.get() == 1);
    }

    SECTION("Decrementing onto the lower bound of an open range throws") {
        ranged_value<int, 0, 10, rv_bound_policy::open> value{1};

        REQUIRE_THROWS_AS(value--, ranged_value_exception);
        REQUIRE(value.get() == 1);
    }

    SECTION("Decrementing a max_open ranged_value behaves the same way") {
        ranged_value<int, 0, 10, rv_bound_policy::max_open> value{9};

        const auto previous{value--};

        REQUIRE(previous.get() == 9);
        REQUIRE(value.get() == 8);
    }

    SECTION("Decrementing an unsigned zero throws instead of wrapping around to max()") {
        // The wrapped-around value would be inside this range, so only the overflow check stops it.
        ranged_value<std::size_t, 0, std::numeric_limits<std::size_t>::max()> value{0};

        REQUIRE_THROWS_AS(--value, ranged_value_exception);
        REQUIRE(value.get() == 0);
    }

    SECTION("Decrementing a signed min() throws instead of overflowing") {
        ranged_value<int, std::numeric_limits<int>::min(), 0> value{
            std::numeric_limits<int>::min()};

        REQUIRE_THROWS_AS(value--, ranged_value_exception);
        REQUIRE(value.get() == std::numeric_limits<int>::min());
    }
}

TEST_CASE("ranged_value pre/post increment operators",
          "[tca][engine][core][utilities][ranged_value]") {
    SECTION("Pre-increment stores the incremented value and returns it") {
        ranged_value<int, 0, 10> value{3};

        const auto& result{++value};

        REQUIRE(value.get() == 4);
        REQUIRE(result.get() == 4);
        REQUIRE(&result == &value);
    }

    SECTION("Post-increment stores the incremented value and returns the previous one") {
        ranged_value<int, 0, 10> value{3};

        const auto previous{value++};

        REQUIRE(value.get() == 4);
        REQUIRE(previous.get() == 3);
    }

    SECTION("Incrementing above the upper bound throws and leaves the value untouched") {
        ranged_value<int, 0, 10> value{10};

        REQUIRE_THROWS_AS(++value, ranged_value_exception);
        REQUIRE(value.get() == 10);
    }

    SECTION("Incrementing from the lower bound of a closed range is allowed") {
        ranged_value<int, 0, 10> value{0};

        ++value;

        REQUIRE(value.get() == 1);
    }

    SECTION("Incrementing onto the upper bound of a max_open range throws") {
        ranged_value<int, 0, 10, rv_bound_policy::max_open> value{9};

        REQUIRE_THROWS_AS(++value, ranged_value_exception);
        REQUIRE(value.get() == 9);
    }

    SECTION("Incrementing onto the upper bound of an open range throws") {
        ranged_value<int, 0, 10, rv_bound_policy::open> value{9};

        REQUIRE_THROWS_AS(value++, ranged_value_exception);
        REQUIRE(value.get() == 9);
    }

    SECTION("Incrementing a min_open ranged_value behaves the same way") {
        ranged_value<int, 0, 10, rv_bound_policy::min_open> value{1};

        const auto previous{value++};

        REQUIRE(previous.get() == 1);
        REQUIRE(value.get() == 2);
    }

    SECTION("Incrementing an unsigned max() throws instead of wrapping around to zero") {
        // The wrapped-around value would be inside this range, so only the overflow check stops it.
        ranged_value<std::size_t, 0, std::numeric_limits<std::size_t>::max()> value{
            std::numeric_limits<std::size_t>::max()};

        REQUIRE_THROWS_AS(++value, ranged_value_exception);
        REQUIRE(value.get() == std::numeric_limits<std::size_t>::max());
    }

    SECTION("Incrementing a signed max() throws instead of overflowing") {
        ranged_value<int, 0, std::numeric_limits<int>::max()> value{
            std::numeric_limits<int>::max()};

        REQUIRE_THROWS_AS(value++, ranged_value_exception);
        REQUIRE(value.get() == std::numeric_limits<int>::max());
    }

    SECTION("Increment is only available when value_type has one") {
        using int_range = ranged_value<int, 0, 10>;
        using level_range = ranged_value<level, level{0}, level{10}>;

        STATIC_REQUIRE(pre_incrementable<int_range>);
        STATIC_REQUIRE(post_incrementable<int_range>);
        STATIC_REQUIRE_FALSE(pre_incrementable<level_range>);
        STATIC_REQUIRE_FALSE(post_incrementable<level_range>);
    }
}

TEST_CASE("ranged_value compound addition and subtraction",
          "[tca][engine][core][utilities][ranged_value]") {
    SECTION("+= adds to the value and returns the ranged_value itself") {
        ranged_value<int, 0, 10> value{3};

        auto& result{value += 4};

        REQUIRE(value.get() == 7);
        REQUIRE(&result == &value);
    }

    SECTION("-= subtracts from the value and returns the ranged_value itself") {
        ranged_value<int, 0, 10> value{7};

        auto& result{value -= 4};

        REQUIRE(value.get() == 3);
        REQUIRE(&result == &value);
    }

    SECTION("A negative delta moves the value the other way") {
        ranged_value<int, -10, 10> value{0};

        value += -3;
        REQUIRE(value.get() == -3);

        value -= -5;
        REQUIRE(value.get() == 2);
    }

    SECTION("Adding past the upper bound throws and leaves the value untouched") {
        ranged_value<int, 0, 10> value{8};

        REQUIRE_THROWS_AS(value += 3, ranged_value_exception);
        REQUIRE(value.get() == 8);
    }

    SECTION("Subtracting past the lower bound throws and leaves the value untouched") {
        ranged_value<int, 0, 10> value{2};

        REQUIRE_THROWS_AS(value -= 3, ranged_value_exception);
        REQUIRE(value.get() == 2);
    }

    SECTION("Landing exactly on a bound follows the bound policy") {
        ranged_value<int, 0, 10> closed{5};
        REQUIRE_NOTHROW(closed += 5);
        REQUIRE_NOTHROW(closed -= 10);

        ranged_value<int, 0, 10, rv_bound_policy::open> open{5};
        REQUIRE_THROWS_AS(open += 5, ranged_value_exception);
        REQUIRE_THROWS_AS(open -= 5, ranged_value_exception);
        REQUIRE(open.get() == 5);
    }

    SECTION("Unsigned arithmetic throws instead of wrapping around") {
        using count = ranged_value<std::size_t, 0, std::numeric_limits<std::size_t>::max()>;

        count low{2};
        REQUIRE_THROWS_AS(low -= 3, ranged_value_exception);
        REQUIRE(low.get() == 2);

        count high{std::numeric_limits<std::size_t>::max() - 1};
        REQUIRE_THROWS_AS(high += 2, ranged_value_exception);
        REQUIRE(high.get() == std::numeric_limits<std::size_t>::max() - 1);
        REQUIRE_NOTHROW(high += 1);
        REQUIRE(high.get() == std::numeric_limits<std::size_t>::max());
    }

    SECTION("Signed arithmetic throws instead of overflowing") {
        using wide =
            ranged_value<int, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()>;

        wide high{std::numeric_limits<int>::max() - 1};
        REQUIRE_THROWS_AS(high += 2, ranged_value_exception);
        REQUIRE_THROWS_AS(high -= -2, ranged_value_exception);
        REQUIRE(high.get() == std::numeric_limits<int>::max() - 1);

        wide low{std::numeric_limits<int>::min() + 1};
        REQUIRE_THROWS_AS(low -= 2, ranged_value_exception);
        REQUIRE_THROWS_AS(low += -2, ranged_value_exception);
        REQUIRE(low.get() == std::numeric_limits<int>::min() + 1);
    }

    SECTION("Floating-point values are checked against the range only") {
        ranged_value<double, 0, 1> value{0.25};

        value += 0.5;
        REQUIRE(value.get() == 0.75);
        REQUIRE_THROWS_AS(value += 0.5, ranged_value_exception);
        REQUIRE(value.get() == 0.75);
    }

    SECTION("Compound operators are only available when value_type has them") {
        using int_range = ranged_value<int, 0, 10>;
        using level_range = ranged_value<level, level{0}, level{10}>;

        STATIC_REQUIRE(add_assignable<int_range, int>);
        STATIC_REQUIRE(subtract_assignable<int_range, int>);
        STATIC_REQUIRE_FALSE(add_assignable<level_range, level>);
        STATIC_REQUIRE_FALSE(subtract_assignable<level_range, level>);
    }
}

TEST_CASE("RangeableType accepts any totally ordered, copyable type",
          "[tca][engine][core][utilities][ranged_value]") {
    STATIC_REQUIRE(RangeableType<std::chrono::nanoseconds>);
    STATIC_REQUIRE(RangeableType<std::chrono::duration<float>>);
    STATIC_REQUIRE(RangeableType<level>);

    // No ordering to check a range against.
    STATIC_REQUIRE_FALSE(RangeableType<std::complex<float>>);
    // Not copyable.
    STATIC_REQUIRE_FALSE(RangeableType<std::unique_ptr<int>>);
}

TEST_CASE("ranged_value bounds of a type other than value_type",
          "[tca][engine][core][utilities][ranged_value]") {
    SECTION("MIN and MAX return the bounds converted to value_type") {
        using range = ranged_value<double, 0, 1>;

        STATIC_REQUIRE(std::same_as<decltype(range::MIN()), double>);
        REQUIRE(range::MIN() == 0.0);
        REQUIRE(range::MAX() == 1.0);
    }

    SECTION("Values are checked against the converted bounds") {
        REQUIRE_NOTHROW((ranged_value<double, 0, 1>{0.5}));
        REQUIRE_THROWS_AS((ranged_value<double, 0, 1>{1.5}), ranged_value_exception);
    }
}

TEST_CASE("ranged_value of std::chrono durations", "[tca][engine][core][utilities][ranged_value]") {
    using namespace std::chrono_literals;

    SECTION("A range including zero is default-constructible, defaulting to zero") {
        using timeout = ranged_value<std::chrono::milliseconds, 0, 1000>;

        STATIC_REQUIRE(std::default_initializable<timeout>);
        REQUIRE(timeout{} == 0ms);
        REQUIRE(timeout::MAX() == 1000ms);
    }

    SECTION("Values are checked against the range") {
        using timeout = ranged_value<std::chrono::milliseconds, 0, 1000>;

        REQUIRE(timeout{500ms}.get() == 500ms);
        REQUIRE_NOTHROW(timeout{1000ms});
        REQUIRE_THROWS_AS(timeout{1001ms}, ranged_value_exception);
        REQUIRE_THROWS_AS(timeout{-1ms}, ranged_value_exception);
    }

    SECTION("A strictly positive, unbounded-above duration") {
        using step = ranged_value<std::chrono::nanoseconds, 1,
                                  std::numeric_limits<std::chrono::nanoseconds::rep>::max()>;

        STATIC_REQUIRE_FALSE(std::default_initializable<step>);
        REQUIRE_NOTHROW(step{1ns});
        REQUIRE_NOTHROW(step{std::chrono::hours{24}});
        REQUIRE_THROWS_AS(step{0ns}, ranged_value_exception);
    }

    SECTION("Bound policies apply to durations too") {
        using window = ranged_value<std::chrono::seconds, 0, 10, rv_bound_policy::open>;

        STATIC_REQUIRE_FALSE(std::default_initializable<window>);
        REQUIRE_NOTHROW(window{5s});
        REQUIRE_THROWS_AS(window{0s}, ranged_value_exception);
        REQUIRE_THROWS_AS(window{10s}, ranged_value_exception);
    }

    SECTION("Assignment re-validates the value") {
        ranged_value<std::chrono::milliseconds, 0, 1000> value{100ms};

        value = 900ms;
        REQUIRE(value.get() == 900ms);

        REQUIRE_THROWS_AS((value = 2000ms), ranged_value_exception);
        REQUIRE(value.get() == 900ms);
    }

    SECTION("Durations compare against raw durations and each other") {
        using timeout = ranged_value<std::chrono::milliseconds, 0, 1000>;

        const timeout low{100ms};
        const timeout high{900ms};

        REQUIRE(low < high);
        REQUIRE(low == 100ms);
        REQUIRE(low < 200ms);
        REQUIRE(200ms > low);
    }

    SECTION("Decrementing steps by one tick") {
        ranged_value<std::chrono::milliseconds, 0, 1000> value{1ms};

        const auto previous{value--};

        REQUIRE(previous.get() == 1ms);
        REQUIRE(value.get() == 0ms);
        REQUIRE_THROWS_AS(--value, ranged_value_exception);
        REQUIRE(value.get() == 0ms);
    }

    SECTION("Incrementing steps by one tick") {
        ranged_value<std::chrono::milliseconds, 0, 1000> value{999ms};

        const auto previous{value++};

        REQUIRE(previous.get() == 999ms);
        REQUIRE(value.get() == 1000ms);
        REQUIRE_THROWS_AS(++value, ranged_value_exception);
        REQUIRE(value.get() == 1000ms);
    }

    SECTION("Compound addition and subtraction take durations, converting them to value_type") {
        ranged_value<std::chrono::milliseconds, 0, 1000> value{100ms};

        value += 400ms;
        REQUIRE(value.get() == 500ms);

        value -= std::chrono::seconds{0};
        REQUIRE(value.get() == 500ms);

        REQUIRE_THROWS_AS(value += 1s, ranged_value_exception);
        REQUIRE(value.get() == 500ms);
    }

    SECTION("The exception message includes the formatted value") {
        using timeout = ranged_value<std::chrono::milliseconds, 0, 1000>;

        try {
            [[maybe_unused]] const timeout value{1500ms};
            FAIL("Expected ranged_value_exception");
        } catch (const ranged_value_exception& e) {
            REQUIRE(std::string{e.what()}.find("1500ms") != std::string::npos);
        }
    }
}

TEST_CASE("ranged_value of a user-defined type", "[tca][engine][core][utilities][ranged_value]") {
    using level_range = ranged_value<level, level{1}, level{10}>;

    SECTION("A range excluding the value-initialized value has no default constructor") {
        STATIC_REQUIRE_FALSE(std::default_initializable<level_range>);
    }

    SECTION("A range including the value-initialized value defaults to it") {
        REQUIRE((ranged_value<level, level{0}, level{10}>{}).get() == level{0});
    }

    SECTION("Values are checked against the range") {
        REQUIRE(level_range{level{5}}.get() == level{5});
        REQUIRE_THROWS_AS(level_range{level{0}}, ranged_value_exception);
        REQUIRE_THROWS_AS(level_range{level{11}}, ranged_value_exception);
    }

    SECTION("Ranged values of the type compare by their value") {
        REQUIRE(level_range{level{2}} < level_range{level{3}});
        REQUIRE(level_range{level{2}} == level{2});
    }

    SECTION("A type with no formatter still gets an exception message") {
        try {
            [[maybe_unused]] const level_range value{level{42}};
            FAIL("Expected ranged_value_exception");
        } catch (const ranged_value_exception& e) {
            REQUIRE(std::string{e.what()} == "The given value is out of range");
        }
    }
}

#ifndef NDEBUG

TEMPLATE_TEST_CASE("debug_ranged_value tests (debug builds validate at runtime)",
                   "[tca][engine][core][utilities][ranged_value][debug_ranged_value]", int, float) {
    // See the comment in the first TEMPLATE_TEST_CASE about why these are named
    // constexpr locals instead of inline int literals.
    constexpr TestType RANGE_MIN{0};
    constexpr TestType RANGE_MAX{5};

    SECTION("Constructing with a value inside the range does not throw") {
        REQUIRE_NOTHROW((debug_ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{3}}));
    }

    SECTION("Constructing with the upper bound of a closed range does not throw") {
        REQUIRE_NOTHROW((debug_ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{5}}));
    }

    SECTION("Constructing with a value outside the range throws") {
        REQUIRE_THROWS_AS((debug_ranged_value<TestType, RANGE_MIN, RANGE_MAX>{TestType{6}}),
                          ranged_value_exception);
    }

    SECTION("The bound policy is honoured") {
        REQUIRE_THROWS_AS(
            (debug_ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::max_open>{
                TestType{5}}),
            ranged_value_exception);
        REQUIRE_THROWS_AS(
            (debug_ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::min_open>{
                TestType{0}}),
            ranged_value_exception);
        REQUIRE_THROWS_AS(
            (debug_ranged_value<TestType, RANGE_MIN, RANGE_MAX, rv_bound_policy::open>{
                TestType{0}}),
            ranged_value_exception);
    }
}

#endif // NDEBUG
