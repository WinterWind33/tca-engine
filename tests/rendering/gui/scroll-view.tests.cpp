// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/scroll-view.hpp"

// C++ STL
#include <functional>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <imgui.h>

// Engine testing support
#include "testing/imgui/headless-imgui-context.hpp"

namespace {

using tca::engine::rendering::renderable;

class render_counter_fake final : public renderable {
public:
    void render() override {
        ++m_renderCount;
    }

    std::size_t get_render_count() const noexcept {
        return m_renderCount;
    }

private:
    std::size_t m_renderCount{};
};

// scroll_view::render() calls ImGui::BeginChild()/EndChild(), which require an enclosing
// ImGui::Begin()/End() window scope to be active, exactly like any other ImGui child region.
void render_within_a_window(const std::function<void()>& body) {
    ImGui::Begin("scroll-view-tests-window", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoSavedSettings);
    body();
    ImGui::End();
}

} // namespace

TEST_CASE("scroll_view class tests", "[tca][engine][rendering][gui][scroll_view]") {
    using namespace tca::engine::widgets;

    tca::engine::testing::headless_imgui_context imguiContext{};

    GIVEN("A scroll_view with a renderable child") {
        scroll_view view{"##test-scroll-view"};
        render_counter_fake child{};
        view.add_renderable(child);

        WHEN("It is rendered within a window") {
            imguiContext.new_frame();
            render_within_a_window([&view]() {
                REQUIRE_NOTHROW(view.render());
            });
            imguiContext.end_frame();

            THEN("The child is rendered") {
                REQUIRE(child.get_render_count() == 1);
            }
        }

        WHEN("The child is paused and the view is rendered within a window") {
            view.pause_renderable(child);

            imguiContext.new_frame();
            render_within_a_window([&view]() {
                view.render();
            });
            imguiContext.end_frame();

            THEN("The child is not rendered, since scroll_view must respect pause_renderable() "
                 "exactly like any other pausable_renderables_view") {
                REQUIRE(child.get_render_count() == 0);
            }
        }
    }
}
