// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdlib>
#include <string>
#include <vector>

namespace tca::engine::web {

/**
 * @brief Which sub-parser produces a diagnostic (HTLM, CSS).
 */
enum class diagnostic_source {
    html,
    css,
};

/**
 * @brief Severity of a parse diagnostic. Nothing in this module ever throws; severity is how
 * callers distinguish "recovered, page is fine" from "recovered, but badly".
 */
enum class diagnostic_severity {
    info,
    warning,
    error,
};

/**
 * @brief A single parse issue
 */
struct diagnostic_data {
    diagnostic_source source{};
    diagnostic_severity severity{};
    std::size_t line{};
    std::size_t column{};
    std::string message{};
};

/**
 * @brief A collection of diagnostics.
 */
using diagnostic_collection = std::vector<diagnostic_data>;

} // namespace tca::engine::web
