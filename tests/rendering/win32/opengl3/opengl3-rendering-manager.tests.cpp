// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/opengl3/opengl3-rendering-manager.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

// Engine
#include "platform/win32/win32-application-window.hpp"

TEST_CASE("opengl3_rendering_manager tests",
          "[functional][tca][engine][rendering][win32][opengl3][opengl3_rendering_manager]") {
    using namespace tca::engine;

    GIVEN("A Win32 application window") {
        auto window = platform::win32::win32_application_window::two_phase_window_creation<
            platform::win32::win32_application_window>(TEXT("OpenGL 3 Test Window"));

        WHEN("Creating an OpenGL 3 rendering manager with the window") {
            THEN("It should not throw") {
                REQUIRE_NOTHROW(rendering::win32::opengl3::opengl3_rendering_manager{*window});
            }
        }
    }
}
