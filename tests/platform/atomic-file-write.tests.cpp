// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/atomic-file-write.hpp"

// Engine testing support
#include "testing/scoped-temp-directory.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

// C++ STL
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace tca::engine::platform;

namespace {

void write_file(const std::filesystem::path& filePath, const std::string_view contents) {
    std::ofstream outputStream{filePath};
    outputStream << contents;
}

[[nodiscard]] std::string file_contents(const std::filesystem::path& filePath) {
    std::ifstream inputStream{filePath};

    return std::string{std::istreambuf_iterator<char>{inputStream},
                       std::istreambuf_iterator<char>{}};
}

// The temporary the write goes through must not be left behind: a settings folder filling up
// with them would be a bug of its own.
[[nodiscard]] bool holds_only(const std::filesystem::path& directoryPath,
                              const std::filesystem::path& expectedFilePath) {
    for (const auto& entry : std::filesystem::directory_iterator{directoryPath}) {
        if (entry.path() != expectedFilePath) {
            return false;
        }
    }

    return true;
}

} // namespace

TEST_CASE("write_file_atomically tests", "[tca][engine][platform][atomic_file_write]") {
    const tca::engine::testing::scoped_temp_directory tempDirectory{"tca-engine-atomic-file-write"};
    const auto filePath{tempDirectory.path() / "user.data"};

    SECTION("A file that is not there yet is created with what the writer put on the stream") {
        REQUIRE(write_file_atomically(filePath, [](std::ostream& outputStream) {
            outputStream << "first contents";

            return true;
        }));

        REQUIRE(file_contents(filePath) == "first contents");
        REQUIRE(holds_only(tempDirectory.path(), filePath));
    }

    SECTION("The folders leading to the file are created") {
        const auto nestedFilePath{tempDirectory.path() / "profiles" / "player-1" / "user.data"};

        REQUIRE(write_file_atomically(nestedFilePath, [](std::ostream& outputStream) {
            outputStream << "nested";

            return true;
        }));

        REQUIRE(file_contents(nestedFilePath) == "nested");
    }

    SECTION("A second write replaces the contents of the first") {
        REQUIRE(write_file_atomically(filePath, [](std::ostream& outputStream) {
            outputStream << "a much longer first version";

            return true;
        }));

        REQUIRE(write_file_atomically(filePath, [](std::ostream& outputStream) {
            outputStream << "second";

            return true;
        }));

        REQUIRE(file_contents(filePath) == "second");
    }

    SECTION("A writer that gives up part-way through leaves the previous contents intact") {
        write_file(filePath, "the previously saved contents");

        REQUIRE_FALSE(write_file_atomically(filePath, [](std::ostream& outputStream) {
            outputStream << "half of the new c";

            return false;
        }));

        REQUIRE(file_contents(filePath) == "the previously saved contents");
        REQUIRE(holds_only(tempDirectory.path(), filePath));
    }

    SECTION("A writer that throws part-way through leaves the previous contents intact") {
        write_file(filePath, "the previously saved contents");

        REQUIRE_FALSE(write_file_atomically(filePath, [](std::ostream& outputStream) -> bool {
            outputStream << "half of the new c";

            throw std::runtime_error{"interrupted"};
        }));

        REQUIRE(file_contents(filePath) == "the previously saved contents");
        REQUIRE(holds_only(tempDirectory.path(), filePath));
    }

    SECTION("A failed write does not create the file it was going to replace") {
        REQUIRE_FALSE(write_file_atomically(filePath, [](std::ostream& /*outputStream*/) {
            return false;
        }));

        REQUIRE_FALSE(std::filesystem::exists(filePath));
    }

    SECTION("Writing over something that is not a file reports failure instead of throwing") {
        const auto directoryPath{tempDirectory.path() / "not-a-file"};
        std::filesystem::create_directories(directoryPath);

        REQUIRE_FALSE(write_file_atomically(directoryPath, [](std::ostream& outputStream) {
            outputStream << "contents";

            return true;
        }));

        REQUIRE(std::filesystem::is_directory(directoryPath));
    }

    SECTION("An empty path and an empty writer are refused rather than acted on") {
        REQUIRE_FALSE(write_file_atomically({}, [](std::ostream& /*outputStream*/) {
            return true;
        }));

        REQUIRE_FALSE(write_file_atomically(filePath, file_contents_writer{}));
        REQUIRE_FALSE(std::filesystem::exists(filePath));
    }
}
