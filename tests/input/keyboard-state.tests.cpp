// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/keyboard-state.hpp"

#include "input/key.hpp"

// C++ STL
#include <cstddef>
#include <initializer_list>

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("keyboard_state levels and edges", "[tca][engine][input][keyboard-state]") {
    using namespace tca::engine::input;

    keyboard_state state{};

    SECTION("A default-constructed state has every key up and untouched") {
        for (std::size_t index{0}; index < key_count; ++index) {
            const auto k{static_cast<key>(index)};
            REQUIRE_FALSE(state.is_down(k));
            REQUIRE_FALSE(state.was_pressed(k));
            REQUIRE_FALSE(state.was_released(k));
            REQUIRE(state.press_count(k) == 0);
            REQUIRE(state.release_count(k) == 0);
        }
    }

    SECTION("A press makes the key down and reports one press") {
        state.press(key::space);

        REQUIRE(state.is_down(key::space));
        REQUIRE(state.was_pressed(key::space));
        REQUIRE_FALSE(state.was_released(key::space));
        REQUIRE(state.press_count(key::space) == 1);
        REQUIRE(state.release_count(key::space) == 0);
    }

    SECTION("A release after a press makes the key up and reports one release") {
        state.press(key::space);
        state.clear_edges();
        state.release(key::space);

        REQUIRE_FALSE(state.is_down(key::space));
        REQUIRE_FALSE(state.was_pressed(key::space));
        REQUIRE(state.was_released(key::space));
        REQUIRE(state.press_count(key::space) == 0);
        REQUIRE(state.release_count(key::space) == 1);
    }

    SECTION("Press and release in one frame report one press and one release, key up") {
        state.press(key::w);
        state.release(key::w);

        REQUIRE_FALSE(state.is_down(key::w));
        REQUIRE(state.was_pressed(key::w));
        REQUIRE(state.was_released(key::w));
        REQUIRE(state.press_count(key::w) == 1);
        REQUIRE(state.release_count(key::w) == 1);
    }

    SECTION("Two taps in one frame count two presses and two releases") {
        state.press(key::w);
        state.release(key::w);
        state.press(key::w);
        state.release(key::w);

        REQUIRE_FALSE(state.is_down(key::w));
        REQUIRE(state.press_count(key::w) == 2);
        REQUIRE(state.release_count(key::w) == 2);
    }

    SECTION("A release of a key that is not held is ignored") {
        state.release(key::space);

        REQUIRE_FALSE(state.is_down(key::space));
        REQUIRE_FALSE(state.was_released(key::space));
        REQUIRE(state.release_count(key::space) == 0);
    }

    SECTION("A second release of the same key is ignored") {
        state.press(key::space);
        state.release(key::space);

        state.release(key::space);

        REQUIRE_FALSE(state.is_down(key::space));
        REQUIRE(state.release_count(key::space) == 1);
    }

    SECTION("Release then press in one frame leaves the key down with both edges") {
        state.press(key::w);
        state.clear_edges();
        state.release(key::w);
        state.press(key::w);

        REQUIRE(state.is_down(key::w));
        REQUIRE(state.was_pressed(key::w));
        REQUIRE(state.was_released(key::w));
    }

    SECTION("Keys are independent of each other") {
        state.press(key::a);

        REQUIRE(state.is_down(key::a));
        REQUIRE_FALSE(state.is_down(key::d));
        REQUIRE_FALSE(state.was_pressed(key::d));
        REQUIRE_FALSE(state.was_pressed(key::shift_left));
    }

    SECTION("Left and right modifiers are separate keys") {
        state.press(key::shift_left);

        REQUIRE(state.is_down(key::shift_left));
        REQUIRE_FALSE(state.is_down(key::shift_right));
    }

    SECTION("The first and the last key of the enum are tracked") {
        const auto first{static_cast<key>(0)};
        const auto last{static_cast<key>(key_count - 1)};

        state.press(first);
        state.press(last);

        REQUIRE(state.is_down(first));
        REQUIRE(state.is_down(last));
        REQUIRE(state.was_pressed(first));
        REQUIRE(state.was_pressed(last));
    }

    SECTION("clear_edges() forgets presses and releases but keeps held keys down") {
        state.press(key::a);
        state.press(key::d);
        state.release(key::d);

        state.clear_edges();

        REQUIRE(state.is_down(key::a));
        REQUIRE_FALSE(state.was_pressed(key::a));
        REQUIRE(state.press_count(key::a) == 0);

        REQUIRE_FALSE(state.is_down(key::d));
        REQUIRE_FALSE(state.was_pressed(key::d));
        REQUIRE_FALSE(state.was_released(key::d));
        REQUIRE(state.release_count(key::d) == 0);
    }
}

TEST_CASE("keyboard_state consumption", "[tca][engine][input][keyboard-state]") {
    using namespace tca::engine::input;

    keyboard_state state{};

    SECTION("A consumed held key reads as untouched") {
        state.press(key::escape);

        state.consume(key::escape);

        REQUIRE_FALSE(state.is_down(key::escape));
        REQUIRE_FALSE(state.was_pressed(key::escape));
        REQUIRE_FALSE(state.was_released(key::escape));
        REQUIRE(state.press_count(key::escape) == 0);
        REQUIRE(state.release_count(key::escape) == 0);
    }

    SECTION("A consumed released key reads as untouched") {
        state.press(key::escape);
        state.release(key::escape);

        state.consume(key::escape);

        REQUIRE_FALSE(state.was_pressed(key::escape));
        REQUIRE_FALSE(state.was_released(key::escape));
        REQUIRE(state.release_count(key::escape) == 0);
    }

    SECTION("Consuming a key leaves the other keys alone") {
        state.press(key::escape);
        state.press(key::enter);

        state.consume(key::escape);

        REQUIRE(state.is_down(key::enter));
        REQUIRE(state.was_pressed(key::enter));
    }

    SECTION("Consuming an untouched key is harmless") {
        state.consume(key::f1);

        REQUIRE_FALSE(state.is_down(key::f1));
        REQUIRE_FALSE(state.was_pressed(key::f1));
    }

    SECTION("Consuming a key twice is the same as once") {
        state.press(key::escape);

        state.consume(key::escape);
        state.consume(key::escape);

        REQUIRE_FALSE(state.is_down(key::escape));
        REQUIRE_FALSE(state.was_pressed(key::escape));
    }

    SECTION("consume_all() hides every key with input and leaves untouched keys untouched") {
        // Events are all pumped before any step runs, so no key can be pressed after a
        // consume_all() within the same frame: untouched keys have nothing to hide.
        state.press(key::a);
        state.press(key::b);
        state.release(key::b);

        state.consume_all();

        for (const auto k : {key::a, key::b}) {
            REQUIRE_FALSE(state.is_down(k));
            REQUIRE_FALSE(state.was_pressed(k));
            REQUIRE_FALSE(state.was_released(k));
        }
    }

    SECTION("A consumed tap does not hide the presses of the next frame") {
        state.press(key::escape);
        state.release(key::escape);
        state.consume(key::escape);
        state.clear_edges();

        state.press(key::escape);

        REQUIRE(state.is_down(key::escape));
        REQUIRE(state.was_pressed(key::escape));
        REQUIRE(state.press_count(key::escape) == 1);
    }

    SECTION("A consumed held key stays hidden after clear_edges() while it is held") {
        state.press(key::escape);
        state.consume(key::escape);

        state.clear_edges();

        REQUIRE_FALSE(state.is_down(key::escape));
        REQUIRE_FALSE(state.was_pressed(key::escape));
    }

    SECTION("The release of a consumed held key is not reported") {
        state.press(key::escape);
        state.consume(key::escape);
        state.clear_edges();

        state.release(key::escape);

        REQUIRE_FALSE(state.is_down(key::escape));
        REQUIRE_FALSE(state.was_released(key::escape));
        REQUIRE(state.release_count(key::escape) == 0);
    }

    SECTION("The release of a consumed held key is not reported within the same frame either") {
        state.press(key::escape);
        state.consume(key::escape);

        state.release(key::escape);

        REQUIRE_FALSE(state.is_down(key::escape));
        REQUIRE_FALSE(state.was_released(key::escape));
        REQUIRE(state.release_count(key::escape) == 0);
    }

    SECTION("A new press after the hidden release is reported") {
        state.press(key::escape);
        state.consume(key::escape);
        state.clear_edges();
        state.release(key::escape);
        state.clear_edges();

        state.press(key::escape);

        REQUIRE(state.is_down(key::escape));
        REQUIRE(state.was_pressed(key::escape));
        REQUIRE(state.press_count(key::escape) == 1);
        REQUIRE_FALSE(state.was_released(key::escape));
    }

    SECTION("A new press in the same frame as the hidden release is reported, the release is not") {
        state.press(key::escape);
        state.consume(key::escape);
        state.clear_edges();

        state.release(key::escape);
        state.press(key::escape);

        REQUIRE(state.is_down(key::escape));
        REQUIRE(state.was_pressed(key::escape));
        REQUIRE(state.press_count(key::escape) == 1);
        REQUIRE_FALSE(state.was_released(key::escape));
        REQUIRE(state.release_count(key::escape) == 0);
    }

    SECTION("consume_all() hides held keys until they are released") {
        state.press(key::a);
        state.press(key::d);
        state.consume_all();
        state.clear_edges();

        REQUIRE_FALSE(state.is_down(key::a));
        REQUIRE_FALSE(state.is_down(key::d));

        state.release(key::a);

        REQUIRE_FALSE(state.was_released(key::a));
        REQUIRE_FALSE(state.is_down(key::d));
    }
}

TEST_CASE("keyboard_state release_all() on focus loss", "[tca][engine][input][keyboard-state]") {
    using namespace tca::engine::input;

    keyboard_state state{};

    SECTION("On an untouched state it changes nothing") {
        state.release_all();

        for (std::size_t index{0}; index < key_count; ++index) {
            const auto k{static_cast<key>(index)};
            REQUIRE_FALSE(state.is_down(k));
            REQUIRE_FALSE(state.was_released(k));
        }
    }

    SECTION("Every held key goes up and reports one release") {
        state.press(key::a);
        state.press(key::d);
        state.clear_edges();

        state.release_all();

        for (const auto k : {key::a, key::d}) {
            REQUIRE_FALSE(state.is_down(k));
            REQUIRE(state.was_released(k));
            REQUIRE(state.release_count(k) == 1);
        }
    }

    SECTION("Keys that are not held report no release") {
        state.press(key::a);
        state.release(key::a);
        state.clear_edges();

        state.release_all();

        REQUIRE_FALSE(state.was_released(key::a));
        REQUIRE_FALSE(state.was_released(key::b));
    }

    SECTION("A key already released in the same frame is not released twice") {
        state.press(key::a);
        state.release(key::a);

        state.release_all();

        REQUIRE(state.release_count(key::a) == 1);
    }

    SECTION("Consumed held keys go up without reporting a release") {
        state.press(key::a);
        state.press(key::escape);
        state.consume(key::escape);
        state.clear_edges();

        state.release_all();

        REQUIRE(state.was_released(key::a));
        REQUIRE_FALSE(state.is_down(key::escape));
        REQUIRE_FALSE(state.was_released(key::escape));
    }

    SECTION("After it, a new press is reported normally") {
        state.press(key::escape);
        state.consume(key::escape);
        state.clear_edges();
        state.release_all();
        state.clear_edges();

        state.press(key::escape);

        REQUIRE(state.is_down(key::escape));
        REQUIRE(state.was_pressed(key::escape));
    }
}

TEST_CASE("keyboard_state merge_into_pending() for a fixed step's pending state",
          "[tca][engine][input][keyboard-state]") {
    using namespace tca::engine::input;

    keyboard_state pending{};
    keyboard_state incoming{};

    SECTION("Folding two empty states gives an empty state") {
        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        for (std::size_t index{0}; index < key_count; ++index) {
            const auto k{static_cast<key>(index)};
            REQUIRE_FALSE(merged.is_down(k));
            REQUIRE_FALSE(merged.was_pressed(k));
            REQUIRE_FALSE(merged.was_released(k));
        }
    }

    SECTION("A key the pending state has behaves as merge(): edges add up, level from incoming") {
        pending.press(key::space);
        pending.release(key::space);
        pending.press(key::space);
        incoming.press(key::space);
        incoming.clear_edges();
        incoming.release(key::space);

        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(key::space));
        REQUIRE(merged.press_count(key::space) == 2);
        REQUIRE(merged.release_count(key::space) == 2);
    }

    SECTION("A key the pending state holds, still held in incoming without edges, stays down") {
        pending.press(key::d);
        pending.clear_edges();
        incoming.press(key::d);
        incoming.clear_edges();

        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.is_down(key::d));
        REQUIRE_FALSE(merged.was_pressed(key::d));
    }

    SECTION("A key the pending state knows as up, held in incoming without a press, is down") {
        pending.press(key::d);
        pending.release(key::d);
        pending.clear_edges();
        incoming.press(key::d);
        incoming.clear_edges();

        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.is_down(key::d));
    }

    SECTION("A key only in the pending state keeps its edges and reads as up") {
        pending.press(key::a);

        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(key::a));
        REQUIRE(merged.press_count(key::a) == 1);
    }

    SECTION("A key absent from the pending state, held in incoming without a press, stays absent") {
        incoming.press(key::space);
        incoming.clear_edges();

        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(key::space));
        REQUIRE_FALSE(merged.was_pressed(key::space));

        AND_THEN("Its later release isn't reported either") {
            keyboard_state released{incoming};
            released.release(key::space);
            const auto afterRelease{keyboard_state::merge_into_pending(merged, released)};

            REQUIRE_FALSE(afterRelease.was_released(key::space));
            REQUIRE_FALSE(afterRelease.is_down(key::space));
        }
    }

    SECTION("A key consumed in the pending state, still held in incoming, stays hidden") {
        pending.press(key::escape);
        pending.consume(key::escape);
        incoming.press(key::escape);
        incoming.clear_edges();

        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(key::escape));
        REQUIRE_FALSE(merged.was_pressed(key::escape));
    }

    SECTION("A key absent from the pending state with a press in incoming is taken") {
        incoming.press(key::a);

        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.is_down(key::a));
        REQUIRE(merged.press_count(key::a) == 1);
    }

    SECTION("A key absent from the pending state tapped in incoming is taken, edges and all") {
        incoming.press(key::a);
        incoming.release(key::a);
        incoming.press(key::a);
        incoming.release(key::a);

        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(key::a));
        REQUIRE(merged.press_count(key::a) == 2);
        REQUIRE(merged.release_count(key::a) == 2);
    }

    SECTION(
        "A key consumed in the pending state, released and pressed again in incoming, is back") {
        pending.press(key::escape);
        pending.consume(key::escape);
        incoming.press(key::escape);
        incoming.clear_edges();
        incoming.release(key::escape);
        incoming.press(key::escape);

        const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.is_down(key::escape));
        REQUIRE(merged.press_count(key::escape) == 1);
        REQUIRE(merged.release_count(key::escape) == 1);
    }

    SECTION("Folding leaves both inputs untouched") {
        pending.press(key::space);
        incoming.press(key::a);

        [[maybe_unused]] const auto merged{keyboard_state::merge_into_pending(pending, incoming)};

        REQUIRE(pending.is_down(key::space));
        REQUIRE_FALSE(pending.is_down(key::a));
        REQUIRE(incoming.is_down(key::a));
        REQUIRE_FALSE(incoming.is_down(key::space));
    }
}

namespace {

template <typename Keyboard>
concept keyboard_writable = requires(Keyboard keyboard, tca::engine::input::key k) {
    keyboard.press(k);
    keyboard.release(k);
    keyboard.release_all();
    keyboard.clear_edges();
};

} // namespace

TEST_CASE("keyboard_view exposes no writer", "[tca][engine][input][keyboard-view]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(keyboard_writable<keyboard_state>);
    STATIC_REQUIRE_FALSE(keyboard_writable<keyboard_view>);
    STATIC_REQUIRE_FALSE(keyboard_writable<const keyboard_view>);
}

TEST_CASE("keyboard_view forwards to the state", "[tca][engine][input][keyboard-view]") {
    using namespace tca::engine::input;

    keyboard_state state{};

    SECTION("A view of an untouched state has every key up and untouched") {
        const keyboard_view view{state};

        for (std::size_t index{0}; index < key_count; ++index) {
            const auto k{static_cast<key>(index)};
            REQUIRE_FALSE(view.is_down(k));
            REQUIRE_FALSE(view.was_pressed(k));
            REQUIRE_FALSE(view.was_released(k));
            REQUIRE(view.press_count(k) == 0);
            REQUIRE(view.release_count(k) == 0);
        }
    }

    SECTION("Levels and edge counts read the same as the state") {
        state.press(key::w);
        state.release(key::w);
        state.press(key::w);
        state.press(key::space);
        state.release(key::space);

        const keyboard_view view{state};

        REQUIRE(view.is_down(key::w));
        REQUIRE(view.was_pressed(key::w));
        REQUIRE(view.was_released(key::w));
        REQUIRE(view.press_count(key::w) == 2);
        REQUIRE(view.release_count(key::w) == 1);

        REQUIRE_FALSE(view.is_down(key::space));
        REQUIRE(view.press_count(key::space) == 1);
        REQUIRE(view.release_count(key::space) == 1);
    }

    SECTION("A view sees changes made to the state after it was created") {
        const keyboard_view view{state};

        state.press(key::a);

        REQUIRE(view.is_down(key::a));
        REQUIRE(view.was_pressed(key::a));

        state.clear_edges();

        REQUIRE(view.is_down(key::a));
        REQUIRE_FALSE(view.was_pressed(key::a));
    }

    SECTION("consume() through a view hides the key on the state") {
        state.press(key::escape);
        state.press(key::enter);
        const keyboard_view view{state};

        view.consume(key::escape);

        REQUIRE_FALSE(state.is_down(key::escape));
        REQUIRE_FALSE(state.was_pressed(key::escape));
        REQUIRE_FALSE(view.was_pressed(key::escape));

        REQUIRE(state.is_down(key::enter));
        REQUIRE(view.is_down(key::enter));
    }

    SECTION("consume_all() through a view hides every key with input on the state") {
        state.press(key::a);
        const keyboard_view view{state};

        view.consume_all();

        REQUIRE_FALSE(state.is_down(key::a));
        REQUIRE_FALSE(state.was_pressed(key::a));
        REQUIRE_FALSE(view.was_pressed(key::a));
    }

    SECTION("A copy of a view looks at the same state") {
        state.press(key::escape);
        const keyboard_view view{state};
        const keyboard_view copy{view}; // NOLINT(performance-unnecessary-copy-initialization)

        copy.consume(key::escape);

        REQUIRE_FALSE(view.is_down(key::escape));
        REQUIRE_FALSE(view.was_pressed(key::escape));
    }
}
