// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/folders.hpp"

// C++ STL
#include <cstdlib> // for _dupenv_s, std::free
#include <filesystem>
#include <string_view>

// Windows
#include "platform/win32/windows.hpp"

#include <ShlObj.h>

// Third-party
#include <wil/resource.h>
#include <wil/win32_helpers.h>

namespace tca::engine::platform {

namespace {
constexpr std::string_view PROGRAM_FILES_ENV_VAR{"ProgramFiles"};
constexpr std::string_view APP_DATA_ENV_VAR{"APPDATA"};

// _dupenv_s is used instead of std::getenv because the latter is flagged as unsafe by MSVC
// (C4996) on this platform: it returns a pointer into a shared, non-thread-safe static buffer.
std::filesystem::path get_env_var_path(const std::string_view varName) noexcept {
    char* buffer{nullptr};
    if (std::size_t bufferSize{0};
        ::_dupenv_s(&buffer, &bufferSize, varName.data()) != 0 || buffer == nullptr) {
        return {};
    }

    const std::filesystem::path path{buffer};
    std::free(buffer);
    return path;
}

// SHGetKnownFolderPath asks the shell for the folder itself, so unlike an environment variable it
// cannot be unset or overridden by the user's session.
std::filesystem::path get_known_folder_path(const KNOWNFOLDERID& folderId) noexcept {
    wil::unique_cotaskmem_string buffer{};
    if (FAILED(::SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &buffer))) {
        return {};
    }

    return std::filesystem::path{buffer.get()};
}
} // namespace

std::filesystem::path get_preferred_installation_folder() noexcept {
    // On Windows, the preferred installation folder is typically "C:\Program Files\". We can use
    // the %ProgramFiles% environment variable to get the path.
    if (const std::filesystem::path path{get_env_var_path(PROGRAM_FILES_ENV_VAR)}; !path.empty()) {
        return path;
    }

    // Fallback to current directory if %ProgramFiles% is not set.
    return std::filesystem::current_path();
}

std::filesystem::path get_preferred_user_data_folder() noexcept {
    // On Windows, the preferred user data folder is typically "C:\Users\Username\AppData\Roaming\".
    // We can use the %APPDATA% environment variable to get the path.
    if (const std::filesystem::path path{get_env_var_path(APP_DATA_ENV_VAR)}; !path.empty()) {
        return path;
    }

    // Fallback to current directory if %APPDATA% is not set.
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

    std::filesystem::path resultingPath{get_known_folder_path(FOLDERID_LocalAppData)};
    if (resultingPath.empty()) {
        return {};
    }

    if (!companyName.empty()) {
        resultingPath /= companyName;
    }

    resultingPath /= appName;
    resultingPath /= "logs";

    return resultingPath;
}

std::filesystem::path get_executable_directory() noexcept {
    // GetModuleFileNameW names the running executable whatever the working directory is. WIL's
    // wrapper grows its buffer, so a path past MAX_PATH is not cut off.
    wil::unique_cotaskmem_string executablePath{};
    if (FAILED(wil::GetModuleFileNameW(nullptr, executablePath))) {
        return {};
    }

    return std::filesystem::path{executablePath.get()}.parent_path();
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
