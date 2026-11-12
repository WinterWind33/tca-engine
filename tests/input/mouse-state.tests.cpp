// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/mouse-state.hpp"

#include "input/mouse-button.hpp"

// C++ STL
#include <cstddef>
#include <initializer_list>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Utilities
#include "utilities/input/mouse-string-makers.hpp" // IWYU pragma: keep

TEST_CASE("mouse_state levels and edges", "[tca][engine][input][mouse-state]") {
    using namespace tca::engine::input;

    mouse_state state{};

    SECTION("A default-constructed state has every button up and untouched") {
        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(state.is_down(b));
            REQUIRE_FALSE(state.was_pressed(b));
            REQUIRE_FALSE(state.was_released(b));
            REQUIRE(state.press_count(b) == 0);
            REQUIRE(state.release_count(b) == 0);
        }
    }

    SECTION("A button down makes the button down and reports one press") {
        state.button_down(mouse_button::left);

        REQUIRE(state.is_down(mouse_button::left));
        REQUIRE(state.was_pressed(mouse_button::left));
        REQUIRE_FALSE(state.was_released(mouse_button::left));
        REQUIRE(state.press_count(mouse_button::left) == 1);
        REQUIRE(state.release_count(mouse_button::left) == 0);
    }

    SECTION("A button up after a button down makes the button up and reports one release") {
        state.button_down(mouse_button::left);
        state.clear_edges();
        state.button_up(mouse_button::left);

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.was_pressed(mouse_button::left));
        REQUIRE(state.was_released(mouse_button::left));
        REQUIRE(state.press_count(mouse_button::left) == 0);
        REQUIRE(state.release_count(mouse_button::left) == 1);
    }

    SECTION("A click within one frame reports one press and one release, button up") {
        state.button_down(mouse_button::left);
        state.button_up(mouse_button::left);

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE(state.was_pressed(mouse_button::left));
        REQUIRE(state.was_released(mouse_button::left));
        REQUIRE(state.press_count(mouse_button::left) == 1);
        REQUIRE(state.release_count(mouse_button::left) == 1);
    }

    SECTION("A double click within one frame counts two presses and two releases") {
        state.button_down(mouse_button::left);
        state.button_up(mouse_button::left);
        state.button_down(mouse_button::left);
        state.button_up(mouse_button::left);

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE(state.press_count(mouse_button::left) == 2);
        REQUIRE(state.release_count(mouse_button::left) == 2);
    }

    SECTION("A button up of a button that is not held is ignored") {
        state.button_up(mouse_button::right);

        REQUIRE_FALSE(state.is_down(mouse_button::right));
        REQUIRE_FALSE(state.was_released(mouse_button::right));
        REQUIRE(state.release_count(mouse_button::right) == 0);
    }

    SECTION("A second button up of the same button is ignored") {
        state.button_down(mouse_button::right);
        state.button_up(mouse_button::right);

        state.button_up(mouse_button::right);

        REQUIRE_FALSE(state.is_down(mouse_button::right));
        REQUIRE(state.release_count(mouse_button::right) == 1);
    }

    SECTION("Release then press in one frame leaves the button down with both edges") {
        state.button_down(mouse_button::left);
        state.clear_edges();
        state.button_up(mouse_button::left);
        state.button_down(mouse_button::left);

        REQUIRE(state.is_down(mouse_button::left));
        REQUIRE(state.was_pressed(mouse_button::left));
        REQUIRE(state.was_released(mouse_button::left));
    }

    SECTION("Buttons are independent of each other") {
        state.button_down(mouse_button::left);

        REQUIRE(state.is_down(mouse_button::left));
        for (const auto b :
             {mouse_button::right, mouse_button::middle, mouse_button::x1, mouse_button::x2}) {
            REQUIRE_FALSE(state.is_down(b));
            REQUIRE_FALSE(state.was_pressed(b));
        }
    }

    SECTION("Every button of the enum is tracked") {
        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            state.button_down(b);

            REQUIRE(state.is_down(b));
            REQUIRE(state.press_count(b) == 1);
        }
    }

    SECTION("clear_edges() forgets presses and releases but keeps held buttons down") {
        state.button_down(mouse_button::left);
        state.button_down(mouse_button::right);
        state.button_up(mouse_button::right);

        state.clear_edges();

        REQUIRE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.was_pressed(mouse_button::left));
        REQUIRE(state.press_count(mouse_button::left) == 0);

        REQUIRE_FALSE(state.is_down(mouse_button::right));
        REQUIRE_FALSE(state.was_pressed(mouse_button::right));
        REQUIRE_FALSE(state.was_released(mouse_button::right));
        REQUIRE(state.release_count(mouse_button::right) == 0);
    }
}

TEST_CASE("mouse_state consumption", "[tca][engine][input][mouse-state]") {
    using namespace tca::engine::input;

    mouse_state state{};

    SECTION("A consumed held button reads as untouched") {
        state.button_down(mouse_button::left);

        state.consume(mouse_button::left);

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.was_pressed(mouse_button::left));
        REQUIRE_FALSE(state.was_released(mouse_button::left));
        REQUIRE(state.press_count(mouse_button::left) == 0);
        REQUIRE(state.release_count(mouse_button::left) == 0);
    }

    SECTION("A consumed click reads as untouched") {
        state.button_down(mouse_button::left);
        state.button_up(mouse_button::left);

        state.consume(mouse_button::left);

        REQUIRE_FALSE(state.was_pressed(mouse_button::left));
        REQUIRE_FALSE(state.was_released(mouse_button::left));
        REQUIRE(state.release_count(mouse_button::left) == 0);
    }

    SECTION("Consuming a button leaves the other buttons alone") {
        state.button_down(mouse_button::left);
        state.button_down(mouse_button::right);

        state.consume(mouse_button::left);

        REQUIRE(state.is_down(mouse_button::right));
        REQUIRE(state.was_pressed(mouse_button::right));
    }

    SECTION("Consuming an untouched button is harmless") {
        state.consume(mouse_button::middle);

        REQUIRE_FALSE(state.is_down(mouse_button::middle));
        REQUIRE_FALSE(state.was_pressed(mouse_button::middle));
    }

    SECTION("Consuming a button twice is the same as once") {
        state.button_down(mouse_button::left);

        state.consume(mouse_button::left);
        state.consume(mouse_button::left);

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.was_pressed(mouse_button::left));
    }

    SECTION("consume_all() hides every button with input and leaves untouched buttons untouched") {
        state.button_down(mouse_button::left);
        state.button_down(mouse_button::x1);
        state.button_up(mouse_button::x1);

        state.consume_all();

        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(state.is_down(b));
            REQUIRE_FALSE(state.was_pressed(b));
            REQUIRE_FALSE(state.was_released(b));
        }
    }

    SECTION("A consumed click does not hide the presses of the next frame") {
        state.button_down(mouse_button::left);
        state.button_up(mouse_button::left);
        state.consume(mouse_button::left);
        state.clear_edges();

        state.button_down(mouse_button::left);

        REQUIRE(state.is_down(mouse_button::left));
        REQUIRE(state.was_pressed(mouse_button::left));
        REQUIRE(state.press_count(mouse_button::left) == 1);
    }

    SECTION("A consumed held button stays hidden after clear_edges() while it is held") {
        state.button_down(mouse_button::left);
        state.consume(mouse_button::left);

        state.clear_edges();

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.was_pressed(mouse_button::left));
    }

    SECTION("The release of a consumed held button is not reported") {
        state.button_down(mouse_button::left);
        state.consume(mouse_button::left);
        state.clear_edges();

        state.button_up(mouse_button::left);

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.was_released(mouse_button::left));
        REQUIRE(state.release_count(mouse_button::left) == 0);
    }

    SECTION("The release of a consumed held button is not reported within the same frame either") {
        state.button_down(mouse_button::left);
        state.consume(mouse_button::left);

        state.button_up(mouse_button::left);

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.was_released(mouse_button::left));
        REQUIRE(state.release_count(mouse_button::left) == 0);
    }

    SECTION("A new press after the hidden release is reported") {
        state.button_down(mouse_button::left);
        state.consume(mouse_button::left);
        state.clear_edges();
        state.button_up(mouse_button::left);
        state.clear_edges();

        state.button_down(mouse_button::left);

        REQUIRE(state.is_down(mouse_button::left));
        REQUIRE(state.was_pressed(mouse_button::left));
        REQUIRE(state.press_count(mouse_button::left) == 1);
        REQUIRE_FALSE(state.was_released(mouse_button::left));
    }

    SECTION("A new press in the same frame as the hidden release is reported, the release is not") {
        state.button_down(mouse_button::left);
        state.consume(mouse_button::left);
        state.clear_edges();

        state.button_up(mouse_button::left);
        state.button_down(mouse_button::left);

        REQUIRE(state.is_down(mouse_button::left));
        REQUIRE(state.was_pressed(mouse_button::left));
        REQUIRE(state.press_count(mouse_button::left) == 1);
        REQUIRE_FALSE(state.was_released(mouse_button::left));
        REQUIRE(state.release_count(mouse_button::left) == 0);
    }

    SECTION("consume_all() hides held buttons until they are released") {
        state.button_down(mouse_button::left);
        state.button_down(mouse_button::right);
        state.consume_all();
        state.clear_edges();

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.is_down(mouse_button::right));

        state.button_up(mouse_button::left);

        REQUIRE_FALSE(state.was_released(mouse_button::left));
        REQUIRE_FALSE(state.is_down(mouse_button::right));
    }
}

TEST_CASE("mouse_state release_all() on focus loss", "[tca][engine][input][mouse-state]") {
    using namespace tca::engine::input;

    mouse_state state{};

    SECTION("On an untouched state it changes nothing") {
        state.release_all();

        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(state.is_down(b));
            REQUIRE_FALSE(state.was_released(b));
        }
    }

    SECTION("Every held button goes up and reports one release") {
        state.button_down(mouse_button::left);
        state.button_down(mouse_button::x2);
        state.clear_edges();

        state.release_all();

        for (const auto b : {mouse_button::left, mouse_button::x2}) {
            REQUIRE_FALSE(state.is_down(b));
            REQUIRE(state.was_released(b));
            REQUIRE(state.release_count(b) == 1);
        }
    }

    SECTION("Buttons that are not held report no release") {
        state.button_down(mouse_button::left);
        state.button_up(mouse_button::left);
        state.clear_edges();

        state.release_all();

        REQUIRE_FALSE(state.was_released(mouse_button::left));
        REQUIRE_FALSE(state.was_released(mouse_button::right));
    }

    SECTION("A button already released in the same frame is not released twice") {
        state.button_down(mouse_button::left);
        state.button_up(mouse_button::left);

        state.release_all();

        REQUIRE(state.release_count(mouse_button::left) == 1);
    }

    SECTION("Consumed held buttons go up without reporting a release") {
        state.button_down(mouse_button::left);
        state.button_down(mouse_button::right);
        state.consume(mouse_button::right);
        state.clear_edges();

        state.release_all();

        REQUIRE(state.was_released(mouse_button::left));
        REQUIRE_FALSE(state.is_down(mouse_button::right));
        REQUIRE_FALSE(state.was_released(mouse_button::right));
    }

    SECTION("After it, a new press is reported normally") {
        state.button_down(mouse_button::left);
        state.consume(mouse_button::left);
        state.clear_edges();
        state.release_all();
        state.clear_edges();

        state.button_down(mouse_button::left);

        REQUIRE(state.is_down(mouse_button::left));
        REQUIRE(state.was_pressed(mouse_button::left));
    }
}

TEST_CASE("mouse_state merge() of two consecutive states", "[tca][engine][input][mouse-state]") {
    using namespace tca::engine::input;

    mouse_state older{};
    mouse_state newer{};

    SECTION("Merging two empty states gives an empty state") {
        const auto merged{mouse_state::merge(older, newer)};

        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(merged.is_down(b));
            REQUIRE_FALSE(merged.was_pressed(b));
            REQUIRE_FALSE(merged.was_released(b));
        }
    }

    SECTION("Edge counts add up and the level comes from the newer state") {
        older.button_down(mouse_button::left);
        older.button_up(mouse_button::left);
        older.button_down(mouse_button::left);
        newer.button_down(mouse_button::left);
        newer.clear_edges();
        newer.button_up(mouse_button::left);

        const auto merged{mouse_state::merge(older, newer)};

        REQUIRE_FALSE(merged.is_down(mouse_button::left));
        REQUIRE(merged.press_count(mouse_button::left) == 2);
        REQUIRE(merged.release_count(mouse_button::left) == 2);
    }

    SECTION("A click that no step read is carried into the next frame") {
        older.button_down(mouse_button::left);
        older.button_up(mouse_button::left);

        const auto merged{mouse_state::merge(older, newer)};

        REQUIRE_FALSE(merged.is_down(mouse_button::left));
        REQUIRE(merged.press_count(mouse_button::left) == 1);
        REQUIRE(merged.release_count(mouse_button::left) == 1);
    }

    SECTION("A button only in the older state keeps its edges and reads as up") {
        older.button_down(mouse_button::middle);

        const auto merged{mouse_state::merge(older, newer)};

        REQUIRE_FALSE(merged.is_down(mouse_button::middle));
        REQUIRE(merged.press_count(mouse_button::middle) == 1);
    }

    SECTION("A button only in the newer state is taken as it is") {
        newer.button_down(mouse_button::x1);

        const auto merged{mouse_state::merge(older, newer)};

        REQUIRE(merged.is_down(mouse_button::x1));
        REQUIRE(merged.press_count(mouse_button::x1) == 1);
    }

    SECTION("Merging leaves both inputs untouched") {
        older.button_down(mouse_button::left);
        newer.button_down(mouse_button::right);

        [[maybe_unused]] const auto merged{mouse_state::merge(older, newer)};

        REQUIRE(older.is_down(mouse_button::left));
        REQUIRE(older.press_count(mouse_button::left) == 1);
        REQUIRE_FALSE(older.is_down(mouse_button::right));
        REQUIRE(newer.is_down(mouse_button::right));
        REQUIRE_FALSE(newer.is_down(mouse_button::left));
    }
}

TEST_CASE("mouse_state merge_into_pending() for a fixed step's pending state",
          "[tca][engine][input][mouse-state]") {
    using namespace tca::engine::input;

    mouse_state pending{};
    mouse_state incoming{};

    SECTION("Folding two empty states gives an empty state") {
        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(merged.is_down(b));
            REQUIRE_FALSE(merged.was_pressed(b));
            REQUIRE_FALSE(merged.was_released(b));
        }
    }

    SECTION(
        "A button the pending state has behaves as merge(): edges add up, level from incoming") {
        pending.button_down(mouse_button::left);
        pending.button_up(mouse_button::left);
        pending.button_down(mouse_button::left);
        incoming.button_down(mouse_button::left);
        incoming.clear_edges();
        incoming.button_up(mouse_button::left);

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(mouse_button::left));
        REQUIRE(merged.press_count(mouse_button::left) == 2);
        REQUIRE(merged.release_count(mouse_button::left) == 2);
    }

    SECTION("A button the pending state holds, still held in incoming without edges, stays down") {
        pending.button_down(mouse_button::left);
        pending.clear_edges();
        incoming.button_down(mouse_button::left);
        incoming.clear_edges();

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.is_down(mouse_button::left));
        REQUIRE_FALSE(merged.was_pressed(mouse_button::left));
    }

    SECTION("A button the pending state knows as up, held in incoming without a press, is down") {
        pending.button_down(mouse_button::left);
        pending.button_up(mouse_button::left);
        pending.clear_edges();
        incoming.button_down(mouse_button::left);
        incoming.clear_edges();

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.is_down(mouse_button::left));
    }

    SECTION("A button only in the pending state keeps its edges and reads as up") {
        pending.button_down(mouse_button::right);

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(mouse_button::right));
        REQUIRE(merged.press_count(mouse_button::right) == 1);
    }

    SECTION(
        "A button absent from the pending state, held in incoming without a press, stays absent") {
        incoming.button_down(mouse_button::left);
        incoming.clear_edges();

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(mouse_button::left));
        REQUIRE_FALSE(merged.was_pressed(mouse_button::left));

        AND_THEN("Its later release isn't reported either") {
            mouse_state released{incoming};
            released.button_up(mouse_button::left);
            const auto afterRelease{mouse_state::merge_into_pending(merged, released)};

            REQUIRE_FALSE(afterRelease.was_released(mouse_button::left));
            REQUIRE_FALSE(afterRelease.is_down(mouse_button::left));
        }
    }

    SECTION("A button consumed in the pending state, still held in incoming, stays hidden") {
        pending.button_down(mouse_button::left);
        pending.consume(mouse_button::left);
        incoming.button_down(mouse_button::left);
        incoming.clear_edges();

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(mouse_button::left));
        REQUIRE_FALSE(merged.was_pressed(mouse_button::left));
    }

    SECTION("A button absent from the pending state with a press in incoming is taken") {
        incoming.button_down(mouse_button::left);

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.is_down(mouse_button::left));
        REQUIRE(merged.press_count(mouse_button::left) == 1);
    }

    SECTION("A click between two fixed steps reaches the next step, edges and all") {
        // The pending state was read and cleared by the last step; the click lands in the frame
        // that follows, before the next step runs.
        incoming.button_down(mouse_button::left);
        incoming.button_up(mouse_button::left);

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(mouse_button::left));
        REQUIRE(merged.was_pressed(mouse_button::left));
        REQUIRE(merged.was_released(mouse_button::left));
        REQUIRE(merged.press_count(mouse_button::left) == 1);
        REQUIRE(merged.release_count(mouse_button::left) == 1);
    }

    SECTION("Clicks over several frames with no step in between all reach the next step") {
        mouse_state frame{};
        frame.button_down(mouse_button::left);
        frame.button_up(mouse_button::left);

        auto accumulated{mouse_state::merge_into_pending(pending, frame)};
        accumulated = mouse_state::merge_into_pending(accumulated, frame);

        REQUIRE(accumulated.press_count(mouse_button::left) == 2);
        REQUIRE(accumulated.release_count(mouse_button::left) == 2);

        AND_THEN("Once the step has read and cleared them, they are not seen twice") {
            accumulated.clear_edges();

            REQUIRE_FALSE(accumulated.was_pressed(mouse_button::left));
            REQUIRE_FALSE(accumulated.was_released(mouse_button::left));
        }
    }

    SECTION(
        "A button consumed in the pending state, released and pressed again in incoming, is "
        "back") {
        pending.button_down(mouse_button::left);
        pending.consume(mouse_button::left);
        incoming.button_down(mouse_button::left);
        incoming.clear_edges();
        incoming.button_up(mouse_button::left);
        incoming.button_down(mouse_button::left);

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.is_down(mouse_button::left));
        REQUIRE(merged.press_count(mouse_button::left) == 1);
        REQUIRE(merged.release_count(mouse_button::left) == 1);
    }

    SECTION("Folding leaves both inputs untouched") {
        pending.button_down(mouse_button::left);
        incoming.button_down(mouse_button::right);

        [[maybe_unused]] const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(pending.is_down(mouse_button::left));
        REQUIRE_FALSE(pending.is_down(mouse_button::right));
        REQUIRE(incoming.is_down(mouse_button::right));
        REQUIRE_FALSE(incoming.is_down(mouse_button::left));
    }
}

namespace {

template <typename Mouse>
concept mouse_writable = requires(Mouse mouse, tca::engine::input::mouse_button button) {
    mouse.button_down(button);
    mouse.button_up(button);
    mouse.release_all();
    mouse.clear_edges();
};

} // namespace

TEST_CASE("mouse_view exposes no writer", "[tca][engine][input][mouse-view]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(mouse_writable<mouse_state>);
    STATIC_REQUIRE_FALSE(mouse_writable<mouse_view>);
    STATIC_REQUIRE_FALSE(mouse_writable<const mouse_view>);
}

TEST_CASE("mouse_view forwards to the state", "[tca][engine][input][mouse-view]") {
    using namespace tca::engine::input;

    mouse_state state{};

    SECTION("A view of an untouched state has every button up and untouched") {
        const mouse_view view{state};

        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(view.is_down(b));
            REQUIRE_FALSE(view.was_pressed(b));
            REQUIRE_FALSE(view.was_released(b));
            REQUIRE(view.press_count(b) == 0);
            REQUIRE(view.release_count(b) == 0);
        }
    }

    SECTION("Levels and edge counts read the same as the state") {
        state.button_down(mouse_button::left);
        state.button_up(mouse_button::left);
        state.button_down(mouse_button::left);
        state.button_down(mouse_button::right);
        state.button_up(mouse_button::right);

        const mouse_view view{state};

        REQUIRE(view.is_down(mouse_button::left));
        REQUIRE(view.was_pressed(mouse_button::left));
        REQUIRE(view.was_released(mouse_button::left));
        REQUIRE(view.press_count(mouse_button::left) == 2);
        REQUIRE(view.release_count(mouse_button::left) == 1);

        REQUIRE_FALSE(view.is_down(mouse_button::right));
        REQUIRE(view.press_count(mouse_button::right) == 1);
        REQUIRE(view.release_count(mouse_button::right) == 1);
    }

    SECTION("A view sees changes made to the state after it was created") {
        const mouse_view view{state};

        state.button_down(mouse_button::left);

        REQUIRE(view.is_down(mouse_button::left));
        REQUIRE(view.was_pressed(mouse_button::left));

        state.clear_edges();

        REQUIRE(view.is_down(mouse_button::left));
        REQUIRE_FALSE(view.was_pressed(mouse_button::left));
    }

    SECTION("consume() through a view hides the button on the state") {
        state.button_down(mouse_button::left);
        state.button_down(mouse_button::right);
        const mouse_view view{state};

        view.consume(mouse_button::left);

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.was_pressed(mouse_button::left));
        REQUIRE_FALSE(view.was_pressed(mouse_button::left));

        REQUIRE(state.is_down(mouse_button::right));
        REQUIRE(view.is_down(mouse_button::right));
    }

    SECTION("consume_all() through a view hides every button with input on the state") {
        state.button_down(mouse_button::left);
        const mouse_view view{state};

        view.consume_all();

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE_FALSE(state.was_pressed(mouse_button::left));
        REQUIRE_FALSE(view.was_pressed(mouse_button::left));
    }

    SECTION("A copy of a view looks at the same state") {
        state.button_down(mouse_button::left);
        const mouse_view view{state};
        const mouse_view copy{view}; // NOLINT(performance-unnecessary-copy-initialization)

        copy.consume(mouse_button::left);

        REQUIRE_FALSE(view.is_down(mouse_button::left));
        REQUIRE_FALSE(view.was_pressed(mouse_button::left));
    }
}

TEST_CASE("mouse_state pointer", "[tca][engine][input][mouse-state][pointer]") {
    using namespace tca::engine::input;

    mouse_state state{};

    SECTION("A default-constructed state has no pointer position and the pointer outside") {
        REQUIRE_FALSE(state.pointer().has_value());
        REQUIRE_FALSE(state.is_pointer_inside());
    }

    SECTION("A move gives the pointer that position") {
        state.move_pointer({.x = 120, .y = 45});

        REQUIRE(state.pointer() == pointer_position{.x = 120, .y = 45});
    }

    SECTION("The last move wins") {
        state.move_pointer({.x = 1, .y = 2});
        state.move_pointer({.x = 3, .y = 4});
        state.move_pointer({.x = 640, .y = 360});

        REQUIRE(state.pointer() == pointer_position{.x = 640, .y = 360});
    }

    SECTION("The origin is a position like any other") {
        state.move_pointer({.x = 0, .y = 0});

        REQUIRE(state.pointer().has_value());
        REQUIRE(state.pointer() == pointer_position{.x = 0, .y = 0});
    }

    SECTION("A position outside the client area, as during a drag, is kept as it is") {
        state.move_pointer({.x = -30, .y = -5});

        REQUIRE(state.pointer() == pointer_position{.x = -30, .y = -5});
    }

    SECTION("Entering puts the pointer inside, leaving puts it outside") {
        state.pointer_entered();

        REQUIRE(state.is_pointer_inside());

        state.pointer_left();

        REQUIRE_FALSE(state.is_pointer_inside());
    }

    SECTION("Entering or leaving twice is the same as once") {
        state.pointer_entered();
        state.pointer_entered();

        REQUIRE(state.is_pointer_inside());

        state.pointer_left();
        state.pointer_left();

        REQUIRE_FALSE(state.is_pointer_inside());
    }

    SECTION("A move alone does not put the pointer inside") {
        state.move_pointer({.x = 10, .y = 10});

        REQUIRE_FALSE(state.is_pointer_inside());
    }

    SECTION("A move does not take the pointer outside") {
        state.pointer_entered();

        state.move_pointer({.x = -1, .y = 10});

        REQUIRE(state.is_pointer_inside());
    }

    SECTION("Leaving keeps the last position") {
        state.pointer_entered();
        state.move_pointer({.x = 799, .y = 12});

        state.pointer_left();

        REQUIRE(state.pointer() == pointer_position{.x = 799, .y = 12});
    }

    SECTION("Entering does not invent a position") {
        state.pointer_entered();

        REQUIRE_FALSE(state.pointer().has_value());
    }

    SECTION("Buttons do not move the pointer") {
        state.move_pointer({.x = 50, .y = 60});
        state.pointer_entered();

        state.button_down(mouse_button::left);
        state.button_up(mouse_button::left);

        REQUIRE(state.pointer() == pointer_position{.x = 50, .y = 60});
        REQUIRE(state.is_pointer_inside());
    }

    SECTION("The pointer does not press buttons") {
        state.move_pointer({.x = 50, .y = 60});
        state.pointer_entered();
        state.pointer_left();

        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(state.is_down(b));
            REQUIRE_FALSE(state.was_pressed(b));
            REQUIRE_FALSE(state.was_released(b));
        }
    }

    SECTION("clear_edges() keeps the pointer") {
        state.move_pointer({.x = 50, .y = 60});
        state.pointer_entered();

        state.clear_edges();

        REQUIRE(state.pointer() == pointer_position{.x = 50, .y = 60});
        REQUIRE(state.is_pointer_inside());
    }

    SECTION("Consuming buttons does not hide the pointer") {
        state.move_pointer({.x = 50, .y = 60});
        state.pointer_entered();
        state.button_down(mouse_button::left);

        state.consume(mouse_button::left);
        state.consume_all();

        REQUIRE(state.pointer() == pointer_position{.x = 50, .y = 60});
        REQUIRE(state.is_pointer_inside());
    }

    SECTION("release_all() on focus loss keeps the pointer") {
        state.move_pointer({.x = 50, .y = 60});
        state.pointer_entered();
        state.button_down(mouse_button::left);

        state.release_all();

        REQUIRE(state.pointer() == pointer_position{.x = 50, .y = 60});
        REQUIRE(state.is_pointer_inside());
    }

    SECTION("A drag that leaves the window is followed to its release") {
        state.pointer_entered();
        state.move_pointer({.x = 100, .y = 100});
        state.button_down(mouse_button::left);
        state.clear_edges();

        state.pointer_left();
        state.move_pointer({.x = 900, .y = -40});
        state.button_up(mouse_button::left);

        REQUIRE_FALSE(state.is_pointer_inside());
        REQUIRE(state.pointer() == pointer_position{.x = 900, .y = -40});
        REQUIRE(state.was_released(mouse_button::left));
        REQUIRE_FALSE(state.is_down(mouse_button::left));
    }
}

TEST_CASE("mouse_state pointer through merge()", "[tca][engine][input][mouse-state][pointer]") {
    using namespace tca::engine::input;

    mouse_state older{};
    mouse_state newer{};

    SECTION("Merging two states without a pointer gives no pointer, outside") {
        const auto merged{mouse_state::merge(older, newer)};

        REQUIRE_FALSE(merged.pointer().has_value());
        REQUIRE_FALSE(merged.is_pointer_inside());
    }

    SECTION("The position comes from the newer state") {
        older.move_pointer({.x = 1, .y = 1});
        newer.move_pointer({.x = 2, .y = 2});

        const auto merged{mouse_state::merge(older, newer)};

        REQUIRE(merged.pointer() == pointer_position{.x = 2, .y = 2});
    }

    SECTION("The position comes from the older state if the newer one has none") {
        older.move_pointer({.x = 7, .y = 8});

        const auto merged{mouse_state::merge(older, newer)};

        REQUIRE(merged.pointer() == pointer_position{.x = 7, .y = 8});
    }

    SECTION("Inside comes from the newer state") {
        older.pointer_entered();

        REQUIRE_FALSE(mouse_state::merge(older, newer).is_pointer_inside());

        newer.pointer_entered();

        REQUIRE(mouse_state::merge(older, newer).is_pointer_inside());
    }

    SECTION("A pointer that left in the newer state is outside") {
        older.pointer_entered();
        newer.pointer_entered();
        newer.pointer_left();

        REQUIRE_FALSE(mouse_state::merge(older, newer).is_pointer_inside());
    }

    SECTION("Merging leaves the pointers of both inputs untouched") {
        older.move_pointer({.x = 1, .y = 1});
        older.pointer_entered();
        newer.move_pointer({.x = 2, .y = 2});

        [[maybe_unused]] const auto merged{mouse_state::merge(older, newer)};

        REQUIRE(older.pointer() == pointer_position{.x = 1, .y = 1});
        REQUIRE(older.is_pointer_inside());
        REQUIRE(newer.pointer() == pointer_position{.x = 2, .y = 2});
        REQUIRE_FALSE(newer.is_pointer_inside());
    }
}

TEST_CASE("mouse_state pointer through merge_into_pending()",
          "[tca][engine][input][mouse-state][pointer]") {
    using namespace tca::engine::input;

    mouse_state pending{};
    mouse_state incoming{};

    SECTION("The position and inside come from the incoming state") {
        pending.move_pointer({.x = 1, .y = 1});
        incoming.move_pointer({.x = 2, .y = 2});
        incoming.pointer_entered();

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.pointer() == pointer_position{.x = 2, .y = 2});
        REQUIRE(merged.is_pointer_inside());
    }

    SECTION("The position comes from the pending state if the incoming one has none") {
        pending.move_pointer({.x = 7, .y = 8});

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.pointer() == pointer_position{.x = 7, .y = 8});
    }

    SECTION("Inside comes from the incoming state even when it has no position") {
        pending.move_pointer({.x = 7, .y = 8});
        pending.pointer_entered();

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.pointer() == pointer_position{.x = 7, .y = 8});
        REQUIRE_FALSE(merged.is_pointer_inside());
        REQUIRE(merged.is_pointer_inside() ==
                mouse_state::merge(pending, incoming).is_pointer_inside());
    }

    SECTION("Buttons consumed in the pending state do not hide the incoming pointer") {
        pending.button_down(mouse_button::left);
        pending.consume_all();
        incoming.button_down(mouse_button::left);
        incoming.clear_edges();
        incoming.move_pointer({.x = 30, .y = 40});
        incoming.pointer_entered();

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.is_down(mouse_button::left));
        REQUIRE(merged.pointer() == pointer_position{.x = 30, .y = 40});
        REQUIRE(merged.is_pointer_inside());
    }
}

namespace {

template <typename Mouse>
concept pointer_writable = requires(Mouse mouse, tca::engine::input::pointer_position position) {
    mouse.move_pointer(position);
    mouse.pointer_entered();
    mouse.pointer_left();
};

} // namespace

TEST_CASE("mouse_view reads the pointer but cannot move it",
          "[tca][engine][input][mouse-view][pointer]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(pointer_writable<mouse_state>);
    STATIC_REQUIRE_FALSE(pointer_writable<mouse_view>);
    STATIC_REQUIRE_FALSE(pointer_writable<const mouse_view>);

    mouse_state state{};
    const mouse_view view{state};

    SECTION("A view of a state without a pointer has none, outside") {
        REQUIRE_FALSE(view.pointer().has_value());
        REQUIRE_FALSE(view.is_pointer_inside());
    }

    SECTION("A view reads the state's pointer, including later moves") {
        state.move_pointer({.x = 5, .y = 6});
        state.pointer_entered();

        REQUIRE(view.pointer() == pointer_position{.x = 5, .y = 6});
        REQUIRE(view.is_pointer_inside());

        state.move_pointer({.x = 7, .y = 8});
        state.pointer_left();

        REQUIRE(view.pointer() == pointer_position{.x = 7, .y = 8});
        REQUIRE_FALSE(view.is_pointer_inside());
    }
}

TEST_CASE("mouse_state wheel", "[tca][engine][input][mouse-state][wheel]") {
    using namespace tca::engine::input;

    mouse_state state{};

    SECTION("A default-constructed state has not scrolled") {
        REQUIRE(state.wheel() == wheel_delta{});
    }

    SECTION("A turn is reported as it is") {
        state.scroll({.vertical = 1.0F});

        REQUIRE(state.wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("Turns within one frame add up, per axis") {
        state.scroll({.vertical = 1.0F});
        state.scroll({.vertical = 1.0F, .horizontal = 0.5F});
        state.scroll({.horizontal = 0.25F});

        REQUIRE(state.wheel() == wheel_delta{.vertical = 2.0F, .horizontal = 0.75F});
    }

    SECTION("Fractions of a notch, as from a touchpad, add up to whole notches") {
        for (int i{0}; i < 4; ++i) {
            state.scroll({.vertical = 0.25F});
        }

        REQUIRE(state.wheel().vertical == 1.0F);
    }

    SECTION("Opposite turns cancel out") {
        state.scroll({.vertical = 1.0F, .horizontal = -2.0F});
        state.scroll({.vertical = -1.0F, .horizontal = 2.0F});

        REQUIRE(state.wheel() == wheel_delta{});
    }

    SECTION("clear_edges() resets the wheel") {
        state.scroll({.vertical = 3.0F, .horizontal = -1.0F});

        state.clear_edges();

        REQUIRE(state.wheel() == wheel_delta{});

        AND_THEN("The next turn is counted from zero") {
            state.scroll({.vertical = -1.0F});

            REQUIRE(state.wheel() == wheel_delta{.vertical = -1.0F, .horizontal = 0.0F});
        }
    }

    SECTION("The wheel neither presses buttons nor moves the pointer") {
        state.scroll({.vertical = 1.0F});

        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(state.is_down(b));
            REQUIRE_FALSE(state.was_pressed(b));
        }

        REQUIRE_FALSE(state.pointer().has_value());
        REQUIRE_FALSE(state.is_pointer_inside());
    }

    SECTION("Buttons and the pointer do not turn the wheel") {
        state.button_down(mouse_button::middle);
        state.button_up(mouse_button::middle);
        state.move_pointer({.x = 10, .y = 20});
        state.pointer_entered();

        REQUIRE(state.wheel() == wheel_delta{});
    }

    SECTION("release_all() on focus loss keeps the turns of the frame") {
        state.scroll({.vertical = 2.0F});
        state.button_down(mouse_button::left);

        state.release_all();

        REQUIRE(state.wheel() == wheel_delta{.vertical = 2.0F, .horizontal = 0.0F});
    }
}

TEST_CASE("mouse_state wheel consumption", "[tca][engine][input][mouse-state][wheel]") {
    using namespace tca::engine::input;

    mouse_state state{};

    SECTION("A consumed wheel reads as zero") {
        state.scroll({.vertical = 2.0F, .horizontal = 1.0F});

        state.consume_wheel();

        REQUIRE(state.wheel() == wheel_delta{});
    }

    SECTION("Consuming an untouched wheel is harmless") {
        state.consume_wheel();

        REQUIRE(state.wheel() == wheel_delta{});
    }

    SECTION("A turn after the wheel was consumed is reported") {
        state.scroll({.vertical = 2.0F});
        state.consume_wheel();

        state.scroll({.vertical = -1.0F});

        REQUIRE(state.wheel() == wheel_delta{.vertical = -1.0F, .horizontal = 0.0F});
    }

    SECTION("Consuming the wheel leaves the buttons and the pointer alone") {
        state.button_down(mouse_button::left);
        state.move_pointer({.x = 10, .y = 20});
        state.pointer_entered();
        state.scroll({.vertical = 1.0F});

        state.consume_wheel();

        REQUIRE(state.is_down(mouse_button::left));
        REQUIRE(state.was_pressed(mouse_button::left));
        REQUIRE(state.pointer() == pointer_position{.x = 10, .y = 20});
        REQUIRE(state.is_pointer_inside());
    }

    SECTION("Consuming a button leaves the wheel alone") {
        state.button_down(mouse_button::middle);
        state.scroll({.vertical = 1.0F});

        state.consume(mouse_button::middle);

        REQUIRE(state.wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("consume_all() consumes the wheel too") {
        state.button_down(mouse_button::left);
        state.scroll({.vertical = 1.0F, .horizontal = 1.0F});

        state.consume_all();

        REQUIRE_FALSE(state.is_down(mouse_button::left));
        REQUIRE(state.wheel() == wheel_delta{});
    }
}

TEST_CASE("mouse_state wheel through merge()", "[tca][engine][input][mouse-state][wheel]") {
    using namespace tca::engine::input;

    mouse_state older{};
    mouse_state newer{};

    SECTION("Merging two states that have not scrolled gives no turn") {
        REQUIRE(mouse_state::merge(older, newer).wheel() == wheel_delta{});
    }

    SECTION("Turns of both states add up, per axis") {
        older.scroll({.vertical = 1.0F, .horizontal = 0.5F});
        newer.scroll({.vertical = 2.0F, .horizontal = -1.0F});

        const auto merged{mouse_state::merge(older, newer)};

        REQUIRE(merged.wheel() == wheel_delta{.vertical = 3.0F, .horizontal = -0.5F});
    }

    SECTION("A turn that no step read is carried into the next frame") {
        older.scroll({.vertical = -1.0F});

        const auto merged{mouse_state::merge(older, newer)};

        REQUIRE(merged.wheel() == wheel_delta{.vertical = -1.0F, .horizontal = 0.0F});
    }

    SECTION("Merging leaves the wheels of both inputs untouched") {
        older.scroll({.vertical = 1.0F});
        newer.scroll({.vertical = 2.0F});

        [[maybe_unused]] const auto merged{mouse_state::merge(older, newer)};

        REQUIRE(older.wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
        REQUIRE(newer.wheel() == wheel_delta{.vertical = 2.0F, .horizontal = 0.0F});
    }
}

TEST_CASE("mouse_state wheel through merge_into_pending()",
          "[tca][engine][input][mouse-state][wheel]") {
    using namespace tca::engine::input;

    mouse_state pending{};
    mouse_state incoming{};

    SECTION("Turns of both states add up, per axis") {
        pending.scroll({.vertical = 1.0F});
        incoming.scroll({.vertical = 1.0F, .horizontal = 2.0F});

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.wheel() == wheel_delta{.vertical = 2.0F, .horizontal = 2.0F});
    }

    SECTION("A turn between two fixed steps reaches the next step") {
        incoming.scroll({.vertical = -2.0F});

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.wheel() == wheel_delta{.vertical = -2.0F, .horizontal = 0.0F});
    }

    SECTION("Turns over several frames with no step in between all reach the next step, once") {
        mouse_state frame{};
        frame.scroll({.vertical = 1.0F});

        auto accumulated{mouse_state::merge_into_pending(pending, frame)};
        accumulated = mouse_state::merge_into_pending(accumulated, frame);

        REQUIRE(accumulated.wheel() == wheel_delta{.vertical = 2.0F, .horizontal = 0.0F});

        AND_THEN("Once the step has read and cleared them, they are not seen twice") {
            accumulated.clear_edges();

            REQUIRE(accumulated.wheel() == wheel_delta{});
        }
    }

    SECTION("A wheel consumed in the pending state stays hidden, newer turns are reported") {
        pending.scroll({.vertical = 3.0F});
        pending.consume_wheel();
        incoming.scroll({.vertical = 1.0F});

        const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(merged.wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("Folding leaves the wheels of both inputs untouched") {
        pending.scroll({.vertical = 1.0F});
        incoming.scroll({.horizontal = 1.0F});

        [[maybe_unused]] const auto merged{mouse_state::merge_into_pending(pending, incoming)};

        REQUIRE(pending.wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
        REQUIRE(incoming.wheel() == wheel_delta{.vertical = 0.0F, .horizontal = 1.0F});
    }
}

namespace {

template <typename Mouse>
concept wheel_writable =
    requires(Mouse mouse, tca::engine::input::wheel_delta delta) { mouse.scroll(delta); };

} // namespace

TEST_CASE("mouse_view reads and consumes the wheel but cannot turn it",
          "[tca][engine][input][mouse-view][wheel]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(wheel_writable<mouse_state>);
    STATIC_REQUIRE_FALSE(wheel_writable<mouse_view>);
    STATIC_REQUIRE_FALSE(wheel_writable<const mouse_view>);

    mouse_state state{};
    const mouse_view view{state};

    SECTION("A view reads the state's wheel, including later turns") {
        REQUIRE(view.wheel() == wheel_delta{});

        state.scroll({.vertical = 1.0F});

        REQUIRE(view.wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("consume_wheel() through a view hides the wheel on the state") {
        state.scroll({.vertical = 1.0F});

        view.consume_wheel();

        REQUIRE(state.wheel() == wheel_delta{});
        REQUIRE(view.wheel() == wheel_delta{});
    }

    SECTION("consume_all() through a view hides the wheel on the state") {
        state.scroll({.vertical = 1.0F});

        view.consume_all();

        REQUIRE(state.wheel() == wheel_delta{});
    }
}
