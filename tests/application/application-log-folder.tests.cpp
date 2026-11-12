// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
//
// Built into its own test executable (see tests/CMakeLists.txt): the shared log sink is created
// once per process, and in the main test binary something always creates it before this test
// could name a folder. Keep this the only test case here for the same reason.
#include "application/simple-console-application.hpp"

// C++ STL
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>

// Third-party
#include <spdlog/spdlog.h>
#include <catch2/catch_test_macros.hpp>

// Engine
#include "application/application-logger.hpp"
#include "core/logging/logging.hpp"
#include "platform/folders.hpp"

// Test doubles
#include "test-doubles/application/application-logic.test-double.hpp"

namespace {

constexpr std::string_view COMPANY_NAME{"tca-engine-tests"};

// A name no earlier run of the suite can have used, so a stale log can't satisfy the test.
const std::string APP_NAME{
    "log-folder-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())};

// Removes the log folder the test makes in the real per-user folder (it can't be redirected to a
// temp folder on Windows). Done at static destruction rather than at the end of the test because
// the shared sink keeps engine.log open until then, and Windows can't delete an open file: this
// object is constructed before the shared sink, so it is destroyed after it. Best effort.
struct log_folder_cleanup {
    // spdlog's registry is a function-local static, otherwise first built by the test, after this
    // object: it would then be destroyed before it, and the drop_all() below would clear a map
    // that no longer exists. Building it here makes it outlive this object.
    log_folder_cleanup() {
        std::ignore = spdlog::details::registry::instance();
    }
    log_folder_cleanup(const log_folder_cleanup&) = delete;
    log_folder_cleanup& operator=(const log_folder_cleanup&) = delete;

    ~log_folder_cleanup() {
        const auto appFolder{
            tca::engine::platform::get_preferred_log_folder(APP_NAME, COMPANY_NAME).parent_path()};
        if (appFolder.empty()) {
            return;
        }

        // Engine loggers are registered with spdlog, whose registry can outlive this object (it
        // lives in spdlog's DLL on Windows) and would keep engine.log open: let go of them first.
        spdlog::drop_all();

        std::error_code ignoredError{};
        std::filesystem::remove_all(appFolder, ignoredError);

        // remove() leaves the company folder alone if another run's folder is still in it.
        std::filesystem::remove(appFolder.parent_path(), ignoredError);
    }
};

const log_folder_cleanup LOG_FOLDER_CLEANUP{};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

} // namespace

TEST_CASE("The application writes its log into its own per-user log folder",
          "[tca][engine][application][log-folder]") {
    using namespace tca::engine;

    const auto expectedLogFolder{platform::get_preferred_log_folder(APP_NAME, COMPANY_NAME)};
    REQUIRE_FALSE(expectedLogFolder.empty());

    const std::string marker{"logged-from-tick-" + APP_NAME};
    const std::string gameMarker{"logged-by-the-game-" + APP_NAME};

    // A game takes its logger wherever it likes, typically before run(). Creating it must not
    // settle the log folder, or the run would write no file at all.
    const auto gameLogger{core::logging::get_logger("log-folder-tests.game")};

    application::simple_console_application app{};

    // The company is named after the app on purpose: both are read when run() starts, so the order
    // of the builder calls must not matter.
    app.set_app_name(APP_NAME).set_company_name(COMPANY_NAME);
    app.set_logic(std::make_unique<application::test_doubles::application_logic_fake>(
        [&marker, &gameMarker, &gameLogger] {
            application::get_application_logger()->info(marker);
            gameLogger->info(gameMarker);
            return false;
        },
        nullptr));

    REQUIRE_NOTHROW(app.run());
    core::logging::get_log_file_sink()->flush();

    CHECK(core::logging::get_log_folder() == expectedLogFolder);

    const auto log{read_file(expectedLogFolder / std::string{core::logging::LOG_FILE_NAME})};
    CHECK(log.find(marker) != std::string::npos);
    CHECK(log.find(gameMarker) != std::string::npos);
}
