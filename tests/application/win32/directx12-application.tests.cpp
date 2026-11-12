// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/directx12-application.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

// Test doubles
#include "test-doubles/application/application-logic.test-double.hpp"

TEST_CASE("directx12_application functional tests",
          "[functional][tca][engine][application][win32][directx12-application]") {
    using namespace tca::engine::application;

    SECTION("No window") {
        GIVEN("A DirectX 12 application with no window") {
            win32::directx12_application app{};
            app.set_logic(std::make_unique<test_doubles::application_logic_fake>());

            WHEN("The application is run") {
                THEN(
                    "The application should throw a runtime error indicating that the application "
                    "window is not set") {
                    REQUIRE_THROWS_AS(app.run(), std::runtime_error);
                }
            }
        }
    }
}
