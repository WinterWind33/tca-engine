// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/imgui-dx12-application.hpp"

// C++ STL
#include <stdexcept>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Engine testing support
#include "testing/win32/windowing-utilities.hpp"

// Test utilities
#include "utilities/win32/wm-quit-drain.hpp"

// Test doubles
#include "test-doubles/application/application-logic.test-double.hpp"

namespace {

// Simulates initialize() failing before the ImGui Win32/DX12 backends are ever set up (e.g. a
// failure while creating the DirectX 12 device), so that close() must be able to run safely
// afterwards without asserting/crashing on backends that were never initialized.
class imgui_dx12_application_failing_init_tss final
    : public tca::engine::application::win32::imgui_dx12_application {
protected:
    void initialize() override {
        throw std::runtime_error{"Simulated partial initialization failure"};
    }
};

} // namespace

TEST_CASE("imgui_dx12_application functional tests",
          "[functional][tca][engine][application][win32][imgui_dx12_application]") {
    using namespace tca::engine::application;

    tests::utilities::drain_wm_quit_messages();

    SECTION("Construction") {
        GIVEN("An imgui_dx12_application") {
            WHEN("Constructed") {
                THEN("It should not throw an exception") {
                    REQUIRE_NOTHROW(win32::imgui_dx12_application{});
                }
            }
        }
    }

    SECTION("Running an imgui_dx12_application") {
        SECTION("Without setting the application logic") {
            GIVEN("An imgui_dx12_application") {
                win32::imgui_dx12_application app{};

                THEN("Running the application should throw an exception") {
                    REQUIRE_THROWS_AS(app.run(), std::runtime_error);
                }
            }
        }

        SECTION("initialize() fails before the ImGui backends are set up") {
            GIVEN("An imgui_dx12_application whose initialize() throws before setting up ImGui") {
                imgui_dx12_application_failing_init_tss app{};
                app.set_logic(std::make_unique<test_doubles::application_logic_fake>());

                THEN("run() should rethrow the exception without crashing while cleaning up") {
                    REQUIRE_THROWS_AS(app.run(), std::runtime_error);
                }
            }
        }

        SECTION("With application logic setting") {
            GIVEN("An imgui_dx12_application fully built") {
                std::size_t tickCallCount{};
                tca::engine::testing::application_tss<win32::imgui_dx12_application> app{};
                bool isCloseCalled{};
                app.set_logic(std::make_unique<test_doubles::application_logic_fake>(
                    [&app, &tickCallCount]() -> bool {
                        if (tickCallCount >= 20) {
                            tca::engine::testing::simulate_user_closing_application(
                                app.get_window_handle());
                        }

                        ++tickCallCount;
                        return true;
                    },
                    [&isCloseCalled]() {
                        isCloseCalled = true;
                    }));
                app.set_app_name("Test ImGui Dx12 Application");

                WHEN("Running the application") {
                    REQUIRE_NOTHROW(app.run());

                    AND_WHEN("Simulating the user closing the application") {
                        THEN("The application should close properly") {
                            REQUIRE(tickCallCount >= 20);
                            REQUIRE(isCloseCalled);
                        }
                    }
                }
            }
        }
    }
}
