// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gpu-texture.hpp"

// C++ STL
#include <utility>

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("gpu_texture tests", "[tca][engine][rendering][gpu_texture]") {
    using namespace tca::engine::rendering;

    GIVEN("A default-constructed gpu_texture") {
        gpu_texture texture{};

        THEN("It has zero width and height") {
            REQUIRE(texture.get_width() == 0);
            REQUIRE(texture.get_height() == 0);
        }
    }

    GIVEN("A gpu_texture constructed with a releaser") {
        std::size_t releaseCallCount{};
        {
            gpu_texture texture{static_cast<gpu_texture::texture_id>(42), 100, 200,
                                [&releaseCallCount]() {
                                    ++releaseCallCount;
                                }};

            THEN("Its id, width and height are as constructed") {
                REQUIRE(texture.get_id() == static_cast<gpu_texture::texture_id>(42));
                REQUIRE(texture.get_width() == 100);
                REQUIRE(texture.get_height() == 200);
            }

            AND_THEN("The releaser has not been called yet") {
                REQUIRE(releaseCallCount == 0);
            }
        }

        THEN("The releaser is called exactly once when the texture is destroyed") {
            REQUIRE(releaseCallCount == 1);
        }
    }

    GIVEN("A gpu_texture with a releaser, that gets move-constructed") {
        std::size_t releaseCallCount{};

        {
            gpu_texture original{static_cast<gpu_texture::texture_id>(7), 10, 20,
                                 [&releaseCallCount]() {
                                     ++releaseCallCount;
                                 }};

            gpu_texture moved{std::move(original)};

            THEN("The moved-to texture owns the resource and the moved-from one does not") {
                REQUIRE(moved.get_id() == static_cast<gpu_texture::texture_id>(7));
                REQUIRE(original.get_id() == ImTextureID_Invalid);
            }

            AND_THEN("The releaser has not been called yet") {
                REQUIRE(releaseCallCount == 0);
            }
        }

        THEN("The releaser is invoked exactly once, after both go out of scope") {
            REQUIRE(releaseCallCount == 1);
        }
    }

    GIVEN("A gpu_texture that gets move-assigned over another one with a releaser") {
        std::size_t firstReleaseCallCount{};
        std::size_t secondReleaseCallCount{};

        gpu_texture first{static_cast<gpu_texture::texture_id>(1), 1, 1,
                          [&firstReleaseCallCount]() {
                              ++firstReleaseCallCount;
                          }};
        gpu_texture second{static_cast<gpu_texture::texture_id>(2), 2, 2,
                           [&secondReleaseCallCount]() {
                               ++secondReleaseCallCount;
                           }};

        first = std::move(second);

        THEN("The first texture's original releaser is invoked immediately by the assignment") {
            REQUIRE(firstReleaseCallCount == 1);
            REQUIRE(secondReleaseCallCount == 0);
        }

        AND_THEN("The first texture now owns what used to be the second one's resource") {
            REQUIRE(first.get_id() == static_cast<gpu_texture::texture_id>(2));
        }
    }
}
