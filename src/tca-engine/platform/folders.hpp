// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <filesystem>
#include <string_view>

namespace tca::engine::platform {

using app_name_t = std::string_view;
using company_name_t = std::string_view;

/**
 * @brief Retrieves the platform preferred installation folder. This is for x64 applications. This
 * is the folder where the application should be installed, and where it can find its resources. For
 * example:
 * - On Windows, this might be "C:\Program Files\".
 * - On Linux, this might be "/usr/local/".
 * - On macOS, this might be "/Applications/".
 *
 * @return std::filesystem::path The preferred installation folder path.
 */
std::filesystem::path get_preferred_installation_folder() noexcept;

/**
 * @brief Retrieves the platform preferred user data folder. This folder is where the application
 * should store user-specific data and configuration files. For example:
 * - On Windows, this might be "C:\Users\Username\AppData\Roaming\" (%AppData%).
 * - On Linux, this might be "/home/username/.config/".
 * - On macOS, this might be "/Users/username/Library/Application Support/"
 *
 * Falls back to the current directory if the preferred user data folder cannot be determined.
 *
 * @return std::filesystem::path The preferred user data folder path.
 */
std::filesystem::path get_preferred_user_data_folder() noexcept;

/**
 * @brief Retrieves the platform preferred installation folder. This is for x64 applications.
 * This is the folder where the application should be installed, and where it can find its
 * resources. For example:
 * - On Windows, this might be "C:\Program Files\MyApp".
 * - On Linux, this might be "/usr/local/MyApp".
 * - On macOS, this might be "/Applications/MyApp".
 *
 * @note Falls back to the current directory if the preferred installation folder cannot be
 * determined or if the input application name is empty.
 *
 * @param appName The name of the application, used to retrieve the subfolder in the installation
 * directory.
 * @return std::filesystem::path The preferred installation folder path.
 */
std::filesystem::path get_preferred_installation_folder(app_name_t appName) noexcept;

/**
 * @brief Get the preferred user data folder object. This folder is where the application should
 * store user-specific data and configuration files. For example:
 * - On Windows, this might be "C:\Users\Username\AppData\Roaming\MyApp" (%AppData%).
 * - On Linux, this might be "/home/username/.config/MyApp".
 * - On macOS, this might be "/Users/username/Library/Application Support/MyApp".
 *
 * @note Falls back to the current directory if the preferred user data folder cannot be determined
 * or if the input application name is empty.
 *
 * @param appName The name of the application, used to retrieve the subfolder in the user data
 * directory.
 * @return std::filesystem::path The preferred user data folder path.
 */
std::filesystem::path get_preferred_user_data_folder(app_name_t appName) noexcept;

/**
 * @brief Retrieves the folder an application's log files belong in. Logs are machine-local state,
 * not configuration, so this is not the (roaming) user data folder:
 * - On Windows, "C:\Users\Username\AppData\Local\MyCompany\MyApp\logs" (%LocalAppData%).
 * - On Linux, "$XDG_STATE_HOME/MyCompany/MyApp/logs", defaulting to
 *   "~/.local/state/MyCompany/MyApp/logs".
 * - On macOS, "~/Library/Logs/MyCompany/MyApp".
 *
 * Without a company name, the company folder is left out ("...\Local\MyApp\logs").
 *
 * @note Unlike the other helpers, this never falls back to the current directory: if the folder
 * cannot be determined or the application name is empty, it returns an empty path, which the
 * logging module turns into "no log file".
 *
 * @param appName The name of the application, used to name its subfolder.
 * @param companyName The name of the company publishing the application, used to name the folder
 * the application's own sits in. Optional.
 * @return std::filesystem::path The log folder path, or an empty path.
 */
std::filesystem::path get_preferred_log_folder(app_name_t appName,
                                               company_name_t companyName = {}) noexcept;

/**
 * @brief Retrieves the absolute path of the folder holding the running executable. Unlike the
 * working directory, this does not depend on how the game was launched (a shortcut, a launch
 * option, a double-click from another folder), so it is where shipped assets should be looked up.
 * - On Windows, it comes from GetModuleFileNameW.
 * - On Linux, from "/proc/self/exe".
 * - On macOS, from _NSGetExecutablePath, with symbolic links resolved.
 *
 * @return std::filesystem::path The executable's folder, or an empty path if it cannot be
 * determined.
 */
std::filesystem::path get_executable_directory() noexcept;

/**
 * @brief Resolves a path relative to the executable's folder (see get_executable_directory()),
 * e.g. get_asset_path("res/images/player.png"). The file is not required to exist.
 *
 * @note An empty relative path gives the executable's folder itself, and an absolute one is
 * returned unchanged. If the executable's folder cannot be determined, the result is empty.
 *
 * @param relativePath The asset's path relative to the executable's folder.
 * @return std::filesystem::path The asset's absolute path, or an empty path.
 */
std::filesystem::path get_asset_path(const std::filesystem::path& relativePath) noexcept;

} // namespace tca::engine::platform
