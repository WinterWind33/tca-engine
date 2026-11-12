// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/input-frame.hpp"

#include "input/key.hpp"
#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"

// C++ STL
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <string_view>
#include <utility>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Utilities
#include "utilities/input/mouse-string-makers.hpp"

TEST_CASE("input_frame contents", "[tca][engine][input][input-frame]") {
    using namespace tca::engine::input;

    input_frame frame{};

    SECTION("A default-constructed frame has no keys down and no text") {
        for (std::size_t index{0}; index < key_count; ++index) {
            const auto k{static_cast<key>(index)};
            REQUIRE_FALSE(frame.keyboard().is_down(k));
            REQUIRE_FALSE(frame.keyboard().was_pressed(k));
            REQUIRE_FALSE(frame.keyboard().was_released(k));
        }
        REQUIRE(frame.text().empty());
    }

    SECTION("The keyboard state is reachable through the frame") {
        frame.keyboard().press(key::space);

        const input_frame& const_frame{frame};
        REQUIRE(const_frame.keyboard().is_down(key::space));
        REQUIRE(const_frame.keyboard().was_pressed(key::space));
    }

    SECTION("Typed text is appended in order, as UTF-8") {
        frame.append_text("h");
        frame.append_text("\xC3\xA9");     // é, 2 bytes
        frame.append_text("\xE2\x82\xAC"); // €, 3 bytes

        REQUIRE(frame.text() == "h\xC3\xA9\xE2\x82\xAC");
    }

    SECTION("Appending empty text changes nothing") {
        frame.append_text("a");
        frame.append_text("");

        REQUIRE(frame.text() == "a");
    }
}

TEST_CASE("input_frame consumption", "[tca][engine][input][input-frame]") {
    using namespace tca::engine::input;

    input_frame frame{};
    frame.keyboard().press(key::escape);
    frame.keyboard().press(key::enter);
    frame.append_text("x");

    SECTION("consume(key) hides that key only") {
        frame.consume(key::escape);

        REQUIRE_FALSE(frame.keyboard().is_down(key::escape));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::escape));

        REQUIRE(frame.keyboard().is_down(key::enter));
        REQUIRE(frame.keyboard().was_pressed(key::enter));
    }

    SECTION("consume(key) leaves the typed text alone") {
        frame.consume(key::escape);

        REQUIRE(frame.text() == "x");
    }

    SECTION("consume_all() hides every key with input") {
        frame.consume_all();

        for (const auto k : {key::escape, key::enter}) {
            REQUIRE_FALSE(frame.keyboard().is_down(k));
            REQUIRE_FALSE(frame.keyboard().was_pressed(k));
        }
    }

    SECTION("consume_all() hides the typed text too") {
        // Whoever takes the whole keyboard (e.g. a focused text box in ImGui) takes what is typed
        // as well, otherwise the game would also receive the characters.
        frame.consume_all();

        REQUIRE(frame.text().empty());
    }
}

TEST_CASE("input_frame end_tick()", "[tca][engine][input][input-frame]") {
    using namespace tca::engine::input;

    input_frame frame{};

    SECTION("Held keys stay down, their edges are forgotten") {
        frame.keyboard().press(key::d);

        frame.end_tick();

        REQUIRE(frame.keyboard().is_down(key::d));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::d));
        REQUIRE(frame.keyboard().press_count(key::d) == 0);
    }

    SECTION("A tap within the tick leaves nothing behind") {
        frame.keyboard().press(key::space);
        frame.keyboard().release(key::space);

        frame.end_tick();

        REQUIRE_FALSE(frame.keyboard().is_down(key::space));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::space));
        REQUIRE_FALSE(frame.keyboard().was_released(key::space));
    }

    SECTION("The typed text is forgotten") {
        frame.append_text("abc");

        frame.end_tick();

        REQUIRE(frame.text().empty());
    }

    SECTION("A consumed held key stays hidden across ticks until released") {
        frame.keyboard().press(key::escape);
        frame.consume(key::escape);

        frame.end_tick();

        REQUIRE_FALSE(frame.keyboard().is_down(key::escape));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::escape));

        frame.keyboard().release(key::escape);

        REQUIRE_FALSE(frame.keyboard().was_released(key::escape));

        frame.end_tick();
        frame.keyboard().press(key::escape);

        REQUIRE(frame.keyboard().is_down(key::escape));
        REQUIRE(frame.keyboard().press_count(key::escape) == 1);
    }

    SECTION("Keys consumed by consume_all() stay hidden across ticks") {
        frame.keyboard().press(key::a);
        frame.keyboard().press(key::b);
        frame.consume_all();

        frame.end_tick();

        for (const auto k : {key::a, key::b}) {
            REQUIRE_FALSE(frame.keyboard().is_down(k));
            REQUIRE_FALSE(frame.keyboard().was_pressed(k));
        }
    }

    SECTION("Text typed after end_tick() is the next tick's only") {
        frame.append_text("a");
        frame.end_tick();
        frame.append_text("b");

        REQUIRE(frame.text() == "b");
    }
}

TEST_CASE("merge() of frames where no step ran", "[tca][engine][input][input-frame]") {
    using namespace tca::engine::input;

    input_frame older{};
    input_frame newer{};

    SECTION("Merging two empty frames gives an empty frame") {
        const auto merged{merge(older, newer)};

        for (std::size_t index{0}; index < key_count; ++index) {
            const auto k{static_cast<key>(index)};
            REQUIRE_FALSE(merged.keyboard().is_down(k));
            REQUIRE_FALSE(merged.keyboard().was_pressed(k));
            REQUIRE_FALSE(merged.keyboard().was_released(k));
        }
        REQUIRE(merged.text().empty());
    }

    SECTION("Edge counts add up") {
        older.keyboard().press(key::space);
        older.keyboard().release(key::space);
        newer.keyboard().press(key::space);
        newer.keyboard().release(key::space);
        newer.keyboard().press(key::space);

        const auto merged{merge(older, newer)};

        REQUIRE(merged.keyboard().press_count(key::space) == 3);
        REQUIRE(merged.keyboard().release_count(key::space) == 2);
    }

    SECTION("A tap in the older frame survives an idle newer frame") {
        older.keyboard().press(key::space);
        older.keyboard().release(key::space);

        const auto merged{merge(older, newer)};

        REQUIRE_FALSE(merged.keyboard().is_down(key::space));
        REQUIRE(merged.keyboard().was_pressed(key::space));
        REQUIRE(merged.keyboard().was_released(key::space));
    }

    SECTION("A key pressed in the older frame and still held in the newer is down, pressed once") {
        older.keyboard().press(key::d);
        newer.keyboard().press(key::d);
        newer.keyboard().clear_edges(); // held through the newer frame, no new edge

        const auto merged{merge(older, newer)};

        REQUIRE(merged.keyboard().is_down(key::d));
        REQUIRE(merged.keyboard().press_count(key::d) == 1);
        REQUIRE(merged.keyboard().release_count(key::d) == 0);
    }

    SECTION("The held state comes from the newer frame") {
        older.keyboard().press(key::d);
        newer.keyboard().press(key::d);
        newer.keyboard().clear_edges();
        newer.keyboard().release(key::d);

        const auto merged{merge(older, newer)};

        REQUIRE_FALSE(merged.keyboard().is_down(key::d));
        REQUIRE(merged.keyboard().was_pressed(key::d));
        REQUIRE(merged.keyboard().was_released(key::d));
    }

    SECTION("A key pressed only in the newer frame is down and pressed") {
        newer.keyboard().press(key::a);

        const auto merged{merge(older, newer)};

        REQUIRE(merged.keyboard().is_down(key::a));
        REQUIRE(merged.keyboard().press_count(key::a) == 1);
    }

    SECTION("Different keys from each frame are all kept") {
        older.keyboard().press(key::a);
        older.keyboard().release(key::a);
        newer.keyboard().press(key::d);

        const auto merged{merge(older, newer)};

        REQUIRE(merged.keyboard().was_pressed(key::a));
        REQUIRE(merged.keyboard().was_released(key::a));
        REQUIRE_FALSE(merged.keyboard().is_down(key::a));
        REQUIRE(merged.keyboard().was_pressed(key::d));
        REQUIRE(merged.keyboard().is_down(key::d));
    }

    SECTION("Text is appended, older first") {
        older.append_text("ab");
        newer.append_text("\xE2\x82\xAC"); // €

        const auto merged{merge(older, newer)};

        REQUIRE(merged.text() == "ab\xE2\x82\xAC");
    }

    SECTION("Merging leaves both inputs untouched") {
        older.keyboard().press(key::space);
        older.append_text("a");
        newer.keyboard().press(key::space);
        newer.append_text("b");

        [[maybe_unused]] const auto merged{merge(older, newer)};

        REQUIRE(older.keyboard().press_count(key::space) == 1);
        REQUIRE(older.text() == "a");
        REQUIRE(newer.keyboard().press_count(key::space) == 1);
        REQUIRE(newer.text() == "b");
    }

    SECTION("Merging into an accumulator over several idle frames keeps every edge") {
        input_frame pending{};
        for (int frame_index{0}; frame_index < 3; ++frame_index) {
            input_frame incoming{};
            incoming.keyboard().press(key::space);
            incoming.keyboard().release(key::space);
            pending = merge(pending, incoming);
        }

        REQUIRE(pending.keyboard().press_count(key::space) == 3);
        REQUIRE(pending.keyboard().release_count(key::space) == 3);
        REQUIRE_FALSE(pending.keyboard().is_down(key::space));
    }
}

TEST_CASE("merge_into_pending() of frames for a fixed step", "[tca][engine][input][input-frame]") {
    using namespace tca::engine::input;

    input_frame pending{};
    input_frame incoming{};

    SECTION("Keyboards are folded with keyboard_state::merge_into_pending()") {
        pending.keyboard().press(key::escape);
        pending.consume(key::escape);
        pending.keyboard().press(key::d);
        incoming.keyboard().press(key::escape);
        incoming.keyboard().press(key::d);
        incoming.keyboard().clear_edges();
        incoming.keyboard().press(key::a);

        const auto merged{merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.keyboard().is_down(key::escape));
        REQUIRE(merged.keyboard().is_down(key::d));
        REQUIRE(merged.keyboard().press_count(key::d) == 1);
        REQUIRE(merged.keyboard().was_pressed(key::a));
    }

    SECTION("Text is appended, pending first") {
        pending.append_text("ab");
        incoming.append_text("\xE2\x82\xAC"); // €

        const auto merged{merge_into_pending(pending, incoming)};

        REQUIRE(merged.text() == "ab\xE2\x82\xAC");
    }

    SECTION("Folding leaves both inputs untouched") {
        pending.keyboard().press(key::space);
        pending.append_text("a");
        incoming.keyboard().press(key::enter);
        incoming.append_text("b");

        [[maybe_unused]] const auto merged{merge_into_pending(pending, incoming)};

        REQUIRE(pending.keyboard().press_count(key::space) == 1);
        REQUIRE_FALSE(pending.keyboard().is_down(key::enter));
        REQUIRE(pending.text() == "a");
        REQUIRE(incoming.keyboard().press_count(key::enter) == 1);
        REQUIRE_FALSE(incoming.keyboard().is_down(key::space));
        REQUIRE(incoming.text() == "b");
    }
}

namespace {

template <typename Frame>
concept text_writable = requires(Frame frame, std::string_view text) { frame.append_text(text); };

template <typename Frame>
concept tick_endable = requires(Frame frame) { frame.end_tick(); };

} // namespace

TEST_CASE("input_view exposes no writer", "[tca][engine][input][input-view]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(text_writable<input_frame>);
    STATIC_REQUIRE_FALSE(text_writable<input_view>);
    STATIC_REQUIRE_FALSE(text_writable<const input_view>);
    STATIC_REQUIRE(tick_endable<input_frame>);
    STATIC_REQUIRE_FALSE(tick_endable<input_view>);
    STATIC_REQUIRE_FALSE(tick_endable<const input_view>);
    STATIC_REQUIRE(
        std::same_as<decltype(std::declval<const input_view&>().keyboard()), keyboard_view>);
}

TEST_CASE("input_view forwards to the frame", "[tca][engine][input][input-view]") {
    using namespace tca::engine::input;

    input_frame frame{};

    SECTION("A view of an empty frame has no keys down and no text") {
        const input_view view{frame};

        for (std::size_t index{0}; index < key_count; ++index) {
            const auto k{static_cast<key>(index)};
            REQUIRE_FALSE(view.keyboard().is_down(k));
            REQUIRE_FALSE(view.keyboard().was_pressed(k));
        }
        REQUIRE(view.text().empty());
    }

    SECTION("The keyboard of a view is the keyboard of the frame") {
        frame.keyboard().press(key::space);
        const input_view view{frame};

        REQUIRE(view.keyboard().is_down(key::space));
        REQUIRE(view.keyboard().press_count(key::space) == 1);

        view.keyboard().consume(key::space);

        REQUIRE_FALSE(frame.keyboard().is_down(key::space));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::space));
    }

    SECTION("The text of a view is the text of the frame, including text appended later") {
        frame.append_text("a");
        const input_view view{frame};

        frame.append_text("\xE2\x82\xAC"); // €

        REQUIRE(view.text() == "a\xE2\x82\xAC");
    }

    SECTION("consume(key) through a view hides that key only and leaves the text") {
        frame.keyboard().press(key::escape);
        frame.keyboard().press(key::enter);
        frame.append_text("x");
        const input_view view{frame};

        view.consume(key::escape);

        REQUIRE_FALSE(frame.keyboard().is_down(key::escape));
        REQUIRE_FALSE(view.keyboard().is_down(key::escape));
        REQUIRE(view.keyboard().is_down(key::enter));
        REQUIRE(view.text() == "x");
    }

    SECTION("consume_all() through a view hides every key with input and the text") {
        frame.keyboard().press(key::a);
        frame.append_text("x");
        const input_view view{frame};

        view.consume_all();

        REQUIRE_FALSE(frame.keyboard().is_down(key::a));
        REQUIRE_FALSE(view.keyboard().is_down(key::a));
        REQUIRE_FALSE(view.keyboard().was_pressed(key::a));
        REQUIRE(frame.text().empty());
        REQUIRE(view.text().empty());
    }

    SECTION("Two views of the same frame see each other's consumption") {
        frame.keyboard().press(key::escape);
        const input_view first{frame};
        const input_view second{frame};

        first.consume(key::escape);

        REQUIRE_FALSE(second.keyboard().is_down(key::escape));
        REQUIRE_FALSE(second.keyboard().was_pressed(key::escape));
    }
}

TEST_CASE("input_view snapshot() type", "[tca][engine][input][input-view]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(
        std::same_as<decltype(std::declval<const input_view&>().snapshot()), input_frame>);
}

TEST_CASE("input_view snapshot() copies the frame", "[tca][engine][input][input-view]") {
    using namespace tca::engine::input;

    input_frame frame{};

    SECTION("A snapshot of an empty frame is empty") {
        const input_frame copy{input_view{frame}.snapshot()};

        for (std::size_t index{0}; index < key_count; ++index) {
            const auto k{static_cast<key>(index)};
            REQUIRE_FALSE(copy.keyboard().is_down(k));
            REQUIRE(copy.keyboard().press_count(k) == 0);
            REQUIRE(copy.keyboard().release_count(k) == 0);
        }
        REQUIRE(copy.text().empty());
    }

    SECTION("A snapshot has the frame's levels, edge counts and text") {
        frame.keyboard().press(key::space);
        frame.keyboard().press(key::a);
        frame.keyboard().release(key::a);
        frame.keyboard().press(key::a);
        frame.keyboard().release(key::a);
        frame.append_text("h\xC3\xA9"); // hé

        const input_frame copy{input_view{frame}.snapshot()};

        REQUIRE(copy.keyboard().is_down(key::space));
        REQUIRE(copy.keyboard().press_count(key::space) == 1);
        REQUIRE(copy.keyboard().release_count(key::space) == 0);
        REQUIRE_FALSE(copy.keyboard().is_down(key::a));
        REQUIRE(copy.keyboard().press_count(key::a) == 2);
        REQUIRE(copy.keyboard().release_count(key::a) == 2);
        REQUIRE_FALSE(copy.keyboard().is_down(key::enter));
        REQUIRE(copy.text() == "h\xC3\xA9");
    }

    SECTION("A snapshot of a held key after end_tick() has the level and no edges") {
        frame.keyboard().press(key::space);
        frame.end_tick();

        const input_frame copy{input_view{frame}.snapshot()};

        REQUIRE(copy.keyboard().is_down(key::space));
        REQUIRE_FALSE(copy.keyboard().was_pressed(key::space));
    }

    SECTION("A key consumed in the frame is hidden in the snapshot, and stays hidden there") {
        frame.keyboard().press(key::escape);
        frame.keyboard().press(key::enter);
        const input_view view{frame};
        view.consume(key::escape);

        input_frame copy{view.snapshot()};

        REQUIRE_FALSE(copy.keyboard().is_down(key::escape));
        REQUIRE_FALSE(copy.keyboard().was_pressed(key::escape));
        REQUIRE(copy.keyboard().is_down(key::enter));

        // Its later release isn't reported by the copy either.
        copy.keyboard().release(key::escape);
        REQUIRE_FALSE(copy.keyboard().was_released(key::escape));
    }

    SECTION("After consume_all() the snapshot has no keys and no text") {
        frame.keyboard().press(key::a);
        frame.append_text("x");
        const input_view view{frame};
        view.consume_all();

        const input_frame copy{view.snapshot()};

        REQUIRE_FALSE(copy.keyboard().is_down(key::a));
        REQUIRE_FALSE(copy.keyboard().was_pressed(key::a));
        REQUIRE(copy.text().empty());
    }
}

TEST_CASE("input_view snapshot() is detached from the frame", "[tca][engine][input][input-view]") {
    using namespace tca::engine::input;

    input_frame frame{};
    frame.keyboard().press(key::space);
    frame.append_text("a");
    const input_view view{frame};
    input_frame copy{view.snapshot()};

    SECTION("end_tick() on the frame doesn't reach the snapshot") {
        frame.end_tick();

        REQUIRE(copy.keyboard().was_pressed(key::space));
        REQUIRE(copy.keyboard().press_count(key::space) == 1);
        REQUIRE(copy.text() == "a");
    }

    SECTION("Input written to the frame later doesn't reach the snapshot") {
        frame.keyboard().release(key::space);
        frame.keyboard().press(key::enter);
        frame.append_text("b");

        REQUIRE(copy.keyboard().is_down(key::space));
        REQUIRE_FALSE(copy.keyboard().was_released(key::space));
        REQUIRE_FALSE(copy.keyboard().is_down(key::enter));
        REQUIRE(copy.text() == "a");
    }

    SECTION("Consuming in the frame later doesn't reach the snapshot") {
        view.consume_all();

        REQUIRE(copy.keyboard().is_down(key::space));
        REQUIRE(copy.text() == "a");
    }

    SECTION("Writing to or consuming in the snapshot doesn't reach the frame") {
        copy.keyboard().press(key::enter);
        copy.append_text("b");
        copy.consume(key::space);
        copy.end_tick();

        REQUIRE(frame.keyboard().is_down(key::space));
        REQUIRE(frame.keyboard().was_pressed(key::space));
        REQUIRE_FALSE(frame.keyboard().is_down(key::enter));
        REQUIRE(frame.text() == "a");
    }

    SECTION("Two snapshots are independent of each other") {
        input_frame second{view.snapshot()};
        second.consume_all();

        REQUIRE(copy.keyboard().is_down(key::space));
        REQUIRE(copy.text() == "a");
    }
}

TEST_CASE("input_view snapshots of consecutive ticks merge into what happened over both",
          "[tca][engine][input][input-view]") {
    using namespace tca::engine::input;

    GIVEN("A persistent frame, snapshotted at the end of each tick before end_tick()") {
        input_frame frame{};
        const input_view view{frame};

        WHEN("Space is pressed in one tick and released in the next") {
            frame.keyboard().press(key::space);
            frame.append_text("a");
            const input_frame first{view.snapshot()};
            frame.end_tick();

            frame.keyboard().release(key::space);
            frame.append_text("b");
            const input_frame second{view.snapshot()};
            frame.end_tick();

            const input_frame merged{merge(first, second)};

            THEN("The merge has both edges once each, the key up and the text of both ticks") {
                REQUIRE(merged.keyboard().press_count(key::space) == 1);
                REQUIRE(merged.keyboard().release_count(key::space) == 1);
                REQUIRE_FALSE(merged.keyboard().is_down(key::space));
                REQUIRE(merged.text() == "ab");
            }
        }

        WHEN("Space is pressed in one tick and only held in the next") {
            frame.keyboard().press(key::space);
            const input_frame first{view.snapshot()};
            frame.end_tick();

            const input_frame second{view.snapshot()};
            frame.end_tick();

            const input_frame merged{merge(first, second)};

            THEN("The press is counted once, not once per tick it was held, and the key is down") {
                REQUIRE(merged.keyboard().press_count(key::space) == 1);
                REQUIRE(merged.keyboard().is_down(key::space));
            }
        }
    }
}

TEST_CASE("input_frame mouse", "[tca][engine][input][input-frame][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};

    SECTION("A default-constructed frame has the mouse untouched") {
        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(frame.mouse().is_down(b));
            REQUIRE_FALSE(frame.mouse().was_pressed(b));
            REQUIRE_FALSE(frame.mouse().was_released(b));
        }
        REQUIRE_FALSE(frame.mouse().pointer().has_value());
        REQUIRE_FALSE(frame.mouse().is_pointer_inside());
        REQUIRE(frame.mouse().wheel() == wheel_delta{});
    }

    SECTION("The mouse state is reachable through the frame") {
        frame.mouse().button_down(mouse_button::left);
        frame.mouse().move_pointer({.x = 10, .y = 20});
        frame.mouse().scroll({.vertical = 1.0F});

        const input_frame& const_frame{frame};
        REQUIRE(const_frame.mouse().is_down(mouse_button::left));
        REQUIRE(const_frame.mouse().was_pressed(mouse_button::left));
        REQUIRE(const_frame.mouse().pointer() == pointer_position{.x = 10, .y = 20});
        REQUIRE(const_frame.mouse().wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("The mouse and the keyboard are independent of each other") {
        frame.mouse().button_down(mouse_button::left);

        for (std::size_t index{0}; index < key_count; ++index) {
            REQUIRE_FALSE(frame.keyboard().is_down(static_cast<key>(index)));
        }

        frame.keyboard().press(key::space);

        REQUIRE(frame.mouse().press_count(mouse_button::left) == 1);
        REQUIRE_FALSE(frame.mouse().is_down(mouse_button::right));
    }
}

TEST_CASE("input_frame consumption and the mouse", "[tca][engine][input][input-frame][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};
    frame.keyboard().press(key::escape);
    frame.append_text("x");
    frame.mouse().button_down(mouse_button::left);
    frame.mouse().move_pointer({.x = 10, .y = 20});
    frame.mouse().pointer_entered();
    frame.mouse().scroll({.vertical = 1.0F});

    SECTION("consume(key) leaves the mouse alone") {
        frame.consume(key::escape);

        REQUIRE(frame.mouse().is_down(mouse_button::left));
        REQUIRE(frame.mouse().was_pressed(mouse_button::left));
        REQUIRE(frame.mouse().wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("consume_all() leaves the mouse alone") {
        // Whoever grabs the whole keyboard (e.g. a focused ImGui text box) takes the keys and the
        // text, not the mouse: the game still sees clicks and the wheel.
        frame.consume_all();

        REQUIRE_FALSE(frame.keyboard().is_down(key::escape));
        REQUIRE(frame.text().empty());
        REQUIRE(frame.mouse().is_down(mouse_button::left));
        REQUIRE(frame.mouse().was_pressed(mouse_button::left));
        REQUIRE(frame.mouse().wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
        REQUIRE(frame.mouse().pointer() == pointer_position{.x = 10, .y = 20});
    }

    SECTION("Consuming the whole mouse leaves the keyboard and the text alone") {
        frame.mouse().consume_all();

        REQUIRE_FALSE(frame.mouse().is_down(mouse_button::left));
        REQUIRE(frame.mouse().wheel() == wheel_delta{});
        REQUIRE(frame.keyboard().is_down(key::escape));
        REQUIRE(frame.text() == "x");
    }
}

TEST_CASE("input_frame end_tick() and the mouse", "[tca][engine][input][input-frame][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};

    SECTION("Held buttons stay down, their edges are forgotten") {
        frame.mouse().button_down(mouse_button::left);

        frame.end_tick();

        REQUIRE(frame.mouse().is_down(mouse_button::left));
        REQUIRE_FALSE(frame.mouse().was_pressed(mouse_button::left));
        REQUIRE(frame.mouse().press_count(mouse_button::left) == 0);
    }

    SECTION("A click within the tick leaves nothing behind") {
        frame.mouse().button_down(mouse_button::left);
        frame.mouse().button_up(mouse_button::left);

        frame.end_tick();

        REQUIRE_FALSE(frame.mouse().is_down(mouse_button::left));
        REQUIRE_FALSE(frame.mouse().was_pressed(mouse_button::left));
        REQUIRE_FALSE(frame.mouse().was_released(mouse_button::left));
    }

    SECTION("The wheel is reset") {
        frame.mouse().scroll({.vertical = 2.0F, .horizontal = -1.0F});

        frame.end_tick();

        REQUIRE(frame.mouse().wheel() == wheel_delta{});
    }

    SECTION("The pointer stays where it is") {
        frame.mouse().move_pointer({.x = 10, .y = 20});
        frame.mouse().pointer_entered();

        frame.end_tick();

        REQUIRE(frame.mouse().pointer() == pointer_position{.x = 10, .y = 20});
        REQUIRE(frame.mouse().is_pointer_inside());
    }

    SECTION("A consumed held button stays hidden across ticks until released") {
        frame.mouse().button_down(mouse_button::left);
        frame.mouse().consume(mouse_button::left);

        frame.end_tick();

        REQUIRE_FALSE(frame.mouse().is_down(mouse_button::left));
        REQUIRE_FALSE(frame.mouse().was_pressed(mouse_button::left));

        frame.mouse().button_up(mouse_button::left);

        REQUIRE_FALSE(frame.mouse().was_released(mouse_button::left));

        frame.end_tick();
        frame.mouse().button_down(mouse_button::left);

        REQUIRE(frame.mouse().is_down(mouse_button::left));
        REQUIRE(frame.mouse().press_count(mouse_button::left) == 1);
    }

    SECTION("A wheel turned after end_tick() is the next tick's only") {
        frame.mouse().scroll({.vertical = 1.0F});
        frame.end_tick();
        frame.mouse().scroll({.vertical = -1.0F});

        REQUIRE(frame.mouse().wheel() == wheel_delta{.vertical = -1.0F, .horizontal = 0.0F});
    }
}

TEST_CASE("merge() of frames where no step ran, and the mouse",
          "[tca][engine][input][input-frame][mouse]") {
    using namespace tca::engine::input;

    input_frame older{};
    input_frame newer{};

    SECTION("Merging two empty frames gives an untouched mouse") {
        const auto merged{merge(older, newer)};

        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(merged.mouse().is_down(b));
            REQUIRE_FALSE(merged.mouse().was_pressed(b));
        }
        REQUIRE_FALSE(merged.mouse().pointer().has_value());
        REQUIRE(merged.mouse().wheel() == wheel_delta{});
    }

    SECTION("A click in the older frame survives an idle newer frame") {
        older.mouse().button_down(mouse_button::left);
        older.mouse().button_up(mouse_button::left);

        const auto merged{merge(older, newer)};

        REQUIRE_FALSE(merged.mouse().is_down(mouse_button::left));
        REQUIRE(merged.mouse().was_pressed(mouse_button::left));
        REQUIRE(merged.mouse().was_released(mouse_button::left));
    }

    SECTION("Button edges and wheel turns add up, the held state comes from the newer frame") {
        older.mouse().button_down(mouse_button::right);
        older.mouse().scroll({.vertical = 1.0F});
        newer.mouse().button_down(mouse_button::right);
        newer.mouse().clear_edges();
        newer.mouse().button_up(mouse_button::right);
        newer.mouse().scroll({.vertical = 2.0F});

        const auto merged{merge(older, newer)};

        REQUIRE_FALSE(merged.mouse().is_down(mouse_button::right));
        REQUIRE(merged.mouse().press_count(mouse_button::right) == 1);
        REQUIRE(merged.mouse().release_count(mouse_button::right) == 1);
        REQUIRE(merged.mouse().wheel() == wheel_delta{.vertical = 3.0F, .horizontal = 0.0F});
    }

    SECTION("The pointer comes from the newer frame") {
        older.mouse().move_pointer({.x = 1, .y = 1});
        older.mouse().pointer_entered();
        newer.mouse().move_pointer({.x = 2, .y = 2});

        const auto merged{merge(older, newer)};

        REQUIRE(merged.mouse().pointer() == pointer_position{.x = 2, .y = 2});
        REQUIRE_FALSE(merged.mouse().is_pointer_inside());
    }

    SECTION("The keyboard and the text are still merged alongside the mouse") {
        older.keyboard().press(key::space);
        older.append_text("a");
        older.mouse().button_down(mouse_button::left);
        newer.append_text("b");

        const auto merged{merge(older, newer)};

        REQUIRE(merged.keyboard().was_pressed(key::space));
        REQUIRE(merged.text() == "ab");
        REQUIRE(merged.mouse().was_pressed(mouse_button::left));
    }

    SECTION("Merging leaves the mice of both inputs untouched") {
        older.mouse().button_down(mouse_button::left);
        older.mouse().scroll({.vertical = 1.0F});
        newer.mouse().button_down(mouse_button::right);

        [[maybe_unused]] const auto merged{merge(older, newer)};

        REQUIRE(older.mouse().press_count(mouse_button::left) == 1);
        REQUIRE_FALSE(older.mouse().is_down(mouse_button::right));
        REQUIRE(older.mouse().wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
        REQUIRE(newer.mouse().press_count(mouse_button::right) == 1);
        REQUIRE_FALSE(newer.mouse().is_down(mouse_button::left));
        REQUIRE(newer.mouse().wheel() == wheel_delta{});
    }

    SECTION("Merging into an accumulator over several idle frames keeps every click and turn") {
        input_frame pending{};
        for (int frame_index{0}; frame_index < 3; ++frame_index) {
            input_frame incoming{};
            incoming.mouse().button_down(mouse_button::left);
            incoming.mouse().button_up(mouse_button::left);
            incoming.mouse().scroll({.vertical = 1.0F});
            pending = merge(pending, incoming);
        }

        REQUIRE(pending.mouse().press_count(mouse_button::left) == 3);
        REQUIRE(pending.mouse().release_count(mouse_button::left) == 3);
        REQUIRE(pending.mouse().wheel() == wheel_delta{.vertical = 3.0F, .horizontal = 0.0F});
    }
}

TEST_CASE("merge_into_pending() of frames for a fixed step, and the mouse",
          "[tca][engine][input][input-frame][mouse]") {
    using namespace tca::engine::input;

    input_frame pending{};
    input_frame incoming{};

    SECTION("Mice are folded with mouse_state::merge_into_pending()") {
        pending.mouse().button_down(mouse_button::left);
        pending.mouse().consume(mouse_button::left);
        pending.mouse().button_down(mouse_button::right);
        incoming.mouse().button_down(mouse_button::left);
        incoming.mouse().button_down(mouse_button::right);
        incoming.mouse().clear_edges();
        incoming.mouse().button_down(mouse_button::middle);

        const auto merged{merge_into_pending(pending, incoming)};

        REQUIRE_FALSE(merged.mouse().is_down(mouse_button::left));
        REQUIRE(merged.mouse().is_down(mouse_button::right));
        REQUIRE(merged.mouse().press_count(mouse_button::right) == 1);
        REQUIRE(merged.mouse().was_pressed(mouse_button::middle));
    }

    SECTION("A click between two fixed steps reaches the next step") {
        incoming.mouse().button_down(mouse_button::left);
        incoming.mouse().button_up(mouse_button::left);

        const auto merged{merge_into_pending(pending, incoming)};

        REQUIRE(merged.mouse().press_count(mouse_button::left) == 1);
        REQUIRE(merged.mouse().release_count(mouse_button::left) == 1);
    }

    SECTION("A wheel a step consumed stays hidden, newer turns are reported") {
        pending.mouse().scroll({.vertical = 3.0F});
        pending.mouse().consume_wheel();
        incoming.mouse().scroll({.vertical = 1.0F});

        const auto merged{merge_into_pending(pending, incoming)};

        REQUIRE(merged.mouse().wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("The pointer comes from the incoming frame") {
        pending.mouse().move_pointer({.x = 1, .y = 1});
        incoming.mouse().move_pointer({.x = 2, .y = 2});
        incoming.mouse().pointer_entered();

        const auto merged{merge_into_pending(pending, incoming)};

        REQUIRE(merged.mouse().pointer() == pointer_position{.x = 2, .y = 2});
        REQUIRE(merged.mouse().is_pointer_inside());
    }

    SECTION("Folding leaves the mice of both inputs untouched") {
        pending.mouse().button_down(mouse_button::left);
        incoming.mouse().button_down(mouse_button::right);
        incoming.mouse().scroll({.vertical = 1.0F});

        [[maybe_unused]] const auto merged{merge_into_pending(pending, incoming)};

        REQUIRE(pending.mouse().press_count(mouse_button::left) == 1);
        REQUIRE_FALSE(pending.mouse().is_down(mouse_button::right));
        REQUIRE(pending.mouse().wheel() == wheel_delta{});
        REQUIRE(incoming.mouse().press_count(mouse_button::right) == 1);
        REQUIRE_FALSE(incoming.mouse().is_down(mouse_button::left));
    }
}

TEST_CASE("input_view mouse type", "[tca][engine][input][input-view][mouse]") {
    using namespace tca::engine::input;

    STATIC_REQUIRE(std::same_as<decltype(std::declval<const input_view&>().mouse()), mouse_view>);
}

TEST_CASE("input_view forwards the mouse to the frame", "[tca][engine][input][input-view][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};

    SECTION("A view of an empty frame has an untouched mouse") {
        const input_view view{frame};

        for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
            const auto b{static_cast<mouse_button>(index)};
            REQUIRE_FALSE(view.mouse().is_down(b));
            REQUIRE_FALSE(view.mouse().was_pressed(b));
        }
        REQUIRE_FALSE(view.mouse().pointer().has_value());
        REQUIRE(view.mouse().wheel() == wheel_delta{});
    }

    SECTION("The mouse of a view is the mouse of the frame, including later input") {
        const input_view view{frame};

        frame.mouse().button_down(mouse_button::left);
        frame.mouse().move_pointer({.x = 10, .y = 20});
        frame.mouse().pointer_entered();
        frame.mouse().scroll({.vertical = 1.0F});

        REQUIRE(view.mouse().is_down(mouse_button::left));
        REQUIRE(view.mouse().press_count(mouse_button::left) == 1);
        REQUIRE(view.mouse().pointer() == pointer_position{.x = 10, .y = 20});
        REQUIRE(view.mouse().is_pointer_inside());
        REQUIRE(view.mouse().wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("Consuming through the view's mouse reaches the frame") {
        frame.mouse().button_down(mouse_button::left);
        frame.mouse().button_down(mouse_button::right);
        frame.mouse().scroll({.vertical = 1.0F});
        const input_view view{frame};

        view.mouse().consume(mouse_button::left);
        view.mouse().consume_wheel();

        REQUIRE_FALSE(frame.mouse().is_down(mouse_button::left));
        REQUIRE_FALSE(frame.mouse().was_pressed(mouse_button::left));
        REQUIRE(frame.mouse().is_down(mouse_button::right));
        REQUIRE(frame.mouse().wheel() == wheel_delta{});
    }

    SECTION("consume_all() through a view leaves the mouse alone") {
        frame.keyboard().press(key::a);
        frame.mouse().button_down(mouse_button::left);
        frame.mouse().scroll({.vertical = 1.0F});
        const input_view view{frame};

        view.consume_all();

        REQUIRE_FALSE(view.keyboard().is_down(key::a));
        REQUIRE(view.mouse().is_down(mouse_button::left));
        REQUIRE(view.mouse().wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("Two views of the same frame see each other's mouse consumption") {
        frame.mouse().button_down(mouse_button::left);
        const input_view first{frame};
        const input_view second{frame};

        first.mouse().consume(mouse_button::left);

        REQUIRE_FALSE(second.mouse().is_down(mouse_button::left));
        REQUIRE_FALSE(second.mouse().was_pressed(mouse_button::left));
    }
}

TEST_CASE("input_view snapshot() and the mouse", "[tca][engine][input][input-view][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};
    frame.mouse().button_down(mouse_button::left);
    frame.mouse().button_down(mouse_button::right);
    frame.mouse().button_up(mouse_button::right);
    frame.mouse().move_pointer({.x = 10, .y = 20});
    frame.mouse().pointer_entered();
    frame.mouse().scroll({.vertical = 1.0F});
    const input_view view{frame};

    SECTION("A snapshot has the frame's buttons, pointer and wheel") {
        const input_frame copy{view.snapshot()};

        REQUIRE(copy.mouse().is_down(mouse_button::left));
        REQUIRE(copy.mouse().press_count(mouse_button::left) == 1);
        REQUIRE_FALSE(copy.mouse().is_down(mouse_button::right));
        REQUIRE(copy.mouse().release_count(mouse_button::right) == 1);
        REQUIRE(copy.mouse().pointer() == pointer_position{.x = 10, .y = 20});
        REQUIRE(copy.mouse().is_pointer_inside());
        REQUIRE(copy.mouse().wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
    }

    SECTION("Buttons and the wheel consumed in the frame are hidden in the snapshot") {
        view.mouse().consume(mouse_button::left);
        view.mouse().consume_wheel();

        input_frame copy{view.snapshot()};

        REQUIRE_FALSE(copy.mouse().is_down(mouse_button::left));
        REQUIRE_FALSE(copy.mouse().was_pressed(mouse_button::left));
        REQUIRE(copy.mouse().wheel() == wheel_delta{});

        // Its later release isn't reported by the copy either.
        copy.mouse().button_up(mouse_button::left);
        REQUIRE_FALSE(copy.mouse().was_released(mouse_button::left));
    }

    SECTION("The snapshot is detached from the frame's mouse") {
        input_frame copy{view.snapshot()};

        frame.end_tick();
        frame.mouse().button_up(mouse_button::left);
        frame.mouse().move_pointer({.x = 30, .y = 40});

        REQUIRE(copy.mouse().is_down(mouse_button::left));
        REQUIRE(copy.mouse().was_pressed(mouse_button::left));
        REQUIRE(copy.mouse().pointer() == pointer_position{.x = 10, .y = 20});
        REQUIRE(copy.mouse().wheel() == wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});

        copy.mouse().consume_all();

        REQUIRE(frame.mouse().was_released(mouse_button::left));
    }
}

TEST_CASE("input_view snapshots of consecutive ticks merge the mouse over both",
          "[tca][engine][input][input-view][mouse]") {
    using namespace tca::engine::input;

    GIVEN("A persistent frame, snapshotted at the end of each tick before end_tick()") {
        input_frame frame{};
        const input_view view{frame};

        WHEN("The left button goes down in one tick and up in the next, scrolling in both") {
            frame.mouse().button_down(mouse_button::left);
            frame.mouse().scroll({.vertical = 1.0F});
            const input_frame first{view.snapshot()};
            frame.end_tick();

            frame.mouse().button_up(mouse_button::left);
            frame.mouse().scroll({.vertical = 1.0F});
            const input_frame second{view.snapshot()};
            frame.end_tick();

            const input_frame merged{merge(first, second)};

            THEN("The merge has both edges once each, the button up and both turns") {
                REQUIRE(merged.mouse().press_count(mouse_button::left) == 1);
                REQUIRE(merged.mouse().release_count(mouse_button::left) == 1);
                REQUIRE_FALSE(merged.mouse().is_down(mouse_button::left));
                REQUIRE(merged.mouse().wheel() ==
                        wheel_delta{.vertical = 2.0F, .horizontal = 0.0F});
            }
        }

        WHEN("The left button goes down in one tick and is only held in the next") {
            frame.mouse().button_down(mouse_button::left);
            frame.mouse().scroll({.vertical = 1.0F});
            const input_frame first{view.snapshot()};
            frame.end_tick();

            const input_frame second{view.snapshot()};
            frame.end_tick();

            const input_frame merged{merge(first, second)};

            THEN("The press and the turn are counted once, not once per tick") {
                REQUIRE(merged.mouse().press_count(mouse_button::left) == 1);
                REQUIRE(merged.mouse().is_down(mouse_button::left));
                REQUIRE(merged.mouse().wheel() ==
                        wheel_delta{.vertical = 1.0F, .horizontal = 0.0F});
            }
        }
    }
}
