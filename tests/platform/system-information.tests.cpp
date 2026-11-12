// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/system-information.hpp"

// C++ STL
#include <string>

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("get_operating_system_description() function tests",
          "[tca][engine][platform][system-information]") {
    using namespace tca::engine;

    std::string description{};
    REQUIRE_NOTHROW(description = platform::get_operating_system_description());

    SECTION("It is never empty") {
        CHECK_FALSE(description.empty());
    }

    SECTION("It names the platform") {
#if defined(_WIN32)
        CHECK(description.starts_with("Windows"));
#elif defined(__linux__)
        CHECK(description.starts_with("Linux"));
#elif defined(__APPLE__)
        CHECK(description.starts_with("macOS"));
#endif
    }

    SECTION("It carries a version number, not just the platform's name") {
        CHECK(description.find_first_of("0123456789") != std::string::npos);
    }

    SECTION("It fits on one line of the log header") {
        CHECK(description.find('\n') == std::string::npos);
    }
}
