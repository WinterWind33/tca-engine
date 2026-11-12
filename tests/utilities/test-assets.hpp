// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <filesystem>

namespace tca::tests::utilities {

/**
 * @brief Path (relative to the test executable's working directory) to the folder containing
 * binary assets used by the engine tests (e.g. sound files, images). This folder is copied next
 * to the test executable at build time, see tests/CMakeLists.txt.
 */
inline constexpr const char* TEST_ENGINE_ASSETS_REL_DIR = "res";

/**
 * @brief Path to the shared 4x2 test pattern fixture image (top row opaque red, bottom row
 * opaque blue), used by every test that needs a real, decodable image with known pixel content:
 * tests/platform/image-decoder.tests.cpp, tests/rendering/image-factory.tests.cpp
 * and tests/rendering/image-color-sampling.tests.cpp.
 *
 * @return std::filesystem::path The path to the test pattern image, relative to the test
 * executable's working directory.
 */
inline std::filesystem::path get_test_pattern_path() {
    return std::filesystem::path{TEST_ENGINE_ASSETS_REL_DIR} / "images" / "test-pattern.png";
}

} // namespace tca::tests::utilities
