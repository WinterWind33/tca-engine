// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <memory>

// Third-party
#include <spdlog/logger.h>

namespace tca::engine::platform {

/**
 * @brief Retrieves the logger used for logging messages related to the platform layer of the
 * engine. This should only be used within the "platform" sub module of the engine, and not outside
 * of it.
 *
 * @return std::shared_ptr<spdlog::logger> A shared pointer to the logger used for logging messages
 * related to the platform layer of the engine.
 */
std::shared_ptr<spdlog::logger> get_platform_logger();

} // namespace tca::engine::platform
