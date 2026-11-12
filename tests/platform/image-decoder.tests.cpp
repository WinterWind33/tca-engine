// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/images/image-decoder.hpp"

// C++ STL
#include <cstddef>
#include <filesystem>
#include <stdexcept>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Utilities
#include "utilities/test-assets.hpp"

namespace {

// A 4x2 fixture image: the top row is fully opaque red, the bottom row is fully opaque blue. This
// lets tests verify both the reported dimensions and that pixel data is laid out row-major (as
// documented on decoded_image), without depending on any particular image format's internals.
constexpr std::uint32_t FIXTURE_WIDTH{4};
constexpr std::uint32_t FIXTURE_HEIGHT{2};
constexpr std::uint32_t BYTES_PER_PIXEL{4};

} // namespace

TEST_CASE("image decoder tests", "[tca][engine][platform][image_decoder]") {
    using namespace tca::engine::platform::images;

    GIVEN("A valid PNG file") {
        const std::filesystem::path testPatternPath{
            tca::tests::utilities::get_test_pattern_path()};
        REQUIRE(std::filesystem::exists(testPatternPath));

        WHEN("Decoding it") {
            const decoded_image image{decode_image_from_file(testPatternPath)};

            THEN("The reported dimensions match the source file") {
                REQUIRE(image.width == FIXTURE_WIDTH);
                REQUIRE(image.height == FIXTURE_HEIGHT);
            }

            AND_THEN("The pixel buffer is tightly packed RGBA8, width * height * 4 bytes") {
                REQUIRE(image.pixels.size() == FIXTURE_WIDTH * FIXTURE_HEIGHT * BYTES_PER_PIXEL);
            }

            AND_THEN("The top row is opaque red, row-major, with no padding between rows") {
                for (std::uint32_t x{}; x < FIXTURE_WIDTH; ++x) {
                    const std::size_t pixelOffset{x * BYTES_PER_PIXEL};
                    REQUIRE(image.pixels[pixelOffset + 0] == std::byte{255}); // R
                    REQUIRE(image.pixels[pixelOffset + 1] == std::byte{0});   // G
                    REQUIRE(image.pixels[pixelOffset + 2] == std::byte{0});   // B
                    REQUIRE(image.pixels[pixelOffset + 3] == std::byte{255}); // A
                }
            }

            AND_THEN("The bottom row is opaque blue, immediately following the top row") {
                const std::size_t rowStride{FIXTURE_WIDTH * BYTES_PER_PIXEL};
                for (std::uint32_t x{}; x < FIXTURE_WIDTH; ++x) {
                    const std::size_t pixelOffset{rowStride + (x * BYTES_PER_PIXEL)};
                    REQUIRE(image.pixels[pixelOffset + 0] == std::byte{0});   // R
                    REQUIRE(image.pixels[pixelOffset + 1] == std::byte{0});   // G
                    REQUIRE(image.pixels[pixelOffset + 2] == std::byte{255}); // B
                    REQUIRE(image.pixels[pixelOffset + 3] == std::byte{255}); // A
                }
            }
        }
    }

    GIVEN("A path to a file that does not exist") {
        const std::filesystem::path missingPath{"res/images/does-not-exist.png"};
        REQUIRE(!std::filesystem::exists(missingPath));

        WHEN("Decoding it") {
            THEN("It throws a std::runtime_error") {
                REQUIRE_THROWS_AS(decode_image_from_file(missingPath), std::runtime_error);
            }
        }
    }
}
