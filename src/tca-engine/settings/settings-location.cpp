// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "settings/settings-location.hpp"

// C++ STL
#include <filesystem>
#include <string>
#include <string_view>

namespace tca::engine::settings {

std::filesystem::path get_settings_file_path(const platform::app_name_t appName,
                                             const std::string_view fileName) noexcept {
    try {
        const auto settingsFolder{platform::get_preferred_user_data_folder(appName)};

        // An empty name would give back the folder itself, which the store would then fail to
        // write: the default is a better answer than a path that cannot be a file.
        return settingsFolder / std::filesystem::path{std::string{
                                    fileName.empty() ? DEFAULT_SETTINGS_FILE_NAME : fileName}};
    } catch (...) {
        // The path helpers already fall back to the current directory when they cannot answer;
        // this only catches the allocation failing, which the caller cannot do anything with
        // either.
        return std::filesystem::path{std::string{DEFAULT_SETTINGS_FILE_NAME}};
    }
}

} // namespace tca::engine::settings
