// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/fsm.hpp"

// C++ STL
#include <memory>

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace {

struct event_toggle {};

class light_bulb;

using light_bulb_fsm = tca::engine::core::fsm<light_bulb, event_toggle>;

// Only for testing purposes.
enum class light_bulb_state {
    off,
    on,
};

// Only for testing purposes.
struct test_data {
    light_bulb_state state{};
    bool on_enter_called{};
    bool on_exit_called{};
    bool toggle_event_called{};
};

/**
 * @brief A simple light bulb class to test the FSM implementation. It has two states: on and off.
 * Toggling the light bulb triggers a state transition. The test_data struct is used to verify that
 * the on_enter and on_exit events are properly called during state transitions.
 */
class light_bulb {
public:
    explicit light_bulb(test_data& prevData, test_data& currentData)
        : m_previousData{prevData},
          m_currentData{currentData},
          m_fsm{this, &light_bulb::state_off} {}

    void toggle() {
        m_fsm.feed(event_toggle{});
    }

private:
    auto state_off(const light_bulb_fsm::event_t& event) -> light_bulb_fsm::state {
        return std::visit(
            tca::engine::core::overloaded{
                [this](tca::engine::core::base_fsm_events::on_enter) -> light_bulb_fsm::state {
                    m_currentData = test_data{
                        .state = light_bulb_state::off,
                        .on_enter_called = true,
                    };
                    return light_bulb_fsm::stay;
                },
                [this](tca::engine::core::base_fsm_events::on_exit) -> light_bulb_fsm::state {
                    m_previousData.on_exit_called = true;
                    return light_bulb_fsm::stay;
                },
                [this](event_toggle) -> light_bulb_fsm::state {
                    m_previousData = test_data{
                        .state = light_bulb_state::off,
                        .toggle_event_called = true,
                    };
                    return &light_bulb::state_on;
                },
            },
            event);
    }

    auto state_on(const light_bulb_fsm::event_t& event) -> light_bulb_fsm::state {
        return std::visit(
            tca::engine::core::overloaded{
                [this](tca::engine::core::base_fsm_events::on_enter) -> light_bulb_fsm::state {
                    m_currentData = test_data{
                        .state = light_bulb_state::on,
                        .on_enter_called = true,
                    };
                    return light_bulb_fsm::stay;
                },
                [this](tca::engine::core::base_fsm_events::on_exit) -> light_bulb_fsm::state {
                    m_previousData.on_exit_called = true;
                    return light_bulb_fsm::stay;
                },
                [this](event_toggle) -> light_bulb_fsm::state {
                    m_previousData = test_data{
                        .state = light_bulb_state::on,
                        .toggle_event_called = true,
                    };
                    return &light_bulb::state_off;
                },
            },
            event);
    }

    test_data& m_previousData;
    test_data& m_currentData;
    light_bulb_fsm m_fsm;
};

} // namespace

TEST_CASE("fsm static tests", "[tca][engine][core][fsm]") {
    using namespace tca::engine::core;

    STATIC_CHECK(light_bulb_fsm::stay == nullptr);
    STATIC_CHECK(light_bulb_fsm::state{} == light_bulb_fsm::state{light_bulb_fsm::stay});
    STATIC_CHECK(light_bulb_fsm::state{}.is_null());
}

TEST_CASE("fsm class tests - light bulb scenario", "[tca][engine][core][fsm]") {
    using namespace tca::engine::core;

    SECTION("fsm constructor") {
        std::unique_ptr<light_bulb> lightBulb{};

        WHEN("Constructing a light bulb with an initial state") {
            test_data previousData{};
            test_data currentData{};

            REQUIRE_NOTHROW(lightBulb = std::make_unique<light_bulb>(previousData, currentData));

            THEN("The on_enter event of the initial state is properly called") {
                REQUIRE(currentData.state == light_bulb_state::off);
                REQUIRE(currentData.on_enter_called);
                REQUIRE_FALSE(currentData.on_exit_called);
                REQUIRE_FALSE(currentData.toggle_event_called);
            }
        }
    }

    SECTION("fsm state transitions") {
        GIVEN("A light bulb in the off state") {
            std::unique_ptr<light_bulb> lightBulb{};

            test_data previousData{};
            test_data currentData{};

            REQUIRE_NOTHROW(lightBulb = std::make_unique<light_bulb>(previousData, currentData));

            WHEN("Toggling the light bulb") {
                REQUIRE_NOTHROW(lightBulb->toggle());

                THEN("The on_exit event of the previous state is properly called") {
                    REQUIRE(previousData.state == light_bulb_state::off);
                    REQUIRE_FALSE(previousData.on_enter_called);
                    REQUIRE(previousData.on_exit_called);
                    REQUIRE(previousData.toggle_event_called);
                }

                THEN("The on_enter event of the new state is properly called") {
                    REQUIRE(currentData.state == light_bulb_state::on);
                    REQUIRE(currentData.on_enter_called);
                    REQUIRE_FALSE(currentData.on_exit_called);
                    REQUIRE_FALSE(currentData.toggle_event_called);
                }
            }
        }

        GIVEN("A light bulb in the on state") {
            std::unique_ptr<light_bulb> lightBulb{};

            test_data previousData{};
            test_data currentData{};

            REQUIRE_NOTHROW(lightBulb = std::make_unique<light_bulb>(previousData, currentData));
            // Go to the on state.
            lightBulb->toggle();

            WHEN("Toggling the light bulb") {
                REQUIRE_NOTHROW(lightBulb->toggle());

                THEN("The on_exit event of the previous state is properly called") {
                    REQUIRE(previousData.state == light_bulb_state::on);
                    REQUIRE_FALSE(previousData.on_enter_called);
                    REQUIRE(previousData.on_exit_called);
                    REQUIRE(previousData.toggle_event_called);
                }

                THEN("The on_enter event of the new state is properly called") {
                    REQUIRE(currentData.state == light_bulb_state::off);
                    REQUIRE(currentData.on_enter_called);
                    REQUIRE_FALSE(currentData.on_exit_called);
                    REQUIRE_FALSE(currentData.toggle_event_called);
                }
            }
        }
    }
}
