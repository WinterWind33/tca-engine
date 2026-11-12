// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/actions/standard-actions.hpp"

#include "input/actions/action-map.hpp"
#include "input/actions/action-schema.hpp"
#include "input/input-frame.hpp"
#include "input/key.hpp"

// C++ STL
#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace {

using tca::engine::input::standard_action;

constexpr std::array all_standard_actions{
    standard_action::move_up,    standard_action::move_down, standard_action::move_left,
    standard_action::move_right, standard_action::primary,   standard_action::secondary,
    standard_action::confirm,    standard_action::cancel,    standard_action::pause};

[[nodiscard]] bool has_bindings(std::span<const tca::engine::input::binding> actual,
                                std::initializer_list<tca::engine::input::binding> expected) {
    return std::ranges::equal(actual, expected);
}

// How many schema entries declare the given action.
[[nodiscard]] constexpr std::size_t schema_entries_for(standard_action actionId) {
    return std::apply(
        [actionId](const auto&... entries) {
            return (std::size_t{0} + ... + (entries.id == actionId ? 1U : 0U));
        },
        tca::engine::input::action_schema<standard_action>::actions);
}

// Whether no two schema entries share a name.
[[nodiscard]] constexpr bool schema_names_are_unique() {
    return std::apply(
        [](const auto&... entries) {
            const std::array<std::string_view, sizeof...(entries)> names{entries.name...};
            for (std::size_t i{0}; i < names.size(); ++i) {
                if (names[i].empty()) {
                    return false;
                }
                for (std::size_t j{i + 1}; j < names.size(); ++j) {
                    if (names[i] == names[j]) {
                        return false;
                    }
                }
            }
            return true;
        },
        tca::engine::input::action_schema<standard_action>::actions);
}

// A game's own actions, used next to the standard ones.
enum class gameplay_action {
    dash
};

} // namespace

template <>
struct tca::engine::input::action_schema<gameplay_action> {
    static constexpr auto actions{
        std::make_tuple(action{.name = "dash",
                               .id = gameplay_action::dash,
                               .default_bindings = std::array{binding{key::c}}})};
};

TEST_CASE("standard_action schema", "[tca][engine][input][actions][standard-actions]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(has_action_schema<standard_action>);
    STATIC_REQUIRE(std::is_same_v<standard_action_map, action_map<standard_action>>);
    STATIC_REQUIRE(
        std::tuple_size_v<std::remove_cvref_t<decltype(action_schema<standard_action>::actions)>> ==
        all_standard_actions.size());
    STATIC_REQUIRE(schema_names_are_unique());

    STATIC_REQUIRE(std::ranges::all_of(all_standard_actions, [](standard_action a) {
        return schema_entries_for(a) == 1;
    }));
}

TEST_CASE("standard_action_map default bindings",
          "[tca][engine][input][actions][standard-actions]") {
    using namespace tca::engine::input;

    const standard_action_map actions{};

    REQUIRE(has_bindings(actions.bindings_of(standard_action::move_up),
                         {binding{key::w}, binding{key::arrow_up}}));
    REQUIRE(has_bindings(actions.bindings_of(standard_action::move_down),
                         {binding{key::s}, binding{key::arrow_down}}));
    REQUIRE(has_bindings(actions.bindings_of(standard_action::move_left),
                         {binding{key::a}, binding{key::arrow_left}}));
    REQUIRE(has_bindings(actions.bindings_of(standard_action::move_right),
                         {binding{key::d}, binding{key::arrow_right}}));
    REQUIRE(has_bindings(actions.bindings_of(standard_action::primary),
                         {binding{key::e}, binding{key::space}}));
    REQUIRE(has_bindings(actions.bindings_of(standard_action::secondary), {binding{key::r}}));
    REQUIRE(has_bindings(actions.bindings_of(standard_action::confirm),
                         {binding{key::enter}, binding{key::numpad_enter}}));
    REQUIRE(has_bindings(actions.bindings_of(standard_action::cancel),
                         {binding{key::escape}, binding{key::backspace}}));
    REQUIRE(has_bindings(actions.bindings_of(standard_action::pause),
                         {binding{key::escape}, binding{key::pause}}));
}

TEST_CASE("standard_action_map in use", "[tca][engine][input][actions][standard-actions]") {
    using namespace tca::engine::input;

    standard_action_map actions{};
    input_frame frame{};
    const input_view input{frame};

    SECTION("Arrows and WASD both move") {
        frame.keyboard().press(key::arrow_left);
        frame.keyboard().press(key::d);

        REQUIRE(actions.is_active(input, standard_action::move_left));
        REQUIRE(actions.is_active(input, standard_action::move_right));
        REQUIRE_FALSE(actions.is_active(input, standard_action::move_up));
        REQUIRE_FALSE(actions.is_active(input, standard_action::move_down));
    }

    SECTION("Escape triggers both cancel and pause until one consumes it") {
        frame.keyboard().press(key::escape);

        REQUIRE(actions.was_triggered(input, standard_action::cancel));
        REQUIRE(actions.was_triggered(input, standard_action::pause));

        actions.consume(input, standard_action::cancel);

        REQUIRE_FALSE(actions.was_triggered(input, standard_action::pause));
    }

    SECTION("Consuming cancel leaves pause's own key alone") {
        frame.keyboard().press(key::pause);
        actions.consume(input, standard_action::cancel);

        REQUIRE(actions.was_triggered(input, standard_action::pause));
    }

    SECTION("Standard actions are rebindable") {
        actions.rebind(standard_action::primary, {binding{key::arrow_up}});
        frame.keyboard().press(key::arrow_up);

        REQUIRE(actions.was_triggered(input, standard_action::primary));
        REQUIRE(actions.is_active(input, standard_action::move_up));
    }

    SECTION("A game's own map reads the same input next to the standard one") {
        const action_map<gameplay_action> gameplay{};
        frame.keyboard().press(key::c);
        frame.keyboard().press(key::e);

        REQUIRE(gameplay.was_triggered(input, gameplay_action::dash));
        REQUIRE(actions.was_triggered(input, standard_action::primary));

        gameplay.consume(input, gameplay_action::dash);

        REQUIRE_FALSE(gameplay.was_triggered(input, gameplay_action::dash));
        REQUIRE(actions.was_triggered(input, standard_action::primary));
    }
}
