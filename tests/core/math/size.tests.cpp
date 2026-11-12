// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/math/size.hpp"

// C++ STL
#include <tuple>
#include <type_traits>

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("size2 unit tests", "[tca][engine][math][size2]") {
    using namespace tca::engine;

    SECTION("Default constructor initializes to zero") {
        const math::size2i size{};
        REQUIRE(size.width == 0);
        REQUIRE(size.height == 0);
    }

    SECTION("Designated initialization sets both dimensions") {
        const math::size2u size{.width = 640U, .height = 360U};
        REQUIRE(size.width == 640U);
        REQUIRE(size.height == 360U);
    }

    SECTION("Equality") {
        constexpr math::size2i a{.width = 320, .height = 180};
        STATIC_REQUIRE(a == math::size2i{.width = 320, .height = 180});
        STATIC_REQUIRE(a != math::size2i{.width = 180, .height = 320});
    }

    SECTION("is_empty()") {
        STATIC_REQUIRE(math::size2i{}.is_empty());
        STATIC_REQUIRE(math::size2i{.width = 0, .height = 10}.is_empty());
        STATIC_REQUIRE(math::size2i{.width = 10, .height = 0}.is_empty());
        STATIC_REQUIRE(math::size2i{.width = -10, .height = 10}.is_empty());
        STATIC_REQUIRE(math::size2d{.width = 0.0, .height = 1.0}.is_empty());
        STATIC_REQUIRE_FALSE(math::size2u{.width = 1U, .height = 1U}.is_empty());
        STATIC_REQUIRE_FALSE(math::size2d{.width = 0.5, .height = 0.5}.is_empty());
    }

    SECTION("operator[] accesses the width at 0 and the height at 1") {
        math::size2i size{.width = 640, .height = 360};
        REQUIRE(size[0] == 640);
        REQUIRE(size[1] == 360);

        size[0] = 320;
        size[1] = 180;
        REQUIRE(size == math::size2i{.width = 320, .height = 180});

        constexpr math::size2u constSize{.width = 4U, .height = 2U};
        STATIC_REQUIRE(constSize[0] == 4U);
        STATIC_REQUIRE(constSize[1] == 2U);
    }

    SECTION("Tuple-like access") {
        STATIC_REQUIRE(std::tuple_size_v<math::size2i> == 2);
        STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, math::size2f>, float>);
        STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<1, math::size2f>, float>);

        SECTION("get<I>() accesses the width at 0 and the height at 1") {
            math::size2i size{.width = 640, .height = 360};
            REQUIRE(math::get<0>(size) == 640);
            REQUIRE(math::get<1>(size) == 360);

            math::get<1>(size) = 480;
            REQUIRE(size.height == 480);

            STATIC_REQUIRE(math::get<0>(math::size2u{.width = 7U, .height = 9U}) == 7U);
        }

        SECTION("Structured bindings") {
            const auto [width, height] = math::size2d{.width = 1.5, .height = 2.5};
            REQUIRE(width == 1.5);
            REQUIRE(height == 2.5);
        }
    }

    SECTION("is_nearly_equal()") {
        SECTION("Nearly equal sizes compare equal") {
            const math::size2d a{.width = 0.1 + 0.2, .height = 1.0};
            const math::size2d b{.width = 0.3, .height = 1.0};
            REQUIRE(math::is_nearly_equal(a, b));
        }

        SECTION("Sizes differing in one dimension do not compare equal") {
            const math::size2d a{.width = 1.0, .height = 1.0};
            REQUIRE_FALSE(math::is_nearly_equal(a, math::size2d{.width = 1.1, .height = 1.0}));
            REQUIRE_FALSE(math::is_nearly_equal(a, math::size2d{.width = 1.0, .height = 1.1}));
        }

        SECTION("A custom epsilon widens the tolerance") {
            const math::size2f a{.width = 1.0F, .height = 2.0F};
            const math::size2f b{.width = 1.05F, .height = 1.95F};
            REQUIRE(math::is_nearly_equal(a, b, 0.1F));
            REQUIRE_FALSE(math::is_nearly_equal(a, b, 0.01F));
        }
    }

    SECTION("is_nearly_zero()") {
        REQUIRE(math::is_nearly_zero(math::size2d{}));
        REQUIRE(math::is_nearly_zero(math::size2d{.width = 1e-3, .height = -1e-3}, 1e-2));
        REQUIRE_FALSE(math::is_nearly_zero(math::size2d{.width = 0.0, .height = 0.1}));
    }

    SECTION("to_vector2()") {
        const math::vector2 vec{math::to_vector2(math::size2i{.width = 640, .height = 360})};
        REQUIRE(vec.x() == 640.0);
        REQUIRE(vec.y() == 360.0);
    }

    SECTION("to_size2()") {
        SECTION("Floating-point sizes keep the components as they are") {
            const math::size2d size{math::to_size2<double>(math::vector2{12.25, 7.5})};
            REQUIRE(size.width == 12.25);
            REQUIRE(size.height == 7.5);
        }

        SECTION("Integral sizes round each component to the nearest integer") {
            const math::size2i size{math::to_size2<std::int32_t>(math::vector2{959.9999, 540.4})};
            REQUIRE(size == math::size2i{.width = 960, .height = 540});
        }

        SECTION("Halves round away from zero") {
            const math::size2i size{math::to_size2<std::int32_t>(math::vector2{2.5, -2.5})};
            REQUIRE(size == math::size2i{.width = 3, .height = -3});
        }
    }

    SECTION("Round trip through vector2 preserves an integral size") {
        constexpr math::size2u original{.width = 1280U, .height = 720U};
        REQUIRE(math::to_size2<std::uint32_t>(math::to_vector2(original)) == original);
    }
}
