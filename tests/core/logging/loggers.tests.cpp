// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/logging/logging.hpp"

// C++ STL
#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>

// Third-party
#include <spdlog/spdlog.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "test-doubles/core/logging/capture-sink.test-double.hpp"

using namespace tca::engine::core;
using tca::engine::test_doubles::core::logging::capture_sink;
using tca::engine::test_doubles::core::logging::scoped_capture_sink;
using tca::engine::test_doubles::core::logging::scoped_log_level;

TEST_CASE("get_logger() function tests", "[tca][engine][core][logging][get-logger]") {
    SECTION("It gives a logger with the requested name") {
        const auto logger{logging::get_logger("get-logger-tests.name")};

        REQUIRE(logger != nullptr);
        CHECK(logger->name() == "get-logger-tests.name");
    }

    SECTION("The same name gives the same instance, another name another one") {
        const auto first{logging::get_logger("get-logger-tests.same")};
        const auto second{logging::get_logger("get-logger-tests.same")};
        const auto other{logging::get_logger("get-logger-tests.other")};

        CHECK(first == second);
        CHECK(first != other);
    }

    SECTION("The logger is registered with spdlog, so flush_every() reaches it") {
        const auto logger{logging::get_logger("get-logger-tests.registered")};

        CHECK(spdlog::get("get-logger-tests.registered") == logger);
    }

    SECTION("It writes to the shared sink") {
        const auto logger{logging::get_logger("get-logger-tests.shared-sink")};
        const auto& sinks{logger->sinks()};

        CHECK(std::find(sinks.begin(), sinks.end(), logging::get_log_file_sink()) != sinks.end());
    }

    SECTION("It starts at the engine log level and flushes at the flush level") {
        const auto logger{logging::get_logger("get-logger-tests.policy")};

        CHECK(logger->level() == logging::get_log_level());
        CHECK(logger->flush_level() == logging::FLUSH_LEVEL);
    }
}

TEST_CASE("Engine logger flush policy tests", "[tca][engine][core][logging][flush-policy]") {
    const scoped_capture_sink capture{};
    const auto logger{logging::get_logger("flush-policy-tests")};

    SECTION("A warning or worse is flushed straight away") {
        const auto level{
            GENERATE(spdlog::level::warn, spdlog::level::err, spdlog::level::critical)};
        const auto flushesBefore{capture->flush_count()};

        logger->log(level, "something went wrong");

        CHECK(capture->flush_count() > flushesBefore);
    }

    SECTION("Messages below a warning are left to the buffer") {
        const auto flushesBefore{capture->flush_count()};

        logger->info("all is well");

        CHECK(capture->flush_count() == flushesBefore);
    }
}

TEST_CASE("Engine log level tests", "[tca][engine][core][logging][log-level]") {
    SECTION("The engine starts at the default level") {
        CHECK(logging::get_log_level() == logging::DEFAULT_LOG_LEVEL);
    }

    SECTION("Setting the level reaches loggers created before and after") {
        const auto before{logging::get_logger("log-level-tests.before")};
        const scoped_log_level level{spdlog::level::debug};
        const auto after{logging::get_logger("log-level-tests.after")};

        CHECK(logging::get_log_level() == spdlog::level::debug);
        CHECK(before->level() == spdlog::level::debug);
        CHECK(after->level() == spdlog::level::debug);
    }

    SECTION("Messages below the level are dropped, those at it or above are kept") {
        const scoped_capture_sink capture{};
        const auto logger{logging::get_logger("log-level-tests.filter")};

        {
            const scoped_log_level level{spdlog::level::info};
            logger->debug("dropped-at-info");
            logger->info("kept-at-info");
        }

        {
            const scoped_log_level level{spdlog::level::debug};
            logger->debug("kept-at-debug");
        }

        CHECK_FALSE(capture->contains("dropped-at-info"));
        CHECK(capture->contains("kept-at-info"));
        CHECK(capture->contains("kept-at-debug"));
    }
}

TEST_CASE("parse_log_level() function tests", "[tca][engine][core][logging][parse-log-level]") {
    SECTION("Every level name is recognised") {
        const auto [text, expected]{
            GENERATE(table<std::string_view, spdlog::level::level_enum>({
                                                                         {"trace", spdlog::level::trace},
                                                                         {"debug", spdlog::level::debug},
                                                                         {"info", spdlog::level::info},
                                                                         {"warn", spdlog::level::warn},
                                                                         {"warning", spdlog::level::warn},
                                                                         {"error", spdlog::level::err},
                                                                         {"critical", spdlog::level::critical},
                                                                         {"off", spdlog::level::off},
                                                                         }
             ))
        };

        CHECK(logging::parse_log_level(text) == std::optional{expected});
    }

    SECTION("Case doesn't matter") {
        CHECK(logging::parse_log_level("DEBUG") == std::optional{spdlog::level::debug});
        CHECK(logging::parse_log_level("Warning") == std::optional{spdlog::level::warn});
    }

    SECTION("Text that names no level gives nothing") {
        const auto text{GENERATE(std::string_view{}, std::string_view{"verbose"},
                                 std::string_view{" debug"}, std::string_view{"err"})};

        CHECK_FALSE(logging::parse_log_level(text).has_value());
    }
}

TEST_CASE("Engine extra sinks tests", "[tca][engine][core][logging][extra-sinks]") {
    SECTION("A null sink is refused") {
        CHECK_FALSE(logging::add_sink(nullptr));
    }

    SECTION("A sink can only be added once and removed once") {
        const auto sink{std::make_shared<capture_sink>()};

        CHECK(logging::add_sink(sink));
        CHECK_FALSE(logging::add_sink(sink));
        CHECK(logging::remove_sink(sink));
        CHECK_FALSE(logging::remove_sink(sink));
    }

    SECTION("Removing a sink that was never added is refused") {
        CHECK_FALSE(logging::remove_sink(std::make_shared<capture_sink>()));
    }

    SECTION("An added sink hears loggers created before and after it was added") {
        const auto before{logging::get_logger("extra-sinks-tests.before")};
        const scoped_capture_sink capture{};
        const auto after{logging::get_logger("extra-sinks-tests.after")};

        before->info("from-before");
        after->info("from-after");

        CHECK(capture->contains("from-before"));
        CHECK(capture->contains("from-after"));
    }

    SECTION("The captured message keeps the logger's name and the level") {
        const scoped_capture_sink capture{};
        logging::get_logger("extra-sinks-tests.details")->warn("detailed-message");

        const auto messages{capture->messages()};
        REQUIRE(messages.size() == 1);
        CHECK(messages.front().loggerName == "extra-sinks-tests.details");
        CHECK(messages.front().level == spdlog::level::warn);
        CHECK(messages.front().text == "detailed-message");
    }

    SECTION("A removed sink hears nothing more") {
        const auto sink{std::make_shared<capture_sink>()};
        const auto logger{logging::get_logger("extra-sinks-tests.removed")};

        REQUIRE(logging::add_sink(sink));
        REQUIRE(logging::remove_sink(sink));
        logger->info("after-removal");

        CHECK_FALSE(sink->contains("after-removal"));
    }
}
