// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/system-information.hpp"

// C++ STL
#include <format>
#include <string>

// POSIX
#include <sys/utsname.h>

namespace tca::engine::platform {

std::string get_operating_system_description() noexcept {
    try {
        utsname systemName{};
        if (::uname(&systemName) != 0) {
            return "Linux";
        }

        // The kernel release and the machine, e.g. "Linux 6.8.0-45-generic x86_64": the
        // distribution would need /etc/os-release parsed, and the kernel is what tells crashes
        // apart more often.
        return std::format("Linux {} {}", systemName.release, systemName.machine);
    } catch (...) {
        return "Linux";
    }
}

} // namespace tca::engine::platform
