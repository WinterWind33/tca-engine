// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/logging/logging.hpp"

// C++ STL
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>

// Third-party
#include <spdlog/logger.h>
#include <catch2/catch_test_macros.hpp>

#include "testing/scoped-temp-directory.hpp"

namespace {

constexpr const char* TEMP_SUB_FOLDER_NAME{"tca-engine-logging-tests"};

std::filesystem::path log_file(const std::filesystem::path& folder, const std::size_t index = 0) {
    if (index == 0) {
        return folder / std::string{tca::engine::core::logging::LOG_FILE_NAME};
    }

    return folder / ("engine." + std::to_string(index) + ".log");
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    stream << content;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

// Whether the file is named like one of the engine's logs: engine.log or engine.<n>.log. Other .log
// files aren't the engine's: CI, for one, redirects the test binary's output (the fallback sink's
// stderr included) into a .log file in the working directory.
bool is_engine_log_file(const std::filesystem::path& path) {
    const std::filesystem::path logFileName{tca::engine::core::logging::LOG_FILE_NAME};
    const auto fileName{path.filename().string()};
    return fileName.starts_with(logFileName.stem().string() + '.') &&
           path.extension() == logFileName.extension();
}

// Whether any engine log file under the folder, at any depth, contains the text.
bool any_log_file_contains(const std::filesystem::path& folder, const std::string& text) {
    std::error_code ignoredError{};
    for (std::filesystem::recursive_directory_iterator it{folder, ignoredError}, end{}; it != end;
         it.increment(ignoredError)) {
        if (it->is_regular_file(ignoredError) && is_engine_log_file(it->path()) &&
            read_file(it->path()).find(text) != std::string::npos) {
            return true;
        }
    }

    return false;
}

// A marker no earlier run of the suite can have written, so a stale log can't contain it.
std::string unique_marker(const std::string& name) {
    return name + '-' + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

// Logs one line through the sink and flushes it, then drops the logger so the file is no longer
// held by anything but the sink itself.
void log_through(const std::shared_ptr<spdlog::sinks::sink>& sink, const std::string& message) {
    spdlog::logger logger{"logging-tests", sink};
    logger.info(message);
    logger.flush();
}

} // namespace

TEST_CASE("make_log_sink() function tests", "[tca][engine][core][logging][make-log-sink]") {
    using namespace tca::engine::core;

    // Declared first so it outlives every sink below: Windows cannot delete a file still open.
    const tca::engine::testing::scoped_temp_directory tempDirectory{TEMP_SUB_FOLDER_NAME};

    SECTION("A folder that doesn't exist yet is created, along with its parents") {
        const auto logFolder{tempDirectory.path() / "Company" / "Game" / "logs"};

        std::shared_ptr<spdlog::sinks::sink> sink{};
        REQUIRE_NOTHROW(sink = logging::make_log_sink(logFolder));
        REQUIRE(sink != nullptr);

        CHECK(std::filesystem::is_directory(logFolder));
        CHECK(std::filesystem::exists(log_file(logFolder)));
        CHECK_FALSE(std::filesystem::exists(log_file(logFolder, 1)));
    }

    SECTION("What is logged through the sink ends up in engine.log in that folder") {
        const auto logFolder{tempDirectory.path() / "logs"};
        const auto sink{logging::make_log_sink(logFolder)};
        REQUIRE(sink != nullptr);

        log_through(sink, "hello from the log test");

        CHECK(read_file(log_file(logFolder)).find("hello from the log test") != std::string::npos);
    }

    SECTION("The previous run's log is kept as engine.1.log and engine.log starts empty") {
        const auto logFolder{tempDirectory.path() / "logs"};
        std::filesystem::create_directories(logFolder);
        write_file(log_file(logFolder), "previous run");

        const auto sink{logging::make_log_sink(logFolder)};
        REQUIRE(sink != nullptr);

        CHECK(read_file(log_file(logFolder, 1)) == "previous run");
        CHECK(read_file(log_file(logFolder)).find("previous run") == std::string::npos);
    }

    SECTION("Older logs shift by one and the oldest beyond the kept count is deleted") {
        constexpr std::size_t KEPT_PREVIOUS_LOGS{3};
        const auto logFolder{tempDirectory.path() / "logs"};
        std::filesystem::create_directories(logFolder);

        // engine.log is run 0 (the latest), engine.3.log is run 3 (the oldest kept).
        for (std::size_t index{0}; index <= KEPT_PREVIOUS_LOGS; ++index) {
            write_file(log_file(logFolder, index), "run " + std::to_string(index));
        }

        const auto sink{logging::make_log_sink(logFolder, KEPT_PREVIOUS_LOGS)};
        REQUIRE(sink != nullptr);

        CHECK(read_file(log_file(logFolder, 1)) == "run 0");
        CHECK(read_file(log_file(logFolder, 2)) == "run 1");
        CHECK(read_file(log_file(logFolder, 3)) == "run 2");
        CHECK_FALSE(std::filesystem::exists(log_file(logFolder, KEPT_PREVIOUS_LOGS + 1)));
    }

    SECTION("A gap in the history doesn't stop the rotation") {
        const auto logFolder{tempDirectory.path() / "logs"};
        std::filesystem::create_directories(logFolder);
        write_file(log_file(logFolder), "run 0");
        write_file(log_file(logFolder, 2), "run 2");

        const auto sink{logging::make_log_sink(logFolder, 3)};
        REQUIRE(sink != nullptr);

        CHECK(read_file(log_file(logFolder, 1)) == "run 0");
        CHECK(read_file(log_file(logFolder, 3)) == "run 2");
    }

    SECTION("Keeping zero previous logs truncates engine.log and keeps no history") {
        const auto logFolder{tempDirectory.path() / "logs"};
        std::filesystem::create_directories(logFolder);
        write_file(log_file(logFolder), "previous run");

        const auto sink{logging::make_log_sink(logFolder, 0)};
        REQUIRE(sink != nullptr);

        CHECK(std::filesystem::exists(log_file(logFolder)));
        CHECK(read_file(log_file(logFolder)).find("previous run") == std::string::npos);
        CHECK_FALSE(std::filesystem::exists(log_file(logFolder, 1)));
    }

    SECTION("Files that aren't engine logs are left alone") {
        const auto logFolder{tempDirectory.path() / "logs"};
        std::filesystem::create_directories(logFolder);
        write_file(logFolder / "crash.dmp", "dump");
        write_file(logFolder / "game.log", "the game's own log");

        const auto sink{logging::make_log_sink(logFolder)};
        REQUIRE(sink != nullptr);

        CHECK(read_file(logFolder / "crash.dmp") == "dump");
        CHECK(read_file(logFolder / "game.log") == "the game's own log");
    }

    SECTION("A folder that can't be created falls back to a sink that writes no file") {
        // A regular file where the folder should be: create_directories() cannot succeed.
        const auto blockingFile{tempDirectory.path() / "not-a-folder"};
        write_file(blockingFile, "in the way");
        const auto logFolder{blockingFile / "logs"};

        std::shared_ptr<spdlog::sinks::sink> sink{};
        REQUIRE_NOTHROW(sink = logging::make_log_sink(logFolder));
        REQUIRE(sink != nullptr);
        REQUIRE_NOTHROW(log_through(sink, "this goes to the debugger or stderr"));

        CHECK(read_file(blockingFile) == "in the way");
        CHECK(std::filesystem::is_regular_file(blockingFile));
    }

    SECTION("An empty folder falls back to a sink that writes no file") {
        const auto MARKER{unique_marker("empty-folder-fallback")};

        std::shared_ptr<spdlog::sinks::sink> sink{};
        REQUIRE_NOTHROW(sink = logging::make_log_sink({}));
        REQUIRE(sink != nullptr);
        REQUIRE_NOTHROW(log_through(sink, MARKER));

        CHECK_FALSE(any_log_file_contains(std::filesystem::current_path(), MARKER));
    }
}

TEST_CASE("get_log_file_sink() function tests", "[tca][engine][core][logging][get-log-file-sink]") {
    using namespace tca::engine::core;

    WHEN("Calling get_log_file_sink() for the first time") {
        std::shared_ptr<spdlog::sinks::sink> first_call_sink{};
        REQUIRE_NOTHROW(first_call_sink = logging::get_log_file_sink());

        THEN("It should return a valid shared pointer") {
            REQUIRE(first_call_sink != nullptr);
        }

        // The test binary never names a log folder before the shared sink is created, which is
        // exactly the case of a game that forgot to: nothing may land in the working directory.
        THEN("It should not write any file relative to the working directory") {
            const auto MARKER{unique_marker("shared-sink-working-directory")};
            REQUIRE_NOTHROW(log_through(first_call_sink, MARKER));

            CHECK_FALSE(any_log_file_contains(std::filesystem::current_path(), MARKER));
        }

        AND_WHEN("It's called a second time") {
            std::shared_ptr<spdlog::sinks::sink> second_call_sink{};
            REQUIRE_NOTHROW(second_call_sink = logging::get_log_file_sink());

            THEN("It should return the same instance as the first call") {
                REQUIRE(second_call_sink == first_call_sink);
            }
        }

        AND_WHEN("A log folder is named after the shared sink already exists") {
            const tca::engine::testing::scoped_temp_directory tempDirectory{TEMP_SUB_FOLDER_NAME};
            const auto logFolder{tempDirectory.path() / "too-late"};

            THEN("It is refused and the shared sink stays the same") {
                CHECK_FALSE(logging::set_log_folder(logFolder));
                CHECK(logging::get_log_file_sink() == first_call_sink);
                CHECK_FALSE(std::filesystem::exists(logFolder));
                CHECK(logging::get_log_folder() != logFolder);
            }
        }

        // As above, the test binary names no folder, so the shared sink writes no file. The case of
        // a named folder needs a fresh process, where nothing has created the shared sink yet.
        THEN("No log folder is reported, rather than one relative to the working directory") {
            CHECK(logging::get_log_folder().empty());
        }
    }
}
