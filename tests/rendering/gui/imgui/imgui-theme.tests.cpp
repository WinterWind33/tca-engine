// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/imgui/imgui-theme.hpp"

// C++ STL
#include <cmath>
#include <cstddef>

// Third-party
#include <imgui.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "rendering/colors.hpp"
#include "rendering/theming/active-theme.hpp"
#include "rendering/theming/theme.hpp"

// Engine testing support
#include "testing/imgui/headless-imgui-context.hpp"

namespace {

using tca::engine::rendering::color;
using tca::engine::theming::color_role;
using tca::engine::theming::COLOR_ROLE_COUNT;
using tca::engine::theming::theme;

/**
 * @brief Builds a theme painting every single color role with the same color, so that "did this
 * ImGui entry come from the theme?" can be answered for all of them at once.
 */
theme theme_painted_entirely_in(const color& paint) {
    theme uniform{};
    for (std::size_t roleIndex{}; roleIndex < COLOR_ROLE_COUNT; ++roleIndex) {
        uniform.set(static_cast<color_role>(roleIndex), paint);
    }

    return uniform;
}

/**
 * @brief Whether an ImGui color matches the given color's RGB. Alpha is deliberately excluded:
 * some entries take their role's color at a fraction of its opacity, which is a legitimate part of
 * the mapping and says nothing about whether the entry was themed.
 */
bool has_rgb_of(const ImVec4& imguiColor, const color& expected) {
    constexpr float TOLERANCE{1.0f / 255.0f};
    const auto normalized{tca::engine::rendering::to_normalized_float4(expected)};

    return std::abs(imguiColor.x - normalized[0]) <= TOLERANCE &&
           std::abs(imguiColor.y - normalized[1]) <= TOLERANCE &&
           std::abs(imguiColor.z - normalized[2]) <= TOLERANCE;
}

} // namespace

TEST_CASE("apply_theme() function tests", "[tca][engine][rendering][gui][imgui][imgui_theme]") {
    using namespace tca::engine;

    tca::engine::testing::headless_imgui_context imguiContext{};

    // Applying a theme changes the ambient one too in some of the cases below, so the surrounding
    // theme is put back on the way out.
    const theming::scoped_active_theme isolation{theming::get_active_theme()};

    SECTION("Applying a theme leaves no ImGui color unthemed") {
        constexpr color PAINT{rendering::colors::MAGENTA};
        imgui::apply_theme(theme_painted_entirely_in(PAINT));

        const ImVec4* styleColors{ImGui::GetStyle().Colors};
        for (int imguiColor{}; imguiColor < ImGuiCol_COUNT; ++imguiColor) {
            // Not a single entry may have been left holding whatever ImGui's own default style put
            // there: this is the failure mode that makes a theme look half applied and that is
            // invisible in review.
            REQUIRE(has_rgb_of(styleColors[imguiColor], PAINT));
        }
    }

    SECTION("Applying a second theme leaves nothing behind from the first") {
        imgui::apply_theme(theme_painted_entirely_in(rendering::colors::MAGENTA));
        imgui::apply_theme(theme_painted_entirely_in(rendering::colors::GREEN));

        const ImVec4* styleColors{ImGui::GetStyle().Colors};
        for (int imguiColor{}; imguiColor < ImGuiCol_COUNT; ++imguiColor) {
            REQUIRE(has_rgb_of(styleColors[imguiColor], rendering::colors::GREEN));
        }
    }

    SECTION("The theme's dimensions reach the ImGui style") {
        imgui::apply_theme(theme{}
                               .set(theming::dimension_role::corner_radius, 9.0)
                               .set(theming::dimension_role::scrollbar_size, 20.0)
                               .set(theming::dimension_role::border_thickness, 2.0));

        const ImGuiStyle& style{ImGui::GetStyle()};
        REQUIRE_THAT(style.WindowRounding, Catch::Matchers::WithinAbs(9.0f, 0.001f));
        REQUIRE_THAT(style.FrameRounding, Catch::Matchers::WithinAbs(9.0f, 0.001f));
        REQUIRE_THAT(style.ScrollbarSize, Catch::Matchers::WithinAbs(20.0f, 0.001f));
        REQUIRE_THAT(style.WindowBorderSize, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    }

    SECTION("A size scale scales the sizes the theme contributes, not its colors") {
        constexpr float SIZE_SCALE{2.0f};
        const theme scaled{theme{}.set(theming::dimension_role::scrollbar_size, 10.0)};
        imgui::apply_theme(scaled, SIZE_SCALE);

        REQUIRE_THAT(ImGui::GetStyle().ScrollbarSize, Catch::Matchers::WithinAbs(20.0f, 0.001f));
        REQUIRE(has_rgb_of(ImGui::GetStyle().Colors[ImGuiCol_WindowBg],
                           scaled.resolve(color_role::surface)));
    }

    SECTION("Applying a theme does not disturb the font scaling the display decides") {
        ImGui::GetStyle().FontScaleDpi = 1.5f;
        imgui::apply_theme(theme{});

        REQUIRE_THAT(ImGui::GetStyle().FontScaleDpi, Catch::Matchers::WithinAbs(1.5f, 0.001f));
    }

    SECTION("apply_theme() does not change which theme widgets resolve against") {
        const theme before{theming::get_active_theme()};
        imgui::apply_theme(theme_painted_entirely_in(rendering::colors::MAGENTA));

        REQUIRE(theming::get_active_theme() == before);
    }
}

TEST_CASE("use_theme() function tests", "[tca][engine][rendering][gui][imgui][imgui_theme]") {
    using namespace tca::engine;

    tca::engine::testing::headless_imgui_context imguiContext{};
    const theming::scoped_active_theme isolation{theming::get_active_theme()};

    GIVEN("An application already wearing a theme") {
        imgui::use_theme(theme_painted_entirely_in(rendering::colors::MAGENTA));

        WHEN("A different theme is used after startup") {
            imgui::use_theme(theme_painted_entirely_in(rendering::colors::GREEN));

            THEN("Widgets resolving roles see the new one") {
                REQUIRE(theming::color_for(color_role::accent) == rendering::colors::GREEN);
            }

            THEN("The ImGui style sees it too, with nothing left from the previous theme") {
                const ImVec4* styleColors{ImGui::GetStyle().Colors};
                for (int imguiColor{}; imguiColor < ImGuiCol_COUNT; ++imguiColor) {
                    REQUIRE(has_rgb_of(styleColors[imguiColor], rendering::colors::GREEN));
                }
            }
        }
    }

    SECTION("Re-theming keeps the DPI scale the application is running at") {
        ImGui::GetStyle().FontScaleDpi = 2.0f;
        imgui::use_theme(theme{}.set(theming::dimension_role::scrollbar_size, 10.0));

        REQUIRE_THAT(ImGui::GetStyle().ScrollbarSize, Catch::Matchers::WithinAbs(20.0f, 0.001f));
        REQUIRE_THAT(ImGui::GetStyle().FontScaleDpi, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    }
}
