// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <map>
#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

// Engine
#include "input/actions/action-schema.hpp"
#include "input/input-frame.hpp"
#include "input/key.hpp"
#include "input/keyboard-state.hpp"
#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"

namespace tca::engine::input {

/**
 * @brief The current bindings of every action an action_schema declares, and the queries a game
 *  reads its actions with. Built from the schema's default bindings; rebindable at run time.
 *
 *  An action reads through its bindings, any of which is enough: it is active while any binding is
 *  down, triggered when any binding was pressed and released when any binding was released. A
 *  binding consumed earlier in the update pass reads as untouched, like any consumed input.
 *
 *  Queries never consume: a game that handled an action calls consume() to hide its
 *  bindings from every later reader, as with raw keys. Consuming on a query would hide a held
 *  binding until it is pressed again, so a held action would only read active for one tick.
 *
 *  A binding is a keyboard key or a mouse button, read from the keyboard or the mouse of the input
 *  accordingly. Raw keyboard and mouse queries stay available next to actions, for input that
 *  must not be rebindable.
 *
 * @tparam Action The game's action enum, with an action_schema specialization. Querying with any
 *  other type does not compile.
 */
template <has_action_schema Action>
class action_map final {
public:
    /**
     * @brief Creates a map with every action of the schema bound to its default bindings.
     */
    action_map();

    /**
     * @brief Whether any binding of the given action is held down in the given input.
     */
    [[nodiscard]] bool is_active(input_view input, Action actionId) const noexcept;

    /**
     * @brief Whether any binding of the given action was pressed this tick in the given input.
     */
    [[nodiscard]] bool was_triggered(input_view input, Action actionId) const noexcept;

    /**
     * @brief Whether any binding of the given action was released this tick in the given input.
     */
    [[nodiscard]] bool was_released(input_view input, Action actionId) const noexcept;

    /**
     * @brief Consumes every binding of the given action in the given input, held or not: from
     *  then on they read as untouched to every later reader, through this map, any other map or
     *  the raw keyboard and mouse. See keyboard_state::consume() and mouse_state::consume().
     */
    void consume(input_view input, Action actionId) const noexcept;

    /**
     * @brief Replaces every binding of the given action. An empty list leaves it unbound, never
     *  active. Ignored for an enum value the schema doesn't declare.
     */
    void rebind(Action actionId, std::vector<binding> bindings);

    /**
     * @brief The current bindings of the given action, in the order they were given; empty for an
     *  unbound action or an enum value the schema doesn't declare. Valid until the next rebind()
     *  or reset_to_defaults().
     */
    [[nodiscard]] std::span<const binding> bindings_of(const Action actionId) const noexcept {
        const auto it{m_bindings.find(actionId)};
        if (it == std::ranges::cend(m_bindings)) {
            return {};
        }

        return std::span<const binding>{it->second};
    }

    /**
     * @brief Binds every action back to the schema's default bindings.
     */
    void reset_to_defaults();

private:
    std::map<Action, std::vector<binding>> m_bindings{};

    /**
     * @brief Which of the per-source readers a query asks (is_down, was_pressed, was_released):
     *  keyboard_view and mouse_view name theirs the same.
     */
    enum class binding_state : std::uint8_t {
        down,
        pressed,
        released,
    };

    /**
     * @brief Whether the given reader of the given view answers true for the given source.
     */
    template <typename View, typename Source>
    [[nodiscard]] static bool read_source(const View view, const Source source,
                                          const binding_state state) noexcept {
        switch (state) {
            case binding_state::down:
                return view.is_down(source);
            case binding_state::pressed:
                return view.was_pressed(source);
            case binding_state::released:
                return view.was_released(source);
        }

        return false;
    }

    /**
     * @brief Whether the given binding is in the given state, read from the keyboard for a key
     *  and from the mouse for a mouse button.
     */
    [[nodiscard]] static bool check_binding_state(const binding& b, const input_view inputView,
                                                  const binding_state state) noexcept {
        if (const key* const boundKey{std::get_if<key>(&b)}; boundKey) {
            return read_source(inputView.keyboard(), *boundKey, state);
        }

        if (const mouse_button* const boundButton{std::get_if<mouse_button>(&b)}; boundButton) {
            return read_source(inputView.mouse(), *boundButton, state);
        }

        return false;
    }

    [[nodiscard]] bool query_state_impl(const input_view inputView, const Action actionId,
                                        const binding_state state) const noexcept {
        const std::span<const binding> inputBindings{bindings_of(actionId)};
        for (const auto& b : inputBindings) {
            if (check_binding_state(b, inputView, state)) {
                // In this case, the user pressed a key or a button which maps to the
                // requested action id, so let's return true.
                return true;
            }
        }

        return false;
    }
};

// === Implementations ===
template <has_action_schema Action>
action_map<Action>::action_map() {
    reset_to_defaults();
}

template <has_action_schema Action>
bool action_map<Action>::is_active(const input_view inputView,
                                   const Action actionId) const noexcept {
    return query_state_impl(inputView, actionId, binding_state::down);
}

template <has_action_schema Action>
bool action_map<Action>::was_triggered(const input_view inputView,
                                       const Action actionId) const noexcept {
    return query_state_impl(inputView, actionId, binding_state::pressed);
}

template <has_action_schema Action>
bool action_map<Action>::was_released(const input_view inputView,
                                      const Action actionId) const noexcept {
    return query_state_impl(inputView, actionId, binding_state::released);
}

template <has_action_schema Action>
void action_map<Action>::consume(const input_view input, const Action actionId) const noexcept {
    for (const auto& b : bindings_of(actionId)) {
        if (auto* const keyboardAlt{std::get_if<key>(&b)}; keyboardAlt) {
            input.keyboard().consume(*keyboardAlt);
        } else if (auto* const mouseAlt{std::get_if<mouse_button>(&b)}; mouseAlt) {
            input.mouse().consume(*mouseAlt);
        }
    }
}

template <has_action_schema Action>
void action_map<Action>::reset_to_defaults() {
    std::apply(
        [this](const auto&... entries) {
            (m_bindings.insert_or_assign(entries.id,
                                         std::vector<binding>(entries.default_bindings.begin(),
                                                              entries.default_bindings.end())),
             ...);
        },
        action_schema<Action>::actions);
}

template <has_action_schema Action>
void action_map<Action>::rebind(const Action actionId, std::vector<binding> bindings) {
    const auto it{m_bindings.find(actionId)};
    if (it == std::ranges::end(m_bindings)) {
        return;
    }

    it->second = std::move(bindings);
}

} // namespace tca::engine::input
