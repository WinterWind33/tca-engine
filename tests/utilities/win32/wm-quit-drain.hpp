// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Third-party
#include <catch2/catch_test_macros.hpp>

// Engine
#include "testing/win32/windowing-utilities.hpp"

namespace tests::utilities {

/**
 * @brief Drains any lingering WM_QUIT messages from this thread's message queue, failing the
 * current test case if the drain hits its safety limit instead of the queue running dry.
 *
 * @note The assertion lives here rather than in the engine's testing support module, which is
 * deliberately free of any test framework dependency: reporting a failure is the test suite's job,
 * so this is the Catch2-flavoured wrapper the engine's own suite uses.
 */
inline void drain_wm_quit_messages() {
    CHECK(tca::engine::testing::remove_wm_quit_messages_from_queue() <
          tca::engine::testing::WM_QUIT_DRAIN_LIMIT);
}

} // namespace tests::utilities
