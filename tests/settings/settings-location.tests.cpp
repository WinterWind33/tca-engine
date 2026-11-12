// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "settings/settings-location.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

// C++ STL
#include <filesystem>
#include <string_view>

#include "platform/folders.hpp"

using namespace tca::engine;

TEST_CASE("settings file location tests", "[tca][engine][settings][settings_location]") {
    constexpr std::string_view appName{"TestGame"};

    SECTION("The settings file sits in the folder the platform keeps user data in") {
        const auto settingsFilePath{settings::get_settings_file_path(appName)};

        REQUIRE(settingsFilePath.parent_path() ==
                platform::get_preferred_user_data_folder(appName));
        REQUIRE(settingsFilePath.filename() == settings::DEFAULT_SETTINGS_FILE_NAME);
    }

    SECTION("A game that wants to name its own file gets that name in the same folder") {
        const auto settingsFilePath{settings::get_settings_file_path(appName, "player-1.cfg")};

        REQUIRE(settingsFilePath.parent_path() ==
                platform::get_preferred_user_data_folder(appName));
        REQUIRE(settingsFilePath.filename() == "player-1.cfg");
    }

    SECTION("An empty file name falls back to the default instead of naming a folder") {
        const auto settingsFilePath{settings::get_settings_file_path(appName, {})};

        REQUIRE(settingsFilePath.filename() == settings::DEFAULT_SETTINGS_FILE_NAME);
    }

    SECTION("An empty app name still names a file, as the platform helpers still name a folder") {
        const auto settingsFilePath{settings::get_settings_file_path({})};

        REQUIRE(settingsFilePath.filename() == settings::DEFAULT_SETTINGS_FILE_NAME);
        REQUIRE(settingsFilePath.parent_path() == std::filesystem::current_path());
    }
}
