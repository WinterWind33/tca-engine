// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/input-collector.hpp"

#include "input/input-frame.hpp"
#include "input/key.hpp"
#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"

// C++ STL
#include <cstddef>
#include <initializer_list>
#include <string>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Utilities
#include "utilities/input/mouse-string-makers.hpp"

TEST_CASE("append_utf8() encodes code points", "[tca][engine][input][input-collector][utf8]") {
    using namespace tca::engine::input;

    std::string text{};

    SECTION("A code point below 0x80 takes one byte") {
        append_utf8(text, U'a');

        REQUIRE(text == "a");
    }

    SECTION("A code point below 0x800 takes two bytes") {
        append_utf8(text, char32_t{0xE9}); // e acute

        REQUIRE(text == "\xC3\xA9");
    }

    SECTION("A code point below 0x10000 takes three bytes") {
        append_utf8(text, char32_t{0x20AC}); // euro sign

        REQUIRE(text == "\xE2\x82\xAC");
    }

    SECTION("A code point from 0x10000 takes four bytes") {
        append_utf8(text, char32_t{0x1F600}); // grinning face

        REQUIRE(text == "\xF0\x9F\x98\x80");
    }

    SECTION("The boundaries of each length are encoded with the right length") {
        append_utf8(text, char32_t{0x7F});
        REQUIRE(text == "\x7F");
        text.clear();

        append_utf8(text, char32_t{0x80});
        REQUIRE(text == "\xC2\x80");
        text.clear();

        append_utf8(text, char32_t{0x7FF});
        REQUIRE(text == "\xDF\xBF");
        text.clear();

        append_utf8(text, char32_t{0x800});
        REQUIRE(text == "\xE0\xA0\x80");
        text.clear();

        append_utf8(text, char32_t{0xFFFF});
        REQUIRE(text == "\xEF\xBF\xBF");
        text.clear();

        append_utf8(text, char32_t{0x10000});
        REQUIRE(text == "\xF0\x90\x80\x80");
        text.clear();

        append_utf8(text, char32_t{0x10FFFF});
        REQUIRE(text == "\xF4\x8F\xBF\xBF");
    }

    SECTION("The encoding is appended after what the string already holds") {
        text = "ab";

        append_utf8(text, char32_t{0xE9});

        REQUIRE(text == "ab\xC3\xA9");
    }

    SECTION("Surrogates are not code points and append nothing") {
        text = "x";

        for (const char32_t surrogate :
             {char32_t{0xD800}, char32_t{0xDBFF}, char32_t{0xDC00}, char32_t{0xDFFF}}) {
            append_utf8(text, surrogate);
        }

        REQUIRE(text == "x");
    }

    SECTION("Values above 0x10FFFF are not code points and append nothing") {
        text = "x";

        append_utf8(text, char32_t{0x110000});
        append_utf8(text, char32_t{0xFFFFFFFF});

        REQUIRE(text == "x");
    }

    SECTION("Control characters are still encoded: filtering them is the collector's job") {
        append_utf8(text, U'\n');

        REQUIRE(text == "\n");
    }
}

TEST_CASE("input_collector key presses and releases", "[tca][engine][input][input-collector]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    SECTION("A key down presses the key in the frame") {
        collector.on_key_down(key::space, false);

        REQUIRE(frame.keyboard().is_down(key::space));
        REQUIRE(frame.keyboard().press_count(key::space) == 1);
    }

    SECTION("A key up after a key down releases the key in the frame") {
        collector.on_key_down(key::space, false);
        collector.on_key_up(key::space);

        REQUIRE_FALSE(frame.keyboard().is_down(key::space));
        REQUIRE(frame.keyboard().press_count(key::space) == 1);
        REQUIRE(frame.keyboard().release_count(key::space) == 1);
    }

    SECTION("Repeats while a key is held are not new presses") {
        collector.on_key_down(key::d, false);
        collector.on_key_down(key::d, true);
        collector.on_key_down(key::d, true);
        collector.on_key_down(key::d, true);

        REQUIRE(frame.keyboard().is_down(key::d));
        REQUIRE(frame.keyboard().press_count(key::d) == 1);
    }

    SECTION("Repeats in later ticks keep the key down without reporting a press") {
        collector.on_key_down(key::d, false);
        frame.end_tick();

        collector.on_key_down(key::d, true);

        REQUIRE(frame.keyboard().is_down(key::d));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::d));
    }

    SECTION("A repeat of a key that is not held is ignored") {
        // A key already held when the window got focus arrives as a repeat.
        collector.on_key_down(key::w, true);

        REQUIRE_FALSE(frame.keyboard().is_down(key::w));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::w));
    }

    SECTION("The key up of a key that was never pressed is ignored") {
        collector.on_key_up(key::w);

        REQUIRE_FALSE(frame.keyboard().is_down(key::w));
        REQUIRE_FALSE(frame.keyboard().was_released(key::w));
    }

    SECTION("Two taps within one tick are two presses and two releases") {
        collector.on_key_down(key::j, false);
        collector.on_key_up(key::j);
        collector.on_key_down(key::j, false);
        collector.on_key_up(key::j);

        REQUIRE_FALSE(frame.keyboard().is_down(key::j));
        REQUIRE(frame.keyboard().press_count(key::j) == 2);
        REQUIRE(frame.keyboard().release_count(key::j) == 2);
    }

    SECTION("Different keys are tracked independently") {
        collector.on_key_down(key::a, false);
        collector.on_key_down(key::d, false);
        collector.on_key_up(key::a);

        REQUIRE_FALSE(frame.keyboard().is_down(key::a));
        REQUIRE(frame.keyboard().is_down(key::d));
        REQUIRE(frame.keyboard().release_count(key::d) == 0);
    }

    SECTION("Key events leave the typed text alone") {
        collector.on_key_down(key::a, false);
        collector.on_key_up(key::a);

        REQUIRE(frame.text().empty());
    }
}

TEST_CASE("input_collector never undoes consumption", "[tca][engine][input][input-collector]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    collector.on_key_down(key::escape, false);
    frame.consume(key::escape);

    SECTION("Repeats of a consumed held key keep it hidden") {
        collector.on_key_down(key::escape, true);

        REQUIRE_FALSE(frame.keyboard().is_down(key::escape));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::escape));
    }

    SECTION("A consumed held key stays hidden across end_tick() and its repeats") {
        frame.end_tick();
        collector.on_key_down(key::escape, true);
        frame.end_tick();
        collector.on_key_down(key::escape, true);

        REQUIRE_FALSE(frame.keyboard().is_down(key::escape));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::escape));
    }

    SECTION("The release of a consumed held key is swallowed") {
        frame.end_tick();
        collector.on_key_up(key::escape);

        REQUIRE_FALSE(frame.keyboard().is_down(key::escape));
        REQUIRE_FALSE(frame.keyboard().was_released(key::escape));
    }

    SECTION("A new press after the release is reported normally") {
        frame.end_tick();
        collector.on_key_up(key::escape);
        frame.end_tick();
        collector.on_key_down(key::escape, false);

        REQUIRE(frame.keyboard().is_down(key::escape));
        REQUIRE(frame.keyboard().press_count(key::escape) == 1);
    }

    SECTION("Focus loss does not report the release of a consumed held key") {
        collector.on_focus_lost();

        REQUIRE_FALSE(frame.keyboard().was_released(key::escape));
    }
}

TEST_CASE("input_collector on_focus_lost()", "[tca][engine][input][input-collector]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    SECTION("Every held key is released, reporting one release each") {
        collector.on_key_down(key::a, false);
        collector.on_key_down(key::shift_left, false);
        frame.end_tick();

        collector.on_focus_lost();

        for (const auto k : {key::a, key::shift_left}) {
            REQUIRE_FALSE(frame.keyboard().is_down(k));
            REQUIRE(frame.keyboard().release_count(k) == 1);
        }
    }

    SECTION("Keys that are already up report nothing") {
        collector.on_key_down(key::a, false);
        collector.on_key_up(key::a);
        frame.end_tick();

        collector.on_focus_lost();

        REQUIRE_FALSE(frame.keyboard().was_released(key::a));
    }

    SECTION("Presses seen earlier in the tick are kept") {
        collector.on_key_down(key::space, false);

        collector.on_focus_lost();

        REQUIRE(frame.keyboard().press_count(key::space) == 1);
        REQUIRE(frame.keyboard().release_count(key::space) == 1);
    }

    SECTION("The key ups that arrive late are ignored") {
        collector.on_key_down(key::meta_left, false);
        collector.on_focus_lost();
        frame.end_tick();

        collector.on_key_up(key::meta_left);

        REQUIRE_FALSE(frame.keyboard().was_released(key::meta_left));
    }

    SECTION("A key held through focus loss is ignored until pressed again") {
        // Back in focus with the key still physically down: Win32 reports it as a repeat.
        collector.on_key_down(key::d, false);
        collector.on_focus_lost();
        frame.end_tick();

        collector.on_key_down(key::d, true);

        REQUIRE_FALSE(frame.keyboard().is_down(key::d));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::d));
    }

    SECTION("The typed text is left alone") {
        collector.on_char(U'a');

        collector.on_focus_lost();

        REQUIRE(frame.text() == "a");
    }
}

TEST_CASE("input_collector clear()", "[tca][engine][input][input-collector]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    SECTION("Held keys are forgotten without reporting a release") {
        collector.on_key_down(key::d, false);
        frame.end_tick();

        collector.clear();

        REQUIRE_FALSE(frame.keyboard().is_down(key::d));
        REQUIRE_FALSE(frame.keyboard().was_released(key::d));
    }

    SECTION("Edges seen so far are forgotten") {
        collector.on_key_down(key::j, false);
        collector.on_key_up(key::j);

        collector.clear();

        REQUIRE_FALSE(frame.keyboard().was_pressed(key::j));
        REQUIRE_FALSE(frame.keyboard().was_released(key::j));
    }

    SECTION("The typed text is forgotten") {
        collector.on_char(U'a');

        collector.clear();

        REQUIRE(frame.text().empty());
    }

    SECTION("A key held through it is ignored until released") {
        collector.on_key_down(key::d, false);
        collector.clear();

        collector.on_key_down(key::d, true);
        REQUIRE_FALSE(frame.keyboard().is_down(key::d));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::d));

        collector.on_key_up(key::d);
        REQUIRE_FALSE(frame.keyboard().was_released(key::d));
    }

    SECTION("A key held through it is reported again once pressed again") {
        collector.on_key_down(key::d, false);
        collector.clear();
        collector.on_key_up(key::d);
        frame.end_tick();

        collector.on_key_down(key::d, false);

        REQUIRE(frame.keyboard().is_down(key::d));
        REQUIRE(frame.keyboard().press_count(key::d) == 1);
    }

    SECTION("Events after it are collected normally") {
        collector.clear();

        collector.on_key_down(key::space, false);
        collector.on_char(U'b');

        REQUIRE(frame.keyboard().is_down(key::space));
        REQUIRE(frame.keyboard().press_count(key::space) == 1);
        REQUIRE(frame.text() == "b");
    }
}

TEST_CASE("input_collector text", "[tca][engine][input][input-collector]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    SECTION("Characters of every UTF-8 length are appended in order") {
        collector.on_char(U'h');              // 1 byte
        collector.on_char(char32_t{0xE9});    // e acute, 2 bytes
        collector.on_char(char32_t{0x20AC});  // euro sign, 3 bytes
        collector.on_char(char32_t{0x1F600}); // grinning face, 4 bytes

        REQUIRE(frame.text() == "h\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");
    }

    SECTION("Space and the first printable characters around the control ranges are kept") {
        collector.on_char(U' ');           // 0x20
        collector.on_char(U'~');           // 0x7E
        collector.on_char(char32_t{0xA0}); // no-break space, right after the C1 range

        REQUIRE(frame.text() == " ~\xC2\xA0");
    }

    SECTION("C0 control characters are dropped") {
        for (const char32_t control :
             {char32_t{0x00}, char32_t{0x08}, char32_t{0x09}, char32_t{0x0A}, char32_t{0x0D},
              char32_t{0x1B}, char32_t{0x1F}}) {
            collector.on_char(control);
        }

        REQUIRE(frame.text().empty());
    }

    SECTION("DEL and C1 control characters are dropped") {
        for (const char32_t control :
             {char32_t{0x7F}, char32_t{0x80}, char32_t{0x85}, char32_t{0x9F}}) {
            collector.on_char(control);
        }

        REQUIRE(frame.text().empty());
    }

    SECTION("Surrogates are dropped") {
        collector.on_char(char32_t{0xD83D});
        collector.on_char(char32_t{0xDE00});

        REQUIRE(frame.text().empty());
    }

    SECTION("Values above 0x10FFFF are dropped") {
        collector.on_char(char32_t{0x110000});

        REQUIRE(frame.text().empty());
    }

    SECTION("A dropped character does not disturb its neighbours") {
        collector.on_char(U'a');
        collector.on_char(char32_t{0x08});
        collector.on_char(char32_t{0xD800});
        collector.on_char(U'b');

        REQUIRE(frame.text() == "ab");
    }

    SECTION("Characters leave the keyboard alone") {
        collector.on_char(U'a');

        REQUIRE_FALSE(frame.keyboard().is_down(key::a));
        REQUIRE_FALSE(frame.keyboard().was_pressed(key::a));
    }

    SECTION("Text typed after end_tick() is the next tick's only") {
        collector.on_char(U'a');
        frame.end_tick();
        collector.on_char(U'b');

        REQUIRE(frame.text() == "b");
    }

    SECTION("Text consumed by a reader is not brought back") {
        collector.on_char(U'a');
        frame.consume_all();
        collector.on_char(U'b');

        REQUIRE(frame.text() == "b");
    }
}

TEST_CASE("input_collector writes into the frame it was given",
          "[tca][engine][input][input-collector]") {
    using namespace tca::engine::input;

    input_frame first{};
    input_frame second{};
    input_collector firstCollector{first};
    input_collector secondCollector{second};

    firstCollector.on_key_down(key::a, false);
    firstCollector.on_char(U'a');
    secondCollector.on_key_down(key::b, false);

    REQUIRE(first.keyboard().is_down(key::a));
    REQUIRE_FALSE(first.keyboard().is_down(key::b));
    REQUIRE(first.text() == "a");

    REQUIRE(second.keyboard().is_down(key::b));
    REQUIRE_FALSE(second.keyboard().is_down(key::a));
    REQUIRE(second.text().empty());
}

TEST_CASE("input_collector mouse buttons", "[tca][engine][input][input-collector][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    SECTION("A button down presses the button in the frame") {
        collector.on_mouse_button_down(mouse_button::left);

        REQUIRE(frame.mouse().is_down(mouse_button::left));
        REQUIRE(frame.mouse().press_count(mouse_button::left) == 1);
    }

    SECTION("A button up releases it") {
        collector.on_mouse_button_down(mouse_button::right);
        collector.on_mouse_button_up(mouse_button::right);

        REQUIRE_FALSE(frame.mouse().is_down(mouse_button::right));
        REQUIRE(frame.mouse().press_count(mouse_button::right) == 1);
        REQUIRE(frame.mouse().release_count(mouse_button::right) == 1);
    }

    SECTION("Every button is fed to its own slot") {
        for (const auto button : {mouse_button::left, mouse_button::right, mouse_button::middle,
                                  mouse_button::x1, mouse_button::x2}) {
            CAPTURE(mouse_button_name(button));
            collector.on_mouse_button_down(button);

            REQUIRE(frame.mouse().is_down(button));
            REQUIRE(frame.mouse().press_count(button) == 1);
        }
    }

    SECTION("A button up for a button that isn't held reports nothing") {
        collector.on_mouse_button_up(mouse_button::middle);

        REQUIRE_FALSE(frame.mouse().was_released(mouse_button::middle));
    }

    SECTION("Two clicks within one tick are both counted") {
        collector.on_mouse_button_down(mouse_button::left);
        collector.on_mouse_button_up(mouse_button::left);
        collector.on_mouse_button_down(mouse_button::left);
        collector.on_mouse_button_up(mouse_button::left);

        REQUIRE(frame.mouse().press_count(mouse_button::left) == 2);
        REQUIRE(frame.mouse().release_count(mouse_button::left) == 2);
    }

    SECTION("Buttons leave the keyboard alone") {
        collector.on_mouse_button_down(mouse_button::left);

        for (std::size_t i{}; i < key_count; ++i) {
            REQUIRE_FALSE(frame.keyboard().is_down(static_cast<key>(i)));
        }
    }
}

TEST_CASE("input_collector never undoes mouse button consumption",
          "[tca][engine][input][input-collector][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    collector.on_mouse_button_down(mouse_button::left);
    frame.mouse().consume(mouse_button::left);

    SECTION("The release of a consumed held button is swallowed") {
        frame.end_tick();
        collector.on_mouse_button_up(mouse_button::left);

        REQUIRE_FALSE(frame.mouse().was_released(mouse_button::left));
    }

    SECTION("A new press after the release is reported normally") {
        frame.end_tick();
        collector.on_mouse_button_up(mouse_button::left);
        frame.end_tick();
        collector.on_mouse_button_down(mouse_button::left);

        REQUIRE(frame.mouse().is_down(mouse_button::left));
        REQUIRE(frame.mouse().press_count(mouse_button::left) == 1);
    }

    SECTION("Focus loss does not report the release of a consumed held button") {
        collector.on_focus_lost();

        REQUIRE_FALSE(frame.mouse().was_released(mouse_button::left));
    }
}

TEST_CASE("input_collector pointer", "[tca][engine][input][input-collector][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    SECTION("The pointer has no position until it moves") {
        REQUIRE_FALSE(frame.mouse().pointer().has_value());
    }

    SECTION("A move sets the position, the latest one wins") {
        collector.on_pointer_moved({.x = 10, .y = 20});
        collector.on_pointer_moved({.x = 30, .y = 40});

        REQUIRE(frame.mouse().pointer() == pointer_position{.x = 30, .y = 40});
    }

    SECTION("Positions outside the client area (a captured drag) are kept as they are") {
        collector.on_pointer_moved({.x = -15, .y = 5000});

        REQUIRE(frame.mouse().pointer() == pointer_position{.x = -15, .y = 5000});
    }

    SECTION("Entering and leaving the client area are reported") {
        collector.on_pointer_entered();
        REQUIRE(frame.mouse().is_pointer_inside());

        collector.on_pointer_left();
        REQUIRE_FALSE(frame.mouse().is_pointer_inside());
    }

    SECTION("Leaving keeps the last position") {
        collector.on_pointer_entered();
        collector.on_pointer_moved({.x = 3, .y = 4});
        collector.on_pointer_left();

        REQUIRE(frame.mouse().pointer() == pointer_position{.x = 3, .y = 4});
    }

    SECTION("The pointer survives end_tick()") {
        collector.on_pointer_entered();
        collector.on_pointer_moved({.x = 7, .y = 8});
        frame.end_tick();

        REQUIRE(frame.mouse().is_pointer_inside());
        REQUIRE(frame.mouse().pointer() == pointer_position{.x = 7, .y = 8});
    }
}

TEST_CASE("input_collector wheel", "[tca][engine][input][input-collector][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    SECTION("Turns within one tick are summed per axis") {
        collector.on_scroll({.vertical = 1.0F});
        collector.on_scroll({.vertical = 0.5F, .horizontal = -1.0F});

        REQUIRE(frame.mouse().wheel() == wheel_delta{.vertical = 1.5F, .horizontal = -1.0F});
    }

    SECTION("end_tick() resets the wheel") {
        collector.on_scroll({.vertical = 1.0F});
        frame.end_tick();

        REQUIRE(frame.mouse().wheel() == wheel_delta{});
    }

    SECTION("A turn after the wheel was consumed is reported") {
        collector.on_scroll({.vertical = 1.0F});
        frame.mouse().consume_wheel();
        collector.on_scroll({.vertical = -2.0F});

        REQUIRE(frame.mouse().wheel() == wheel_delta{.vertical = -2.0F});
    }
}

TEST_CASE("input_collector on_focus_lost() and the mouse",
          "[tca][engine][input][input-collector][mouse]") {
    using namespace tca::engine::input;

    input_frame frame{};
    input_collector collector{frame};

    SECTION("Every held button is released, reporting one release each") {
        collector.on_mouse_button_down(mouse_button::left);
        collector.on_mouse_button_down(mouse_button::x2);
        frame.end_tick();

        collector.on_focus_lost();

        for (const auto button : {mouse_button::left, mouse_button::x2}) {
            CAPTURE(mouse_button_name(button));
            REQUIRE_FALSE(frame.mouse().is_down(button));
            REQUIRE(frame.mouse().release_count(button) == 1);
        }
    }

    SECTION("The button-ups that arrive late are ignored") {
        collector.on_mouse_button_down(mouse_button::right);
        collector.on_focus_lost();
        frame.end_tick();

        collector.on_mouse_button_up(mouse_button::right);

        REQUIRE_FALSE(frame.mouse().was_released(mouse_button::right));
    }

    SECTION("Keys and buttons are both released") {
        collector.on_key_down(key::w, false);
        collector.on_mouse_button_down(mouse_button::left);

        collector.on_focus_lost();

        REQUIRE_FALSE(frame.keyboard().is_down(key::w));
        REQUIRE_FALSE(frame.mouse().is_down(mouse_button::left));
    }

    SECTION("The pointer and the wheel are left alone") {
        collector.on_pointer_entered();
        collector.on_pointer_moved({.x = 1, .y = 2});
        collector.on_scroll({.vertical = 1.0F});

        collector.on_focus_lost();

        REQUIRE(frame.mouse().is_pointer_inside());
        REQUIRE(frame.mouse().pointer() == pointer_position{.x = 1, .y = 2});
        REQUIRE(frame.mouse().wheel() == wheel_delta{.vertical = 1.0F});
    }
}

TEST_CASE("input_collector writes the mouse into the frame it was given",
          "[tca][engine][input][input-collector][mouse]") {
    using namespace tca::engine::input;

    input_frame first{};
    input_frame second{};
    input_collector firstCollector{first};
    input_collector secondCollector{second};

    firstCollector.on_mouse_button_down(mouse_button::left);
    firstCollector.on_pointer_moved({.x = 1, .y = 1});
    secondCollector.on_scroll({.vertical = 1.0F});

    REQUIRE(first.mouse().is_down(mouse_button::left));
    REQUIRE(first.mouse().wheel() == wheel_delta{});

    REQUIRE_FALSE(second.mouse().is_down(mouse_button::left));
    REQUIRE_FALSE(second.mouse().pointer().has_value());
    REQUIRE(second.mouse().wheel() == wheel_delta{.vertical = 1.0F});
}
