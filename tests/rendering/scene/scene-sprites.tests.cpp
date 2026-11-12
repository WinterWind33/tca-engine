// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/scene-surface.hpp"
#include "rendering/scene/sprite.hpp"
#include "rendering/scene/texture-atlas.hpp"

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
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
using tca::engine::scene::fill_window;
using tca::engine::scene::grid_options;
using tca::engine::scene::pixel_rect;
using tca::engine::scene::scene_painter;
using tca::engine::scene::scene_surface;
using tca::engine::scene::screen_rect;
using tca::engine::scene::sprite_options;
using tca::engine::scene::sprite_region;
using tca::engine::scene::sprite_texture;
using tca::engine::scene::texture_atlas;
using tca::engine::scene::texture_filter;
using tca::engine::scene::texture_id;
using tca::engine::scene::uv_rect;
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

// A sprite's edges are snapped to whole pixels, so each lands within half a pixel of where the
// camera maps it exactly.
constexpr double SNAP_TOLERANCE{0.5 + PIXEL_EPSILON};

constexpr ImVec2 DISPLAY_SIZE{1280.0f, 720.0f};

constexpr ImVec2 HOST_POSITION{40.0f, 30.0f};
constexpr ImVec2 HOST_SIZE{800.0f, 600.0f};
constexpr ImGuiWindowFlags HOST_FLAGS{
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse};

const vector2 SURFACE_SIZE{400.0, 300.0};

// Identifiers no real backend hands out in a headless test, so a draw command carrying one can
// only come from a sprite the test drew.
constexpr texture_id PROBE_TEXTURE_ID{0x5A17E};
constexpr texture_id OTHER_TEXTURE_ID{0x0DDBA11};

constexpr sprite_texture PROBE_TEXTURE{.id = PROBE_TEXTURE_ID, .width = 64, .height = 32};
constexpr sprite_texture OTHER_TEXTURE{.id = OTHER_TEXTURE_ID, .width = 16, .height = 16};

// Colours no default role uses, so a vertex carrying one can only come from what the test asked
// for.
constexpr color PROBE_COLOR{tca::engine::rendering::rgba(11, 22, 33, 255)};
constexpr color OTHER_PROBE_COLOR{tca::engine::rendering::rgba(99, 1, 180, 255)};
constexpr color HALF_TRANSPARENT_PROBE_COLOR{tca::engine::rendering::rgba(40, 80, 160, 128)};

constexpr ImU32 UNTINTED{IM_COL32_WHITE};

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

bool is_whole(const double value) {
    return std::floor(value) == value;
}

world_rect world_box(const double minX, const double minY, const double maxX, const double maxY) {
    return world_rect{
        Eigen::Vector2d{minX, minY},
        Eigen::Vector2d{maxX, maxY}
    };
}

theming::theme theme_with(const color_role role, const color& value) {
    theming::theme customTheme{};
    customTheme.set(role, value);
    return customTheme;
}

// The texture a draw command samples. Read from the reference itself rather than through
// ImDrawCmd::GetTexID(), which asserts on the font atlas's texture when no backend has uploaded it,
// as is the case headlessly: every command that is not a sprite reads as ImTextureID_Invalid.
ImTextureID command_texture(const ImDrawCmd& command) {
    return command.TexRef._TexData != nullptr ? ImTextureID_Invalid : command.TexRef._TexID;
}

// One vertex of what the scene drew, in draw order, with the command it was drawn by.
// The texture filter the backend samples a vertex with, as last set by a sampler callback in the
// draw list. Linear until told otherwise, which is what every backend starts a frame with.
enum class active_sampler {
    linear,
    nearest
};

struct drawn_vertex {
    ImDrawVert vertex{};
    ImTextureID texture{ImTextureID_Invalid};
    screen_rect clipRect{};
    int command{};
    active_sampler sampler{active_sampler::linear};
};

// Everything one render of a surface drew, in the order the GPU will draw it.
struct scene_capture {
    std::vector<drawn_vertex> vertices{};
    screen_rect bounds{};
    int splitterChannelsAfter{};

    std::vector<drawn_vertex> with_texture(const texture_id texture) const {
        std::vector<drawn_vertex> matching{};
        std::ranges::copy_if(vertices, std::back_inserter(matching),
                             [texture](const drawn_vertex& drawn) {
                                 return drawn.texture == static_cast<ImTextureID>(texture);
                             });
        return matching;
    }

    std::vector<drawn_vertex> with_color(const ImU32 wanted) const {
        std::vector<drawn_vertex> matching{};
        std::ranges::copy_if(vertices, std::back_inserter(matching),
                             [wanted](const drawn_vertex& drawn) {
                                 return drawn.vertex.col == wanted;
                             });
        return matching;
    }

    // Where in the draw order something matching predicate is first drawn.
    std::size_t first_index_of(const std::function<bool(const drawn_vertex&)>& predicate) const {
        const auto found{std::ranges::find_if(vertices, predicate)};
        REQUIRE(found != vertices.end());
        return static_cast<std::size_t>(std::distance(vertices.begin(), found));
    }

    std::size_t last_index_of(const std::function<bool(const drawn_vertex&)>& predicate) const {
        const auto found{std::ranges::find_if(vertices.rbegin(), vertices.rend(), predicate)};
        REQUIRE(found != vertices.rend());
        return static_cast<std::size_t>(std::distance(found, vertices.rend()) - 1);
    }

    std::size_t first_index_of_texture(const texture_id texture) const {
        return first_index_of([texture](const drawn_vertex& drawn) {
            return drawn.texture == static_cast<ImTextureID>(texture);
        });
    }

    std::size_t last_index_of_texture(const texture_id texture) const {
        return last_index_of([texture](const drawn_vertex& drawn) {
            return drawn.texture == static_cast<ImTextureID>(texture);
        });
    }

    std::size_t first_index_of_color(const ImU32 wanted) const {
        return first_index_of([wanted](const drawn_vertex& drawn) {
            return drawn.vertex.col == wanted;
        });
    }

    std::size_t last_index_of_color(const ImU32 wanted) const {
        return last_index_of([wanted](const drawn_vertex& drawn) {
            return drawn.vertex.col == wanted;
        });
    }

    std::size_t commands_with_texture(const texture_id texture) const {
        std::set<int> commands{};
        for (const drawn_vertex& drawn : with_texture(texture)) {
            commands.insert(drawn.command);
        }
        return commands.size();
    }
};

screen_rect bounding_box(const std::vector<drawn_vertex>& vertices) {
    screen_rect box{};
    for (const drawn_vertex& drawn : vertices) {
        box.extend(to_eigen(drawn.vertex.pos));
    }
    return box;
}

uv_rect uv_box(const std::vector<drawn_vertex>& vertices) {
    uv_rect box{};
    for (const drawn_vertex& drawn : vertices) {
        box.extend(to_eigen(drawn.vertex.uv));
    }
    return box;
}

// Every vertex drawList holds from firstIndex on, walked through the index buffer command by
// command: the order the GPU draws them in, each vertex repeated once per triangle using it.
//
// Sampler callbacks are followed from the start of the draw list, so each vertex records the filter
// the backend will actually sample it with.
std::vector<drawn_vertex> vertices_in_draw_order(const ImDrawList& drawList, const int firstIndex) {
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
        const int end{begin + static_cast<int>(command.ElemCount)};
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

// How many times the draw list asks the backend to switch sampler through callback, counting
// only the switches at or after firstIndex.
std::size_t sampler_switches(const ImDrawList& drawList, const ImDrawCallback callback,
                             const int firstIndex) {
    std::size_t switches{0};
    for (const ImDrawCmd& command : drawList.CmdBuffer) {
        if (command.UserCallback != nullptr && command.UserCallback == callback &&
            static_cast<int>(command.IdxOffset) >= firstIndex) {
            ++switches;
        }
    }
    return switches;
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

bool sampled_nearest(const drawn_vertex& drawn) {
    return drawn.sampler == active_sampler::nearest;
}

bool sampled_linear(const drawn_vertex& drawn) {
    return drawn.sampler == active_sampler::linear;
}

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
    ImGui::Begin("scene-sprites-tests-host", nullptr, HOST_FLAGS);

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

// Renders surface for one frame, drawing the scene with draw, and returns what it drew.
//
// Sprites may be drawn later than they are asked for (to honour their layers), so what the scene
// drew is read after the surface has rendered rather than around each painter call. The surface
// must have no background and no grid for this to be only the scene: the host draws nothing more
// after it.
scene_capture capture_scene(headless_imgui_context& context, scene_surface& surface,
                            const std::function<void(scene_painter&)>& draw) {
    const scoped_scene_drawing drawing{surface, draw};

    scene_capture capture{};
    run_frame(context, [&] {
        render_in_host([&] {
            const ImDrawList& drawList{*ImGui::GetWindowDrawList()};
            const int firstIndex{drawList.IdxBuffer.Size};

            surface.render();

            capture.vertices = vertices_in_draw_order(drawList, firstIndex);
            capture.bounds = surface.bounds();
            capture.splitterChannelsAfter = drawList._Splitter._Count;
        });
    });
    return capture;
}

// A surface with no background and no grid, so everything it draws is the scene's.
void make_bare(scene_surface& surface) {
    surface.set_background_role(std::nullopt);
    surface.set_grid(std::nullopt);
}

// Where the camera maps a world point to, in the window's screen space, before any snapping.
Eigen::Vector2d exact_screen_point(const screen_rect& bounds, const camera_2d& camera,
                                   const Eigen::Vector2d& world) {
    return bounds.min() + Eigen::Vector2d{camera.world_to_screen(vector2{world})};
}

screen_rect exact_screen_rect(const screen_rect& bounds, const camera_2d& camera,
                              const world_rect& world) {
    return screen_rect{exact_screen_point(bounds, camera, world.min()),
                       exact_screen_point(bounds, camera, world.max())};
}

// A sprite quad: its corners land on whole pixels, within half a pixel of the exact mapping.
void require_snapped_to(const screen_rect& drawn, const screen_rect& exact) {
    REQUIRE(is_whole(drawn.min().x()));
    REQUIRE(is_whole(drawn.min().y()));
    REQUIRE(is_whole(drawn.max().x()));
    REQUIRE(is_whole(drawn.max().y()));
    require_same_rect(drawn, exact, SNAP_TOLERANCE);
}

// The vertex drawn at the given screen corner, to check which UV landed where.
const drawn_vertex& vertex_at(const std::vector<drawn_vertex>& vertices,
                              const Eigen::Vector2d& corner) {
    const auto found{std::ranges::find_if(vertices, [&corner](const drawn_vertex& drawn) {
        return (to_eigen(drawn.vertex.pos) - corner).cwiseAbs().maxCoeff() < PIXEL_EPSILON;
    })};
    REQUIRE(found != vertices.end());
    return *found;
}

// A texture per tile, so every tile of a tiled floor can be told apart in the draw list.
sprite_texture tile_texture(const int column, const int row) {
    return sprite_texture{
        .id = 0x7000 + static_cast<texture_id>(row * 1000 + column), .width = 16, .height = 16};
}

} // namespace

// ================================ Drawing in world space ================================

TEST_CASE("scene_painter draws sprites at a world rectangle through the camera",
          "[tca][engine][rendering][scene][sprites][drawing]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    // Parenthesised rather than braced: GENERATE would split a braced vector2{x, y} at the comma.
    const vector2 cameraPosition =
        GENERATE(vector2::zero(), vector2(25.0, -40.0), vector2(3.25, 7.75));
    const double zoom = GENERATE(1.0, 2.0, 0.5, 1.37);
    CAPTURE(cameraPosition.x(), cameraPosition.y(), zoom);

    surface.camera().set_position(cameraPosition);
    surface.camera().set_zoom(zoom);

    const world_rect spriteRect{world_box(-10.0, -20.0, 30.0, 40.0)};

    SECTION("A sprite covers the screen rectangle of its world rectangle, snapped to pixels") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE);
            })};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE_FALSE(sprite.empty());
        require_snapped_to(bounding_box(sprite),
                           exact_screen_rect(capture.bounds, surface.camera(), spriteRect));
    }

    SECTION("A sprite is one textured quad, with no anti-aliasing fringe around it") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE);
            })};

        // Two triangles; a fringe would add more, and would bleed into a neighbouring tile.
        REQUIRE(capture.vertices.size() == 6);
        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).size() == 6);
    }

    SECTION(
        "A whole texture maps UV (0, 0) to the top-left corner and (1, 1) to the bottom-right") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE);
            })};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        const screen_rect box{bounding_box(sprite)};

        require_near(to_eigen(vertex_at(sprite, box.min()).vertex.uv), Eigen::Vector2d{0.0, 0.0},
                     UV_EPSILON);
        require_near(to_eigen(vertex_at(sprite, box.max()).vertex.uv), Eigen::Vector2d{1.0, 1.0},
                     UV_EPSILON);
        require_near(
            to_eigen(vertex_at(sprite, Eigen::Vector2d{box.max().x(), box.min().y()}).vertex.uv),
            Eigen::Vector2d{1.0, 0.0}, UV_EPSILON);
        require_near(
            to_eigen(vertex_at(sprite, Eigen::Vector2d{box.min().x(), box.max().y()}).vertex.uv),
            Eigen::Vector2d{0.0, 1.0}, UV_EPSILON);
    }

    SECTION("Drawing a texture is drawing its whole-texture region") {
        const scene_capture fromTexture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE);
            })};
        const scene_capture fromRegion{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, sprite_region::whole(PROBE_TEXTURE));
            })};

        const std::vector<drawn_vertex> textureSprite{fromTexture.with_texture(PROBE_TEXTURE_ID)};
        const std::vector<drawn_vertex> regionSprite{fromRegion.with_texture(PROBE_TEXTURE_ID)};
        require_same_rect(bounding_box(regionSprite), bounding_box(textureSprite));
        require_same_rect(uv_box(regionSprite), uv_box(textureSprite), UV_EPSILON);
    }

    SECTION("An untinted sprite is drawn white, showing the texture's own colours") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE);
            })};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(std::ranges::all_of(sprite, [](const drawn_vertex& drawn) {
            return drawn.vertex.col == UNTINTED;
        }));
    }

    SECTION("A sprite's size is in world units: it scales with the zoom") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE);
            })};

        const Eigen::Vector2d size{bounding_box(capture.with_texture(PROBE_TEXTURE_ID)).sizes()};
        // Both edges snap, so the size is off by at most a pixel.
        require_near(size, spriteRect.sizes() * zoom, 1.0 + PIXEL_EPSILON);
    }
}

// ===================================== Atlas regions =====================================

TEST_CASE("scene_painter draws texture atlas regions",
          "[tca][engine][rendering][scene][sprites][atlas]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    // A 256x128 tileset of 32x32 tiles.
    constexpr sprite_texture ATLAS_TEXTURE{.id = PROBE_TEXTURE_ID, .width = 256, .height = 128};
    texture_atlas atlas{ATLAS_TEXTURE};
    atlas.add_region("floor", pixel_rect{.x = 0, .y = 0, .width = 32, .height = 32});
    atlas.add_region("wall", pixel_rect{.x = 32, .y = 0, .width = 32, .height = 32});
    atlas.add_region("desk", pixel_rect{.x = 64, .y = 32, .width = 64, .height = 32});
    atlas.add_region("plant", pixel_rect{.x = 224, .y = 96, .width = 32, .height = 32});

    const double zoom = GENERATE(1.0, 1.5, 0.75);
    CAPTURE(zoom);
    surface.camera().set_zoom(zoom);

    SECTION(
        "A region draws exactly its sub-rectangle of the texture, verified against its pixels") {
        // The desk spans texels [64, 128) x [32, 64) of a 256x128 texture.
        const world_rect deskRect{world_box(0.0, 0.0, 64.0, 32.0)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(deskRect, atlas.region("desk"));
            })};

        const std::vector<drawn_vertex> desk{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE_FALSE(desk.empty());

        const uv_rect drawnUv{uv_box(desk)};
        require_near(drawnUv.min(), Eigen::Vector2d{64.0 / 256.0, 32.0 / 128.0}, UV_EPSILON);
        require_near(drawnUv.max(), Eigen::Vector2d{128.0 / 256.0, 64.0 / 128.0}, UV_EPSILON);
    }

    SECTION("A region's UVs are not flipped: its top-left texel lands on the top-left corner") {
        const world_rect wallRect{world_box(-16.0, -16.0, 16.0, 16.0)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(wallRect, atlas.region("wall"));
            })};

        const std::vector<drawn_vertex> wall{capture.with_texture(PROBE_TEXTURE_ID)};
        const screen_rect box{bounding_box(wall)};
        const uv_rect expected{atlas.region("wall").uv()};

        require_near(to_eigen(vertex_at(wall, box.min()).vertex.uv), expected.min(), UV_EPSILON);
        require_near(to_eigen(vertex_at(wall, box.max()).vertex.uv), expected.max(), UV_EPSILON);
    }

    SECTION("A region at the texture's far corner ends exactly at UV (1, 1)") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(world_box(0.0, 0.0, 32.0, 32.0), atlas.region("plant"));
            })};

        const uv_rect drawnUv{uv_box(capture.with_texture(PROBE_TEXTURE_ID))};
        require_near(drawnUv.min(), Eigen::Vector2d{224.0 / 256.0, 96.0 / 128.0}, UV_EPSILON);
        require_near(drawnUv.max(), Eigen::Vector2d{1.0, 1.0}, UV_EPSILON);
    }

    SECTION(
        "A region is stretched to the world rectangle it is drawn at, whatever its pixel size") {
        // A 64x32-texel region drawn over a 10x40 world rectangle.
        const world_rect squashed{world_box(0.0, 0.0, 10.0, 40.0)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(squashed, atlas.region("desk"));
            })};

        const std::vector<drawn_vertex> desk{capture.with_texture(PROBE_TEXTURE_ID)};
        require_snapped_to(bounding_box(desk),
                           exact_screen_rect(capture.bounds, surface.camera(), squashed));
        require_same_rect(uv_box(desk), atlas.region("desk").uv(), UV_EPSILON);
    }

    SECTION("Drawing a region positions it in the world like a whole texture") {
        const world_rect rect{world_box(12.0, -4.0, 44.0, 28.0)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(rect, atlas.region("floor"));
            })};

        require_snapped_to(bounding_box(capture.with_texture(PROBE_TEXTURE_ID)),
                           exact_screen_rect(capture.bounds, surface.camera(), rect));
    }
}

TEST_CASE("Sprites from one atlas share a draw command",
          "[tca][engine][rendering][scene][sprites][atlas][batching]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    constexpr sprite_texture ATLAS_TEXTURE{.id = PROBE_TEXTURE_ID, .width = 64, .height = 64};
    texture_atlas atlas{ATLAS_TEXTURE};
    atlas.add_region("floor", pixel_rect{.x = 0, .y = 0, .width = 16, .height = 16});
    atlas.add_region("wall", pixel_rect{.x = 16, .y = 0, .width = 16, .height = 16});
    atlas.add_region("door", pixel_rect{.x = 32, .y = 0, .width = 16, .height = 16});

    SECTION("Tiles of different kinds from one atlas, on one layer, need no texture switch") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                const std::vector<std::string> kinds{"floor", "wall", "door"};
                for (int index{0}; index < 30; ++index) {
                    const double x{static_cast<double>(index % 10) * 16.0};
                    const double y{static_cast<double>(index / 10) * 16.0};
                    painter.draw_sprite(world_box(x, y, x + 16.0, y + 16.0),
                                        atlas.region(kinds[static_cast<std::size_t>(index) % 3]));
                }
            })};

        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).size() == 30 * 6);
        REQUIRE(capture.commands_with_texture(PROBE_TEXTURE_ID) == 1);
    }

    SECTION("Alternating between two textures does switch: the check above measures something") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                for (int index{0}; index < 4; ++index) {
                    const double x{static_cast<double>(index) * 16.0};
                    painter.draw_sprite(world_box(x, 0.0, x + 16.0, 16.0),
                                        index % 2 == 0 ? atlas.region("floor")
                                                       : sprite_region::whole(OTHER_TEXTURE));
                }
            })};

        REQUIRE(capture.commands_with_texture(PROBE_TEXTURE_ID) == 2);
        REQUIRE(capture.commands_with_texture(OTHER_TEXTURE_ID) == 2);
    }
}

// ================================= Tracking pan and zoom =================================

TEST_CASE("Sprites track the camera's pan and zoom",
          "[tca][engine][rendering][scene][sprites][camera]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    const world_rect spriteRect{world_box(0.0, 0.0, 32.0, 32.0)};
    auto drawSprite{[&spriteRect](scene_painter& painter) {
        painter.draw_sprite(spriteRect, PROBE_TEXTURE);
    }};

    SECTION("Panning moves the sprite the opposite way, by the offset times the zoom") {
        surface.camera().set_zoom(2.0);
        const scene_capture before{capture_scene(imguiContext, surface, drawSprite)};

        surface.camera().pan(vector2{10.0, -5.0});
        const scene_capture after{capture_scene(imguiContext, surface, drawSprite)};

        const screen_rect beforeBox{bounding_box(before.with_texture(PROBE_TEXTURE_ID))};
        const screen_rect afterBox{bounding_box(after.with_texture(PROBE_TEXTURE_ID))};
        // Whole-pixel offsets at a whole zoom: snapping cannot blur the movement.
        require_near(afterBox.min() - beforeBox.min(), Eigen::Vector2d{-20.0, 10.0});
        require_near(afterBox.sizes(), beforeBox.sizes());
    }

    SECTION("Every frame, the sprite follows wherever the camera is now") {
        for (int frame{0}; frame < 12; ++frame) {
            CAPTURE(frame);
            surface.camera().pan(vector2{3.3, -1.7});
            surface.camera().set_zoom(camera_2d::clamp_zoom(0.6 + 0.23 * frame));

            const scene_capture capture{capture_scene(imguiContext, surface, drawSprite)};

            require_snapped_to(bounding_box(capture.with_texture(PROBE_TEXTURE_ID)),
                               exact_screen_rect(capture.bounds, surface.camera(), spriteRect));
        }
    }

    SECTION("Zooming in scales the sprite about the viewport centre") {
        surface.camera().set_position(vector2{16.0, 16.0});
        const scene_capture before{capture_scene(imguiContext, surface, drawSprite)};

        surface.camera().set_zoom(3.0);
        const scene_capture after{capture_scene(imguiContext, surface, drawSprite)};

        const screen_rect beforeBox{bounding_box(before.with_texture(PROBE_TEXTURE_ID))};
        const screen_rect afterBox{bounding_box(after.with_texture(PROBE_TEXTURE_ID))};
        require_near(afterBox.center(), beforeBox.center(), 1.0);
        require_near(afterBox.sizes(), beforeBox.sizes() * 3.0, 1.0 + PIXEL_EPSILON);
    }

    SECTION("Zooming about a point keeps the sprite corner under it where it was") {
        const scene_capture before{capture_scene(imguiContext, surface, drawSprite)};
        const Eigen::Vector2d cornerOnScreen{
            bounding_box(before.with_texture(PROBE_TEXTURE_ID)).min()};

        // zoom_about() takes viewport coordinates, the painter draws in window coordinates.
        surface.camera().zoom_about(vector2{cornerOnScreen - before.bounds.min()},
                                    camera_2d::clamp_zoom(2.5));
        const scene_capture after{capture_scene(imguiContext, surface, drawSprite)};

        require_near(bounding_box(after.with_texture(PROBE_TEXTURE_ID)).min(), cornerOnScreen,
                     SNAP_TOLERANCE);
    }

    SECTION("A sprite follows the surface wherever the page lays it out") {
        scene_surface lowered{"##lowered-surface", SURFACE_SIZE};
        lowered.set_background_role(std::nullopt);
        const scoped_scene_drawing drawing{lowered, drawSprite};

        scene_capture capture{};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                ImGui::Dummy(ImVec2{0.0f, 57.0f});
                const ImDrawList& drawList{*ImGui::GetWindowDrawList()};
                const int firstIndex{drawList.IdxBuffer.Size};
                lowered.render();
                capture.vertices = vertices_in_draw_order(drawList, firstIndex);
                capture.bounds = lowered.bounds();
            });
        });

        REQUIRE(capture.bounds.min().y() > HOST_POSITION.y + 57.0);
        require_snapped_to(bounding_box(capture.with_texture(PROBE_TEXTURE_ID)),
                           exact_screen_rect(capture.bounds, lowered.camera(), spriteRect));
    }

    SECTION("A full-window surface maps sprites the same way") {
        scene_surface fullWindow{"##full-window-sprites", fill_window};
        fullWindow.set_background_role(std::nullopt);
        fullWindow.camera().set_position(vector2{-7.5, 12.25});
        fullWindow.camera().set_zoom(1.75);

        std::vector<drawn_vertex> drawn{};
        const scoped_scene_drawing drawing{fullWindow, [&](scene_painter& painter) {
                                               drawSprite(painter);
                                           }};
        run_frame(imguiContext, [&] {
            fullWindow.render();
            // The surface's own window is gone from the stack by now; find its draw list by the
            // texture instead of by the current window.
            for (const ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
                std::vector<drawn_vertex> inWindow{vertices_in_draw_order(*window->DrawList, 0)};
                std::ranges::copy_if(
                    inWindow, std::back_inserter(drawn), [](const drawn_vertex& vertex) {
                        return vertex.texture == static_cast<ImTextureID>(PROBE_TEXTURE_ID);
                    });
            }
        });

        REQUIRE_FALSE(drawn.empty());
        require_snapped_to(bounding_box(drawn),
                           exact_screen_rect(fullWindow.bounds(), fullWindow.camera(), spriteRect));
    }
}

// ======================================== Seams ========================================

// The single most likely visual defect of tiled scenes: two tiles sharing a world edge that land
// on different, fractional pixels leave a hairline of background between them (or overlap and
// double-blend). Each tile edge must land on a whole pixel, and two tiles sharing a world edge must
// share the very same pixel edge, at any zoom and any camera position.
TEST_CASE("Adjacent tiles show no seam", "[tca][engine][rendering][scene][sprites][seams]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    constexpr int COLUMNS{8};
    constexpr int ROWS{5};
    constexpr double TILE_SIZE{16.0};

    auto tileRect{[](const int column, const int row) {
        return world_box(column * TILE_SIZE, row * TILE_SIZE, (column + 1) * TILE_SIZE,
                         (row + 1) * TILE_SIZE);
    }};

    auto drawFloor{[&tileRect](scene_painter& painter) {
        for (int row{0}; row < ROWS; ++row) {
            for (int column{0}; column < COLUMNS; ++column) {
                painter.draw_sprite(tileRect(column, row), tile_texture(column, row));
            }
        }
    }};

    // Checks every tile of one captured floor for seams and overlaps.
    //
    // At high zoom part of the floor falls outside the view and is culled: those tiles are skipped,
    // but every tile wholly in view must have been drawn, and at least one must be.
    auto requireSeamless{[&](const scene_capture& capture) {
        const world_rect view{surface.camera().visible_world_rect()};

        std::vector<std::vector<std::optional<screen_rect>>> tiles(
            ROWS, std::vector<std::optional<screen_rect>>(COLUMNS));
        std::size_t tilesDrawn{0};
        for (int row{0}; row < ROWS; ++row) {
            for (int column{0}; column < COLUMNS; ++column) {
                CAPTURE(column, row);
                const std::vector<drawn_vertex> tile{
                    capture.with_texture(tile_texture(column, row).id)};
                if (tile.empty()) {
                    REQUIRE_FALSE(view.contains(tileRect(column, row)));
                    continue;
                }
                REQUIRE(tile.size() == 6);
                ++tilesDrawn;

                tiles[row][column] = bounding_box(tile);
                require_snapped_to(
                    *tiles[row][column],
                    exact_screen_rect(capture.bounds, surface.camera(), tileRect(column, row)));
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
                    const screen_rect& right{*tiles[row][column + 1]};
                    REQUIRE(tile.max().x() == right.min().x());
                    REQUIRE(tile.min().y() == right.min().y());
                    REQUIRE(tile.max().y() == right.max().y());
                }
                if (row + 1 < ROWS && tiles[row + 1][column]) {
                    const screen_rect& below{*tiles[row + 1][column]};
                    REQUIRE(tile.max().y() == below.min().y());
                    REQUIRE(tile.min().x() == below.min().x());
                    REQUIRE(tile.max().x() == below.max().x());
                }
            }
        }
    }};

    SECTION("At a range of fractional zooms and camera positions") {
        const double zoom = GENERATE(0.3, 0.37, 0.5, 0.66, 0.75, 0.9, 1.0, 1.1, 1.25, 1.33, 1.5,
                                     1.75, 2.0, 2.2, 2.5, 3.0, 3.7, 4.4, 7.3);
        // Parenthesised rather than braced: GENERATE would split a braced vector2{x, y} at the
        // comma.
        const vector2 cameraPosition = GENERATE(vector2(64.0, 40.0), vector2(63.3, 41.7),
                                                vector2(0.125, -0.375), vector2(71.51, 39.49));
        CAPTURE(zoom, cameraPosition.x(), cameraPosition.y());

        surface.camera().set_position(cameraPosition);
        surface.camera().set_zoom(zoom);

        requireSeamless(capture_scene(imguiContext, surface, drawFloor));
    }

    SECTION("Across a continuous sweep of zoom and camera position, frame after frame") {
        // A slow zoom-and-drift, the way a player scrolls about: every frame must be seamless, not
        // only the handful of zooms picked above.
        constexpr int FRAMES{240};
        for (int frame{0}; frame < FRAMES; ++frame) {
            const double zoom{0.25 + frame * 0.0173};
            const vector2 cameraPosition{60.0 + frame * 0.137, 38.0 - frame * 0.071};
            CAPTURE(frame, zoom, cameraPosition.x(), cameraPosition.y());

            surface.camera().set_position(cameraPosition);
            surface.camera().set_zoom(camera_2d::clamp_zoom(zoom));

            requireSeamless(capture_scene(imguiContext, surface, drawFloor));
        }
    }

    SECTION("Under a non-1.0 UI scale, where the surface itself may sit on fractional pixels") {
        const float uiScale = GENERATE(1.25f, 1.5f, 0.75f);
        const double zoom = GENERATE(0.5, 1.0, 1.33, 2.5);
        CAPTURE(uiScale, zoom);
        ImGui::GetStyle().FontScaleDpi = uiScale;

        surface.camera().set_position(vector2{63.3, 41.7});
        surface.camera().set_zoom(zoom);

        requireSeamless(capture_scene(imguiContext, surface, drawFloor));
    }

    SECTION("A negative world origin snaps like a positive one: rounding is not truncation") {
        // Truncating towards zero would snap tiles either side of the world origin differently.
        auto tileAroundOrigin{[](const int column, const int row) {
            return world_box((column - 4) * TILE_SIZE, (row - 2) * TILE_SIZE,
                             (column - 3) * TILE_SIZE, (row - 1) * TILE_SIZE);
        }};
        surface.camera().set_position(vector2{0.3, -0.2});
        surface.camera().set_zoom(1.37);

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                for (int row{0}; row < ROWS; ++row) {
                    for (int column{0}; column < COLUMNS; ++column) {
                        painter.draw_sprite(tileAroundOrigin(column, row),
                                            tile_texture(column, row));
                    }
                }
            })};

        for (int column{0}; column + 1 < COLUMNS; ++column) {
            CAPTURE(column);
            const screen_rect left{bounding_box(capture.with_texture(tile_texture(column, 0).id))};
            const screen_rect right{
                bounding_box(capture.with_texture(tile_texture(column + 1, 0).id))};
            REQUIRE(left.max().x() == right.min().x());
            require_snapped_to(left, exact_screen_rect(capture.bounds, surface.camera(),
                                                       tileAroundOrigin(column, 0)));
        }
    }

    SECTION("Tiles smaller than a pixel never turn inside out") {
        surface.camera().set_zoom(camera_2d::MIN_ZOOM);
        surface.camera().set_position(vector2{3.1, 2.9});

        const scene_capture capture{capture_scene(imguiContext, surface, drawFloor)};

        for (int row{0}; row < ROWS; ++row) {
            for (int column{0}; column < COLUMNS; ++column) {
                CAPTURE(column, row);
                const std::vector<drawn_vertex> tile{
                    capture.with_texture(tile_texture(column, row).id)};
                if (tile.empty()) {
                    // Snapped away to nothing is fine; a negative size is not.
                    continue;
                }
                const screen_rect box{bounding_box(tile)};
                REQUIRE(box.min().x() <= box.max().x());
                REQUIRE(box.min().y() <= box.max().y());
            }
        }
    }

    SECTION("Adjacent atlas tiles never reach into each other's texels") {
        // Two neighbouring regions of one atlas, drawn side by side. Each tile's UVs must stay
        // within its own region, or the filter blends the neighbour's texels into the shared edge.
        const scoped_fake_backend_samplers samplers{};

        constexpr sprite_texture ATLAS_TEXTURE{.id = PROBE_TEXTURE_ID, .width = 64, .height = 16};
        texture_atlas atlas{ATLAS_TEXTURE};
        const sprite_region left{
            atlas.add_region("left", pixel_rect{.x = 16, .y = 0, .width = 16, .height = 16})};
        const sprite_region right{
            atlas.add_region("right", pixel_rect{.x = 32, .y = 0, .width = 16, .height = 16})};

        const double zoom = GENERATE(0.37, 0.75, 1.0, 1.33, 2.2, 3.7, 5.5);
        CAPTURE(zoom);
        surface.camera().set_zoom(zoom);
        surface.camera().set_position(vector2{16.3, 7.9});

        std::vector<drawn_vertex> leftTile{};
        std::vector<drawn_vertex> rightTile{};
        for (const bool drawLeft : {true, false}) {
            const scene_capture capture{
                capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                    if (drawLeft) {
                        painter.draw_sprite(world_box(0.0, 0.0, 16.0, 16.0), left);
                    } else {
                        painter.draw_sprite(world_box(16.0, 0.0, 32.0, 16.0), right);
                    }
                })};
            (drawLeft ? leftTile : rightTile) = capture.with_texture(PROBE_TEXTURE_ID);
        }

        const uv_rect leftUv{uv_box(leftTile)};
        const uv_rect rightUv{uv_box(rightTile)};
        REQUIRE(leftUv.max().x() <= left.uv().max().x() + UV_EPSILON);
        REQUIRE(rightUv.min().x() >= right.uv().min().x() - UV_EPSILON);
        REQUIRE(leftUv.min().x() >= left.uv().min().x() - UV_EPSILON);
        REQUIRE(rightUv.max().x() <= right.uv().max().x() + UV_EPSILON);

        // And the two tiles meet on one pixel edge.
        REQUIRE(bounding_box(leftTile).max().x() == bounding_box(rightTile).min().x());

        // Sampled nearest-neighbour, every pixel of a tile reads a texel of its own region: pixel
        // centres sit half a pixel inside the snapped quad, so the UV interpolated there is
        // strictly inside the region and the texel containing it is the region's own. Checked for
        // every pixel column of both tiles, edge columns included.
        const std::vector<std::pair<const std::vector<drawn_vertex>*, const sprite_region*>> tiles{
            {&leftTile,  &left },
            {&rightTile, &right},
        };
        for (const auto& [tile, region] : tiles) {
            REQUIRE(std::ranges::all_of(*tile, sampled_nearest));

            const screen_rect quad{bounding_box(*tile)};
            const uv_rect uv{uv_box(*tile)};
            for (double pixelCentre{quad.min().x() + 0.5}; pixelCentre < quad.max().x();
                 pixelCentre += 1.0) {
                CAPTURE(pixelCentre);
                const double t{(pixelCentre - quad.min().x()) / quad.sizes().x()};
                const double u{uv.min().x() + t * uv.sizes().x()};
                const auto texel{static_cast<std::uint32_t>(std::floor(u * ATLAS_TEXTURE.width))};

                REQUIRE(texel >= region->pixels.x);
                REQUIRE(texel < region->pixels.x + region->pixels.width);
            }
        }
    }
}

// ==================================== Texture filtering ====================================

// Sprites are sampled nearest-neighbour by default: tiles stay crisp at any zoom and, with pixel
// snapped edges and exact UVs, never pick up a neighbouring atlas region's texels. The switch goes
// through ImGui's standard sampler callbacks (ImGuiPlatformIO::DrawCallback_SetSampler*), which
// every backend implements, so the surface needs no backend-specific code for it.
TEST_CASE("Sprites are sampled nearest-neighbour by default",
          "[tca][engine][rendering][scene][sprites][filtering]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    const scoped_fake_backend_samplers samplers{};
    const ImGuiPlatformIO& platformIo{ImGui::GetPlatformIO()};

    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    const world_rect cell{world_box(0.0, 0.0, 16.0, 16.0)};

    SECTION("A surface samples its sprites nearest-neighbour unless told otherwise") {
        REQUIRE(surface.sprite_filter() == texture_filter::nearest);
    }

    SECTION("The filter can be changed to linear, for smooth, non-pixel-art sprites") {
        surface.set_sprite_filter(texture_filter::linear);

        REQUIRE(surface.sprite_filter() == texture_filter::linear);
    }

    SECTION("Every sprite vertex is sampled nearest-neighbour") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE);
                painter.draw_sprite(world_box(16.0, 0.0, 32.0, 16.0), OTHER_TEXTURE,
                                    sprite_options{.layer = 2});
            })};

        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).size() == 6);
        REQUIRE(std::ranges::all_of(capture.with_texture(PROBE_TEXTURE_ID), sampled_nearest));
        REQUIRE(std::ranges::all_of(capture.with_texture(OTHER_TEXTURE_ID), sampled_nearest));
    }

    SECTION("Shapes keep linear sampling: anti-aliased lines rely on it") {
        // ImGui draws thick anti-aliased lines from a pre-baked texture that only reads right
        // filtered linearly, so shapes must not inherit the sprites' sampler, however they
        // interleave with them.
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.fill_rect(cell, PROBE_COLOR);
                painter.draw_sprite(cell, PROBE_TEXTURE);
                painter.draw_line(vector2{0.0, 0.0}, vector2{40.0, 12.0}, OTHER_PROBE_COLOR, 3.0);
                painter.draw_sprite(cell, OTHER_TEXTURE, sprite_options{.layer = -1});
                painter.fill_circle(vector2{20.0, 20.0}, 6.0, HALF_TRANSPARENT_PROBE_COLOR);
            })};

        REQUIRE(std::ranges::all_of(capture.with_color(to_imu32(PROBE_COLOR)), sampled_linear));
        REQUIRE(
            std::ranges::all_of(capture.with_color(to_imu32(OTHER_PROBE_COLOR)), sampled_linear));
        REQUIRE(std::ranges::all_of(capture.with_color(to_imu32(HALF_TRANSPARENT_PROBE_COLOR)),
                                    sampled_linear));
        REQUIRE(std::ranges::all_of(capture.with_texture(PROBE_TEXTURE_ID), sampled_nearest));
        REQUIRE(std::ranges::all_of(capture.with_texture(OTHER_TEXTURE_ID), sampled_nearest));
    }

    SECTION("The background and the grid keep linear sampling") {
        const theming::scoped_active_theme activeTheme{[] {
            theming::theme customTheme{};
            customTheme.set(color_role::surface, PROBE_COLOR);
            customTheme.set(color_role::outline_variant, OTHER_PROBE_COLOR);
            return customTheme;
        }()};
        surface.set_background_role(color_role::surface);
        surface.set_grid(grid_options{.spacing = 8.0});

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = -5});
            })};

        REQUIRE_FALSE(capture.with_color(to_imu32(PROBE_COLOR)).empty());
        REQUIRE(std::ranges::all_of(capture.with_color(to_imu32(PROBE_COLOR)), sampled_linear));
        REQUIRE(
            std::ranges::all_of(capture.with_color(to_imu32(OTHER_PROBE_COLOR)), sampled_linear));
        REQUIRE(std::ranges::all_of(capture.with_texture(PROBE_TEXTURE_ID), sampled_nearest));
    }

    SECTION("Whatever the page draws after the surface is sampled linearly again") {
        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               painter.draw_sprite(cell, PROBE_TEXTURE,
                                                                   sprite_options{.layer = 9});
                                           }};

        std::vector<drawn_vertex> drawn{};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                ImDrawList& drawList{*ImGui::GetWindowDrawList()};
                const int firstIndex{drawList.IdxBuffer.Size};
                surface.render();
                drawList.AddRectFilled(ImVec2{0.0f, 0.0f}, ImVec2{10.0f, 10.0f},
                                       to_imu32(OTHER_PROBE_COLOR));
                drawn = vertices_in_draw_order(drawList, firstIndex);
            });
        });

        const scene_capture capture{.vertices = drawn};
        REQUIRE(std::ranges::all_of(capture.with_texture(PROBE_TEXTURE_ID), sampled_nearest));
        REQUIRE_FALSE(capture.with_color(to_imu32(OTHER_PROBE_COLOR)).empty());
        REQUIRE(
            std::ranges::all_of(capture.with_color(to_imu32(OTHER_PROBE_COLOR)), sampled_linear));
    }

    SECTION("A renderable that throws still hands the page back sampled linearly") {
        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               painter.draw_sprite(cell, PROBE_TEXTURE);
                                               throw std::runtime_error{"scene failure"};
                                           }};

        std::vector<drawn_vertex> drawn{};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                ImDrawList& drawList{*ImGui::GetWindowDrawList()};
                const int firstIndex{drawList.IdxBuffer.Size};
                REQUIRE_THROWS_AS(surface.render(), std::runtime_error);
                drawList.AddRectFilled(ImVec2{0.0f, 0.0f}, ImVec2{10.0f, 10.0f},
                                       to_imu32(OTHER_PROBE_COLOR));
                drawn = vertices_in_draw_order(drawList, firstIndex);
            });
        });

        const scene_capture capture{.vertices = drawn};
        REQUIRE_FALSE(capture.with_color(to_imu32(OTHER_PROBE_COLOR)).empty());
        REQUIRE(
            std::ranges::all_of(capture.with_color(to_imu32(OTHER_PROBE_COLOR)), sampled_linear));
    }

    SECTION("A tiled floor switches sampler once, not once per tile") {
        constexpr sprite_texture ATLAS_TEXTURE{.id = PROBE_TEXTURE_ID, .width = 64, .height = 64};
        texture_atlas atlas{ATLAS_TEXTURE};
        atlas.add_region("floor", pixel_rect{.x = 0, .y = 0, .width = 16, .height = 16});
        atlas.add_region("wall", pixel_rect{.x = 16, .y = 0, .width = 16, .height = 16});

        std::size_t nearestSwitches{};
        std::size_t linearSwitches{};
        std::size_t textureCommands{};
        const scoped_scene_drawing drawing{
            surface, [&](scene_painter& painter) {
                for (int index{0}; index < 40; ++index) {
                    const double x{static_cast<double>(index % 10) * 16.0};
                    const double y{static_cast<double>(index / 10) * 16.0};
                    painter.draw_sprite(world_box(x, y, x + 16.0, y + 16.0),
                                        atlas.region(index % 2 == 0 ? "floor" : "wall"));
                }
            }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                const ImDrawList& drawList{*ImGui::GetWindowDrawList()};
                const int firstIndex{drawList.IdxBuffer.Size};
                surface.render();

                nearestSwitches = sampler_switches(
                    drawList, platformIo.DrawCallback_SetSamplerNearest, firstIndex);
                linearSwitches =
                    sampler_switches(drawList, platformIo.DrawCallback_SetSamplerLinear,
                                     firstIndex) +
                    sampler_switches(drawList, platformIo.DrawCallback_ResetRenderState,
                                     firstIndex);
                textureCommands =
                    scene_capture{.vertices = vertices_in_draw_order(drawList, firstIndex)}
                        .commands_with_texture(PROBE_TEXTURE_ID);
            });
        });

        REQUIRE(nearestSwitches == 1);
        REQUIRE(linearSwitches == 1);
        // The switch does not break the atlas's batching either.
        REQUIRE(textureCommands == 1);
    }

    SECTION("A scene drawing no sprites issues no sampler switch at all") {
        std::size_t switches{};
        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               painter.fill_rect(cell, PROBE_COLOR);
                                               painter.draw_line(vector2{0.0, 0.0},
                                                                 vector2{10.0, 10.0}, PROBE_COLOR);
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                const ImDrawList& drawList{*ImGui::GetWindowDrawList()};
                const int firstIndex{drawList.IdxBuffer.Size};
                surface.render();
                switches = sampler_switches(drawList, platformIo.DrawCallback_SetSamplerNearest,
                                            firstIndex) +
                           sampler_switches(drawList, platformIo.DrawCallback_SetSamplerLinear,
                                            firstIndex) +
                           sampler_switches(drawList, platformIo.DrawCallback_ResetRenderState,
                                            firstIndex);
            });
        });

        REQUIRE(switches == 0);
    }

    SECTION("A linear surface samples its sprites linearly and never switches to nearest") {
        surface.set_sprite_filter(texture_filter::linear);

        std::size_t nearestSwitches{};
        std::vector<drawn_vertex> drawn{};
        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               painter.draw_sprite(cell, PROBE_TEXTURE);
                                           }};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                const ImDrawList& drawList{*ImGui::GetWindowDrawList()};
                const int firstIndex{drawList.IdxBuffer.Size};
                surface.render();
                nearestSwitches = sampler_switches(
                    drawList, platformIo.DrawCallback_SetSamplerNearest, firstIndex);
                drawn = vertices_in_draw_order(drawList, firstIndex);
            });
        });

        const scene_capture capture{.vertices = drawn};
        REQUIRE(nearestSwitches == 0);
        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).size() == 6);
        REQUIRE(std::ranges::all_of(capture.with_texture(PROBE_TEXTURE_ID), sampled_linear));
    }

    SECTION("A filter change takes effect from the next frame") {
        auto drawSprite{[&](scene_painter& painter) {
            painter.draw_sprite(cell, PROBE_TEXTURE);
        }};

        const scene_capture nearest{capture_scene(imguiContext, surface, drawSprite)};
        surface.set_sprite_filter(texture_filter::linear);
        const scene_capture linear{capture_scene(imguiContext, surface, drawSprite)};
        surface.set_sprite_filter(texture_filter::nearest);
        const scene_capture nearestAgain{capture_scene(imguiContext, surface, drawSprite)};

        REQUIRE(std::ranges::all_of(nearest.with_texture(PROBE_TEXTURE_ID), sampled_nearest));
        REQUIRE(std::ranges::all_of(linear.with_texture(PROBE_TEXTURE_ID), sampled_linear));
        REQUIRE(std::ranges::all_of(nearestAgain.with_texture(PROBE_TEXTURE_ID), sampled_nearest));
    }

    SECTION("The filter changes sampling only: geometry and UVs are the same either way") {
        surface.camera().set_zoom(1.37);
        surface.camera().set_position(vector2{3.3, -2.1});
        auto drawSprite{[&](scene_painter& painter) {
            painter.draw_sprite(world_box(-8.0, -8.0, 24.0, 24.0), PROBE_TEXTURE);
        }};

        const scene_capture nearest{capture_scene(imguiContext, surface, drawSprite)};
        surface.set_sprite_filter(texture_filter::linear);
        const scene_capture linear{capture_scene(imguiContext, surface, drawSprite)};

        require_same_rect(bounding_box(nearest.with_texture(PROBE_TEXTURE_ID)),
                          bounding_box(linear.with_texture(PROBE_TEXTURE_ID)));
        require_same_rect(uv_box(nearest.with_texture(PROBE_TEXTURE_ID)),
                          uv_box(linear.with_texture(PROBE_TEXTURE_ID)), UV_EPSILON);
    }
}

TEST_CASE("Sprites still draw on a renderer without sampler callbacks",
          "[tca][engine][rendering][scene][sprites][filtering]") {
    // A backend that predates ImGui's standard sampler callbacks, or the null renderer of a
    // headless application, leaves them unset: the surface must not add a null callback (which
    // ImGui asserts on) and must still draw every sprite, sampled however the backend samples.
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    REQUIRE(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest == nullptr);

    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    int callbackCommands{};
    std::vector<drawn_vertex> drawn{};
    const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                           painter.draw_sprite(world_box(0.0, 0.0, 16.0, 16.0),
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
            drawn = vertices_in_draw_order(drawList, firstIndex);
        });
    });

    REQUIRE(callbackCommands == 0);
    REQUIRE(scene_capture{.vertices = drawn}.with_texture(PROBE_TEXTURE_ID).size() == 6);
}

// ========================================= Tint =========================================

TEST_CASE("Sprites are tinted through a theme role",
          "[tca][engine][rendering][scene][sprites][tint][theming]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    const world_rect spriteRect{world_box(0.0, 0.0, 32.0, 32.0)};

    SECTION("A tinted sprite multiplies in the active theme's colour for the role") {
        const theming::scoped_active_theme activeTheme{theme_with(color_role::error, PROBE_COLOR)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE,
                                    sprite_options{.tint = color_role::error});
            })};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE_FALSE(sprite.empty());
        REQUIRE(std::ranges::all_of(sprite, [](const drawn_vertex& drawn) {
            return drawn.vertex.col == to_imu32(PROBE_COLOR);
        }));
    }

    SECTION("Different roles tint differently, so a valid and an invalid placement tell apart") {
        theming::theme customTheme{};
        customTheme.set(color_role::success, PROBE_COLOR);
        customTheme.set(color_role::error, OTHER_PROBE_COLOR);
        const theming::scoped_active_theme activeTheme{customTheme};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE,
                                    sprite_options{.tint = color_role::success});
                painter.draw_sprite(world_box(32.0, 0.0, 64.0, 32.0), OTHER_TEXTURE,
                                    sprite_options{.tint = color_role::error});
            })};

        REQUIRE(std::ranges::all_of(capture.with_texture(PROBE_TEXTURE_ID),
                                    [](const drawn_vertex& drawn) {
                                        return drawn.vertex.col == to_imu32(PROBE_COLOR);
                                    }));
        REQUIRE(std::ranges::all_of(capture.with_texture(OTHER_TEXTURE_ID),
                                    [](const drawn_vertex& drawn) {
                                        return drawn.vertex.col == to_imu32(OTHER_PROBE_COLOR);
                                    }));
    }

    SECTION("A tint's alpha carries through, so a ghosted placement preview is see-through") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::accent, HALF_TRANSPARENT_PROBE_COLOR)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE,
                                    sprite_options{.tint = color_role::accent});
            })};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE_FALSE(sprite.empty());
        REQUIRE(std::ranges::all_of(sprite, [](const drawn_vertex& drawn) {
            return drawn.vertex.col == to_imu32(HALF_TRANSPARENT_PROBE_COLOR);
        }));
    }

    SECTION("The tint is resolved at draw time, following a theme swapped afterwards") {
        auto drawTinted{[&](scene_painter& painter) {
            painter.draw_sprite(spriteRect, PROBE_TEXTURE,
                                sprite_options{.tint = color_role::warning});
        }};

        {
            const theming::scoped_active_theme first{theme_with(color_role::warning, PROBE_COLOR)};
            const scene_capture capture{capture_scene(imguiContext, surface, drawTinted)};
            REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).front().vertex.col ==
                    to_imu32(PROBE_COLOR));
        }
        {
            const theming::scoped_active_theme second{
                theme_with(color_role::warning, OTHER_PROBE_COLOR)};
            const scene_capture capture{capture_scene(imguiContext, surface, drawTinted)};
            REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).front().vertex.col ==
                    to_imu32(OTHER_PROBE_COLOR));
        }
    }

    SECTION("A tint changes the colour only: geometry and UVs are those of the untinted sprite") {
        const theming::scoped_active_theme activeTheme{theme_with(color_role::error, PROBE_COLOR)};
        surface.camera().set_zoom(1.37);

        const scene_capture plain{capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            painter.draw_sprite(spriteRect, PROBE_TEXTURE);
        })};
        const scene_capture tinted{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(spriteRect, PROBE_TEXTURE,
                                    sprite_options{.tint = color_role::error});
            })};

        require_same_rect(bounding_box(tinted.with_texture(PROBE_TEXTURE_ID)),
                          bounding_box(plain.with_texture(PROBE_TEXTURE_ID)));
        require_same_rect(uv_box(tinted.with_texture(PROBE_TEXTURE_ID)),
                          uv_box(plain.with_texture(PROBE_TEXTURE_ID)), UV_EPSILON);
    }
}

// ====================================== Draw order ======================================

TEST_CASE("Sprites draw in layer order within the surface",
          "[tca][engine][rendering][scene][sprites][layers]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    const world_rect cell{world_box(0.0, 0.0, 16.0, 16.0)};

    SECTION("On one layer, sprites draw in the order they are asked for") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE);
                painter.draw_sprite(cell, OTHER_TEXTURE);
            })};

        REQUIRE(capture.last_index_of_texture(PROBE_TEXTURE_ID) <
                capture.first_index_of_texture(OTHER_TEXTURE_ID));
    }

    SECTION("A wall on a higher layer draws over the floor, even when asked for first") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                // wall
                painter.draw_sprite(cell, OTHER_TEXTURE, sprite_options{.layer = 1});
                // floor
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = 0});
            })};

        REQUIRE(capture.last_index_of_texture(PROBE_TEXTURE_ID) <
                capture.first_index_of_texture(OTHER_TEXTURE_ID));
    }

    SECTION("Interleaved calls are grouped by layer, keeping call order within each layer") {
        // Three layers, asked for in a scrambled order: each sprite gets its own texture so its
        // position in the draw order can be read back.
        struct call {
            int layer;
            texture_id id;
        };
        const std::vector<call> calls{
            {2, 0x9001},
            {0, 0x9002},
            {1, 0x9003},
            {0, 0x9004},
            {2, 0x9005},
            {1, 0x9006},
            {0, 0x9007},
        };

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                for (const call& each : calls) {
                    painter.draw_sprite(cell,
                                        sprite_texture{.id = each.id, .width = 8, .height = 8},
                                        sprite_options{.layer = each.layer});
                }
            })};

        const std::vector<texture_id> expectedOrder{0x9002, 0x9004, 0x9007, 0x9003,
                                                    0x9006, 0x9001, 0x9005};
        for (std::size_t index{0}; index + 1 < expectedOrder.size(); ++index) {
            CAPTURE(index);
            REQUIRE(capture.last_index_of_texture(expectedOrder[index]) <
                    capture.first_index_of_texture(expectedOrder[index + 1]));
        }
    }

    SECTION("Negative layers draw beneath layer 0") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE);
                painter.draw_sprite(cell, OTHER_TEXTURE, sprite_options{.layer = -3});
            })};

        REQUIRE(capture.last_index_of_texture(OTHER_TEXTURE_ID) <
                capture.first_index_of_texture(PROBE_TEXTURE_ID));
    }

    SECTION("Layers far apart still order correctly") {
        constexpr int LOWEST{std::numeric_limits<int>::min()};
        constexpr int HIGHEST{std::numeric_limits<int>::max()};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = HIGHEST});
                painter.draw_sprite(cell, OTHER_TEXTURE, sprite_options{.layer = LOWEST});
            })};

        REQUIRE(capture.last_index_of_texture(OTHER_TEXTURE_ID) <
                capture.first_index_of_texture(PROBE_TEXTURE_ID));
    }

    SECTION("Shapes draw on layer 0: under a higher-layer sprite asked for before them") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = 1});
                painter.fill_rect(cell, PROBE_COLOR);
            })};

        REQUIRE(capture.last_index_of_color(to_imu32(PROBE_COLOR)) <
                capture.first_index_of_texture(PROBE_TEXTURE_ID));
    }

    SECTION("Shapes draw on layer 0: over a negative-layer sprite asked for after them") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.fill_rect(cell, PROBE_COLOR);
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = -1});
            })};

        REQUIRE(capture.last_index_of_texture(PROBE_TEXTURE_ID) <
                capture.first_index_of_color(to_imu32(PROBE_COLOR)));
    }

    SECTION("Shapes and layer-0 sprites keep their call order among themselves") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.fill_rect(cell, PROBE_COLOR);
                painter.draw_sprite(cell, PROBE_TEXTURE);
                painter.fill_rect(cell, OTHER_PROBE_COLOR);
            })};

        REQUIRE(capture.last_index_of_color(to_imu32(PROBE_COLOR)) <
                capture.first_index_of_texture(PROBE_TEXTURE_ID));
        REQUIRE(capture.last_index_of_texture(PROBE_TEXTURE_ID) <
                capture.first_index_of_color(to_imu32(OTHER_PROBE_COLOR)));
    }

    // capture_scene() draws through a renderable added after the one below, so the two are
    // independent objects drawing into one surface in a defined order.
    SECTION("Layers order sprites across renderables, not only within one") {
        const scoped_scene_drawing walls{surface, [&](scene_painter& painter) {
                                             painter.draw_sprite(cell, OTHER_TEXTURE,
                                                                 sprite_options{.layer = 1});
                                         }};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = 0});
            })};

        // The wall's renderable draws first, but its higher layer puts it over the floor.
        REQUIRE(capture.last_index_of_texture(PROBE_TEXTURE_ID) <
                capture.first_index_of_texture(OTHER_TEXTURE_ID));
    }

    SECTION("A later renderable's negative layer draws beneath an earlier one's shapes") {
        const scoped_scene_drawing board{surface, [&](scene_painter& painter) {
                                             painter.fill_rect(cell, PROBE_COLOR);
                                         }};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = -1});
            })};

        REQUIRE(capture.last_index_of_texture(PROBE_TEXTURE_ID) <
                capture.first_index_of_color(to_imu32(PROBE_COLOR)));
    }

    SECTION("On one layer, renderables draw in the order they were added") {
        const scoped_scene_drawing first{surface, [&](scene_painter& painter) {
                                             painter.draw_sprite(cell, PROBE_TEXTURE,
                                                                 sprite_options{.layer = 2});
                                         }};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, OTHER_TEXTURE, sprite_options{.layer = 2});
            })};

        REQUIRE(capture.last_index_of_texture(PROBE_TEXTURE_ID) <
                capture.first_index_of_texture(OTHER_TEXTURE_ID));
    }

    SECTION("The background and the grid stay beneath every layer, even negative ones") {
        const theming::scoped_active_theme activeTheme{[] {
            theming::theme customTheme{};
            customTheme.set(color_role::surface, PROBE_COLOR);
            customTheme.set(color_role::outline_variant, OTHER_PROBE_COLOR);
            return customTheme;
        }()};
        surface.set_background_role(color_role::surface);
        surface.set_grid(grid_options{.spacing = 8.0});

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = -100});
            })};

        REQUIRE(capture.last_index_of_color(to_imu32(PROBE_COLOR)) <
                capture.first_index_of_texture(PROBE_TEXTURE_ID));
        REQUIRE(capture.last_index_of_color(to_imu32(OTHER_PROBE_COLOR)) <
                capture.first_index_of_texture(PROBE_TEXTURE_ID));
    }

    SECTION("Layers do not carry over from one frame to the next") {
        // Frame one leaves a high layer behind; frame two's layer-0 sprite must not be sorted
        // against it.
        (void)capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            painter.draw_sprite(cell, OTHER_TEXTURE, sprite_options{.layer = 5});
        });

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE);
            })};

        REQUIRE(capture.with_texture(OTHER_TEXTURE_ID).empty());
        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).size() == 6);
    }

    SECTION("After the scene, the draw list is back to a single channel for the page") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = 2});
                painter.draw_sprite(cell, OTHER_TEXTURE, sprite_options{.layer = -2});
                painter.fill_rect(cell, PROBE_COLOR);
            })};

        // Whatever the page draws after the surface must land on top of the scene, not in a
        // channel of it.
        REQUIRE(capture.splitterChannelsAfter <= 1);
    }

    SECTION("Drawing into the page after the surface lands on top of every layer") {
        const scoped_scene_drawing drawing{surface, [&](scene_painter& painter) {
                                               painter.draw_sprite(cell, PROBE_TEXTURE,
                                                                   sprite_options{.layer = 50});
                                           }};

        std::vector<drawn_vertex> drawn{};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                ImDrawList& drawList{*ImGui::GetWindowDrawList()};
                const int firstIndex{drawList.IdxBuffer.Size};
                surface.render();
                drawList.AddRectFilled(ImVec2{0.0f, 0.0f}, ImVec2{2000.0f, 2000.0f},
                                       to_imu32(OTHER_PROBE_COLOR));
                drawn = vertices_in_draw_order(drawList, firstIndex);
            });
        });

        const scene_capture capture{.vertices = drawn};
        REQUIRE(capture.last_index_of_texture(PROBE_TEXTURE_ID) <
                capture.first_index_of_color(to_imu32(OTHER_PROBE_COLOR)));
    }

    SECTION("A renderable throwing mid-scene leaves the draw list merged and ImGui balanced") {
        const scoped_scene_drawing drawing{
            surface, [&](scene_painter& painter) {
                painter.draw_sprite(cell, PROBE_TEXTURE, sprite_options{.layer = 3});
                painter.draw_sprite(cell, OTHER_TEXTURE, sprite_options{.layer = -3});
                throw std::runtime_error{"scene failure"};
            }};

        int channelsAfter{};
        int clipRectStackBefore{};
        int clipRectStackAfter{};
        run_frame(imguiContext, [&] {
            render_in_host([&] {
                const ImDrawList& drawList{*ImGui::GetWindowDrawList()};
                clipRectStackBefore = drawList._ClipRectStack.Size;
                REQUIRE_THROWS_AS(surface.render(), std::runtime_error);
                channelsAfter = drawList._Splitter._Count;
                clipRectStackAfter = drawList._ClipRectStack.Size;
            });
        });

        REQUIRE(channelsAfter <= 1);
        REQUIRE(clipRectStackAfter == clipRectStackBefore);
    }
}

// ================================== Clipping and culling ==================================

TEST_CASE("Sprites are clipped to the surface and culled outside the view",
          "[tca][engine][rendering][scene][sprites][clipping]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    SECTION("A sprite reaching past the bounds is drawn clipped to them") {
        // Centred on the viewport's right edge.
        const double halfWidth{SURFACE_SIZE.x() / 2.0};
        const world_rect straddling{world_box(halfWidth - 20.0, -10.0, halfWidth + 20.0, 10.0)};

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(straddling, PROBE_TEXTURE);
            })};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE_FALSE(sprite.empty());
        for (const drawn_vertex& drawn : sprite) {
            REQUIRE(drawn.clipRect.min().x() >= capture.bounds.min().x() - PIXEL_EPSILON);
            REQUIRE(drawn.clipRect.min().y() >= capture.bounds.min().y() - PIXEL_EPSILON);
            REQUIRE(drawn.clipRect.max().x() <= capture.bounds.max().x() + PIXEL_EPSILON);
            REQUIRE(drawn.clipRect.max().y() <= capture.bounds.max().y() + PIXEL_EPSILON);
        }
    }

    SECTION("A sprite entirely outside the visible world issues no geometry at all") {
        // Culled before it reaches the draw list: a big level must not cost vertices for every
        // tile off screen.
        const double zoom = GENERATE(0.5, 1.0, 2.0);
        CAPTURE(zoom);
        surface.camera().set_zoom(zoom);

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                const world_rect view{painter.camera().visible_world_rect()};
                painter.draw_sprite(
                    world_box(view.max().x() + 1.0, 0.0, view.max().x() + 33.0, 32.0),
                    PROBE_TEXTURE);
                painter.draw_sprite(
                    world_box(view.min().x() - 33.0, 0.0, view.min().x() - 1.0, 32.0),
                    PROBE_TEXTURE);
                painter.draw_sprite(
                    world_box(0.0, view.max().y() + 1.0, 32.0, view.max().y() + 33.0),
                    PROBE_TEXTURE);
                painter.draw_sprite(
                    world_box(0.0, view.min().y() - 33.0, 32.0, view.min().y() - 1.0),
                    PROBE_TEXTURE);
            })};

        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).empty());
    }

    SECTION("A sprite overlapping the view by a sliver is still drawn") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                const world_rect view{painter.camera().visible_world_rect()};
                painter.draw_sprite(
                    world_box(view.max().x() - 2.0, 0.0, view.max().x() + 30.0, 32.0),
                    PROBE_TEXTURE);
            })};

        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).size() == 6);
    }

    SECTION("A 10,000-tile level only costs vertices for the tiles in view") {
        surface.camera().set_zoom(1.0);
        surface.camera().set_position(vector2{0.0, 0.0});

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                for (int row{-50}; row < 50; ++row) {
                    for (int column{-50}; column < 50; ++column) {
                        painter.draw_sprite(world_box(column * 32.0, row * 32.0,
                                                      (column + 1) * 32.0, (row + 1) * 32.0),
                                            PROBE_TEXTURE);
                    }
                }
            })};

        // 400x300 pixels at zoom 1 shows at most 14x11 tiles of 32 (13.5 and 10.4 rounded up,
        // plus one partial tile each side).
        const std::size_t tilesDrawn{capture.with_texture(PROBE_TEXTURE_ID).size() / 6};
        REQUIRE(tilesDrawn > 0);
        REQUIRE(tilesDrawn <= 15 * 12);
    }
}

TEST_CASE("Sprites that cannot be drawn draw nothing",
          "[tca][engine][rendering][scene][sprites][invalid]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    SECTION("An empty world rectangle draws nothing") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(world_rect{}, PROBE_TEXTURE);
            })};

        REQUIRE(capture.vertices.empty());
    }

    SECTION("A zero-area world rectangle draws nothing") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(world_box(4.0, 4.0, 4.0, 20.0), PROBE_TEXTURE);
                painter.draw_sprite(world_box(4.0, 4.0, 20.0, 4.0), PROBE_TEXTURE);
            })};

        REQUIRE(capture.vertices.empty());
    }

    SECTION("An empty texture draws nothing, like an empty image") {
        const sprite_texture emptyTexture =
            GENERATE(sprite_texture{}, sprite_texture{.id = 0, .width = 8, .height = 8},
                     sprite_texture{.id = 9, .width = 0, .height = 8});
        CAPTURE(emptyTexture.id, emptyTexture.width, emptyTexture.height);

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(world_box(0.0, 0.0, 16.0, 16.0), emptyTexture);
            })};

        REQUIRE(capture.vertices.empty());
    }

    SECTION("A non-finite world rectangle draws nothing") {
        const double bad = GENERATE(std::numeric_limits<double>::quiet_NaN(),
                                    std::numeric_limits<double>::infinity());
        CAPTURE(bad);

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(world_box(0.0, 0.0, bad, 16.0), PROBE_TEXTURE);
            })};

        REQUIRE(capture.vertices.empty());
    }

    SECTION("A sprite that draws nothing does not disturb the ones around it") {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(world_box(0.0, 0.0, 16.0, 16.0), PROBE_TEXTURE);
                painter.draw_sprite(world_rect{}, OTHER_TEXTURE, sprite_options{.layer = 4});
                painter.draw_sprite(world_box(16.0, 0.0, 32.0, 16.0), PROBE_TEXTURE);
            })};

        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).size() == 12);
        REQUIRE(capture.with_texture(OTHER_TEXTURE_ID).empty());
    }
}

// ================================ Compile-time contract ================================

static_assert(sizeof(ImTextureID) <= sizeof(texture_id),
              "every backend's ImTextureID fits the engine's opaque texture identifier");

// ================================== Transforms ==================================

namespace {

// A camera at the centre of a SURFACE_SIZE viewport, unzoomed: world points land on the surface at
// the same coordinates, so expected quads can be written down as plain pixel numbers.
const vector2 IDENTITY_CAMERA_POSITION{SURFACE_SIZE / 2.0};

// A 32x48 character standing at (100, 50)-(132, 98), its pivot at its feet but off-centre, as an
// asymmetric sprite's would be: a flip that kept the rectangle's centre fixed would land elsewhere.
const world_rect CHARACTER_RECT{world_box(100.0, 50.0, 132.0, 98.0)};
const vector2 CHARACTER_FEET{8.0, 48.0};

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

// A point given in surface pixels, moved into the window's screen space. The surface sits on whole
// pixels in the host, so a whole surface pixel stays a whole screen pixel.
Eigen::Vector2d on_surface(const scene_capture& capture, const double x, const double y) {
    REQUIRE(is_whole(capture.bounds.min().x()));
    REQUIRE(is_whole(capture.bounds.min().y()));
    return capture.bounds.min() + Eigen::Vector2d{x, y};
}

screen_rect on_surface(const scene_capture& capture, const double minX, const double minY,
                       const double maxX, const double maxY) {
    return screen_rect{on_surface(capture, minX, minY), on_surface(capture, maxX, maxY)};
}

sprite_quad on_surface(const scene_capture& capture, const sprite_quad& surfaceQuad) {
    const Eigen::Vector2d offset{on_surface(capture, 0.0, 0.0)};
    return sprite_quad{
        .topLeftUv = offset + surfaceQuad.topLeftUv,
        .topRightUv = offset + surfaceQuad.topRightUv,
        .bottomRightUv = offset + surfaceQuad.bottomRightUv,
        .bottomLeftUv = offset + surfaceQuad.bottomLeftUv,
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
// origin, then turned about it, then mapped through the camera.
Eigen::Vector2d exact_transformed_corner(const screen_rect& bounds, const camera_2d& camera,
                                         const world_rect& rect, const sprite_options& options,
                                         const Eigen::Vector2d& uvCorner) {
    const Eigen::Vector2d pivot{rect.min() + Eigen::Vector2d{options.origin}};
    const Eigen::Vector2d corner{rect.min() + rect.sizes().cwiseProduct(uvCorner)};
    const Eigen::Vector2d scaled{(corner - pivot).cwiseProduct(Eigen::Vector2d{options.scale})};
    const Eigen::Rotation2Dd rotation{options.rotation * std::numbers::pi / 180.0};
    return exact_screen_point(bounds, camera, pivot + rotation * scaled);
}

sprite_quad exact_transformed_quad(const screen_rect& bounds, const camera_2d& camera,
                                   const world_rect& rect, const sprite_options& options) {
    return sprite_quad{
        .topLeftUv = exact_transformed_corner(bounds, camera, rect, options, UV_TOP_LEFT),
        .topRightUv = exact_transformed_corner(bounds, camera, rect, options, UV_TOP_RIGHT),
        .bottomRightUv = exact_transformed_corner(bounds, camera, rect, options, UV_BOTTOM_RIGHT),
        .bottomLeftUv = exact_transformed_corner(bounds, camera, rect, options, UV_BOTTOM_LEFT),
    };
}

bool every_vertex_on_whole_pixels(const std::vector<drawn_vertex>& vertices) {
    return std::ranges::all_of(vertices, [](const drawn_vertex& drawn) {
        return is_whole(drawn.vertex.pos.x) && is_whole(drawn.vertex.pos.y);
    });
}

bool every_vertex_colored(const std::vector<drawn_vertex>& vertices, const ImU32 wanted) {
    return std::ranges::all_of(vertices, [wanted](const drawn_vertex& drawn) {
        return drawn.vertex.col == wanted;
    });
}

unsigned int alpha_of(const drawn_vertex& drawn) {
    return (drawn.vertex.col >> IM_COL32_A_SHIFT) & 0xFFu;
}

// Drawing no geometry at all is just as invisible as drawing it fully transparent.
bool every_vertex_invisible(const std::vector<drawn_vertex>& vertices) {
    return std::ranges::all_of(vertices, [](const drawn_vertex& drawn) {
        return alpha_of(drawn) == 0u;
    });
}

void center_identity_camera(scene_surface& surface) {
    surface.camera().set_position(IDENTITY_CAMERA_POSITION);
    surface.camera().set_zoom(1.0);
}

} // namespace

TEST_CASE("Sprites flip about their origin",
          "[tca][engine][rendering][scene][sprites][transforms][flip]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);
    center_identity_camera(surface);

    auto drawCharacter{[&](const sprite_options& options) {
        return capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            painter.draw_sprite(CHARACTER_RECT, PROBE_TEXTURE, options);
        });
    }};

    SECTION("An origin on its own moves nothing: the sprite covers its world rectangle") {
        const scene_capture capture{drawCharacter(sprite_options{.origin = CHARACTER_FEET})};

        require_quad(quad_of(capture.with_texture(PROBE_TEXTURE_ID)),
                     on_surface(capture, sprite_quad{
                                             .topLeftUv = {100.0, 50.0},
                                             .topRightUv = {132.0, 50.0},
                                             .bottomRightUv = {132.0, 98.0},
                                             .bottomLeftUv = {100.0, 98.0},
        }));
    }

    SECTION("A horizontal flip mirrors the sprite about its origin's x, which stays put") {
        // The feet are 8 pixels from the left edge; flipped, they are 8 pixels from the right one.
        const scene_capture capture{
            drawCharacter(sprite_options{.origin = CHARACTER_FEET, .scale = vector2{-1.0, 1.0}}
            )
        };

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        require_same_rect(bounding_box(sprite), on_surface(capture, 84.0, 50.0, 116.0, 98.0));
        require_quad(quad_of(sprite), on_surface(capture, sprite_quad{
                                                              .topLeftUv = {116.0, 50.0},
                                                              .topRightUv = {84.0,  50.0},
                                                              .bottomRightUv = {84.0,  98.0},
                                                              .bottomLeftUv = {116.0, 98.0},
        }));
    }

    SECTION("A vertical flip mirrors the sprite about its origin's y, which stays put") {
        const scene_capture capture{
            drawCharacter(sprite_options{.origin = CHARACTER_FEET, .scale = vector2{1.0, -1.0}}
            )
        };

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        require_same_rect(bounding_box(sprite), on_surface(capture, 100.0, 98.0, 132.0, 146.0));
        require_quad(quad_of(sprite), on_surface(capture, sprite_quad{
                                                              .topLeftUv = {100.0, 146.0},
                                                              .topRightUv = {132.0, 146.0},
                                                              .bottomRightUv = {132.0, 98.0 },
                                                              .bottomLeftUv = {100.0, 98.0 },
        }));
    }

    SECTION("Flipping both axes mirrors the sprite through its origin") {
        const scene_capture capture{
            drawCharacter(sprite_options{.origin = CHARACTER_FEET, .scale = vector2{-1.0, -1.0}}
            )
        };

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        require_same_rect(bounding_box(sprite), on_surface(capture, 84.0, 98.0, 116.0, 146.0));
        require_near(quad_of(sprite).topLeftUv, on_surface(capture, 116.0, 146.0));
        require_near(quad_of(sprite).bottomRightUv, on_surface(capture, 84.0, 98.0));
    }

    SECTION("A scale other than one stretches the sprite about its origin too") {
        // The feet's column and row stay put: the 8 pixels left of them become 16 on their right,
        // the 24 on their right become 48 on their left, and the 48 above them shrink to 24.
        const scene_capture capture{
            drawCharacter(sprite_options{.origin = CHARACTER_FEET, .scale = vector2{-2.0, 0.5}}
            )
        };

        require_same_rect(bounding_box(capture.with_texture(PROBE_TEXTURE_ID)),
                          on_surface(capture, 60.0, 74.0, 124.0, 98.0));
    }

    SECTION("A flip only swaps UVs: the same texels, still one quad with no fringe") {
        const scene_capture plain{drawCharacter(sprite_options{.origin = CHARACTER_FEET})};
        const scene_capture flipped{
            drawCharacter(sprite_options{.origin = CHARACTER_FEET, .scale = vector2{-1.0, 1.0}}
            )
        };

        REQUIRE(flipped.vertices.size() == 6);
        REQUIRE(flipped.with_texture(PROBE_TEXTURE_ID).size() == 6);
        require_same_rect(uv_box(flipped.with_texture(PROBE_TEXTURE_ID)),
                          uv_box(plain.with_texture(PROBE_TEXTURE_ID)), UV_EPSILON);
    }

    SECTION("A flipped atlas region keeps to its own texels") {
        constexpr sprite_texture ATLAS_TEXTURE{.id = PROBE_TEXTURE_ID, .width = 256, .height = 128};
        texture_atlas atlas{ATLAS_TEXTURE};
        atlas.add_region("hero", pixel_rect{.x = 64, .y = 32, .width = 32, .height = 48});

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(CHARACTER_RECT, atlas.region("hero"),
                                    sprite_options{
                                        .origin = CHARACTER_FEET, .scale = vector2{-1.0, 1.0}
                });
            })};

        const std::vector<drawn_vertex> hero{capture.with_texture(PROBE_TEXTURE_ID)};
        const uv_rect expected{atlas.region("hero").uv()};
        require_same_rect(uv_box(hero), expected, UV_EPSILON);
        // The region's top-left texel is now drawn top-right.
        require_near(to_eigen(vertex_at(hero, on_surface(capture, 116.0, 50.0)).vertex.uv),
                     expected.min(), UV_EPSILON);
    }

    SECTION("Through any camera, a flipped sprite lands on whole pixels about its origin") {
        // Parenthesised rather than braced: GENERATE would split a braced vector2{x, y} at the
        // comma.
        const vector2 cameraPosition =
            GENERATE(vector2(200.0, 150.0), vector2(103.25, 57.75), vector2(141.5, 112.125));
        const double zoom = GENERATE(1.0, 2.0, 0.5, 1.37);
        const vector2 scale = GENERATE(vector2(-1.0, 1.0), vector2(1.0, -1.0), vector2(-1.0, -1.0),
                                       vector2(-1.5, 2.0));
        CAPTURE(cameraPosition.x(), cameraPosition.y(), zoom, scale.x(), scale.y());
        surface.camera().set_position(cameraPosition);
        surface.camera().set_zoom(zoom);

        const sprite_options options{.origin = CHARACTER_FEET, .scale = scale};
        const scene_capture capture{drawCharacter(options)};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(every_vertex_on_whole_pixels(sprite));
        require_quad(
            quad_of(sprite),
            exact_transformed_quad(capture.bounds, surface.camera(), CHARACTER_RECT, options),
            SNAP_TOLERANCE);
    }
}

TEST_CASE("Sprites rotate about their origin",
          "[tca][engine][rendering][scene][sprites][transforms][rotation]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);
    center_identity_camera(surface);

    auto drawCharacter{[&](const sprite_options& options) {
        return capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            painter.draw_sprite(CHARACTER_RECT, PROBE_TEXTURE, options);
        });
    }};

    SECTION("0, 90, 180 and 270 degrees produce exactly the expected quads") {
        // The feet are at (108, 98). Relative to them the corners are (-8, -48), (24, -48),
        // (24, 0) and (-8, 0); each positive quarter turn takes (x, y) to (-y, x).
        struct quarter_turn {
            double degrees{};
            sprite_quad quad{};
        };
        const sprite_quad upright{
            {100.0, 50.0},
            {132.0, 50.0},
            {132.0, 98.0},
            {100.0, 98.0}
        };
        const sprite_quad quarter{
            {156.0, 90.0 },
            {156.0, 122.0},
            {108.0, 122.0},
            {108.0, 90.0 }
        };
        const sprite_quad half{
            {116.0, 146.0},
            {84.0,  146.0},
            {84.0,  98.0 },
            {116.0, 98.0 }
        };
        const sprite_quad threeQuarters{
            {60.0,  106.0},
            {60.0,  74.0 },
            {108.0, 74.0 },
            {108.0, 106.0}
        };

        const std::array<quarter_turn, 8> turns{
            {
             {.degrees = 0.0, .quad = upright},
             {.degrees = 90.0, .quad = quarter},
             {.degrees = 180.0, .quad = half},
             {.degrees = 270.0, .quad = threeQuarters},
             // Whole turns and negative angles land on the same quarter turns.
                {.degrees = 360.0, .quad = upright},
             {.degrees = 450.0, .quad = quarter},
             {.degrees = -90.0, .quad = threeQuarters},
             {.degrees = -180.0, .quad = half},
             }
        };

        for (const quarter_turn& turn : turns) {
            CAPTURE(turn.degrees);
            const scene_capture capture{
                drawCharacter(sprite_options{.origin = CHARACTER_FEET, .rotation = turn.degrees})};

            const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
            REQUIRE(every_vertex_on_whole_pixels(sprite));
            require_quad(quad_of(sprite), on_surface(capture, turn.quad));
        }
    }

    SECTION("A positive angle turns clockwise on screen, since y points down") {
        const scene_capture capture{
            drawCharacter(sprite_options{.origin = CHARACTER_FEET, .rotation = 90.0})};

        // Upright, the head is above the feet; a clockwise quarter turn points it to their right.
        const sprite_quad quad{quad_of(capture.with_texture(PROBE_TEXTURE_ID))};
        const Eigen::Vector2d feet{on_surface(capture, 108.0, 98.0)};
        REQUIRE(quad.topLeftUv.x() > feet.x());
        REQUIRE(quad.topRightUv.x() > feet.x());
    }

    SECTION("Quarter turns stay on whole pixels through any camera") {
        const double degrees = GENERATE(90.0, 180.0, 270.0, -90.0);
        const double zoom = GENERATE(0.5, 1.37, 2.2);
        // Parenthesised rather than braced: GENERATE would split a braced vector2{x, y} at the
        // comma.
        const vector2 cameraPosition = GENERATE(vector2(113.25, 70.5), vector2(99.9, 101.3));
        CAPTURE(degrees, zoom, cameraPosition.x(), cameraPosition.y());
        surface.camera().set_position(cameraPosition);
        surface.camera().set_zoom(zoom);

        const sprite_options options{.origin = CHARACTER_FEET, .rotation = degrees};
        const scene_capture capture{drawCharacter(options)};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(every_vertex_on_whole_pixels(sprite));
        require_quad(
            quad_of(sprite),
            exact_transformed_quad(capture.bounds, surface.camera(), CHARACTER_RECT, options),
            SNAP_TOLERANCE);
    }

    SECTION("Any angle turns the sprite about its origin, through any camera") {
        const double degrees = GENERATE(30.0, 45.0, -60.0, 137.5, 359.0);
        const double zoom = GENERATE(1.0, 2.0, 0.75, 1.37);
        // Parenthesised rather than braced: GENERATE would split a braced vector2{x, y} at the
        // comma.
        const vector2 cameraPosition = GENERATE(vector2(200.0, 150.0), vector2(113.25, 70.5));
        CAPTURE(degrees, zoom, cameraPosition.x(), cameraPosition.y());
        surface.camera().set_position(cameraPosition);
        surface.camera().set_zoom(zoom);

        const sprite_options options{.origin = CHARACTER_FEET, .rotation = degrees};
        const scene_capture capture{drawCharacter(options)};

        require_quad(
            quad_of(capture.with_texture(PROBE_TEXTURE_ID)),
            exact_transformed_quad(capture.bounds, surface.camera(), CHARACTER_RECT, options),
            SNAP_TOLERANCE);
    }

    SECTION("A rotated sprite is still a single quad, with no anti-aliasing fringe") {
        const scene_capture capture{
            drawCharacter(sprite_options{.origin = CHARACTER_FEET, .rotation = 30.0})};

        REQUIRE(capture.vertices.size() == 6);
        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).size() == 6);
    }

    SECTION("A weapon swings about its grip, which never moves") {
        // A 40x8 sword gripped 6 pixels in from its left end, at (156, 124), swung through a full
        // circle: the middle of its far end stays 34 pixels from the grip all the way round.
        const world_rect sword{world_box(150.0, 120.0, 190.0, 128.0)};
        const vector2 grip{6.0, 4.0};

        for (int step{0}; step < 24; ++step) {
            const double degrees{step * 15.0};
            CAPTURE(degrees);
            const scene_capture capture{
                capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                    painter.draw_sprite(sword, PROBE_TEXTURE,
                                        sprite_options{.origin = grip, .rotation = degrees});
                })};

            const sprite_quad quad{quad_of(capture.with_texture(PROBE_TEXTURE_ID))};
            const Eigen::Vector2d gripOnScreen{on_surface(capture, 156.0, 124.0)};
            const Eigen::Vector2d hiltMiddle{(quad.topLeftUv + quad.bottomLeftUv) / 2.0};
            const Eigen::Vector2d tipMiddle{(quad.topRightUv + quad.bottomRightUv) / 2.0};
            require_near((tipMiddle - gripOnScreen).norm(), 34.0, 2.0 * SNAP_TOLERANCE);
            require_near((hiltMiddle - gripOnScreen).norm(), 6.0, 2.0 * SNAP_TOLERANCE);
        }
    }

    SECTION("The scale applies before the turn: a flipped sprite turns as its mirror image") {
        // (-8, -48) from the feet, flipped to (8, -48), turned a quarter to (48, 8).
        const scene_capture capture{
            drawCharacter(sprite_options{
                                         .origin = CHARACTER_FEET, .scale = vector2{-1.0, 1.0}, .rotation = 90.0}
            )
        };

        require_near(quad_of(capture.with_texture(PROBE_TEXTURE_ID)).topLeftUv,
                     on_surface(capture, 156.0, 106.0));
    }

    SECTION("A rotated atlas region keeps to its own texels") {
        constexpr sprite_texture ATLAS_TEXTURE{.id = PROBE_TEXTURE_ID, .width = 256, .height = 128};
        texture_atlas atlas{ATLAS_TEXTURE};
        atlas.add_region("hero", pixel_rect{.x = 64, .y = 32, .width = 32, .height = 48});

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(CHARACTER_RECT, atlas.region("hero"),
                                    sprite_options{.origin = CHARACTER_FEET, .rotation = 30.0});
            })};

        require_same_rect(uv_box(capture.with_texture(PROBE_TEXTURE_ID)), atlas.region("hero").uv(),
                          UV_EPSILON);
    }
}

TEST_CASE("Transformed sprites are culled by where they end up",
          "[tca][engine][rendering][scene][sprites][transforms][culling]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);
    // The visible world is (0, 0)-(400, 300).
    center_identity_camera(surface);

    SECTION("A sprite whose rectangle is out of view but whose transform brings it in is drawn") {
        // Left of the view, pivoting on the middle of its right edge, which is on the view's left
        // edge: flipped or turned half way, it lands at (0, 100)-(40, 132).
        const world_rect outside{world_box(-40.0, 100.0, 0.0, 132.0)};
        const bool flip = GENERATE(true, false);
        CAPTURE(flip);
        const sprite_options options{
            flip ? sprite_options{.origin = vector2{40.0, 16.0}, .scale = vector2{-1.0, 1.0}}
                 : sprite_options{.origin = vector2{40.0, 16.0}, .rotation = 180.0          }
        };

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(outside, PROBE_TEXTURE, options);
            })};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(sprite.size() == 6);
        require_same_rect(bounding_box(sprite), on_surface(capture, 0.0, 100.0, 40.0, 132.0));
    }

    SECTION("A sprite whose rectangle is in view but whose transform takes it out is culled") {
        // Pivoting on the middle of its left edge, which is on the view's left edge: flipped, it
        // lands at (-40, 100)-(0, 132).
        const world_rect inside{world_box(0.0, 100.0, 40.0, 132.0)};
        const sprite_options options{
            .origin = vector2{0.0,  16.0},
              .scale = vector2{-1.0, 1.0 }
        };

        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(inside, PROBE_TEXTURE, options);
                painter.draw_sprite(world_box(200.0, 100.0, 240.0, 132.0), OTHER_TEXTURE);
            })};

        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).empty());
        REQUIRE(capture.with_texture(OTHER_TEXTURE_ID).size() == 6);
    }
}

TEST_CASE("Sprite transforms that cannot be drawn draw nothing",
          "[tca][engine][rendering][scene][sprites][transforms][degenerate]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);
    center_identity_camera(surface);

    constexpr double NOT_A_NUMBER{std::numeric_limits<double>::quiet_NaN()};
    constexpr double INFINITE{std::numeric_limits<double>::infinity()};

    auto requireNothingDrawn{[&](const sprite_options& options) {
        const scene_capture capture{
            capture_scene(imguiContext, surface, [&](scene_painter& painter) {
                painter.draw_sprite(world_box(200.0, 20.0, 232.0, 52.0), OTHER_TEXTURE);
                painter.draw_sprite(CHARACTER_RECT, PROBE_TEXTURE, options);
                painter.draw_sprite(world_box(240.0, 20.0, 272.0, 52.0), OTHER_TEXTURE);
            })};

        REQUIRE(capture.with_texture(PROBE_TEXTURE_ID).empty());
        // The sprites around it still draw.
        REQUIRE(capture.with_texture(OTHER_TEXTURE_ID).size() == 12);
    }};

    SECTION("A zero scale on either axis draws nothing") {
        const vector2 scale =
            GENERATE(vector2(0.0, 1.0), vector2(1.0, 0.0), vector2(0.0, 0.0), vector2(-0.0, -1.0));
        CAPTURE(scale.x(), scale.y());
        requireNothingDrawn(sprite_options{.origin = CHARACTER_FEET, .scale = scale});
    }

    SECTION("A non-finite scale draws nothing") {
        const vector2 scale = GENERATE_COPY(vector2(NOT_A_NUMBER, 1.0), vector2(1.0, INFINITE),
                                            vector2(-INFINITE, 1.0));
        CAPTURE(scale.x(), scale.y());
        requireNothingDrawn(sprite_options{.scale = scale});
    }

    SECTION("A non-finite origin draws nothing") {
        const vector2 origin = GENERATE_COPY(vector2(NOT_A_NUMBER, 0.0), vector2(0.0, INFINITE));
        CAPTURE(origin.x(), origin.y());
        requireNothingDrawn(sprite_options{
            .origin = origin, .scale = vector2{-1.0, 1.0}
        });
    }

    SECTION("A non-finite rotation draws nothing") {
        const double degrees = GENERATE_COPY(NOT_A_NUMBER, INFINITE, -INFINITE);
        CAPTURE(degrees);
        requireNothingDrawn(sprite_options{.origin = CHARACTER_FEET, .rotation = degrees});
    }
}

TEST_CASE("Flipped and quarter-turned tiles snap like plain ones, showing no seam",
          "[tca][engine][rendering][scene][sprites][transforms][seams]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);

    constexpr int COLUMNS{12};
    constexpr double TILE_SIZE{16.0};
    const vector2 tileCentre{TILE_SIZE / 2.0, TILE_SIZE / 2.0};

    auto tileRect{[](const int column) {
        return world_box(column * TILE_SIZE, 0.0, (column + 1) * TILE_SIZE, TILE_SIZE);
    }};

    // A mirrored floor: every tile flipped or turned about its centre, which keeps each one over
    // the square it would cover untransformed.
    auto tileOptions{[&tileCentre](const int column) {
        switch (column % 4) {
            case 0:
                return sprite_options{
                    .origin = tileCentre, .scale = vector2{-1.0, 1.0}
                };
            case 1:
                return sprite_options{.origin = tileCentre, .rotation = 90.0};
            case 2:
                return sprite_options{
                    .origin = tileCentre, .scale = vector2{1.0, -1.0}
                };
            default:
                return sprite_options{.origin = tileCentre, .rotation = 270.0};
        }
    }};

    const double zoom = GENERATE(0.37, 0.75, 1.0, 1.33, 2.2, 3.7);
    // Parenthesised rather than braced: GENERATE would split a braced vector2{x, y} at the comma.
    const vector2 cameraPosition = GENERATE(vector2(96.0, 8.0), vector2(95.3, 7.7));
    CAPTURE(zoom, cameraPosition.x(), cameraPosition.y());
    surface.camera().set_position(cameraPosition);
    surface.camera().set_zoom(zoom);

    const scene_capture capture{capture_scene(imguiContext, surface, [&](scene_painter& painter) {
        for (int column{0}; column < COLUMNS; ++column) {
            painter.draw_sprite(tileRect(column), tile_texture(column, 0), tileOptions(column));
        }
    })};

    std::size_t tilesDrawn{0};
    std::optional<screen_rect> previous{};
    for (int column{0}; column < COLUMNS; ++column) {
        CAPTURE(column);
        const std::vector<drawn_vertex> tile{capture.with_texture(tile_texture(column, 0).id)};
        if (tile.empty()) {
            REQUIRE_FALSE(surface.camera().visible_world_rect().contains(tileRect(column)));
            previous.reset();
            continue;
        }
        REQUIRE(tile.size() == 6);
        ++tilesDrawn;

        const screen_rect box{bounding_box(tile)};
        require_snapped_to(box,
                           exact_screen_rect(capture.bounds, surface.camera(), tileRect(column)));
        // Compared exactly: a gap or an overlap of any size is a seam.
        if (previous) {
            REQUIRE(previous->max().x() == box.min().x());
            REQUIRE(previous->min().y() == box.min().y());
            REQUIRE(previous->max().y() == box.max().y());
        }
        previous = box;
    }
    REQUIRE(tilesDrawn > 0);
}

// ===================================== Tint colour =====================================

TEST_CASE("Sprites are tinted with an explicit colour",
          "[tca][engine][rendering][scene][sprites][tint][tint_color]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);
    center_identity_camera(surface);

    const world_rect spriteRect{world_box(0.0, 0.0, 32.0, 32.0)};

    auto drawWith{[&](const sprite_options& options) {
        return capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            painter.draw_sprite(spriteRect, PROBE_TEXTURE, options);
        });
    }};

    SECTION("A tint colour multiplies the texture's colours: an enemy flashes red when hit") {
        const scene_capture capture{drawWith(sprite_options{.tint_color = PROBE_COLOR})};

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(sprite.size() == 6);
        REQUIRE(every_vertex_colored(sprite, to_imu32(PROBE_COLOR)));
    }

    SECTION("A tint colour is game data, not the theme's: the active theme leaves it alone") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::error, OTHER_PROBE_COLOR)};

        const scene_capture capture{drawWith(sprite_options{.tint_color = PROBE_COLOR})};

        REQUIRE(
            every_vertex_colored(capture.with_texture(PROBE_TEXTURE_ID), to_imu32(PROBE_COLOR)));
    }

    SECTION("The theme-role tint keeps working, and a colour given as well takes its place") {
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::error, OTHER_PROBE_COLOR)};

        const scene_capture roleOnly{drawWith(sprite_options{.tint = color_role::error})};
        const scene_capture both{
            drawWith(sprite_options{.tint = color_role::error, .tint_color = PROBE_COLOR})};

        REQUIRE(every_vertex_colored(roleOnly.with_texture(PROBE_TEXTURE_ID),
                                     to_imu32(OTHER_PROBE_COLOR)));
        REQUIRE(every_vertex_colored(both.with_texture(PROBE_TEXTURE_ID), to_imu32(PROBE_COLOR)));
    }

    SECTION("A tint colour's own alpha carries through") {
        const scene_capture capture{
            drawWith(sprite_options{.tint_color = HALF_TRANSPARENT_PROBE_COLOR})};

        REQUIRE(every_vertex_colored(capture.with_texture(PROBE_TEXTURE_ID),
                                     to_imu32(HALF_TRANSPARENT_PROBE_COLOR)));
    }

    SECTION("A tint colour changes the colour only: geometry and UVs are the untinted sprite's") {
        surface.camera().set_zoom(1.37);
        const sprite_options transform{
            .origin = vector2{4.0,  30.0},
              .scale = vector2{-1.0, 1.0 }
        };
        sprite_options tinted{transform};
        tinted.tint_color = PROBE_COLOR;

        const scene_capture plain{drawWith(transform)};
        const scene_capture colored{drawWith(tinted)};

        require_same_rect(bounding_box(colored.with_texture(PROBE_TEXTURE_ID)),
                          bounding_box(plain.with_texture(PROBE_TEXTURE_ID)));
        require_same_rect(uv_box(colored.with_texture(PROBE_TEXTURE_ID)),
                          uv_box(plain.with_texture(PROBE_TEXTURE_ID)), UV_EPSILON);
    }

    SECTION("A rotated sprite carries its tint on every vertex") {
        const scene_capture capture{
            drawWith(sprite_options{
                                    .tint_color = PROBE_COLOR, .origin = vector2{16.0, 16.0}, .rotation = 30.0}
            )
        };

        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(sprite.size() == 6);
        REQUIRE(every_vertex_colored(sprite, to_imu32(PROBE_COLOR)));
    }
}

// ======================================== Alpha ========================================

TEST_CASE("Sprites fade with alpha", "[tca][engine][rendering][scene][sprites][alpha]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);
    center_identity_camera(surface);

    const world_rect spriteRect{world_box(0.0, 0.0, 32.0, 32.0)};

    auto drawWith{[&](const sprite_options& options) {
        return capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            painter.draw_sprite(spriteRect, PROBE_TEXTURE, options);
        });
    }};

    SECTION("Alpha scales an untinted sprite's opacity, leaving the texture's colours") {
        // 255 * 0.25 = 63.75, rounded to 64.
        const scene_capture capture{drawWith(sprite_options{.alpha = 0.25})};

        REQUIRE(every_vertex_colored(capture.with_texture(PROBE_TEXTURE_ID),
                                     IM_COL32(255, 255, 255, 64)));
    }

    SECTION("Alpha multiplies a tint colour's own alpha") {
        const scene_capture capture{drawWith(sprite_options{
            .tint_color = tca::engine::rendering::rgba(11, 22, 33, 200), .alpha = 0.5})};

        REQUIRE(every_vertex_colored(capture.with_texture(PROBE_TEXTURE_ID),
                                     IM_COL32(11, 22, 33, 100)));
    }

    SECTION("Alpha multiplies a theme role's own alpha") {
        // HALF_TRANSPARENT_PROBE_COLOR is (40, 80, 160, 128): halved to 64.
        const theming::scoped_active_theme activeTheme{
            theme_with(color_role::accent, HALF_TRANSPARENT_PROBE_COLOR)};

        const scene_capture capture{
            drawWith(sprite_options{.tint = color_role::accent, .alpha = 0.5})};

        REQUIRE(every_vertex_colored(capture.with_texture(PROBE_TEXTURE_ID),
                                     IM_COL32(40, 80, 160, 64)));
    }

    SECTION("Alpha 0 draws nothing visible, however the sprite is tinted or turned") {
        const theming::scoped_active_theme activeTheme{theme_with(color_role::error, PROBE_COLOR)};
        const std::array<sprite_options, 4> faded{
            {
             {.alpha = 0.0},
             {.tint = color_role::error, .alpha = 0.0},
             {.tint_color = PROBE_COLOR, .alpha = 0.0},
             {.alpha = 0.0, .origin = vector2{16.0, 16.0}, .rotation = 45.0},
             }
        };

        for (std::size_t index{0}; index < faded.size(); ++index) {
            CAPTURE(index);
            REQUIRE(every_vertex_invisible(drawWith(faded[index]).with_texture(PROBE_TEXTURE_ID)));
        }
    }

    SECTION("An effect fading out frame by frame never brightens, and ends invisible") {
        constexpr int FRAMES{20};
        unsigned int previousAlpha{256u};
        for (int frame{0}; frame <= FRAMES; ++frame) {
            const double alpha{1.0 - static_cast<double>(frame) / FRAMES};
            CAPTURE(frame, alpha);
            const std::vector<drawn_vertex> sprite{
                drawWith(sprite_options{.tint_color = PROBE_COLOR, .alpha = alpha})
                    .with_texture(PROBE_TEXTURE_ID)};

            if (frame == FRAMES) {
                REQUIRE(every_vertex_invisible(sprite));
                break;
            }
            if (frame == 0) {
                REQUIRE(every_vertex_colored(sprite, to_imu32(PROBE_COLOR)));
            }
            REQUIRE_FALSE(sprite.empty());
            REQUIRE(alpha_of(sprite.front()) < previousAlpha);
            previousAlpha = alpha_of(sprite.front());
        }
    }

    SECTION("Alpha changes the colour only: geometry and UVs are the opaque sprite's") {
        surface.camera().set_zoom(1.37);
        const scene_capture opaque{drawWith(sprite_options{})};
        const scene_capture faded{drawWith(sprite_options{.alpha = 0.4})};

        require_same_rect(bounding_box(faded.with_texture(PROBE_TEXTURE_ID)),
                          bounding_box(opaque.with_texture(PROBE_TEXTURE_ID)));
        require_same_rect(uv_box(faded.with_texture(PROBE_TEXTURE_ID)),
                          uv_box(opaque.with_texture(PROBE_TEXTURE_ID)), UV_EPSILON);
    }
}

TEST_CASE("Transformed sprites keep the surface's layering and sampling",
          "[tca][engine][rendering][scene][sprites][transforms][layers][sampling]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    const scoped_fake_backend_samplers samplers{};
    scene_surface surface{"##sprites-surface", SURFACE_SIZE};
    make_bare(surface);
    center_identity_camera(surface);

    const sprite_options rotatedAbove{
        .layer = 2, .tint_color = PROBE_COLOR, .origin = CHARACTER_FEET, .rotation = 30.0};

    const scene_capture capture{capture_scene(imguiContext, surface, [&](scene_painter& painter) {
        painter.draw_sprite(CHARACTER_RECT, PROBE_TEXTURE, rotatedAbove);
        painter.fill_rect(world_box(90.0, 40.0, 140.0, 110.0), OTHER_PROBE_COLOR);
    })};

    SECTION("A rotated sprite on a higher layer draws over a shape asked for after it") {
        REQUIRE(capture.last_index_of_color(to_imu32(OTHER_PROBE_COLOR)) <
                capture.first_index_of_texture(PROBE_TEXTURE_ID));
    }

    SECTION("A rotated sprite is sampled like any other sprite") {
        const std::vector<drawn_vertex> sprite{capture.with_texture(PROBE_TEXTURE_ID)};
        REQUIRE(sprite.size() == 6);
        REQUIRE(std::ranges::all_of(sprite, sampled_nearest));
    }
}
