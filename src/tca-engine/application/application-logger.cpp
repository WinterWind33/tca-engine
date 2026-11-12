// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/application-logger.hpp"

// C++ STL
#include <chrono>
#include <sstream>
#include <string>
#include <string_view>

#include "core/logging/log-header.hpp"
#include "core/logging/logging.hpp"
#include "platform/folders.hpp"
#include "platform/system-information.hpp"

namespace tca::engine::application {

constexpr std::string_view APPLICATION_LOGGER_NAME{"application_logger"};

std::shared_ptr<spdlog::logger> get_application_logger() {
    static std::shared_ptr<spdlog::logger> logger{
        core::logging::get_logger(APPLICATION_LOGGER_NAME)};

    return logger;
}

void begin_application_log(const std::string_view appName, const std::string_view companyName) {
    core::logging::set_log_folder(platform::get_preferred_log_folder(appName, companyName));

    const auto operatingSystem{platform::get_operating_system_description()};
    const auto header{core::logging::make_log_header({
        .gameName = appName,
        .operatingSystem = operatingSystem,
        .startTime = std::chrono::system_clock::now(),
    })};

    // One record per line, so each carries the usual prefix and the header reads like the rest of
    // the log.
    const auto logger{get_application_logger()};
    std::istringstream lines{header};
    for (std::string line{}; std::getline(lines, line);) {
        logger->info(line);
    }
}

} // namespace tca::engine::application
