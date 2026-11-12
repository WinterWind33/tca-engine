// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "testing/imgui/headless-imgui-context.hpp"

// Third-party
#include <imgui.h>
#include <catch2/catch_test_macros.hpp>

// The headless ImGui context is part of the engine's public testing support surface
// (TCAEngine::testing), so downstream games rely on it directly. The rest of this suite only
// exercises it incidentally, while testing widgets; these cases pin down its own contract.
TEST_CASE("headless_imgui_context class tests", "[tca][engine][testing][headless_imgui_context]") {
    SECTION("Construction") {
        GIVEN("No current ImGui context") {
            ImGui::SetCurrentContext(nullptr);

            WHEN("A headless context is constructed") {
                tca::engine::testing::headless_imgui_context context{};

                THEN("It becomes the current ImGui context") {
                    REQUIRE(ImGui::GetCurrentContext() != nullptr);
                }

                THEN("The font atlas is built, as no rendering backend will build it") {
                    CHECK(ImGui::GetIO().Fonts->IsBuilt());
                }

                THEN("The default display size is reported") {
                    CHECK(ImGui::GetIO().DisplaySize.x == 1920.0f);
                    CHECK(ImGui::GetIO().DisplaySize.y == 1080.0f);
                }
            }

            WHEN("A headless context is constructed with an explicit display size") {
                tca::engine::testing::headless_imgui_context context{
                    ImVec2{800.0f, 600.0f}
                };

                THEN("That size is what ImGui reports") {
                    CHECK(ImGui::GetIO().DisplaySize.x == 800.0f);
                    CHECK(ImGui::GetIO().DisplaySize.y == 600.0f);
                }
            }
        }
    }

    SECTION("Driving frames") {
        tca::engine::testing::headless_imgui_context context{};

        GIVEN("An open frame") {
            context.new_frame();

            WHEN("A window is drawn into it") {
                ImGui::Begin("Test window");
                const bool viewportAvailable{ImGui::GetMainViewport() != nullptr};
                ImGui::End();

                context.end_frame();

                THEN("The main viewport was available to the widget code") {
                    CHECK(viewportAvailable);
                }

                THEN("Further frames can still be driven") {
                    context.new_frame();
                    context.end_frame();

                    SUCCEED("Two consecutive frames completed without tripping an ImGui assert");
                }
            }
        }

        GIVEN("A frame left open") {
            context.new_frame();

            THEN("Destroying the context still tears ImGui down cleanly") {
                // Nothing to assert explicitly: the point of this case is that the destructor
                // running with a frame still open does not trip ImGui's own asserts, which is the
                // situation a failing test case leaves behind when it unwinds mid-frame.
                SUCCEED("The context destructor closes the open frame");
            }
        }
    }

    SECTION("Resizing the simulated viewport") {
        tca::engine::testing::headless_imgui_context context{
            ImVec2{1280.0f, 720.0f}
        };

        GIVEN("A context that has already driven a frame") {
            context.new_frame();
            context.end_frame();

            WHEN("The display size is changed in between frames") {
                context.set_display_size(ImVec2{2560.0f, 1440.0f});

                THEN("ImGui reports the new size immediately") {
                    CHECK(ImGui::GetIO().DisplaySize.x == 2560.0f);
                    CHECK(ImGui::GetIO().DisplaySize.y == 1440.0f);
                }

                THEN("The next frame sees the new main viewport size") {
                    context.new_frame();
                    const ImVec2 viewportSize{ImGui::GetMainViewport()->Size};
                    context.end_frame();

                    CHECK(viewportSize.x == 2560.0f);
                    CHECK(viewportSize.y == 1440.0f);
                }
            }
        }
    }
}
