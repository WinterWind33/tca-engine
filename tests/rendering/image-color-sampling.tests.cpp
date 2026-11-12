// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/image-color-sampling.hpp"

// C++ STL
#include <filesystem>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Utilities
#include "utilities/test-assets.hpp"

TEST_CASE("image color sampling tests", "[tca][engine][rendering][image_color_sampling]") {
    using namespace tca::engine::rendering;

    GIVEN("A decoded fixture whose top half is opaque red and bottom half is opaque blue") {
        const std::filesystem::path testPatternPath{
            tca::tests::utilities::get_test_pattern_path()};
        REQUIRE(std::filesystem::exists(testPatternPath));
        const tca::engine::platform::images::decoded_image decodedImage{
            tca::engine::platform::images::decode_image_from_file(testPatternPath)};

        WHEN("Computing its average color") {
            const color averageColor{average_color(decodedImage)};

            THEN("It is the midpoint between red and blue, fully opaque") {
                REQUIRE(averageColor.r == 127);
                REQUIRE(averageColor.g == 0);
                REQUIRE(averageColor.b == 127);
                REQUIRE(averageColor.a == 255);
            }
        }
    }

    GIVEN("An empty decoded image") {
        const tca::engine::platform::images::decoded_image emptyImage{};

        WHEN("Computing its average color") {
            const color averageColor{average_color(emptyImage)};

            THEN("It falls back to black rather than dividing by zero") {
                REQUIRE(averageColor == colors::BLACK);
            }
        }
    }
}
