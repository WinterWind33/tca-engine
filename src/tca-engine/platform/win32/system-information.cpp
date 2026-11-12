// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/system-information.hpp"

// C++ STL
#include <format>
#include <string>

// Windows
#include "platform/win32/windows.hpp"

namespace tca::engine::platform {

namespace {
// Windows 11 kept the 10.0 version number: only the build number tells them apart.
constexpr DWORD FIRST_WINDOWS_11_BUILD{22000};

using rtl_get_version_t = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

// GetVersionEx() reports the version the executable's manifest declares support for, not the one
// it runs on, so the real version is asked to ntdll directly.
bool get_real_version(RTL_OSVERSIONINFOW& versionInfo) noexcept {
    const HMODULE ntdll{::GetModuleHandleW(L"ntdll.dll")};
    if (ntdll == nullptr) {
        return false;
    }

    const auto rtlGetVersion{
        reinterpret_cast<rtl_get_version_t>(::GetProcAddress(ntdll, "RtlGetVersion"))};
    if (rtlGetVersion == nullptr) {
        return false;
    }

    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    // RtlGetVersion returns an NTSTATUS, where 0 is STATUS_SUCCESS.
    return rtlGetVersion(&versionInfo) == 0;
}
} // namespace

std::string get_operating_system_description() noexcept {
    try {
        RTL_OSVERSIONINFOW versionInfo{};
        if (!get_real_version(versionInfo)) {
            return "Windows";
        }

        std::string name{"Windows"};
        if (versionInfo.dwMajorVersion == 10) {
            name =
                versionInfo.dwBuildNumber >= FIRST_WINDOWS_11_BUILD ? "Windows 11" : "Windows 10";
        }

        return std::format("{} ({}.{}.{})", name, versionInfo.dwMajorVersion,
                           versionInfo.dwMinorVersion, versionInfo.dwBuildNumber);
    } catch (...) {
        return "Windows";
    }
}

} // namespace tca::engine::platform
