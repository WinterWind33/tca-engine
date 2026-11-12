// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/events.hpp"

// C++ STL
#include <cstdint>
#include <string>

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("event class tests", "[tca][engine][core][events][event]") {
    using namespace tca::engine::core;

    GIVEN("An event with int and string parameters") {
        event<std::int32_t, std::string> eventUnderTest{};
        auto& subscriber = eventUnderTest.get_subscriber();

        WHEN("Subscribing a handler to the event") {
            bool handlerCalled{false};
            event<std::int32_t, std::string>::subscriber::handler_id_t handlerId{};
            REQUIRE_NOTHROW(
                handlerId = subscriber.subscribe(
                    [&handlerCalled](std::int32_t intParam, const std::string& stringParam) {
                        handlerCalled = true;
                        REQUIRE(intParam == 42);
                        REQUIRE(stringParam == "Hello, World!");
                    }));

            THEN(
                "Executing the event should call the subscribed handler with the correct "
                "parameters") {
                REQUIRE_NOTHROW(eventUnderTest.execute(42, "Hello, World!"));
                REQUIRE(handlerCalled);
            }

            THEN(
                "Unsubscribing the handler should prevent it from being called when executing the "
                "event") {
                REQUIRE_NOTHROW(subscriber.unsubscribe(handlerId));
                handlerCalled = false;
                REQUIRE_NOTHROW(eventUnderTest.execute(42, "Hello, World!"));
                REQUIRE_FALSE(handlerCalled);
            }
        }
    }
}
