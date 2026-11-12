// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/folders.hpp"
#include "testing/scoped-temp-directory.hpp"

// C++ STL
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
// Windows
#include "platform/win32/windows.hpp"

#include <ShlObj.h>
#endif

// Third-party
#include <catch2/catch_test_macros.hpp>
#if defined(_WIN32)
#include <wil/resource.h>
#endif

TEST_CASE("platform folders tests", "[tca][engine][platform][folders]") {
    using namespace tca::engine;

    // These are simple tests, we do not dig into the actual paths returned by the functions, as
    // they can vary greatly between platforms and environments. We just want to ensure that the
    // functions return non-empty strings.
    SECTION("Valid app names") {
        constexpr std::string_view appName{"TestApp"};

        const auto installationFolder{platform::get_preferred_installation_folder(appName)};
        REQUIRE_FALSE(installationFolder.empty());
        REQUIRE(std::filesystem::exists(installationFolder.parent_path()));
        REQUIRE(std::filesystem::is_directory(installationFolder.parent_path()));
        REQUIRE(installationFolder.filename() == appName);
        REQUIRE(installationFolder != std::filesystem::current_path());
        REQUIRE(installationFolder != std::filesystem::current_path() / appName);

        const auto userDataFolder{platform::get_preferred_user_data_folder(appName)};
        REQUIRE_FALSE(userDataFolder.empty());
        REQUIRE(std::filesystem::exists(userDataFolder.parent_path()));
        REQUIRE(std::filesystem::is_directory(userDataFolder.parent_path()));
        REQUIRE(userDataFolder.filename() == appName);
        REQUIRE(userDataFolder != std::filesystem::current_path());
        REQUIRE(userDataFolder != std::filesystem::current_path() / appName);
    }

    SECTION("Invalid app names") {
        constexpr std::string_view emptyAppName{};

        const auto installationFolder{platform::get_preferred_installation_folder(emptyAppName)};
        REQUIRE_FALSE(installationFolder.empty());
        REQUIRE(installationFolder == std::filesystem::current_path());

        const auto userDataFolder{platform::get_preferred_user_data_folder(emptyAppName)};
        REQUIRE_FALSE(userDataFolder.empty());
        REQUIRE(userDataFolder == std::filesystem::current_path());
    }
}

namespace {

#if defined(_WIN32)
// Asks the shell rather than reading %LOCALAPPDATA%, as the engine does: the variable can be unset
// or point elsewhere while the folder itself is still there.
std::optional<std::filesystem::path> get_local_app_data_folder() {
    wil::unique_cotaskmem_string buffer{};
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &buffer))) {
        return std::nullopt;
    }

    return std::filesystem::path{buffer.get()};
}
#elif defined(__linux__)
std::optional<std::string> read_env_var(const char* name) {
    if (const char* value{std::getenv(name)}; value != nullptr) {
        return std::string{value};
    }

    return std::nullopt;
}

// Sets (or unsets, with std::nullopt) an environment variable for the lifetime of the object,
// restoring the previous value afterwards so other tests see the environment they started with.
class scoped_env_var final {
public:
    scoped_env_var(const char* name, const std::optional<std::string>& value)
        : m_name{name},
          m_previous{read_env_var(name)} {
        apply(value);
    }

    ~scoped_env_var() {
        apply(m_previous);
    }

    scoped_env_var(const scoped_env_var&) = delete;
    scoped_env_var& operator=(const scoped_env_var&) = delete;

private:
    void apply(const std::optional<std::string>& value) const {
        if (value.has_value()) {
            ::setenv(m_name, value->c_str(), 1);
        } else {
            ::unsetenv(m_name);
        }
    }

    const char* m_name;
    std::optional<std::string> m_previous;
};
#endif

} // namespace

TEST_CASE("platform log folder tests", "[tca][engine][platform][folders][log-folder]") {
    using namespace tca::engine;

    constexpr std::string_view appName{"TestGame"};
    constexpr std::string_view companyName{"TestCompany"};

    SECTION("The log folder is absolute, named after the game and outside the working directory") {
        const auto logFolder{platform::get_preferred_log_folder(appName)};

        REQUIRE_FALSE(logFolder.empty());
        CHECK(logFolder.is_absolute());

        bool namedAfterTheGame{false};
        for (const auto& component : logFolder) {
            namedAfterTheGame = namedAfterTheGame || component == appName;
        }
        CHECK(namedAfterTheGame);

        // Compared component by component rather than through std::filesystem::relative, which
        // gives an empty path when the two are on different drives (e.g. D:\repo and C:\Users).
        const auto workingDirectory{std::filesystem::current_path()};
        const auto [workingDirectoryEnd,
                    logFolderEnd]{std::ranges::mismatch(workingDirectory, logFolder)};
        CHECK(workingDirectoryEnd != workingDirectory.end());
    }

    SECTION("The log folder is not the roaming user data folder settings live in") {
        CHECK(platform::get_preferred_log_folder(appName) !=
              platform::get_preferred_user_data_folder(appName));
    }

    SECTION("An empty app name gives no folder rather than the working directory") {
        const auto logFolder{platform::get_preferred_log_folder({})};

        CHECK(logFolder.empty());
    }

    SECTION("A company name puts the game's folder inside the company's") {
        const auto logFolder{platform::get_preferred_log_folder(appName, companyName)};
        REQUIRE_FALSE(logFolder.empty());

        // Walk up to the game's folder (on macOS it is the last component, elsewhere "logs" follows
        // it) and check the company's folder holds it, in the same base folder as without one.
        auto gameFolder{logFolder};
        while (!gameFolder.empty() && gameFolder.filename() != appName) {
            gameFolder = gameFolder.parent_path();
        }
        REQUIRE(gameFolder.filename() == appName);
        CHECK(gameFolder.parent_path().filename() == companyName);

        // Compared as paths relative to the game's folder: on macOS that is "." and appending it
        // would give ".../TestGame/.", which is not equal to ".../TestGame".
        const auto relativeToGameFolder{logFolder.lexically_relative(gameFolder)};
        const auto withoutCompany{platform::get_preferred_log_folder(appName)};
        CHECK(withoutCompany.lexically_relative(gameFolder.parent_path().parent_path() / appName) ==
              relativeToGameFolder);
    }

    SECTION("A company name alone, without the game's, still gives no folder") {
        CHECK(platform::get_preferred_log_folder({}, companyName).empty());
    }

#if defined(_WIN32)
    SECTION("On Windows the logs live in %LocalAppData%/<game>/logs") {
        const auto localAppData{get_local_app_data_folder()};
        REQUIRE(localAppData.has_value());

        CHECK(platform::get_preferred_log_folder(appName) == *localAppData / appName / "logs");
    }

    SECTION("On Windows a company adds its folder: %LocalAppData%/<company>/<game>/logs") {
        const auto localAppData{get_local_app_data_folder()};
        REQUIRE(localAppData.has_value());

        CHECK(platform::get_preferred_log_folder(appName, companyName) ==
              *localAppData / companyName / appName / "logs");
    }
#elif defined(__linux__)
    SECTION("On Linux the logs live in $XDG_STATE_HOME/<game>/logs") {
        const scoped_env_var xdgStateHome{"XDG_STATE_HOME", "/tmp/tca-engine-xdg-state"};

        CHECK(platform::get_preferred_log_folder(appName) ==
              std::filesystem::path{"/tmp/tca-engine-xdg-state"} / appName / "logs");
    }

    SECTION("On Linux a company adds its folder: $XDG_STATE_HOME/<company>/<game>/logs") {
        const scoped_env_var xdgStateHome{"XDG_STATE_HOME", "/tmp/tca-engine-xdg-state"};

        CHECK(platform::get_preferred_log_folder(appName, companyName) ==
              std::filesystem::path{"/tmp/tca-engine-xdg-state"} / companyName / appName / "logs");
    }

    SECTION("On Linux an unset XDG_STATE_HOME defaults to ~/.local/state/<game>/logs") {
        const scoped_env_var xdgStateHome{"XDG_STATE_HOME", std::nullopt};
        const auto home{read_env_var("HOME")};
        REQUIRE(home.has_value());

        CHECK(platform::get_preferred_log_folder(appName) ==
              std::filesystem::path{*home} / ".local" / "state" / appName / "logs");
    }

    SECTION("On Linux a relative XDG_STATE_HOME is ignored, as the XDG spec requires") {
        const scoped_env_var xdgStateHome{"XDG_STATE_HOME", "relative/state"};
        const auto home{read_env_var("HOME")};
        REQUIRE(home.has_value());

        CHECK(platform::get_preferred_log_folder(appName) ==
              std::filesystem::path{*home} / ".local" / "state" / appName / "logs");
    }

    SECTION("On Linux no HOME and no XDG_STATE_HOME gives no folder") {
        const scoped_env_var xdgStateHome{"XDG_STATE_HOME", std::nullopt};
        const scoped_env_var home{"HOME", std::nullopt};

        CHECK(platform::get_preferred_log_folder(appName).empty());
    }
#endif
}

namespace {

// Changes the working directory for the lifetime of the object, restoring the previous one
// afterwards so other tests keep finding their resources relative to bin/.
class scoped_working_directory final {
public:
    explicit scoped_working_directory(const std::filesystem::path& newWorkingDirectory)
        : m_previous{std::filesystem::current_path()} {
        std::filesystem::current_path(newWorkingDirectory);
    }

    ~scoped_working_directory() {
        std::error_code ignoredError{};
        std::filesystem::current_path(m_previous, ignoredError);
    }

    scoped_working_directory(const scoped_working_directory&) = delete;
    scoped_working_directory& operator=(const scoped_working_directory&) = delete;

private:
    std::filesystem::path m_previous;
};

bool holds_test_binary(const std::filesystem::path& folder) {
    std::error_code ignoredError{};
    for (const auto& entry : std::filesystem::directory_iterator{folder, ignoredError}) {
        if (entry.is_regular_file() && entry.path().stem() == "tca-engine_tests") {
            return true;
        }
    }

    return false;
}

} // namespace

TEST_CASE("platform executable directory tests",
          "[tca][engine][platform][folders][executable-directory]") {
    using namespace tca::engine;

    SECTION("The executable directory is an absolute, existing folder") {
        const auto executableDirectory{platform::get_executable_directory()};

        REQUIRE_FALSE(executableDirectory.empty());
        CHECK(executableDirectory.is_absolute());
        CHECK(std::filesystem::is_directory(executableDirectory));
    }

    SECTION("The executable directory holds the test binary and the resources copied next to it") {
        const auto executableDirectory{platform::get_executable_directory()};
        REQUIRE_FALSE(executableDirectory.empty());

        CHECK(holds_test_binary(executableDirectory));
        CHECK(std::filesystem::is_directory(executableDirectory / "res"));
    }

    SECTION("The executable directory does not follow the working directory") {
        const auto executableDirectory{platform::get_executable_directory()};
        REQUIRE_FALSE(executableDirectory.empty());

        const testing::scoped_temp_directory tempDirectory{"tca-engine-executable-directory"};
        const scoped_working_directory workingDirectory{tempDirectory.path()};

        CHECK(platform::get_executable_directory() == executableDirectory);
    }
}

TEST_CASE("platform asset path tests", "[tca][engine][platform][folders][asset-path]") {
    using namespace tca::engine;

    const auto executableDirectory{platform::get_executable_directory()};
    REQUIRE_FALSE(executableDirectory.empty());

    SECTION("A relative path is resolved inside the executable directory") {
        const std::filesystem::path relativePath{std::filesystem::path{"res"} / "test-sound.wav"};

        const auto assetPath{platform::get_asset_path(relativePath)};

        CHECK(assetPath == executableDirectory / relativePath);
        CHECK(assetPath.is_absolute());
        CHECK(std::filesystem::is_regular_file(assetPath));
    }

    SECTION("An asset that does not exist still gets its path") {
        const std::filesystem::path relativePath{std::filesystem::path{"res"} / "missing.png"};

        const auto assetPath{platform::get_asset_path(relativePath)};

        CHECK(assetPath == executableDirectory / relativePath);
        CHECK_FALSE(std::filesystem::exists(assetPath));
    }

    SECTION("An empty path gives the executable directory itself") {
        CHECK(platform::get_asset_path({}) == executableDirectory);
    }

    SECTION("An absolute path is returned unchanged") {
        const auto absolutePath{std::filesystem::temp_directory_path() / "tca-engine-asset.png"};

        CHECK(platform::get_asset_path(absolutePath) == absolutePath);
    }

    SECTION("Asset paths do not follow the working directory") {
        const std::filesystem::path relativePath{std::filesystem::path{"res"} / "test-sound.wav"};
        const auto assetPath{platform::get_asset_path(relativePath)};

        const testing::scoped_temp_directory tempDirectory{"tca-engine-asset-path"};
        const scoped_working_directory workingDirectory{tempDirectory.path()};

        CHECK(platform::get_asset_path(relativePath) == assetPath);
        CHECK(std::filesystem::is_regular_file(platform::get_asset_path(relativePath)));
    }
}
