// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/imgui/imgui-color-helpers.hpp"

// Third-party
#include <imgui.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "rendering/colors.hpp"
#include "rendering/theming/active-theme.hpp"

// Engine testing support
#include "testing/imgui/headless-imgui-context.hpp"

namespace {

constexpr float COMPONENT_TOLERANCE{1.0f / 255.0f};

void require_imvec4_is(const ImVec4& actual, const tca::engine::rendering::color& expected) {
    const auto normalized{tca::engine::rendering::to_normalized_float4(expected)};
    REQUIRE_THAT(actual.x, Catch::Matchers::WithinAbs(normalized[0], COMPONENT_TOLERANCE));
    REQUIRE_THAT(actual.y, Catch::Matchers::WithinAbs(normalized[1], COMPONENT_TOLERANCE));
    REQUIRE_THAT(actual.z, Catch::Matchers::WithinAbs(normalized[2], COMPONENT_TOLERANCE));
    REQUIRE_THAT(actual.w, Catch::Matchers::WithinAbs(normalized[3], COMPONENT_TOLERANCE));
}

} // namespace

TEST_CASE("to_imvec4() function tests",
          "[tca][engine][rendering][gui][imgui][imgui_color_helpers]") {
    using namespace tca::engine;

    SECTION("Every component is normalized, alpha included") {
        require_imvec4_is(imgui::to_imvec4(rendering::rgba(255, 128, 0, 64)),
                          rendering::rgba(255, 128, 0, 64));
    }
}

TEST_CASE("scoped_style_color class tests",
          "[tca][engine][rendering][gui][imgui][imgui_color_helpers]") {
    using namespace tca::engine;

    tca::engine::testing::headless_imgui_context imguiContext{};
    const theming::scoped_active_theme isolation{
        theming::theme{}.set(theming::color_role::on_surface_muted, rendering::colors::MAGENTA)};

    // The style stack is only usable inside a frame, exactly like the widget code this exists for.
    imguiContext.new_frame();

    GIVEN("A style color overridden for a scope by naming a role") {
        const ImVec4 before{ImGui::GetStyleColorVec4(ImGuiCol_Text)};

        WHEN("The scope is open") {
            {
                const imgui::scoped_style_color muted{ImGuiCol_Text,
                                                      theming::color_role::on_surface_muted};

                THEN("The role's color from the active theme is what ImGui draws with") {
                    require_imvec4_is(ImGui::GetStyleColorVec4(ImGuiCol_Text),
                                      rendering::colors::MAGENTA);
                }
            }

            THEN("The previous color is back once the scope ends") {
                const ImVec4 after{ImGui::GetStyleColorVec4(ImGuiCol_Text)};
                REQUIRE_THAT(after.x, Catch::Matchers::WithinAbs(before.x, 0.001f));
                REQUIRE_THAT(after.y, Catch::Matchers::WithinAbs(before.y, 0.001f));
                REQUIRE_THAT(after.z, Catch::Matchers::WithinAbs(before.z, 0.001f));
                REQUIRE_THAT(after.w, Catch::Matchers::WithinAbs(before.w, 0.001f));
            }
        }
    }

    SECTION("An explicit color can be pushed, for colors that were computed") {
        const imgui::scoped_style_color computed{ImGuiCol_Button, rendering::rgb(1, 2, 3)};
        require_imvec4_is(ImGui::GetStyleColorVec4(ImGuiCol_Button), rendering::rgb(1, 2, 3));
    }

    SECTION("Overrides nest, innermost first") {
        const imgui::scoped_style_color outer{ImGuiCol_Text, rendering::colors::RED};
        {
            const imgui::scoped_style_color inner{ImGuiCol_Text, rendering::colors::GREEN};
            require_imvec4_is(ImGui::GetStyleColorVec4(ImGuiCol_Text), rendering::colors::GREEN);
        }

        require_imvec4_is(ImGui::GetStyleColorVec4(ImGuiCol_Text), rendering::colors::RED);
    }

    imguiContext.end_frame();
}
