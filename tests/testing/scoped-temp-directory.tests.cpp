// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "testing/scoped-temp-directory.hpp"

// C++ STL
#include <filesystem>
#include <fstream>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Like the headless ImGui context, this helper is part of TCAEngine::testing's public surface, so
// its own guarantees are pinned down here rather than only being leaned on by the settings and
// atomic-file-write suites.
TEST_CASE("scoped_temp_directory class tests", "[tca][engine][testing][scoped_temp_directory]") {
    constexpr const char* SUB_FOLDER_NAME{"tca-engine-scoped-temp-directory-tests"};

    SECTION("Lifetime") {
        GIVEN("A scoped temporary directory") {
            std::filesystem::path createdPath{};

            {
                const tca::engine::testing::scoped_temp_directory tempDirectory{SUB_FOLDER_NAME};
                createdPath = tempDirectory.path();

                THEN("The directory exists while the object is alive") {
                    CHECK(std::filesystem::is_directory(createdPath));
                }

                THEN("It lives under a subfolder named after the suite") {
                    CHECK(createdPath.parent_path().filename() == SUB_FOLDER_NAME);
                }

                THEN("It lives under the system temporary directory") {
                    CHECK(createdPath.string().starts_with(
                        std::filesystem::temp_directory_path().string()));
                }
            }

            THEN("It is removed once the object goes out of scope") {
                CHECK_FALSE(std::filesystem::exists(createdPath));
            }
        }

        GIVEN("A scoped temporary directory holding files") {
            std::filesystem::path createdPath{};

            {
                const tca::engine::testing::scoped_temp_directory tempDirectory{SUB_FOLDER_NAME};
                createdPath = tempDirectory.path();

                std::filesystem::create_directories(createdPath / "nested");
                std::ofstream{createdPath / "nested" / "file.txt"} << "content";

                REQUIRE(std::filesystem::exists(createdPath / "nested" / "file.txt"));
            }

            THEN("Its whole contents go with it") {
                CHECK_FALSE(std::filesystem::exists(createdPath));
            }
        }
    }

    SECTION("Uniqueness") {
        GIVEN("Two scoped temporary directories created from the same subfolder name") {
            const tca::engine::testing::scoped_temp_directory first{SUB_FOLDER_NAME};
            const tca::engine::testing::scoped_temp_directory second{SUB_FOLDER_NAME};

            THEN("They do not collide") {
                CHECK(first.path() != second.path());
                CHECK(std::filesystem::is_directory(first.path()));
                CHECK(std::filesystem::is_directory(second.path()));
            }
        }
    }
}
