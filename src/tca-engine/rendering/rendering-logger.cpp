// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/rendering-logger.hpp"

// C++ STL
#include <string_view>

#include "core/logging/logging.hpp"

namespace tca::engine::rendering {

constexpr std::string_view RENDERING_LOGGER_NAME{"rendering_logger"};

std::shared_ptr<spdlog::logger> get_rendering_logger() {
    static std::shared_ptr<spdlog::logger> logger{core::logging::get_logger(RENDERING_LOGGER_NAME)};

    return logger;
}

} // namespace tca::engine::rendering
