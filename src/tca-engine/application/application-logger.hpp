// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Memory
#include <memory>
#include <string_view>

// Third-party
#include <spdlog/logger.h>

namespace tca::engine::application {

/**
 * @brief Retrieves the logger used for logging messages related to the application layer of the
 * engine. This should only be used within the "application" sub module of the engine, and not
 * outside of it.
 *
 * @return std::shared_ptr<spdlog::logger> A shared pointer to the logger used for logging messages
 * related to the application layer of the engine.
 */
std::shared_ptr<spdlog::logger> get_application_logger();

/**
 * @brief Starts the engine log for a run of the application: names the per-user log folder after
 * the application (and its company) and writes the run header at the top of the log.
 *
 * @note Called by run() before anything else logs, since the log folder can only be named before
 * the shared sink is created. Without an application name, the log goes to the debugger output and
 * stderr only.
 *
 * @param appName The name of the application.
 * @param companyName The name of the company publishing the application, possibly empty.
 */
void begin_application_log(std::string_view appName, std::string_view companyName);

} // namespace tca::engine::application
