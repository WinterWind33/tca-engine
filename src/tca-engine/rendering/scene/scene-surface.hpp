// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <array>
#include <cstdint>
#include <limits>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Third-party
#include <Eigen/Geometry>
#include <gsl/gsl>

// Engine
#include "core/math/vector.hpp"
#include "core/utilities/ranged-value.hpp"
#include "rendering/colors.hpp"
#include "rendering/gui/widget.hpp"
#include "rendering/scene/camera-2d.hpp"
#include "rendering/scene/scene-renderable.hpp"
#include "rendering/scene/sprite.hpp"
#include "rendering/scene/virtual-resolution.hpp"
#include "rendering/theming/theme-roles.hpp"
#include "rendering/tickables.hpp"

namespace tca::engine::scene {

/**
 * @brief Tag selecting a surface that covers the whole client area.
 */
struct fill_window_t {
    explicit fill_window_t() = default;
};

inline constexpr fill_window_t fill_window{};

/**
 * @brief How a surface draws its grid.
 */
struct grid_options {
    using spacing_type = core::ranged_value<double, 0.0, std::numeric_limits<double>::max(),
                                            core::rv_bound_policy::min_open>;
    /**
     * @brief The distance between two lines, in world units.
     */
    spacing_type spacing{32.0};

    /**
     * @brief The theme colour role the lines are drawn with.
     */
    theming::color_role role{theming::color_role::outline_variant};
};

/**
 * @brief What a scene draws through: world-space drawing primitives, clipped to the surface.
 *  Only valid for the duration of the scene_renderable::render() call whose render_context hands
 *  it out.
 */
class scene_painter final {
public:
    scene_painter(const scene_painter&) = delete;
    scene_painter& operator=(const scene_painter&) = delete;

    /**
     * @brief The surface's bounds, in screen pixels: under a virtual resolution, the content the
     *  virtual screen is scaled onto, without the letterbox.
     */
    screen_rect bounds() const noexcept;

    /**
     * @brief The camera the surface maps the world through.
     */
    const camera_2d& camera() const noexcept;

    /**
     * @brief Fills a world rectangle with a flat colour, e.g. a hit flash, a fade or a simple
     *  panel. Drawn on layer 0, with shapes.
     *
     * @param rect The rectangle to fill, in world units. Draws nothing if it is empty.
     * @param fillColor The colour to fill it with.
     * @param alpha Multiplies the colour's own alpha, rounded to the nearest 8-bit value. 0 draws
     *  nothing visible.
     */
    void fill_rect(const world_rect& rect, const rendering::color& fillColor,
                   alpha_value alpha = 1.0);

    /**
     * @brief Fills a world rectangle with the active theme's colour for a role.
     *
     * @param rect The rectangle to fill, in world units. Draws nothing if it is empty.
     * @param fillRole The theme colour role to fill it with, resolved when drawn.
     * @param alpha Multiplies the role colour's own alpha, rounded to the nearest 8-bit value.
     */
    void fill_rect(const world_rect& rect, theming::color_role fillRole, alpha_value alpha = 1.0);

    // Under a virtual resolution a line's thickness is in virtual pixels, and a filled rectangle's
    // corners snap to whole virtual pixels like a sprite's.
    void draw_line(const math::vector2& from, const math::vector2& to,
                   const rendering::color& lineColor, double thickness = 1.0);
    void fill_circle(const math::vector2& centre, double radius, const rendering::color& fillColor);

    /**
     * @brief Draws a whole texture stretched over a world rectangle. Same as drawing
     *  sprite_region::whole(texture).
     *
     * @param rect Where the sprite goes, in world units.
     * @param texture The texture to draw. Must outlive the frame.
     * @param options The sprite's layer, tint, alpha and transform.
     */
    void draw_sprite(const world_rect& rect, const sprite_texture& texture,
                     const sprite_options& options = {});

    /**
     * @brief Draws a region of a texture, such as an atlas tile, stretched over a world
     *  rectangle.
     *
     * The sprite is a single textured quad with no anti-aliasing fringe. It follows the camera's
     * pan and zoom, and its edges are snapped to whole pixels so adjacent tiles show no seam, even
     * flipped or turned by whole quarter turns; a sprite turned by any other angle is not snapped.
     * It is sampled with the surface's sprite_filter(), clipped to the surface, and culled with no
     * geometry at all when its transformed quad lies outside the visible world.
     *
     * Draws nothing if the rectangle is empty, has zero area or is not finite, if the region's
     * texture is empty, or if the options' origin, scale or rotation cannot be drawn.
     *
     * @param rect Where the sprite goes, in world units.
     * @param region The part of a texture to draw. Its texture must outlive the frame.
     * @param options The sprite's layer, tint, alpha and transform: its origin, scale (a negative
     *  component flips it) and rotation.
     */
    void draw_sprite(const world_rect& rect, const sprite_region& region,
                     const sprite_options& options = {});

    /**
     * @brief How what is drawn from now on blends with what is underneath: every sprite and shape
     *  drawn after the call, until the next one. A sprite held back for its layer keeps the mode it
     *  was drawn with.
     *
     *  Each renderable starts drawing with blend_mode::normal, and the surface switches back to it
     *  when it ends, so neither another renderable nor whatever the page draws after the surface
     *  sees what a renderable left.
     *
     *  On a renderer that cannot blend additively (see can_blend_additively()), additive draws are
     *  drawn with normal blending instead.
     *
     * @param mode The blend mode to draw with.
     */
    void set_blend_mode(blend_mode mode) noexcept;

    /**
     * @brief The blend mode what is drawn next blends with, as last set.
     */
    [[nodiscard]] blend_mode get_blend_mode() const noexcept;

    /**
     * @brief Tells whether the renderer can draw blend_mode::additive. If it cannot, a game may
     *  rather leave its additive draws out than draw them as solid shapes.
     */
    [[nodiscard]] bool can_blend_additively() const noexcept;

private:
    friend class scene_surface;

    // virtualScale is the layout's integer scale under a virtual resolution, where bounds is its
    // content; nullopt without one.
    scene_painter(const camera_2d& camera, const screen_rect& bounds, texture_filter spriteFilter,
                  std::optional<std::uint32_t> virtualScale = std::nullopt) noexcept;

    // Maps a world point to the window's screen space: the camera's viewport space (virtual pixels
    // under a virtual resolution), scaled and offset by where the surface was laid out.
    Eigen::Vector2d to_screen(const Eigen::Vector2d& world) const noexcept;

    // The same, snapped to a whole pixel: a whole virtual pixel under a virtual resolution, so
    // every virtual pixel spans exactly the scale in screen pixels.
    Eigen::Vector2d to_snapped_screen(const Eigen::Vector2d& world) const noexcept;

    // How many screen pixels one viewport pixel spans: the virtual scale, or 1.
    double pixel_scale() const noexcept;

    // Switch the backend's sampler only when what is drawn next needs the other one, so a run of
    // sprites costs one switch however many tiles it has. Shapes, and whatever the page draws after
    // the scene, are always sampled linearly.
    void sample_as_sprites();
    void sample_as_shapes();

    // Switch the backend's blending only when what is drawn next blends the other way, sampling as
    // nextFilter says if it switches to additive. Going back to normal blending resets the
    // backend's render state, which samples linearly again.
    void blend_as(blend_mode mode, texture_filter nextFilter);

    // A sprite resolved to what the draw list needs: its screen corners, in the order of the UV
    // corners they carry (top-left, top-right, bottom-right, bottom-left), its UVs and its tint.
    // An upright sprite (unturned or turned half way, possibly flipped) keeps its UV axes along the
    // screen's, so a plain image can draw it; any other one needs a full quad.
    struct placed_sprite {
        int layer{};
        std::array<Eigen::Vector2d, 4> corners{};
        bool upright{true};
        uv_rect uv{};
        texture_id texture{NO_TEXTURE_ID};
        std::uint32_t tint{};
        blend_mode blend{blend_mode::normal};
    };

    void add_sprite(const placed_sprite& sprite);

    // Sprites on a layer other than 0 are held back while the scene draws, then drawn in layer
    // order (call order within a layer) when it ends: the positive ones over the shapes, the
    // negative ones into the channel beneath them. Leaves the sampler linear and the blending
    // normal, so each channel ends in the state the next one starts from.
    enum class layer_side : std::uint8_t {
        above_shapes,
        below_shapes
    };
    void draw_held_sprites(layer_side side);

    const camera_2d& m_camera;
    screen_rect m_bounds;
    texture_filter m_spriteFilter;
    std::optional<std::uint32_t> m_virtualScale;
    bool m_samplingNearest{false};
    blend_mode m_blendMode{blend_mode::normal};
    bool m_blendingAdditive{false};
    std::vector<placed_sprite> m_spritesAbove{};
    std::vector<placed_sprite> m_spritesBelow{};
};

/**
 * @brief A widget reserving a rectangle of the window for a scene: it owns a camera_2d kept in
 *  sync with its size, clips drawing to its bounds and draws its scene_renderables through a
 *  scene_painter.
 *
 *  Drawing context flows one way: the surface knows its renderables and hands each the
 *  render_context of the current draw, and a renderable never needs a reference back to the
 *  surface.
 */
class scene_surface final : public rendering::renderable {
public:
    explicit scene_surface(std::string_view id, const math::vector2& size = math::vector2::zero());
    scene_surface(std::string_view id, fill_window_t fillWindow);

    scene_surface(const scene_surface&) = delete;
    scene_surface& operator=(const scene_surface&) = delete;
    scene_surface(scene_surface&&) = delete;
    scene_surface& operator=(scene_surface&&) = delete;

    camera_2d& camera() noexcept;
    const camera_2d& camera() const noexcept;

    /**
     * @brief The bounds the surface was laid out at on its last render. Empty before the first.
     */
    screen_rect bounds() const noexcept;

    /**
     * @brief Adds a renderable to draw into the surface, after the ones already added. The surface
     *  does not take ownership: the renderable must stay alive while it is added. Adding the same
     *  renderable twice does nothing.
     *
     *  Must not be called while the surface draws, i.e. from a scene_renderable::render().
     *
     * @param renderable The renderable to draw.
     */
    void add_renderable(scene_renderable& renderable);

    /**
     * @brief Removes a renderable, so it is no longer drawn. Does nothing if it was not added.
     *
     *  Must not be called while the surface draws, i.e. from a scene_renderable::render().
     *
     * @param renderable The renderable to remove.
     */
    void remove_renderable(scene_renderable& renderable);

    std::optional<theming::color_role> background_role() const noexcept;
    void set_background_role(std::optional<theming::color_role> role);

    const std::optional<grid_options>& grid() const noexcept;
    void set_grid(std::optional<grid_options> options);

    /**
     * @brief How the surface samples its sprites. texture_filter::nearest by default.
     */
    texture_filter sprite_filter() const noexcept;

    /**
     * @brief Changes how the surface samples its sprites, from the next render() on.
     *
     *  Nearest sampling only applies around sprites: shapes, the background, the grid and whatever
     *  the page draws after the surface are still sampled linearly. On a renderer that has no
     *  sampler callbacks, sprites still draw, sampled however the renderer samples.
     *
     * @param filter The filter to sample sprites with.
     */
    void set_sprite_filter(texture_filter filter) noexcept;

    /**
     * @brief The fixed resolution the surface presents, or nullopt when it shows its camera at its
     *  laid-out size (the default).
     */
    std::optional<virtual_size> virtual_resolution() const noexcept;

    /**
     * @brief Makes the surface present a fixed virtual resolution, from the next render() on, or
     *  stops it with nullopt.
     *
     *  The virtual screen is scaled by the largest whole number that fits the surface's size in
     *  physical pixels (whatever the UI scale), centred, and letterboxed with letterbox_role(). The
     *  camera's viewport becomes the virtual resolution, so one world unit is one virtual pixel at
     *  zoom 1, and the painter snaps what it draws to whole virtual pixels.
     *
     * @param resolution The virtual resolution, or nullopt.
     * @throws std::invalid_argument if the resolution is zero on either axis, leaving the surface
     *  as it was.
     */
    void set_virtual_resolution(std::optional<virtual_size> resolution);

    /**
     * @brief The theme colour role the letterbox bars are filled with.
     *  color_role::window_background by default.
     */
    theming::color_role letterbox_role() const noexcept;
    void set_letterbox_role(theming::color_role role) noexcept;

    /**
     * @brief Where the virtual resolution landed on the last render, or nullopt if the surface
     *  presents none or has not rendered since it was given one.
     */
    const std::optional<pixel_perfect_layout>& presentation_layout() const noexcept;

    /**
     * @brief Maps a screen point, such as the pointer, to the camera's viewport space: virtual
     *  pixels when the surface presents a virtual resolution, the surface's own pixels otherwise.
     *  camera().screen_to_world() takes it on to the world.
     *
     * @return std::optional<math::vector2> The point, or nullopt if it lies outside what the
     *  surface shows (in a letterbox bar, or past its bounds), or before the first render.
     */
    std::optional<math::vector2> screen_to_virtual(const math::vector2& screenPoint) const noexcept;

    /**
     * @brief Maps a point of the camera's viewport space to the screen: the inverse of
     *  screen_to_virtual() for any point it maps.
     */
    math::vector2 virtual_to_screen(const math::vector2& virtualPoint) const noexcept;

    /**
     * @brief Lays the surface out, syncs its camera's viewport to its size, then paints the
     *  background, the grid and then each of its scene renderables in the order they were added,
     *  all clipped to the surface and through one painter, so sprite layers order across
     *  renderables. Called once per frame, between ImGui's NewFrame() and Render().
     */
    void render() override;

private:
    void render_in_page();
    void render_fill_window();
    void lay_out(const screen_rect& bounds);
    void draw_scene();

    std::string m_id{};
    math::vector2 m_size{};
    bool m_fillWindow{false};

    camera_2d m_camera{};

    screen_rect m_bounds{};
    // A list, like rendering::renderable_view: cheap to add to and remove from, no random access.
    std::list<gsl::not_null<scene_renderable*>> m_renderables{};
    std::optional<theming::color_role> m_backgroundRole{theming::color_role::surface};
    std::optional<grid_options> m_grid{};
    texture_filter m_spriteFilter{texture_filter::nearest};
    std::optional<virtual_size> m_virtualResolution{};
    theming::color_role m_letterboxRole{theming::color_role::window_background};
    std::optional<pixel_perfect_layout> m_presentationLayout{};
};

static_assert(widgets::widget<scene_surface>);

} // namespace tca::engine::scene
