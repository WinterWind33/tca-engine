// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/command-description.hpp"
#include "platform/terminal/argument-parser.hpp"
#include "platform/terminal/option-description.hpp"

// C++ STL
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace tca::engine::platform::terminal;

/**
 * Builds the reference tree used across the help-rendering sections:
 *
 *   git                 -v/--verbose, -h/--help (synthetic)
 *   git commit
 *   git remote          (no options of its own)
 *   git remote add      -t/--track <ref>, -f/--fetch [<n>], positionals <name> <url>
 */
command_description make_git_help_tree() {
    command_description add{"add", "Add a new remote repository."};
    add.add_option(option_description{"track"}
                       .with_short_name("t")
                       .with_description("Track only this branch")
                       .with_value(value<std::string>().with_name("ref")));
    add.add_option(
        option_description{"fetch"}
            .with_short_name("f")
            .with_description("Fetch after adding")
            .with_value(value<std::int32_t>().default_value(5).implicit_value(10).with_name("n")));
    add.add_positional(positional_description{"name"}.with_description("The name of the remote"));
    add.add_positional(positional_description{"url"}.with_description("The URL of the remote"));

    command_description remote{"remote", "Manage the set of tracked repositories."};
    remote.add_subcommand(std::move(add));

    command_description git{"git", "The stupid content tracker."};
    git.add_option(
        option_description{"verbose"}.with_short_name("v").with_description("Print more "
                                                                            "information"));
    git.with_help_option();
    git.add_subcommand(std::move(remote));
    git.add_subcommand(command_description{"commit", "Record changes to the repository."});
    return git;
}

/// @brief Returns true when no line of text ends in a space or a tab.
bool has_no_trailing_whitespace(std::string_view text) {
    std::size_t line_start{0};
    while (line_start <= text.size()) {
        const std::size_t line_end{text.find('\n', line_start)};
        const std::string_view line{text.substr(
            line_start,
            (line_end == std::string_view::npos ? text.size() : line_end) - line_start)};
        if (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            return false;
        }

        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1;
    }
    return true;
}

} // namespace

TEST_CASE("command description - building and lookup",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("A command exposes its name and its description") {
        const command_description command{"scan", "Scans a host for open ports."};

        REQUIRE(command.name() == "scan");
        REQUIRE(command.description() == "Scans a host for open ports.");
        REQUIRE(command.options().empty());
        REQUIRE(command.subcommands().empty());
        REQUIRE(command.positionals().empty());
    }

    SECTION("A command can be built without a description") {
        const command_description command{"clear"};

        REQUIRE(command.name() == "clear");
        REQUIRE(command.description().empty());
    }

    SECTION("Declared options are kept in declaration order") {
        command_description command{"scan"};
        command.add_option(option_description{"verbose"}.with_short_name("v"));
        command.add_option(option_description{"port"}.with_short_name("p"));

        REQUIRE(command.options().size() == 2);
        REQUIRE(command.options().at(0).long_name() == "verbose");
        REQUIRE(command.options().at(1).long_name() == "port");
    }

    SECTION("An option can be found by its long name") {
        command_description command{"scan"};
        command.add_option(
            option_description{"verbose"}.with_short_name("v").with_description("Be loud"));

        const option_description* found{command.find_option("verbose")};
        REQUIRE(found != nullptr);
        REQUIRE(found->description() == "Be loud");
        REQUIRE(command.find_option("quiet") == nullptr);
        // A long-name lookup must not answer for a short name.
        REQUIRE(command.find_option("v") == nullptr);
    }

    SECTION("An option can be found by its short name") {
        command_description command{"scan"};
        command.add_option(option_description{"verbose"}.with_short_name("v"));
        command.add_option(option_description::short_only("sV"));

        REQUIRE(command.find_short_option("v") != nullptr);
        REQUIRE(command.find_short_option("v")->long_name() == "verbose");
        REQUIRE(command.find_short_option("sV") != nullptr);
        REQUIRE(command.find_short_option("q") == nullptr);
        // A short-name lookup must not answer for a long name.
        REQUIRE(command.find_short_option("verbose") == nullptr);
    }

    SECTION("Sub-commands can be nested and looked up by keyword") {
        const command_description git{make_git_help_tree()};

        REQUIRE(git.subcommands().size() == 2);

        const command_description* remote{git.find_subcommand("remote")};
        REQUIRE(remote != nullptr);
        REQUIRE(remote->name() == "remote");
        REQUIRE(remote->find_subcommand("add") != nullptr);
        REQUIRE(git.find_subcommand("push") == nullptr);
    }
}

TEST_CASE("command description - resolving a command path",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("A full path resolves to the deepest command") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git", "remote", "add"};

        const command_description* found{git.find_command(path)};
        REQUIRE(found != nullptr);
        REQUIRE(found->name() == "add");
    }

    SECTION("A path holding only the root resolves to the root itself") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git"};

        REQUIRE(git.find_command(path) == &git);
    }

    SECTION("A path whose first element is not this command does not resolve") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"svn", "remote"};

        REQUIRE(git.find_command(path) == nullptr);
    }

    SECTION("A path naming an unknown sub-command does not resolve") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git", "remote", "rename"};

        REQUIRE(git.find_command(path) == nullptr);
    }

    SECTION("An empty path does not resolve") {
        const command_description git{make_git_help_tree()};

        REQUIRE(git.find_command(std::vector<std::string>{}) == nullptr);
    }

    SECTION("The whole chain can be recovered, root first") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git", "remote", "add"};

        const std::vector<const command_description*> chain{git.resolve_path(path)};
        REQUIRE(chain.size() == 3);
        REQUIRE(chain.at(0) == &git);
        REQUIRE(chain.at(1)->name() == "remote");
        REQUIRE(chain.at(2)->name() == "add");
    }

    SECTION("An unresolvable path yields an empty chain") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git", "push"};

        REQUIRE(git.resolve_path(path).empty());
    }
}

TEST_CASE("command description - positional arguments",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("A positional is required and single-valued by default") {
        const positional_description positional{"name"};

        REQUIRE(positional.name() == "name");
        REQUIRE(positional.description().empty());
        REQUIRE_FALSE(positional.is_optional());
        REQUIRE_FALSE(positional.is_repeated());
    }

    SECTION("A positional can be made optional, repeated, or both") {
        const positional_description optional_one{positional_description{"url"}.optional()};
        const positional_description repeated_one{positional_description{"paths"}.repeated()};
        const positional_description both{positional_description{"extras"}.optional().repeated()};

        REQUIRE(optional_one.is_optional());
        REQUIRE_FALSE(optional_one.is_repeated());
        REQUIRE_FALSE(repeated_one.is_optional());
        REQUIRE(repeated_one.is_repeated());
        REQUIRE(both.is_optional());
        REQUIRE(both.is_repeated());
    }

    SECTION("Positionals are kept in declaration order and can be looked up by name") {
        command_description command{"cp"};
        command.add_positional(
            positional_description{"source"}.with_description("What to copy").repeated());
        command.add_positional(
            positional_description{"destination"}.with_description("Where to copy it"));

        REQUIRE(command.positionals().size() == 2);
        REQUIRE(command.positionals().at(0).name() == "source");
        REQUIRE(command.positionals().at(1).name() == "destination");

        const positional_description* found{command.find_positional("destination")};
        REQUIRE(found != nullptr);
        REQUIRE(found->description() == "Where to copy it");
        REQUIRE(command.find_positional("nothing") == nullptr);
    }
}

TEST_CASE("command description - the help option",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("A command has no help option unless it asks for one") {
        const command_description command{"scan"};

        REQUIRE_FALSE(command.has_help_option());
        REQUIRE(command.find_option("help") == nullptr);
        REQUIRE(command.options().empty());
    }

    SECTION("Enabling the help option adds a -h/--help entry with a stock description") {
        command_description command{"scan"};
        command.with_help_option();

        REQUIRE(command.has_help_option());
        REQUIRE(command.options().size() == 1);

        const option_description* help{command.find_option("help")};
        REQUIRE(help != nullptr);
        REQUIRE(help->short_name() == "h");
        REQUIRE(help->description() == "Print this help page");
        // The help option is a pure flag: "--help=something" makes no sense.
        REQUIRE_FALSE(help->takes_value());
    }

    SECTION("The help option can carry a custom description") {
        command_description command{"scan"};
        command.with_help_option("Show what this command can do");

        const option_description* help{command.find_option("help")};
        REQUIRE(help != nullptr);
        REQUIRE(help->description() == "Show what this command can do");
    }

    SECTION("The help option is always the last entry, whatever the declaration order") {
        command_description command{"scan"};
        command.with_help_option();
        command.add_option(option_description{"verbose"}.with_short_name("v"));
        command.add_option(option_description{"port"}.with_short_name("p"));

        REQUIRE(command.options().size() == 3);
        REQUIRE(command.options().at(0).long_name() == "verbose");
        REQUIRE(command.options().at(1).long_name() == "port");
        REQUIRE(command.options().at(2).long_name() == "help");
    }

    SECTION("The help option can be turned back off") {
        command_description command{"scan"};
        command.with_help_option();
        command.without_help_option();

        REQUIRE_FALSE(command.has_help_option());
        REQUIRE(command.find_option("help") == nullptr);
        REQUIRE(command.options().empty());
    }

    SECTION("Turning off a help option that was never enabled is harmless") {
        command_description command{"scan"};
        command.add_option(option_description{"verbose"}.with_short_name("v"));
        command.without_help_option();

        REQUIRE_FALSE(command.has_help_option());
        REQUIRE(command.options().size() == 1);
    }

    SECTION("A hand-declared 'help' option wins: no second one is synthesised") {
        command_description command{"scan"};
        command.add_option(
            option_description{"help"}.with_description("List the available help topics"));
        command.with_help_option();

        REQUIRE(command.has_help_option());
        REQUIRE(command.options().size() == 1);

        const option_description* help{command.find_option("help")};
        REQUIRE(help != nullptr);
        REQUIRE(help->description() == "List the available help topics");
    }

    SECTION("A short name already taken by another option is left alone") {
        // "-h" means "--host" here, so the synthetic help option must settle for "--help" only.
        command_description command{"scan"};
        command.add_option(
            option_description{"host"}.with_short_name("h").with_value(value<std::string>()));
        command.with_help_option();

        const option_description* help{command.find_option("help")};
        REQUIRE(help != nullptr);
        REQUIRE_FALSE(help->has_short_name());

        const option_description* host{command.find_short_option("h")};
        REQUIRE(host != nullptr);
        REQUIRE(host->long_name() == "host");
    }

    SECTION("Enabling the help option twice does not duplicate it") {
        command_description command{"scan"};
        command.with_help_option();
        command.with_help_option("A better description");

        REQUIRE(command.options().size() == 1);

        const option_description* help{command.find_option("help")};
        REQUIRE(help != nullptr);
        REQUIRE(help->description() == "A better description");
    }

    SECTION("A sub-command does not silently grow its own help option") {
        // Enabling help on the root makes it *reachable* from every sub-command (see the validator
        // tests), but the sub-command's own declaration list stays untouched.
        const command_description git{make_git_help_tree()};
        const command_description* remote{git.find_subcommand("remote")};

        REQUIRE(git.has_help_option());
        REQUIRE_FALSE(remote->has_help_option());
        REQUIRE(remote->options().empty());
    }
}

TEST_CASE("command description - bridging to the structural parser",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("The command tree converts to the command_spec the parser expects") {
        const command_description git{make_git_help_tree()};
        const command_spec spec{git.to_command_spec()};

        REQUIRE(spec.name() == "git");
        REQUIRE(spec.subcommands().size() == 2);

        const command_spec* remote{spec.find_subcommand("remote")};
        REQUIRE(remote != nullptr);
        REQUIRE(remote->find_subcommand("add") != nullptr);
        REQUIRE(spec.find_subcommand("commit") != nullptr);
    }
}

TEST_CASE("command description - help page, minimal commands",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("A bare command renders only its usage line") {
        const command_description command{"clear"};

        const std::string expected{
            "Usage:\n"
            "  clear\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("A described command renders its description as the opening paragraph") {
        const command_description command{"clear", "Clears the terminal screen."};

        const std::string expected{
            "Clears the terminal screen.\n"
            "\n"
            "Usage:\n"
            "  clear\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("The page always ends with exactly one newline and never leaves trailing spaces") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git", "remote", "add"};
        const std::string page{git.help(path)};

        REQUIRE(page.ends_with("\n"));
        REQUIRE_FALSE(page.ends_with("\n\n"));
        REQUIRE(has_no_trailing_whitespace(page));
    }
}

TEST_CASE("command description - help page, sections",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("A root command lists its sub-commands and its own options") {
        const command_description git{make_git_help_tree()};

        const std::string expected{
            "The stupid content tracker.\n"
            "\n"
            "Usage:\n"
            "  git <command> [options]\n"
            "\n"
            "Commands:\n"
            "  remote    Manage the set of tracked repositories.\n"
            "  commit    Record changes to the repository.\n"
            "\n"
            "Options:\n"
            "  -v, --verbose    Print more information\n"
            "  -h, --help       Print this help page\n"};

        REQUIRE(git.help() == expected);
    }

    SECTION("A leaf sub-command renders its arguments, its options and the inherited ones") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git", "remote", "add"};

        const std::string expected{
            "Add a new remote repository.\n"
            "\n"
            "Usage:\n"
            "  git remote add [options] <name> <url>\n"
            "\n"
            "Arguments:\n"
            "  <name>    The name of the remote\n"
            "  <url>     The URL of the remote\n"
            "\n"
            "Options:\n"
            "  -t, --track <ref>    Track only this branch\n"
            "  -f, --fetch [<n>]    Fetch after adding (default: 5)\n"
            "\n"
            "Global options:\n"
            "  -v, --verbose    Print more information\n"
            "  -h, --help       Print this help page\n"};

        REQUIRE(git.help(path) == expected);
    }

    SECTION("An intermediate command with no options of its own skips the Options section") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git", "remote"};

        const std::string expected{
            "Manage the set of tracked repositories.\n"
            "\n"
            "Usage:\n"
            "  git remote <command> [options]\n"
            "\n"
            "Commands:\n"
            "  add    Add a new remote repository.\n"
            "\n"
            "Global options:\n"
            "  -v, --verbose    Print more information\n"
            "  -h, --help       Print this help page\n"};

        REQUIRE(git.help(path) == expected);
    }

    SECTION("Rendering a sub-command in isolation loses the ancestors it cannot see") {
        // A node extracted from the tree has no way back up, so it renders its own name as the
        // whole usage line and shows no Global options section. Render from the root to get both.
        const command_description git{make_git_help_tree()};
        const command_description* remote{git.find_subcommand("remote")};
        REQUIRE(remote != nullptr);

        const command_description* add{remote->find_subcommand("add")};
        REQUIRE(add != nullptr);

        const std::string expected{
            "Add a new remote repository.\n"
            "\n"
            "Usage:\n"
            "  add [options] <name> <url>\n"
            "\n"
            "Arguments:\n"
            "  <name>    The name of the remote\n"
            "  <url>     The URL of the remote\n"
            "\n"
            "Options:\n"
            "  -t, --track <ref>    Track only this branch\n"
            "  -f, --fetch [<n>]    Fetch after adding (default: 5)\n"};

        REQUIRE(add->help() == expected);
    }

    SECTION("A path that does not resolve renders nothing at all") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git", "push"};

        REQUIRE(git.help(path).empty());
    }
}

TEST_CASE("command description - help page, the usage line",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("[options] is omitted when the command reaches no option at all") {
        command_description command{"clear"};
        command.add_positional(positional_description{"screen"});

        const std::string expected{
            "Usage:\n"
            "  clear <screen>\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("Optional and repeated positionals get their own placeholder shapes") {
        command_description command{"rm"};
        command.add_positional(positional_description{"path"});
        command.add_positional(positional_description{"backup"}.optional());
        command.add_positional(positional_description{"paths"}.repeated());
        command.add_positional(positional_description{"extras"}.optional().repeated());

        const std::string expected{
            "Usage:\n"
            "  rm <path> [<backup>] <paths>... [<extras>...]\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("A command carrying both sub-commands and positionals shows both placeholders") {
        command_description command{"task"};
        command.add_subcommand(command_description{"run"});
        command.add_positional(positional_description{"id"});

        const std::string expected{
            "Usage:\n"
            "  task <command> <id>\n"
            "\n"
            "Commands:\n"
            "  run\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("Undescribed positionals still shape the usage line but raise no Arguments section") {
        command_description command{"echo"};
        command.add_positional(positional_description{"text"}.repeated());

        const std::string expected{
            "Usage:\n"
            "  echo <text>...\n"};

        REQUIRE(command.help() == expected);
    }
}

TEST_CASE("command description - help page, option entries",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("An option with no short name is indented so the long names stay aligned") {
        command_description command{"build"};
        command.add_option(option_description{"dry-run"}.with_description("Do not write anything"));
        command.add_option(option_description{"jobs"}
                               .with_short_name("j")
                               .with_description("Parallel jobs")
                               .with_value(value<std::int32_t>()));

        const std::string expected{
            "Usage:\n"
            "  build [options]\n"
            "\n"
            "Options:\n"
            "      --dry-run       Do not write anything\n"
            "  -j, --jobs <arg>    Parallel jobs\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("A short-only option renders without any long form") {
        command_description command{"nmap"};
        command.add_option(
            option_description::short_only("sV").with_description("Probe for service versions"));

        const std::string expected{
            "Usage:\n"
            "  nmap [options]\n"
            "\n"
            "Options:\n"
            "  -sV    Probe for service versions\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("A mandatory value is shown bare, an optional one in brackets") {
        command_description command{"scan"};
        command.add_option(option_description{"port"}
                               .with_description("Target port")
                               .with_value(value<std::int32_t>().with_name("port")));
        command.add_option(
            option_description{"fetch"}
                .with_description("Fetch after adding")
                .with_value(value<std::int32_t>().implicit_value(10).with_name("n")));

        const std::string expected{
            "Usage:\n"
            "  scan [options]\n"
            "\n"
            "Options:\n"
            "      --port <port>    Target port\n"
            "      --fetch [<n>]    Fetch after adding\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("A required option is marked as such") {
        command_description command{"scan"};
        command.add_option(option_description{"target"}
                               .with_short_name("T")
                               .with_description("Host to scan")
                               .with_value(value<std::string>().required().with_name("host")));

        const std::string expected{
            "Usage:\n"
            "  scan [options]\n"
            "\n"
            "Options:\n"
            "  -T, --target <host>    Host to scan (required)\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("A default value is appended to the description, an implicit one is not") {
        // The brackets around the placeholder already say the value may be left out; spelling the
        // implicit value out too would just add noise.
        command_description command{"scan"};
        command.add_option(
            option_description{"threads"}
                .with_description("Worker threads")
                .with_value(value<std::int32_t>().default_value(1).implicit_value(8)));

        const std::string expected{
            "Usage:\n"
            "  scan [options]\n"
            "\n"
            "Options:\n"
            "      --threads [<arg>]    Worker threads (default: 1)\n"};

        REQUIRE(command.help() == expected);
    }

    SECTION("An option with no description shows only its label") {
        command_description command{"scan"};
        command.add_option(option_description{"verbose"}.with_short_name("v"));

        const std::string expected{
            "Usage:\n"
            "  scan [options]\n"
            "\n"
            "Options:\n"
            "  -v, --verbose\n"};

        REQUIRE(command.help() == expected);
    }
}

TEST_CASE("command description - help page, wrapping",
          "[tca][engine][platform][terminal][command-description]") {
    /// Builds a command whose texts are long enough to need wrapping.
    const auto make_wordy_command{[]() {
        command_description scan{"scan",
                                 "Scans a remote host for open TCP ports and reports what it "
                                 "finds."};
        scan.add_option(option_description{"verbose"}.with_short_name("v").with_description(
            "Print a great deal more information about what is going on"));
        return scan;
    }};

    SECTION("Descriptions wrap at word boundaries, aligned under the description column") {
        const command_description scan{make_wordy_command()};
        help_format format{};
        format.line_width = 40;

        const std::string expected{
            "Scans a remote host for open TCP ports\n"
            "and reports what it finds.\n"
            "\n"
            "Usage:\n"
            "  scan [options]\n"
            "\n"
            "Options:\n"
            "  -v, --verbose    Print a great deal\n"
            "                   more information\n"
            "                   about what is going\n"
            "                   on\n"};

        REQUIRE(scan.help(format) == expected);
    }

    SECTION("A line width of zero disables wrapping entirely") {
        const command_description scan{make_wordy_command()};
        help_format format{};
        format.line_width = 0;

        const std::string expected{
            "Scans a remote host for open TCP ports and reports what it finds.\n"
            "\n"
            "Usage:\n"
            "  scan [options]\n"
            "\n"
            "Options:\n"
            "  -v, --verbose    Print a great deal more information about what is going on\n"};

        REQUIRE(scan.help(format) == expected);
    }

    SECTION("A word wider than the column is never broken, it just overflows") {
        command_description command{"tune"};
        command.add_option(option_description{"mode"}.with_short_name("m").with_description(
            "Supercalifragilisticexpialidocious tuning"));
        help_format format{};
        format.line_width = 25;

        const std::string expected{
            "Usage:\n"
            "  tune [options]\n"
            "\n"
            "Options:\n"
            "  -m, --mode    Supercalifragilisticexpialidocious\n"
            "                tuning\n"};

        REQUIRE(command.help(format) == expected);
    }

    SECTION("A line width narrower than the description column switches wrapping off") {
        command_description command{"tune"};
        command.add_option(option_description{"mode"}.with_short_name("m").with_description(
            "Pick the tuning profile to use"));
        help_format format{};
        format.line_width = 10;

        const std::string expected{
            "Usage:\n"
            "  tune [options]\n"
            "\n"
            "Options:\n"
            "  -m, --mode    Pick the tuning profile to use\n"};

        REQUIRE(command.help(format) == expected);
    }

    SECTION("The indent and the gap before the description column are configurable") {
        command_description command{"scan"};
        command.add_option(
            option_description{"verbose"}.with_short_name("v").with_description("Be loud"));
        command.add_option(
            option_description{"port"}.with_short_name("p").with_description("Target port"));
        help_format format{};
        format.indent = 4;
        format.column_gap = 2;

        const std::string expected{
            "Usage:\n"
            "    scan [options]\n"
            "\n"
            "Options:\n"
            "    -v, --verbose  Be loud\n"
            "    -p, --port     Target port\n"};

        REQUIRE(command.help(format) == expected);
    }
}

TEST_CASE("command description - printing the help page",
          "[tca][engine][platform][terminal][command-description]") {
    SECTION("Printing to a stream writes exactly what help() returns") {
        const command_description git{make_git_help_tree()};

        std::ostringstream stream{};
        git.print_help(stream);

        REQUIRE(stream.str() == git.help());
    }

    SECTION("Printing a sub-command to a stream writes exactly what help(path) returns") {
        const command_description git{make_git_help_tree()};
        const std::vector<std::string> path{"git", "remote", "add"};

        std::ostringstream stream{};
        git.print_help(stream, path);

        REQUIRE(stream.str() == git.help(path));
    }

    SECTION("The stream insertion operator prints the command's own help page") {
        const command_description git{make_git_help_tree()};

        std::ostringstream stream{};
        stream << git;

        REQUIRE(stream.str() == git.help());
    }

    SECTION("The formatting options carry over to the stream overloads") {
        const command_description command{"scan", "Scans a remote host for open TCP ports."};
        help_format format{};
        format.line_width = 30;

        std::ostringstream stream{};
        command.print_help(stream, format);

        REQUIRE(stream.str() == command.help(format));
        REQUIRE(stream.str().starts_with("Scans a remote host for open\nTCP ports.\n"));
    }
}
