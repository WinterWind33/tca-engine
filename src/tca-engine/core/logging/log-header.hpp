// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <chrono>
#include <string>
#include <string_view>

namespace tca::engine::core::logging {

/**
 * @brief What the first lines of a log say about the run that wrote it, so a log a player sends
 * explains itself without a back-and-forth about versions and hardware.
 */
struct run_description {
    std::string_view gameName{};
    std::string_view gameVersion{};
    std::string_view engineVersion{};
    std::string_view operatingSystem{};
    std::string_view renderer{};
    std::chrono::system_clock::time_point startTime{};
};

/**
 * @brief The value printed for a field of the run description that was left empty.
 */
inline constexpr std::string_view UNKNOWN_RUN_FIELD{"unknown"};

/**
 * @brief Builds the header written at the top of every log: one "key: value" line per field of
 * the description, with the start time in UTC as ISO 8601 (e.g. "2026-10-21T09:30:00Z") and
 * UNKNOWN_RUN_FIELD for any field left empty.
 *
 * @param description The run to describe.
 * @return The header, one line per field, each ending with a newline.
 */
[[nodiscard]] std::string make_log_header(const run_description& description);

} // namespace tca::engine::core::logging
