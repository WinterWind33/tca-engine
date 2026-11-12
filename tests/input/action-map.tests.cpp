// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/actions/action-map.hpp"

#include "input/actions/action-schema.hpp"
#include "input/input-frame.hpp"
#include "input/key.hpp"
#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"

// C++ STL
#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace {

enum class test_action {
    jump,
    move_left,
    move_right,
    pause,
    menu_back,
    unbound,
    undeclared
};

// No action_schema specialization.
enum class schemaless_action {
    jump
};

// Has a schema, but is a different type from test_action.
enum class other_action {
    fire
};

// Bound to mouse buttons, alone or next to keys.
enum class pointer_action {
    pet,
    feed,
    back
};

} // namespace

template <>
struct tca::engine::input::action_schema<pointer_action> {
    static constexpr auto actions{
        std::make_tuple(
            action{.name = "pet",
                   .id = pointer_action::pet,
                   .default_bindings = std::array{binding{mouse_button::left}}                  },
            // A key and a button for the same action.
            action{.name = "feed",
                   .id = pointer_action::feed,
                   .default_bindings = std::array{binding{key::f}, binding{mouse_button::right}}},
            action{.name = "back",
                   .id = pointer_action::back,
                   .default_bindings = std::array{binding{mouse_button::x1}}                    }
            )
    };
};

template <>
struct tca::engine::input::action_schema<test_action> {
    static constexpr auto actions{
        std::make_tuple(
            action{.name = "jump",
                   .id = test_action::jump,
                   .default_bindings = std::array{binding{key::space}, binding{key::w}}},
            action{.name = "move_left",
                   .id = test_action::move_left,
                   .default_bindings = std::array{binding{key::a}, binding{key::arrow_left}}},
            action{.name = "move_right",
                   .id = test_action::move_right,
                   .default_bindings = std::array{binding{key::d}, binding{key::arrow_right}}},
            action{.name = "pause",
                   .id = test_action::pause,
                   .default_bindings = std::array{binding{key::escape}}},
            // Shares escape with pause: consuming one hides it from the other.
            action{.name = "menu_back",
                   .id = test_action::menu_back,
                   .default_bindings = std::array{binding{key::escape}}},
            action{.name = "unbound",
                   .id = test_action::unbound,
                   .default_bindings = std::array<binding, 0>{}}
            )
    };
    // test_action::undeclared is deliberately missing.
};

template <>
struct tca::engine::input::action_schema<other_action> {
    static constexpr auto actions{
        std::make_tuple(action{.name = "fire",
                               .id = other_action::fire,
                               .default_bindings = std::array{binding{key::enter}}})};
};

namespace {

[[nodiscard]] bool has_bindings(std::span<const tca::engine::input::binding> actual,
                                std::initializer_list<tca::engine::input::binding> expected) {
    return std::ranges::equal(actual, expected);
}

template <typename Map, typename Action>
concept queryable_with = requires(const Map map, tca::engine::input::input_view input, Action a) {
    map.is_active(input, a);
    map.was_triggered(input, a);
    map.was_released(input, a);
    map.consume(input, a);
    map.bindings_of(a);
};

template <typename Action>
concept action_map_nameable = requires { typename tca::engine::input::action_map<Action>; };

} // namespace

TEST_CASE("action_schema declares a game's actions",
          "[tca][engine][input][actions][action-schema]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(std::is_same_v<binding, std::variant<key, mouse_button>>);
    STATIC_REQUIRE(has_action_schema<test_action>);
    STATIC_REQUIRE(has_action_schema<other_action>);
    STATIC_REQUIRE_FALSE(has_action_schema<schemaless_action>);
    STATIC_REQUIRE_FALSE(has_action_schema<int>);

    constexpr auto& jump{std::get<0>(action_schema<test_action>::actions)};
    STATIC_REQUIRE(std::is_same_v<std::remove_cvref_t<decltype(jump)>, action<test_action, 2>>);
    STATIC_REQUIRE(jump.name == std::string_view{"jump"});
    STATIC_REQUIRE(jump.id == test_action::jump);
    STATIC_REQUIRE(jump.default_bindings[0] == binding{key::space});
    STATIC_REQUIRE(jump.default_bindings[1] == binding{key::w});

    constexpr auto& unbound{std::get<5>(action_schema<test_action>::actions)};
    STATIC_REQUIRE(unbound.default_bindings.empty());
}

TEST_CASE("action_map type requirements", "[tca][engine][input][actions][action-map]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(action_map_nameable<test_action>);
    STATIC_REQUIRE_FALSE(action_map_nameable<schemaless_action>);
    STATIC_REQUIRE_FALSE(action_map_nameable<int>);

    STATIC_REQUIRE(queryable_with<action_map<test_action>, test_action>);
    STATIC_REQUIRE_FALSE(queryable_with<action_map<test_action>, other_action>);
    STATIC_REQUIRE_FALSE(queryable_with<action_map<test_action>, schemaless_action>);
    STATIC_REQUIRE_FALSE(queryable_with<action_map<test_action>, int>);

    STATIC_REQUIRE(std::is_default_constructible_v<action_map<test_action>>);
    STATIC_REQUIRE(std::is_copy_constructible_v<action_map<test_action>>);
    STATIC_REQUIRE(std::is_copy_assignable_v<action_map<test_action>>);
}

TEST_CASE("action_map starts from the schema's defaults",
          "[tca][engine][input][actions][action-map]") {
    using namespace tca::engine::input;

    const action_map<test_action> actions{};

    SECTION("Every declared action has its default bindings, in schema order") {
        REQUIRE(has_bindings(actions.bindings_of(test_action::jump),
                             {binding{key::space}, binding{key::w}}));
        REQUIRE(has_bindings(actions.bindings_of(test_action::move_left),
                             {binding{key::a}, binding{key::arrow_left}}));
        REQUIRE(has_bindings(actions.bindings_of(test_action::move_right),
                             {binding{key::d}, binding{key::arrow_right}}));
        REQUIRE(has_bindings(actions.bindings_of(test_action::pause), {binding{key::escape}}));
        REQUIRE(has_bindings(actions.bindings_of(test_action::menu_back), {binding{key::escape}}));
    }

    SECTION("An action with no default bindings is unbound") {
        REQUIRE(actions.bindings_of(test_action::unbound).empty());
    }

    SECTION("An enum value the schema doesn't declare has no bindings") {
        REQUIRE(actions.bindings_of(test_action::undeclared).empty());
    }

    SECTION("A fresh map over an empty frame reads nothing") {
        input_frame frame{};
        const input_view input{frame};

        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(actions.was_triggered(input, test_action::jump));
        REQUIRE_FALSE(actions.was_released(input, test_action::jump));
    }
}

TEST_CASE("action_map reads actions through their bindings",
          "[tca][engine][input][actions][action-map]") {
    using namespace tca::engine::input;

    const action_map<test_action> actions{};
    input_frame frame{};
    const input_view input{frame};

    SECTION("Nothing pressed: no action is active, triggered or released") {
        for (const auto a : {test_action::jump, test_action::move_left, test_action::move_right,
                             test_action::pause, test_action::menu_back, test_action::unbound,
                             test_action::undeclared}) {
            REQUIRE_FALSE(actions.is_active(input, a));
            REQUIRE_FALSE(actions.was_triggered(input, a));
            REQUIRE_FALSE(actions.was_released(input, a));
        }
    }

    SECTION("Pressing a binding makes the action active and triggered") {
        frame.keyboard().press(key::space);

        REQUIRE(actions.is_active(input, test_action::jump));
        REQUIRE(actions.was_triggered(input, test_action::jump));
        REQUIRE_FALSE(actions.was_released(input, test_action::jump));
    }

    SECTION("Any of several bindings is enough") {
        frame.keyboard().press(key::w);

        REQUIRE(actions.is_active(input, test_action::jump));
        REQUIRE(actions.was_triggered(input, test_action::jump));
    }

    SECTION("A key bound to nothing activates no action") {
        frame.keyboard().press(key::s);

        for (const auto a : {test_action::jump, test_action::move_left, test_action::move_right,
                             test_action::pause, test_action::menu_back, test_action::unbound,
                             test_action::undeclared}) {
            REQUIRE_FALSE(actions.is_active(input, a));
            REQUIRE_FALSE(actions.was_triggered(input, a));
        }
    }

    SECTION("A pressed binding activates only the actions it is bound to") {
        frame.keyboard().press(key::a);

        REQUIRE(actions.is_active(input, test_action::move_left));
        REQUIRE_FALSE(actions.is_active(input, test_action::move_right));
        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
    }

    SECTION("A key bound to two actions activates both") {
        frame.keyboard().press(key::escape);

        REQUIRE(actions.was_triggered(input, test_action::pause));
        REQUIRE(actions.was_triggered(input, test_action::menu_back));
    }

    SECTION("A held binding stays active in later ticks but triggers only once") {
        frame.keyboard().press(key::space);
        frame.end_tick();

        REQUIRE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(actions.was_triggered(input, test_action::jump));
        REQUIRE_FALSE(actions.was_released(input, test_action::jump));
    }

    SECTION("Releasing the only held binding releases the action") {
        frame.keyboard().press(key::space);
        frame.end_tick();
        frame.keyboard().release(key::space);

        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(actions.was_triggered(input, test_action::jump));
        REQUIRE(actions.was_released(input, test_action::jump));
    }

    SECTION("A tap within one tick reads as triggered and released, not active") {
        frame.keyboard().press(key::space);
        frame.keyboard().release(key::space);

        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
        REQUIRE(actions.was_triggered(input, test_action::jump));
        REQUIRE(actions.was_released(input, test_action::jump));
    }

    SECTION("Releasing one of two held bindings reports a release, the action stays active") {
        frame.keyboard().press(key::space);
        frame.keyboard().press(key::w);
        frame.end_tick();
        frame.keyboard().release(key::space);

        REQUIRE(actions.is_active(input, test_action::jump));
        REQUIRE(actions.was_released(input, test_action::jump));
    }

    SECTION("Pressing a second binding while one is held triggers the action again") {
        frame.keyboard().press(key::space);
        frame.end_tick();
        frame.keyboard().press(key::w);

        REQUIRE(actions.is_active(input, test_action::jump));
        REQUIRE(actions.was_triggered(input, test_action::jump));
    }

    SECTION("Focus loss releases a held action") {
        frame.keyboard().press(key::d);
        frame.end_tick();
        frame.keyboard().release_all();

        REQUIRE_FALSE(actions.is_active(input, test_action::move_right));
        REQUIRE(actions.was_released(input, test_action::move_right));
    }

    SECTION("Two maps over the same input read the same") {
        const action_map<test_action> otherActions{};
        frame.keyboard().press(key::arrow_left);

        REQUIRE(actions.is_active(input, test_action::move_left));
        REQUIRE(otherActions.is_active(input, test_action::move_left));
    }

    SECTION("Typed text doesn't activate actions") {
        frame.append_text(" w");

        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(actions.was_triggered(input, test_action::jump));
    }
}

TEST_CASE("action_map queries never consume", "[tca][engine][input][actions][action-map]") {
    using namespace tca::engine::input;

    const action_map<test_action> actions{};
    input_frame frame{};
    const input_view input{frame};

    frame.keyboard().press(key::space);

    REQUIRE(actions.is_active(input, test_action::jump));
    REQUIRE(actions.was_triggered(input, test_action::jump));
    REQUIRE_FALSE(actions.was_released(input, test_action::jump));

    SECTION("Querying again gives the same answers") {
        REQUIRE(actions.is_active(input, test_action::jump));
        REQUIRE(actions.was_triggered(input, test_action::jump));
    }

    SECTION("The raw keyboard still sees the binding") {
        REQUIRE(input.keyboard().is_down(key::space));
        REQUIRE(input.keyboard().was_pressed(key::space));
    }

    SECTION("A queried held action is still active in the next tick") {
        frame.end_tick();

        REQUIRE(actions.is_active(input, test_action::jump));
    }
}

TEST_CASE("action_map consume hides an action's bindings",
          "[tca][engine][input][actions][action-map]") {
    using namespace tca::engine::input;

    const action_map<test_action> actions{};
    input_frame frame{};
    const input_view input{frame};

    SECTION("A consumed action reads as untouched") {
        frame.keyboard().press(key::space);
        actions.consume(input, test_action::jump);

        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(actions.was_triggered(input, test_action::jump));
        REQUIRE_FALSE(actions.was_released(input, test_action::jump));
    }

    SECTION("Consumption reaches the raw keyboard and the frame") {
        frame.keyboard().press(key::space);
        actions.consume(input, test_action::jump);

        REQUIRE_FALSE(input.keyboard().is_down(key::space));
        REQUIRE_FALSE(input.keyboard().was_pressed(key::space));
        REQUIRE_FALSE(frame.keyboard().is_down(key::space));
    }

    SECTION("Every binding is consumed, not only the one that fired") {
        frame.keyboard().press(key::space);
        frame.keyboard().press(key::w);
        actions.consume(input, test_action::jump);

        REQUIRE_FALSE(input.keyboard().is_down(key::space));
        REQUIRE_FALSE(input.keyboard().is_down(key::w));
        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
    }

    SECTION("An action sharing a consumed binding loses it too") {
        frame.keyboard().press(key::escape);
        actions.consume(input, test_action::pause);

        REQUIRE_FALSE(actions.was_triggered(input, test_action::menu_back));
        REQUIRE_FALSE(actions.is_active(input, test_action::menu_back));
    }

    SECTION("Another map over the same input sees the consumption") {
        const action_map<test_action> otherActions{};
        frame.keyboard().press(key::escape);
        actions.consume(input, test_action::pause);

        REQUIRE_FALSE(otherActions.was_triggered(input, test_action::pause));
    }

    SECTION("Other actions and unbound keys are left alone") {
        frame.keyboard().press(key::space);
        frame.keyboard().press(key::d);
        frame.keyboard().press(key::s);
        actions.consume(input, test_action::jump);

        REQUIRE(actions.is_active(input, test_action::move_right));
        REQUIRE(actions.was_triggered(input, test_action::move_right));
        REQUIRE(input.keyboard().is_down(key::s));
    }

    SECTION("The typed text is left alone") {
        frame.keyboard().press(key::space);
        frame.append_text(" ");
        actions.consume(input, test_action::jump);

        REQUIRE(input.text() == " ");
    }

    SECTION("A consumed held action stays inactive across ticks, its release isn't reported") {
        frame.keyboard().press(key::d);
        actions.consume(input, test_action::move_right);
        frame.end_tick();

        REQUIRE_FALSE(actions.is_active(input, test_action::move_right));

        frame.keyboard().release(key::d);

        REQUIRE_FALSE(actions.was_released(input, test_action::move_right));
    }

    SECTION("A consumed held action fires again once its binding is pressed again") {
        frame.keyboard().press(key::d);
        actions.consume(input, test_action::move_right);
        frame.end_tick();
        frame.keyboard().release(key::d);
        frame.end_tick();
        frame.keyboard().press(key::d);

        REQUIRE(actions.is_active(input, test_action::move_right));
        REQUIRE(actions.was_triggered(input, test_action::move_right));
    }

    SECTION("Consuming an action nobody pressed changes nothing visible") {
        actions.consume(input, test_action::jump);

        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(input.keyboard().is_down(key::space));
    }

    SECTION("Consuming an unbound or undeclared action consumes nothing") {
        frame.keyboard().press(key::space);
        actions.consume(input, test_action::unbound);
        actions.consume(input, test_action::undeclared);

        REQUIRE(actions.is_active(input, test_action::jump));
    }

    SECTION("Consumption follows the current bindings, not the defaults") {
        action_map<test_action> rebound{};
        rebound.rebind(test_action::jump, {binding{key::enter}});
        frame.keyboard().press(key::space);
        frame.keyboard().press(key::enter);
        rebound.consume(input, test_action::jump);

        REQUIRE_FALSE(input.keyboard().is_down(key::enter));
        REQUIRE(input.keyboard().is_down(key::space));
    }

    SECTION("Raw consumption of a binding hides the action") {
        frame.keyboard().press(key::space);
        input.consume(key::space);

        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(actions.was_triggered(input, test_action::jump));
    }

    SECTION("consume_all() hides every action") {
        frame.keyboard().press(key::space);
        frame.keyboard().press(key::escape);
        input.consume_all();

        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(actions.was_triggered(input, test_action::pause));
    }
}

TEST_CASE("action_map rebinding", "[tca][engine][input][actions][action-map]") {
    using namespace tca::engine::input;

    action_map<test_action> actions{};
    input_frame frame{};
    const input_view input{frame};

    SECTION("rebind() replaces every binding of the action") {
        actions.rebind(test_action::jump, {binding{key::enter}});

        REQUIRE(has_bindings(actions.bindings_of(test_action::jump), {binding{key::enter}}));

        frame.keyboard().press(key::space);
        frame.keyboard().press(key::w);
        REQUIRE_FALSE(actions.is_active(input, test_action::jump));

        frame.keyboard().press(key::enter);
        REQUIRE(actions.is_active(input, test_action::jump));
        REQUIRE(actions.was_triggered(input, test_action::jump));
    }

    SECTION("An action can be rebound to several bindings, kept in order") {
        actions.rebind(test_action::jump, {binding{key::arrow_up}, binding{key::enter}});

        REQUIRE(has_bindings(actions.bindings_of(test_action::jump),
                             {binding{key::arrow_up}, binding{key::enter}}));

        frame.keyboard().press(key::enter);
        REQUIRE(actions.is_active(input, test_action::jump));
    }

    SECTION("Rebinding to nothing leaves the action unbound and never active") {
        actions.rebind(test_action::jump, {});

        REQUIRE(actions.bindings_of(test_action::jump).empty());

        frame.keyboard().press(key::space);
        REQUIRE_FALSE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(actions.was_triggered(input, test_action::jump));
    }

    SECTION("An action with no default bindings can be bound") {
        actions.rebind(test_action::unbound, {binding{key::s}});
        frame.keyboard().press(key::s);

        REQUIRE(actions.is_active(input, test_action::unbound));
    }

    SECTION("Two actions can be rebound to the same key") {
        actions.rebind(test_action::move_left, {binding{key::space}});
        frame.keyboard().press(key::space);

        REQUIRE(actions.is_active(input, test_action::move_left));
        REQUIRE(actions.is_active(input, test_action::jump));
    }

    SECTION("Rebinding one action leaves the others alone") {
        actions.rebind(test_action::jump, {binding{key::enter}});

        REQUIRE(has_bindings(actions.bindings_of(test_action::move_left),
                             {binding{key::a}, binding{key::arrow_left}}));
        REQUIRE(has_bindings(actions.bindings_of(test_action::pause), {binding{key::escape}}));
    }

    SECTION("Rebinding an enum value the schema doesn't declare is ignored") {
        actions.rebind(test_action::undeclared, {binding{key::s}});
        frame.keyboard().press(key::s);

        REQUIRE(actions.bindings_of(test_action::undeclared).empty());
        REQUIRE_FALSE(actions.is_active(input, test_action::undeclared));
    }

    SECTION("A rebound held key is read with the new bindings right away") {
        frame.keyboard().press(key::enter);
        frame.end_tick();
        actions.rebind(test_action::jump, {binding{key::enter}});

        REQUIRE(actions.is_active(input, test_action::jump));
        REQUIRE_FALSE(actions.was_triggered(input, test_action::jump));
    }

    SECTION("reset_to_defaults() restores every action's default bindings") {
        actions.rebind(test_action::jump, {binding{key::enter}});
        actions.rebind(test_action::pause, {});
        actions.rebind(test_action::unbound, {binding{key::s}});
        actions.reset_to_defaults();

        REQUIRE(has_bindings(actions.bindings_of(test_action::jump),
                             {binding{key::space}, binding{key::w}}));
        REQUIRE(has_bindings(actions.bindings_of(test_action::pause), {binding{key::escape}}));
        REQUIRE(actions.bindings_of(test_action::unbound).empty());
    }

    SECTION("A copy keeps its own bindings") {
        actions.rebind(test_action::jump, {binding{key::enter}});
        auto copy{actions};
        copy.rebind(test_action::jump, {binding{key::arrow_up}});

        REQUIRE(has_bindings(actions.bindings_of(test_action::jump), {binding{key::enter}}));
        REQUIRE(has_bindings(copy.bindings_of(test_action::jump), {binding{key::arrow_up}}));
    }

    SECTION("Rebinding doesn't affect another map of the same schema") {
        const action_map<test_action> otherActions{};
        actions.rebind(test_action::jump, {binding{key::enter}});

        REQUIRE(has_bindings(otherActions.bindings_of(test_action::jump),
                             {binding{key::space}, binding{key::w}}));
    }
}

TEST_CASE("action_map reads mouse button bindings",
          "[tca][engine][input][actions][action-map][mouse]") {
    using namespace tca::engine::input;

    const action_map<pointer_action> actions{};
    input_frame frame{};
    const input_view input{frame};

    SECTION("An action bound to the left button fires through action_map") {
        frame.mouse().button_down(mouse_button::left);

        THEN("It is active and triggered") {
            REQUIRE(actions.is_active(input, pointer_action::pet));
            REQUIRE(actions.was_triggered(input, pointer_action::pet));
            REQUIRE_FALSE(actions.was_released(input, pointer_action::pet));
        }

        AND_WHEN("The button goes up") {
            frame.mouse().button_up(mouse_button::left);

            THEN("It is released and no longer active") {
                REQUIRE(actions.was_released(input, pointer_action::pet));
                REQUIRE_FALSE(actions.is_active(input, pointer_action::pet));
            }
        }

        AND_WHEN("The tick ends with the button held") {
            frame.end_tick();

            THEN("It stays active but triggers only once") {
                REQUIRE(actions.is_active(input, pointer_action::pet));
                REQUIRE_FALSE(actions.was_triggered(input, pointer_action::pet));
            }
        }
    }

    SECTION("A key and a button bound to one action: either is enough") {
        SECTION("The key") {
            frame.keyboard().press(key::f);
            REQUIRE(actions.was_triggered(input, pointer_action::feed));
        }

        SECTION("The button") {
            frame.mouse().button_down(mouse_button::right);
            REQUIRE(actions.was_triggered(input, pointer_action::feed));
        }
    }

    SECTION("A button activates only the actions it is bound to") {
        frame.mouse().button_down(mouse_button::x1);

        REQUIRE(actions.was_triggered(input, pointer_action::back));
        REQUIRE_FALSE(actions.is_active(input, pointer_action::pet));
        REQUIRE_FALSE(actions.is_active(input, pointer_action::feed));
    }

    SECTION("A button and a key never stand in for each other") {
        // mouse_button::left and key::a share the underlying value 0 in their enums.
        frame.keyboard().press(key::a);
        REQUIRE_FALSE(actions.is_active(input, pointer_action::pet));

        frame.mouse().button_down(mouse_button::middle);
        REQUIRE_FALSE(actions.is_active(input, pointer_action::pet));
        REQUIRE_FALSE(actions.is_active(input, pointer_action::feed));
    }

    SECTION("A consumed button reads as untouched to the action") {
        frame.mouse().button_down(mouse_button::left);
        frame.mouse().consume(mouse_button::left);

        REQUIRE_FALSE(actions.is_active(input, pointer_action::pet));
        REQUIRE_FALSE(actions.was_triggered(input, pointer_action::pet));
    }

    SECTION("Queries never consume a button") {
        frame.mouse().button_down(mouse_button::left);
        std::ignore = actions.was_triggered(input, pointer_action::pet);

        REQUIRE(frame.mouse().is_down(mouse_button::left));
        REQUIRE(frame.mouse().was_pressed(mouse_button::left));
    }
}

TEST_CASE("action_map consume hides mouse button bindings",
          "[tca][engine][input][actions][action-map][mouse]") {
    using namespace tca::engine::input;

    const action_map<pointer_action> actions{};
    input_frame frame{};
    const input_view input{frame};

    SECTION("Consuming an action hides its button from the action and the raw mouse") {
        frame.mouse().button_down(mouse_button::left);

        actions.consume(input, pointer_action::pet);

        REQUIRE_FALSE(actions.is_active(input, pointer_action::pet));
        REQUIRE_FALSE(input.mouse().is_down(mouse_button::left));
        REQUIRE_FALSE(input.mouse().was_pressed(mouse_button::left));
    }

    SECTION("Consuming an action with a key and a button hides both") {
        frame.keyboard().press(key::f);
        frame.mouse().button_down(mouse_button::right);

        actions.consume(input, pointer_action::feed);

        REQUIRE_FALSE(input.keyboard().is_down(key::f));
        REQUIRE_FALSE(input.mouse().is_down(mouse_button::right));
    }

    SECTION("Consuming an action leaves other buttons alone") {
        frame.mouse().button_down(mouse_button::left);
        frame.mouse().button_down(mouse_button::x1);

        actions.consume(input, pointer_action::back);

        REQUIRE(actions.is_active(input, pointer_action::pet));
        REQUIRE_FALSE(actions.is_active(input, pointer_action::back));
    }
}

TEST_CASE("action_map rebinding to mouse buttons",
          "[tca][engine][input][actions][action-map][mouse]") {
    using namespace tca::engine::input;

    action_map<test_action> actions{};
    input_frame frame{};
    const input_view input{frame};

    SECTION("A keyboard action can be rebound to a mouse button") {
        actions.rebind(test_action::jump, {binding{mouse_button::middle}});

        frame.keyboard().press(key::space);
        REQUIRE_FALSE(actions.is_active(input, test_action::jump));

        frame.mouse().button_down(mouse_button::middle);
        REQUIRE(actions.is_active(input, test_action::jump));
    }

    SECTION("Bindings keep their order across devices") {
        actions.rebind(test_action::jump, {binding{mouse_button::x2}, binding{key::enter}});

        REQUIRE(has_bindings(actions.bindings_of(test_action::jump),
                             {binding{mouse_button::x2}, binding{key::enter}}));
    }
}
