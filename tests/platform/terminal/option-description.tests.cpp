// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/option-description.hpp"

// C++ STL
#include <cstdint>
#include <string>

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace tca::engine::platform::terminal;

} // namespace

TEST_CASE("option description - names", "[tca][engine][platform][terminal][option-description]") {
    SECTION("An option built from a long name only exposes that name and nothing else") {
        const option_description option{"verbose"};

        REQUIRE(option.has_long_name());
        REQUIRE(option.long_name() == "verbose");
        REQUIRE_FALSE(option.has_short_name());
        REQUIRE(option.short_name().empty());
        REQUIRE(option.description().empty());
    }

    SECTION("A short name and a description can be attached fluently") {
        const option_description option{
            option_description{"verbose"}.with_short_name("v").with_description(
                "Print more information")};

        REQUIRE(option.long_name() == "verbose");
        REQUIRE(option.has_short_name());
        REQUIRE(option.short_name() == "v");
        REQUIRE(option.description() == "Print more information");
    }

    SECTION("A multi-character short name is kept intact, nmap style") {
        // The parser reads "-sV" as a single short option named "sV" (there is no getopt-style
        // bundling), so the declaration side must be able to name it the same way.
        const option_description option{
            option_description{"service-version"}.with_short_name("sV")};

        REQUIRE(option.short_name() == "sV");
    }

    SECTION("An option can be declared with a short name only") {
        const option_description option{
            option_description::short_only("sV").with_description("Probe open ports for services")};

        REQUIRE_FALSE(option.has_long_name());
        REQUIRE(option.long_name().empty());
        REQUIRE(option.has_short_name());
        REQUIRE(option.short_name() == "sV");
    }

    SECTION(
        "The builder methods return the very same option, so chaining never copies a stale "
        "state") {
        option_description option{"verbose"};
        option_description& returned{option.with_short_name("v")};

        REQUIRE(&returned == &option);
    }
}

TEST_CASE("option description - flags carry no value",
          "[tca][engine][platform][terminal][option-description]") {
    SECTION("An option with no value specification is a pure flag") {
        const option_description option{"verbose"};

        REQUIRE_FALSE(option.takes_value());
        REQUIRE(option.arity() == value_arity::none);
        REQUIRE_FALSE(option.is_required());
        REQUIRE_FALSE(option.has_default_value());
        REQUIRE_FALSE(option.has_implicit_value());
    }

    SECTION("A flag has no value placeholder to render") {
        const option_description option{"verbose"};

        REQUIRE(option.value_name().empty());
    }
}

TEST_CASE("option description - mandatory values",
          "[tca][engine][platform][terminal][option-description]") {
    SECTION("A bare value specification makes the value mandatory whenever the option appears") {
        const option_description option{
            option_description{"port"}.with_value(value<std::int32_t>())};

        REQUIRE(option.takes_value());
        REQUIRE(option.arity() == value_arity::required);
        // The option itself stays optional: only its value is mandatory.
        REQUIRE_FALSE(option.is_required());
    }

    SECTION("An unnamed value renders under the generic 'arg' placeholder") {
        const option_description option{
            option_description{"port"}.with_value(value<std::int32_t>())};

        REQUIRE(option.value_name() == "arg");
    }

    SECTION("A value can be given a domain-specific placeholder name") {
        const option_description option{
            option_description{"track"}.with_value(value<std::string>().with_name("ref"))};

        REQUIRE(option.value_name() == "ref");
    }
}

TEST_CASE("option description - required options",
          "[tca][engine][platform][terminal][option-description]") {
    SECTION("required() makes the option itself mandatory on the command line") {
        const option_description option{
            option_description{"target"}.with_value(value<std::string>().required())};

        REQUIRE(option.is_required());
        REQUIRE(option.arity() == value_arity::required);
    }

    SECTION("An option is not required unless it says so") {
        const option_description option{
            option_description{"target"}.with_value(value<std::string>())};

        REQUIRE_FALSE(option.is_required());
    }
}

TEST_CASE("option description - default values",
          "[tca][engine][platform][terminal][option-description]") {
    SECTION("A default value is remembered and rendered as text for the help page") {
        const option_description option{
            option_description{"threads"}.with_value(value<std::int32_t>().default_value(5))};

        REQUIRE(option.has_default_value());
        REQUIRE(option.default_value_text() == "5");
    }

    SECTION("A default value does not make the value itself optional") {
        // The default only applies when the option is absent; writing "--threads" with no value is
        // still a missing value.
        const option_description option{
            option_description{"threads"}.with_value(value<std::int32_t>().default_value(5))};

        REQUIRE(option.arity() == value_arity::required);
    }

    SECTION("A string default is rendered verbatim") {
        const option_description option{
            option_description{"output"}.with_value(value<std::string>().default_value("out.txt"))};

        REQUIRE(option.default_value_text() == "out.txt");
    }

    SECTION("A boolean default is rendered as true/false") {
        const option_description enabled{
            option_description{"color"}.with_value(value<bool>().default_value(true))};
        const option_description disabled{
            option_description{"color"}.with_value(value<bool>().default_value(false))};

        REQUIRE(enabled.default_value_text() == "true");
        REQUIRE(disabled.default_value_text() == "false");
    }

    SECTION("A default value can carry its own textual rendering for the help page") {
        // Useful when the raw value reads badly, e.g. a port number that means "pick one for me".
        const option_description option{option_description{"port"}.with_value(
            value<std::int32_t>().default_value(0, "any free port"))};

        REQUIRE(option.default_value_text() == "any free port");
    }

    SECTION("An option with no default reports none") {
        const option_description option{
            option_description{"threads"}.with_value(value<std::int32_t>())};

        REQUIRE_FALSE(option.has_default_value());
    }
}

TEST_CASE("option description - implicit values",
          "[tca][engine][platform][terminal][option-description]") {
    SECTION("An implicit value makes the value optional when the option appears") {
        const option_description option{
            option_description{"fetch"}.with_value(value<std::int32_t>().implicit_value(8))};

        REQUIRE(option.takes_value());
        REQUIRE(option.arity() == value_arity::optional);
        REQUIRE(option.has_implicit_value());
        REQUIRE(option.implicit_value_text() == "8");
    }

    SECTION("Default and implicit values coexist and mean different things") {
        // default: the option was absent entirely. implicit: the option appeared bare.
        const option_description option{option_description{"fetch"}.with_value(
            value<std::int32_t>().default_value(5).implicit_value(10))};

        REQUIRE(option.has_default_value());
        REQUIRE(option.default_value_text() == "5");
        REQUIRE(option.has_implicit_value());
        REQUIRE(option.implicit_value_text() == "10");
        REQUIRE(option.arity() == value_arity::optional);
    }

    SECTION("An option with no implicit value reports none") {
        const option_description option{
            option_description{"fetch"}.with_value(value<std::int32_t>())};

        REQUIRE_FALSE(option.has_implicit_value());
    }
}

TEST_CASE("option description - copying", "[tca][engine][platform][terminal][option-description]") {
    SECTION("A copied option keeps its whole value specification") {
        // command_description stores options by value, so the type-erased value semantics must
        // survive a copy.
        const option_description original{
            option_description{"threads"}
                .with_short_name("t")
                .with_description("Worker threads")
                .with_value(
                    value<std::int32_t>().default_value(5).implicit_value(10).with_name("count"))};
        const option_description copy{original};

        REQUIRE(copy.long_name() == "threads");
        REQUIRE(copy.short_name() == "t");
        REQUIRE(copy.description() == "Worker threads");
        REQUIRE(copy.arity() == value_arity::optional);
        REQUIRE(copy.default_value_text() == "5");
        REQUIRE(copy.implicit_value_text() == "10");
        REQUIRE(copy.value_name() == "count");
    }
}
