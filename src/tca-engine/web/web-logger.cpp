// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "web/web-logger.hpp"

// C++ STL
#include <string_view>

#include "core/logging/logging.hpp"

namespace tca::engine::web::details {

namespace {
constexpr std::string_view WEB_LOGGER_NAME{"web_logger"};
}

std::shared_ptr<spdlog::logger> get_web_logger() {
    static std::shared_ptr<spdlog::logger> logger{core::logging::get_logger(WEB_LOGGER_NAME)};

    return logger;
}

} // namespace tca::engine::web::details
