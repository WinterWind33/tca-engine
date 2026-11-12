// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <filesystem>
#include <string_view>

#include "platform/folders.hpp"

namespace tca::engine::settings {

/**
 * @brief The name a settings file is given when the game does not ask for another one.
 */
inline constexpr std::string_view DEFAULT_SETTINGS_FILE_NAME{"settings.json"};

/**
 * @brief Where a game's settings file belongs on this platform, so that a game names itself
 *  rather than a path.
 *
 * The folder is the one platform::get_preferred_user_data_folder() picks for the application —
 * `%AppData%\<app>` on Windows, `~/.config/<app>` on Linux, `~/Library/Application Support/<app>`
 * on macOS — which also means the folder a game already keeps its other user data in. The folder
 * itself does not have to exist: saving creates the folders leading to the file.
 *
 * A game only needs the second parameter if it keeps more than one settings file (one per player
 * profile, say) or wants a different extension because it uses a different formatter.
 *
 * @param appName The application name, which names the folder under the user data directory.
 * @param fileName The name of the settings file within that folder.
 * @return The full path to the settings file.
 */
[[nodiscard]] std::filesystem::path get_settings_file_path(
    platform::app_name_t appName, std::string_view fileName = DEFAULT_SETTINGS_FILE_NAME) noexcept;

} // namespace tca::engine::settings
