// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/directx-12/directx12-rendering-manager.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

#include "platform/win32/win32-application-window.hpp"

TEST_CASE("directx12_rendering_manager functional tests",
          "[functional][tca][engine][rendering][win32][directx-12][directx12-rendering-manager]") {
    using namespace tca::engine;
    using namespace rendering::win32::directx12;

    GIVEN("A Win32 application window") {
        auto window = platform::win32::win32_application_window::two_phase_window_creation<
            platform::win32::win32_application_window>(TEXT("DirectX 12 Test Window"));

        AND_GIVEN("A valid rendering configuration") {
            constexpr rendering_config config{
                .frames_in_flight_count = 2,
                .back_buffers_count = 2,
                .srv_heap_size = 64,
            };

            WHEN("Creating a DirectX 12 rendering manager") {
                THEN("It should not throw") {
                    REQUIRE_NOTHROW(directx12_rendering_manager{window.get(), config});
                }
            }
        }
    }
}
