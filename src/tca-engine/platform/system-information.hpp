// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <string>

namespace tca::engine::platform {

/**
 * @brief A short, human-readable description of the operating system the game runs on, with its
 * version (e.g. "Windows 11 (10.0.26300)", "Linux 6.8.0 x86_64"), for the log header.
 *
 * @note Never empty: if the version can't be read, the name of the platform alone is returned.
 *
 * @return The description.
 */
[[nodiscard]] std::string get_operating_system_description() noexcept;

} // namespace tca::engine::platform
