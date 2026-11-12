// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/scene-surface.hpp"

// A game must be able to place a surface and draw into it without including an ImGui header: the
// ImGui dependency stays inside the surface, so a later backend-level pass can replace what is
// underneath without every caller changing. Checked right after the surface header, before this
// file includes ImGui itself to inspect what the surface drew.
#if defined(IMGUI_VERSION)
#error "rendering/scene/scene-surface.hpp must not include an ImGui header"
#endif

// C++ STL
#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// Third-party
#include <imgui.h>
#include <imgui_internal.h>
#include <Eigen/Geometry>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "core/math/vector.hpp"
#include "core/utilities/ranged-value.hpp"
#include "rendering/colors.hpp"
#include "rendering/gui/layout-widgets.hpp"
#include "rendering/gui/scroll-view.hpp"
#include "rendering/gui/widget.hpp"
#include "rendering/scene/camera-2d.hpp"
#include "rendering/scene/scene-renderable.hpp"
#include "rendering/theming/active-theme.hpp"
#include "rendering/theming/theme-roles.hpp"
#include "rendering/theming/theme.hpp"

// Engine testing support
#include "testing/imgui/headless-imgui-context.hpp"

// Test utilities
#include "utilities/scene-drawing.hpp"

namespace {

using tca::engine::math::vector2;
using tca::engine::rendering::color;
using tca::engine::scene::callback_scene_renderable;
using tca::engine::scene::camera_2d;
using tca::engine::scene::fill_window;
using tca::engine::scene::grid_options;
using tca::engine::scene::render_context;
using tca::engine::scene::scene_painter;
using tca::engine::scene::scene_renderable;
using tca::engine::scene::scene_surface;
using tca::engine::scene::screen_rect;
using tca::engine::scene::world_rect;
using tca::engine::testing::headless_imgui_context;
using tca::engine::theming::color_role;
using tca::tests::utilities::painting;
using tca::tests::utilities::scoped_scene_drawing;

using Catch::Matchers::WithinAbs;

namespace theming = tca::engine::theming;

// ImGui lays out in floats and the surface reports doubles, so positions only agree to float
// precision.
constexpr double PIXEL_EPSILON{1e-3};

constexpr ImVec2 DISPLAY_SIZE{1280.0f, 720.0f};

constexpr ImVec2 HOST_POSITION{40.0f, 30.0f};
constexpr ImVec2 HOST_SIZE{800.0f, 600.0f};
constexpr ImGuiWindowFlags HOST_FLAGS{
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse};

// Colours no default role uses, so a vertex carrying one can only come from what the test asked
// for.
constexpr color PROBE_COLOR{tca::engine::rendering::rgba(11, 22, 33, 255)};
constexpr color BACKGROUND_PROBE_COLOR{tca::engine::rendering::rgba(201, 13, 77, 255)};
constexpr color GRID_PROBE_COLOR{tca::engine::rendering::rgba(3, 250, 9, 255)};
constexpr color OTHER_PROBE_COLOR{tca::engine::rendering::rgba(99, 1, 180, 255)};

// ======================================= Helpers =======================================

void require_near(const double actual, const double expected,
                  const double epsilon = PIXEL_EPSILON) {
    REQUIRE_THAT(actual, WithinAbs(expected, epsilon));
}

void require_near(const Eigen::Vector2d& actual, const Eigen::Vector2d& expected,
                  const double epsilon = PIXEL_EPSILON) {
    require_near(actual.x(), expected.x(), epsilon);
    require_near(actual.y(), expected.y(), epsilon);
}

void require_same_rect(const screen_rect& actual, const screen_rect& expected,
                       const double epsilon = PIXEL_EPSILON) {
    require_near(actual.min(), expected.min(), epsilon);
    require_near(actual.max(), expected.max(), epsilon);
}

Eigen::Vector2d to_eigen(const ImVec2 value) {
    return Eigen::Vector2d{static_cast<double>(value.x), static_cast<double>(value.y)};
}

screen_rect rect_from(const Eigen::Vector2d& position, const Eigen::Vector2d& size) {
    return screen_rect{position, position + size};
}

ImU32 to_imu32(const color& value) {
    return IM_COL32(value.r, value.g, value.b, value.a);
}

// Whether inner lies entirely within outer, allowing for float layout.
bool contains(const screen_rect& outer, const screen_rect& inner) {
    return inner.min().x() >= outer.min().x() - PIXEL_EPSILON &&
           inner.min().y() >= outer.min().y() - PIXEL_EPSILON &&
           inner.max().x() <= outer.max().x() + PIXEL_EPSILON &&
           inner.max().y() <= outer.max().y() + PIXEL_EPSILON;
}

// The draw list the scene is currently drawing into, and the clip rectangle applied to it.
//
// The surface hands no ImDrawList* out; these tests reach it from ImGui's side instead. Inside a
// scene renderable, the current window's draw list is the one the surface draws into - whether the
// surface opens a child window of its own or pushes a clip rectangle onto its host's.
ImDrawList& current_draw_list() {
    return *ImGui::GetWindowDrawList();
}

screen_rect current_clip_rect() {
    const ImDrawList& drawList{current_draw_list()};
    return screen_rect{to_eigen(drawList.GetClipRectMin()), to_eigen(drawList.GetClipRectMax())};
}

// Vertices of the current draw list from the given index on: what was drawn since that point.
std::vector<ImDrawVert> vertices_since(const int firstVertex) {
    const ImDrawList& drawList{current_draw_list()};
    return std::vector<ImDrawVert>(drawList.VtxBuffer.begin() + firstVertex,
                                   drawList.VtxBuffer.end());
}

std::vector<ImDrawVert> vertices_with_color(const std::vector<ImDrawVert>& vertices,
                                            const color& wanted) {
    std::vector<ImDrawVert> matching{};
    for (const ImDrawVert& vertex : vertices) {
        if (vertex.col == to_imu32(wanted)) {
            matching.push_back(vertex);
        }
    }
    return matching;
}

screen_rect bounding_box(const std::vector<ImDrawVert>& vertices) {
    screen_rect box{};
    for (const ImDrawVert& vertex : vertices) {
        box.extend(to_eigen(vertex.pos));
    }
    return box;
}

// Runs one painter call and returns exactly the vertices it produced.
std::vector<ImDrawVert> vertices_drawn_by(const std::function<void()>& paint) {
    const int firstVertex{current_draw_list().VtxBuffer.Size};
    paint();
    return vertices_since(firstVertex);
}

// Where the painter should put a world point, given the surface's bounds and camera.
Eigen::Vector2d expected_screen_point(const screen_rect& bounds, const camera_2d& camera,
                                      const vector2& world) {
    return bounds.min() + Eigen::Vector2d{camera.world_to_screen(world)};
}

// A snapshot of the ImGui state the surface must leave exactly as it found it.
struct imgui_state_snapshot {
    ImGuiWindow* currentWindow{};
    ImDrawList* drawList{};
    int windowStackDepth{};
    int colorStackDepth{};
    int styleVarStackDepth{};
    int clipRectStackDepth{};
    screen_rect clipRect{};

    static imgui_state_snapshot take() {
        const ImGuiContext& context{*ImGui::GetCurrentContext()};
        return imgui_state_snapshot{
            .currentWindow = ImGui::GetCurrentWindow(),
            .drawList = ImGui::GetWindowDrawList(),
            .windowStackDepth = context.CurrentWindowStack.Size,
            .colorStackDepth = context.ColorStack.Size,
            .styleVarStackDepth = context.StyleVarStack.Size,
            .clipRectStackDepth = ImGui::GetWindowDrawList()->_ClipRectStack.Size,
            .clipRect = current_clip_rect(),
        };
    }
};

void require_same_state(const imgui_state_snapshot& actual, const imgui_state_snapshot& expected) {
    REQUIRE(actual.currentWindow == expected.currentWindow);
    REQUIRE(actual.drawList == expected.drawList);
    REQUIRE(actual.windowStackDepth == expected.windowStackDepth);
    REQUIRE(actual.colorStackDepth == expected.colorStackDepth);
    REQUIRE(actual.styleVarStackDepth == expected.styleVarStackDepth);
    REQUIRE(actual.clipRectStackDepth == expected.clipRectStackDepth);
    require_same_rect(actual.clipRect, expected.clipRect);
}

// Runs one simulated frame around body.
void run_frame(headless_imgui_context& context, const std::function<void()>& body) {
    context.new_frame();
    body();
    context.end_frame();
}

// Renders body inside a fixed "page": a plain ImGui window of known position and size, standing
// for whatever page a game hosts the surface in.
void render_in_host(const std::function<void()>& body, const ImVec2 hostSize = HOST_SIZE) {
    ImGui::SetNextWindowPos(HOST_POSITION, ImGuiCond_Always);
    ImGui::SetNextWindowSize(hostSize, ImGuiCond_Always);
    ImGui::Begin("scene-surface-tests-host", nullptr, HOST_FLAGS);

    // A failed REQUIRE inside body throws: End() must still run, or ImGui aborts the whole test
    // binary with a "Missing End()" assertion instead of Catch reporting the failure.
    try {
        body();
    } catch (...) {
        ImGui::End();
        throw;
    }
    ImGui::End();
}

// Where the surface should land when rendered next inside the host: the host's cursor, and the
// content region still available from it.
struct host_layout {
    Eigen::Vector2d cursor;
    Eigen::Vector2d available;

    static host_layout take() {
        return host_layout{
            .cursor = to_eigen(ImGui::GetCursorScreenPos()),
            .available = to_eigen(ImGui::GetContentRegionAvail()),
        };
    }
};

// What a scene renderable saw on its last render.
struct callback_observation {
    std::size_t calls{};
    screen_rect bounds{};
    screen_rect clipRect{};
    vector2 viewportSize{};
    const camera_2d* camera{};
    int verticesAtStart{};
    std::vector<ImDrawVert> verticesBeforeScene{};
};

std::function<void(scene_painter&)> observing(callback_observation& observation,
                                              std::function<void(scene_painter&)> then = {}) {
    return [&observation, then = std::move(then)](scene_painter& painter) {
        ++observation.calls;
        observation.bounds = painter.bounds();
        observation.clipRect = current_clip_rect();
        observation.viewportSize = painter.camera().viewport_size();
        observation.camera = &painter.camera();
        observation.verticesAtStart = current_draw_list().VtxBuffer.Size;
        observation.verticesBeforeScene = vertices_since(0);

        if (then) {
            then(painter);
        }
    };
}

theming::theme theme_with(const color_role role, const color& value) {
    theming::theme customTheme{};
    customTheme.set(role, value);
    return customTheme;
}

} // namespace

// ================================= Compile-time contract =================================

static_assert(tca::engine::widgets::widget<scene_surface>,
              "the surface follows the rendering/gui/widget.hpp conventions");

static_assert(std::is_invocable_r_v<void, decltype(&scene_renderable::render), scene_renderable&,
                                    render_context&>,
              "scene content is handed a context, so what it gets can grow without the signature "
              "changing");

static_assert(
    std::same_as<decltype(std::declval<const render_context&>().painter()), scene_painter&>,
    "scene content draws through a painter, never through a raw ImDrawList*");

static_assert(!std::is_constructible_v<render_context, scene_painter&> &&
                  !std::is_copy_constructible_v<render_context> &&
                  !std::is_move_constructible_v<render_context>,
              "only the surface builds a render context, for the duration of its draw");

static_assert(!std::is_invocable_v<decltype(&scene_surface::add_renderable), scene_surface&,
                                   tca::engine::rendering::renderable&>,
              "only scene renderables can be added: a plain renderable takes no render context");

static_assert(std::same_as<screen_rect, Eigen::AlignedBox2d>,
              "screen rectangles reuse Eigen's box type, like world_rect");

static_assert(std::same_as<decltype(std::declval<const scene_surface&>().bounds()), screen_rect>);
static_assert(std::same_as<decltype(std::declval<const scene_painter&>().bounds()), screen_rect>);

static_assert(std::same_as<decltype(std::declval<scene_surface&>().camera()), camera_2d&>,
              "the game drives the camera (pan, zoom) through the surface");
static_assert(
    std::same_as<decltype(std::declval<const scene_surface&>().camera()), const camera_2d&>);
static_assert(
    std::same_as<decltype(std::declval<const scene_painter&>().camera()), const camera_2d&>,
    "a scene renderable reads the camera; it must not move it mid-frame");

static_assert(!std::is_copy_constructible_v<scene_painter> &&
                  !std::is_copy_assignable_v<scene_painter>,
              "a painter only lives for the render() call it was handed to");

static_assert(!std::is_copy_constructible_v<scene_surface>, "a surface owns an ImGui ID");

// The spacing range goes through core's ranged_value rather than a bespoke check, so a bad
// spacing is rejected as soon as the grid options are built.
static_assert(
    std::same_as<grid_options::spacing_type,
                 tca::engine::core::ranged_value<double, 0.0, std::numeric_limits<double>::max(),
                                                 tca::engine::core::rv_bound_policy::min_open>>);

// ====================================== Construction ======================================

TEST_CASE("scene_surface construction", "[tca][engine][rendering][scene][scene_surface]") {
    SECTION("A surface owns a default camera until it is first laid out") {
        const scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };

        REQUIRE(surface.camera().position() == vector2::zero());
        REQUIRE(surface.camera().zoom() == 1.0);
        REQUIRE(surface.camera().viewport_size() == vector2::zero());
    }

    SECTION("A surface that has never been rendered has empty bounds") {
        const scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };

        REQUIRE(surface.bounds().isEmpty());
    }

    SECTION("A negative size is rejected") {
        const vector2 negativeWidth{-1.0, 200.0};
        const vector2 negativeHeight{320.0, -1.0};

        REQUIRE_THROWS_AS(scene_surface("##surface", negativeWidth), std::invalid_argument);
        REQUIRE_THROWS_AS(scene_surface("##surface", negativeHeight), std::invalid_argument);
    }

    SECTION("A NaN or infinite size is rejected") {
        const double bad = GENERATE(std::numeric_limits<double>::quiet_NaN(),
                                    std::numeric_limits<double>::infinity(),
                                    -std::numeric_limits<double>::infinity());
        CAPTURE(bad);
        const vector2 badWidth{bad, 200.0};
        const vector2 badHeight{320.0, bad};

        REQUIRE_THROWS_AS(scene_surface("##surface", badWidth), std::invalid_argument);
        REQUIRE_THROWS_AS(scene_surface("##surface", badHeight), std::invalid_argument);
    }

    SECTION("The background defaults to the surface colour role") {
        const scene_surface surface{"##surface"};

        REQUIRE(surface.background_role() == std::optional<color_role>{color_role::surface});
    }

    SECTION("There is no grid by default") {
        const scene_surface surface{"##surface"};

        REQUIRE_FALSE(surface.grid().has_value());
    }

    SECTION("A grid defaults to the quiet outline role") {
        REQUIRE(grid_options{}.role == color_role::outline_variant);
    }

    SECTION("A grid spacing defaults to 32 world units") {
        REQUIRE(grid_options{}.spacing == 32.0);
    }

    SECTION("A zero, negative or non-finite grid spacing is rejected before it reaches a surface") {
        const double badSpacing = GENERATE(0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
                                           std::numeric_limits<double>::infinity());
        CAPTURE(badSpacing);

        REQUIRE_THROWS_AS(grid_options{.spacing = badSpacing},
                          tca::engine::core::ranged_value_exception);
    }

    SECTION("The smallest positive spacing is accepted") {
        scene_surface surface{"##surface"};
        const double tinySpacing{std::numeric_limits<double>::denorm_min()};

        surface.set_grid(grid_options{.spacing = tinySpacing});

        REQUIRE(surface.grid().has_value());
        REQUIRE(surface.grid()->spacing == tinySpacing);
    }
}

// ================================= Layout within a page =================================

TEST_CASE("scene_surface layout inside a page",
          "[tca][engine][rendering][scene][scene_surface][layout]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    SECTION("A fixed-size surface reserves exactly its size at the host's cursor") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        host_layout layout{};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                layout = host_layout::take();
                surface.render();
            });
        });

        require_same_rect(surface.bounds(),
                          rect_from(layout.cursor, Eigen::Vector2d{320.0, 200.0}));
    }

    SECTION("Widgets rendered after the surface are laid out below it, not on top of it") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        Eigen::Vector2d cursorAfter{};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
                cursorAfter = to_eigen(ImGui::GetCursorScreenPos());
            });
        });

        REQUIRE(cursorAfter.y() >= surface.bounds().max().y() - PIXEL_EPSILON);
    }

    SECTION("A zero size fills the content region still available in the page") {
        scene_surface surface{"##surface"};
        host_layout layout{};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                layout = host_layout::take();
                surface.render();
            });
        });

        require_same_rect(surface.bounds(), rect_from(layout.cursor, layout.available));
    }

    SECTION("A zero width fills the available width only, keeping the fixed height") {
        scene_surface surface{
            "##surface", vector2{0.0, 150.0}
        };
        host_layout layout{};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                layout = host_layout::take();
                surface.render();
            });
        });

        require_same_rect(surface.bounds(),
                          rect_from(layout.cursor, Eigen::Vector2d{layout.available.x(), 150.0}));
    }

    SECTION("A zero height fills the available height only, keeping the fixed width") {
        scene_surface surface{
            "##surface", vector2{250.0, 0.0}
        };
        host_layout layout{};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                layout = host_layout::take();
                surface.render();
            });
        });

        require_same_rect(surface.bounds(),
                          rect_from(layout.cursor, Eigen::Vector2d{250.0, layout.available.y()}));
    }

    SECTION("A filling surface placed below other content only takes what is left") {
        scene_surface surface{"##surface"};
        host_layout layout{};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                ImGui::TextUnformatted("A page header above the scene");
                layout = host_layout::take();
                surface.render();
            });
        });

        require_same_rect(surface.bounds(), rect_from(layout.cursor, layout.available));
        REQUIRE(surface.bounds().max().y() <= HOST_POSITION.y + HOST_SIZE.y + PIXEL_EPSILON);
    }

    SECTION("Two surfaces with different IDs in one page get separate, non-overlapping bounds") {
        scene_surface first{
            "##first", vector2{200.0, 100.0}
        };
        scene_surface second{
            "##second", vector2{200.0, 100.0}
        };

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                first.render();
                second.render();
            });
        });

        // ImGui stacks items vertically by default: the second surface starts below the first.
        REQUIRE(second.bounds().min().y() >= first.bounds().max().y() - PIXEL_EPSILON);
    }

    SECTION("A surface can be a child of the engine's own layout widgets") {
        tca::engine::widgets::viewport_layout_widget page{"##page"};
        tca::engine::widgets::scroll_view scrollView{
            "##scroll", vector2{600.0, 400.0}
        };
        scene_surface surface{
            "##surface", vector2{300.0, 200.0}
        };
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        scrollView.add_renderable(surface);
        page.add_renderable(scrollView);

        run_frame(imguiContext, [&] {
            static_cast<tca::engine::rendering::renderable&>(page).render();
        });

        REQUIRE(observation.calls == 1);
        REQUIRE(surface.bounds().sizes().isApprox(Eigen::Vector2d{300.0, 200.0}));
    }
}

// ============================ Camera viewport synchronisation ============================

TEST_CASE("scene_surface keeps its camera's viewport in sync with its size",
          "[tca][engine][rendering][scene][scene_surface][camera]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    SECTION("After a render, the camera viewport is the surface's size") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        require_near(surface.camera().viewport_size(), Eigen::Vector2d{surface.bounds().sizes()});
        require_near(surface.camera().viewport_size(), Eigen::Vector2d{320.0, 200.0});
    }

    SECTION("A renderable already sees this frame's viewport, never last frame's") {
        scene_surface surface{"##surface"};
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host(
                [&] {
                    surface.render();
                },
                ImVec2{800.0f, 600.0f});
        });
        const vector2 firstViewport{observation.viewportSize};

        run_frame(imguiContext, [&] {
            render_in_host(
                [&] {
                    surface.render();
                },
                ImVec2{1000.0f, 500.0f});
        });

        REQUIRE_FALSE(observation.viewportSize.isApprox(firstViewport));
        require_near(observation.viewportSize, Eigen::Vector2d{observation.bounds.sizes()});
        require_near(observation.viewportSize, Eigen::Vector2d{surface.bounds().sizes()});
    }

    SECTION("Resizing the page resizes the viewport of a filling surface") {
        scene_surface surface{"##surface"};
        host_layout layout{};

        // Parenthesised rather than braced: GENERATE would split a braced ImVec2{x, y} at the
        // comma.
        const ImVec2 hostSize = GENERATE(ImVec2(800.0f, 600.0f), ImVec2(1000.0f, 500.0f),
                                         ImVec2(300.0f, 700.0f), ImVec2(1200.0f, 680.0f));
        CAPTURE(hostSize.x, hostSize.y);

        // A first frame at another size, so the surface really has to follow a change.
        run_frame(imguiContext, [&] {
            render_in_host(
                [&] {
                    surface.render();
                },
                ImVec2{640.0f, 480.0f});
        });
        run_frame(imguiContext, [&] {
            render_in_host(
                [&] {
                    layout = host_layout::take();
                    surface.render();
                },
                hostSize);
        });

        require_near(surface.camera().viewport_size(), layout.available);
    }

    SECTION("A resize keeps the game's position and zoom: the world stays anchored at the centre") {
        scene_surface surface{"##surface"};
        surface.camera().set_position(vector2{37.0, -12.0});
        surface.camera().set_zoom(2.0);

        run_frame(imguiContext, [&] {
            render_in_host(
                [&] {
                    surface.render();
                },
                ImVec2{800.0f, 600.0f});
        });
        run_frame(imguiContext, [&] {
            render_in_host(
                [&] {
                    surface.render();
                },
                ImVec2{1100.0f, 450.0f});
        });

        REQUIRE(surface.camera().position() == vector2{37.0, -12.0});
        REQUIRE(surface.camera().zoom() == 2.0);

        // The camera position is still shown at the centre of the resized surface.
        const Eigen::Vector2d centre{surface.bounds().center()};
        require_near(
            expected_screen_point(surface.bounds(), surface.camera(), surface.camera().position()),
            centre);
    }

    SECTION("A render does not touch the position or the zoom the game set") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        surface.camera().set_position(vector2{-4.5, 9.0});
        surface.camera().set_zoom(0.25);

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE(surface.camera().position() == vector2{-4.5, 9.0});
        REQUIRE(surface.camera().zoom() == 0.25);
    }
}

// ================================== Scene renderables ==================================

TEST_CASE("scene_surface scene renderables",
          "[tca][engine][rendering][scene][scene_surface][drawing]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    scene_surface surface{
        "##surface", vector2{320.0, 200.0}
    };
    const auto renderOnce{[&] {
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });
    }};

    SECTION("A surface with no renderables renders fine") {
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                REQUIRE_NOTHROW(surface.render());
            });
        });
    }

    SECTION("A renderable is drawn exactly once per render") {
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        renderOnce();
        REQUIRE(observation.calls == 1);

        renderOnce();
        REQUIRE(observation.calls == 2);
    }

    SECTION("The painter reports the surface's bounds and hands out the surface's camera") {
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        renderOnce();

        require_same_rect(observation.bounds, surface.bounds());
        REQUIRE(observation.camera == &surface.camera());
    }

    SECTION("Renderables draw in the order they were added, through one painter") {
        std::vector<int> order{};
        std::vector<const scene_painter*> painters{};
        const auto recording{[&](const int id) {
            return [&order, &painters, id](scene_painter& painter) {
                order.push_back(id);
                painters.push_back(&painter);
            };
        }};
        callback_scene_renderable first{painting(recording(1))};
        callback_scene_renderable second{painting(recording(2))};
        callback_scene_renderable third{painting(recording(3))};
        surface.add_renderable(second);
        surface.add_renderable(first);
        surface.add_renderable(third);

        renderOnce();

        REQUIRE(order == std::vector<int>{2, 1, 3});
        REQUIRE(std::ranges::all_of(painters, [&](const scene_painter* painter) {
            return painter == painters.front();
        }));
    }

    SECTION("Adding the same renderable twice draws it once") {
        callback_observation observation{};
        callback_scene_renderable renderable{painting(observing(observation))};
        surface.add_renderable(renderable);
        surface.add_renderable(renderable);

        renderOnce();

        REQUIRE(observation.calls == 1);
    }

    SECTION("A removed renderable is no longer drawn, and the others still are") {
        callback_observation removed{};
        callback_observation kept{};
        callback_scene_renderable removedRenderable{painting(observing(removed))};
        callback_scene_renderable keptRenderable{painting(observing(kept))};
        surface.add_renderable(removedRenderable);
        surface.add_renderable(keptRenderable);

        surface.remove_renderable(removedRenderable);
        renderOnce();

        REQUIRE(removed.calls == 0);
        REQUIRE(kept.calls == 1);
    }

    SECTION("Removing a renderable that was never added does nothing") {
        callback_scene_renderable stranger{[](render_context&) {}};

        REQUIRE_NOTHROW(surface.remove_renderable(stranger));
        renderOnce();
    }

    SECTION("A callback renderable with no callback draws nothing") {
        callback_scene_renderable empty{nullptr};
        surface.add_renderable(empty);

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                REQUIRE_NOTHROW(surface.render());
            });
        });
    }

    SECTION("A renderable that throws leaves ImGui balanced and the exception reaches the caller") {
        const scoped_scene_drawing drawing{surface, [](scene_painter&) {
                                               throw std::runtime_error{"scene failed"};
                                           }};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                const imgui_state_snapshot before{imgui_state_snapshot::take()};

                REQUIRE_THROWS_AS(surface.render(), std::runtime_error);

                require_same_state(imgui_state_snapshot::take(), before);
            });
        });
    }
}

// ================================ Drawing through the painter ================================

TEST_CASE("scene_painter maps world coordinates through the camera",
          "[tca][engine][rendering][scene][scene_surface][drawing]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    scene_surface surface{
        "##surface", vector2{400.0, 300.0}
    };
    surface.set_background_role(std::nullopt);

    // Parenthesised rather than braced: GENERATE would split a braced vector2{x, y} at the comma.
    const vector2 cameraPosition = GENERATE(vector2::zero(), vector2(25.0, -40.0));
    const double zoom = GENERATE(1.0, 2.0, 0.5);
    CAPTURE(cameraPosition.x(), cameraPosition.y(), zoom);

    surface.camera().set_position(cameraPosition);
    surface.camera().set_zoom(zoom);

    SECTION("fill_rect() covers exactly the screen rectangle of its world rectangle") {
        const world_rect worldRect{
            Eigen::Vector2d{-10.0, -20.0},
            Eigen::Vector2d{30.0,  40.0 }
        };
        std::vector<ImDrawVert> drawn{};
        screen_rect bounds{};

        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               bounds = painter.bounds();
                                               drawn = vertices_drawn_by([&] {
                                                   painter.fill_rect(worldRect, PROBE_COLOR);
                                               });
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE_FALSE(drawn.empty());
        REQUIRE(vertices_with_color(drawn, PROBE_COLOR).size() == drawn.size());

        const camera_2d& camera{surface.camera()};
        const screen_rect expected{expected_screen_point(bounds, camera, vector2{worldRect.min()}),
                                   expected_screen_point(bounds, camera, vector2{worldRect.max()})};
        require_same_rect(bounding_box(drawn), expected);
    }

    SECTION("fill_rect() with a colour role paints the active theme's colour for that role") {
        const theming::scoped_active_theme activeTheme{theme_with(color_role::accent, PROBE_COLOR)};
        const world_rect worldRect{
            Eigen::Vector2d{0.0,  0.0 },
            Eigen::Vector2d{10.0, 10.0}
        };
        std::vector<ImDrawVert> drawn{};

        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               drawn = vertices_drawn_by([&] {
                                                   painter.fill_rect(worldRect, color_role::accent);
                                               });
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE_FALSE(drawn.empty());
        REQUIRE(vertices_with_color(drawn, PROBE_COLOR).size() == drawn.size());
    }

    SECTION("draw_line() runs between the screen points of its world endpoints") {
        const vector2 from{-50.0, 10.0};
        const vector2 to{60.0, -30.0};
        constexpr double THICKNESS{2.0};
        std::vector<ImDrawVert> drawn{};
        screen_rect bounds{};

        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               bounds = painter.bounds();
                                               drawn = vertices_drawn_by([&] {
                                                   painter.draw_line(from, to, PROBE_COLOR,
                                                                     THICKNESS);
                                               });
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE_FALSE(drawn.empty());

        // Anti-aliasing adds a fringe of about a pixel around the line's own thickness.
        const camera_2d& camera{surface.camera()};
        screen_rect expected{};
        expected.extend(expected_screen_point(bounds, camera, from));
        expected.extend(expected_screen_point(bounds, camera, to));
        const double slack{THICKNESS / 2.0 + 1.5};
        require_same_rect(bounding_box(drawn), expected, slack);
    }

    SECTION("draw_line() thickness is in pixels, whatever the zoom") {
        // A horizontal line: its vertical extent is its thickness (plus the AA fringe), which the
        // zoom must not scale.
        constexpr double THICKNESS{3.0};
        std::vector<ImDrawVert> drawn{};

        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               drawn = vertices_drawn_by([&] {
                                                   painter.draw_line(vector2{-20.0, 0.0},
                                                                     vector2{20.0, 0.0},
                                                                     PROBE_COLOR, THICKNESS);
                                               });
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE_FALSE(drawn.empty());
        const double height{bounding_box(drawn).sizes().y()};
        REQUIRE(height >= THICKNESS - PIXEL_EPSILON);
        REQUIRE(height <= THICKNESS + 2.0 + PIXEL_EPSILON);
    }

    SECTION("fill_circle() is centred on its world centre with a radius in world units") {
        const vector2 centre{12.0, -8.0};
        constexpr double RADIUS{20.0};
        std::vector<ImDrawVert> drawn{};
        screen_rect bounds{};

        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               bounds = painter.bounds();
                                               drawn = vertices_drawn_by([&] {
                                                   painter.fill_circle(centre, RADIUS, PROBE_COLOR);
                                               });
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE_FALSE(drawn.empty());

        const Eigen::Vector2d screenCentre{expected_screen_point(bounds, surface.camera(), centre)};
        const double screenRadius{RADIUS * zoom};
        const screen_rect expected{screenCentre - Eigen::Vector2d::Constant(screenRadius),
                                   screenCentre + Eigen::Vector2d::Constant(screenRadius)};
        // Tessellation and the AA fringe leave the outline within about a pixel of the circle.
        require_same_rect(bounding_box(drawn), expected, 1.5);
    }

    SECTION("Drawing is in the order the renderable asks for it") {
        std::vector<ImDrawVert> drawn{};

        const scoped_scene_drawing drawing{
            surface, [&](scene_painter& painter) {
                drawn = vertices_drawn_by([&] {
                    painter.fill_rect(
                        world_rect{
                            Eigen::Vector2d{0.0, 0.0},
                            Eigen::Vector2d{5.0, 5.0}
                    },
                        PROBE_COLOR);
                    painter.fill_rect(
                        world_rect{
                            Eigen::Vector2d{0.0, 0.0},
                            Eigen::Vector2d{5.0, 5.0}
                    },
                        OTHER_PROBE_COLOR);
                });
            }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE(drawn.size() >= 2);
        REQUIRE(drawn.front().col == to_imu32(PROBE_COLOR));
        REQUIRE(drawn.back().col == to_imu32(OTHER_PROBE_COLOR));
    }
}

// ======================================== Clipping ========================================

TEST_CASE("scene_surface clips drawing to its bounds",
          "[tca][engine][rendering][scene][scene_surface][clipping]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    SECTION("Inside a renderable the clip rectangle is the surface's bounds") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        require_same_rect(observation.clipRect, surface.bounds());
    }

    SECTION("Geometry reaching past the bounds is still clipped to them") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        screen_rect clipWhileDrawing{};
        std::vector<ImDrawVert> drawn{};

        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               // Far larger than the surface on every side.
                                               drawn = vertices_drawn_by([&] {
                                                   painter.fill_rect(
                                                       world_rect{
                                                           Eigen::Vector2d{-10'000.0, -10'000.0},
                                                           Eigen::Vector2d{10'000.0,  10'000.0 }
                                                   },
                                                       PROBE_COLOR);
                                               });
                                               clipWhileDrawing = current_clip_rect();
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        // The vertices themselves overflow - clipping is ImGui's clip rectangle, not geometry -
        // but the rectangle they are drawn under never reaches the surrounding page.
        REQUIRE_FALSE(contains(surface.bounds(), bounding_box(drawn)));
        REQUIRE(contains(surface.bounds(), clipWhileDrawing));
    }

    SECTION("Every draw command the scene issued is clipped within the surface") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        surface.set_grid(grid_options{.spacing = 16.0});
        ImDrawList* sceneDrawList{};
        // Where the page's indices end before the surface renders. Read outside the renderable: the
        // scene may draw into a channel of the draw list, whose command buffer only becomes the
        // list's own once the channels are merged back after the scene.
        int firstIndex{};

        const scoped_scene_drawing drawing{
            surface, [&](scene_painter& painter) {
                sceneDrawList = &current_draw_list();
                painter.fill_rect(
                    world_rect{
                        Eigen::Vector2d{-1'000.0, -1'000.0},
                        Eigen::Vector2d{1'000.0,  1'000.0 }
                },
                    PROBE_COLOR);
                painter.draw_line(vector2{-1'000.0, 0.0}, vector2{1'000.0, 0.0}, PROBE_COLOR, 4.0);
                painter.fill_circle(vector2{150.0, 0.0}, 100.0, PROBE_COLOR);
            }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                firstIndex = current_draw_list().IdxBuffer.Size;
                surface.render();

                REQUIRE(sceneDrawList != nullptr);
                for (int i{0}; i < sceneDrawList->CmdBuffer.Size; ++i) {
                    const ImDrawCmd& command{sceneDrawList->CmdBuffer[i]};
                    // Only the commands drawing indices the surface added.
                    if (command.ElemCount == 0 ||
                        static_cast<int>(command.IdxOffset + command.ElemCount) <= firstIndex) {
                        continue;
                    }
                    CAPTURE(i);
                    const screen_rect commandClip{
                        Eigen::Vector2d{command.ClipRect.x, command.ClipRect.y},
                        Eigen::Vector2d{command.ClipRect.z, command.ClipRect.w}
                    };
                    REQUIRE(contains(surface.bounds(), commandClip));
                }
            });
        });
    }

    SECTION("A surface partly outside its page is clipped to the page as well") {
        // 600x500 surface in a 300x250 page: only the page's visible part may be drawn to.
        scene_surface surface{
            "##surface", vector2{600.0, 500.0}
        };
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};
        screen_rect pageClip{};

        run_frame(imguiContext, [&] {
            render_in_host(
                [&] {
                    pageClip = current_clip_rect();
                    surface.render();
                },
                ImVec2{300.0f, 250.0f});
        });

        REQUIRE(observation.calls <= 1);
        if (observation.calls == 1) {
            REQUIRE(contains(pageClip, observation.clipRect));
            REQUIRE(contains(surface.bounds(), observation.clipRect));
        }
        // Whatever part of it is visible, the camera still describes the whole surface.
        require_near(surface.camera().viewport_size(), Eigen::Vector2d{600.0, 500.0});
    }

    SECTION("After rendering, the page's clip rectangle and ImGui stacks are as they were") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        surface.set_grid(grid_options{.spacing = 32.0});
        const scoped_scene_drawing drawing{
            surface, [](scene_painter& painter) {
                painter.fill_rect(
                    world_rect{
                        Eigen::Vector2d{-50.0, -50.0},
                        Eigen::Vector2d{50.0,  50.0 }
                },
                    PROBE_COLOR);
            }};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                const imgui_state_snapshot before{imgui_state_snapshot::take()};
                surface.render();
                require_same_state(imgui_state_snapshot::take(), before);
            });
        });
    }

    SECTION("Drawing into the page after the surface is not clipped by it") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        screen_rect pageClipAfter{};
        screen_rect pageClipBefore{};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                pageClipBefore = current_clip_rect();
                surface.render();
                pageClipAfter = current_clip_rect();
            });
        });

        require_same_rect(pageClipAfter, pageClipBefore);
        REQUIRE_FALSE(contains(surface.bounds(), pageClipAfter));
    }
}

// ================================== UI scale and DPI ==================================

TEST_CASE("scene_surface under a non-1.0 UI scale",
          "[tca][engine][rendering][scene][scene_surface][dpi]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    const float uiScale = GENERATE(1.25f, 1.5f, 2.0f, 0.75f);
    CAPTURE(uiScale);

    // The engine carries the per-monitor DPI scale in the ImGui style (see
    // application/win32/imgui-win32-utilities.cpp): that is the UI scale the surface must honour.
    ImGui::GetStyle().FontScaleDpi = uiScale;

    SECTION("A fixed size is in unscaled pixels, like every other widget dimension") {
        scene_surface surface{
            "##surface", vector2{200.0, 100.0}
        };
        host_layout layout{};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                layout = host_layout::take();
                surface.render();
            });
        });

        const Eigen::Vector2d scaledSize{200.0 * uiScale, 100.0 * uiScale};
        require_same_rect(surface.bounds(), rect_from(layout.cursor, scaledSize));
    }

    SECTION("The camera viewport is the laid-out size in physical pixels") {
        scene_surface surface{
            "##surface", vector2{200.0, 100.0}
        };

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        require_near(surface.camera().viewport_size(),
                     Eigen::Vector2d{200.0 * uiScale, 100.0 * uiScale});
    }

    SECTION("A filling surface takes the available region as is, without scaling it again") {
        scene_surface surface{"##surface"};
        host_layout layout{};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                layout = host_layout::take();
                surface.render();
            });
        });

        require_same_rect(surface.bounds(), rect_from(layout.cursor, layout.available));
    }

    SECTION("The UI scale does not leak into the camera: its zoom is still the game's") {
        scene_surface surface{
            "##surface", vector2{200.0, 100.0}
        };
        surface.camera().set_zoom(3.0);

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE(surface.camera().zoom() == 3.0);
    }

    SECTION("World geometry maps to exact physical pixels: one unit at zoom 1 is one pixel") {
        scene_surface surface{
            "##surface", vector2{200.0, 100.0}
        };
        surface.set_background_role(std::nullopt);
        std::vector<ImDrawVert> drawn{};

        const scoped_scene_drawing drawing{
            surface, [&](scene_painter& painter) {
                drawn = vertices_drawn_by([&] {
                    painter.fill_rect(
                        world_rect{
                            Eigen::Vector2d{0.0,  0.0 },
                            Eigen::Vector2d{10.0, 10.0}
                    },
                        PROBE_COLOR);
                });
            }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        require_near(Eigen::Vector2d{bounding_box(drawn).sizes()}, Eigen::Vector2d{10.0, 10.0});
    }

    SECTION("Drawing is still clipped to the scaled bounds") {
        scene_surface surface{
            "##surface", vector2{200.0, 100.0}
        };
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        require_same_rect(observation.clipRect, surface.bounds());
    }

    SECTION("A UI scale change between frames re-lays the surface out and resyncs the camera") {
        scene_surface surface{
            "##surface", vector2{200.0, 100.0}
        };

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });
        ImGui::GetStyle().FontScaleDpi = 1.0f;
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        require_near(Eigen::Vector2d{surface.bounds().sizes()}, Eigen::Vector2d{200.0, 100.0});
        require_near(surface.camera().viewport_size(), Eigen::Vector2d{200.0, 100.0});
    }

    SECTION("A full-window surface covers the client area exactly, not a scaled version of it") {
        scene_surface surface{"##surface", fill_window};

        run_frame(imguiContext, [&] {
            surface.render();
        });

        require_same_rect(surface.bounds(),
                          rect_from(Eigen::Vector2d::Zero(), to_eigen(DISPLAY_SIZE)));
        require_near(surface.camera().viewport_size(), to_eigen(DISPLAY_SIZE));
    }
}

// ===================================== Full window =====================================

TEST_CASE("scene_surface in full-window mode",
          "[tca][engine][rendering][scene][scene_surface][full_window]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    // Deliberately non-zero, so a surface that forgets to cancel them leaves a visible gap.
    ImGuiStyle& style{ImGui::GetStyle()};
    style.WindowPadding = ImVec2{12.0f, 9.0f};
    style.WindowBorderSize = 2.0f;
    style.WindowRounding = 6.0f;
    style.ScrollbarSize = 16.0f;

    auto mainViewportRect{[] {
        const ImGuiViewport& viewport{*ImGui::GetMainViewport()};
        return rect_from(to_eigen(viewport.Pos), to_eigen(viewport.Size));
    }};

    SECTION("It covers the main viewport edge to edge") {
        scene_surface surface{"##surface", fill_window};
        screen_rect viewportRect{};

        run_frame(imguiContext, [&] {
            viewportRect = mainViewportRect();
            surface.render();
        });

        require_same_rect(surface.bounds(), viewportRect);
        require_same_rect(surface.bounds(),
                          rect_from(Eigen::Vector2d::Zero(), to_eigen(DISPLAY_SIZE)));
    }

    SECTION("Its camera viewport is the whole client area") {
        scene_surface surface{"##surface", fill_window};

        run_frame(imguiContext, [&] {
            surface.render();
        });

        require_near(surface.camera().viewport_size(), to_eigen(DISPLAY_SIZE));
    }

    SECTION("It clips to the whole client area: no padding, border or title bar eats into it") {
        scene_surface surface{"##surface", fill_window};
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            surface.render();
        });

        REQUIRE(observation.calls == 1);
        require_same_rect(observation.clipRect,
                          rect_from(Eigen::Vector2d::Zero(), to_eigen(DISPLAY_SIZE)));
    }

    SECTION("It paints no window background or decoration of its own") {
        scene_surface surface{"##surface", fill_window};
        surface.set_background_role(std::nullopt);
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            surface.render();
        });

        // With no background role and no grid, nothing may be drawn before the scene: an ImGui
        // window background, border, title bar or scrollbar would all show up here.
        REQUIRE(observation.calls == 1);
        REQUIRE(observation.verticesBeforeScene.empty());
    }

    SECTION("Content larger than the window never brings up a scrollbar") {
        scene_surface surface{"##surface", fill_window};
        surface.set_background_role(std::nullopt);
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface,
                                           observing(observation, [](scene_painter& painter) {
                                               painter.fill_rect(
                                                   world_rect{
                                                       Eigen::Vector2d{-100'000.0, -100'000.0},
                                                       Eigen::Vector2d{100'000.0,  100'000.0 }
                                               },
                                                   PROBE_COLOR);
                                           })};

        // Several frames: ImGui only decides on scrollbars from the previous frame's content.
        for (int frame{0}; frame < 4; ++frame) {
            run_frame(imguiContext, [&] {
                surface.render();
            });
        }

        require_same_rect(surface.bounds(),
                          rect_from(Eigen::Vector2d::Zero(), to_eigen(DISPLAY_SIZE)));
        require_same_rect(observation.clipRect,
                          rect_from(Eigen::Vector2d::Zero(), to_eigen(DISPLAY_SIZE)));
        REQUIRE(observation.verticesBeforeScene.empty());
    }

    SECTION("It follows window resizes and fullscreen toggles, frame by frame") {
        scene_surface surface{"##surface", fill_window};
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        // Windowed, then "fullscreen" on a bigger display, then a small window, then back.
        const std::vector<ImVec2> sizes{
            ImVec2{1280.0f, 720.0f },
            ImVec2{2560.0f, 1440.0f},
            ImVec2{800.0f,  600.0f },
            ImVec2{1281.0f, 719.0f },
            ImVec2{1280.0f, 720.0f },
        };

        for (const ImVec2& size : sizes) {
            CAPTURE(size.x, size.y);
            imguiContext.set_display_size(size);

            run_frame(imguiContext, [&] {
                surface.render();
            });

            const screen_rect expected{rect_from(Eigen::Vector2d::Zero(), to_eigen(size))};
            require_same_rect(surface.bounds(), expected);
            require_same_rect(observation.clipRect, expected);
            require_near(surface.camera().viewport_size(), to_eigen(size));
            require_near(observation.viewportSize, to_eigen(size));
        }
    }

    SECTION("A resize keeps the camera's position at the centre of the new client area") {
        scene_surface surface{"##surface", fill_window};
        surface.camera().set_position(vector2{100.0, 50.0});
        surface.camera().set_zoom(1.5);

        run_frame(imguiContext, [&] {
            surface.render();
        });
        imguiContext.set_display_size(ImVec2{1920.0f, 1080.0f});
        run_frame(imguiContext, [&] {
            surface.render();
        });

        REQUIRE(surface.camera().position() == vector2{100.0, 50.0});
        REQUIRE(surface.camera().zoom() == 1.5);
        require_near(surface.camera().world_to_screen(surface.camera().position()),
                     Eigen::Vector2d{960.0, 540.0});
    }

    SECTION("It still covers the whole window when rendered from inside a page") {
        scene_surface surface{"##surface", fill_window};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        require_same_rect(surface.bounds(),
                          rect_from(Eigen::Vector2d::Zero(), to_eigen(DISPLAY_SIZE)));
    }

    SECTION("It leaves the ImGui stacks as it found them") {
        scene_surface surface{"##surface", fill_window};
        surface.set_grid(grid_options{.spacing = 64.0});

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                const imgui_state_snapshot before{imgui_state_snapshot::take()};
                surface.render();
                require_same_state(imgui_state_snapshot::take(), before);
            });
        });
    }

    SECTION("It does not change the style for whatever is rendered after it") {
        scene_surface surface{"##surface", fill_window};

        run_frame(imguiContext, [&] {
            surface.render();
        });

        REQUIRE(style.WindowPadding.x == 12.0f);
        REQUIRE(style.WindowPadding.y == 9.0f);
        REQUIRE(style.WindowBorderSize == 2.0f);
        REQUIRE(style.WindowRounding == 6.0f);
    }
}

// ======================================= Theming =======================================

TEST_CASE("scene_surface resolves its colours from the active theme",
          "[tca][engine][rendering][scene][scene_surface][theming]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    SECTION("The background is painted with the default role's colour, covering the bounds") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::surface, BACKGROUND_PROBE_COLOR)};
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        const std::vector<ImDrawVert> background{
            vertices_with_color(observation.verticesBeforeScene, BACKGROUND_PROBE_COLOR)};
        REQUIRE_FALSE(background.empty());
        require_same_rect(bounding_box(background), surface.bounds());
    }

    SECTION("Another background role can be chosen") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::window_background, BACKGROUND_PROBE_COLOR)};
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        surface.set_background_role(color_role::window_background);
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE(surface.background_role() ==
                std::optional<color_role>{color_role::window_background});
        REQUIRE_FALSE(
            vertices_with_color(observation.verticesBeforeScene, BACKGROUND_PROBE_COLOR).empty());
    }

    SECTION("No background role paints no background") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::surface, BACKGROUND_PROBE_COLOR)};
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        surface.set_background_role(std::nullopt);
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE(
            vertices_with_color(observation.verticesBeforeScene, BACKGROUND_PROBE_COLOR).empty());
    }

    SECTION("A full-window surface's background covers the whole client area") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::surface, BACKGROUND_PROBE_COLOR)};
        scene_surface surface{"##surface", fill_window};
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            surface.render();
        });

        const std::vector<ImDrawVert> background{
            vertices_with_color(observation.verticesBeforeScene, BACKGROUND_PROBE_COLOR)};
        REQUIRE_FALSE(background.empty());
        require_same_rect(bounding_box(background),
                          rect_from(Eigen::Vector2d::Zero(), to_eigen(DISPLAY_SIZE)));
    }

    SECTION("The background follows a theme swapped after the surface was built") {
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });
        REQUIRE(
            vertices_with_color(observation.verticesBeforeScene, BACKGROUND_PROBE_COLOR).empty());

        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::surface, BACKGROUND_PROBE_COLOR)};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE_FALSE(
            vertices_with_color(observation.verticesBeforeScene, BACKGROUND_PROBE_COLOR).empty());
    }

    SECTION("The grid is drawn with its role's colour, under the scene") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::outline_variant, GRID_PROBE_COLOR)};
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        surface.set_grid(grid_options{.spacing = 50.0});
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        // Drawn before the renderables run, so everything the scene draws lands on top of it.
        REQUIRE_FALSE(
            vertices_with_color(observation.verticesBeforeScene, GRID_PROBE_COLOR).empty());
    }

    SECTION("The grid is drawn above the background") {
        const theming::scoped_active_theme activeTheme{[] {
            theming::theme customTheme{};
            customTheme.set(color_role::surface, BACKGROUND_PROBE_COLOR);
            customTheme.set(color_role::outline_variant, GRID_PROBE_COLOR);
            return customTheme;
        }()};
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        surface.set_grid(grid_options{.spacing = 50.0});
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        std::optional<std::size_t> lastBackgroundVertex{};
        std::optional<std::size_t> firstGridVertex{};
        for (std::size_t i{0}; i < observation.verticesBeforeScene.size(); ++i) {
            const ImU32 vertexColor{observation.verticesBeforeScene[i].col};
            if (vertexColor == to_imu32(BACKGROUND_PROBE_COLOR)) {
                lastBackgroundVertex = i;
            }
            if (vertexColor == to_imu32(GRID_PROBE_COLOR) && !firstGridVertex) {
                firstGridVertex = i;
            }
        }

        REQUIRE(lastBackgroundVertex.has_value());
        REQUIRE(firstGridVertex.has_value());
        REQUIRE(*lastBackgroundVertex < *firstGridVertex);
    }

    SECTION("The grid can use a role of the game's choosing") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::accent, GRID_PROBE_COLOR)};
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        surface.set_grid(grid_options{.spacing = 50.0, .role = color_role::accent});
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE(surface.grid()->role == color_role::accent);
        REQUIRE_FALSE(
            vertices_with_color(observation.verticesBeforeScene, GRID_PROBE_COLOR).empty());
    }

    SECTION("Removing the grid stops drawing it") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::outline_variant, GRID_PROBE_COLOR)};
        scene_surface surface{
            "##surface", vector2{320.0, 200.0}
        };
        surface.set_grid(grid_options{.spacing = 50.0});
        surface.set_grid(std::nullopt);
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });

        REQUIRE_FALSE(surface.grid().has_value());
        REQUIRE(vertices_with_color(observation.verticesBeforeScene, GRID_PROBE_COLOR).empty());
    }

    SECTION("Grid lines stay within the surface and follow the camera") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::outline_variant, GRID_PROBE_COLOR)};
        scene_surface surface{
            "##surface", vector2{400.0, 300.0}
        };
        surface.set_background_role(std::nullopt);
        surface.set_grid(grid_options{.spacing = 100.0});
        callback_observation observation{};
        const scoped_scene_drawing drawing{surface, observing(observation)};

        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });
        const std::vector<ImDrawVert> centred{
            vertices_with_color(observation.verticesBeforeScene, GRID_PROBE_COLOR)};

        // Panning by half a cell moves every line by half a cell on screen.
        surface.camera().pan(vector2{50.0, 0.0});
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });
        const std::vector<ImDrawVert> panned{
            vertices_with_color(observation.verticesBeforeScene, GRID_PROBE_COLOR)};

        REQUIRE_FALSE(centred.empty());
        REQUIRE_FALSE(panned.empty());

        // Lines are drawn across the surface, so their extent matches it (give or take the line
        // thickness and anti-aliasing fringe), whether the camera moved or not.
        require_same_rect(bounding_box(centred), surface.bounds(), 2.5);
        require_same_rect(bounding_box(panned), surface.bounds(), 2.5);

        // The unpanned 400x300 camera has a vertical line exactly through the surface's centre;
        // after the half-cell pan it has none there.
        const double centreX{surface.bounds().center().x()};
        auto hasVerticalLineAt{[](const std::vector<ImDrawVert>& vertices, const double x) {
            std::size_t near{0};
            for (const ImDrawVert& vertex : vertices) {
                if (std::abs(static_cast<double>(vertex.pos.x) - x) <= 2.0) {
                    ++near;
                }
            }
            // A vertical line spans the surface height, so it puts vertices at both its ends;
            // horizontal lines cross it too, but only contribute vertices at their own ends.
            return near >= 4;
        }};
        REQUIRE(hasVerticalLineAt(centred, centreX));
        REQUIRE_FALSE(hasVerticalLineAt(panned, centreX));
    }
}

TEST_CASE("scene_painter fills rectangles with an alpha, for flashes and fades",
          "[tca][engine][rendering][scene][scene_surface][drawing][alpha]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    scene_surface surface{
        "##surface", vector2{400.0, 300.0}
    };
    surface.set_background_role(std::nullopt);

    const world_rect worldRect{
        Eigen::Vector2d{-10.0, -20.0},
        Eigen::Vector2d{30.0,  40.0 }
    };

    // Runs one frame, painting with paint, and returns what it drew and where the surface was.
    auto paintFrame{[&](const std::function<void(scene_painter&)>& paint) {
        std::vector<ImDrawVert> drawn{};
        screen_rect bounds{};
        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               bounds = painter.bounds();
                                               drawn = vertices_drawn_by([&] {
                                                   paint(painter);
                                               });
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                surface.render();
            });
        });
        return std::pair{drawn, bounds};
    }};

    SECTION("An alpha multiplies the colour's own alpha, leaving its colour and its rectangle") {
        // 200 * 0.5 = 100.
        const auto [drawn, bounds]{paintFrame([&](scene_painter& painter) {
            painter.fill_rect(worldRect, tca::engine::rendering::rgba(11, 22, 33, 200), 0.5);
        })};

        REQUIRE_FALSE(drawn.empty());
        REQUIRE(vertices_with_color(drawn, tca::engine::rendering::rgba(11, 22, 33, 100)).size() ==
                drawn.size());

        const camera_2d& camera{surface.camera()};
        require_same_rect(
            bounding_box(drawn),
            screen_rect{expected_screen_point(bounds, camera, vector2{worldRect.min()}),
                        expected_screen_point(bounds, camera, vector2{worldRect.max()})});
    }

    SECTION("Alpha is rounded to the nearest 8-bit value") {
        // 255 * 0.25 = 63.75, rounded to 64.
        const auto [drawn, bounds]{paintFrame([&](scene_painter& painter) {
            painter.fill_rect(worldRect, PROBE_COLOR, 0.25);
        })};

        REQUIRE_FALSE(drawn.empty());
        REQUIRE(vertices_with_color(drawn, tca::engine::rendering::rgba(11, 22, 33, 64)).size() ==
                drawn.size());
    }

    SECTION("Without an alpha, the colour is drawn as it is") {
        const auto [drawn, bounds]{paintFrame([&](scene_painter& painter) {
            painter.fill_rect(worldRect, PROBE_COLOR);
        })};

        REQUIRE_FALSE(drawn.empty());
        REQUIRE(vertices_with_color(drawn, PROBE_COLOR).size() == drawn.size());
    }

    SECTION("A colour role takes an alpha too, multiplied into the theme's colour") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::accent, tca::engine::rendering::rgba(40, 80, 160, 128))};

        const auto [drawn, bounds]{paintFrame([&](scene_painter& painter) {
            painter.fill_rect(worldRect, color_role::accent, 0.5);
        })};

        REQUIRE_FALSE(drawn.empty());
        REQUIRE(vertices_with_color(drawn, tca::engine::rendering::rgba(40, 80, 160, 64)).size() ==
                drawn.size());
    }

    SECTION("Alpha 0 draws nothing visible") {
        const auto [drawn, bounds]{paintFrame([&](scene_painter& painter) {
            painter.fill_rect(worldRect, PROBE_COLOR, 0.0);
            painter.fill_rect(worldRect, color_role::accent, 0.0);
        })};

        // Drawing no geometry at all is just as invisible as drawing it fully transparent.
        REQUIRE(std::ranges::all_of(drawn, [](const ImDrawVert& vertex) {
            return ((vertex.col >> IM_COL32_A_SHIFT) & 0xFFu) == 0u;
        }));
    }

    SECTION("A screen flash over the visible world covers exactly the surface") {
        const double zoom = GENERATE(1.0, 0.5, 1.37);
        CAPTURE(zoom);
        surface.camera().set_position(vector2{25.0, -40.0});
        surface.camera().set_zoom(zoom);

        const auto [drawn, bounds]{paintFrame([&](scene_painter& painter) {
            painter.fill_rect(painter.camera().visible_world_rect(), OTHER_PROBE_COLOR, 0.25);
        })};

        REQUIRE_FALSE(drawn.empty());
        require_same_rect(bounding_box(drawn), bounds);
    }
}
