// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/platform-logger.hpp"

// C++ STL
#include <string_view>

#include "core/logging/logging.hpp"

namespace tca::engine::platform {

constexpr std::string_view PLATFORM_LOGGER_NAME{"platform_logger"};

std::shared_ptr<spdlog::logger> get_platform_logger() {
    static std::shared_ptr<spdlog::logger> logger{core::logging::get_logger(PLATFORM_LOGGER_NAME)};

    return logger;
}

} // namespace tca::engine::platform
