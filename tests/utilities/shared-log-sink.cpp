// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
//
// Creates the shared log sink before any test runs, with no log folder named. Every application a
// test runs names its log folder when run() starts, which would otherwise leave a folder in the
// real per-user folder for each app name the suite uses, and make the tests that expect no folder
// depend on the order they run in. tests/application/application-log-folder.tests.cpp, which does
// want a folder, is built into a binary of its own for that reason.
#include "core/logging/logging.hpp"

namespace {
const auto SHARED_LOG_SINK{tca::engine::core::logging::get_log_file_sink()};
} // namespace
