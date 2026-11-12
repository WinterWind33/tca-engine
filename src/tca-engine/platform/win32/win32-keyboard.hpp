// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <optional>

// Input
#include "input/key.hpp"

// Platform
#include "platform/win32/win32-application-window.hpp" // for lparam_t

namespace tca::engine::platform::win32 {

using scan_code = std::uint16_t;

scan_code scan_code_from_lparam(details::lparam_t param) noexcept;

std::optional<input::key> key_from_scan_code(scan_code scanCode) noexcept;

} // namespace tca::engine::platform::win32
