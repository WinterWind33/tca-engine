// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <array>
#include <tuple>

// Engine
#include "input/actions/action-map.hpp"
#include "input/actions/action-schema.hpp"
#include "input/key.hpp"

namespace tca::engine::input {

/**
 * @brief The actions most 2D games need, ready to use: movement, two action buttons, menu
 *  navigation and pause. A game whose controls fit them uses standard_action_map and declares
 *  nothing; one that needs other actions declares its own enum and action_schema instead (the two
 *  can also live side by side, e.g. standard actions for menus and the game's own for gameplay).
 *
 *  Default bindings are WASD-first: E and R sit next to the movement keys for the action buttons,
 *  with space as a second primary. The arrows also move. Every action is rebindable like any
 *  other.
 */
enum class standard_action {
    move_up,
    move_down,
    move_left,
    move_right,
    /**
     * @brief The main gameplay button (jump, interact...).
     */
    primary,
    /**
     * @brief The second gameplay button (shoot, use item...).
     */
    secondary,
    /**
     * @brief Accepts the selected menu entry.
     */
    confirm,
    /**
     * @brief Goes back from a menu. Shares escape with pause: whoever handles it first consumes
     *  it (an open menu registered before gameplay closes without also pausing).
     */
    cancel,
    pause
};

} // namespace tca::engine::input

template <>
struct tca::engine::input::action_schema<tca::engine::input::standard_action> {
    static constexpr auto actions{
        std::make_tuple(
            action{.name = "move_up",
                   .id = standard_action::move_up,
                   .default_bindings = std::array{binding{key::w}, binding{key::arrow_up}}        },
            action{.name = "move_down",
                   .id = standard_action::move_down,
                   .default_bindings = std::array{binding{key::s}, binding{key::arrow_down}}      },
            action{.name = "move_left",
                   .id = standard_action::move_left,
                   .default_bindings = std::array{binding{key::a}, binding{key::arrow_left}}      },
            action{.name = "move_right",
                   .id = standard_action::move_right,
                   .default_bindings = std::array{binding{key::d}, binding{key::arrow_right}}     },
            action{.name = "primary",
                   .id = standard_action::primary,
                   .default_bindings = std::array{binding{key::e}, binding{key::space}}           },
            action{.name = "secondary",
                   .id = standard_action::secondary,
                   .default_bindings = std::array{binding{key::r}}                                },
            action{.name = "confirm",
                   .id = standard_action::confirm,
                   .default_bindings = std::array{binding{key::enter}, binding{key::numpad_enter}}},
            action{.name = "cancel",
                   .id = standard_action::cancel,
                   .default_bindings = std::array{binding{key::escape}, binding{key::backspace}}  },
            action{.name = "pause",
                   .id = standard_action::pause,
                   .default_bindings = std::array{binding{key::escape}, binding{key::pause}}      }
            )
    };
};

namespace tca::engine::input {

/**
 * @brief An action_map over the standard actions.
 */
using standard_action_map = action_map<standard_action>;

} // namespace tca::engine::input
