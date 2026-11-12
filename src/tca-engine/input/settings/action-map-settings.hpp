// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

// Engine
#include "input/actions/action-map.hpp"
#include "input/actions/action-schema.hpp"
#include "input/key.hpp"
#include "input/mouse-button.hpp"
#include "settings/settings-schema.hpp"
#include "settings/settings-value-traits.hpp"

/**
 * @file
 * @brief Lets an input::action_map be a settings field, so a game saves its players' bindings the
 *  way it saves any other setting. Kept out of the input module so that input never depends on
 *  settings: this header only exists when TCA_ENGINE_BUILD_SETTINGS_MODULE is on, behind
 *  the tca-engine-input-settings target.
 *
 *  A map is written as one group with a key per action, named by the action's schema `name`, each
 *  holding the action's bindings as a list of strings. With the JSON formatter:
 *
 * @code
 *  "bindings": {
 *      "jump": ["keyboard:space", "keyboard:w"],
 *      "pause": ["keyboard:escape"],
 *      "pet": ["mouse:left"]
 *  }
 * @endcode
 *
 *  An action the file doesn't have, or whose list holds anything that isn't a binding, keeps its
 *  schema default; an empty list is an action the player unbound. Actions the file has and the
 *  schema doesn't are ignored.
 */

namespace tca::engine::input::details {

/**
 * @brief What a keyboard binding's saved form starts with, before the key's name.
 */
inline constexpr std::string_view keyboard_binding_prefix{"keyboard:"};

/**
 * @brief What a mouse binding's saved form starts with, before the button's name.
 */
inline constexpr std::string_view mouse_binding_prefix{"mouse:"};

} // namespace tca::engine::input::details

/**
 * @brief Stores a binding as its device and its name: `"keyboard:space"` for key::space, the name
 *  being key_name()'s, and `"mouse:left"` for mouse_button::left, the name being
 *  mouse_button_name()'s. Further devices get their own prefix.
 *
 *  from_storage() throws std::invalid_argument for anything else (an unknown prefix, a key name
 *  key_from_name() doesn't know, a button name mouse_button_from_name() doesn't know), which the
 *  formatter reports as unreadable.
 */
template <>
struct tca::engine::settings::settings_value_traits<tca::engine::input::binding> {
    using storage_type = std::string;

    static storage_type to_storage(const input::binding& value);
    static input::binding from_storage(storage_type value);
};

/**
 * @brief Stores an action's bindings as a list of strings, one per binding, in order. One string
 *  that isn't a binding refuses the whole list, so the action keeps its default rather than
 *  silently losing a binding.
 */
template <>
struct tca::engine::settings::settings_value_traits<std::vector<tca::engine::input::binding>> {
    using storage_type = std::vector<std::string>;

    static storage_type to_storage(const std::vector<input::binding>& value);
    static std::vector<input::binding> from_storage(storage_type value);
};

/**
 * @brief Writes an action map as a group with one entry per action of its schema, keyed by the
 *  action's name, and reads it back the same way.
 */
template <tca::engine::input::has_action_schema Action>
struct tca::engine::settings::settings_group_traits<tca::engine::input::action_map<Action>> {
    using entry_type = std::vector<input::binding>;

    template <typename EntryWriter>
    static bool write_entries(const input::action_map<Action>& group, EntryWriter&& write) {
        return std::apply(
            [&group, &write](const auto&... entries) {
                const auto writeOne{[&group, &write](const auto& entry) {
                    const auto bindings{group.bindings_of(entry.id)};

                    return write(entry.name, entry_type(bindings.begin(), bindings.end()));
                }};

                // Folded with && so the first failed write stops the walk.
                return (writeOne(entries) && ...);
            },
            input::action_schema<Action>::actions);
    }

    template <typename EntryReader>
    static bool read_entries(input::action_map<Action>& group, EntryReader&& read) {
        return std::apply(
            [&group, &read](const auto&... entries) {
                const auto readOne{[&group, &read](const auto& entry) {
                    auto loadedBindings{read(entry.name)};
                    if (!loadedBindings.has_value()) {
                        // The store reset the map before reading: the action is on its default.
                        return false;
                    }

                    group.rebind(entry.id, std::move(loadedBindings.value()));

                    return true;
                }};

                // Every action is looked up whatever the ones before it did.
                bool everyActionRead{true};
                ((everyActionRead = readOne(entries) && everyActionRead), ...);

                return everyActionRead;
            },
            input::action_schema<Action>::actions);
    }
};

// === Implementations ===
inline std::string tca::engine::settings::settings_value_traits<
    tca::engine::input::binding>::to_storage(const input::binding& value) {
    if (const auto* const k{std::get_if<input::key>(&value)}; k) {
        return std::string{input::details::keyboard_binding_prefix} +
               std::string{input::key_name(*k)};
    }

    return std::string{input::details::mouse_binding_prefix} +
           std::string{input::mouse_button_name(std::get<input::mouse_button>(value))};
}

inline tca::engine::input::binding tca::engine::settings::settings_value_traits<
    tca::engine::input::binding>::from_storage(storage_type value) {
    const std::string_view stored{value};
    if (stored.starts_with(input::details::keyboard_binding_prefix)) {
        const auto k{
            input::key_from_name(stored.substr(input::details::keyboard_binding_prefix.size()))};
        if (k.has_value()) {
            return input::binding{k.value()};
        }
    } else if (stored.starts_with(input::details::mouse_binding_prefix)) {
        const auto button{input::mouse_button_from_name(
            stored.substr(input::details::mouse_binding_prefix.size()))};
        if (button.has_value()) {
            return input::binding{button.value()};
        }
    }

    throw std::invalid_argument{"Not an input binding: \"" + value + '"'};
}

inline std::vector<std::string>
tca::engine::settings::settings_value_traits<std::vector<tca::engine::input::binding>>::to_storage(
    const std::vector<input::binding>& value) {
    std::vector<std::string> stored{};
    stored.reserve(value.size());
    for (const auto& b : value) {
        stored.push_back(settings_value_traits<input::binding>::to_storage(b));
    }

    return stored;
}

inline std::vector<tca::engine::input::binding> tca::engine::settings::settings_value_traits<
    std::vector<tca::engine::input::binding>>::from_storage(storage_type value) {
    std::vector<input::binding> bindings{};
    bindings.reserve(value.size());
    for (auto& stored : value) {
        bindings.push_back(settings_value_traits<input::binding>::from_storage(std::move(stored)));
    }

    return bindings;
}
