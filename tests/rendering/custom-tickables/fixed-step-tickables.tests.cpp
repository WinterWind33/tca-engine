// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/custom-tickables/fixed-step-tickables.hpp"
#include "rendering/tickables.hpp"

#include "input/input-frame.hpp"
#include "input/key.hpp"
#include "input/keyboard-state.hpp"
#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"

#include "utilities/input/mouse-string-makers.hpp"
#include "utilities/update-context.hpp"

// C++ STL
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

using tca::engine::core::ranged_value_exception;
using tca::engine::input::input_frame;
using tca::engine::input::input_view;
using tca::engine::input::key;
using tca::engine::input::keyboard_state;
using tca::engine::input::mouse_button;
using tca::engine::input::mouse_state;
using tca::engine::input::pointer_position;
using tca::engine::input::wheel_delta;
using tca::engine::rendering::fixed_step_updatable;
using tca::engine::rendering::step_context;
using tca::engine::rendering::updatable;
using tca::engine::rendering::update_context;
using tca::tests::utilities::update_with_delta;
using namespace std::chrono_literals;

using delta_time = updatable::delta_time;
using step_duration = fixed_step_updatable::step_duration;
using step_count = fixed_step_updatable::step_count;
using max_steps_per_frame = fixed_step_updatable::max_steps_per_frame;
using alpha_type = fixed_step_updatable::alpha_type;

constexpr delta_time ONE_SECOND{1s};

// The step is kept in nanoseconds, so 1s / rate is exact for every rate dividing 10^9 (50, 100,
// 125, ...). Others round down (60 Hz is 16'666'666ns), which still gives exactly `rate` steps per
// second over the durations tested here; see the no-drift test.
constexpr step_duration step_duration_for(const std::int64_t stepRateHz) {
    return step_duration{std::chrono::nanoseconds{ONE_SECOND} / stepRateHz};
}

/**
 * @brief A fixed_step_updatable that records every step() call. An optional callback runs inside
 * step(), so a test can observe the updatable's state and the step's context while it is stepping.
 */
class fixed_step_updatable_spy final : public fixed_step_updatable {
public:
    fixed_step_updatable_spy() = default;

    fixed_step_updatable_spy(
        const step_duration stepDuration, const max_steps_per_frame maxStepsPerFrame,
        std::function<void(fixed_step_updatable_spy&, step_context&)> onStep = {})
        : fixed_step_updatable{stepDuration, maxStepsPerFrame},
          m_onStep{std::move(onStep)} {}

    [[nodiscard]] step_count get_step_calls() const noexcept {
        return m_stepCalls;
    }

private:
    void step(step_context& context) override {
        ++m_stepCalls;
        if (m_onStep) {
            m_onStep(*this, context);
        }
    }

    step_count m_stepCalls{};
    std::function<void(fixed_step_updatable_spy&, step_context&)> m_onStep{};
};

/**
 * @brief Returns the integer-microsecond frame deltas a display running at displayRateHz produces
 * over the given number of seconds. The deltas are not all equal (1'000'000 / 165 isn't an
 * integer), but they sum to exactly `seconds` seconds: frame i lasts
 * floor((i + 1) * 1s / rate) - floor(i * 1s / rate).
 */
std::vector<delta_time> display_frame_deltas(const std::int64_t displayRateHz,
                                             const std::int64_t seconds = 1) {
    std::vector<delta_time> deltas{};
    const std::int64_t frameCount = displayRateHz * seconds;
    deltas.reserve(static_cast<std::size_t>(frameCount));

    const std::int64_t oneSecondUs = ONE_SECOND.count();
    for (std::int64_t frame = 0; frame < frameCount; ++frame) {
        const std::int64_t begin = (frame * oneSecondUs) / displayRateHz;
        const std::int64_t end = ((frame + 1) * oneSecondUs) / displayRateHz;
        deltas.emplace_back(end - begin);
    }

    return deltas;
}

/**
 * @brief Returns deterministic, jittered frame deltas in [minDelta, maxDelta] that sum to exactly
 * totalTime (the last delta is clipped to land on it).
 */
std::vector<delta_time> jittered_frame_deltas(const delta_time totalTime, const delta_time minDelta,
                                              const delta_time maxDelta, const std::uint32_t seed) {
    std::mt19937 generator{seed};
    std::uniform_int_distribution<std::int64_t> distribution{minDelta.count(), maxDelta.count()};

    std::vector<delta_time> deltas{};
    delta_time elapsed{};
    while (elapsed < totalTime) {
        delta_time delta{distribution(generator)};
        if (elapsed + delta > totalTime) {
            delta = totalTime - elapsed;
        }

        deltas.push_back(delta);
        elapsed += delta;
    }

    return deltas;
}

void feed(updatable& target, const std::vector<delta_time>& deltas) {
    for (const auto delta : deltas) {
        update_with_delta(target, delta);
    }
}

/**
 * @brief What one step saw of a single key and of the typed text.
 */
struct step_snapshot {
    bool down{};
    bool pressed{};
    bool released{};
    keyboard_state::edge_count presses{};
    keyboard_state::edge_count releases{};
    std::string text{};
};

/**
 * @brief Returns an onStep callback appending what each step sees of the given key (and the typed
 * text) to the given vector.
 */
std::function<void(fixed_step_updatable_spy&, step_context&)> record_steps_into(
    std::vector<step_snapshot>& snapshots, const key k) {
    return [&snapshots, k](fixed_step_updatable_spy&, step_context& context) {
        const auto keyboard = context.input().keyboard();
        snapshots.push_back(step_snapshot{.down = keyboard.is_down(k),
                                          .pressed = keyboard.was_pressed(k),
                                          .released = keyboard.was_released(k),
                                          .presses = keyboard.press_count(k),
                                          .releases = keyboard.release_count(k),
                                          .text = std::string{context.input().text()}});
    };
}

/**
 * @brief Runs one application tick on the persistent frame, the way the application does: the
 * update pass with the frame's input, then end_tick(). Events for the tick are written into the
 * frame before calling it.
 */
void run_tick(updatable& target, input_frame& frame, const delta_time delta) {
    update_context context{delta, input_view{frame}};
    target.update(context);
    frame.end_tick();
}

} // namespace

TEST_CASE("fixed_step_updatable type traits",
          "[tca][engine][rendering][tickables][fixed_step_updatable]") {
    STATIC_REQUIRE(std::is_base_of_v<updatable, fixed_step_updatable>);
    STATIC_REQUIRE(std::is_abstract_v<fixed_step_updatable>);
    STATIC_REQUIRE_FALSE(std::is_abstract_v<fixed_step_updatable_spy>);
    STATIC_REQUIRE(std::is_same_v<
                   decltype(std::declval<const fixed_step_updatable&>().get_interpolation_alpha()),
                   alpha_type>);
}

TEST_CASE("fixed_step_updatable construction",
          "[tca][engine][rendering][tickables][fixed_step_updatable]") {
    GIVEN("A default-constructed fixed-step updatable") {
        const fixed_step_updatable_spy updatableUnderTest{};

        THEN("Its step lasts one nanosecond") {
            REQUIRE(updatableUnderTest.get_step_duration() == 1ns);
        }

        THEN("It runs at most one step per frame") {
            REQUIRE(updatableUnderTest.get_max_steps_per_frame() == 1u);
        }

        THEN("No step has run or been dropped yet") {
            REQUIRE(updatableUnderTest.get_step_count() == 0u);
            REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
            REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
        }
    }

    GIVEN("A fixed-step updatable with a 20ms (50 Hz) step and at most 5 steps per frame") {
        const fixed_step_updatable_spy updatableUnderTest{step_duration{20ms},
                                                          max_steps_per_frame{5}};

        THEN("It exposes the step duration it was constructed with") {
            REQUIRE(updatableUnderTest.get_step_duration() == 20ms);
        }

        THEN("It exposes the maximum number of steps per frame it was constructed with") {
            REQUIRE(updatableUnderTest.get_max_steps_per_frame() == 5u);
        }

        THEN("No step has run yet") {
            REQUIRE(updatableUnderTest.get_step_count() == 0u);
            REQUIRE(updatableUnderTest.get_step_calls() == 0u);
        }

        THEN("No step has been dropped yet") {
            REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
        }

        THEN("The interpolation alpha is zero") {
            REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
        }
    }

    WHEN("Building a step duration of zero") {
        THEN("step_duration rejects it, as its range is open at zero") {
            REQUIRE_THROWS_AS(step_duration{0ns}, ranged_value_exception);
        }
    }

    WHEN("Building a negative step duration") {
        THEN("step_duration rejects it") {
            REQUIRE_THROWS_AS(step_duration{-20ms}, ranged_value_exception);
        }
    }

    WHEN("Building a one-nanosecond step duration") {
        THEN("step_duration accepts it") {
            REQUIRE_NOTHROW(step_duration{1ns});
        }
    }

    WHEN("Building a cap of zero steps per frame") {
        THEN("max_steps_per_frame rejects it, as such a cap would never step") {
            REQUIRE_THROWS_AS(max_steps_per_frame{0}, ranged_value_exception);
        }
    }

    WHEN("Building a cap of one step per frame") {
        THEN("max_steps_per_frame accepts it") {
            REQUIRE_NOTHROW(max_steps_per_frame{1});
        }
    }

    WHEN("Building a step count of zero") {
        THEN("step_count accepts it, as no step having run yet is a valid count") {
            REQUIRE_NOTHROW(step_count{0});
        }
    }
}

TEST_CASE("fixed_step_updatable steps on single frames",
          "[tca][engine][rendering][tickables][fixed_step_updatable]") {
    using Catch::Matchers::WithinAbs;

    GIVEN("A fixed-step updatable with a 20ms step and at most 5 steps per frame") {
        fixed_step_updatable_spy updatableUnderTest{step_duration{20ms}, max_steps_per_frame{5}};

        WHEN("Updating it with a zero delta") {
            update_with_delta(updatableUnderTest, delta_time::zero());

            THEN("No step runs and alpha stays zero") {
                REQUIRE(updatableUnderTest.get_step_calls() == 0u);
                REQUIRE(updatableUnderTest.get_step_count() == 0u);
                REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
            }
        }

        WHEN("Updating it with a negative delta") {
            update_with_delta(updatableUnderTest, 10ms);
            update_with_delta(updatableUnderTest, -5ms);

            THEN("The negative delta is ignored: no step runs and the accumulator is untouched") {
                REQUIRE(updatableUnderTest.get_step_calls() == 0u);
                REQUIRE_THAT(updatableUnderTest.get_interpolation_alpha(), WithinAbs(0.5, 1e-9));
            }
        }

        WHEN("Updating it with less than one step") {
            update_with_delta(updatableUnderTest, 5ms);

            THEN("No step runs") {
                REQUIRE(updatableUnderTest.get_step_calls() == 0u);
                REQUIRE(updatableUnderTest.get_step_count() == 0u);
            }

            THEN("The alpha is the accumulated fraction of a step") {
                REQUIRE_THAT(updatableUnderTest.get_interpolation_alpha(), WithinAbs(0.25, 1e-9));
            }

            AND_WHEN("Accumulating the rest of the step over later frames") {
                update_with_delta(updatableUnderTest, 5ms);
                update_with_delta(updatableUnderTest, 5ms);
                REQUIRE(updatableUnderTest.get_step_calls() == 0u);
                update_with_delta(updatableUnderTest, 5ms);

                THEN("Exactly one step runs once the accumulated time reaches the step") {
                    REQUIRE(updatableUnderTest.get_step_calls() == 1u);
                    REQUIRE(updatableUnderTest.get_step_count() == 1u);
                }

                THEN("The alpha is back to zero") {
                    REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
                }
            }
        }

        WHEN("Updating it with exactly one step") {
            update_with_delta(updatableUnderTest, 20ms);

            THEN("Exactly one step runs") {
                REQUIRE(updatableUnderTest.get_step_calls() == 1u);
                REQUIRE(updatableUnderTest.get_step_count() == 1u);
            }

            THEN("The alpha is zero") {
                REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
            }
        }

        WHEN("Updating it with one microsecond less than a step") {
            update_with_delta(updatableUnderTest, 20ms - 1us);

            THEN("No step runs") {
                REQUIRE(updatableUnderTest.get_step_calls() == 0u);
            }

            THEN("The alpha is just below one") {
                const alpha_type alpha = updatableUnderTest.get_interpolation_alpha();
                REQUIRE(alpha < 1.0);
                REQUIRE_THAT(alpha, WithinAbs(19'999.0 / 20'000.0, 1e-9));
            }
        }

        WHEN("Updating it with two and a half steps") {
            update_with_delta(updatableUnderTest, 50ms);

            THEN("Two steps run") {
                REQUIRE(updatableUnderTest.get_step_calls() == 2u);
                REQUIRE(updatableUnderTest.get_step_count() == 2u);
            }

            THEN("The half step left over shows up as the alpha") {
                REQUIRE_THAT(updatableUnderTest.get_interpolation_alpha(), WithinAbs(0.5, 1e-9));
            }

            THEN("Nothing is dropped") {
                REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
            }

            AND_WHEN("Another half step arrives") {
                update_with_delta(updatableUnderTest, 10ms);

                THEN("The carried-over half completes a third step") {
                    REQUIRE(updatableUnderTest.get_step_count() == 3u);
                    REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
                }
            }
        }

        WHEN("Updating it with exactly the maximum number of steps") {
            update_with_delta(updatableUnderTest, 100ms);

            THEN("All of them run and none is dropped") {
                REQUIRE(updatableUnderTest.get_step_count() == 5u);
                REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
                REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
            }
        }
    }

    GIVEN("A fixed-step updatable whose step() inspects its own state") {
        std::vector<step_count> stepCountsSeenInsideStep{};
        fixed_step_updatable_spy updatableUnderTest{
            step_duration{20ms}, max_steps_per_frame{5},
            [&stepCountsSeenInsideStep](fixed_step_updatable_spy& self, step_context&) {
                stepCountsSeenInsideStep.push_back(self.get_step_count());
            }};

        WHEN("A frame runs three steps") {
            update_with_delta(updatableUnderTest, 60ms);

            THEN("get_step_count() inside step() counts the step currently running") {
                REQUIRE(stepCountsSeenInsideStep == std::vector<step_count>{1, 2, 3});
            }
        }
    }
}

TEST_CASE("fixed_step_updatable caps the steps run in a single frame",
          "[tca][engine][rendering][tickables][fixed_step_updatable]") {
    using Catch::Matchers::WithinAbs;

    GIVEN("A fixed-step updatable with a 20ms step and at most 5 steps per frame") {
        fixed_step_updatable_spy updatableUnderTest{step_duration{20ms}, max_steps_per_frame{5}};

        WHEN("A single, very large delta arrives (a 3 second stall plus a quarter step)") {
            update_with_delta(updatableUnderTest, 3s + 5ms);

            THEN("Only the maximum number of steps runs") {
                REQUIRE(updatableUnderTest.get_step_calls() == 5u);
                REQUIRE(updatableUnderTest.get_step_count() == 5u);
            }

            THEN("The rest of the whole steps is recorded as dropped") {
                // 3s / 20ms = 150 whole steps, 5 of which ran.
                REQUIRE(updatableUnderTest.get_dropped_steps() == 145u);
            }

            THEN("Only the sub-step remainder is carried over, so alpha stays in [0, 1)") {
                REQUIRE_THAT(updatableUnderTest.get_interpolation_alpha(), WithinAbs(0.25, 1e-9));
            }

            AND_WHEN("The next frame is a normal one-step frame") {
                update_with_delta(updatableUnderTest, 20ms);

                THEN("There is no burst of catch-up steps: exactly one more step runs") {
                    REQUIRE(updatableUnderTest.get_step_count() == 6u);
                    REQUIRE(updatableUnderTest.get_dropped_steps() == 145u);
                }
            }
        }

        WHEN("A delta of exactly one step more than the cap arrives") {
            update_with_delta(updatableUnderTest, 120ms);

            THEN("The cap runs and exactly one step is dropped") {
                REQUIRE(updatableUnderTest.get_step_count() == 5u);
                REQUIRE(updatableUnderTest.get_dropped_steps() == 1u);
                REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
            }
        }
    }

    GIVEN("A 50 Hz fixed-step updatable allowed at most 2 steps per frame") {
        fixed_step_updatable_spy updatableUnderTest{step_duration{20ms}, max_steps_per_frame{2}};

        WHEN("The game runs at 20 fps for one second, below step_rate / max_steps = 25 fps") {
            feed(updatableUnderTest, display_frame_deltas(20));

            THEN("The simulation slows down instead of skipping: every frame runs the cap") {
                REQUIRE(updatableUnderTest.get_step_count() == 40u);
            }

            THEN("The steps it couldn't run are accounted for as dropped") {
                REQUIRE(updatableUnderTest.get_dropped_steps() == 10u);
            }
        }

        WHEN("The game runs at exactly 25 fps for one second") {
            feed(updatableUnderTest, display_frame_deltas(25));

            THEN("It keeps up without dropping anything") {
                REQUIRE(updatableUnderTest.get_step_count() == 50u);
                REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
            }
        }
    }
}

TEST_CASE("fixed_step_updatable runs the expected number of steps at any display rate",
          "[tca][engine][rendering][tickables][fixed_step_updatable]") {
    const std::int64_t stepRateHz = GENERATE(50, 60, 100, 125);
    const std::int64_t displayRateHz = GENERATE(30, 50, 60, 144, 165, 240);

    // 60 Hz doesn't divide one second in whole nanoseconds (16'666'666ns), but a second still holds
    // exactly 60 of them, with 40ns left over.
    const step_duration stepDuration = step_duration_for(stepRateHz);

    GIVEN("A fixed-step updatable at " + std::to_string(stepRateHz) + " Hz") {
        fixed_step_updatable_spy updatableUnderTest{stepDuration, max_steps_per_frame{8}};

        WHEN("Feeding it one second of " + std::to_string(displayRateHz) + " Hz frames") {
            feed(updatableUnderTest, display_frame_deltas(displayRateHz));

            THEN("It runs exactly step_rate steps") {
                REQUIRE(updatableUnderTest.get_step_count() ==
                        static_cast<std::size_t>(stepRateHz));
                REQUIRE(updatableUnderTest.get_step_calls() ==
                        static_cast<std::size_t>(stepRateHz));
                REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
            }
        }
    }
}

TEST_CASE("fixed_step_updatable runs the expected number of steps with jittered frame deltas",
          "[tca][engine][rendering][tickables][fixed_step_updatable]") {
    const std::uint32_t seed = GENERATE(1u, 42u, 1337u, 0xC0FFEEu);

    GIVEN("A 50 Hz fixed-step updatable") {
        fixed_step_updatable_spy updatableUnderTest{step_duration_for(50), max_steps_per_frame{8}};

        WHEN("Feeding it one second of frames jittering between 1ms and 45ms (seed " +
             std::to_string(seed) + ")") {
            const auto deltas = jittered_frame_deltas(ONE_SECOND, 1ms, 45ms, seed);

            alpha_type minAlpha{1.0};
            alpha_type maxAlpha{0.0};
            for (const auto delta : deltas) {
                update_with_delta(updatableUnderTest, delta);
                const alpha_type alpha = updatableUnderTest.get_interpolation_alpha();
                minAlpha = std::min(minAlpha, alpha);
                maxAlpha = std::max(maxAlpha, alpha);
            }

            THEN("It runs exactly 50 steps") {
                REQUIRE(updatableUnderTest.get_step_count() == 50u);
                REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
            }

            THEN("The alpha stays in [0, 1) after every frame") {
                REQUIRE(minAlpha >= 0.0);
                REQUIRE(maxAlpha < 1.0);
            }

            THEN("The alpha is zero at the end, since one second is an exact multiple of 20ms") {
                REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
            }
        }
    }

    GIVEN("A 50 Hz fixed-step updatable allowed at most 3 steps per frame") {
        fixed_step_updatable_spy updatableUnderTest{step_duration_for(50), max_steps_per_frame{3}};

        WHEN("Feeding it frames that sometimes stall far beyond the cap (seed " +
             std::to_string(seed) + ")") {
            const auto deltas = jittered_frame_deltas(10s, 1ms, 250ms, seed);
            feed(updatableUnderTest, deltas);

            THEN("Every whole step of elapsed time is either run or dropped, never lost") {
                // 10s / 20ms = 500 whole steps.
                REQUIRE(updatableUnderTest.get_step_count() +
                            updatableUnderTest.get_dropped_steps() ==
                        500u);
            }

            THEN("Some steps were dropped") {
                REQUIRE(updatableUnderTest.get_dropped_steps() > 0u);
            }
        }
    }
}

TEST_CASE("fixed_step_updatable doesn't drift over a long session",
          "[tca][engine][rendering][tickables][fixed_step_updatable][long]") {
    using Catch::Matchers::WithinAbs;

    const std::int64_t stepRateHz = GENERATE(50, 60, 100, 125);
    const std::int64_t displayRateHz = GENERATE(144, 165, 240);

    GIVEN("A fixed-step updatable at " + std::to_string(stepRateHz) + " Hz") {
        const step_duration stepDuration = step_duration_for(stepRateHz);
        fixed_step_updatable_spy updatableUnderTest{stepDuration, max_steps_per_frame{8}};

        WHEN("Feeding it one hour of " + std::to_string(displayRateHz) + " Hz frames") {
            constexpr std::int64_t ONE_HOUR_SECONDS = 3600;
            feed(updatableUnderTest, display_frame_deltas(displayRateHz, ONE_HOUR_SECONDS));

            THEN("It runs exactly 3600 * step_rate steps") {
                // Holds for 60 Hz too: the 40ns a second leaves over add up to 144us in an hour,
                // far short of another 16.7ms step.
                REQUIRE(updatableUnderTest.get_step_count() ==
                        static_cast<std::size_t>(ONE_HOUR_SECONDS * stepRateHz));
                REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
            }

            THEN("The alpha is whatever is left of the hour past the last whole step") {
                // Zero for the rates dividing a second exactly; 144us / 16.7ms for 60 Hz.
                const std::chrono::nanoseconds oneHour{std::chrono::hours{1}};
                const auto leftOver = oneHour % stepDuration.get();
                const double expectedAlpha = static_cast<double>(leftOver.count()) /
                                             static_cast<double>(stepDuration.get().count());
                REQUIRE_THAT(updatableUnderTest.get_interpolation_alpha(),
                             WithinAbs(expectedAlpha, 1e-12));
            }
        }
    }
}

TEST_CASE("fixed_step_updatable inside a pausable_updatables_view",
          "[tca][engine][rendering][tickables][fixed_step_updatable][pausable_updatables_view]") {
    using Catch::Matchers::WithinAbs;
    using tca::engine::rendering::pausable_updatables_view;

    GIVEN("A 50 Hz fixed-step updatable added to a pausable updatables view") {
        pausable_updatables_view view{};
        fixed_step_updatable_spy updatableUnderTest{step_duration{20ms}, max_steps_per_frame{5}};
        view.add_updatable(updatableUnderTest);

        WHEN("Updating the view") {
            update_with_delta(view, 40ms);

            THEN("The fixed-step updatable steps") {
                REQUIRE(updatableUnderTest.get_step_count() == 2u);
            }
        }

        WHEN("A half step has accumulated and the updatable is then paused") {
            update_with_delta(view, 10ms);
            view.pause_updatable(updatableUnderTest);

            AND_WHEN("Ten seconds of frames go by while paused") {
                for (int frame = 0; frame < 600; ++frame) {
                    update_with_delta(view, delta_time{16'667});
                }

                THEN("It doesn't step") {
                    REQUIRE(updatableUnderTest.get_step_calls() == 0u);
                    REQUIRE(updatableUnderTest.get_step_count() == 0u);
                }

                THEN("Nothing is recorded as dropped") {
                    REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
                }

                THEN("The alpha is frozen where it was when paused") {
                    REQUIRE_THAT(updatableUnderTest.get_interpolation_alpha(),
                                 WithinAbs(0.5, 1e-9));
                }

                AND_WHEN("It is resumed and a zero-length frame arrives") {
                    view.resume_updatable(updatableUnderTest);
                    update_with_delta(view, delta_time::zero());

                    THEN("The paused time isn't replayed: no step runs") {
                        REQUIRE(updatableUnderTest.get_step_count() == 0u);
                        REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
                    }
                }

                AND_WHEN("It is resumed and the other half of the step arrives") {
                    view.resume_updatable(updatableUnderTest);
                    update_with_delta(view, 10ms);

                    THEN("Exactly one step runs, completing the half step from before the pause") {
                        REQUIRE(updatableUnderTest.get_step_count() == 1u);
                        REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
                        REQUIRE(updatableUnderTest.get_interpolation_alpha() == 0.0);
                    }
                }

                AND_WHEN("It is resumed and one second of 60 Hz-ish frames goes by") {
                    view.resume_updatable(updatableUnderTest);
                    feed(view, display_frame_deltas(60));

                    THEN("It steps for that second (plus the half step it had) and nothing more") {
                        // 10ms carried + 1s = 1.01s -> 50 steps, 10ms left over.
                        REQUIRE(updatableUnderTest.get_step_count() == 50u);
                        REQUIRE(updatableUnderTest.get_dropped_steps() == 0u);
                        REQUIRE_THAT(updatableUnderTest.get_interpolation_alpha(),
                                     WithinAbs(0.5, 1e-9));
                    }
                }
            }
        }
    }
}

TEST_CASE("step_context type traits", "[tca][engine][rendering][tickables][step_context]") {
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<step_context>);
    STATIC_REQUIRE(std::is_nothrow_constructible_v<step_context, input_view>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<input_view, step_context>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(std::declval<const step_context&>().input()), input_view>);
}

TEST_CASE("step_context forwards to the frame it was given",
          "[tca][engine][rendering][tickables][step_context]") {
    GIVEN("A frame with space pressed and some text typed, and a step context over it") {
        input_frame frame{};
        frame.keyboard().press(key::space);
        frame.append_text("a");
        const step_context context{input_view{frame}};

        THEN("The context's input reads the frame") {
            REQUIRE(context.input().keyboard().was_pressed(key::space));
            REQUIRE(context.input().text() == "a");
        }

        WHEN("Consuming space through the context") {
            context.input().consume(key::space);

            THEN("The frame no longer shows it") {
                REQUIRE_FALSE(frame.keyboard().was_pressed(key::space));
                REQUIRE_FALSE(frame.keyboard().is_down(key::space));
            }
        }
    }
}

TEST_CASE("fixed_step_updatable reports each edge to exactly one step",
          "[tca][engine][rendering][tickables][fixed_step_updatable][step_context]") {
    GIVEN(
        "A 50 Hz fixed-step updatable (at most 5 steps per frame) recording what its steps see "
        "of space, fed from a persistent frame") {
        std::vector<step_snapshot> steps{};
        fixed_step_updatable_spy updatableUnderTest{step_duration{20ms}, max_steps_per_frame{5},
                                                    record_steps_into(steps, key::space)};
        input_frame frame{};

        WHEN("No input arrives over frames running steps") {
            run_tick(updatableUnderTest, frame, 20ms);
            run_tick(updatableUnderTest, frame, 40ms);

            THEN("No step sees space") {
                REQUIRE(steps.size() == 3u);
                for (const auto& seen : steps) {
                    REQUIRE_FALSE(seen.down);
                    REQUIRE_FALSE(seen.pressed);
                    REQUIRE_FALSE(seen.released);
                    REQUIRE(seen.text.empty());
                }
            }
        }

        WHEN("Space is pressed in a frame running one step") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, 20ms);

            THEN("The step sees the press") {
                REQUIRE(steps.size() == 1u);
                REQUIRE(steps[0].down);
                REQUIRE(steps[0].pressed);
                REQUIRE(steps[0].presses == 1u);
            }

            AND_WHEN("Space is held through the next frame running one step") {
                run_tick(updatableUnderTest, frame, 20ms);

                THEN("That step sees it down, without the press") {
                    REQUIRE(steps.size() == 2u);
                    REQUIRE(steps[1].down);
                    REQUIRE_FALSE(steps[1].pressed);
                    REQUIRE(steps[1].presses == 0u);
                }
            }

            AND_WHEN("Frames running no step go by, then a frame runs one step") {
                run_tick(updatableUnderTest, frame, 5ms);
                run_tick(updatableUnderTest, frame, 5ms);
                run_tick(updatableUnderTest, frame, 10ms);

                THEN(
                    "The press already seen isn't reported again: the edges were cleared once a "
                    "step had seen them") {
                    REQUIRE(steps.size() == 2u);
                    REQUIRE(steps[1].down);
                    REQUIRE_FALSE(steps[1].pressed);
                }
            }
        }

        WHEN("Space is pressed in a frame running no step, then the next frame runs one step") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, 5ms);
            REQUIRE(steps.empty());
            run_tick(updatableUnderTest, frame, 15ms);

            THEN("The step sees the press, once") {
                REQUIRE(steps.size() == 1u);
                REQUIRE(steps[0].down);
                REQUIRE(steps[0].pressed);
                REQUIRE(steps[0].presses == 1u);
            }
        }

        WHEN("Space is pressed in a frame running two steps") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, 40ms);

            THEN("The first step sees the press") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(steps[0].down);
                REQUIRE(steps[0].pressed);
                REQUIRE(steps[0].presses == 1u);
            }

            THEN("The second step sees space held, without the press") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(steps[1].down);
                REQUIRE_FALSE(steps[1].pressed);
                REQUIRE(steps[1].presses == 0u);
            }
        }

        WHEN("Space is pressed in a frame whose delta is beyond the cap") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, 3s);

            THEN("Only the first of the steps run sees the press") {
                REQUIRE(steps.size() == 5u);
                REQUIRE(steps[0].pressed);
                for (std::size_t i = 1; i < steps.size(); ++i) {
                    REQUIRE(steps[i].down);
                    REQUIRE_FALSE(steps[i].pressed);
                }
            }
        }

        WHEN(
            "Space is tapped (pressed and released) in a frame running no step, then the next "
            "frame runs one step") {
            frame.keyboard().press(key::space);
            frame.keyboard().release(key::space);
            run_tick(updatableUnderTest, frame, 5ms);
            run_tick(updatableUnderTest, frame, 15ms);

            THEN("The tap between steps is still reported: pressed and released, not down") {
                REQUIRE(steps.size() == 1u);
                REQUIRE_FALSE(steps[0].down);
                REQUIRE(steps[0].pressed);
                REQUIRE(steps[0].released);
                REQUIRE(steps[0].presses == 1u);
                REQUIRE(steps[0].releases == 1u);
            }
        }

        WHEN(
            "Space is pressed in one frame running no step and released in the next, also "
            "running no step, then a frame runs one step") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, 5ms);
            frame.keyboard().release(key::space);
            run_tick(updatableUnderTest, frame, 5ms);
            run_tick(updatableUnderTest, frame, 10ms);

            THEN("The step sees both edges and the key up") {
                REQUIRE(steps.size() == 1u);
                REQUIRE_FALSE(steps[0].down);
                REQUIRE(steps[0].pressed);
                REQUIRE(steps[0].released);
            }
        }

        WHEN("Space is tapped in each of two frames running no step, then a frame runs one step") {
            frame.keyboard().press(key::space);
            frame.keyboard().release(key::space);
            run_tick(updatableUnderTest, frame, 5ms);
            frame.keyboard().press(key::space);
            frame.keyboard().release(key::space);
            run_tick(updatableUnderTest, frame, 5ms);
            run_tick(updatableUnderTest, frame, 10ms);

            THEN("The step sees the edges of both frames added up") {
                REQUIRE(steps.size() == 1u);
                REQUIRE(steps[0].presses == 2u);
                REQUIRE(steps[0].releases == 2u);
                REQUIRE_FALSE(steps[0].down);
            }
        }

        WHEN(
            "Space is held over steps, then released in a frame running no step, then a frame "
            "runs one step") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, 20ms);
            frame.keyboard().release(key::space);
            run_tick(updatableUnderTest, frame, 5ms);
            run_tick(updatableUnderTest, frame, 15ms);

            THEN("The step after the release sees it, and the key up") {
                REQUIRE(steps.size() == 2u);
                REQUIRE_FALSE(steps[1].down);
                REQUIRE_FALSE(steps[1].pressed);
                REQUIRE(steps[1].released);
                REQUIRE(steps[1].releases == 1u);
            }
        }

        WHEN("Space is pressed in a frame with a negative delta, then a frame runs one step") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, -5ms);
            run_tick(updatableUnderTest, frame, 20ms);

            THEN("The press isn't lost with the ignored delta: the step sees it") {
                REQUIRE(steps.size() == 1u);
                REQUIRE(steps[0].down);
                REQUIRE(steps[0].pressed);
            }
        }
    }
}

TEST_CASE("fixed_step_updatable delivers the typed text to exactly one step",
          "[tca][engine][rendering][tickables][fixed_step_updatable][step_context]") {
    GIVEN(
        "A 50 Hz fixed-step updatable recording the text its steps see, fed from a persistent "
        "frame") {
        std::vector<step_snapshot> steps{};
        fixed_step_updatable_spy updatableUnderTest{step_duration{20ms}, max_steps_per_frame{5},
                                                    record_steps_into(steps, key::space)};
        input_frame frame{};

        WHEN("Text is typed in a frame running no step, then more in a frame running two steps") {
            frame.append_text("h");
            run_tick(updatableUnderTest, frame, 5ms);
            frame.append_text("\xC3\xA9"); // U+00E9
            run_tick(updatableUnderTest, frame, 35ms);

            THEN("The first step sees all of it, older first") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(steps[0].text == "h\xC3\xA9");
            }

            THEN("The second step sees none of it") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(steps[1].text.empty());
            }
        }

        WHEN("Text is typed in a frame running one step, then a later frame runs one step") {
            frame.append_text("a");
            run_tick(updatableUnderTest, frame, 20ms);
            run_tick(updatableUnderTest, frame, 20ms);

            THEN("Only the first step sees it") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(steps[0].text == "a");
                REQUIRE(steps[1].text.empty());
            }
        }
    }
}

namespace {

/**
 * @brief What one step saw of the left mouse button, the pointer and the wheel.
 */
struct mouse_step_snapshot {
    bool down{};
    bool pressed{};
    bool released{};
    mouse_state::edge_count presses{};
    mouse_state::edge_count releases{};
    std::optional<pointer_position> pointer{};
    wheel_delta wheel{};
};

/**
 * @brief Returns an onStep callback appending what each step sees of the mouse to the given vector.
 */
std::function<void(fixed_step_updatable_spy&, step_context&)> record_mouse_steps_into(
    std::vector<mouse_step_snapshot>& snapshots) {
    return [&snapshots](fixed_step_updatable_spy&, step_context& context) {
        const auto mouse = context.input().mouse();
        snapshots.push_back(mouse_step_snapshot{.down = mouse.is_down(mouse_button::left),
                                                .pressed = mouse.was_pressed(mouse_button::left),
                                                .released = mouse.was_released(mouse_button::left),
                                                .presses = mouse.press_count(mouse_button::left),
                                                .releases = mouse.release_count(mouse_button::left),
                                                .pointer = mouse.pointer(),
                                                .wheel = mouse.wheel()});
    };
}

} // namespace

TEST_CASE("fixed_step_updatable delivers the mouse to its steps",
          "[tca][engine][rendering][tickables][fixed_step_updatable][step_context][mouse]") {
    GIVEN(
        "A 50 Hz fixed-step updatable (at most 5 steps per frame) recording what its steps see "
        "of the mouse, fed from a persistent frame") {
        std::vector<mouse_step_snapshot> steps{};
        fixed_step_updatable_spy updatableUnderTest{step_duration{20ms}, max_steps_per_frame{5},
                                                    record_mouse_steps_into(steps)};
        input_frame frame{};

        WHEN("The left button is clicked in a frame running no step, then a frame runs one step") {
            frame.mouse().button_down(mouse_button::left);
            frame.mouse().button_up(mouse_button::left);
            run_tick(updatableUnderTest, frame, 5ms);
            REQUIRE(steps.empty());
            run_tick(updatableUnderTest, frame, 15ms);

            THEN("The click between steps is reported once: pressed and released, not down") {
                REQUIRE(steps.size() == 1u);
                REQUIRE_FALSE(steps[0].down);
                REQUIRE(steps[0].pressed);
                REQUIRE(steps[0].released);
                REQUIRE(steps[0].presses == 1u);
                REQUIRE(steps[0].releases == 1u);
            }

            AND_WHEN("A later frame runs one more step") {
                run_tick(updatableUnderTest, frame, 20ms);

                THEN("That step doesn't see the click again") {
                    REQUIRE(steps.size() == 2u);
                    REQUIRE_FALSE(steps[1].pressed);
                    REQUIRE_FALSE(steps[1].released);
                }
            }
        }

        WHEN("The left button is pressed in a frame running two steps") {
            frame.mouse().button_down(mouse_button::left);
            run_tick(updatableUnderTest, frame, 40ms);

            THEN("The first step sees the press, the second the button held without it") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(steps[0].pressed);
                REQUIRE(steps[0].down);
                REQUIRE(steps[1].down);
                REQUIRE_FALSE(steps[1].pressed);
            }
        }

        WHEN("The wheel turns in a frame running no step and again in a frame running two steps") {
            frame.mouse().scroll({.vertical = 1.0F});
            run_tick(updatableUnderTest, frame, 5ms);
            frame.mouse().scroll({.vertical = 0.5F, .horizontal = -1.0F});
            run_tick(updatableUnderTest, frame, 35ms);

            THEN("The first step sees both turns added up, the second sees none") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(steps[0].wheel == wheel_delta{.vertical = 1.5F, .horizontal = -1.0F});
                REQUIRE(steps[1].wheel == wheel_delta{});
            }
        }

        WHEN("The pointer moves in frames running no step, then a frame runs two steps") {
            frame.mouse().move_pointer({.x = 10, .y = 20});
            run_tick(updatableUnderTest, frame, 5ms);
            frame.mouse().move_pointer({.x = 30, .y = 40});
            run_tick(updatableUnderTest, frame, 5ms);
            run_tick(updatableUnderTest, frame, 30ms);

            THEN("Every step sees the latest position: the pointer is a level, not an edge") {
                REQUIRE(steps.size() == 2u);
                for (const auto& seen : steps) {
                    REQUIRE(seen.pointer == pointer_position{.x = 30, .y = 40});
                }
            }
        }
    }
}

namespace {

/**
 * @brief Records whether space was pressed and what text was typed, as seen by update().
 */
class input_reader final : public updatable {
public:
    void update(update_context& context) override {
        m_sawPress = context.input().keyboard().was_pressed(key::space);
        m_text = std::string{context.input().text()};
    }

    [[nodiscard]] bool saw_press() const noexcept {
        return m_sawPress;
    }

    [[nodiscard]] const std::string& text() const noexcept {
        return m_text;
    }

private:
    bool m_sawPress{};
    std::string m_text{};
};

} // namespace

TEST_CASE("fixed_step_updatable leaves the frame's input to the updatables after it",
          "[tca][engine][rendering][tickables][fixed_step_updatable][step_context]") {
    using tca::engine::rendering::updatable_view;

    GIVEN(
        "A view holding a fixed-step updatable whose steps only read input, then an updatable "
        "reading the frame") {
        std::vector<step_snapshot> steps{};
        fixed_step_updatable_spy fixedStep{step_duration{20ms}, max_steps_per_frame{5},
                                           record_steps_into(steps, key::space)};
        input_reader reader{};
        updatable_view view{};
        view.add_updatable(fixedStep);
        view.add_updatable(reader);
        input_frame frame{};

        WHEN("Space is pressed and text typed in a frame running two steps") {
            frame.keyboard().press(key::space);
            frame.append_text("a");
            run_tick(view, frame, 40ms);

            THEN("The updatable after the fixed-step one still sees the press and the text") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(reader.saw_press());
                REQUIRE(reader.text() == "a");
            }
        }

        WHEN("Space is pressed and text typed in a frame running no step") {
            frame.keyboard().press(key::space);
            frame.append_text("a");
            run_tick(view, frame, 5ms);

            THEN("The updatable after the fixed-step one still sees the press and the text") {
                REQUIRE(steps.empty());
                REQUIRE(reader.saw_press());
                REQUIRE(reader.text() == "a");
            }
        }
    }
}

TEST_CASE("fixed_step_updatable keeps a held key a step consumed hidden until pressed again",
          "[tca][engine][rendering][tickables][fixed_step_updatable][step_context]") {
    GIVEN(
        "A 50 Hz fixed-step updatable whose steps consume space whenever they see it pressed, "
        "fed from a persistent frame") {
        std::vector<step_snapshot> steps{};
        const auto record{record_steps_into(steps, key::space)};
        fixed_step_updatable_spy updatableUnderTest{
            step_duration{20ms}, max_steps_per_frame{5},
            [&record](fixed_step_updatable_spy& self, step_context& context) {
                record(self, context);
                if (context.input().keyboard().was_pressed(key::space)) {
                    context.input().consume(key::space);
                }
            }};
        input_frame frame{};

        WHEN("Space is pressed in a frame running two steps") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, 40ms);

            THEN("The second step of that frame doesn't see it held") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(steps[0].pressed);
                REQUIRE_FALSE(steps[1].down);
                REQUIRE_FALSE(steps[1].pressed);
            }
        }

        WHEN("Space is pressed in a frame running one step, then held over later steps") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, 20ms);
            run_tick(updatableUnderTest, frame, 20ms);
            run_tick(updatableUnderTest, frame, 5ms);
            run_tick(updatableUnderTest, frame, 15ms);

            THEN("Later steps don't see it held") {
                REQUIRE(steps.size() == 3u);
                REQUIRE(steps[0].pressed);
                for (std::size_t i = 1; i < steps.size(); ++i) {
                    REQUIRE_FALSE(steps[i].down);
                    REQUIRE_FALSE(steps[i].pressed);
                }
            }

            THEN("The shared frame still has it held for everyone else") {
                REQUIRE(frame.keyboard().is_down(key::space));
            }

            AND_WHEN("It is released") {
                frame.keyboard().release(key::space);
                run_tick(updatableUnderTest, frame, 20ms);

                THEN("The release isn't reported to the steps") {
                    REQUIRE(steps.size() == 4u);
                    REQUIRE_FALSE(steps[3].released);
                    REQUIRE_FALSE(steps[3].down);
                }

                AND_WHEN("It is pressed again") {
                    frame.keyboard().press(key::space);
                    run_tick(updatableUnderTest, frame, 20ms);

                    THEN("The new press is reported") {
                        REQUIRE(steps.size() == 5u);
                        REQUIRE(steps[4].pressed);
                        REQUIRE(steps[4].down);
                        REQUIRE(steps[4].presses == 1u);
                    }
                }
            }

            AND_WHEN("It is released and pressed again within a frame running no step") {
                frame.keyboard().release(key::space);
                frame.keyboard().press(key::space);
                run_tick(updatableUnderTest, frame, 5ms);
                run_tick(updatableUnderTest, frame, 15ms);

                THEN("The next step sees the new press, and the release that came with it") {
                    REQUIRE(steps.size() == 4u);
                    REQUIRE(steps[3].pressed);
                    REQUIRE(steps[3].down);
                    REQUIRE(steps[3].presses == 1u);
                    REQUIRE(steps[3].releases == 1u);
                }
            }
        }
    }
}

TEST_CASE("fixed_step_updatable sees a key held before its first update as up until pressed",
          "[tca][engine][rendering][tickables][fixed_step_updatable][step_context]") {
    GIVEN(
        "A persistent frame where space was pressed before a fixed-step updatable's first "
        "update") {
        input_frame frame{};
        frame.keyboard().press(key::space);
        frame.end_tick();

        std::vector<step_snapshot> steps{};
        fixed_step_updatable_spy updatableUnderTest{step_duration{20ms}, max_steps_per_frame{5},
                                                    record_steps_into(steps, key::space)};

        WHEN("Space stays held over steps") {
            run_tick(updatableUnderTest, frame, 20ms);
            run_tick(updatableUnderTest, frame, 20ms);

            THEN("The steps see it up") {
                REQUIRE(steps.size() == 2u);
                for (const auto& seen : steps) {
                    REQUIRE_FALSE(seen.down);
                    REQUIRE_FALSE(seen.pressed);
                }
            }

            AND_WHEN("It is released and pressed again") {
                frame.keyboard().release(key::space);
                run_tick(updatableUnderTest, frame, 20ms);
                frame.keyboard().press(key::space);
                run_tick(updatableUnderTest, frame, 20ms);

                THEN("The release isn't reported, the new press is") {
                    REQUIRE(steps.size() == 4u);
                    REQUIRE_FALSE(steps[2].released);
                    REQUIRE(steps[3].pressed);
                    REQUIRE(steps[3].down);
                }
            }
        }
    }
}

TEST_CASE("fixed_step_updatable steps don't see what was consumed in the shared frame",
          "[tca][engine][rendering][tickables][fixed_step_updatable][step_context]") {
    GIVEN("A 50 Hz fixed-step updatable fed from a persistent frame") {
        std::vector<step_snapshot> steps{};
        fixed_step_updatable_spy updatableUnderTest{step_duration{20ms}, max_steps_per_frame{5},
                                                    record_steps_into(steps, key::space)};
        input_frame frame{};

        WHEN(
            "Space is pressed and text typed, then everything is consumed before the update "
            "(e.g. ImGui capturing the keyboard)") {
            frame.keyboard().press(key::space);
            frame.append_text("a");
            frame.consume_all();
            run_tick(updatableUnderTest, frame, 20ms);

            THEN("The step sees neither") {
                REQUIRE(steps.size() == 1u);
                REQUIRE_FALSE(steps[0].down);
                REQUIRE_FALSE(steps[0].pressed);
                REQUIRE(steps[0].text.empty());
            }
        }

        WHEN("Space is held over steps, then consumed in the shared frame") {
            frame.keyboard().press(key::space);
            run_tick(updatableUnderTest, frame, 20ms);
            frame.consume(key::space);
            run_tick(updatableUnderTest, frame, 20ms);

            THEN("Steps after the consumption see it up, without a release") {
                REQUIRE(steps.size() == 2u);
                REQUIRE(steps[0].down);
                REQUIRE_FALSE(steps[1].down);
                REQUIRE_FALSE(steps[1].released);
            }
        }
    }
}
