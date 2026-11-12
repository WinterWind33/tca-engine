// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/system-information.hpp"

// C++ STL
#include <array>
#include <cstddef>
#include <format>
#include <string>

// POSIX
#include <sys/sysctl.h>
#include <sys/utsname.h>

namespace tca::engine::platform {

std::string get_operating_system_description() noexcept {
    try {
        // The marketing version, e.g. "14.5", which is what a player knows their Mac by.
        std::array<char, 64> productVersion{};
        if (std::size_t size{productVersion.size()};
            ::sysctlbyname("kern.osproductversion", productVersion.data(), &size, nullptr, 0) ==
            0) {
            return std::format("macOS {}", productVersion.data());
        }

        // Older systems lack kern.osproductversion: the Darwin version still carries a number.
        if (utsname systemName{}; ::uname(&systemName) == 0) {
            return std::format("macOS (Darwin {})", systemName.release);
        }

        return "macOS";
    } catch (...) {
        return "macOS";
    }
}

} // namespace tca::engine::platform
