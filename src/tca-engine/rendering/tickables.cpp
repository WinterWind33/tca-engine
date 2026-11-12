// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/tickables.hpp"

// C++ STL
#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

namespace tca::engine::rendering {

// Update context implementation

update_context::update_context(const delta_time delta, const input::input_view input) noexcept
    : m_delta{delta},
      m_input{input} {}

auto update_context::delta() const noexcept -> delta_time {
    return m_delta;
}

auto update_context::input() const noexcept -> input::input_view {
    return m_input;
}

namespace {
template <typename T>
bool& retrieve_pause_state(const T& object, const std::list<gsl::not_null<T*>>& objects,
                           std::list<bool>& pauseStates) {
    const auto objectIt{std::find(std::cbegin(objects), std::cend(objects), &object)};
    if (objectIt == std::cend(objects)) {
        throw std::runtime_error{"Object not found in the view."};
    }

    const auto distance{std::distance(std::cbegin(objects), objectIt)};
    // Return the state at the same distance from the pause state list.
    auto pauseStateIt{std::next(std::begin(pauseStates), distance)};
    return *pauseStateIt;
}

template <typename T>
bool get_pause_state(const T& object, const std::list<gsl::not_null<T*>>& objects,
                     const std::list<bool>& pauseStates) {
    const auto objectIt{std::find(std::cbegin(objects), std::cend(objects), &object)};
    if (objectIt == std::cend(objects)) {
        throw std::runtime_error{"Object not found in the view."};
    }

    const auto distance{std::distance(std::cbegin(objects), objectIt)};
    // Read the state at the same distance from the pause state list.
    return *std::next(std::cbegin(pauseStates), distance);
}

template <typename T>
void remove_paused_state(const T& object, std::list<gsl::not_null<T*>>& objects,
                         std::list<bool>& pauseStates) {
    const auto objectIt{std::find(std::cbegin(objects), std::cend(objects), &object)};
    if (objectIt == std::cend(objects)) {
        throw std::runtime_error{"Object not found in the view."};
    }

    const auto distance{std::distance(std::cbegin(objects), objectIt)};
    // Remove the state at the same distance from the pause state list.
    auto pauseStateIt{std::next(std::begin(pauseStates), distance)};
    pauseStates.erase(pauseStateIt);
}

template <typename T>
bool is_object_in_view(const T& object, const std::list<gsl::not_null<T*>>& objects) {
    return std::find(std::cbegin(objects), std::cend(objects), &object) != std::cend(objects);
}

template <typename T>
T* find_object_in_view(const T& object, const std::list<gsl::not_null<T*>>& objects) {
    const auto objectIt{std::find(std::cbegin(objects), std::cend(objects), &object)};
    if (objectIt == std::cend(objects)) {
        return nullptr;
    }

    return objectIt->get();
}

} // namespace

void renderable_view::add_renderable(renderable& renderable) {
    if (!is_object_in_view(renderable, m_renderables)) {
        m_renderables.push_back(&renderable);
        on_renderable_added(renderable);
    }
}

void renderable_view::render() {
    for (const gsl::not_null<renderable*>& renderable : m_renderables) {
        renderable->render();
    }
}

void renderable_view::remove_renderable(renderable& renderable) {
    if (auto* const renderablePtr{find_object_in_view(renderable, m_renderables)};
        renderablePtr != nullptr) {
        on_renderable_removing(renderable);
        m_renderables.remove(renderablePtr);
    }
}

void updatable_view::add_updatable(updatable& updatable) {
    if (!is_object_in_view(updatable, m_updatables)) {
        m_updatables.push_back(&updatable);
        on_updatable_added(updatable);
    }
}

void updatable_view::remove_updatable(updatable& updatable) {
    if (auto* const updatablePtr{find_object_in_view(updatable, m_updatables)};
        updatablePtr != nullptr) {
        on_updatable_removing(updatable);
        m_updatables.remove(updatablePtr);
    }
}

void updatable_view::update(update_context& context) {
    for (const gsl::not_null<updatable*>& updatable : m_updatables) {
        updatable->update(context);
    }
}

// Pausable renderables view implementation

void pausable_renderables_view::pause_renderable(renderable& object) {
    retrieve_pause_state<renderable>(object, m_renderables, m_renderablesPauseState) = true;
}

void pausable_renderables_view::resume_renderable(renderable& object) {
    retrieve_pause_state<renderable>(object, m_renderables, m_renderablesPauseState) = false;
}

bool pausable_renderables_view::is_renderable_paused(const renderable& object) const {
    return get_pause_state<renderable>(object, m_renderables, m_renderablesPauseState);
}

void pausable_renderables_view::render() {
    // We use assert because in release mode this won't do anything and speeds up
    // the ticks. Since this function is called every frame, we want to boost performances as much
    // as possible.
    assert(m_renderables.size() == m_renderablesPauseState.size());
    auto pauseStateIt{std::cbegin(m_renderablesPauseState)};

    for (auto renderableIt{std::cbegin(m_renderables)};
         renderableIt != std::cend(m_renderables) &&
         pauseStateIt != std::cend(m_renderablesPauseState);
         ++renderableIt, ++pauseStateIt) {
        if (!*pauseStateIt) {
            (*renderableIt)->render();
        }
    }
}

void pausable_renderables_view::on_renderable_added(renderable& object) {
    renderable_view::on_renderable_added(object);
    // When a renderable is added to the view, we assume that it's not paused, so we add a false
    // value to the pause state list.
    m_renderablesPauseState.push_back(false);
}

void pausable_renderables_view::on_renderable_removing(renderable& object) {
    renderable_view::on_renderable_removing(object);
    remove_paused_state<renderable>(object, m_renderables, m_renderablesPauseState);
}

// Pausable updatables view implementation

void pausable_updatables_view::pause_updatable(updatable& object) {
    retrieve_pause_state<updatable>(object, m_updatables, m_updatablesPauseState) = true;
}

void pausable_updatables_view::resume_updatable(updatable& object) {
    retrieve_pause_state<updatable>(object, m_updatables, m_updatablesPauseState) = false;
}

void pausable_updatables_view::update(update_context& context) {
    // We use assert because in release mode this won't do anything and speeds up
    // the ticks. Since this function is called every frame, we want to boost performances as much
    // as possible.
    assert(m_updatables.size() == m_updatablesPauseState.size());
    auto pauseStateIt{std::cbegin(m_updatablesPauseState)};

    for (auto updatableIt{std::cbegin(m_updatables)};
         updatableIt != std::cend(m_updatables) &&
         pauseStateIt != std::cend(m_updatablesPauseState);
         ++updatableIt, ++pauseStateIt) {
        if (!*pauseStateIt) {
            (*updatableIt)->update(context);
        }
    }
}

void pausable_updatables_view::on_updatable_added(updatable& object) {
    updatable_view::on_updatable_added(object);
    // When an updatable is added to the view, we assume that it's not paused, so we add a false
    // value to the pause state list.
    m_updatablesPauseState.push_back(false);
}

void pausable_updatables_view::on_updatable_removing(updatable& object) {
    updatable_view::on_updatable_removing(object);
    remove_paused_state<updatable>(object, m_updatables, m_updatablesPauseState);
}

} // namespace tca::engine::rendering
