// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
// SPDX-License-Identifier: MIT
#include "rendering/gui/imgui/imgui-math-helpers.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

#include "core/math/math.hpp"

TEST_CASE("to_imvec2 function tests",
          "[tca][engine][rendering][gui][imgui][math-helpers][to_imvec2]") {
    using namespace tca::engine;

    SECTION("Conversion of math::vector2 default instance") {
        const math::vector2 vec{};
        const ImVec2 imvec{imgui::to_imvec2(vec)};

        REQUIRE(math::is_nearly_zero(imvec.x));
        REQUIRE(math::is_nearly_zero(imvec.y));
    }

    SECTION("Conversion of math::vector2::zero() value") {
        const math::vector2 vec{math::vector2::zero()};
        const ImVec2 imvec{imgui::to_imvec2(vec)};

        REQUIRE(math::is_nearly_zero(imvec.x));
        REQUIRE(math::is_nearly_zero(imvec.y));
    }

    SECTION("Conversion of math::vector2::x_axis_vector() value") {
        const math::vector2 vec{math::vector2::x_axis_vector()};
        const ImVec2 imvec{imgui::to_imvec2(vec)};

        REQUIRE(math::is_nearly_equal(imvec.x, 1.0f));
        REQUIRE(math::is_nearly_zero(imvec.y));
    }

    SECTION("Conversion of math::vector2::y_axis_vector() value") {
        const math::vector2 vec{math::vector2::y_axis_vector()};
        const ImVec2 imvec{imgui::to_imvec2(vec)};

        REQUIRE(math::is_nearly_zero(imvec.x));
        REQUIRE(math::is_nearly_equal(imvec.y, 1.0f));
    }
}

TEST_CASE("to_vector2 function tests",
          "[tca][engine][rendering][gui][imgui][math-helpers][to_vector2]") {
    using namespace tca::engine;

    SECTION("Conversion of ImVec2 default instance") {
        constexpr ImVec2 IMVEC{};
        const math::vector2 vec{imgui::to_vector2(IMVEC)};

        REQUIRE(math::is_nearly_zero(vec.x()));
        REQUIRE(math::is_nearly_zero(vec.y()));
    }

    SECTION("Conversion of ImVec2 with specific values") {
        constexpr ImVec2 IMVEC{3.5f, -2.0f};
        const math::vector2 vec{imgui::to_vector2(IMVEC)};

        REQUIRE(math::is_nearly_equal(vec.x(), 3.5));
        REQUIRE(math::is_nearly_equal(vec.y(), -2.0));
    }
}
