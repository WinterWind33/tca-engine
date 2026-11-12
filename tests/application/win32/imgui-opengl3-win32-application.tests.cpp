// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/imgui-opengl3-win32-application.hpp"

// C++ STL
#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Third-party
#include <imgui.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

// Engine
#include "input/input-frame.hpp"
#include "input/key.hpp"

// Engine testing support
#include "testing/win32/windowing-utilities.hpp"

// Test utilities
#include "utilities/win32/keyboard-messages.hpp"
#include "utilities/win32/wm-quit-drain.hpp"

// Test doubles
#include "test-doubles/application/application-logic.test-double.hpp"

namespace {

// Simulates initialize() failing before the ImGui Win32/OpenGL3 backends are ever set up (e.g. a
// failure while creating the OpenGL context), so that close() must be able to run safely
// afterwards without asserting/crashing on backends that were never initialized.
class imgui_opengl3_win32_application_failing_init_tss final
    : public tca::engine::application::win32::imgui_opengl3_win32_application {
protected:
    void initialize() override {
        throw std::runtime_error{"Simulated partial initialization failure"};
    }
};

} // namespace

TEST_CASE("imgui_opengl3_win32_application functional tests",
          "[functional][tca][engine][application][win32][imgui_opengl3_win32_application]") {
    using namespace tca::engine::application;

    tests::utilities::drain_wm_quit_messages();

    SECTION("Construction") {
        GIVEN("An imgui_opengl3_win32_application") {
            WHEN("Constructed") {
                THEN("It should not throw an exception") {
                    REQUIRE_NOTHROW(win32::imgui_opengl3_win32_application{});
                }
            }
        }
    }

    SECTION("Running an imgui_opengl3_win32_application") {
        SECTION("Without setting the application logic") {
            GIVEN("An imgui_opengl3_win32_application") {
                win32::imgui_opengl3_win32_application app{};

                THEN("Running the application should throw an exception") {
                    REQUIRE_THROWS_AS(app.run(), std::runtime_error);
                }
            }
        }

        SECTION("initialize() fails before the ImGui backends are set up") {
            GIVEN("An imgui_opengl3_win32_application whose initialize() throws before setting "
                  "up ImGui") {
                imgui_opengl3_win32_application_failing_init_tss app{};
                app.set_logic(std::make_unique<test_doubles::application_logic_fake>());

                THEN("run() should rethrow the exception without crashing while cleaning up") {
                    REQUIRE_THROWS_AS(app.run(), std::runtime_error);
                }
            }
        }

        SECTION("With application logic setting") {
            GIVEN("An imgui_opengl3_win32_application fully built") {
                std::size_t tickCallCount{};
                tca::engine::testing::application_tss<win32::imgui_opengl3_win32_application> app{};
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
                app.set_app_name("Test ImGui OpenGL3 Win32 Application");

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

namespace {

// What one tick() saw: whether ImGui wanted the keyboard, and what the logic got of it.
struct captured_tick {
    bool imguiWantsKeyboard{};
    bool spacePressed{};
    bool spaceDown{};
    std::string text{};
};

} // namespace

TEST_CASE("imgui_opengl3_win32_application keyboard capture",
          "[functional][tca][engine][application][win32][imgui_opengl3_win32_application][input]") {
    using namespace tca::engine;
    using namespace tca::engine::application;

    tests::utilities::drain_wm_quit_messages();

    // A text field focused every frame keeps ImGui wanting the keyboard; without it, ImGui wants
    // nothing and the logic gets everything.
    const bool focusTextField = GENERATE(true, false);
    CAPTURE(focusTextField);

    testing::application_tss<win32::imgui_opengl3_win32_application> app{};
    app.set_app_name("Test ImGui keyboard capture");

    std::optional<input::input_view> injectedInput{};
    std::vector<captured_tick> ticks{};
    std::array<char, 64> textBuffer{};
    bool isCloseSent{};

    // Tick 1 focuses the field, so ImGui wants the keyboard from tick 2 on. Tick 2 posts a key
    // and a character, pumped before tick 3.
    constexpr std::size_t POST_TICK{2};
    constexpr std::size_t CHECK_TICK{3};

    app.set_logic(std::make_unique<test_doubles::application_logic_fake>(
        [&]() -> bool {
            captured_tick tick{.imguiWantsKeyboard = ImGui::GetIO().WantCaptureKeyboard};
            if (injectedInput) {
                const auto keyboard{injectedInput->keyboard()};
                tick.spacePressed = keyboard.was_pressed(input::key::space);
                tick.spaceDown = keyboard.is_down(input::key::space);
                tick.text = std::string{injectedInput->text()};
            }
            ticks.push_back(tick);

            ImGui::Begin("Keyboard capture");
            if (focusTextField) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputText("##field", textBuffer.data(), textBuffer.size());
            ImGui::End();

            const HWND hwnd{app.get_window_handle().get()};
            if (ticks.size() == POST_TICK) {
                ::PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, tests::utilities::key_down_lparam(0x39));
                ::PostMessage(hwnd, WM_CHAR, L'a', 1);
            }

            if (ticks.size() >= CHECK_TICK + 1 && !isCloseSent) {
                testing::simulate_user_closing_application(app.get_window_handle());
                isCloseSent = true;
            }
            return true;
        },
        std::function<void()>{}, std::function<void(application_controller&)>{},
        std::function<void(rendering::image_factory&)>{},
        [&injectedInput](const input::input_view input) {
            injectedInput = input;
        }));

    REQUIRE_NOTHROW(app.run());
    REQUIRE(ticks.size() >= CHECK_TICK);
    const captured_tick& checked{ticks[CHECK_TICK - 1]};

    if (focusTextField) {
        // Precondition: otherwise the test proves nothing.
        REQUIRE(checked.imguiWantsKeyboard);

        THEN("The logic gets neither the key nor the text ImGui took") {
            REQUIRE_FALSE(checked.spacePressed);
            REQUIRE_FALSE(checked.spaceDown);
            REQUIRE(checked.text.empty());
        }

        THEN("The held key stays hidden in the ticks after (consumed while held)") {
            for (std::size_t i{CHECK_TICK}; i < ticks.size(); ++i) {
                CAPTURE(i);
                REQUIRE_FALSE(ticks[i].spaceDown);
            }
        }
    } else {
        REQUIRE_FALSE(checked.imguiWantsKeyboard);

        THEN("The logic gets the key and the text") {
            REQUIRE(checked.spacePressed);
            REQUIRE(checked.text.find('a') != std::string::npos);
        }
    }
}
