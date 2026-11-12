// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/logging/log-header.hpp"

// C++ STL
#include <chrono>
#include <format>
#include <iterator>
#include <string>
#include <string_view>

namespace tca::engine::core::logging {

namespace {
void append_line(std::string& header, const std::string_view key, const std::string_view value) {
    std::format_to(std::back_inserter(header), "{}: {}\n", key,
                   value.empty() ? UNKNOWN_RUN_FIELD : value);
}
} // namespace

std::string make_log_header(const run_description& description) {
    std::string header{};
    append_line(header, "game", description.gameName);
    append_line(header, "game version", description.gameVersion);
    append_line(header, "engine version", description.engineVersion);
    append_line(header, "operating system", description.operatingSystem);
    append_line(header, "renderer", description.renderer);

    // system_clock counts UTC, and whole seconds are enough to match a log with a player's report.
    append_line(
        header, "start time",
        std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(description.startTime)));

    return header;
}

} // namespace tca::engine::core::logging
