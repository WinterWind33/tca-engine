// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/game-application-logic.hpp"

// C++ STL
#include <chrono>
#include <initializer_list>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Engine
#include "input/input-frame.hpp"
#include "input/key.hpp"

namespace {

using call_log = std::vector<std::string>;

class test_game_logic : public tca::engine::application::game_application_logic {
public:
    using game_application_logic::get_renderables;
    using game_application_logic::get_updatables;
};

class updatable_fake : public tca::engine::rendering::updatable {
public:
    explicit updatable_fake(call_log& log, std::string name)
        : m_log{&log},
          m_name{std::move(name)} {}

    void update(tca::engine::rendering::update_context& context) override {
        m_deltaTime = context.delta();
        m_lastContext = &context;
        m_log->push_back("update " + m_name);
    }

    delta_time get_delta_time() const noexcept {
        return m_deltaTime;
    }

    const tca::engine::rendering::update_context* get_last_context() const noexcept {
        return m_lastContext;
    }

private:
    call_log* m_log{};
    std::string m_name{};
    delta_time m_deltaTime{};
    const tca::engine::rendering::update_context* m_lastContext{};
};

class renderable_fake : public tca::engine::rendering::renderable {
public:
    explicit renderable_fake(call_log& log, std::string name)
        : m_log{&log},
          m_name{std::move(name)} {}

    void render() override {
        m_log->push_back("render " + m_name);
    }

private:
    call_log* m_log{};
    std::string m_name{};
};

// What one updatable saw of the space key and the typed text during its last update(), optionally
// consuming the space key afterwards.
class input_reader_fake : public tca::engine::rendering::updatable {
public:
    struct seen_input {
        bool spaceDown{};
        bool spacePressed{};
        bool spaceReleased{};
        std::string text{};
    };

    explicit input_reader_fake(const bool consumesSpace = false) : m_consumesSpace{consumesSpace} {}

    void update(tca::engine::rendering::update_context& context) override {
        using tca::engine::input::key;

        const auto keyboard{context.input().keyboard()};
        m_seen = seen_input{
            .spaceDown = keyboard.is_down(key::space),
            .spacePressed = keyboard.was_pressed(key::space),
            .spaceReleased = keyboard.was_released(key::space),
            .text = std::string{context.input().text()},
        };

        if (m_consumesSpace) {
            context.input().consume(key::space);
        }
    }

    const std::optional<seen_input>& get_seen() const noexcept {
        return m_seen;
    }

private:
    bool m_consumesSpace{};
    std::optional<seen_input> m_seen{};
};

} // namespace

TEST_CASE("game_application_logic class tests",
          "[tca][engine][application][game_application_logic]") {
    using namespace tca::engine;

    GIVEN("A game application logic with no registered objects") {
        test_game_logic logic{};
        application::application_logic_base* logicBasePtr = &logic;

        THEN("tick() does nothing and keeps the application running") {
            REQUIRE(logicBasePtr->tick());
        }
    }

    GIVEN("A game application logic with interleaved registrations") {
        test_game_logic logic{};

        // We should be able to tick the logic, so we need
        // to retrieve a pointer to the base class and call the tick method.
        application::application_logic_base* logicBasePtr = &logic;

        call_log log{};
        renderable_fake renderableA{log, "A"};
        updatable_fake updatableA{log, "A"};
        renderable_fake renderableB{log, "B"};
        updatable_fake updatableB{log, "B"};

        // Registered renderable first, to show that the order of registration between the two
        // kinds doesn't matter: the update pass always runs first.
        logic.get_renderables().add_renderable(renderableA);
        logic.get_updatables().add_updatable(updatableA);
        logic.get_renderables().add_renderable(renderableB);
        logic.get_updatables().add_updatable(updatableB);

        WHEN("tick() is called the first time") {
            REQUIRE(logicBasePtr->tick());

            THEN("Every updatable is updated, then every renderable is rendered") {
                const call_log expected{"update A", "update B", "render A", "render B"};
                REQUIRE(log == expected);
            }

            THEN("The delta time passed to the updatables should be 0") {
                REQUIRE(updatableA.get_delta_time() == test_game_logic::duration{0});
                REQUIRE(updatableB.get_delta_time() == test_game_logic::duration{0});
            }

            THEN("Every updatable is handed the same context") {
                REQUIRE(updatableA.get_last_context() != nullptr);
                REQUIRE(updatableB.get_last_context() == updatableA.get_last_context());
            }
        }

        WHEN("Some objects are paused and tick() is called") {
            logic.get_updatables().pause_updatable(updatableA);
            logic.get_renderables().pause_renderable(renderableB);
            REQUIRE(logicBasePtr->tick());

            THEN("The paused objects are skipped in their pass") {
                const call_log expected{"update B", "render A"};
                REQUIRE(log == expected);
            }
        }

        WHEN("tick() is called twice with a non-zero interval") {
            const auto timePointBefore{std::chrono::steady_clock::now()};

            REQUIRE(logicBasePtr->tick());
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
            REQUIRE(logicBasePtr->tick());

            const auto timePointAfter{std::chrono::steady_clock::now()};
            const auto expectedDeltaTime{std::chrono::duration_cast<test_game_logic::duration>(
                timePointAfter - timePointBefore)};

            THEN("Each tick runs its own update pass before its render pass") {
                const call_log expected{"update A", "update B", "render A", "render B",
                                        "update A", "update B", "render A", "render B"};
                REQUIRE(log == expected);
            }

            THEN(
                "The delta time passed to the updatables should be approximately the time between "
                "the two ticks") {
                REQUIRE(expectedDeltaTime > test_game_logic::duration{0});
                REQUIRE(updatableA.get_delta_time() > test_game_logic::duration{0});

                // To check that the delta time is approximately the expected delta time, we use a
                // epsilon value (1000us) to account for any small delays in the execution of the
                // code. If the execution exceeds this then it's time to change the PC.
                REQUIRE(std::chrono::abs(updatableA.get_delta_time() - expectedDeltaTime) <
                        test_game_logic::duration{1000});
                REQUIRE(updatableB.get_delta_time() == updatableA.get_delta_time());
            }
        }
    }
}

TEST_CASE("game_application_logic hands its input to its updatables",
          "[tca][engine][application][game_application_logic][input]") {
    using namespace tca::engine;

    test_game_logic logic{};
    application::application_logic& asLogic{logic};

    input_reader_fake readerA{};
    input_reader_fake readerB{};
    logic.get_updatables().add_updatable(readerA);
    logic.get_updatables().add_updatable(readerB);

    GIVEN("No input was set") {
        THEN("tick() still runs and every updatable sees an empty frame") {
            REQUIRE(asLogic.tick());

            REQUIRE(readerA.get_seen().has_value());
            REQUIRE_FALSE(readerA.get_seen()->spaceDown);
            REQUIRE_FALSE(readerA.get_seen()->spacePressed);
            REQUIRE(readerA.get_seen()->text.empty());
        }
    }

    GIVEN("A hand-fed frame with a press and some text") {
        input::input_frame frame{};
        frame.keyboard().press(input::key::space);
        frame.append_text("go");
        asLogic.set_input(input::input_view{frame});

        WHEN("tick() is called") {
            REQUIRE(asLogic.tick());

            THEN("Every updatable sees the press and the text") {
                for (const auto* reader : {&readerA, &readerB}) {
                    REQUIRE(reader->get_seen().has_value());
                    REQUIRE(reader->get_seen()->spaceDown);
                    REQUIRE(reader->get_seen()->spacePressed);
                    REQUIRE(reader->get_seen()->text == "go");
                }
            }

            THEN("The logic doesn't end the tick: the frame keeps its edges and text") {
                // Ending the tick is the application's job, so a frame the application skips
                // ending keeps its edges for the next tick that runs.
                REQUIRE(frame.keyboard().was_pressed(input::key::space));
                REQUIRE(frame.text() == "go");
            }

            AND_WHEN("tick() is called again without ending the tick in between") {
                REQUIRE(asLogic.tick());

                THEN("The updatables see the same press and text again") {
                    REQUIRE(readerA.get_seen()->spacePressed);
                    REQUIRE(readerA.get_seen()->text == "go");
                }
            }

            AND_WHEN("The tick is ended, the key released and tick() called again") {
                frame.end_tick();
                frame.keyboard().release(input::key::space);
                REQUIRE(asLogic.tick());

                THEN("The updatables see the frame as it is now: the release, no press, no text") {
                    for (const auto* reader : {&readerA, &readerB}) {
                        REQUIRE_FALSE(reader->get_seen()->spaceDown);
                        REQUIRE_FALSE(reader->get_seen()->spacePressed);
                        REQUIRE(reader->get_seen()->spaceReleased);
                        REQUIRE(reader->get_seen()->text.empty());
                    }
                }
            }
        }
    }

    GIVEN("A frame and an updatable that consumes the space key, registered first") {
        test_game_logic consumingLogic{};
        application::application_logic& asConsumingLogic{consumingLogic};

        input_reader_fake consumer{true};
        input_reader_fake later{};
        consumingLogic.get_updatables().add_updatable(consumer);
        consumingLogic.get_updatables().add_updatable(later);

        input::input_frame frame{};
        frame.keyboard().press(input::key::space);
        frame.append_text("t");
        asConsumingLogic.set_input(input::input_view{frame});

        WHEN("tick() is called") {
            REQUIRE(asConsumingLogic.tick());

            THEN("The consumer saw the press") {
                REQUIRE(consumer.get_seen()->spacePressed);
            }

            THEN("The later updatable doesn't see the consumed key, only the text") {
                REQUIRE_FALSE(later.get_seen()->spaceDown);
                REQUIRE_FALSE(later.get_seen()->spacePressed);
                REQUIRE(later.get_seen()->text == "t");
            }

            THEN("The consumption reached the frame") {
                REQUIRE_FALSE(frame.keyboard().is_down(input::key::space));
                REQUIRE_FALSE(frame.keyboard().was_pressed(input::key::space));
            }
        }
    }
}
