// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <array>
#include <cstddef>
#include <string_view>
#include <variant>

// Engine
#include "input/key.hpp"
#include "input/mouse-button.hpp"

namespace tca::engine::input {

/**
 * @brief One input source an action can be bound to: a keyboard key or a mouse button. Gamepad
 *  buttons become further alternatives without changing game code that stores or compares
 *  bindings.
 */
using binding = std::variant<key, mouse_button>;

/**
 * @brief One entry of an action_schema: the action's enum value, its stable name and the bindings
 *  it starts with.
 *
 * @tparam Action The game's action enum.
 * @tparam BindingCount How many default bindings the action has (deduced from the array).
 */
template <typename Action, std::size_t BindingCount>
struct action {
    /**
     * @brief Stable identifier of the action, used as its key when bindings are saved. Never
     *  change it once players have saved bindings.
     */
    std::string_view name{};

    /**
     * @brief The enum value games query the action with.
     */
    Action id{};

    /**
     * @brief The bindings the action has until rebound, and after action_map::reset_to_defaults().
     */
    std::array<binding, BindingCount> default_bindings{};
};

/**
 * @brief Declares a game's actions. Specialize it for the action enum, with a static constexpr
 *  `actions` tuple of action entries, one per action, mirroring settings::settings_schema:
 *
 * @code
 *  template <>
 *  struct tca::engine::input::action_schema<player_action> {
 *      static constexpr auto actions{std::make_tuple(
 *          action{.name = "jump",
 *                 .id = player_action::jump,
 *                 .default_bindings = std::array{binding{key::space}, binding{key::w}}},
 *          action{.name = "pause",
 *                 .id = player_action::pause,
 *                 .default_bindings = std::array{binding{key::escape}}},
 *          action{.name = "pet",
 *                 .id = player_action::pet,
 *                 .default_bindings = std::array{binding{mouse_button::left}}})};
 *  };
 * @endcode
 */
template <typename Action>
struct action_schema {};

/**
 * @brief Satisfied by an action enum with an action_schema specialization.
 */
template <typename Action>
concept has_action_schema = requires { action_schema<Action>::actions; };

} // namespace tca::engine::input
