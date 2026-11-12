// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/scene-surface.hpp"
#include "rendering/scene/sprite.hpp"
#include "rendering/scene/virtual-resolution.hpp"

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
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
#include "rendering/colors.hpp"
#include "rendering/scene/camera-2d.hpp"
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
using tca::engine::scene::camera_2d;
using tca::engine::scene::compute_pixel_perfect_layout;
using tca::engine::scene::fill_window;
using tca::engine::scene::grid_options;
using tca::engine::scene::pixel_perfect_layout;
using tca::engine::scene::scene_painter;
using tca::engine::scene::scene_surface;
using tca::engine::scene::screen_rect;
using tca::engine::scene::sprite_options;
using tca::engine::scene::sprite_texture;
using tca::engine::scene::texture_id;
using tca::engine::scene::virtual_size;
using tca::engine::scene::world_rect;
using tca::engine::testing::headless_imgui_context;
using tca::engine::theming::color_role;
using tca::tests::utilities::scoped_scene_drawing;

using Catch::Matchers::WithinAbs;

namespace theming = tca::engine::theming;

// ImGui lays out in floats and the surface reports doubles, so positions only agree to float
// precision.
constexpr double PIXEL_EPSILON{1e-3};

// UVs go through a float too.
constexpr double UV_EPSILON{1e-6};

constexpr ImVec2 DISPLAY_SIZE{1280.0f, 720.0f};

constexpr ImVec2 HOST_POSITION{40.0f, 30.0f};
constexpr ImVec2 HOST_SIZE{800.0f, 600.0f};
constexpr ImGuiWindowFlags HOST_FLAGS{
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse};

// A small virtual resolution, so a surface at every scale from 1 to 6 still fits in the host page.
constexpr virtual_size VIRTUAL{.width = 64, .height = 36};
const vector2 VIRTUAL_EXTENT{64.0, 36.0};

// The game-sized virtual resolution of the full-window tests.
constexpr virtual_size VIRTUAL_320x180{.width = 320, .height = 180};

// Extra pixels a page surface has past scale times VIRTUAL, so it is letterboxed on every side:
// 13 across splits 6 | 7, 7 down splits 3 | 4.
const Eigen::Vector2d LEFTOVER{13.0, 7.0};
const Eigen::Vector2d CONTENT_OFFSET{6.0, 3.0};

// Identifiers no real backend hands out in a headless test, so a draw command carrying one can
// only come from a sprite the test drew.
constexpr texture_id PROBE_TEXTURE_ID{0x5A17E};
constexpr texture_id CHECKER_TEXTURE_ID{0xC4EC};

constexpr sprite_texture PROBE_TEXTURE{.id = PROBE_TEXTURE_ID, .width = 8, .height = 12};

// A 1-pixel checkerboard exactly the size of the virtual screen.
constexpr sprite_texture CHECKER_TEXTURE{.id = CHECKER_TEXTURE_ID,
                                         .width = VIRTUAL.width,
                                         .height = VIRTUAL.height};

// Colours no default role uses, so a vertex carrying one can only come from what the test asked
// for.
constexpr color PROBE_COLOR{tca::engine::rendering::rgba(11, 22, 33, 255)};
constexpr color BACKGROUND_PROBE_COLOR{tca::engine::rendering::rgba(201, 13, 77, 255)};
constexpr color LETTERBOX_PROBE_COLOR{tca::engine::rendering::rgba(7, 140, 66, 255)};
constexpr color GRID_PROBE_COLOR{tca::engine::rendering::rgba(3, 250, 9, 255)};
constexpr color AFTER_SURFACE_PROBE_COLOR{tca::engine::rendering::rgba(99, 1, 180, 255)};

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

void require_same_rect(const Eigen::AlignedBox2d& actual, const Eigen::AlignedBox2d& expected,
                       const double epsilon = PIXEL_EPSILON) {
    require_near(actual.min(), expected.min(), epsilon);
    require_near(actual.max(), expected.max(), epsilon);
}

Eigen::Vector2d to_eigen(const ImVec2 value) {
    return Eigen::Vector2d{static_cast<double>(value.x), static_cast<double>(value.y)};
}

ImU32 to_imu32(const color& value) {
    return IM_COL32(value.r, value.g, value.b, value.a);
}

world_rect world_box(const double minX, const double minY, const double maxX, const double maxY) {
    return world_rect{
        Eigen::Vector2d{minX, minY},
        Eigen::Vector2d{maxX, maxY}
    };
}

screen_rect screen_box(const Eigen::Vector2d& min, const Eigen::Vector2d& max) {
    return screen_rect{min, max};
}

// A theme where the background, the letterbox and the grid each have a colour of their own.
theming::theme probe_theme() {
    theming::theme customTheme{};
    customTheme.set(color_role::surface, BACKGROUND_PROBE_COLOR);
    customTheme.set(color_role::window_background, LETTERBOX_PROBE_COLOR);
    customTheme.set(color_role::outline_variant, GRID_PROBE_COLOR);
    return customTheme;
}

// The texture a draw command samples. Read from the reference itself rather than through
// ImDrawCmd::GetTexID(), which asserts on the font atlas's texture when no backend has uploaded it,
// as is the case headlessly: every command that is not a sprite reads as ImTextureID_Invalid.
ImTextureID command_texture(const ImDrawCmd& command) {
    return command.TexRef._TexData != nullptr ? ImTextureID_Invalid : command.TexRef._TexID;
}

// The texture filter the backend samples a vertex with, as last set by a sampler callback in the
// draw list. Linear until told otherwise, which is what every backend starts a frame with.
enum class active_sampler {
    linear,
    nearest
};

// One vertex of what was drawn, in draw order, with the command it was drawn by.
struct drawn_vertex {
    ImDrawVert vertex{};
    ImTextureID texture{ImTextureID_Invalid};
    screen_rect clipRect{};
    int command{};
    active_sampler sampler{active_sampler::linear};
};

bool sampled_nearest(const drawn_vertex& drawn) {
    return drawn.sampler == active_sampler::nearest;
}

bool sampled_linear(const drawn_vertex& drawn) {
    return drawn.sampler == active_sampler::linear;
}

screen_rect bounding_box(const std::vector<drawn_vertex>& vertices) {
    screen_rect box{};
    for (const drawn_vertex& drawn : vertices) {
        box.extend(to_eigen(drawn.vertex.pos));
    }
    return box;
}

// Rectangles drawn with AddRectFilled(), one per six indexed vertices: the bounding box of each.
std::vector<screen_rect> filled_rects(const std::vector<drawn_vertex>& vertices) {
    REQUIRE(vertices.size() % 6 == 0);
    std::vector<screen_rect> rects{};
    for (std::size_t first{0}; first < vertices.size(); first += 6) {
        rects.push_back(bounding_box(
            std::vector<drawn_vertex>(vertices.begin() + static_cast<std::ptrdiff_t>(first),
                                      vertices.begin() + static_cast<std::ptrdiff_t>(first + 6))));
    }
    return rects;
}

// Two lists of rectangles hold the same ones, in any order.
void require_same_rects(std::vector<screen_rect> actual, std::vector<screen_rect> expected) {
    REQUIRE(actual.size() == expected.size());
    for (const screen_rect& rect : actual) {
        CAPTURE(rect.min().x(), rect.min().y(), rect.max().x(), rect.max().y());
        const auto found{std::ranges::find_if(expected, [&rect](const screen_rect& candidate) {
            return (candidate.min() - rect.min()).cwiseAbs().maxCoeff() < PIXEL_EPSILON &&
                   (candidate.max() - rect.max()).cwiseAbs().maxCoeff() < PIXEL_EPSILON;
        })};
        REQUIRE(found != expected.end());
        expected.erase(found);
    }
}

// Everything one render of a surface drew, in the order the GPU will draw it.
struct scene_capture {
    std::vector<drawn_vertex> vertices{};
    std::vector<drawn_vertex> afterSurface{};
    screen_rect bounds{};
    screen_rect painterBounds{};
    screen_rect sceneClipRect{};
    vector2 viewportSize{};
    std::optional<pixel_perfect_layout> layout{};

    std::vector<drawn_vertex> with_texture(const texture_id texture) const {
        std::vector<drawn_vertex> matching{};
        std::ranges::copy_if(vertices, std::back_inserter(matching),
                             [texture](const drawn_vertex& drawn) {
                                 return drawn.texture == static_cast<ImTextureID>(texture);
                             });
        return matching;
    }

    std::vector<drawn_vertex> with_color(const color& wanted) const {
        std::vector<drawn_vertex> matching{};
        std::ranges::copy_if(vertices, std::back_inserter(matching),
                             [wanted = to_imu32(wanted)](const drawn_vertex& drawn) {
                                 return drawn.vertex.col == wanted;
                             });
        return matching;
    }

    const pixel_perfect_layout& presentation() const {
        REQUIRE(layout.has_value());
        return *layout;
    }
};

// Every vertex drawList holds from firstIndex on, walked through the index buffer command by
// command: the order the GPU draws them in, each vertex repeated once per triangle using it.
//
// Sampler callbacks are followed from the start of the draw list, so each vertex records the filter
// the backend will actually sample it with.
std::vector<drawn_vertex> vertices_in_draw_order(
    const ImDrawList& drawList, const int firstIndex,
    const int endIndex = std::numeric_limits<int>::max()) {
    const ImGuiPlatformIO& platformIo{ImGui::GetPlatformIO()};

    std::vector<drawn_vertex> drawn{};
    active_sampler sampler{active_sampler::linear};
    for (int commandIndex{0}; commandIndex < drawList.CmdBuffer.Size; ++commandIndex) {
        const ImDrawCmd& command{drawList.CmdBuffer[commandIndex]};
        if (command.UserCallback != nullptr) {
            if (command.UserCallback == platformIo.DrawCallback_SetSamplerNearest) {
                sampler = active_sampler::nearest;
            } else if (command.UserCallback == platformIo.DrawCallback_SetSamplerLinear ||
                       command.UserCallback == platformIo.DrawCallback_ResetRenderState) {
                sampler = active_sampler::linear;
            }
            continue;
        }

        const int begin{static_cast<int>(command.IdxOffset)};
        const int end{std::min(begin + static_cast<int>(command.ElemCount), endIndex)};
        for (int index{std::max(begin, firstIndex)}; index < end; ++index) {
            const ImDrawIdx vertexIndex{drawList.IdxBuffer[index]};
            drawn.push_back(drawn_vertex{
                .vertex = drawList.VtxBuffer[static_cast<int>(command.VtxOffset) + vertexIndex],
                .texture = command_texture(command),
                .clipRect = screen_rect{Eigen::Vector2d{command.ClipRect.x, command.ClipRect.y},
                                        Eigen::Vector2d{command.ClipRect.z, command.ClipRect.w}},
                .command = commandIndex,
                .sampler = sampler,
            });
        }
    }
    return drawn;
}

// Stand-ins for the sampler callbacks a real backend (DirectX 12, OpenGL3) installs in the
// platform IO. They are only ever compared against, never called: no draw data is rendered here.
//
// Each body must differ: MSVC's Release linker folds functions with identical code into one
// (/OPT:ICF), which would give all three the same address and make them indistinguishable.
[[maybe_unused]] int last_fake_sampler_callback{0};
void fake_reset_render_state(const ImDrawList* /*drawList*/, const ImDrawCmd* /*command*/) {
    last_fake_sampler_callback = 1;
}
void fake_set_sampler_linear(const ImDrawList* /*drawList*/, const ImDrawCmd* /*command*/) {
    last_fake_sampler_callback = 2;
}
void fake_set_sampler_nearest(const ImDrawList* /*drawList*/, const ImDrawCmd* /*command*/) {
    last_fake_sampler_callback = 3;
}

// Installs the fake sampler callbacks into the current context for as long as it lives, so the
// headless context looks like one with a real renderer behind it.
struct scoped_fake_backend_samplers {
    scoped_fake_backend_samplers() {
        ImGuiPlatformIO& platformIo{ImGui::GetPlatformIO()};
        platformIo.DrawCallback_ResetRenderState = fake_reset_render_state;
        platformIo.DrawCallback_SetSamplerLinear = fake_set_sampler_linear;
        platformIo.DrawCallback_SetSamplerNearest = fake_set_sampler_nearest;
    }

    ~scoped_fake_backend_samplers() {
        ImGuiPlatformIO& platformIo{ImGui::GetPlatformIO()};
        platformIo.DrawCallback_ResetRenderState = nullptr;
        platformIo.DrawCallback_SetSamplerLinear = nullptr;
        platformIo.DrawCallback_SetSamplerNearest = nullptr;
    }

    scoped_fake_backend_samplers(const scoped_fake_backend_samplers&) = delete;
    scoped_fake_backend_samplers& operator=(const scoped_fake_backend_samplers&) = delete;
};

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
    ImGui::Begin("scene-virtual-resolution-tests-host", nullptr, HOST_FLAGS);

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

// Wraps draw so the capture also records what the scene was handed: the painter's bounds, the clip
// rectangle and the camera's viewport.
std::function<void(scene_painter&)> observed(scene_capture& capture,
                                             std::function<void(scene_painter&)> draw) {
    return [&capture, draw = std::move(draw)](scene_painter& painter) {
        capture.painterBounds = painter.bounds();
        const ImDrawList& drawList{*ImGui::GetWindowDrawList()};
        capture.sceneClipRect =
            screen_rect{to_eigen(drawList.GetClipRectMin()), to_eigen(drawList.GetClipRectMax())};
        capture.viewportSize = painter.camera().viewport_size();
        if (draw) {
            draw(painter);
        }
    };
}

// Renders a page surface for one frame inside the host, drawing the scene with draw, and returns
// everything the surface drew: its background, letterbox and grid included. Whatever after draws
// into the host right after the surface is captured on its own.
scene_capture capture_scene(headless_imgui_context& context, scene_surface& surface,
                            const std::function<void(scene_painter&)>& draw = {},
                            const std::function<void(ImDrawList&)>& after = {}) {
    scene_capture capture{};
    const scoped_scene_drawing drawing{surface, observed(capture, draw)};

    run_frame(context, [&] {
        render_in_host([&] {
            ImDrawList& drawList{*ImGui::GetWindowDrawList()};
            const int firstIndex{drawList.IdxBuffer.Size};

            surface.render();
            const int surfaceEnd{drawList.IdxBuffer.Size};
            if (after) {
                after(drawList);
            }

            capture.vertices = vertices_in_draw_order(drawList, firstIndex, surfaceEnd);
            capture.afterSurface = vertices_in_draw_order(drawList, surfaceEnd);
            capture.bounds = surface.bounds();
            capture.layout = surface.presentation_layout();
        });
    });
    return capture;
}

// Renders a full-window surface for one frame and returns everything it drew, read from the
// window it opens for itself.
scene_capture capture_fill_window_scene(headless_imgui_context& context, scene_surface& surface,
                                        const char* surfaceId,
                                        const std::function<void(scene_painter&)>& draw = {}) {
    scene_capture capture{};
    const scoped_scene_drawing drawing{surface, observed(capture, draw)};

    run_frame(context, [&] {
        surface.render();

        const ImGuiWindow* const window{ImGui::FindWindowByName(surfaceId)};
        REQUIRE(window != nullptr);
        capture.vertices = vertices_in_draw_order(*window->DrawList, 0);
        capture.bounds = surface.bounds();
        capture.layout = surface.presentation_layout();
    });
    return capture;
}

// The page size of a surface presenting VIRTUAL at scale, letterboxed on every side.
vector2 surface_size_for(const std::uint32_t scale) {
    return vector2{VIRTUAL_EXTENT * static_cast<double>(scale) + LEFTOVER};
}

// A surface with no grid, presenting VIRTUAL, its camera placed so the world origin is the virtual
// screen's top-left corner: world units and virtual pixels then read the same.
void present_virtual(scene_surface& surface) {
    surface.set_grid(std::nullopt);
    surface.set_virtual_resolution(VIRTUAL);
    surface.camera().set_position(vector2{VIRTUAL_EXTENT / 2.0});
    surface.camera().set_zoom(1.0);
}

// Where a world point lands before any snapping: through the camera into virtual pixels, then
// scaled and offset onto the content.
Eigen::Vector2d exact_point(const pixel_perfect_layout& layout, const camera_2d& camera,
                            const Eigen::Vector2d& world) {
    return layout.content.min() + static_cast<double>(layout.scale) *
                                      Eigen::Vector2d{camera.world_to_screen(vector2{world})};
}

screen_rect exact_rect(const pixel_perfect_layout& layout, const camera_2d& camera,
                       const world_rect& world) {
    return screen_rect{exact_point(layout, camera, world.min()),
                       exact_point(layout, camera, world.max())};
}

// A screen point on a whole virtual pixel: a whole number of scales from the content's corner.
bool on_virtual_grid(const pixel_perfect_layout& layout, const Eigen::Vector2d& point) {
    const Eigen::Vector2d virtualPoint{(point - layout.content.min()) /
                                       static_cast<double>(layout.scale)};
    return std::abs(virtualPoint.x() - std::round(virtualPoint.x())) < PIXEL_EPSILON &&
           std::abs(virtualPoint.y() - std::round(virtualPoint.y())) < PIXEL_EPSILON;
}

bool every_vertex_on_virtual_grid(const pixel_perfect_layout& layout,
                                  const std::vector<drawn_vertex>& vertices) {
    return std::ranges::all_of(vertices, [&layout](const drawn_vertex& drawn) {
        return on_virtual_grid(layout, to_eigen(drawn.vertex.pos));
    });
}

// A snapped quad: its corners on whole virtual pixels, within half a virtual pixel of the exact
// mapping.
void require_snapped_to_virtual(const pixel_perfect_layout& layout, const screen_rect& drawn,
                                const screen_rect& exact) {
    REQUIRE(on_virtual_grid(layout, drawn.min()));
    REQUIRE(on_virtual_grid(layout, drawn.max()));
    require_same_rect(drawn, exact, 0.5 * static_cast<double>(layout.scale) + PIXEL_EPSILON);
}

// A texture per tile, so every tile of a row or a floor can be told apart in the draw list.
sprite_texture tile_texture(const int column, const int row) {
    return sprite_texture{
        .id = 0x7000 + static_cast<texture_id>(row * 1000 + column), .width = 4, .height = 4};
}

// The corners of a whole-texture quad, named by the UV each one carries.
const Eigen::Vector2d UV_TOP_LEFT{0.0, 0.0};
const Eigen::Vector2d UV_TOP_RIGHT{1.0, 0.0};
const Eigen::Vector2d UV_BOTTOM_RIGHT{1.0, 1.0};
const Eigen::Vector2d UV_BOTTOM_LEFT{0.0, 1.0};

// Where each corner of a whole-texture sprite landed, found by the UV it carries rather than by its
// position: flipped or turned, the texture's top-left texel is no longer drawn top-left.
struct sprite_quad {
    Eigen::Vector2d topLeftUv{};
    Eigen::Vector2d topRightUv{};
    Eigen::Vector2d bottomRightUv{};
    Eigen::Vector2d bottomLeftUv{};
};

const drawn_vertex& vertex_with_uv(const std::vector<drawn_vertex>& vertices,
                                   const Eigen::Vector2d& uv) {
    const auto found{std::ranges::find_if(vertices, [&uv](const drawn_vertex& drawn) {
        return (to_eigen(drawn.vertex.uv) - uv).cwiseAbs().maxCoeff() < UV_EPSILON;
    })};
    REQUIRE(found != vertices.end());
    return *found;
}

sprite_quad quad_of(const std::vector<drawn_vertex>& sprite) {
    REQUIRE(sprite.size() == 6);
    return sprite_quad{
        .topLeftUv = to_eigen(vertex_with_uv(sprite, UV_TOP_LEFT).vertex.pos),
        .topRightUv = to_eigen(vertex_with_uv(sprite, UV_TOP_RIGHT).vertex.pos),
        .bottomRightUv = to_eigen(vertex_with_uv(sprite, UV_BOTTOM_RIGHT).vertex.pos),
        .bottomLeftUv = to_eigen(vertex_with_uv(sprite, UV_BOTTOM_LEFT).vertex.pos),
    };
}

void require_quad(const sprite_quad& actual, const sprite_quad& expected,
                  const double epsilon = PIXEL_EPSILON) {
    require_near(actual.topLeftUv, expected.topLeftUv, epsilon);
    require_near(actual.topRightUv, expected.topRightUv, epsilon);
    require_near(actual.bottomRightUv, expected.bottomRightUv, epsilon);
    require_near(actual.bottomLeftUv, expected.bottomLeftUv, epsilon);
}

// Where the corner of rect carrying uvCorner should land, before any snapping: scaled about the
// origin, then turned about it, then mapped through the camera and the virtual resolution.
Eigen::Vector2d exact_transformed_corner(const pixel_perfect_layout& layout,
                                         const camera_2d& camera, const world_rect& rect,
                                         const sprite_options& options,
                                         const Eigen::Vector2d& uvCorner) {
    const Eigen::Vector2d pivot{rect.min() + Eigen::Vector2d{options.origin}};
    const Eigen::Vector2d corner{rect.min() + rect.sizes().cwiseProduct(uvCorner)};
    const Eigen::Vector2d scaled{(corner - pivot).cwiseProduct(Eigen::Vector2d{options.scale})};
    const Eigen::Rotation2Dd rotation{options.rotation * std::numbers::pi / 180.0};
    return exact_point(layout, camera, pivot + rotation * scaled);
}

sprite_quad exact_transformed_quad(const pixel_perfect_layout& layout, const camera_2d& camera,
                                   const world_rect& rect, const sprite_options& options) {
    return sprite_quad{
        .topLeftUv = exact_transformed_corner(layout, camera, rect, options, UV_TOP_LEFT),
        .topRightUv = exact_transformed_corner(layout, camera, rect, options, UV_TOP_RIGHT),
        .bottomRightUv = exact_transformed_corner(layout, camera, rect, options, UV_BOTTOM_RIGHT),
        .bottomLeftUv = exact_transformed_corner(layout, camera, rect, options, UV_BOTTOM_LEFT),
    };
}

} // namespace

// ===================================== Configuration =====================================

static_assert(
    std::same_as<decltype(std::declval<const scene_surface&>().virtual_resolution()),
                 std::optional<virtual_size>>,
    "presenting a virtual resolution is opt-in: a surface shows its camera at its own size");
static_assert(std::same_as<decltype(std::declval<const scene_surface&>().screen_to_virtual(
                               std::declval<const vector2&>())),
                           std::optional<vector2>>,
              "a pointer in a letterbox bar points at nothing in the scene");

TEST_CASE("scene_surface virtual resolution settings",
          "[tca][engine][rendering][scene][scene_surface][virtual_resolution]") {
    scene_surface surface{
        "##surface", vector2{400.0, 300.0}
    };

    SECTION("By default a surface presents no virtual resolution") {
        REQUIRE_FALSE(surface.virtual_resolution().has_value());
        REQUIRE_FALSE(surface.presentation_layout().has_value());
    }

    SECTION("The letterbox is filled with the window background by default") {
        REQUIRE(surface.letterbox_role() == color_role::window_background);
    }

    SECTION("A virtual resolution can be set, read back and cleared") {
        surface.set_virtual_resolution(VIRTUAL_320x180);
        REQUIRE(surface.virtual_resolution() == std::optional{VIRTUAL_320x180});

        surface.set_virtual_resolution(std::nullopt);
        REQUIRE_FALSE(surface.virtual_resolution().has_value());
    }

    SECTION("The letterbox role can be changed") {
        surface.set_letterbox_role(color_role::overlay_scrim);
        REQUIRE(surface.letterbox_role() == color_role::overlay_scrim);
    }

    SECTION("A resolution of zero on either axis is rejected, leaving the surface as it was") {
        surface.set_virtual_resolution(VIRTUAL_320x180);

        const virtual_size invalid = GENERATE(values<virtual_size>({
            {0,   180},
            {320, 0  },
            {0,   0  }
        }));
        CAPTURE(invalid.width, invalid.height);

        REQUIRE_THROWS_AS(surface.set_virtual_resolution(invalid), std::invalid_argument);
        REQUIRE(surface.virtual_resolution() == std::optional{VIRTUAL_320x180});
    }

    SECTION("There is no layout until the surface renders") {
        surface.set_virtual_resolution(VIRTUAL_320x180);
        REQUIRE_FALSE(surface.presentation_layout().has_value());
    }
}

// ======================================== Layout ========================================

TEST_CASE("scene_surface presents a virtual resolution inside a page",
          "[tca][engine][rendering][scene][scene_surface][virtual_resolution][layout]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    const theming::scoped_active_theme activeTheme{probe_theme()};

    const std::uint32_t scale = GENERATE(1u, 2u, 3u, 4u, 5u, 6u);
    CAPTURE(scale);

    scene_surface surface{"##surface", surface_size_for(scale)};
    present_virtual(surface);

    const scene_capture capture{capture_scene(imguiContext, surface)};
    const pixel_perfect_layout& layout{capture.presentation()};

    SECTION("The surface keeps the size it was given") {
        require_near(capture.bounds.sizes(), surface_size_for(scale));
    }

    SECTION("The layout is the largest whole scale that fits the surface, centred in it") {
        REQUIRE(layout.resolution == VIRTUAL);
        REQUIRE(layout.scale == scale);
        const Eigen::Vector2d contentMin{capture.bounds.min() + CONTENT_OFFSET};
        require_same_rect(
            layout.content,
            screen_box(contentMin, contentMin + VIRTUAL_EXTENT * static_cast<double>(scale)));
    }

    SECTION("It is exactly the shared layout maths applied to the surface's bounds") {
        const pixel_perfect_layout expected{compute_pixel_perfect_layout(capture.bounds, VIRTUAL)};
        REQUIRE(layout.scale == expected.scale);
        require_same_rect(layout.content, expected.content);
        require_same_rects(layout.letterbox, expected.letterbox);
    }

    SECTION("The camera's viewport is the virtual resolution: one world unit, one virtual pixel") {
        require_near(surface.camera().viewport_size(), VIRTUAL_EXTENT);
        require_near(capture.viewportSize, VIRTUAL_EXTENT);
    }

    SECTION("The scene is handed the content as its bounds, and clipped to it") {
        require_same_rect(capture.painterBounds, layout.content);
        require_same_rect(capture.sceneClipRect, layout.content);
    }

    SECTION("The background fills the content and the letterbox fills the rest") {
        const std::vector<drawn_vertex> background{capture.with_color(BACKGROUND_PROBE_COLOR)};
        REQUIRE(background.size() == 6);
        require_same_rect(bounding_box(background), layout.content);

        require_same_rects(filled_rects(capture.with_color(LETTERBOX_PROBE_COLOR)),
                           layout.letterbox);
        REQUIRE(layout.letterbox.size() == 4);
    }

    SECTION("The letterbox follows its theme role") {
        surface.set_letterbox_role(color_role::overlay_scrim);
        const theming::scoped_active_theme scrimTheme{[] {
            theming::theme customTheme{probe_theme()};
            customTheme.set(color_role::overlay_scrim, PROBE_COLOR);
            return customTheme;
        }()};

        const scene_capture scrimCapture{capture_scene(imguiContext, surface)};

        REQUIRE(scrimCapture.with_color(LETTERBOX_PROBE_COLOR).empty());
        require_same_rects(filled_rects(scrimCapture.with_color(PROBE_COLOR)),
                           scrimCapture.presentation().letterbox);
    }

    SECTION("Without a background role the content is left undrawn, but the letterbox is not") {
        surface.set_background_role(std::nullopt);

        const scene_capture bareCapture{capture_scene(imguiContext, surface)};

        REQUIRE(bareCapture.with_color(BACKGROUND_PROBE_COLOR).empty());
        REQUIRE(filled_rects(bareCapture.with_color(LETTERBOX_PROBE_COLOR)).size() == 4);
    }
}

TEST_CASE("scene_surface presents a virtual resolution over the whole window",
          "[tca][engine][rendering][scene][scene_surface][virtual_resolution][full_window]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    const theming::scoped_active_theme activeTheme{probe_theme()};

    scene_surface surface{"##virtual-surface", fill_window};
    surface.set_virtual_resolution(VIRTUAL_320x180);

    SECTION("A window exactly 4 times the resolution is covered edge to edge, with no letterbox") {
        const scene_capture capture{
            capture_fill_window_scene(imguiContext, surface, "##virtual-surface")};
        const pixel_perfect_layout& layout{capture.presentation()};

        REQUIRE(layout.scale == 4u);
        require_same_rect(layout.content,
                          screen_box(Eigen::Vector2d::Zero(), to_eigen(DISPLAY_SIZE)));
        REQUIRE(layout.letterbox.empty());
        REQUIRE(capture.with_color(LETTERBOX_PROBE_COLOR).empty());
    }

    SECTION("The letterbox covers the whole client area the content leaves") {
        imguiContext.set_display_size(ImVec2{1366.0f, 768.0f});

        const scene_capture capture{
            capture_fill_window_scene(imguiContext, surface, "##virtual-surface")};
        const pixel_perfect_layout& layout{capture.presentation()};

        REQUIRE(layout.scale == 4u);
        require_same_rect(layout.content,
                          screen_box(Eigen::Vector2d{43.0, 24.0}, Eigen::Vector2d{1323.0, 744.0}));

        const std::vector<screen_rect> bars{
            filled_rects(capture.with_color(LETTERBOX_PROBE_COLOR))};
        require_same_rects(bars, layout.letterbox);
        double coveredArea{layout.content.sizes().prod()};
        for (const screen_rect& bar : bars) {
            coveredArea += bar.sizes().prod();
        }
        require_near(coveredArea, 1366.0 * 768.0);
    }

    SECTION("The scale follows the client area as the window is resized or goes fullscreen") {
        struct step {
            ImVec2 displaySize;
            std::uint32_t scale;
        };
        const std::array steps{
            step{ImVec2{1280.0f, 720.0f},  4 },
            step{ImVec2{1920.0f, 1080.0f}, 6 },
            step{ImVec2{1366.0f, 768.0f},  4 },
            step{ImVec2{640.0f, 360.0f},   2 },
            step{ImVec2{3840.0f, 2160.0f}, 12},
            step{ImVec2{200.0f, 100.0f},   1 },
        };

        for (const step& next : steps) {
            CAPTURE(next.displaySize.x, next.displaySize.y);
            imguiContext.set_display_size(next.displaySize);

            const scene_capture capture{
                capture_fill_window_scene(imguiContext, surface, "##virtual-surface")};

            REQUIRE(capture.presentation().scale == next.scale);
            require_same_rect(capture.bounds,
                              screen_box(Eigen::Vector2d::Zero(), to_eigen(next.displaySize)));
            require_near(surface.camera().viewport_size(), Eigen::Vector2d{320.0, 180.0});
        }
    }
}

TEST_CASE("scene_surface scales a virtual resolution in physical pixels, whatever the UI scale",
          "[tca][engine][rendering][scene][scene_surface][virtual_resolution][dpi]") {
    headless_imgui_context imguiContext{
        ImVec2{960.0f, 540.0f}
    };

    const float uiScale = GENERATE(1.0f, 1.25f, 1.5f, 2.0f);
    CAPTURE(uiScale);

    // The engine carries the per-monitor DPI scale in the ImGui style (see
    // application/win32/imgui-win32-utilities.cpp): it must not reach the integer scale.
    ImGui::GetStyle().FontScaleDpi = uiScale;

    SECTION("A 960x540 client area shows 320x180 at exactly 3x") {
        scene_surface surface{"##virtual-surface", fill_window};
        surface.set_virtual_resolution(VIRTUAL_320x180);

        const scene_capture capture{
            capture_fill_window_scene(imguiContext, surface, "##virtual-surface")};

        REQUIRE(capture.presentation().scale == 3u);
        require_same_rect(capture.presentation().content,
                          screen_box(Eigen::Vector2d::Zero(), Eigen::Vector2d{960.0, 540.0}));
        REQUIRE(capture.presentation().letterbox.empty());
    }

    SECTION("A page surface is scaled from the physical pixels it was laid out at") {
        // A fixed size is in unscaled pixels, like every other widget dimension, so the surface
        // lays out at 320x180 times the UI scale: only a whole multiple reaches the next scale.
        scene_surface surface{
            "##surface", vector2{320.0, 180.0}
        };
        surface.set_virtual_resolution(VIRTUAL_320x180);

        const scene_capture capture{capture_scene(imguiContext, surface)};

        require_near(capture.bounds.sizes(), Eigen::Vector2d{320.0 * uiScale, 180.0 * uiScale});
        const std::uint32_t expectedScale{static_cast<std::uint32_t>(std::floor(uiScale))};
        REQUIRE(capture.presentation().scale == expectedScale);
        REQUIRE(capture.presentation().scale ==
                compute_pixel_perfect_layout(capture.bounds, VIRTUAL_320x180).scale);
    }
}

TEST_CASE("scene_surface goes back to a plain surface when its virtual resolution is cleared",
          "[tca][engine][rendering][scene][scene_surface][virtual_resolution]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    const theming::scoped_active_theme activeTheme{probe_theme()};

    scene_surface surface{"##surface", surface_size_for(3)};
    present_virtual(surface);
    static_cast<void>(capture_scene(imguiContext, surface));

    surface.set_virtual_resolution(std::nullopt);
    const scene_capture capture{capture_scene(imguiContext, surface)};

    REQUIRE_FALSE(capture.layout.has_value());
    require_near(surface.camera().viewport_size(), capture.bounds.sizes());
    require_same_rect(capture.painterBounds, capture.bounds);
    require_same_rect(capture.sceneClipRect, capture.bounds);
    REQUIRE(capture.with_color(LETTERBOX_PROBE_COLOR).empty());
    require_same_rect(bounding_box(capture.with_color(BACKGROUND_PROBE_COLOR)), capture.bounds);
}

// =================================== Pointer mapping ===================================

TEST_CASE("scene_surface maps pointer positions through its virtual resolution",
          "[tca][engine][rendering][scene][scene_surface][virtual_resolution][pointer]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    SECTION("Before the first render nothing is shown, so nothing is pointed at") {
        scene_surface surface{"##surface", surface_size_for(3)};
        present_virtual(surface);

        REQUIRE_FALSE(surface.screen_to_virtual(vector2{100.0, 100.0}).has_value());
    }

    SECTION("Presenting a virtual resolution") {
        scene_surface surface{"##surface", surface_size_for(3)};
        present_virtual(surface);
        const scene_capture capture{capture_scene(imguiContext, surface)};
        const Eigen::Vector2d contentMin{capture.presentation().content.min()};

        // Screen pixel (31, 17) from the content's corner is inside virtual pixel (10, 5).
        const std::optional<vector2> pointed{
            surface.screen_to_virtual(vector2{contentMin + Eigen::Vector2d{31.5, 17.5}}
            )
        };
        REQUIRE(pointed.has_value());
        REQUIRE(std::floor(pointed->x()) == 10.0);
        REQUIRE(std::floor(pointed->y()) == 5.0);

        require_near(surface.virtual_to_screen(vector2{10.0, 5.0}),
                     contentMin + Eigen::Vector2d{30.0, 15.0});
        const std::optional<vector2> roundTrip{
            surface.screen_to_virtual(surface.virtual_to_screen(vector2{12.25, 7.5}))};
        REQUIRE(roundTrip.has_value());
        require_near(*roundTrip, Eigen::Vector2d{12.25, 7.5});
    }

    SECTION("A pointer in a letterbox bar or outside the surface points at nothing") {
        scene_surface surface{"##surface", surface_size_for(3)};
        present_virtual(surface);
        const scene_capture capture{capture_scene(imguiContext, surface)};

        REQUIRE_FALSE(surface
                          .screen_to_virtual(vector2{
                              capture.bounds.min() + Eigen::Vector2d{2.0, 50.0}
        })
                          .has_value());
        REQUIRE_FALSE(surface
                          .screen_to_virtual(vector2{
                              capture.bounds.max() + Eigen::Vector2d{10.0, 10.0}
        })
                          .has_value());
    }

    SECTION("The world point under the pointer is drawn under the pointer") {
        scene_surface surface{"##surface", surface_size_for(3)};
        present_virtual(surface);
        surface.camera().pan(vector2{5.0, -3.0});
        const scene_capture first{capture_scene(imguiContext, surface)};

        const vector2 pointer{
            first.presentation().content.min() + Eigen::Vector2d{100.5, 40.5}
        };
        const std::optional<vector2> virtualPoint{surface.screen_to_virtual(pointer)};
        REQUIRE(virtualPoint.has_value());
        const vector2 world{surface.camera().screen_to_world(*virtualPoint)};

        const double cellX{std::floor(world.x())};
        const double cellY{std::floor(world.y())};
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(world_box(cellX, cellY, cellX + 1.0, cellY + 1.0),
                                    PROBE_TEXTURE);
            })};

        const screen_rect cell{bounding_box(capture.with_texture(PROBE_TEXTURE_ID))};
        REQUIRE(cell.contains(Eigen::Vector2d{pointer}));
    }

    SECTION("Without a virtual resolution, the surface's own pixels are the camera's viewport") {
        scene_surface surface{
            "##surface", vector2{400.0, 300.0}
        };
        const scene_capture capture{capture_scene(imguiContext, surface)};

        const std::optional<vector2> pointed{
            surface.screen_to_virtual(vector2{capture.bounds.min() + Eigen::Vector2d{12.5, 7.25}}
            )
        };
        REQUIRE(pointed.has_value());
        require_near(*pointed, Eigen::Vector2d{12.5, 7.25});
        require_near(surface.virtual_to_screen(vector2{12.5, 7.25}),
                     capture.bounds.min() + Eigen::Vector2d{12.5, 7.25});
        REQUIRE_FALSE(surface
                          .screen_to_virtual(vector2{
                              capture.bounds.max() + Eigen::Vector2d{1.0, 1.0}
        })
                          .has_value());
    }
}

// =============================== Snapping to virtual pixels ===============================

TEST_CASE("Sprites snap to whole virtual pixels",
          "[tca][engine][rendering][scene][virtual_resolution][sprites][snapping]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    const std::uint32_t scale = GENERATE(1u, 2u, 3u, 4u, 5u, 6u);
    CAPTURE(scale);
    scene_surface surface{"##surface", surface_size_for(scale)};
    present_virtual(surface);
    surface.set_background_role(std::nullopt);

    SECTION("A sprite between virtual pixels lands on whole ones, scale screen pixels each") {
        const world_rect rect{world_box(10.3, 5.6, 18.45, 17.2)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(rect, PROBE_TEXTURE);
            })};
        const pixel_perfect_layout& layout{capture.presentation()};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(sprite.size() == 6);
        require_snapped_to_virtual(layout, bounding_box(sprite),
                                   exact_rect(layout, surface.camera(), rect));
    }

    SECTION("A sprite on whole virtual pixels lands exactly where they are") {
        const world_rect rect{world_box(10.0, 5.0, 18.0, 17.0)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(rect, PROBE_TEXTURE);
            })};
        const pixel_perfect_layout& layout{capture.presentation()};

        require_same_rect(bounding_box(capture.with_texture(PROBE_TEXTURE_ID)),
                          exact_rect(layout, surface.camera(), rect));
    }

    SECTION("Every virtual pixel spans exactly scale screen pixels, wherever the camera is") {
        // The camera between virtual pixels too, so each one-unit sprite starts part-way through a
        // virtual pixel before snapping.
        surface.camera().set_position(vector2{32.37, 18.61});
        constexpr int COLUMNS{64};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                for (int column{0}; column < COLUMNS; ++column) {
                    painter.draw_sprite(world_box(column, 10.0, column + 1.0, 11.0),
                                        tile_texture(column, 0));
                }
            })};

        std::optional<screen_rect> previous{};
        std::size_t drawnColumns{0};
        for (int column{0}; column < COLUMNS; ++column) {
            CAPTURE(column);
            const std::vector<drawn_vertex> cell{capture.with_texture(tile_texture(column, 0).id)};
            if (cell.empty()) {
                previous.reset();
                continue;
            }
            ++drawnColumns;
            const screen_rect box{bounding_box(cell)};
            // Compared exactly: one uneven column is what pixel art cannot have.
            REQUIRE(box.sizes().x() == static_cast<double>(scale));
            REQUIRE(box.sizes().y() == static_cast<double>(scale));
            if (previous) {
                REQUIRE(previous->max().x() == box.min().x());
            }
            previous = box;
        }
        REQUIRE(drawnColumns >= 63);
    }
}

TEST_CASE("Adjacent tiles show no seam under a virtual resolution",
          "[tca][engine][rendering][scene][virtual_resolution][sprites][seams]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    const std::uint32_t scale = GENERATE(1u, 3u, 5u);
    const double zoom = GENERATE(1.0, 2.0, 0.5, 1.37);
    // Parenthesised rather than braced: GENERATE would split a braced vector2{x, y} at the comma.
    const vector2 cameraPosition = GENERATE(vector2(32.0, 18.0), vector2(31.3, 17.7));
    CAPTURE(scale, zoom, cameraPosition.x(), cameraPosition.y());

    scene_surface surface{"##surface", surface_size_for(scale)};
    present_virtual(surface);
    surface.set_background_role(std::nullopt);
    surface.camera().set_position(cameraPosition);
    surface.camera().set_zoom(zoom);

    constexpr int COLUMNS{16};
    constexpr int ROWS{9};
    constexpr double TILE_SIZE{4.0};
    auto tileRect{[](const int column, const int row) {
        return world_box(column * TILE_SIZE, row * TILE_SIZE, (column + 1) * TILE_SIZE,
                         (row + 1) * TILE_SIZE);
    }};

    const scene_capture capture{capture_scene(imguiContext, surface, [&](scene_painter& painter) {
        for (int row{0}; row < ROWS; ++row) {
            for (int column{0}; column < COLUMNS; ++column) {
                painter.draw_sprite(tileRect(column, row), tile_texture(column, row));
            }
        }
    })};
    const pixel_perfect_layout& layout{capture.presentation()};

    std::vector<std::vector<std::optional<screen_rect>>> tiles(
        ROWS, std::vector<std::optional<screen_rect>>(COLUMNS));
    std::size_t tilesDrawn{0};
    for (int row{0}; row < ROWS; ++row) {
        for (int column{0}; column < COLUMNS; ++column) {
            CAPTURE(column, row);
            const std::vector<drawn_vertex> tile{
                capture.with_texture(tile_texture(column, row).id)};
            if (tile.empty()) {
                REQUIRE_FALSE(
                    surface.camera().visible_world_rect().contains(tileRect(column, row)));
                continue;
            }
            REQUIRE(tile.size() == 6);
            ++tilesDrawn;
            tiles[row][column] = bounding_box(tile);
            require_snapped_to_virtual(layout, *tiles[row][column],
                                       exact_rect(layout, surface.camera(), tileRect(column, row)));
        }
    }
    REQUIRE(tilesDrawn > 0);

    for (int row{0}; row < ROWS; ++row) {
        for (int column{0}; column < COLUMNS; ++column) {
            CAPTURE(column, row);
            if (!tiles[row][column]) {
                continue;
            }
            const screen_rect& tile{*tiles[row][column]};
            // Compared exactly: a gap or an overlap of any size is a seam.
            if (column + 1 < COLUMNS && tiles[row][column + 1]) {
                REQUIRE(tile.max().x() == tiles[row][column + 1]->min().x());
            }
            if (row + 1 < ROWS && tiles[row + 1][column]) {
                REQUIRE(tile.max().y() == tiles[row + 1][column]->min().y());
            }
        }
    }
}

TEST_CASE("A 1-pixel checkerboard at the virtual resolution covers the content exactly",
          "[tca][engine][rendering][scene][virtual_resolution][sprites][checkerboard]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    const scoped_fake_backend_samplers samplers{};

    const std::uint32_t scale = GENERATE(1u, 2u, 3u, 4u, 5u, 6u);
    CAPTURE(scale);
    scene_surface surface{"##surface", surface_size_for(scale)};
    present_virtual(surface);

    const scene_capture capture{capture_scene(imguiContext, surface, [](scene_painter& painter) {
        painter.draw_sprite(world_box(0.0, 0.0, 64.0, 36.0), CHECKER_TEXTURE);
    })};

    const std::vector<drawn_vertex> checker{capture.with_texture(CHECKER_TEXTURE_ID)};
    REQUIRE(checker.size() == 6);

    // Each texel is then exactly scale by scale screen pixels: the quad matches the content to the
    // pixel and samples the whole texture.
    require_same_rect(bounding_box(checker), capture.presentation().content);
    require_quad(quad_of(checker),
                 sprite_quad{
                     .topLeftUv = capture.presentation().content.min(),
                     .topRightUv = Eigen::Vector2d{capture.presentation().content.max().x(),
                                                   capture.presentation().content.min().y()},
                     .bottomRightUv = capture.presentation().content.max(),
                     .bottomLeftUv = Eigen::Vector2d{capture.presentation().content.min().x(),
                                                   capture.presentation().content.max().y()},
    });

    // Sampled nearest-neighbour, so no texel bleeds into its neighbour.
    REQUIRE(std::ranges::all_of(checker, sampled_nearest));
}

TEST_CASE("The camera zooms in virtual pixels under a virtual resolution",
          "[tca][engine][rendering][scene][virtual_resolution][sprites][zoom]") {
    // Zoom stays available to a pixel-art game: it scales the world in virtual pixels, and the
    // integer scale is applied on top of it.
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##surface", surface_size_for(3)};
    present_virtual(surface);
    surface.camera().set_position(vector2{16.0, 9.0});
    surface.camera().set_zoom(2.0);

    const scene_capture capture{capture_scene(imguiContext, surface, [](scene_painter& painter) {
        painter.draw_sprite(world_box(16.0, 9.0, 17.0, 10.0), PROBE_TEXTURE);
    })};
    const Eigen::Vector2d contentMin{capture.presentation().content.min()};

    // World (16, 9) is the virtual centre (32, 18); one world unit is two virtual pixels, six
    // screen pixels.
    require_same_rect(bounding_box(capture.with_texture(PROBE_TEXTURE_ID)),
                      screen_box(contentMin + Eigen::Vector2d{96.0, 54.0},
                                 contentMin + Eigen::Vector2d{102.0, 60.0}));
}

// ============================= Transformed sprites and shapes =============================

TEST_CASE("Transformed sprites under a virtual resolution",
          "[tca][engine][rendering][scene][virtual_resolution][sprites][transforms]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    const std::uint32_t scale = GENERATE(2u, 3u);
    CAPTURE(scale);
    scene_surface surface{"##surface", surface_size_for(scale)};
    present_virtual(surface);
    surface.set_background_role(std::nullopt);

    // An 8x12 character at (20, 10), its pivot at its feet but off-centre, all on whole virtual
    // pixels: a snapped flip or quarter turn lands exactly where the exact one does.
    const world_rect characterRect{world_box(20.0, 10.0, 28.0, 22.0)};
    const vector2 characterFeet{2.0, 12.0};

    auto drawCharacter{[&](const sprite_options& options) {
        return capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            painter.draw_sprite(characterRect, PROBE_TEXTURE, options);
        });
    }};

    auto requireExactly{[&](const sprite_options& options) {
        const scene_capture capture{drawCharacter(options)};
        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        require_quad(quad_of(sprite),
                     exact_transformed_quad(capture.presentation(), surface.camera(), characterRect,
                                            options));
        REQUIRE(every_vertex_on_virtual_grid(capture.presentation(), sprite));
    }};

    SECTION("An origin on its own moves nothing") {
        requireExactly(sprite_options{.origin = characterFeet});
    }

    SECTION("A horizontal flip mirrors the sprite about its origin, on whole virtual pixels") {
        requireExactly(sprite_options{
            .origin = characterFeet, .scale = vector2{-1.0, 1.0}
        });
    }

    SECTION("A vertical flip mirrors the sprite about its origin, on whole virtual pixels") {
        requireExactly(sprite_options{
            .origin = characterFeet, .scale = vector2{1.0, -1.0}
        });
    }

    SECTION("Flipping both axes mirrors the sprite through its origin") {
        requireExactly(sprite_options{
            .origin = characterFeet, .scale = vector2{-1.0, -1.0}
        });
    }

    SECTION("A whole-number scale stretches it about its origin") {
        requireExactly(sprite_options{
            .origin = characterFeet, .scale = vector2{2.0, 3.0}
        });
    }

    SECTION("Quarter turns keep it on whole virtual pixels") {
        const double rotation = GENERATE(90.0, 180.0, 270.0, -90.0);
        CAPTURE(rotation);
        requireExactly(sprite_options{.origin = characterFeet, .rotation = rotation});
    }

    SECTION("A flipped or quarter-turned sprite between virtual pixels snaps to whole ones") {
        // Picked by index: GENERATE would split a braced sprite_options at its commas.
        const int variant = GENERATE(0, 1, 2);
        CAPTURE(variant);
        const vector2 offPixelFeet{2.3, 11.6};
        const std::array variants{
            sprite_options{.origin = offPixelFeet, .scale = vector2{-1.0, 1.0}},
            sprite_options{.origin = offPixelFeet, .rotation = 90.0},
            sprite_options{.origin = offPixelFeet, .scale = vector2{1.0, -1.0}, .rotation = 270.0},
        };
        const sprite_options& options{variants[static_cast<std::size_t>(variant)]};
        surface.camera().set_position(vector2{32.37, 18.61});

        const scene_capture capture{drawCharacter(options)};
        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(sprite.size() == 6);
        REQUIRE(every_vertex_on_virtual_grid(capture.presentation(), sprite));
        require_quad(quad_of(sprite),
                     exact_transformed_quad(capture.presentation(), surface.camera(), characterRect,
                                            options),
                     0.5 * static_cast<double>(scale) + PIXEL_EPSILON);
    }

    SECTION("Any other angle is not snapped, but still scaled and offset onto the content") {
        const double rotation = GENERATE(30.0, 45.0, -12.5);
        CAPTURE(rotation);
        const sprite_options options{.origin = characterFeet, .rotation = rotation};
        surface.camera().set_position(vector2{32.37, 18.61});

        const scene_capture capture{drawCharacter(options)};

        require_quad(quad_of(capture.with_texture(PROBE_TEXTURE_ID)),
                     exact_transformed_quad(capture.presentation(), surface.camera(), characterRect,
                                            options));
    }
}

TEST_CASE("Filled rectangles under a virtual resolution",
          "[tca][engine][rendering][scene][virtual_resolution][drawing][alpha]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};

    const std::uint32_t scale = GENERATE(1u, 3u, 4u);
    CAPTURE(scale);
    scene_surface surface{"##surface", surface_size_for(scale)};
    present_virtual(surface);
    surface.set_background_role(std::nullopt);

    SECTION("A rectangle on whole virtual pixels covers exactly them") {
        const world_rect rect{world_box(4.0, 2.0, 20.0, 9.0)};
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.fill_rect(rect, PROBE_COLOR);
            })};

        require_same_rect(bounding_box(capture.with_color(PROBE_COLOR)),
                          exact_rect(capture.presentation(), surface.camera(), rect));
    }

    SECTION("A rectangle between virtual pixels snaps to whole ones") {
        const world_rect rect{world_box(4.3, 2.6, 20.2, 9.45)};
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.fill_rect(rect, PROBE_COLOR);
            })};

        const std::vector<drawn_vertex> filled{capture.with_color(PROBE_COLOR)};
        REQUIRE_FALSE(filled.empty());
        REQUIRE(every_vertex_on_virtual_grid(capture.presentation(), filled));
        require_snapped_to_virtual(capture.presentation(), bounding_box(filled),
                                   exact_rect(capture.presentation(), surface.camera(), rect));
    }

    SECTION("An alpha still fades it, snapped the same way") {
        // 200 * 0.5 = 100.
        const color translucent{tca::engine::rendering::rgba(11, 22, 33, 200)};
        const color faded{tca::engine::rendering::rgba(11, 22, 33, 100)};
        const world_rect rect{world_box(4.3, 2.6, 20.2, 9.45)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.fill_rect(rect, translucent, 0.5);
            })};

        const std::vector<drawn_vertex> filled{capture.with_color(faded)};
        REQUIRE_FALSE(filled.empty());
        REQUIRE(capture.with_color(translucent).empty());
        REQUIRE(every_vertex_on_virtual_grid(capture.presentation(), filled));
    }

    SECTION("A screen flash over the visible world covers the content, not the letterbox") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.fill_rect(painter.camera().visible_world_rect(), PROBE_COLOR, 0.5);
            })};

        require_same_rect(
            bounding_box(capture.with_color(tca::engine::rendering::rgba(11, 22, 33, 128))),
            capture.presentation().content);
    }
}

TEST_CASE("Lines, circles and the grid follow a virtual resolution",
          "[tca][engine][rendering][scene][virtual_resolution][drawing]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    constexpr std::uint32_t SCALE{3};
    scene_surface surface{"##surface", surface_size_for(SCALE)};
    present_virtual(surface);
    surface.set_background_role(std::nullopt);

    SECTION("A line runs between its ends mapped onto the content") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [](scene_painter& painter) {
                painter.draw_line(vector2{10.0, 20.0}, vector2{40.0, 20.0}, PROBE_COLOR, 1.0);
            })};

        const screen_rect line{bounding_box(capture.with_color(PROBE_COLOR))};
        const Eigen::Vector2d contentMin{capture.presentation().content.min()};
        // Its ends at virtual x 10 and 40, three screen pixels each, give or take the line's
        // thickness and anti-aliasing fringe.
        require_near(line.center(), contentMin + Eigen::Vector2d{75.0, 60.0}, 1.0);
        require_near(line.sizes().x(), 90.0, 4.0);
    }

    SECTION("A circle is centred and sized in virtual pixels") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [](scene_painter& painter) {
                painter.fill_circle(vector2{30.0, 15.0}, 5.0, PROBE_COLOR);
            })};

        const screen_rect circle{bounding_box(capture.with_color(PROBE_COLOR))};
        const Eigen::Vector2d contentMin{capture.presentation().content.min()};
        require_near(circle.center(), contentMin + Eigen::Vector2d{90.0, 45.0}, 1.0);
        require_near(circle.sizes().x(), 30.0, 2.0);
        require_near(circle.sizes().y(), 30.0, 2.0);
    }

    SECTION("Grid lines sit on the world coordinates they mark, mapped onto the content") {
        const theming::scoped_active_theme activeTheme{probe_theme()};
        surface.set_grid(grid_options{.spacing = 16.0});

        const scene_capture capture{capture_scene(imguiContext, surface)};
        const pixel_perfect_layout& layout{capture.presentation()};

        std::vector<double> verticalCentres{};
        for (const screen_rect& line : filled_rects(capture.with_color(GRID_PROBE_COLOR))) {
            if (line.sizes().y() > line.sizes().x()) {
                verticalCentres.push_back(line.center().x());
            }
        }
        // World x 16, 32 and 48 are virtual x 16, 32 and 48: 48 screen pixels apart.
        for (const double worldX : {16.0, 32.0, 48.0}) {
            CAPTURE(worldX);
            const double expected{layout.content.min().x() + SCALE * worldX};
            REQUIRE(std::ranges::any_of(verticalCentres, [expected](const double centre) {
                return std::abs(centre - expected) < PIXEL_EPSILON;
            }));
        }
        for (const double centre : verticalCentres) {
            CAPTURE(centre);
            const double virtualX{(centre - layout.content.min().x()) / SCALE};
            REQUIRE(std::abs(virtualX / 16.0 - std::round(virtualX / 16.0)) < PIXEL_EPSILON);
        }
    }
}

// ================================== Culling and clipping ==================================

TEST_CASE("Sprites outside the virtual screen are culled, even over the letterbox",
          "[tca][engine][rendering][scene][virtual_resolution][sprites][culling]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    constexpr std::uint32_t SCALE{3};
    scene_surface surface{"##surface", surface_size_for(SCALE)};
    present_virtual(surface);
    surface.set_background_role(std::nullopt);

    SECTION(
        "A sprite just past the virtual screen's right edge, inside the surface, is not drawn") {
        // Virtual x 64.5 to 66 is screen x 199.5 to 204 from the surface's left edge: within its
        // 205 pixels, over the right letterbox bar, but outside the virtual screen.
        const scene_capture capture{
            capture_scene(imguiContext, surface, [](scene_painter& painter) {
                painter.draw_sprite(world_box(64.5, 10.0, 66.0, 14.0), PROBE_TEXTURE);
            })};

        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).empty());
    }

    SECTION("A sprite straddling the edge is drawn, clipped to the content") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [](scene_painter& painter) {
                painter.draw_sprite(world_box(62.0, 10.0, 66.0, 14.0), PROBE_TEXTURE);
            })};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(sprite.size() == 6);
        for (const drawn_vertex& drawn : sprite) {
            require_same_rect(drawn.clipRect, capture.presentation().content);
        }
    }
}

// ======================================== Sampling ========================================

TEST_CASE("A virtual resolution keeps nearest sampling scoped to its sprites",
          "[tca][engine][rendering][scene][virtual_resolution][filtering]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    const theming::scoped_active_theme activeTheme{probe_theme()};

    scene_surface surface{"##surface", surface_size_for(3)};
    present_virtual(surface);

    SECTION("With sampler callbacks") {
        const scoped_fake_backend_samplers samplers{};

        const scene_capture capture{capture_scene(
            imguiContext, surface,
            [](scene_painter& painter) {
                painter.draw_sprite(world_box(0.0, 0.0, 8.0, 8.0), PROBE_TEXTURE);
                painter.fill_rect(world_box(10.0, 10.0, 20.0, 20.0), PROBE_COLOR);
                painter.draw_sprite(world_box(30.0, 0.0, 38.0, 8.0), CHECKER_TEXTURE,
                                    sprite_options{.layer = 3});
            },
            [](ImDrawList& drawList) {
                // A widget drawn by the page right after the surface.
                drawList.AddRectFilled(ImVec2{50.0f, 500.0f}, ImVec2{90.0f, 520.0f},
                                       to_imu32(AFTER_SURFACE_PROBE_COLOR));
            })};

        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).size() == 6);
        REQUIRE(std::ranges::all_of(capture.with_texture(PROBE_TEXTURE_ID), sampled_nearest));
        REQUIRE(std::ranges::all_of(capture.with_texture(CHECKER_TEXTURE_ID), sampled_nearest));

        REQUIRE_FALSE(capture.with_color(PROBE_COLOR).empty());
        REQUIRE(std::ranges::all_of(capture.with_color(PROBE_COLOR), sampled_linear));
        REQUIRE_FALSE(capture.with_color(BACKGROUND_PROBE_COLOR).empty());
        REQUIRE(std::ranges::all_of(capture.with_color(BACKGROUND_PROBE_COLOR), sampled_linear));
        REQUIRE_FALSE(capture.with_color(LETTERBOX_PROBE_COLOR).empty());
        REQUIRE(std::ranges::all_of(capture.with_color(LETTERBOX_PROBE_COLOR), sampled_linear));

        REQUIRE_FALSE(capture.afterSurface.empty());
        REQUIRE(std::ranges::all_of(capture.afterSurface, sampled_linear));
    }

    SECTION("Without sampler callbacks, sprites still draw and no callback is added") {
        REQUIRE(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest == nullptr);

        int callbackCommands{};
        std::size_t spriteVertices{};
        const scoped_scene_drawing drawing{surface, [](scene_painter& painter) {
                                               painter.draw_sprite(world_box(0.0, 0.0, 8.0, 8.0),
                                                                   PROBE_TEXTURE);
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                const ImDrawList& drawList{*ImGui::GetWindowDrawList()};
                const int firstIndex{drawList.IdxBuffer.Size};
                surface.render();
                for (const ImDrawCmd& command : drawList.CmdBuffer) {
                    if (command.UserCallback != nullptr &&
                        static_cast<int>(command.IdxOffset) >= firstIndex) {
                        ++callbackCommands;
                    }
                }
                spriteVertices =
                    scene_capture{.vertices = vertices_in_draw_order(drawList, firstIndex)}
                        .with_texture(PROBE_TEXTURE_ID)
                        .size();
            });
        });

        REQUIRE(callbackCommands == 0);
        REQUIRE(spriteVertices == 6);
    }
}
