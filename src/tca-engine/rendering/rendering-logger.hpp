// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <memory>

// Third-party
#include <spdlog/logger.h>

namespace tca::engine::rendering {

/**
 * @brief Retrieves the logger used for logging messages related to the rendering layer of the
 * engine. This should only be used within the "rendering" sub module of the engine, and not outside
 * of it.
 *
 * @return std::shared_ptr<spdlog::logger> A shared pointer to the logger used for logging messages
 * related to the rendering layer of the engine.
 */
std::shared_ptr<spdlog::logger> get_rendering_logger();

} // namespace tca::engine::rendering
