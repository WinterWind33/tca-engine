// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/argument-parser.hpp"
#include "platform/terminal/tokenizer.hpp"

// C++ STL
#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace tca::engine::platform::terminal;

/// @brief Extracts the texts of the parsed positionals, which is all most scenarios care about;
/// the offsets and the terminator flag have sections of their own.
std::vector<std::string> values_of(const std::vector<parsed_positional>& positionals) {
    std::vector<std::string> values{};
    values.reserve(positionals.size());
    std::ranges::transform(positionals, std::back_inserter(values),
                           [](const parsed_positional& positional) {
                               return positional.value;
                           });
    return values;
}

/// @brief Builds a flat command with no sub-commands, e.g. nmap, echo.
command_spec make_flat_command(std::string name) {
    return command_spec{std::move(name)};
}

/// @brief Builds the "git" tree used across several sections: git, git remote, git remote add,
/// git remote remove, git commit.
command_spec make_git_command_spec() {
    command_spec remote{"remote"};
    remote.add_subcommand(command_spec{"add"});
    remote.add_subcommand(command_spec{"remove"});

    command_spec git{"git"};
    git.add_subcommand(std::move(remote));
    git.add_subcommand(command_spec{"commit"});
    return git;
}

/// @brief Builds the "dotnet nuget add source" tree to exercise deep (4-level) command chains.
command_spec make_dotnet_command() {
    command_spec add{"add"};
    add.add_subcommand(command_spec{"source"});

    command_spec nuget{"nuget"};
    nuget.add_subcommand(std::move(add));

    command_spec dotnet{"dotnet"};
    dotnet.add_subcommand(std::move(nuget));
    return dotnet;
}

} // namespace

TEST_CASE("command_spec building and lookup",
          "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("A freshly built command exposes its name and has no sub-commands") {
        const command_spec spec{"nmap"};

        REQUIRE(spec.name() == "nmap");
        REQUIRE(spec.subcommands().empty());
        REQUIRE(spec.find_subcommand("anything") == nullptr);
    }

    SECTION("A registered sub-command can be found by its exact keyword") {
        const command_spec git{make_git_command_spec()};

        REQUIRE(git.subcommands().size() == 2);

        const command_spec* remote{git.find_subcommand("remote")};
        REQUIRE(remote != nullptr);
        REQUIRE(remote->name() == "remote");
        REQUIRE(remote->find_subcommand("add") != nullptr);
        REQUIRE(remote->find_subcommand("remove") != nullptr);
    }

    SECTION("Looking up an unregistered sub-command returns nullptr") {
        const command_spec git{make_git_command_spec()};

        REQUIRE(git.find_subcommand("push") == nullptr);
    }
}

TEST_CASE("argument parser - empty and malformed input",
          "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("An empty line is an empty_input error") {
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == parse_error_code::empty_input);
    }

    SECTION("A whitespace-only line is an empty_input error") {
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "   \t  ")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == parse_error_code::empty_input);
    }

    SECTION("A comment-only line is an empty_input error") {
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "# just a comment")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == parse_error_code::empty_input);
    }

    SECTION("A tokenizer failure is surfaced as a tokenization_failed error") {
        const std::vector<command_spec> commands{make_flat_command("echo")};
        // An unterminated double quote makes the tokenizer fail; the parser must forward it.
        const parse_result result{parse_command_line(commands, R"(echo "oops)")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == parse_error_code::tokenization_failed);
        REQUIRE(result.error().tokenizer.has_value());
        REQUIRE(result.error().tokenizer.value() ==
                tokenizer_error{tokenizer_error_code::unterminated_double_quote, 5});
        // The error position mirrors the underlying tokenizer error.
        REQUIRE(result.error().position == 5);
    }
}

TEST_CASE("argument parser - the command itself",
          "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("A bare command with no arguments") {
        const std::vector<command_spec> commands{make_flat_command("help")};
        const parse_result result{parse_command_line(commands, "help")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"help"});
        REQUIRE(result.value().options.empty());
        REQUIRE(result.value().positionals.empty());
    }

    SECTION("A first token that names no known command is an unknown_command error") {
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "ping 10.0.0.1")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == parse_error_code::unknown_command);
        // The position points at the offending first token.
        REQUIRE(result.error().position == 0);
    }

    SECTION("An empty command set makes every line an unknown_command error") {
        const std::vector<command_spec> commands{};
        const parse_result result{parse_command_line(commands, "nmap")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == parse_error_code::unknown_command);
        REQUIRE(result.error().position == 0);
    }

    SECTION("Leading whitespace before the command is tolerated") {
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "   nmap")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"nmap"});
    }
}

TEST_CASE("argument parser - dispatch across the whole command set",
          "[tca][engine][platform][terminal][argument-parser]") {
    /// @brief The full set a shell would hand over: two flat commands plus two command trees.
    const std::vector<command_spec> commands{make_flat_command("echo"), make_flat_command("nmap"),
                                             make_git_command_spec(), make_dotnet_command()};

    SECTION("The first token selects its command out of the set") {
        const parse_result result{parse_command_line(commands, "nmap -sV 10.0.0.1")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"nmap"});
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"10.0.0.1"});
    }

    SECTION("Selecting a command keeps its own sub-command tree in play") {
        const parse_result result{parse_command_line(commands, "git remote add origin")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git", "remote", "add"});
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"origin"});
    }

    SECTION("A sub-command keyword of one command is not a command of its own") {
        // "commit" only exists under "git"; on its own it names nothing in the set.
        const parse_result result{parse_command_line(commands, "commit -a")};

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == parse_error_code::unknown_command);
        REQUIRE(result.error().position == 0);
    }

    SECTION("A keyword shared by two commands resolves against the one that was invoked") {
        // Both "git remote" and "dotnet nuget" nest an "add"; the head token disambiguates.
        const parse_result result{parse_command_line(commands, "dotnet nuget add source feed")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path ==
                std::vector<std::string>{"dotnet", "nuget", "add", "source"});
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"feed"});
    }
}

TEST_CASE("argument parser - positionals", "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("Bare words after a leaf command become positionals in order") {
        const std::vector<command_spec> commands{make_flat_command("echo")};
        const parse_result result{parse_command_line(commands, "echo hello world")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"echo"});
        REQUIRE(values_of(result.value().positionals) ==
                std::vector<std::string>{"hello", "world"});
        REQUIRE(result.value().options.empty());
    }

    SECTION("A quoted positional keeps the tokenizer-resolved value") {
        const std::vector<command_spec> commands{make_flat_command("echo")};
        const parse_result result{parse_command_line(commands, R"(echo "hello there")")};

        REQUIRE(result.has_value());
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"hello there"});
    }

    SECTION("A bare '-' is a positional, not an option") {
        const std::vector<command_spec> commands{make_flat_command("cat")};
        const parse_result result{parse_command_line(commands, "cat -")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options.empty());
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"-"});
    }
}

TEST_CASE("argument parser - positionals keep their place in the line",
          "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("Every positional records the offset it was read from") {
        const std::vector<command_spec> commands{make_flat_command("echo")};
        const parse_result result{parse_command_line(commands, "echo hello world")};

        REQUIRE(result.has_value());
        REQUIRE(
            result.value().positionals ==
            std::vector<parsed_positional>{
                parsed_positional{.value = "hello", .position = 5,  .after_terminator = false},
                parsed_positional{.value = "world", .position = 11, .after_terminator = false}
        });
    }

    SECTION("A quoted positional points at its opening quote") {
        const std::vector<command_spec> commands{make_flat_command("echo")};
        const parse_result result{parse_command_line(commands, R"(echo "hello there" tail)")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().positionals.at(0).position == 5);
        REQUIRE(result.value().positionals.at(1).position == 19);
    }

    SECTION("Offsets interleave with the option offsets, which is what recovers the source order") {
        // The split between options and positionals throws the left-to-right order away; comparing
        // offsets is what lets a later pass see that "8080" directly followed "--port".
        const std::vector<command_spec> commands{make_flat_command("scan")};
        const parse_result result{parse_command_line(commands, "scan host --port 8080")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options.at(0).position == 10);
        REQUIRE(result.value().positionals ==
                std::vector<parsed_positional>{
                    parsed_positional{.value = "host", .position = 5,  .after_terminator = false},
                    parsed_positional{.value = "8080", .position = 17, .after_terminator = false}
        });
    }

    SECTION("Positionals past the '--' terminator are flagged as such") {
        const std::vector<command_spec> commands{make_flat_command("rm")};
        const parse_result result{parse_command_line(commands, "rm file -- --weird")};

        REQUIRE(result.has_value());
        REQUIRE(
            result.value().positionals ==
            std::vector<parsed_positional>{
                parsed_positional{.value = "file",    .position = 3,  .after_terminator = false},
                parsed_positional{.value = "--weird", .position = 11, .after_terminator = true }
        });
    }
}

TEST_CASE("argument parser - long options", "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("A bare long flag has a name and no value") {
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "nmap --verbose")};

        REQUIRE(result.has_value());
        REQUIRE(
            result.value().options ==
            std::vector<parsed_option>{
                parsed_option{
                              .name = "verbose", .long_form = true, .value = std::nullopt, .position = 5}
        });
    }

    SECTION("A long option with an attached value splits on the first '='") {
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "nmap --output=scan.txt")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{
                                  .name = "output", .long_form = true, .value = "scan.txt", .position = 5}
        });
    }

    SECTION("A long option value may itself contain '=' characters") {
        const std::vector<command_spec> commands{make_flat_command("run")};
        const parse_result result{parse_command_line(commands, "run --define=A=B")};

        REQUIRE(result.has_value());
        REQUIRE(
            result.value().options ==
            std::vector<parsed_option>{
                parsed_option{.name = "define", .long_form = true, .value = "A=B", .position = 4}
        });
    }

    SECTION("A long option with a trailing '=' has an empty (but present) value") {
        const std::vector<command_spec> commands{make_flat_command("run")};
        const parse_result result{parse_command_line(commands, "run --name=")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{.name = "name", .long_form = true, .value = "", .position = 4}
        });
    }
}

TEST_CASE("argument parser - short options", "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("A single-character short flag") {
        const std::vector<command_spec> commands{make_flat_command("ls")};
        const parse_result result{parse_command_line(commands, "ls -l")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{
                                  .name = "l", .long_form = false, .value = std::nullopt, .position = 3}
        });
    }

    SECTION("A multi-character short option stays intact (no getopt bundling)") {
        // nmap-style: "-sV" is one option named "sV", not the flags s, V.
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "nmap -sV")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{
                                  .name = "sV", .long_form = false, .value = std::nullopt, .position = 5}
        });
    }

    SECTION("A short option with an attached value") {
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "nmap -oN=scan.txt")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{
                                  .name = "oN", .long_form = false, .value = "scan.txt", .position = 5}
        });
    }

    SECTION("A space-separated value is NOT attached: it becomes a positional") {
        // The parser is structural and cannot know that "-o" takes a value, so "scan.txt" is a
        // positional. Callers must write "-o=scan.txt" to bind a value.
        const std::vector<command_spec> commands{make_flat_command("nmap")};
        const parse_result result{parse_command_line(commands, "nmap -o scan.txt")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{
                                  .name = "o", .long_form = false, .value = std::nullopt, .position = 5}
        });
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"scan.txt"});
    }
}

TEST_CASE("argument parser - the '--' terminator",
          "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("Everything after a bare '--' is a positional, even if it looks like an option") {
        const std::vector<command_spec> commands{make_flat_command("rm")};
        const parse_result result{parse_command_line(commands, "rm -f -- --weird -x")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{
                                  .name = "f", .long_form = false, .value = std::nullopt, .position = 3}
        });
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"--weird", "-x"});
    }

    SECTION("A trailing '--' with nothing after it yields no extra positionals") {
        const std::vector<command_spec> commands{make_flat_command("rm")};
        const parse_result result{parse_command_line(commands, "rm file --")};

        REQUIRE(result.has_value());
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"file"});
    }

    SECTION("A second '--' past the terminator is itself a positional") {
        // The terminator is consumed only once: every later token, "--" included, is passed
        // through verbatim.
        const std::vector<command_spec> commands{make_flat_command("rm")};
        const parse_result result{parse_command_line(commands, "rm -- file -- more")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options.empty());
        REQUIRE(values_of(result.value().positionals) ==
                std::vector<std::string>{"file", "--", "more"});
    }
}

TEST_CASE("argument parser - hyphen runs that do not start an option",
          "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("The cutoff sits exactly at two hyphens") {
        // One hyphen is short, two are long, three or more are nothing at all.
        const std::vector<command_spec> commands{make_flat_command("echo")};
        const parse_result result{parse_command_line(commands, "echo -v --v ---v")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{
                                  .name = "v", .long_form = false, .value = std::nullopt, .position = 5},
                    parsed_option{
                                  .name = "v", .long_form = true,  .value = std::nullopt, .position = 8}
        });
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"---v"});
    }

    SECTION("A token starting with three or more hyphens is a positional, verbatim") {
        const std::vector<command_spec> commands{make_flat_command("echo")};
        const parse_result result{parse_command_line(commands, "echo ---foo ----bar")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options.empty());
        REQUIRE(values_of(result.value().positionals) ==
                std::vector<std::string>{"---foo", "----bar"});
    }

    SECTION("A '=' inside a three-hyphen token is not split into a name and a value") {
        // The token never reaches option parsing, so the '=' stays part of the positional.
        const std::vector<command_spec> commands{make_flat_command("echo")};
        const parse_result result{parse_command_line(commands, "echo ---foo=bar")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options.empty());
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"---foo=bar"});
    }

    SECTION("A token made only of hyphens is a positional, whatever its length") {
        // "-" and "--" are special-cased elsewhere; every longer run is just text.
        const std::vector<command_spec> commands{make_flat_command("echo")};
        const parse_result result{parse_command_line(commands, "echo --- ---- -----")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options.empty());
        REQUIRE(values_of(result.value().positionals) ==
                std::vector<std::string>{"---", "----", "-----"});
    }

    SECTION("A quoted hyphen run is a positional as well") {
        const std::vector<command_spec> commands{make_flat_command("echo")};
        const parse_result result{parse_command_line(commands, R"(echo "---foo")")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().options.empty());
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"---foo"});
    }
}

TEST_CASE("argument parser - sub-command resolution",
          "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("One level of sub-command is folded into the command path") {
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git commit")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git", "commit"});
        REQUIRE(result.value().positionals.empty());
    }

    SECTION("Two levels of sub-command, followed by options and positionals") {
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git remote add origin https://x")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git", "remote", "add"});
        REQUIRE(values_of(result.value().positionals) ==
                std::vector<std::string>{"origin", "https://x"});
    }

    SECTION("A deep (4-level) command chain like 'dotnet nuget add source'") {
        const std::vector<command_spec> commands{make_dotnet_command()};
        const parse_result result{
            parse_command_line(commands, "dotnet nuget add source https://feed --name=corp")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path ==
                std::vector<std::string>{"dotnet", "nuget", "add", "source"});
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"https://feed"});
        REQUIRE(
            result.value().options ==
            std::vector<parsed_option>{
                parsed_option{.name = "name", .long_form = true, .value = "corp", .position = 37}
        });
    }

    SECTION("A bare word that is not a known sub-command stops resolution and is a positional") {
        // "git" has no "status" sub-command, so "status" is treated as a positional argument.
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git status")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git"});
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"status"});
    }

    SECTION("Resolution stops at the first option: later sub-command keywords are positionals") {
        // Global options before a sub-command are not supported: once "-v" appears, "commit" can no
        // longer be treated as a sub-command and falls through to the positionals.
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git -v commit")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git"});
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{
                                  .name = "v", .long_form = false, .value = std::nullopt, .position = 4}
        });
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"commit"});
    }

    SECTION("A bare word that does not match stops resolution for good") {
        // Regression: "origin" is not a sub-command of "remote", so resolution must end there.
        // "add" *is* one, but it comes after a positional, so it must not be folded back into the
        // command path - otherwise the path would stop being a contiguous prefix of the line.
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git remote origin add")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git", "remote"});
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"origin", "add"});
    }

    SECTION("A deep chain also stops at its first non-matching bare word") {
        // Regression: neither "add" nor "source" may resolve once "foo" has broken the chain.
        const std::vector<command_spec> commands{make_dotnet_command()};
        const parse_result result{parse_command_line(commands, "dotnet nuget foo add source")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"dotnet", "nuget"});
        REQUIRE(values_of(result.value().positionals) ==
                std::vector<std::string>{"foo", "add", "source"});
    }

    SECTION("A non-matching bare word right after the root command stops resolution") {
        // Regression: "commit" is a sub-command of "git", but "status" already broke the chain.
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git status commit")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git"});
        REQUIRE(values_of(result.value().positionals) ==
                std::vector<std::string>{"status", "commit"});
    }

    SECTION("A bare '-' stops resolution, like any other positional") {
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git - commit")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git"});
        REQUIRE(result.value().options.empty());
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"-", "commit"});
    }

    SECTION("A three-hyphen token stops resolution, like any other positional") {
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git ---v commit")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git"});
        REQUIRE(result.value().options.empty());
        REQUIRE(values_of(result.value().positionals) ==
                std::vector<std::string>{"---v", "commit"});
    }

    SECTION("Everything past a '--' terminator stays out of the command path") {
        // The terminator is stronger than sub-command resolution: "remote" is a sub-command of
        // "git", but after "--" it is plain text.
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git -- remote add")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git"});
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"remote", "add"});
    }

    SECTION("Only a partial chain resolves when it is not carried all the way down") {
        // "git remote" resolves two levels; the trailing bare word "origin" is not a sub-command of
        // "remote" (only "add"/"remove" are), so it stays a positional.
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(commands, "git remote origin")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git", "remote"});
        REQUIRE(values_of(result.value().positionals) == std::vector<std::string>{"origin"});
    }
}

TEST_CASE("argument parser - a realistic mixed command line",
          "[tca][engine][platform][terminal][argument-parser]") {
    SECTION("Sub-command, short flag, long option with value, and quoted positionals together") {
        const std::vector<command_spec> commands{make_git_command_spec()};
        const parse_result result{parse_command_line(
            commands, R"(git commit -a --message="initial commit" src README.md)")};

        REQUIRE(result.has_value());
        REQUIRE(result.value().command_path == std::vector<std::string>{"git", "commit"});
        REQUIRE(result.value().options ==
                std::vector<parsed_option>{
                    parsed_option{
                                  .name = "a",       .long_form = false, .value = std::nullopt, .position = 11},
                    parsed_option{.name = "message",
                                  .long_form = true,
                                  .value = "initial commit",
                                  .position = 14                                                              }
        });
        REQUIRE(values_of(result.value().positionals) ==
                std::vector<std::string>{"src", "README.md"});
    }
}
