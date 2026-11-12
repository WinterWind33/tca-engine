// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/folders.hpp"

// C++ STL
#include <cstdint>
#include <cstdlib> // for std::getenv
#include <filesystem>
#include <string_view>
#include <system_error>
#include <vector>

// macOS
#include <mach-o/dyld.h>

// Third-party
#include <gsl/gsl>

namespace tca::engine::platform {

namespace {
constexpr std::string_view HOME_ENV_VAR{"HOME"};
}

std::filesystem::path get_preferred_installation_folder() noexcept {
    // On macOS, the preferred installation folder is typically "/Applications/".
    return std::filesystem::path{"/Applications/"};
}

std::filesystem::path get_preferred_user_data_folder() noexcept {
    // On macOS, the preferred user data folder is typically "/Users/Username/Library/Application
    // Support/". We can use the user's home directory and append "Library/Application Support".
    if (const std::filesystem::path homeDir{gsl::not_null(std::getenv(HOME_ENV_VAR.data())).get()};
        !homeDir.empty()) {
        return homeDir / "Library" / "Application Support";
    }
    // Fallback to the current directory if the HOME environment variable is not set.
    return std::filesystem::current_path();
}

std::filesystem::path get_preferred_installation_folder(const app_name_t appName) noexcept {
    if (appName.empty()) {
        // Fallback to the current directory if the application name is empty.
        return std::filesystem::current_path();
    }

    return get_preferred_installation_folder() / appName;
}

std::filesystem::path get_preferred_user_data_folder(const app_name_t appName) noexcept {
    if (appName.empty()) {
        // Fallback to the current directory if the application name is empty.
        return std::filesystem::current_path();
    }

    return get_preferred_user_data_folder() / appName;
}

std::filesystem::path get_preferred_log_folder(const app_name_t appName,
                                               const company_name_t companyName) noexcept {
    if (appName.empty()) {
        return {};
    }

    // On macOS, logs live in "~/Library/Logs/", one folder per application. Unlike Windows and
    // Linux there is no "logs" subfolder: the application's folder already is the log folder.
    const char* homeDir{std::getenv(HOME_ENV_VAR.data())};
    if (homeDir == nullptr || std::string_view{homeDir}.empty()) {
        return {};
    }

    std::filesystem::path resultingPath{std::filesystem::path{homeDir} / "Library" / "Logs"};
    if (!companyName.empty()) {
        resultingPath /= companyName;
    }

    resultingPath /= appName;

    return resultingPath;
}

std::filesystem::path get_executable_directory() noexcept {
    // _NSGetExecutablePath reports the path the executable was launched through, which can hold
    // symbolic links or "..": canonicalising it gives the folder the executable really lives in.
    std::uint32_t bufferSize{0};
    ::_NSGetExecutablePath(nullptr, &bufferSize);

    std::vector<char> buffer(bufferSize);
    if (::_NSGetExecutablePath(buffer.data(), &bufferSize) != 0) {
        return {};
    }

    std::error_code error{};
    const std::filesystem::path executablePath{
        std::filesystem::canonical(std::filesystem::path{buffer.data()}, error)};
    if (error) {
        return {};
    }

    return executablePath.parent_path();
}

std::filesystem::path get_asset_path(const std::filesystem::path& relativePath) noexcept {
    const std::filesystem::path executableDirectory{get_executable_directory()};
    if (executableDirectory.empty() || relativePath.empty()) {
        // Joining an empty path would add a trailing separator rather than give the folder back.
        return executableDirectory;
    }

    return executableDirectory / relativePath;
}

} // namespace tca::engine::platform
