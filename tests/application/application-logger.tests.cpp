// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/application-logger.hpp"

// Third-party
#include <spdlog/spdlog.h>
#include <catch2/catch_test_macros.hpp>

#include "core/logging/logging.hpp"

TEST_CASE("Application logger tests", "[tca][engine][application][application-logger]") {
    using namespace tca::engine::application;

    WHEN("Getting the application logger one time") {
        std::shared_ptr<spdlog::logger> logger{};
        REQUIRE_NOTHROW(logger = get_application_logger());

        THEN("The logger should be valid") {
            REQUIRE(logger != nullptr);
        }

        THEN("It should have the correct sink") {
            REQUIRE(logger->sinks().size() == 1);
            REQUIRE(logger->sinks()[0] == tca::engine::core::logging::get_log_file_sink());
        }

        THEN("It should be an engine logger, following the engine-wide level and flush policy") {
            REQUIRE(spdlog::get(logger->name()) == logger);
            REQUIRE(logger == tca::engine::core::logging::get_logger(logger->name()));
            REQUIRE(logger->level() == tca::engine::core::logging::get_log_level());
            REQUIRE(logger->flush_level() == tca::engine::core::logging::FLUSH_LEVEL);
        }

        AND_WHEN("Getting the application logger another time") {
            std::shared_ptr<spdlog::logger> logger2{};
            REQUIRE_NOTHROW(logger2 = get_application_logger());

            THEN("The two loggers should be the same instance") {
                REQUIRE(logger == logger2);
            }
        }
    }
}
