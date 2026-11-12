// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/simple-console-application.hpp"

// C++ STL
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Engine
#include "input/input-frame.hpp"
#include "input/key.hpp"

// Test doubles
#include "test-doubles/application/application-logic.test-double.hpp"

TEST_CASE("simple_console_application class tests",
          "[tca][engine][application][simple-console-application]") {
    using namespace tca::engine::application;

    SECTION("No builder pattern configuration") {
        GIVEN("A simple_console_application instance") {
            simple_console_application app{};

            WHEN("run() is called without setting any application logic") {
                THEN("run() should throw a std::runtime_error") {
                    REQUIRE_THROWS_AS(app.run(), std::runtime_error);
                }
            }
        }
    }

    SECTION("Builder pattern configuration") {
        GIVEN("A simple_console_application instance") {
            simple_console_application app{};

            WHEN(
                "set_logic() is called with a valid application logic instance which returns "
                "false on tick()") {
                auto logic{std::make_unique<test_doubles::application_logic_fake>()};
                const auto* const logicPtr{logic.get()};

                app.set_logic(std::move(logic));
                REQUIRE_NOTHROW(app.run());

                THEN("tick() should be called exactly once") {
                    CHECK(logicPtr->get_tick_call_count() == 1);
                }

                THEN("on_close() should be called exactly once") {
                    CHECK(logicPtr->get_on_close_call_count() == 1);
                }

                THEN("set_app_controller() should be called exactly once") {
                    CHECK(logicPtr->get_set_app_controller_call_count() == 1);
                }
            }

            WHEN(
                "set_logic() is called with a valid application logic instance which returns true "
                "on tick()") {
                std::size_t tickCallCount{};
                auto logic{std::make_unique<test_doubles::application_logic_fake>(
                    [&tickCallCount] {
                        // Only return true for the first 3 calls, then return false to stop the
                        // application loop.
                        return ++tickCallCount < 3;
                    },
                    nullptr)};
                const auto* const logicPtr{logic.get()};

                app.set_logic(std::move(logic));
                REQUIRE_NOTHROW(app.run());

                THEN("tick() should be called exactly 3 times") {
                    CHECK(logicPtr->get_tick_call_count() == 3);
                }

                THEN("on_close() should be called exactly once") {
                    CHECK(logicPtr->get_on_close_call_count() == 1);
                }

                THEN("set_app_controller() should be called exactly once") {
                    CHECK(logicPtr->get_set_app_controller_call_count() == 1);
                }
            }

            WHEN(
                "the application logic calls the application controller's exit() method after it's "
                "run") {
                application_controller* applicationController{};
                std::size_t tickCallCount{};
                auto logic{std::make_unique<test_doubles::application_logic_fake>(
                    [&applicationController, &tickCallCount]() -> bool {
                        tickCallCount++;
                        if (tickCallCount == 2) {
                            applicationController->exit();
                        }

                        return true;
                    },
                    nullptr,
                    [&applicationController](application_controller& controller) {
                        applicationController = &controller;
                    })};
                const auto* const logicPtr{logic.get()};
                app.set_logic(std::move(logic));
                REQUIRE_NOTHROW(app.run());

                THEN("tick() should be called 2 times") {
                    CHECK(logicPtr->get_tick_call_count() == 2);
                }

                THEN("on_close() should be called exactly once") {
                    CHECK(logicPtr->get_on_close_call_count() == 1);
                }

                THEN("set_app_controller() should be called exactly once") {
                    CHECK(logicPtr->get_set_app_controller_call_count() == 1);
                }
            }
        }
    }

    SECTION("Window controls on an application without a window") {
        GIVEN("The controller a simple_console_application hands its logic") {
            simple_console_application app{};
            application_controller* controller{};
            app.set_logic(std::make_unique<test_doubles::application_logic_fake>(
                std::function<bool()>{}, std::function<void()>{},
                [&controller](application_controller& injected) {
                    controller = &injected;
                }));
            REQUIRE(controller != nullptr);

            THEN("It is not fullscreen") {
                REQUIRE_FALSE(controller->is_fullscreen());
            }

            WHEN("go_fullscreen() is requested") {
                controller->go_fullscreen();

                THEN("It still is not fullscreen, since there is no window to make fullscreen") {
                    REQUIRE_FALSE(controller->is_fullscreen());
                }

                AND_WHEN("go_windowed() is requested") {
                    controller->go_windowed();

                    THEN("It is not fullscreen") {
                        REQUIRE_FALSE(controller->is_fullscreen());
                    }
                }
            }

            WHEN("set_content_size() is requested") {
                THEN("It is ignored without failing") {
                    REQUIRE_NOTHROW(
                        controller->set_content_size({.width = 640, .height = 360}, {}));
                    REQUIRE_NOTHROW(controller->set_content_size(
                        {.width = 640, .height = 360},
                        {.placement = window_placement::center,
                         .fullscreen = fullscreen_resize::defer_until_windowed}));
                    REQUIRE_NOTHROW(controller->go_windowed());
                    REQUIRE_FALSE(controller->is_fullscreen());
                }
            }

            WHEN("center_window() is requested") {
                THEN("It is ignored without failing") {
                    REQUIRE_NOTHROW(controller->center_window());
                }
            }

            WHEN("The logic resizes the window while the application runs") {
                THEN("run() still completes normally") {
                    std::size_t tickCallCount{};
                    simple_console_application runningApp{};
                    application_controller* runningController{};
                    runningApp.set_logic(std::make_unique<test_doubles::application_logic_fake>(
                        [&runningController, &tickCallCount]() -> bool {
                            runningController->set_content_size({.width = 320, .height = 180}, {});
                            runningController->center_window();
                            return ++tickCallCount < 2;
                        },
                        std::function<void()>{},
                        [&runningController](application_controller& injected) {
                            runningController = &injected;
                        }));

                    REQUIRE_NOTHROW(runningApp.run());
                    REQUIRE(tickCallCount == 2);
                }
            }
        }
    }
}

TEST_CASE("simple_console_application input",
          "[tca][engine][application][simple-console-application][input]") {
    using namespace tca::engine;
    using namespace tca::engine::application;

    simple_console_application app{};

    std::optional<input::input_view> injectedInput{};
    bool wasInputSetBeforeFirstTick{};
    std::size_t tickCallCount{};
    bool everyTickSawAnEmptyFrame{true};

    auto logic{std::make_unique<test_doubles::application_logic_fake>(
        [&]() -> bool {
            if (++tickCallCount == 1) {
                wasInputSetBeforeFirstTick = injectedInput.has_value();
            }

            if (injectedInput) {
                everyTickSawAnEmptyFrame =
                    everyTickSawAnEmptyFrame &&
                    !injectedInput->keyboard().is_down(input::key::space) &&
                    !injectedInput->keyboard().was_pressed(input::key::space) &&
                    injectedInput->text().empty();
            }

            return tickCallCount < 3;
        },
        std::function<void()>{}, std::function<void(application_controller&)>{},
        std::function<void(rendering::image_factory&)>{},
        [&injectedInput](const input::input_view input) {
            injectedInput = input;
        })};
    const auto* const logicPtr{logic.get()};

    app.set_logic(std::move(logic));
    REQUIRE_NOTHROW(app.run());

    THEN("The input is set exactly once") {
        REQUIRE(logicPtr->get_set_input_call_count() == 1);
        REQUIRE(injectedInput.has_value());
    }

    THEN("It is set before the first tick()") {
        REQUIRE(wasInputSetBeforeFirstTick);
    }

    THEN("Every tick sees an empty frame") {
        REQUIRE(tickCallCount == 3);
        REQUIRE(everyTickSawAnEmptyFrame);
    }
}
