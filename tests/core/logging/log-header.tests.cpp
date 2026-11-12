// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/logging/log-header.hpp"

// C++ STL
#include <chrono>
#include <sstream>
#include <string>

// Third-party
#include <catch2/catch_test_macros.hpp>

using namespace tca::engine::core;

namespace {

logging::run_description make_full_description() {
    using namespace std::chrono;

    return logging::run_description{
        .gameName = "TestGame",
        .gameVersion = "1.2.3",
        .engineVersion = "0.20.2",
        .operatingSystem = "Windows 11 (10.0.26300)",
        .renderer = "DirectX 12",
        .startTime = sys_days{year{2026} / October / 21} + hours{9} + minutes{30},
    };
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

} // namespace

TEST_CASE("make_log_header() function tests", "[tca][engine][core][logging][log-header]") {
    SECTION("Every field of the description is in the header") {
        const auto header{logging::make_log_header(make_full_description())};

        CHECK(contains(header, "TestGame"));
        CHECK(contains(header, "1.2.3"));
        CHECK(contains(header, "0.20.2"));
        CHECK(contains(header, "Windows 11 (10.0.26300)"));
        CHECK(contains(header, "DirectX 12"));
    }

    SECTION("The start time is written in UTC as ISO 8601") {
        const auto header{logging::make_log_header(make_full_description())};

        CHECK(contains(header, "2026-10-21T09:30:00Z"));
    }

    SECTION("Each field has a line of its own, every line ends with a newline") {
        const auto header{logging::make_log_header(make_full_description())};

        REQUIRE_FALSE(header.empty());
        CHECK(header.back() == '\n');

        std::istringstream lines{header};
        std::size_t lineCount{0};
        for (std::string line{}; std::getline(lines, line);) {
            CHECK_FALSE(line.empty());
            ++lineCount;
        }
        CHECK(lineCount == 6);
    }

    SECTION("A field left empty reads as unknown rather than a blank value") {
        auto description{make_full_description()};
        description.renderer = {};
        description.gameVersion = {};

        const auto header{logging::make_log_header(description)};

        std::istringstream lines{header};
        std::size_t unknownCount{0};
        for (std::string line{}; std::getline(lines, line);) {
            CHECK_FALSE(line.ends_with(": "));
            if (line.ends_with(logging::UNKNOWN_RUN_FIELD)) {
                ++unknownCount;
            }
        }
        CHECK(unknownCount == 2);
    }
}
