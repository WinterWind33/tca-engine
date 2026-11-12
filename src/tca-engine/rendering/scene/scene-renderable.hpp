// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <functional>

namespace tca::engine::scene {

class scene_painter;
class scene_surface;

/**
 * @brief Everything a scene_renderable is handed for one render: for now, the painter it draws
 *  through. The scene counterpart of rendering::update_context, so what a renderable gets can grow
 *  without its render() signature changing.
 *
 *  Only a scene_surface builds one, for the duration of its draw. It must not be kept past the
 *  render() call it is handed to, and neither must anything it hands out.
 */
class render_context final {
public:
    render_context(const render_context&) = delete;
    render_context& operator=(const render_context&) = delete;
    render_context(render_context&&) = delete;
    render_context& operator=(render_context&&) = delete;
    ~render_context() noexcept = default;

    /**
     * @brief World-space drawing primitives, clipped to the surface. The same painter for every
     *  renderable of one draw, so sprite layers order across all of them.
     */
    [[nodiscard]] scene_painter& painter() const noexcept;

private:
    friend class scene_surface;

    explicit render_context(scene_painter& painter) noexcept;

    scene_painter& m_painter;
};

/**
 * @brief Something drawn into a scene_surface: a layer of a scene, a tilemap, a set of sprites.
 *
 *  The surface holds its scene renderables without owning them and draws them in the order they
 *  were added, after its background and grid. Each one is handed the context of the current draw,
 *  so drawing needs no reference back to the surface.
 */
struct scene_renderable {
    virtual ~scene_renderable() noexcept = default;

    /**
     * @brief Draws into the surface through the context's painter. Called once per frame by the
     *  surface the renderable was added to, while that surface draws.
     *
     * @param context What this render is handed. Only valid for this call.
     */
    virtual void render(render_context& context) = 0;
};

/**
 * @brief Adapts a std::function<void(render_context&)> into a scene_renderable, so callers that
 *  just need to draw a few shapes aren't forced to define their own class for it. Same role as
 *  widgets::callback_renderable for plain renderables.
 */
class callback_scene_renderable final : public scene_renderable {
public:
    using render_callback = std::function<void(render_context&)>;

    explicit callback_scene_renderable(render_callback callback) noexcept;

    /**
     * @brief Calls the callback with the context, if there is a callback.
     */
    void render(render_context& context) override;

private:
    render_callback m_onRender{};
};

} // namespace tca::engine::scene
