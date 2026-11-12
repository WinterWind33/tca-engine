// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/win32/windows-type-traits.hpp"

// C++ STL
#include <string_view>

// Windows
#include "platform/win32/windows.hpp"

// Third-party libraries
#include <wil/stl.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("to_win32_string() function tests",
          "[tca][engine][platform][win32][type_traits][to_win32_string]") {
    using namespace tca::engine::platform::win32::type_traits;

    GIVEN("An empty string") {
        constexpr std::string_view INPUT{};
        WHEN("Converting to win32_string_t") {
            win32_string_t result{};
            REQUIRE_NOTHROW(result = to_win32_string(INPUT));

            THEN("The result should be an empty string") {
                REQUIRE(result.empty());
            }
        }
    }

    GIVEN("A non-empty string - ASCII only characters") {
        constexpr std::string_view INPUT{"Hello, World!"};
        WHEN("Converting to win32_string_t") {
            win32_string_t result{};
            REQUIRE_NOTHROW(result = to_win32_string(INPUT));

            THEN("The result should be the same string") {
                const win32_string_t expectedResult{TEXT("Hello, World!")};
                REQUIRE(result == expectedResult);
            }
        }
    }
}

TEST_CASE("from_win32_string_view() function tests",
          "[tca][engine][platform][win32][type_traits][from_win32_string_view]") {
    using namespace tca::engine::platform::win32::type_traits;

    GIVEN("An empty win32_string_view_t") {
        constexpr win32_string_view_t INPUT{};
        WHEN("Converting to std::string") {
            std::string result{};
            REQUIRE_NOTHROW(result = from_win32_string_view(INPUT));

            THEN("The result should be an empty string") {
                REQUIRE(result.empty());
            }
        }
    }

    GIVEN("A non-empty win32_string_view_t - ASCII only characters") {
        constexpr win32_string_view_t INPUT{TEXT("Hello, World!")};

        WHEN("Converting to std::string") {
            std::string result{};
            REQUIRE_NOTHROW(result = from_win32_string_view(INPUT));

            THEN("The result should be the same string") {
                const std::string expectedResult{"Hello, World!"};
                REQUIRE(result == expectedResult);
            }
        }
    }
}
