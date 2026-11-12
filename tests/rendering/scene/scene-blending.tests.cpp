// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/scene-surface.hpp"
#include "rendering/scene/sprite.hpp"

// C++ STL
#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <vector>

// Third-party
#include <imgui.h>
#include <Eigen/Geometry>
#include <catch2/catch_test_macros.hpp>

// Engine
#include "core/math/vector.hpp"
#include "rendering/colors.hpp"
#include "rendering/gui/imgui/imgui-blend-callbacks.hpp"
#include "rendering/scene/scene-renderable.hpp"

// Engine testing support
#include "testing/imgui/headless-imgui-context.hpp"

// Test utilities
#include "utilities/scene-drawing.hpp"

namespace {

using tca::engine::math::vector2;
using tca::engine::rendering::color;
using tca::engine::scene::blend_mode;
using tca::engine::scene::callback_scene_renderable;
using tca::engine::scene::scene_painter;
using tca::engine::scene::scene_surface;
using tca::engine::scene::sprite_options;
using tca::engine::scene::sprite_texture;
using tca::engine::scene::texture_filter;
using tca::engine::scene::texture_id;
using tca::engine::scene::world_rect;
using tca::engine::testing::headless_imgui_context;
using tca::tests::utilities::painting;
using tca::tests::utilities::scoped_scene_drawing;

namespace imgui = tca::engine::imgui;

constexpr ImVec2 DISPLAY_SIZE{1280.0f, 720.0f};

constexpr ImVec2 HOST_POSITION{40.0f, 30.0f};
constexpr ImVec2 HOST_SIZE{800.0f, 600.0f};
constexpr ImGuiWindowFlags HOST_FLAGS{
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse};

const vector2 SURFACE_SIZE{400.0, 300.0};

// Identifiers no real backend hands out in a headless test, so a draw command carrying one can
// only come from a sprite the test drew.
constexpr texture_id FLAME_TEXTURE_ID{0xF1A3E};
constexpr texture_id WALL_TEXTURE_ID{0x3A11};
constexpr sprite_texture FLAME_TEXTURE{.id = FLAME_TEXTURE_ID, .width = 16, .height = 16};
constexpr sprite_texture WALL_TEXTURE{.id = WALL_TEXTURE_ID, .width = 16, .height = 16};

constexpr color GLOW_COLOR{tca::engine::rendering::rgba(250, 200, 40, 128)};
constexpr color PAGE_COLOR{tca::engine::rendering::rgba(99, 1, 180, 255)};

const world_rect CELL{
    Eigen::Vector2d{0.0,  0.0 },
    Eigen::Vector2d{16.0, 16.0}
};

// ======================================= Helpers =======================================

ImU32 to_imu32(const color& value) {
    return IM_COL32(value.r, value.g, value.b, value.a);
}

// Stand-ins for the callbacks a real backend installs. They are only ever compared against, never
// called: no draw data is rendered here.
//
// Each body must differ: MSVC's Release linker folds functions with identical code into one
// (/OPT:ICF), which would give them all the same address and make them indistinguishable.
[[maybe_unused]] int last_fake_callback{0};
void fake_reset_render_state(const ImDrawList* /*drawList*/, const ImDrawCmd* /*command*/) {
    last_fake_callback = 1;
}
void fake_set_sampler_linear(const ImDrawList* /*drawList*/, const ImDrawCmd* /*command*/) {
    last_fake_callback = 2;
}
void fake_set_sampler_nearest(const ImDrawList* /*drawList*/, const ImDrawCmd* /*command*/) {
    last_fake_callback = 3;
}
void fake_set_additive_linear(const ImDrawList* /*drawList*/, const ImDrawCmd* /*command*/) {
    last_fake_callback = 4;
}
void fake_set_additive_nearest(const ImDrawList* /*drawList*/, const ImDrawCmd* /*command*/) {
    last_fake_callback = 5;
}

// Installs ImGui's standard callbacks into the current context and the additive ones into the
// engine for as long as it lives, so the headless context looks like one with a real renderer
// behind it.
struct scoped_fake_backend {
    explicit scoped_fake_backend(const bool withAdditiveBlending = true) {
        ImGuiPlatformIO& platformIo{ImGui::GetPlatformIO()};
        platformIo.DrawCallback_ResetRenderState = fake_reset_render_state;
        platformIo.DrawCallback_SetSamplerLinear = fake_set_sampler_linear;
        platformIo.DrawCallback_SetSamplerNearest = fake_set_sampler_nearest;
        if (withAdditiveBlending) {
            imgui::set_additive_blend_callbacks(imgui::additive_blend_callbacks{
                .set_additive_linear = fake_set_additive_linear,
                .set_additive_nearest = fake_set_additive_nearest,
            });
        }
    }

    ~scoped_fake_backend() {
        ImGuiPlatformIO& platformIo{ImGui::GetPlatformIO()};
        platformIo.DrawCallback_ResetRenderState = nullptr;
        platformIo.DrawCallback_SetSamplerLinear = nullptr;
        platformIo.DrawCallback_SetSamplerNearest = nullptr;
        imgui::set_additive_blend_callbacks({});
    }

    scoped_fake_backend(const scoped_fake_backend&) = delete;
    scoped_fake_backend& operator=(const scoped_fake_backend&) = delete;
};

// One vertex of what was drawn, in draw order, with the blending and sampling the backend draws it
// with, as last set by a callback in the draw list.
struct drawn_vertex {
    ImDrawVert vertex{};
    ImTextureID texture{ImTextureID_Invalid};
    blend_mode blend{blend_mode::normal};
    bool nearest{false};
};

// Every vertex drawList holds from firstIndex on, walked through the index buffer command by
// command, following the callbacks from the start of the draw list.
std::vector<drawn_vertex> vertices_in_draw_order(const ImDrawList& drawList, const int firstIndex) {
    std::vector<drawn_vertex> drawn{};
    blend_mode blend{blend_mode::normal};
    bool nearest{false};
    for (const ImDrawCmd& command : drawList.CmdBuffer) {
        if (command.UserCallback == fake_reset_render_state ||
            command.UserCallback == fake_set_sampler_linear) {
            blend = command.UserCallback == fake_reset_render_state ? blend_mode::normal : blend;
            nearest = false;
            continue;
        }
        if (command.UserCallback == fake_set_sampler_nearest) {
            nearest = true;
            continue;
        }
        if (command.UserCallback == fake_set_additive_linear ||
            command.UserCallback == fake_set_additive_nearest) {
            blend = blend_mode::additive;
            nearest = command.UserCallback == fake_set_additive_nearest;
            continue;
        }
        if (command.UserCallback != nullptr) {
            continue;
        }

        // Read from the reference itself: GetTexID() asserts on the font atlas headlessly.
        const ImTextureID texture{command.TexRef._TexData != nullptr ? ImTextureID_Invalid
                                                                     : command.TexRef._TexID};
        const int begin{static_cast<int>(command.IdxOffset)};
        const int end{begin + static_cast<int>(command.ElemCount)};
        for (int index{std::max(begin, firstIndex)}; index < end; ++index) {
            drawn.push_back(drawn_vertex{
                .vertex = drawList.VtxBuffer[static_cast<int>(command.VtxOffset) +
                                             drawList.IdxBuffer[index]],
                .texture = texture,
                .blend = blend,
                .nearest = nearest,
            });
        }
    }
    return drawn;
}

// How many times the draw list calls callback at or after firstIndex.
std::size_t callbacks_to(const ImDrawList& drawList, const ImDrawCallback callback,
                         const int firstIndex) {
    return static_cast<std::size_t>(
        std::ranges::count_if(drawList.CmdBuffer, [&](const ImDrawCmd& command) {
            return command.UserCallback == callback &&
                   static_cast<int>(command.IdxOffset) >= firstIndex;
        }));
}

struct capture {
    std::vector<drawn_vertex> vertices{};
    std::size_t additiveSwitches{};
    std::size_t resets{};

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
};

bool blended_additively(const drawn_vertex& drawn) {
    return drawn.blend == blend_mode::additive;
}

bool blended_normally(const drawn_vertex& drawn) {
    return drawn.blend == blend_mode::normal;
}

bool sampled_nearest(const drawn_vertex& drawn) {
    return drawn.nearest;
}

bool sampled_linear(const drawn_vertex& drawn) {
    return !drawn.nearest;
}

void run_frame(headless_imgui_context& context, const std::function<void()>& body) {
    context.new_frame();
    body();
    context.end_frame();
}

// Renders the surface inside a plain ImGui window standing for a game's page, then lets the page
// draw a rectangle after it, and returns everything both drew.
capture render_surface(headless_imgui_context& context, scene_surface& surface,
                       const bool expectThrow = false) {
    capture captured{};
    run_frame(context, [&] {
        ImGui::SetNextWindowPos(HOST_POSITION, ImGuiCond_Always);
        ImGui::SetNextWindowSize(HOST_SIZE, ImGuiCond_Always);
        ImGui::Begin("scene-blending-tests-host", nullptr, HOST_FLAGS);
        // End() must run even if a REQUIRE throws, or ImGui aborts the whole test binary.
        try {
            ImDrawList& drawList{*ImGui::GetWindowDrawList()};
            const int firstIndex{drawList.IdxBuffer.Size};
            if (expectThrow) {
                REQUIRE_THROWS_AS(surface.render(), std::runtime_error);
            } else {
                surface.render();
            }
            drawList.AddRectFilled(ImVec2{0.0f, 0.0f}, ImVec2{10.0f, 10.0f}, to_imu32(PAGE_COLOR));

            captured.vertices = vertices_in_draw_order(drawList, firstIndex);
            captured.additiveSwitches =
                callbacks_to(drawList, fake_set_additive_linear, firstIndex) +
                callbacks_to(drawList, fake_set_additive_nearest, firstIndex);
            captured.resets = callbacks_to(drawList, fake_reset_render_state, firstIndex);
        } catch (...) {
            ImGui::End();
            throw;
        }
        ImGui::End();
    });
    return captured;
}

capture capture_scene(headless_imgui_context& context, scene_surface& surface,
                      const std::function<void(scene_painter&)>& draw) {
    const scoped_scene_drawing drawing{surface, draw};
    return render_surface(context, surface);
}

// A surface with no background and no grid, so everything it draws is the scene's.
void make_bare(scene_surface& surface) {
    surface.set_background_role(std::nullopt);
    surface.set_grid(std::nullopt);
}

} // namespace

TEST_CASE("scene_painter draws additively between normal draws",
          "[tca][engine][rendering][scene][blending]") {
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    const scoped_fake_backend backend{};

    scene_surface surface{"##blending-surface", SURFACE_SIZE};
    make_bare(surface);

    SECTION("A painter starts blending normally, and remembers the mode it is given") {
        bool couldBlend{};
        blend_mode before{blend_mode::additive};
        blend_mode after{blend_mode::normal};
        capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            couldBlend = painter.can_blend_additively();
            before = painter.get_blend_mode();
            painter.set_blend_mode(blend_mode::additive);
            after = painter.get_blend_mode();
        });

        REQUIRE(couldBlend);
        REQUIRE(before == blend_mode::normal);
        REQUIRE(after == blend_mode::additive);
    }

    SECTION("An additive sprite is blended additively and still sampled nearest-neighbour") {
        const capture captured{capture_scene(imguiContext, surface, [](scene_painter& painter) {
            painter.set_blend_mode(blend_mode::additive);
            painter.draw_sprite(CELL, FLAME_TEXTURE);
        })};

        REQUIRE(captured.with_texture(FLAME_TEXTURE_ID).size() == 6);
        REQUIRE(std::ranges::all_of(captured.with_texture(FLAME_TEXTURE_ID), blended_additively));
        REQUIRE(std::ranges::all_of(captured.with_texture(FLAME_TEXTURE_ID), sampled_nearest));
    }

    SECTION("Under a linear filter an additive sprite is sampled linearly") {
        surface.set_sprite_filter(texture_filter::linear);

        const capture captured{capture_scene(imguiContext, surface, [](scene_painter& painter) {
            painter.set_blend_mode(blend_mode::additive);
            painter.draw_sprite(CELL, FLAME_TEXTURE);
        })};

        REQUIRE(std::ranges::all_of(captured.with_texture(FLAME_TEXTURE_ID), blended_additively));
        REQUIRE(std::ranges::all_of(captured.with_texture(FLAME_TEXTURE_ID), sampled_linear));
    }

    SECTION("Normal and additive draws interleave in call order") {
        const capture captured{capture_scene(imguiContext, surface, [](scene_painter& painter) {
            painter.draw_sprite(CELL, WALL_TEXTURE);
            painter.set_blend_mode(blend_mode::additive);
            painter.draw_sprite(CELL, FLAME_TEXTURE);
            painter.fill_rect(CELL, GLOW_COLOR);
            painter.set_blend_mode(blend_mode::normal);
            painter.draw_sprite(CELL, WALL_TEXTURE, sprite_options{.tint_color = PAGE_COLOR});
        })};

        const std::vector<drawn_vertex> walls{captured.with_texture(WALL_TEXTURE_ID)};
        REQUIRE(walls.size() == 12);
        REQUIRE(std::ranges::all_of(walls, blended_normally));
        REQUIRE(std::ranges::all_of(walls, sampled_nearest));
        REQUIRE(std::ranges::all_of(captured.with_texture(FLAME_TEXTURE_ID), blended_additively));
        REQUIRE(std::ranges::all_of(captured.with_texture(FLAME_TEXTURE_ID), sampled_nearest));

        // A shape drawn additively blends additively too, but keeps linear sampling.
        const std::vector<drawn_vertex> glow{captured.with_color(to_imu32(GLOW_COLOR))};
        REQUIRE_FALSE(glow.empty());
        REQUIRE(std::ranges::all_of(glow, blended_additively));
        REQUIRE(std::ranges::all_of(glow, sampled_linear));
    }

    SECTION("A run of additive sprites switches blending once, not once per sprite") {
        const capture captured{capture_scene(imguiContext, surface, [](scene_painter& painter) {
            painter.set_blend_mode(blend_mode::additive);
            for (int index{0}; index < 20; ++index) {
                const double x{static_cast<double>(index) * 4.0};
                painter.draw_sprite(
                    world_rect{
                        Eigen::Vector2d{x,        0.0 },
                        Eigen::Vector2d{x + 16.0, 16.0}
                },
                    FLAME_TEXTURE);
            }
        })};

        REQUIRE(captured.additiveSwitches == 1);
        REQUIRE(captured.resets == 1);
        REQUIRE(captured.with_texture(FLAME_TEXTURE_ID).size() == 20 * 6);
    }

    SECTION("A scene drawing nothing additively issues no blending switch") {
        const capture captured{capture_scene(imguiContext, surface, [](scene_painter& painter) {
            painter.draw_sprite(CELL, WALL_TEXTURE);
            painter.fill_rect(CELL, GLOW_COLOR);
        })};

        REQUIRE(captured.additiveSwitches == 0);
        REQUIRE(std::ranges::all_of(captured.vertices, blended_normally));
    }

    SECTION("A sprite held back for its layer keeps the mode it was drawn with") {
        const capture captured{capture_scene(imguiContext, surface, [](scene_painter& painter) {
            painter.set_blend_mode(blend_mode::additive);
            painter.draw_sprite(CELL, FLAME_TEXTURE, sprite_options{.layer = 3});
            painter.draw_sprite(CELL, FLAME_TEXTURE, sprite_options{.layer = -3});
            painter.set_blend_mode(blend_mode::normal);
            painter.draw_sprite(CELL, WALL_TEXTURE, sprite_options{.layer = 3});
            painter.draw_sprite(CELL, WALL_TEXTURE, sprite_options{.layer = -3});
        })};

        REQUIRE(captured.with_texture(FLAME_TEXTURE_ID).size() == 12);
        REQUIRE(std::ranges::all_of(captured.with_texture(FLAME_TEXTURE_ID), blended_additively));
        REQUIRE(captured.with_texture(WALL_TEXTURE_ID).size() == 12);
        REQUIRE(std::ranges::all_of(captured.with_texture(WALL_TEXTURE_ID), blended_normally));
    }

    SECTION("Whatever the page draws after the surface blends normally and samples linearly") {
        const capture captured{capture_scene(imguiContext, surface, [](scene_painter& painter) {
            painter.set_blend_mode(blend_mode::additive);
            painter.draw_sprite(CELL, FLAME_TEXTURE);
            painter.draw_sprite(CELL, FLAME_TEXTURE, sprite_options{.layer = -1});
        })};

        const std::vector<drawn_vertex> page{captured.with_color(to_imu32(PAGE_COLOR))};
        REQUIRE_FALSE(page.empty());
        REQUIRE(std::ranges::all_of(page, blended_normally));
        REQUIRE(std::ranges::all_of(page, sampled_linear));
    }

    SECTION("A renderable that throws still hands the page back blending normally") {
        const scoped_scene_drawing drawing{surface, [](scene_painter& painter) {
                                               painter.set_blend_mode(blend_mode::additive);
                                               painter.draw_sprite(CELL, FLAME_TEXTURE);
                                               throw std::runtime_error{"scene failure"};
                                           }};
        const capture captured{render_surface(imguiContext, surface, true)};

        const std::vector<drawn_vertex> page{captured.with_color(to_imu32(PAGE_COLOR))};
        REQUIRE_FALSE(page.empty());
        REQUIRE(std::ranges::all_of(page, blended_normally));
    }

    SECTION("Each renderable starts blending normally, whatever the previous one left") {
        callback_scene_renderable first{painting([](scene_painter& painter) {
            painter.set_blend_mode(blend_mode::additive);
            painter.draw_sprite(CELL, FLAME_TEXTURE);
        })};
        blend_mode secondStartedWith{blend_mode::additive};
        callback_scene_renderable second{painting([&](scene_painter& painter) {
            secondStartedWith = painter.get_blend_mode();
            painter.draw_sprite(CELL, WALL_TEXTURE);
        })};
        surface.add_renderable(first);
        surface.add_renderable(second);

        const capture captured{render_surface(imguiContext, surface)};
        surface.remove_renderable(first);
        surface.remove_renderable(second);

        REQUIRE(secondStartedWith == blend_mode::normal);
        REQUIRE(std::ranges::all_of(captured.with_texture(FLAME_TEXTURE_ID), blended_additively));
        REQUIRE(std::ranges::all_of(captured.with_texture(WALL_TEXTURE_ID), blended_normally));
    }
}

TEST_CASE("Additive draws blend normally on a renderer that cannot blend additively",
          "[tca][engine][rendering][scene][blending]") {
    // The null renderer of a headless application has no callbacks at all, and a backend the
    // engine does not wire up has ImGui's but not the additive ones: either way the surface must
    // add no null callback (which ImGui asserts on) and still draw everything.
    headless_imgui_context imguiContext{DISPLAY_SIZE};
    scene_surface surface{"##blending-surface", SURFACE_SIZE};
    make_bare(surface);

    auto drawAdditively{[](scene_painter& painter) {
        painter.set_blend_mode(blend_mode::additive);
        painter.draw_sprite(CELL, FLAME_TEXTURE);
        painter.fill_rect(CELL, GLOW_COLOR);
    }};

    SECTION("With no callbacks at all") {
        bool couldBlend{true};
        const capture captured{capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            couldBlend = painter.can_blend_additively();
            drawAdditively(painter);
        })};

        REQUIRE_FALSE(couldBlend);
        REQUIRE(captured.with_texture(FLAME_TEXTURE_ID).size() == 6);
        REQUIRE_FALSE(captured.with_color(to_imu32(GLOW_COLOR)).empty());
        REQUIRE(std::ranges::all_of(captured.vertices, blended_normally));
    }

    SECTION("With ImGui's callbacks but no additive ones") {
        const scoped_fake_backend backend{false};

        bool couldBlend{true};
        const capture captured{capture_scene(imguiContext, surface, [&](scene_painter& painter) {
            couldBlend = painter.can_blend_additively();
            drawAdditively(painter);
        })};

        REQUIRE_FALSE(couldBlend);
        REQUIRE(captured.additiveSwitches == 0);
        REQUIRE(captured.with_texture(FLAME_TEXTURE_ID).size() == 6);
        REQUIRE(std::ranges::all_of(captured.vertices, blended_normally));
        REQUIRE(std::ranges::all_of(captured.with_texture(FLAME_TEXTURE_ID), sampled_nearest));
    }
}
