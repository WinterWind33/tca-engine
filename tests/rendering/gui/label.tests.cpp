// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/label.hpp"

// C++ STL
#include <utility>

// Third-party
#include <imgui.h>
#include <catch2/catch_test_macros.hpp>

// Engine
#include "rendering/colors.hpp"
#include "rendering/theming/active-theme.hpp"

// Engine testing support
#include "testing/imgui/headless-imgui-context.hpp"

TEST_CASE("label class tests", "[tca][engine][rendering][gui][label]") {
    using namespace tca::engine;
    using namespace tca::engine::widgets;

    tca::engine::testing::headless_imgui_context imguiContext{};
    const theming::scoped_active_theme isolation{theming::get_active_theme()};

    SECTION("A label is described by the roles its text plays, not by a size and a color") {
        const label caption{"12 ms", theming::typography_role::caption,
                            theming::color_role::on_surface_muted};

        REQUIRE(caption.get_text() == "12 ms");
        REQUIRE(caption.get_typography_role() == theming::typography_role::caption);
        REQUIRE(caption.get_color_role() == theming::color_role::on_surface_muted);
    }

    SECTION("A label built with no roles is ordinary body text on a surface") {
        const label plain{"Hello"};

        REQUIRE(plain.get_typography_role() == theming::typography_role::body);
        REQUIRE(plain.get_color_role() == theming::color_role::on_surface);
    }

    GIVEN("A label rendered inside a window") {
        label text{"Hello"};

        // label keeps render() protected, so it is driven the way the rendering system does: as a
        // renderable.
        rendering::renderable& renderableText{text};

        WHEN("The active theme is swapped between two frames") {
            theming::set_active_theme(
                theming::theme{}.set(theming::color_role::on_surface, rendering::colors::RED));

            imguiContext.new_frame();
            ImGui::Begin("label-tests-window", nullptr, ImGuiWindowFlags_NoSavedSettings);
            REQUIRE_NOTHROW(renderableText.render());
            ImGui::End();
            imguiContext.end_frame();

            theming::set_active_theme(
                theming::theme{}.set(theming::color_role::on_surface, rendering::colors::GREEN));

            THEN("It picks the new theme up without having been told") {
                // The label resolves its roles at render time rather than caching them at
                // construction, which is what lets a theme set after startup reach it.
                imguiContext.new_frame();
                ImGui::Begin("label-tests-window", nullptr, ImGuiWindowFlags_NoSavedSettings);
                REQUIRE_NOTHROW(renderableText.render());
                ImGui::End();
                imguiContext.end_frame();

                REQUIRE(theming::color_for(text.get_color_role()) == rendering::colors::GREEN);
            }
        }
    }

    GIVEN("A body label and a display label") {
        label body{"Hello", theming::typography_role::body};
        label display{"Hello", theming::typography_role::display};

        rendering::renderable& renderableBody{body};
        rendering::renderable& renderableDisplay{display};

        // Renders the two labels in the given order within one frame and reports how tall each one
        // came out. ImGui's last-item rectangle is the text ImGui::Text() just laid out, so this
        // measures the size the label was actually rendered at.
        const auto render_heights{
            [&imguiContext](rendering::renderable& first, rendering::renderable& second) {
                imguiContext.new_frame();
                ImGui::Begin("label-typography-window", nullptr, ImGuiWindowFlags_NoSavedSettings);

                first.render();
                const float firstHeight{ImGui::GetItemRectSize().y};
                second.render();
                const float secondHeight{ImGui::GetItemRectSize().y};

                ImGui::End();
                imguiContext.end_frame();

                return std::pair{firstHeight, secondHeight};
            }};

        WHEN("They are rendered in both orders") {
            const auto [bodyFirst,
                        displaySecond]{render_heights(renderableBody, renderableDisplay)};
            const auto [displayFirst,
                        bodySecond]{render_heights(renderableDisplay, renderableBody)};

            THEN("Each one is sized by its own typography role, whatever was rendered before it") {
                // The size a role asks for has to be pushed with the font rather than assigned to
                // it afterwards: assigning it afterwards left every label wearing the size of the
                // label rendered before it, which the two orders below would disagree about.
                REQUIRE(bodyFirst == bodySecond);
                REQUIRE(displayFirst == displaySecond);

                // And display text really is the larger of the two, as its role's scale says.
                REQUIRE(displayFirst > bodyFirst);
            }
        }
    }
}
