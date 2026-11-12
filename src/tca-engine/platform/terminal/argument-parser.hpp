// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Project files
#include "platform/terminal/tokenizer.hpp"

namespace tca::engine::platform::terminal {

/**
 * @brief Declares one node of a command tree: a command name and the sub-commands nested under it.
 * @note The tree carries no option/flag declarations on purpose: options are parsed structurally
 * (see parse_command_line()). The tree exists only so the parser knows how deep the leading
 * command chain runs, letting it tell a sub-command (e.g. the "remote" in "git remote add") apart
 * from an ordinary positional argument (e.g. the "hello" in "echo hello"). This is what lets
 * git-/dotnet-style N-level commands be resolved.
 */
class command_spec {
public:
    /**
     * @brief
     * @param name The exact keyword that names this command (or sub-command) at its level.
     */
    explicit command_spec(std::string name);

    /**
     * @brief Registers a nested sub-command directly under this one.
     * @param subcommand The fully-built sub-command tree to attach.
     * @return A reference to this spec so sibling sub-commands can be chained.
     */
    command_spec& add_subcommand(command_spec subcommand);

    /**
     * @brief
     * @return This command's keyword.
     */
    [[nodiscard]] std::string_view name() const noexcept;

    /**
     * @brief
     * @return The sub-commands nested directly under this one.
     */
    [[nodiscard]] const std::vector<command_spec>& subcommands() const noexcept;

    /**
     * @brief Looks up a directly-nested sub-command by its exact keyword.
     * @param name The keyword to look for.
     * @return A pointer to the matching sub-command, or nullptr if none matches.
     */
    [[nodiscard]] const command_spec* find_subcommand(std::string_view name) const noexcept;

private:
    std::string m_name{};
    std::vector<command_spec> m_subcommands{};
};

/**
 * @brief A single option occurrence recovered from the command line (a "-x" or "--foo" token).
 * @note Options are recognised purely by shape, never against a per-command declaration:
 * - A token starting with "--" is a long option; everything after the "--" up to an optional '='
 *   is the name, and anything after the '=' is the value ("--output=out.txt" -> name "output",
 *   value "out.txt"; "--verbose" -> name "verbose", no value).
 * - A token starting with a single '-' (and at least one more character) is a short option; the
 *   whole run after the '-' up to an optional '=' is the name, so multi-character short options
 *   like nmap's "-sV" stay intact ("-sV" -> name "sV"). There is no getopt-style bundling.
 * - Only those two hyphen runs start an option. A bare "-" is a positional (the conventional stdin
 *   placeholder), and so is any token whose leading hyphen run is three or more characters long
 *   ("---foo", "----", "---"): such a token names no option and is passed through verbatim.
 * A space-separated value ("-o out.txt") is NOT attached here: the parser is structural and cannot
 * know which options take a value, so "out.txt" lands in positionals. It is still recoverable,
 * because every positional records where it came from (see parsed_positional): a later,
 * declaration-aware pass can adopt the positional that directly follows a bare option as that
 * option's value.
 */
struct parsed_option {
    /// @brief The option name, without leading dashes and without any "=value" suffix.
    std::string name;

    /// @brief true for a "--long" option, false for a "-short" one.
    bool long_form{};

    /// @brief The attached value from a "name=value" token, or empty when the option is a bare
    /// flag.
    std::optional<std::string> value{};

    /// @brief The offset, in the original line, of the option's leading '-'.
    std::size_t position{};

    bool operator==(const parsed_option&) const noexcept = default;
};

/**
 * @brief A single positional argument recovered from the command line.
 * @note A positional keeps its source offset so that a later, declaration-aware pass can tell which
 * positional directly followed a bare option and may therefore be adopted as that option's value.
 * Comparing offsets against parsed_option::position is what reconstructs the original left-to-right
 * order, which the split between options and positionals would otherwise lose.
 */
struct parsed_positional {
    /// @brief The argument's text, with quoting and escaping already resolved by the tokenizer.
    std::string value;

    /// @brief The offset, in the original line, of the first character that produced this argument
    /// (the opening quote character when the argument was quoted).
    std::size_t position{};

    /// @brief true when this argument appeared after a bare "--" terminator, which bars it from
    /// ever being read as anything but a positional.
    bool after_terminator{};

    bool operator==(const parsed_positional&) const noexcept = default;
};

/**
 * @brief The fully-structured view of one command line: which command was invoked, its options and
 * its positional arguments.
 */
struct parsed_command_line {
    /// @brief The resolved command chain, from the root command down through every matched
    /// sub-command, e.g. {"git", "remote", "add"}. Always at least one element on success.
    std::vector<std::string> command_path;

    /// @brief Every option occurrence, in the order they appeared on the line.
    std::vector<parsed_option> options;

    /// @brief Every positional argument, in order. Everything after a bare "--" terminator lands
    /// here verbatim, even if it looks like an option.
    std::vector<parsed_positional> positionals;

    bool operator==(const parsed_command_line&) const noexcept = default;
};

/**
 * @brief The reasons parse_command_line() can fail.
 */
enum class parse_error_code : std::uint8_t {
    /// @brief The line held no command token at all (it was blank, whitespace-only, or a comment).
    empty_input,

    /// @brief The first token did not name any of the commands handed to the parser.
    unknown_command,

    /// @brief The underlying tokenizer rejected the line; see parse_error::tokenizer for details.
    tokenization_failed
};

/**
 * @brief Describes why parse_command_line() failed.
 */
struct parse_error {
    /// @brief Why parsing failed.
    parse_error_code code{};

    /// @brief The offset, in the original line, most relevant to the failure (the first token for
    /// unknown_command, 0 for empty_input, the tokenizer's failure offset for tokenization_failed).
    std::size_t position{};

    /// @brief The underlying tokenizer error, set only when code == tokenization_failed.
    std::optional<tokenizer_error> tokenizer{};

    bool operator==(const parse_error&) const noexcept = default;
};

/**
 * @brief The outcome of parse_command_line(): either the structured command line or the error that
 * stopped parsing. Hand-rolled rather than std::expected since the engine targets C++20.
 */
class parse_result {
public:
    /**
     * @brief
     * @param parsed The structured command line.
     * @return A successful result wrapping parsed.
     */
    [[nodiscard]] static parse_result success(parsed_command_line&& parsed) noexcept;

    /**
     * @brief
     * @param error The reason parsing failed.
     * @return A failed result wrapping error.
     */
    [[nodiscard]] static parse_result failure(parse_error&& error) noexcept;

    /**
     * @brief
     * @return true if parsing succeeded and value() can be called.
     */
    [[nodiscard]] bool has_value() const noexcept;

    /**
     * @brief
     * @note Precondition: has_value() is true.
     * @return The structured command line.
     */
    [[nodiscard]] const parsed_command_line& value() const noexcept;

    /**
     * @brief
     * @note Precondition: has_value() is false.
     * @return The reason parsing failed.
     */
    [[nodiscard]] const parse_error& error() const noexcept;

private:
    explicit parse_result(std::variant<parsed_command_line, parse_error> result) noexcept;

    std::variant<parsed_command_line, parse_error> m_result;
};

/**
 * @brief Parses a single terminal command line into its command chain, options and positionals,
 * against the set of commands the shell knows about.
 *
 * The line is first split with tokenize_command_line(), so all quoting, escaping and comment rules
 * are inherited from the tokenizer. The resulting tokens are then interpreted as follows:
 * - The first token is looked up, by exact keyword, among commands. When nothing matches, the
 *   result is an unknown_command error; otherwise the matched command becomes the head of
 *   command_path and the root of sub-command resolution.
 * - Starting from that command, while the next token is a bare word (not an option) that names a
 * sub-command of the current node, the parser descends into that sub-command and appends it to
 * command_path. Resolution stops at the first token that is an option or a bare word with no
 * matching sub-command; sub-commands must therefore precede any option (e.g. "git remote add -v",
 * not "git -v remote add").
 * - Each remaining token is classified by shape (see parsed_option): a "--foo"/"--foo=bar" token is
 *   a long option, a "-x"/"-x=bar" token is a short option, a bare "--" ends option parsing so
 * every later token is a positional, and anything else (including a bare "-" and any token starting
 * with three or more hyphens) is a positional.
 *
 * @note The caller owns the command set and hands the whole of it over, rather than guessing which
 * command the line invokes: resolving the first token is the parser's job. The same set can then
 * back the help/usage layer, which needs every known command anyway.
 * @param commands Every command the shell can dispatch, each as its own tree. Duplicate keywords
 * are not diagnosed; the first match wins.
 * @param line The raw command line to parse.
 * @return The structured command line, or the error that stopped parsing.
 */
[[nodiscard]] parse_result parse_command_line(std::span<const command_spec> commands,
                                              std::string_view line);

} // namespace tca::engine::platform::terminal
