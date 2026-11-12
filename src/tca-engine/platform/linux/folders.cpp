// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/folders.hpp"

// C++ STL
#include <cstdlib> // for std::getenv
#include <filesystem>
#include <string_view>
#include <system_error>

// Third-party
#include <gsl/gsl>

namespace tca::engine::platform {

namespace {
constexpr std::string_view HOME_ENV_VAR{"HOME"};
constexpr std::string_view XDG_STATE_HOME_ENV_VAR{"XDG_STATE_HOME"};

std::filesystem::path get_env_var_path(const std::string_view varName) noexcept {
    if (const char* value{std::getenv(varName.data())}; value != nullptr) {
        return std::filesystem::path{value};
    }

    return {};
}

// $XDG_STATE_HOME, defaulting to "~/.local/state". The XDG Base Directory spec says a relative
// $XDG_STATE_HOME is invalid and must be ignored, so it falls back to the default too. Empty if
// neither can be determined.
std::filesystem::path get_state_home_folder() noexcept {
    if (const std::filesystem::path stateHome{get_env_var_path(XDG_STATE_HOME_ENV_VAR)};
        stateHome.is_absolute()) {
        return stateHome;
    }

    if (const std::filesystem::path homeDir{get_env_var_path(HOME_ENV_VAR)}; !homeDir.empty()) {
        return homeDir / ".local" / "state";
    }

    return {};
}
} // namespace

std::filesystem::path get_preferred_installation_folder() noexcept {
    // On Linux, the preferred installation folder is typically "/usr/local/".
    return std::filesystem::path{"/usr/local/"};
}

std::filesystem::path get_preferred_user_data_folder() noexcept {
    // On Linux, the preferred user data folder is typically "/home/username/.config/".
    // We can use the user's home directory and append ".config".
    if (const std::filesystem::path homeDir{gsl::not_null(std::getenv(HOME_ENV_VAR.data())).get()};
        !homeDir.empty()) {
        return homeDir / ".config";
    }
    // Fallback to the current directory if the HOME environment variable is not set.
    return std::filesystem::current_path();
}

std::filesystem::path get_preferred_installation_folder(const app_name_t appName) noexcept {
    if (appName.empty()) {
        // Fallback to the current directory if the application name is empty.
        return std::filesystem::current_path();
    }

    // On Linux, the preferred installation folder is typically "/usr/local/MyApp".
    // We can use the "/usr/local" directory as the base path.
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

    std::filesystem::path resultingPath{get_state_home_folder()};
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
    // "/proc/self/exe" is a symbolic link the kernel keeps pointing at the running executable,
    // whatever the working directory or argv[0] say.
    std::error_code error{};
    const std::filesystem::path executablePath{
        std::filesystem::read_symlink("/proc/self/exe", error)};
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
