// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/scene-surface.hpp"

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <utility>

// Third-party
#include <imgui.h>
#include <imgui_internal.h>
#include <gsl/gsl>

// Engine
#include "core/math/math.hpp"
#include "core/math/vector.hpp"
#include "rendering/gui/imgui/imgui-blend-callbacks.hpp"
#include "rendering/scene/scene-grid.hpp"
#include "rendering/theming/active-theme.hpp"

namespace tca::engine::scene {

namespace {

// Below this many pixels between two grid lines the grid would read as a solid fill, and a tiny
// spacing would ask for millions of lines, so the grid is not drawn at all.
constexpr double MIN_GRID_PIXEL_SPACING{2.0};

// Grid lines are this many pixels wide, whatever the zoom.
constexpr double GRID_LINE_THICKNESS{1.0};

// The flags of the window a full-window surface opens: nothing of ImGui's own shows, and it stays
// behind every other window.
constexpr ImGuiWindowFlags FILL_WINDOW_FLAGS{
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus |
    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav};

void throw_on_invalid_size(const math::vector2& size) {
    if (!std::isfinite(size.x()) || !std::isfinite(size.y())) {
        throw std::invalid_argument{"Size values must be finite."};
    }

    if (size.x() < 0.0 || size.y() < 0.0) {
        throw std::invalid_argument{"Size values must have non negative values."};
    }
}

// Built straight from the components rather than through ImGui::GetColorU32(), which would also
// multiply in the style's global alpha.
ImU32 to_imu32(const rendering::color& value) noexcept {
    return IM_COL32(value.r, value.g, value.b, value.a);
}

// The colour with its own alpha multiplied by alpha, rounded to the nearest 8-bit value.
ImU32 to_imu32(const rendering::color& value, const alpha_value alpha) noexcept {
    const double faded{std::round(static_cast<double>(value.a) * static_cast<double>(alpha))};
    return IM_COL32(value.r, value.g, value.b, static_cast<std::uint8_t>(faded));
}

ImVec2 to_imvec2(const Eigen::Vector2d& value) noexcept {
    return ImVec2{static_cast<float>(value.x()), static_cast<float>(value.y())};
}

Eigen::Vector2d to_eigen(const ImVec2 value) noexcept {
    return Eigen::Vector2d{static_cast<double>(value.x), static_cast<double>(value.y)};
}

// The size a surface takes in a page: a fixed component is in unscaled pixels, so it follows the
// UI scale like every other widget dimension; a zero one takes what the page still has available,
// which is already in physical pixels.
Eigen::Vector2d layout_size(const math::vector2& size) {
    const double uiScale{static_cast<double>(ImGui::GetStyle().FontScaleDpi)};
    const Eigen::Vector2d available{to_eigen(ImGui::GetContentRegionAvail())};

    auto component{[uiScale](const double fixed, const double availableComponent) {
        return math::is_nearly_zero(fixed) ? std::max(availableComponent, 0.0) : fixed * uiScale;
    }};
    return Eigen::Vector2d{component(size.x(), available.x()), component(size.y(), available.y())};
}

// Paints the grid lines the camera shows, each one across the whole surface. pixelScale is how many
// screen pixels one of the camera's viewport pixels spans: the virtual scale, or 1.
//
// Each line is a GRID_LINE_THICKNESS-wide rectangle centred on the line's screen coordinate rather
// than an ImDrawList::AddLine(), which shifts lines by half a pixel and adds an anti-aliasing
// fringe: a grid line sits exactly where the painter would put the same world coordinate.
void draw_grid(ImDrawList& drawList, const camera_2d& camera, const screen_rect& bounds,
               const double pixelScale, const grid_options& options) {
    constexpr double HALF_THICKNESS{GRID_LINE_THICKNESS / 2.0};

    const double spacing{options.spacing};
    if (spacing * camera.zoom() * pixelScale < MIN_GRID_PIXEL_SPACING) {
        return;
    }

    const ImU32 lineColor{to_imu32(theming::color_for(options.role))};
    const grid_lines lines{visible_grid_lines(camera, spacing)};

    for (const double worldX : lines.vertical) {
        const double x{bounds.min().x() +
                       pixelScale * camera.world_to_screen(math::vector2{worldX, 0.0}).x()};
        drawList.AddRectFilled(to_imvec2(Eigen::Vector2d{x - HALF_THICKNESS, bounds.min().y()}),
                               to_imvec2(Eigen::Vector2d{x + HALF_THICKNESS, bounds.max().y()}),
                               lineColor);
    }
    for (const double worldY : lines.horizontal) {
        const double y{bounds.min().y() +
                       pixelScale * camera.world_to_screen(math::vector2{0.0, worldY}).y()};
        drawList.AddRectFilled(to_imvec2(Eigen::Vector2d{bounds.min().x(), y - HALF_THICKNESS}),
                               to_imvec2(Eigen::Vector2d{bounds.max().x(), y + HALF_THICKNESS}),
                               lineColor);
    }
}

// A sprite needs a rectangle with finite corners and a positive area. The comparison is false for
// NaN, so a NaN corner is rejected along with an empty or zero-area rectangle.
bool is_drawable(const world_rect& rect) noexcept {
    return rect.min().allFinite() && rect.max().allFinite() && (rect.sizes().array() > 0.0).all();
}

// A transform needs a finite origin, a finite rotation and a finite, non-zero scale on both axes: a
// zero one would squash the sprite to nothing.
bool is_drawable(const sprite_options& options) noexcept {
    const Eigen::Vector2d scale{options.scale};
    return Eigen::Vector2d{options.origin}.allFinite() && std::isfinite(options.rotation) &&
           scale.allFinite() && (scale.array() != 0.0).all();
}

// Strictly, unlike AlignedBox::intersects(): a sprite that only touches the view's edge shows no
// pixel of it.
bool overlaps(const world_rect& first, const world_rect& second) noexcept {
    return (first.min().array() < second.max().array()).all() &&
           (second.min().array() < first.max().array()).all();
}

// The corners of a rectangle, in the order of the UV corners they carry: top-left, top-right,
// bottom-right, bottom-left.
using quad_corners = std::array<Eigen::Vector2d, 4>;

quad_corners corners_of(const Eigen::AlignedBox2d& rect) noexcept {
    return quad_corners{
        rect.min(),
        Eigen::Vector2d{rect.max().x(), rect.min().y()},
        rect.max(),
        Eigen::Vector2d{rect.min().x(), rect.max().y()},
    };
}

// How many clockwise quarter turns an angle makes, in [0, 4), or nullopt if it is not a whole
// number of them.
std::optional<int> quarter_turns(const double degrees) noexcept {
    const double quarters{degrees / 90.0};
    if (quarters != std::round(quarters)) {
        return std::nullopt;
    }
    return (static_cast<int>(std::fmod(quarters, 4.0)) + 4) % 4;
}

// Scales each corner about the pivot, then turns it by whole quarter turns about it. Exact: a
// quarter turn only swaps and negates offsets, so the quad stays axis-aligned and corners sharing
// an edge keep sharing it to the last bit. With y down, a clockwise turn takes (x, y) to (-y, x).
quad_corners quarter_turned(const quad_corners& corners, const Eigen::Vector2d& pivot,
                            const Eigen::Vector2d& scale, const int turns) {
    // Left as they are, so an untransformed sprite lands exactly where it did without a transform.
    if (turns == 0 && scale == Eigen::Vector2d::Ones()) {
        return corners;
    }

    quad_corners turned{};
    std::ranges::transform(corners, turned.begin(), [&](const Eigen::Vector2d& corner) {
        const Eigen::Vector2d offset{(corner - pivot).cwiseProduct(scale)};
        switch (turns) {
            case 1:
                return Eigen::Vector2d{pivot.x() - offset.y(), pivot.y() + offset.x()};
            case 2:
                return Eigen::Vector2d{pivot - offset};
            case 3:
                return Eigen::Vector2d{pivot.x() + offset.y(), pivot.y() - offset.x()};
            default:
                return Eigen::Vector2d{pivot + offset};
        }
    });
    return turned;
}

// Scales each corner about the pivot, then turns it about it by any angle, clockwise on screen.
quad_corners rotated(const quad_corners& corners, const Eigen::Vector2d& pivot,
                     const Eigen::Vector2d& scale, const double degrees) {
    const Eigen::Rotation2Dd rotation{degrees * std::numbers::pi / 180.0};

    quad_corners turned{};
    std::ranges::transform(corners, turned.begin(), [&](const Eigen::Vector2d& corner) {
        return Eigen::Vector2d{pivot + rotation * (corner - pivot).cwiseProduct(scale)};
    });
    return turned;
}

// The colour the texture's colours are multiplied by: the explicit colour if there is one, else
// the theme role's, else white, which leaves them as they are. Then faded by the alpha.
ImU32 sprite_tint(const sprite_options& options) {
    if (options.tint_color) {
        return to_imu32(*options.tint_color, options.alpha);
    }
    if (options.tint) {
        return to_imu32(theming::color_for(*options.tint), options.alpha);
    }
    return to_imu32(rendering::colors::WHITE, options.alpha);
}

// The draw list channels a scene is split into while it draws: sprites on negative layers go
// beneath everything drawn on layer 0 and above.
constexpr int BELOW_SHAPES_CHANNEL{0};
constexpr int SHAPES_CHANNEL{1};
constexpr int SCENE_CHANNELS{2};

// The callback that puts the backend back on linear sampling: the dedicated one if the backend has
// it, or else a full render state reset, which starts linear too.
ImDrawCallback linear_sampler_callback(const ImGuiPlatformIO& platformIo) noexcept {
    return platformIo.DrawCallback_SetSamplerLinear != nullptr
               ? platformIo.DrawCallback_SetSamplerLinear
               : platformIo.DrawCallback_ResetRenderState;
}

} // namespace

// =============== scene_painter implementation ===============

scene_painter::scene_painter(const camera_2d& camera, const screen_rect& bounds,
                             const texture_filter spriteFilter,
                             const std::optional<std::uint32_t> virtualScale) noexcept
    : m_camera{camera},
      m_bounds{bounds},
      m_spriteFilter{spriteFilter},
      m_virtualScale{virtualScale} {}

screen_rect scene_painter::bounds() const noexcept {
    return m_bounds;
}

const camera_2d& scene_painter::camera() const noexcept {
    return m_camera;
}

// A painter only lives while the surface draws its renderables, when the surface's draw list is
// the current window's and its clip rectangle is pushed onto it: drawing goes straight there, so no
// ImGui type has to appear in the header.
void scene_painter::fill_rect(const world_rect& rect, const rendering::color& fillColor,
                              const alpha_value alpha) {
    if (rect.isEmpty()) {
        return;
    }

    // Snapped under a virtual resolution only: without one, a fill lands where it falls, as it
    // always has.
    const Eigen::Vector2d min{m_virtualScale ? to_snapped_screen(rect.min())
                                             : to_screen(rect.min())};
    const Eigen::Vector2d max{m_virtualScale ? to_snapped_screen(rect.max())
                                             : to_screen(rect.max())};

    blend_as(m_blendMode, texture_filter::linear);
    sample_as_shapes();
    ImGui::GetWindowDrawList()->AddRectFilled(to_imvec2(min), to_imvec2(max),
                                              to_imu32(fillColor, alpha));
}

void scene_painter::fill_rect(const world_rect& rect, const theming::color_role fillRole,
                              const alpha_value alpha) {
    fill_rect(rect, theming::color_for(fillRole), alpha);
}

void scene_painter::draw_line(const math::vector2& from, const math::vector2& to,
                              const rendering::color& lineColor, const double thickness) {
    // The thickness is in pixels, not world units: it is not scaled by the zoom, only by the
    // virtual scale.
    blend_as(m_blendMode, texture_filter::linear);
    sample_as_shapes();
    ImGui::GetWindowDrawList()->AddLine(to_imvec2(to_screen(from)), to_imvec2(to_screen(to)),
                                        to_imu32(lineColor),
                                        static_cast<float>(thickness * pixel_scale()));
}

void scene_painter::fill_circle(const math::vector2& centre, const double radius,
                                const rendering::color& fillColor) {
    blend_as(m_blendMode, texture_filter::linear);
    sample_as_shapes();
    ImGui::GetWindowDrawList()->AddCircleFilled(
        to_imvec2(to_screen(centre)), static_cast<float>(radius * m_camera.zoom() * pixel_scale()),
        to_imu32(fillColor));
}

double scene_painter::pixel_scale() const noexcept {
    return static_cast<double>(m_virtualScale.value_or(1u));
}

Eigen::Vector2d scene_painter::to_screen(const Eigen::Vector2d& world) const noexcept {
    return m_bounds.min() +
           pixel_scale() * Eigen::Vector2d{m_camera.world_to_screen(math::vector2{world})};
}

Eigen::Vector2d scene_painter::to_snapped_screen(const Eigen::Vector2d& world) const noexcept {
    if (!m_virtualScale) {
        return to_screen(world).array().round().matrix();
    }

    // Rounded in virtual pixels, then scaled: the content starts on a whole pixel, so this lands on
    // one too, a whole number of scales from it.
    const Eigen::Vector2d virtualPoint{m_camera.world_to_screen(math::vector2{world})};
    return m_bounds.min() + pixel_scale() * Eigen::Vector2d{virtualPoint.array().round().matrix()};
}

void scene_painter::sample_as_sprites() {
    if (m_spriteFilter != texture_filter::nearest || m_samplingNearest) {
        return;
    }

    // Blending additively the backend runs a pipeline of the engine's own, which switches sampler
    // through its own callbacks.
    if (m_blendingAdditive) {
        ImGui::GetWindowDrawList()->AddCallback(
            imgui::get_additive_blend_callbacks().set_additive_nearest, nullptr);
        m_samplingNearest = true;
        return;
    }

    // A backend without the standard sampler callbacks (or with no way back to linear) samples
    // sprites however it samples everything else: adding a null callback would assert.
    const ImGuiPlatformIO& platformIo{ImGui::GetPlatformIO()};
    if (platformIo.DrawCallback_SetSamplerNearest == nullptr ||
        linear_sampler_callback(platformIo) == nullptr) {
        return;
    }

    ImGui::GetWindowDrawList()->AddCallback(platformIo.DrawCallback_SetSamplerNearest, nullptr);
    m_samplingNearest = true;
}

void scene_painter::sample_as_shapes() {
    if (!m_samplingNearest) {
        return;
    }

    ImGui::GetWindowDrawList()->AddCallback(
        m_blendingAdditive ? imgui::get_additive_blend_callbacks().set_additive_linear
                           : linear_sampler_callback(ImGui::GetPlatformIO()),
        nullptr);
    m_samplingNearest = false;
}

void scene_painter::set_blend_mode(const blend_mode mode) noexcept {
    m_blendMode = mode;
}

blend_mode scene_painter::get_blend_mode() const noexcept {
    return m_blendMode;
}

// Additive blending needs the backend's callbacks into it, and a render state reset to come back:
// without either, adding a null callback would assert.
bool scene_painter::can_blend_additively() const noexcept {
    return imgui::get_additive_blend_callbacks().available() &&
           ImGui::GetPlatformIO().DrawCallback_ResetRenderState != nullptr;
}

void scene_painter::blend_as(const blend_mode mode, const texture_filter nextFilter) {
    const bool additive{mode == blend_mode::additive && can_blend_additively()};
    if (additive == m_blendingAdditive) {
        return;
    }

    ImDrawList& drawList{*ImGui::GetWindowDrawList()};
    if (additive) {
        // Straight into the additive pipeline that samples as what is drawn next wants, so going
        // additive costs one switch, not one for the blending and another for the sampler.
        const imgui::additive_blend_callbacks& callbacks{imgui::get_additive_blend_callbacks()};
        const bool nearest{nextFilter == texture_filter::nearest};
        drawList.AddCallback(
            nearest ? callbacks.set_additive_nearest : callbacks.set_additive_linear, nullptr);
        m_samplingNearest = nearest;
    } else {
        // The reset puts back ImGui's own pipeline, which samples linearly.
        drawList.AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
        m_samplingNearest = false;
    }
    m_blendingAdditive = additive;
}

// =============== scene_surface implementation ===============

scene_surface::scene_surface(const std::string_view id, const math::vector2& size)
    : m_id{id},
      m_size{size} {
    throw_on_invalid_size(m_size);
}

scene_surface::scene_surface(const std::string_view id, fill_window_t /*fillWindow*/)
    : m_id{id},
      m_fillWindow{true} {}

camera_2d& scene_surface::camera() noexcept {
    return m_camera;
}

const camera_2d& scene_surface::camera() const noexcept {
    return m_camera;
}

screen_rect scene_surface::bounds() const noexcept {
    return m_bounds;
}

void scene_surface::add_renderable(scene_renderable& renderable) {
    if (std::find(std::cbegin(m_renderables), std::cend(m_renderables), &renderable) ==
        std::cend(m_renderables)) {
        m_renderables.emplace_back(&renderable);
    }
}

void scene_surface::remove_renderable(scene_renderable& renderable) {
    m_renderables.remove(&renderable);
}

std::optional<theming::color_role> scene_surface::background_role() const noexcept {
    return m_backgroundRole;
}

void scene_surface::set_background_role(const std::optional<theming::color_role> role) {
    m_backgroundRole = role;
}

const std::optional<grid_options>& scene_surface::grid() const noexcept {
    return m_grid;
}

void scene_surface::set_grid(std::optional<grid_options> options) {
    m_grid = std::move(options);
}

void scene_surface::render() {
    if (m_fillWindow) {
        render_fill_window();
    } else {
        render_in_page();
    }
}

void scene_painter::draw_sprite(const world_rect& rect, const sprite_texture& texture,
                                const sprite_options& options) {
    draw_sprite(rect, sprite_region::whole(texture), options);
}

// Each corner is snapped to a whole pixel on its own, rather than snapping one corner and the size:
// two tiles sharing a world edge then round that edge to the same pixel, leaving no seam. Flipped
// and quarter-turned sprites are still axis-aligned, so they snap the same way; a sprite turned by
// any other angle is left where it falls, since snapping its corners would bend its edges.
void scene_painter::draw_sprite(const world_rect& rect, const sprite_region& region,
                                const sprite_options& options) {
    if (!is_drawable(rect) || !is_drawable(options) || region.texture.empty()) {
        return;
    }

    const Eigen::Vector2d pivot{rect.min() + Eigen::Vector2d{options.origin}};
    const Eigen::Vector2d scale{options.scale};
    const std::optional<int> turns{quarter_turns(options.rotation)};
    const quad_corners world{turns ? quarter_turned(corners_of(rect), pivot, scale, *turns)
                                   : rotated(corners_of(rect), pivot, scale, options.rotation)};

    // Culled by where the transform puts it rather than by its rectangle, and before it reaches the
    // draw list, so a big level only costs vertices for what is in view.
    world_rect extent{};
    for (const Eigen::Vector2d& corner : world) {
        extent.extend(corner);
    }
    if (!overlaps(m_camera.visible_world_rect(), extent)) {
        return;
    }

    // The tint is resolved now, while the sprite is asked for, so it follows the active theme even
    // if the sprite is held back for its layer.
    placed_sprite sprite{
        .layer = options.layer,
        .upright = turns && *turns % 2 == 0,
        .uv = region.uv(),
        .texture = region.texture.id,
        .tint = sprite_tint(options),
        .blend = m_blendMode,
    };
    std::ranges::transform(world, sprite.corners.begin(), [&](const Eigen::Vector2d& corner) {
        return turns ? to_snapped_screen(corner) : to_screen(corner);
    });

    // Layer 0 is the shapes' layer: drawn straight away, it keeps its call order among them.
    if (sprite.layer == 0) {
        add_sprite(sprite);
    } else if (sprite.layer > 0) {
        m_spritesAbove.push_back(sprite);
    } else {
        m_spritesBelow.push_back(sprite);
    }
}

void scene_painter::add_sprite(const placed_sprite& sprite) {
    // Blending first: going back to normal blending also puts the sampler back to linear.
    blend_as(sprite.blend, m_spriteFilter);
    sample_as_sprites();

    // One quad with no anti-aliasing fringe. The tint multiplies the texture's colours: white
    // leaves them as they are.
    ImDrawList& drawList{*ImGui::GetWindowDrawList()};
    const auto textureId{static_cast<ImTextureID>(sprite.texture)};

    if (sprite.upright) {
        // Flipped, the corner carrying the top-left UV is no longer the top-left one: drawn over
        // the same screen rectangle with its UVs swapped on the flipped axes, the texture mirrors.
        const Eigen::Vector2d& topLeftUv{sprite.corners[0]};
        const Eigen::Vector2d& bottomRightUv{sprite.corners[2]};
        Eigen::Vector2d uvMin{sprite.uv.min()};
        Eigen::Vector2d uvMax{sprite.uv.max()};
        for (Eigen::Index axis{0}; axis < 2; ++axis) {
            if (topLeftUv[axis] > bottomRightUv[axis]) {
                std::swap(uvMin[axis], uvMax[axis]);
            }
        }
        drawList.AddImage(textureId, to_imvec2(topLeftUv.cwiseMin(bottomRightUv)),
                          to_imvec2(topLeftUv.cwiseMax(bottomRightUv)), to_imvec2(uvMin),
                          to_imvec2(uvMax), sprite.tint);
        return;
    }

    const quad_corners uvs{corners_of(sprite.uv)};
    drawList.AddImageQuad(textureId, to_imvec2(sprite.corners[0]), to_imvec2(sprite.corners[1]),
                          to_imvec2(sprite.corners[2]), to_imvec2(sprite.corners[3]),
                          to_imvec2(uvs[0]), to_imvec2(uvs[1]), to_imvec2(uvs[2]),
                          to_imvec2(uvs[3]), sprite.tint);
}

void scene_painter::draw_held_sprites(const layer_side side) {
    std::vector<placed_sprite>& sprites{side == layer_side::above_shapes ? m_spritesAbove
                                                                         : m_spritesBelow};

    // Stable, so sprites on one layer keep their call order.
    std::ranges::stable_sort(sprites, std::ranges::less{}, &placed_sprite::layer);
    for (const placed_sprite& sprite : sprites) {
        add_sprite(sprite);
    }
    sprites.clear();

    blend_as(blend_mode::normal, texture_filter::linear);
    sample_as_shapes();
}

texture_filter scene_surface::sprite_filter() const noexcept {
    return m_spriteFilter;
}

void scene_surface::set_sprite_filter(const texture_filter filter) noexcept {
    m_spriteFilter = filter;
}

std::optional<virtual_size> scene_surface::virtual_resolution() const noexcept {
    return m_virtualResolution;
}

void scene_surface::set_virtual_resolution(const std::optional<virtual_size> resolution) {
    if (resolution && (resolution->width == 0 || resolution->height == 0)) {
        throw std::invalid_argument{
            "Invalid virtual resolution: width and height must have a positive non-zero value "
            "each."};
    }

    m_virtualResolution = resolution;
}

theming::color_role scene_surface::letterbox_role() const noexcept {
    return m_letterboxRole;
}

void scene_surface::set_letterbox_role(const theming::color_role role) noexcept {
    m_letterboxRole = role;
}

const std::optional<pixel_perfect_layout>& scene_surface::presentation_layout() const noexcept {
    return m_presentationLayout;
}

std::optional<math::vector2> scene_surface::screen_to_virtual(
    const math::vector2& screenPoint) const noexcept {
    if (m_presentationLayout) {
        return scene::screen_to_virtual(*m_presentationLayout, screenPoint);
    }

    // Without a virtual resolution a virtual pixel is one of the surface's own. Before the first
    // render the bounds are empty, so nothing is pointed at; a NaN fails every comparison too.
    const bool inside{(screenPoint.array() >= m_bounds.min().array()).all() &&
                      (screenPoint.array() < m_bounds.max().array()).all()};
    if (!inside) {
        return std::nullopt;
    }
    return math::vector2{screenPoint - m_bounds.min()};
}

math::vector2 scene_surface::virtual_to_screen(const math::vector2& virtualPoint) const noexcept {
    if (m_presentationLayout) {
        return scene::virtual_to_screen(*m_presentationLayout, virtualPoint);
    }
    return math::vector2{m_bounds.min() + virtualPoint};
}

void scene_surface::render_in_page() {
    const Eigen::Vector2d position{to_eigen(ImGui::GetCursorScreenPos())};
    lay_out(screen_rect{position, position + layout_size(m_size)});

    // Reserves the rectangle in the page, so whatever is rendered next is laid out after it.
    const ImRect itemRect{to_imvec2(m_bounds.min()), to_imvec2(m_bounds.max())};
    ImGui::ItemSize(itemRect);
    if (!ImGui::ItemAdd(itemRect, ImGui::GetID(m_id.c_str()))) {
        // Entirely clipped out of the page: nothing drawn would be seen.
        return;
    }

    draw_scene();
}

void scene_surface::render_fill_window() {
    const ImGuiViewport& viewport{*ImGui::GetMainViewport()};
    lay_out(screen_rect{to_eigen(viewport.Pos), to_eigen(viewport.Pos) + to_eigen(viewport.Size)});

    ImGui::SetNextWindowPos(viewport.Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport.Size, ImGuiCond_Always);

    // Only read by Begin(), so they are popped again straight after it: whatever the scene or the
    // rest of the frame renders keeps the game's style.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    const bool visible{ImGui::Begin(m_id.c_str(), nullptr, FILL_WINDOW_FLAGS)};
    ImGui::PopStyleVar(3);

    // End() must run even if a renderable throws, or ImGui's window stack is left unbalanced.
    const auto endWindow{gsl::finally([] {
        ImGui::End();
    })};
    if (visible) {
        draw_scene();
    }
}

void scene_surface::lay_out(const screen_rect& bounds) {
    m_bounds = bounds;

    // The camera must know this frame's size before anything is drawn through it: under a virtual
    // resolution, one world unit is one virtual pixel at zoom 1, whatever the scale on screen.
    if (m_virtualResolution) {
        m_presentationLayout = compute_pixel_perfect_layout(m_bounds, *m_virtualResolution);
        m_camera.set_viewport_size(math::vector2{static_cast<double>(m_virtualResolution->width),
                                                 static_cast<double>(m_virtualResolution->height)});
    } else {
        m_presentationLayout.reset();
        m_camera.set_viewport_size(math::vector2{m_bounds.sizes()});
    }
}

void scene_surface::draw_scene() {
    ImDrawList& drawList{*ImGui::GetWindowDrawList()};

    // Intersected with the page's own clip rectangle, so a surface partly outside its page does not
    // spill over it. Popped however the renderables leave.
    drawList.PushClipRect(to_imvec2(m_bounds.min()), to_imvec2(m_bounds.max()), true);
    const auto popClipRect{gsl::finally([&drawList] {
        drawList.PopClipRect();
    })};

    // Under a virtual resolution the scene only owns the content: the letterbox fills the rest of
    // the surface, and the scene is clipped to the content so nothing spills over the bars.
    screen_rect sceneBounds{m_bounds};
    std::optional<std::uint32_t> virtualScale{};
    if (m_presentationLayout) {
        virtualScale = m_presentationLayout->scale;
        sceneBounds = m_presentationLayout->content;

        const ImU32 letterboxColor{to_imu32(theming::color_for(m_letterboxRole))};
        for (const screen_rect& bar : m_presentationLayout->letterbox) {
            drawList.AddRectFilled(to_imvec2(bar.min()), to_imvec2(bar.max()), letterboxColor);
        }
        drawList.PushClipRect(to_imvec2(sceneBounds.min()), to_imvec2(sceneBounds.max()), true);
    }
    const auto popContentClipRect{
        gsl::finally([&drawList, clipped = m_presentationLayout.has_value()] {
            if (clipped) {
                drawList.PopClipRect();
            }
        })};

    // Resolved at render time rather than cached, so a theme swapped after the surface was built
    // still reaches it.
    if (m_backgroundRole) {
        drawList.AddRectFilled(to_imvec2(sceneBounds.min()), to_imvec2(sceneBounds.max()),
                               to_imu32(theming::color_for(*m_backgroundRole)));
    }

    if (m_grid) {
        draw_grid(drawList, m_camera, sceneBounds, static_cast<double>(virtualScale.value_or(1u)),
                  *m_grid);
    }

    if (!m_renderables.empty()) {
        // A splitter of the surface's own rather than the draw list's, which the page may already
        // be using (a table splits it too, and splitters do not nest).
        ImDrawListSplitter channels{};
        channels.Split(&drawList, SCENE_CHANNELS);
        channels.SetCurrentChannel(&drawList, SHAPES_CHANNEL);

        scene_painter painter{m_camera, sceneBounds, m_spriteFilter, virtualScale};
        // However the renderables leave: the held-back layers are drawn, the page gets the draw
        // list back in one channel, and whatever it draws next is sampled linearly, blends
        // normally and lands on top.
        const auto endScene{gsl::finally([&] {
            painter.draw_held_sprites(scene_painter::layer_side::above_shapes);
            channels.SetCurrentChannel(&drawList, BELOW_SHAPES_CHANNEL);
            painter.draw_held_sprites(scene_painter::layer_side::below_shapes);
            channels.Merge(&drawList);
        })};
        // One painter for every renderable, so the layers it holds back sort across all of them.
        render_context context{painter};
        for (scene_renderable* const renderable : m_renderables) {
            // Whatever blend mode the previous renderable left, this one starts from normal.
            painter.set_blend_mode(blend_mode::normal);
            renderable->render(context);
        }
    }
}

} // namespace tca::engine::scene
