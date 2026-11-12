// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "web/gui/web-view.hpp"

// C++ STL
#include <functional>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <imgui.h>

#include "rendering/tickables.hpp"

// Engine testing support
#include "testing/imgui/headless-imgui-context.hpp"

namespace {

// web_view::render() draws directly into the current ImGui window's draw list, which requires an
// enclosing ImGui::Begin()/End() window scope to be active, exactly like any other ImGui drawing
// code.
void render_within_a_window(const std::function<void()>& body) {
    ImGui::Begin("web-view-tests-window", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoSavedSettings);
    body();
    ImGui::End();
}

} // namespace

// These tests intentionally only assert on the widget's *contract* as a rendering::renderable
// (does not throw, is safe to call every frame, reserves its own layout space) rather than on
// what it currently draws. What render() paints will grow from a placeholder rectangle into a
// full HTML/CSS display list over many future stages (see docs/web/html-renderer-roadmap.md), so
// pixel-level assertions here would need to be rewritten at every stage. Once the paint pipeline
// exists (Stage 7 of the roadmap), display-list-level tests should live alongside the painter
// instead of here.
TEST_CASE("web_view class tests", "[tca][engine][web][gui][web_view]") {
    using tca::engine::web::gui::web_view;

    tca::engine::testing::headless_imgui_context imguiContext{};

    GIVEN("A default-constructed web_view") {
        web_view view{};

        WHEN("It is rendered within a window") {
            imguiContext.new_frame();

            THEN("It doesn't throw") {
                render_within_a_window([&view]() {
                    REQUIRE_NOTHROW(view.render());
                });
            }

            imguiContext.end_frame();
        }

        WHEN("It is rendered across multiple frames, as it would be every frame in the real "
             "application loop") {
            THEN("None of the frames throw") {
                for (std::size_t frameIndex{}; frameIndex < 5; ++frameIndex) {
                    imguiContext.new_frame();
                    render_within_a_window([&view]() {
                        REQUIRE_NOTHROW(view.render());
                    });
                    imguiContext.end_frame();
                }
            }
        }

        WHEN("It is rendered through a rendering::renderable reference, as the widget tree "
             "does") {
            tca::engine::rendering::renderable& asRenderable{view};

            imguiContext.new_frame();

            THEN("It doesn't throw, i.e. web_view is usable polymorphically like any other "
                 "renderable") {
                render_within_a_window([&asRenderable]() {
                    REQUIRE_NOTHROW(asRenderable.render());
                });
            }

            imguiContext.end_frame();
        }

        WHEN("It is rendered within a window with available content space") {
            imguiContext.new_frame();

            float cursorYBeforeRender{};
            float cursorYAfterRender{};
            render_within_a_window([&view, &cursorYBeforeRender, &cursorYAfterRender]() {
                cursorYBeforeRender = ImGui::GetCursorPosY();
                view.render();
                cursorYAfterRender = ImGui::GetCursorPosY();
            });

            imguiContext.end_frame();

            THEN("It reserves layout space in its parent container, so that sibling widgets "
                 "rendered after it are not overlapped, regardless of what it actually paints") {
                REQUIRE(cursorYAfterRender > cursorYBeforeRender);
            }
        }
    }
}
