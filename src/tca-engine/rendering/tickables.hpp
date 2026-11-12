// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <chrono>
#include <list>

// Third-party
#include <gsl/gsl>

// Engine
#include "input/input-frame.hpp"

namespace tca::engine::rendering {

/**
 * @brief Everything an updatable is handed for one update: the time elapsed since the previous
 *  one and the input of the current frame.
 *
 *  The input is an input::input_view, never the frame itself: an updatable can read and consume
 *  input, not write it. Every updatable of a pass gets the same context, in registration order, so
 *  what one consumes is hidden from the ones after it.
 *
 *  A context doesn't own the frame its input looks at, so it must not outlive it.
 */
class update_context final {
public:
    /**
     * @brief The type of the delta time. Alias for std::chrono::microseconds.
     */
    using delta_time = std::chrono::microseconds;

    /**
     * @brief Creates a context for one update.
     *
     * @param delta The time elapsed since the previous update.
     * @param input The input of the current frame.
     */
    update_context(delta_time delta, input::input_view input) noexcept;

    /**
     * @brief The time elapsed since the previous update.
     */
    [[nodiscard]] delta_time delta() const noexcept;

    /**
     * @brief The input of the current frame, read-and-consume only.
     */
    [[nodiscard]] input::input_view input() const noexcept;

private:
    delta_time m_delta{};
    input::input_view m_input;
};

/**
 * @brief An updatable is an object that is updated once per frame with an update_context: the
 * time elapsed since the last update and the input of the frame. This is useful for objects that
 * need to update their state over time or react to the player, such as animations, physics
 * simulations, or other time-dependent processes.
 */
struct updatable {
    /**
     * @brief The type of the delta time used for updates. Alias for std::chrono::microseconds.
     */
    using delta_time = update_context::delta_time;
    virtual ~updatable() noexcept = default;

    /**
     * @brief Updates the state of the object. The context gives the time elapsed since the last
     * update, which can be used to calculate how much the state of the object should change, and
     * the input of the current frame.
     *
     * @param context The delta time and the input of this update.
     */
    virtual void update(update_context& context) = 0;
};

/**
 * @brief A renderable is an object that can be rendered. This is useful for objects that need to be
 * drawn on the screen, such as sprites, models, or other visual elements. The render() function
 * should contain the logic to draw the object on the screen, and it will be called every frame by
 * the rendering system.
 */
struct renderable {
    virtual ~renderable() noexcept = default;

    /**
     * @brief Renders the object on the screen. This method should contain the logic to draw the
     * object on the screen, and it will be called every frame by the rendering system.
     */
    virtual void render() = 0;
};

/**
 * @brief A renderable_view is a renderable that can contain other renderables but it's not the
 * owner of them. This is useful for objects that need to render multiple child renderables, such as
 * layout widgets, containers, or other composite visual elements.
 */
class renderable_view : public renderable {
public:
    virtual ~renderable_view() noexcept = default;

    /**
     * @brief Adds a renderable to the view. This class does not take ownership of the renderable,
     * so the caller is responsible for managing the lifetime of the renderable and ensuring that it
     * remains valid while it's added to the view.
     *
     * @note If the same renderable is added multiple times to the view, it will only be added once,
     * and the on_renderable_added() callback will only be called once for that renderable.
     *
     * @param renderable The renderable to add to the view.
     */
    void add_renderable(renderable& renderable);

    /**
     * @brief Removes a renderable from the view. This class does not take ownership of the
     * renderable, so the caller is responsible for managing the lifetime of the renderable and
     * ensuring that it remains valid while it's added to the view. If the renderable is not found
     * in the view, this function does nothing.
     *
     * @param renderable The renderable to remove from the view.
     */
    void remove_renderable(renderable& renderable);

    /**
     * @brief Renders the view and all its child renderables. This method will call the render()
     * method of each renderable added to the view. If there are no renderables in the view, this
     * method does nothing.
     */
    void render() override;

protected:
    /**
     * @brief Called when a renderable is added to the view.
     */
    virtual void on_renderable_added(renderable&) {}

    /**
     * @brief Called when a renderable is being removed from the view. This is called before the
     * renderable is removed from the view, so the renderable can perform any necessary cleanup or
     * state updates before it is removed.
     */
    virtual void on_renderable_removing(renderable&) {}

    // We use a list because it's more efficient to add/remove renderables from the container, and
    // we don't need random access to the renderables.
    std::list<gsl::not_null<renderable*>> m_renderables{};
};

/**
 * @brief An updatable_view is an updatable that can contain other updatables but it's not the owner
 * of them. This is useful for objects that need to update multiple child updatables, such as layout
 * widgets, containers, or other composite elements that need to update their state based on the
 * time elapsed since the last update.
 */
class updatable_view : public updatable {
public:
    virtual ~updatable_view() noexcept = default;

    /**
     * @brief Adds an updatable to the view. This class does not take ownership of the updatable, so
     * the caller is responsible for managing the lifetime of the updatable and ensuring that it
     * remains valid while it's added to the view.
     *
     * @note If the same updatable is added multiple times to the view, it will only be added once,
     * and the on_updatable_added() callback will only be called once for that updatable.
     *
     * @param updatable The updatable to add to the view.
     */
    void add_updatable(updatable& updatable);

    /**
     * @brief Removes an updatable from the view. If the updatable is not found in the view, this
     * function does nothing.
     *
     * @param updatable The updatable to remove from the view.
     */
    void remove_updatable(updatable& updatable);

    /**
     * @brief Updates the view and all its child updatables. This method will call the update()
     * method of each updatable added to the view, in the order they were added, passing each the
     * same context. If there are no updatables in the view, this method does nothing.
     *
     * @param context The context passed on to each child updatable's update() method.
     */
    void update(update_context& context) override;

protected:
    /**
     * @brief Called when an updatable is added to the view.
     */
    virtual void on_updatable_added(updatable&) {}

    /**
     * @brief Called when an updatable is being removed from the view. This is called before the
     * updatable is removed from the view, so the updatable can perform any necessary cleanup or
     * state updates before it is removed.
     */
    virtual void on_updatable_removing(updatable&) {}

    // Using std::list because it's more efficient to add/remove updatables from the container, and
    // we don't need random access to the updatables.
    std::list<gsl::not_null<updatable*>> m_updatables{};
};

/**
 * @brief A pausable_renderables_view is a renderable_view that can pause and resume the rendering
 * of its child renderables.
 */
class pausable_renderables_view : public renderable_view {
public:
    /**
     * @brief Pauses the rendering of the given renderable. When a renderable is paused, it will not
     * be rendered when the render() method of the view is called. If the renderable is not found in
     * the view, this function throws. If the renderable is already paused, this function does
     * nothing.
     *
     * @param renderable The renderable to pause.
     */
    void pause_renderable(renderable& renderable);

    /**
     * @brief Resumes the rendering of the given renderable. When a renderable is resumed, it will
     * be rendered when the render() method of the view is called. If the renderable is not found in
     * the view, this function throws. If the renderable is already resumed, this function does
     * nothing.
     *
     * @param renderable The renderable to resume.
     */
    void resume_renderable(renderable& renderable);

    /**
     * @brief Returns whether the given renderable is currently paused, i.e. whether it would be
     * skipped by render(). If the renderable is not found in the view, this function throws.
     *
     * @param renderable The renderable to query.
     * @return true if the renderable is currently paused, false otherwise.
     */
    [[nodiscard]] bool is_renderable_paused(const renderable& renderable) const;

    void render() override;

protected:
    void on_renderable_added(renderable& renderable) override;
    void on_renderable_removing(renderable& renderable) override;

private:
    std::list<bool> m_renderablesPauseState{};
};

/**
 * @brief A pausable_updatables_view is an updatable_view that can pause and resume the updating of
 * its child updatables.
 */
class pausable_updatables_view : public updatable_view {
public:
    /**
     * @brief Pauses the updating of the given updatable. When an updatable is paused, it will not
     * be updated when the update() method of the view is called. If the updatable is not found in
     * the view, this function throws. If the updatable is already paused, this function does
     * nothing.
     *
     * @param updatable The updatable to pause.
     */
    void pause_updatable(updatable& updatable);

    /**
     * @brief Resumes the updating of the given updatable. When an updatable is resumed, it will be
     * updated when the update() method of the view is called. If the updatable is not found in the
     * view, this function throws. If the updatable is already resumed, this function does nothing.
     *
     * @param updatable The updatable to resume.
     */
    void resume_updatable(updatable& updatable);

    /**
     * @brief Updates every child updatable that isn't paused, in the order they were added, passing
     * each the same context. Paused updatables aren't called at all.
     *
     * @param context The context passed on to each child updatable's update() method.
     */
    void update(update_context& context) override;

protected:
    void on_updatable_added(updatable& updatable) override;
    void on_updatable_removing(updatable& updatable) override;

private:
    std::list<bool> m_updatablesPauseState{};
};

} // namespace tca::engine::rendering
