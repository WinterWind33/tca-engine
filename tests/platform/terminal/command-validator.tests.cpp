// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/command-validator.hpp"
#include "platform/terminal/argument-parser.hpp"
#include "platform/terminal/command-description.hpp"
#include "platform/terminal/option-description.hpp"

// C++ STL
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace tca::engine::platform::terminal;

/// @brief Runs a raw line through the structural parser and then through the validator, which is
/// how the shell will chain the two.
validation_result validate_line(const command_description& root, std::string_view line) {
    const std::vector<command_spec> specs{root.to_command_spec()};
    const parse_result parsed{parse_command_line(specs, line)};
    REQUIRE(parsed.has_value());

    return validate_command_line(root, parsed.value());
}

/// @brief Compares a positional binding, handed out as a view, against the values it must hold.
bool holds(std::span<const std::string> actual, const std::vector<std::string>& expected) {
    return std::ranges::equal(actual, expected);
}

/**
 * Builds the flat command used by most sections:
 *
 *   scan  -v/--verbose  flag
 *         -p/--port     mandatory std::int32_t
 *         -o/--output   mandatory std::string, defaults to "scan.txt"
 *             --threads optional std::int32_t, defaults to 1, implicitly 8
 *             --color   mandatory bool
 *             --timeout mandatory double
 *         -sV           flag with no long form
 */
command_description make_scan_command() {
    command_description scan{"scan", "Scans a host for open ports."};
    scan.add_option(
        option_description{"verbose"}.with_short_name("v").with_description("Print more info"));
    scan.add_option(
        option_description{"port"}.with_short_name("p").with_value(value<std::int32_t>()));
    scan.add_option(option_description{"output"}.with_short_name("o").with_value(
        value<std::string>().default_value("scan.txt")));
    scan.add_option(option_description{"threads"}.with_value(
        value<std::int32_t>().default_value(1).implicit_value(8)));
    scan.add_option(option_description{"color"}.with_value(value<bool>()));
    scan.add_option(option_description{"timeout"}.with_value(value<double>()));
    scan.add_option(option_description::short_only("sV").with_description("Probe for services"));
    return scan;
}

/**
 * Builds a nested tree with an inherited option and required positionals:
 *
 *   git                  -v/--verbose flag, -h/--help
 *   git remote add       -f/--fetch, positionals <name> and optional <url>
 */
command_description make_git_validation_tree() {
    command_description add{"add", "Add a new remote repository."};
    add.add_option(option_description{"fetch"}.with_short_name("f").with_value(
        value<std::int32_t>().implicit_value(10)));
    add.add_positional(positional_description{"name"});
    add.add_positional(positional_description{"url"}.optional());

    command_description remote{"remote", "Manage the set of tracked repositories."};
    remote.add_subcommand(std::move(add));

    command_description git{"git", "The stupid content tracker."};
    git.add_option(option_description{"verbose"}.with_short_name("v"));
    git.with_help_option();
    git.add_subcommand(std::move(remote));
    return git;
}

} // namespace

TEST_CASE("command validator - flags", "[tca][engine][platform][terminal][command-validator]") {
    const command_description scan{make_scan_command()};

    SECTION("A flag given in its long form resolves to true") {
        const validation_result result{validate_line(scan, "scan --verbose")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().contains("verbose"));
        REQUIRE(result.value().options().get<bool>("verbose"));
        REQUIRE(result.value().options().count("verbose") == 1);
    }

    SECTION("A flag given in its short form resolves under its long name") {
        const validation_result result{validate_line(scan, "scan -v")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().contains("verbose"));
        // Values are only ever keyed by the long name when there is one.
        REQUIRE_FALSE(result.value().options().contains("v"));
    }

    SECTION("An absent flag is simply not there") {
        const validation_result result{validate_line(scan, "scan")};

        REQUIRE(result.has_value());
        REQUIRE_FALSE(result.value().options().contains("verbose"));
        REQUIRE_FALSE(result.value().options().try_get<bool>("verbose").has_value());
        REQUIRE(result.value().options().count("verbose") == 0);
    }

    SECTION("A repeated flag keeps a count, which is how verbosity levels are read") {
        const validation_result result{validate_line(scan, "scan -v -v -v")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<bool>("verbose"));
        REQUIRE(result.value().options().count("verbose") == 3);
    }

    SECTION("A short-only option is keyed by its short name") {
        const validation_result result{validate_line(scan, "scan -sV")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().contains("sV"));
        REQUIRE(result.value().options().get<bool>("sV"));
    }

    SECTION("Attaching a value to a flag is rejected") {
        const validation_result result{validate_line(scan, "scan --verbose=yes")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::unexpected_value);
        REQUIRE(result.error().name == "verbose");
        REQUIRE(result.error().text == "yes");
        REQUIRE(result.error().position == 5);
    }
}

TEST_CASE("command validator - unknown options",
          "[tca][engine][platform][terminal][command-validator]") {
    const command_description scan{make_scan_command()};

    SECTION("An undeclared long option is rejected and pointed at") {
        const validation_result result{validate_line(scan, "scan --bogus")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::unknown_option);
        REQUIRE(result.error().name == "bogus");
        REQUIRE(result.error().position == 5);
    }

    SECTION("An undeclared short option is rejected") {
        const validation_result result{validate_line(scan, "scan -z")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::unknown_option);
        REQUIRE(result.error().name == "z");
    }

    SECTION("A long form never matches a short name") {
        const validation_result result{validate_line(scan, "scan --v")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::unknown_option);
        REQUIRE(result.error().name == "v");
    }

    SECTION("A short form never matches a long name") {
        const validation_result result{validate_line(scan, "scan -verbose")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::unknown_option);
        REQUIRE(result.error().name == "verbose");
    }

    SECTION("The first offending option is the one reported") {
        const validation_result result{validate_line(scan, "scan --bogus --worse")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().name == "bogus");
    }
}

TEST_CASE("command validator - typed values",
          "[tca][engine][platform][terminal][command-validator]") {
    const command_description scan{make_scan_command()};

    SECTION("An integer value is converted") {
        const validation_result result{validate_line(scan, "scan --port=8080")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("port") == 8080);
    }

    SECTION("The short form carries a value just as well") {
        const validation_result result{validate_line(scan, "scan -p=8080")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("port") == 8080);
    }

    SECTION("A negative integer is converted") {
        const validation_result result{validate_line(scan, "scan --port=-1")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("port") == -1);
    }

    SECTION("A mandatory value left out is reported") {
        const validation_result result{validate_line(scan, "scan --port")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::missing_value);
        REQUIRE(result.error().name == "port");
        REQUIRE(result.error().position == 5);
    }

    SECTION("A value that is not an integer is reported with the offending text") {
        const validation_result result{validate_line(scan, "scan --port=abc")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::invalid_value);
        REQUIRE(result.error().name == "port");
        REQUIRE(result.error().text == "abc");
    }

    SECTION("Trailing junk after a number is not silently ignored") {
        const validation_result result{validate_line(scan, "scan --port=80x")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::invalid_value);
        REQUIRE(result.error().text == "80x");
    }

    SECTION("A number too large for the target type is reported") {
        const validation_result result{validate_line(scan, "scan --port=99999999999")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::invalid_value);
    }

    SECTION("An empty integer value is reported") {
        const validation_result result{validate_line(scan, "scan --port=")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::invalid_value);
        REQUIRE(result.error().text.empty());
    }

    SECTION("A string value is taken verbatim") {
        const validation_result result{validate_line(scan, "scan --output=report.txt")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::string>("output") == "report.txt");
    }

    SECTION("An empty string is a perfectly good string value") {
        const validation_result result{validate_line(scan, "scan --output=")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::string>("output").empty());
    }

    SECTION("Quoting survives all the way into the converted value") {
        const validation_result result{validate_line(scan, R"(scan --output="my report.txt")")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::string>("output") == "my report.txt");
    }

    SECTION("A floating point value is converted") {
        const validation_result result{validate_line(scan, "scan --timeout=2.5")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<double>("timeout") == 2.5);
    }
}

TEST_CASE("command validator - boolean values",
          "[tca][engine][platform][terminal][command-validator]") {
    const command_description scan{make_scan_command()};

    SECTION("The affirmative spellings all mean true") {
        for (const std::string_view spelling : {"true", "1", "yes", "on"}) {
            const validation_result result{
                validate_line(scan, std::string{"scan --color="} + std::string{spelling})};

            REQUIRE(result.has_value());
            REQUIRE(result.value().options().get<bool>("color"));
        }
    }

    SECTION("The negative spellings all mean false") {
        for (const std::string_view spelling : {"false", "0", "no", "off"}) {
            const validation_result result{
                validate_line(scan, std::string{"scan --color="} + std::string{spelling})};

            REQUIRE(result.has_value());
            REQUIRE_FALSE(result.value().options().get<bool>("color"));
        }
    }

    SECTION("Boolean spellings are matched without regard to case") {
        const validation_result result{validate_line(scan, "scan --color=TRUE")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<bool>("color"));
    }

    SECTION("Anything else is not a boolean") {
        const validation_result result{validate_line(scan, "scan --color=maybe")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::invalid_value);
        REQUIRE(result.error().name == "color");
        REQUIRE(result.error().text == "maybe");
    }
}

TEST_CASE("command validator - default and implicit values",
          "[tca][engine][platform][terminal][command-validator]") {
    const command_description scan{make_scan_command()};

    SECTION("An absent option with a default is present in the result, with a count of zero") {
        const validation_result result{validate_line(scan, "scan")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().contains("output"));
        REQUIRE(result.value().options().get<std::string>("output") == "scan.txt");
        // Nothing was written on the line: the count tells a default apart from a real occurrence.
        REQUIRE(result.value().options().count("output") == 0);
    }

    SECTION("An explicit value overrides the default") {
        const validation_result result{validate_line(scan, "scan --output=other.txt")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::string>("output") == "other.txt");
        REQUIRE(result.value().options().count("output") == 1);
    }

    SECTION("A default does not make the value optional when the option is written out") {
        const validation_result result{validate_line(scan, "scan --output")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::missing_value);
        REQUIRE(result.error().name == "output");
    }

    SECTION("An option written bare falls back to its implicit value") {
        const validation_result result{validate_line(scan, "scan --threads")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("threads") == 8);
        REQUIRE(result.value().options().count("threads") == 1);
    }

    SECTION("The same option left out entirely falls back to its default instead") {
        const validation_result result{validate_line(scan, "scan")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("threads") == 1);
    }

    SECTION("An explicit value beats both the implicit and the default one") {
        const validation_result result{validate_line(scan, "scan --threads=4")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("threads") == 4);
    }

    SECTION("An absent option with no default stays absent") {
        const validation_result result{validate_line(scan, "scan")};

        REQUIRE(result.has_value());
        REQUIRE_FALSE(result.value().options().contains("port"));
    }

    SECTION("A repeated valued option keeps the last value but counts every occurrence") {
        const validation_result result{validate_line(scan, "scan --port=1 --port=2")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("port") == 2);
        REQUIRE(result.value().options().count("port") == 2);
    }
}

TEST_CASE("command validator - required options",
          "[tca][engine][platform][terminal][command-validator]") {
    /// Builds a command whose target must always be named.
    const auto make_required_command{[]() {
        command_description scan{"scan"};
        scan.add_option(option_description{"target"}.with_short_name("T").with_value(
            value<std::string>().required()));
        return scan;
    }};

    SECTION("A required option left out is reported") {
        const validation_result result{validate_line(make_required_command(), "scan")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::missing_required_option);
        REQUIRE(result.error().name == "target");
        // Nothing on the line to point a caret at.
        REQUIRE(result.error().position == 0);
    }

    SECTION("A required option that is present passes") {
        const validation_result result{
            validate_line(make_required_command(), "scan --target=10.0.0.1")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::string>("target") == "10.0.0.1");
    }

    SECTION("A required short-only option is reported under its short name") {
        command_description scan{"scan"};
        scan.add_option(
            option_description::short_only("sV").with_value(value<std::string>().required()));

        const validation_result result{validate_line(scan, "scan")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::missing_required_option);
        REQUIRE(result.error().name == "sV");
    }
}

TEST_CASE("command validator - positional arguments",
          "[tca][engine][platform][terminal][command-validator]") {
    /// Builds "copy <source> [<destination>]".
    const auto make_copy_command{[]() {
        command_description copy{"copy"};
        copy.add_positional(positional_description{"source"});
        copy.add_positional(positional_description{"destination"}.optional());
        return copy;
    }};

    SECTION("Positionals are bound to their declared names, in order") {
        const validation_result result{validate_line(make_copy_command(), "copy a.txt b.txt")};

        REQUIRE(result.has_value());
        REQUIRE(holds(result.value().positionals("source"), {"a.txt"}));
        REQUIRE(holds(result.value().positionals("destination"), {"b.txt"}));
    }

    SECTION("An optional positional left out binds to nothing") {
        const validation_result result{validate_line(make_copy_command(), "copy a.txt")};

        REQUIRE(result.has_value());
        REQUIRE(holds(result.value().positionals("source"), {"a.txt"}));
        REQUIRE(result.value().positionals("destination").empty());
    }

    SECTION("A required positional left out is reported") {
        const validation_result result{validate_line(make_copy_command(), "copy")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::missing_required_positional);
        REQUIRE(result.error().name == "source");
    }

    SECTION("A positional nobody declared is reported with its text") {
        const validation_result result{
            validate_line(make_copy_command(), "copy a.txt b.txt c.txt")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::too_many_positionals);
        REQUIRE(result.error().text == "c.txt");
    }

    SECTION("Looking up a positional nobody declared yields nothing") {
        const validation_result result{validate_line(make_copy_command(), "copy a.txt")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().positionals("nowhere").empty());
    }

    SECTION("A repeated positional swallows everything that is left") {
        command_description remove_command{"rm"};
        remove_command.add_positional(positional_description{"paths"}.repeated());

        const validation_result result{validate_line(remove_command, "rm a.txt b.txt c.txt")};

        REQUIRE(result.has_value());
        REQUIRE(holds(result.value().positionals("paths"), {"a.txt", "b.txt", "c.txt"}));
    }

    SECTION("A repeated positional still leaves room for the ones declared before it") {
        command_description move_command{"mv"};
        move_command.add_positional(positional_description{"destination"});
        move_command.add_positional(positional_description{"sources"}.repeated());

        const validation_result result{validate_line(move_command, "mv dir a.txt b.txt")};

        REQUIRE(result.has_value());
        REQUIRE(holds(result.value().positionals("destination"), {"dir"}));
        REQUIRE(holds(result.value().positionals("sources"), {"a.txt", "b.txt"}));
    }

    SECTION("A repeated positional is satisfied by nothing at all when it is optional") {
        command_description remove_command{"rm"};
        remove_command.add_positional(positional_description{"paths"}.optional().repeated());

        const validation_result result{validate_line(remove_command, "rm")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().positionals("paths").empty());
    }

    SECTION("A command declaring no positionals lets every one of them through unchecked") {
        // This is what keeps free-form commands such as "echo" usable without declaring anything.
        command_description echo{"echo"};

        const validation_result result{validate_line(echo, "echo hello there world")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().all_positionals() ==
                std::vector<std::string>{"hello", "there", "world"});
    }

    SECTION("Every positional is also reachable as one flat list") {
        const validation_result result{validate_line(make_copy_command(), "copy a.txt b.txt")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().all_positionals() == std::vector<std::string>{"a.txt", "b.txt"});
    }

    SECTION("Whatever follows the -- terminator is bound as an ordinary positional") {
        const validation_result result{validate_line(make_copy_command(), "copy -- --weird")};

        REQUIRE(result.has_value());
        REQUIRE(holds(result.value().positionals("source"), {"--weird"}));
    }
}

TEST_CASE("command validator - the help option short-circuits everything",
          "[tca][engine][platform][terminal][command-validator]") {
    const command_description git{make_git_validation_tree()};

    SECTION("Asking for help succeeds even though a required positional is missing") {
        const validation_result result{validate_line(git, "git remote add --help")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().help_requested());
        REQUIRE(result.value().command_path() == std::vector<std::string>{"git", "remote", "add"});
    }

    SECTION("The short form asks for help too") {
        const validation_result result{validate_line(git, "git remote add -h")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().help_requested());
    }

    SECTION("Help is reachable at the command that declared it") {
        const validation_result result{validate_line(git, "git --help")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().help_requested());
    }

    SECTION("Nothing else on the line is looked at once help is asked for") {
        const validation_result result{
            validate_line(git, "git remote add --help --fetch=nonsense")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().help_requested());
    }

    SECTION("A normal line does not request help") {
        const validation_result result{validate_line(git, "git remote add origin")};

        REQUIRE(result.has_value());
        REQUIRE_FALSE(result.value().help_requested());
    }

    SECTION("A command that never enabled help does not recognise --help") {
        const command_description scan{make_scan_command()};
        const validation_result result{validate_line(scan, "scan --help")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::unknown_option);
        REQUIRE(result.error().name == "help");
    }
}

TEST_CASE("command validator - inherited options",
          "[tca][engine][platform][terminal][command-validator]") {
    const command_description git{make_git_validation_tree()};

    SECTION("An option declared on an ancestor is usable from a sub-command") {
        const validation_result result{validate_line(git, "git remote add --verbose origin")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<bool>("verbose"));
    }

    SECTION("An option declared on a sub-command is not usable from its ancestor") {
        const validation_result result{validate_line(git, "git --fetch=1")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::unknown_option);
        REQUIRE(result.error().name == "fetch");
    }

    SECTION("The deepest declaration wins when the same name appears twice in the chain") {
        command_description build{"build"};
        build.add_option(
            option_description{"jobs"}.with_value(value<std::int32_t>().default_value(4)));

        command_description tool{"tool"};
        tool.add_option(
            option_description{"jobs"}.with_value(value<std::string>().default_value("auto")));
        tool.add_subcommand(std::move(build));

        const validation_result deep{validate_line(tool, "tool build --jobs=8")};
        REQUIRE(deep.has_value());
        REQUIRE(deep.value().options().get<std::int32_t>("jobs") == 8);

        const validation_result shallow{validate_line(tool, "tool --jobs=8")};
        REQUIRE(shallow.has_value());
        REQUIRE(shallow.value().options().get<std::string>("jobs") == "8");

        // The default seeded when the option is absent comes from the same, deepest declaration.
        const validation_result defaulted{validate_line(tool, "tool build")};
        REQUIRE(defaulted.has_value());
        REQUIRE(defaulted.value().options().get<std::int32_t>("jobs") == 4);
        REQUIRE_FALSE(defaulted.value().options().try_get<std::string>("jobs").has_value());
    }
}

TEST_CASE("command validator - the resolved line",
          "[tca][engine][platform][terminal][command-validator]") {
    SECTION("The command path is carried over from the parser") {
        const command_description git{make_git_validation_tree()};
        const validation_result result{validate_line(git, "git remote add origin git@host:repo")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path() == std::vector<std::string>{"git", "remote", "add"});
    }

    SECTION("A command with no options at all resolves to an empty option set") {
        command_description clear{"clear"};

        const validation_result result{validate_line(clear, "clear")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().empty());
    }

    SECTION("Asking for a value of the wrong type yields nothing rather than a wrong answer") {
        const command_description scan{make_scan_command()};
        const validation_result result{validate_line(scan, "scan --port=8080")};

        REQUIRE(result.has_value());
        REQUIRE_FALSE(result.value().options().try_get<std::string>("port").has_value());
        REQUIRE(result.value().options().try_get<std::int32_t>("port").value() == 8080);
    }

    SECTION("Looking up an option nobody declared yields nothing") {
        const command_description scan{make_scan_command()};
        const validation_result result{validate_line(scan, "scan")};

        REQUIRE(result.has_value());
        REQUIRE_FALSE(result.value().options().contains("nowhere"));
        REQUIRE(result.value().options().count("nowhere") == 0);
        REQUIRE_FALSE(result.value().options().try_get<bool>("nowhere").has_value());
    }
}

TEST_CASE("command validator - space-separated values",
          "[tca][engine][platform][terminal][command-validator]") {
    const command_description scan{make_scan_command()};

    /// Builds "fetch [-d/--depth <n>] <url>", which holds both a valued option and a positional, so
    /// that a value being adopted can be told apart from a positional being bound.
    const auto make_fetch_command{[]() {
        command_description fetch{"fetch"};
        fetch.add_option(
            option_description{"depth"}.with_short_name("d").with_value(value<std::int32_t>()));
        fetch.add_positional(positional_description{"url"});
        return fetch;
    }};

    SECTION("A mandatory value written after a space is adopted by its option") {
        const validation_result result{validate_line(scan, "scan --port 8080")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("port") == 8080);
        // Having become the option's value, it is no longer a positional.
        REQUIRE(result.value().all_positionals().empty());
    }

    SECTION("The short form adopts a space-separated value just as well") {
        const validation_result result{validate_line(scan, "scan -p 8080")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("port") == 8080);
    }

    SECTION("The attached form keeps working next to the space-separated one") {
        const validation_result attached{validate_line(scan, "scan --port=8080")};
        const validation_result separated{validate_line(scan, "scan --port 8080")};

        REQUIRE(attached.has_value());
        REQUIRE(separated.has_value());
        REQUIRE(attached.value().options().get<std::int32_t>("port") ==
                separated.value().options().get<std::int32_t>("port"));
    }

    SECTION("Only the value directly following the option is adopted, the rest stays positional") {
        const validation_result result{
            validate_line(make_fetch_command(), "fetch --depth 3 https://host")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("depth") == 3);
        REQUIRE(holds(result.value().positionals("url"), {"https://host"}));
    }

    SECTION("An option written after the positionals still adopts what follows it") {
        const validation_result result{
            validate_line(make_fetch_command(), "fetch https://host --depth 3")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("depth") == 3);
        REQUIRE(holds(result.value().positionals("url"), {"https://host"}));
    }

    SECTION("A positional swallowed by an option no longer satisfies a declared one") {
        const validation_result result{validate_line(make_fetch_command(), "fetch --depth 3")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::missing_required_positional);
        REQUIRE(result.error().name == "url");
    }

    SECTION("Nothing is adopted across another option") {
        const validation_result result{validate_line(scan, "scan --port --verbose 8080")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::missing_value);
        REQUIRE(result.error().name == "port");
    }

    SECTION("Nothing is adopted from past the -- terminator") {
        const validation_result result{validate_line(scan, "scan --port -- 8080")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::missing_value);
        REQUIRE(result.error().name == "port");
    }

    SECTION("A flag never adopts what follows it") {
        const validation_result result{validate_line(scan, "scan --verbose host")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<bool>("verbose"));
        REQUIRE(result.value().all_positionals() == std::vector<std::string>{"host"});
    }

    SECTION("An option with an implicit value never adopts, it stays implicit") {
        // "--fetch 3" cannot be told apart from a bare "--fetch" followed by a positional, so the
        // implicit value wins and "3" is left where it was. Write "--fetch=3" to mean the value.
        const command_description git{make_git_validation_tree()};
        const validation_result result{validate_line(git, "git remote add --fetch 3 origin")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("fetch") == 10);
        REQUIRE(holds(result.value().positionals("name"), {"3"}));
        REQUIRE(holds(result.value().positionals("url"), {"origin"}));
    }

    SECTION("A negative value cannot be written after a space, the parser reads it as an option") {
        // "-1" is a short option as far as the parser is concerned, so there is nothing to adopt.
        const validation_result result{validate_line(scan, "scan --port -1")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == validation_error_code::missing_value);
        REQUIRE(result.error().name == "port");
    }

    SECTION("Every occurrence adopts its own value") {
        const validation_result result{validate_line(scan, "scan --port 1 --port 2")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options().get<std::int32_t>("port") == 2);
        REQUIRE(result.value().options().count("port") == 2);
        REQUIRE(result.value().all_positionals().empty());
    }
}
