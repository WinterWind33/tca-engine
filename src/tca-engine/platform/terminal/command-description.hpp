// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Project files
#include "platform/terminal/argument-parser.hpp"
#include "platform/terminal/option-description.hpp"

namespace tca::engine::platform::terminal {

/**
 * @brief Declares one positional argument of a command: the name it is bound to once parsed, and
 * how many values it accepts.
 * @note Positionals are bound in declaration order. A repeated positional swallows everything left
 * over, so declaring one anywhere but last starves the ones after it.
 */
class positional_description {
public:
    /**
     * @brief Declares a mandatory, single-valued positional.
     * @param name The name it is bound to, and the placeholder the help page renders inside angle
     * brackets.
     */
    explicit positional_description(std::string name);

    /**
     * @brief Sets the one-line text the help page shows next to the argument.
     * @param description What the argument is for.
     * @return A reference to this argument.
     */
    positional_description& with_description(std::string description);

    /**
     * @brief Makes the argument skippable: leaving it out is not an error.
     * @return A reference to this argument.
     */
    positional_description& optional();

    /**
     * @brief Lets the argument take every value left on the line rather than just one.
     * @return A reference to this argument.
     */
    positional_description& repeated();

    /**
     * @brief
     * @return The name the argument is bound to.
     */
    [[nodiscard]] std::string_view name() const noexcept;

    /**
     * @brief
     * @return What the argument is for, empty when it was never described.
     */
    [[nodiscard]] std::string_view description() const noexcept;

    /**
     * @brief
     * @return true when the argument may be left out.
     */
    [[nodiscard]] bool is_optional() const noexcept;

    /**
     * @brief
     * @return true when the argument takes every value left over.
     */
    [[nodiscard]] bool is_repeated() const noexcept;

private:
    std::string m_name{};
    std::string m_description{};
    bool m_optional{};
    bool m_repeated{};
};

/**
 * @brief How the help page is laid out.
 */
struct help_format {
    /// @brief The widest a line may get before descriptions wrap. 0 switches wrapping off, and so
    /// does any width that would leave no room after the description column.
    std::size_t line_width{80};

    /// @brief How far every entry of a section is indented from the left margin.
    std::size_t indent{2};

    /// @brief How much blank space separates the widest entry label of a section from the column
    /// its descriptions start at.
    std::size_t column_gap{4};
};

/**
 * @brief Describes one command in full: what it is for, the sub-commands nested under it, the
 * options it accepts, the positional arguments it binds, and how all of that renders as a help
 * page.
 *
 * This is the declaration-aware counterpart of command_spec: where command_spec carries only the
 * keywords the structural parser needs to resolve a command chain, a command_description also
 * carries everything the validator needs to turn a parsed line into typed values, and everything
 * the help page needs to describe the command to the player.
 *
 * @note Options declared on a command are visible to every command nested under it, so an option
 * declared once at the root behaves as a global one. When the same name is declared at several
 * levels, the deepest declaration is the one that applies. The help page keeps the two apart: a
 * command's own options render under "Options:", the inherited ones under "Global options:".
 */
class command_description {
public:
    /// @brief The long name a help option answers to, whether synthesised by with_help_option() or
    /// declared by hand. The validator matches against this very constant, so the declaration side
    /// and the resolution side cannot drift apart.
    static constexpr std::string_view help_option_long_name{"help"};

    /**
     * @brief
     * @param name The exact keyword that names this command (or sub-command) at its level.
     * @param description What the command does, shown as the opening paragraph of its help page.
     */
    explicit command_description(std::string name, std::string description = {});

    /**
     * @brief Sets the text shown as the opening paragraph of the help page.
     * @param description What the command does.
     * @return A reference to this command.
     */
    command_description& with_description(std::string description);

    /**
     * @brief Declares an option this command accepts.
     * @param option The fully-built option.
     * @return A reference to this command.
     */
    command_description& add_option(option_description option);

    /**
     * @brief Registers a nested sub-command directly under this one.
     * @param subcommand The fully-built sub-command tree to attach.
     * @return A reference to this command.
     */
    command_description& add_subcommand(command_description subcommand);

    /**
     * @brief Declares the next positional argument this command binds.
     * @param positional The fully-built argument.
     * @return A reference to this command.
     */
    command_description& add_positional(positional_description positional);

    /**
     * @brief Gives this command a "-h"/"--help" option, which the validator answers by resolving
     * the line with nothing else looked at.
     * @note The option is opt-in, and reaches every sub-command nested under this one. It is always
     * listed last. When an option named "help" was already declared by hand, that one is kept and
     * nothing is synthesised; when some other option already answers to "-h", the synthesised one
     * settles for "--help" alone.
     * @param description What to write next to it, or empty for the stock "Print this help page".
     * @return A reference to this command.
     */
    command_description& with_help_option(std::string description = {});

    /**
     * @brief Takes the synthesised help option away again.
     * @note Harmless when there was none. An option named "help" that was declared by hand is left
     * alone.
     * @return A reference to this command.
     */
    command_description& without_help_option();

    /**
     * @brief
     * @return This command's keyword.
     */
    [[nodiscard]] std::string_view name() const noexcept;

    /**
     * @brief
     * @return What the command does, empty when it was never described.
     */
    [[nodiscard]] std::string_view description() const noexcept;

    /**
     * @brief
     * @return The options declared on this command, in declaration order, with the synthesised help
     * option last when there is one.
     */
    [[nodiscard]] const std::vector<option_description>& options() const noexcept;

    /**
     * @brief
     * @return The positional arguments this command binds, in declaration order.
     */
    [[nodiscard]] const std::vector<positional_description>& positionals() const noexcept;

    /**
     * @brief
     * @return The sub-commands nested directly under this one.
     */
    [[nodiscard]] const std::vector<command_description>& subcommands() const noexcept;

    /**
     * @brief
     * @return true when this command was given a help option, whether synthesised or declared by
     * hand.
     */
    [[nodiscard]] bool has_help_option() const noexcept;

    /**
     * @brief Looks up an option of this command by its long name.
     * @param name The long name, without leading hyphens.
     * @return A pointer to the matching option, or nullptr when none matches. Short names are never
     * considered.
     */
    [[nodiscard]] const option_description* find_option(std::string_view name) const noexcept;

    /**
     * @brief Looks up an option of this command by its short name.
     * @param name The short name, without its leading hyphen.
     * @return A pointer to the matching option, or nullptr when none matches. Long names are never
     * considered.
     */
    [[nodiscard]] const option_description* find_short_option(std::string_view name) const noexcept;

    /**
     * @brief Looks up a directly-nested sub-command by its exact keyword.
     * @param name The keyword to look for.
     * @return A pointer to the matching sub-command, or nullptr when none matches.
     */
    [[nodiscard]] const command_description* find_subcommand(std::string_view name) const noexcept;

    /**
     * @brief Looks up one of this command's positional arguments by name.
     * @param name The name to look for.
     * @return A pointer to the matching argument, or nullptr when none matches.
     */
    [[nodiscard]] const positional_description* find_positional(
        std::string_view name) const noexcept;

    /**
     * @brief Walks a whole command path down from this command.
     * @param command_path The chain as parse_command_line() reports it, this command's own keyword
     * included as its first element.
     * @return A pointer to the deepest command of the chain, or nullptr when the path names
     * something this tree does not hold.
     */
    [[nodiscard]] const command_description* find_command(
        std::span<const std::string> command_path) const noexcept;

    /**
     * @brief Walks a whole command path down from this command, keeping every command it crosses.
     * @note This is what recovers the ancestors of a nested command, which the nodes themselves do
     * not know: option inheritance and the "Global options:" section are both built from it.
     * @param command_path The chain as parse_command_line() reports it, this command's own keyword
     * included as its first element.
     * @return Every command of the chain, this one first and the deepest last, or an empty vector
     * when the path does not resolve.
     */
    [[nodiscard]] std::vector<const command_description*> resolve_path(
        std::span<const std::string> command_path) const;

    /**
     * @brief Renders this command's own help page.
     * @note Being a node of a tree, a command does not know its ancestors: rendered this way its
     * usage line carries its keyword alone and no inherited option shows up. Render from the root
     * with the path overload to get the full usage line and the "Global options:" section.
     * @param format How to lay the page out.
     * @return The rendered page, ending in a single newline.
     */
    [[nodiscard]] std::string help(const help_format& format = {}) const;

    /**
     * @brief Renders the help page of a command nested under this one.
     * @param command_path The chain of the command to describe, this command's own keyword included
     * as its first element.
     * @param format How to lay the page out.
     * @return The rendered page, ending in a single newline, or an empty string when the path does
     * not resolve.
     */
    [[nodiscard]] std::string help(std::span<const std::string> command_path,
                                   const help_format& format = {}) const;

    /**
     * @brief Writes this command's own help page to a stream.
     * @param out Where to write it.
     * @param format How to lay the page out.
     */
    void print_help(std::ostream& out, const help_format& format = {}) const;

    /**
     * @brief Writes the help page of a command nested under this one to a stream.
     * @param out Where to write it.
     * @param command_path The chain of the command to describe, this command's own keyword included
     * as its first element.
     * @param format How to lay the page out.
     */
    void print_help(std::ostream& out, std::span<const std::string> command_path,
                    const help_format& format = {}) const;

    /**
     * @brief Strips this tree down to the keywords the structural parser works with.
     * @note This is the bridge into parse_command_line(), which knows nothing about options.
     * @return The matching command_spec tree.
     */
    [[nodiscard]] command_spec to_command_spec() const;

private:
    std::string m_name{};
    std::string m_description{};
    std::vector<option_description> m_options{};
    std::vector<positional_description> m_positionals{};
    std::vector<command_description> m_subcommands{};

    // Whether a help option is reachable on this command at all, and whether the one in m_options
    // is the synthesised one (so that an option named "help" declared by hand is never dropped by
    // without_help_option()).
    bool m_helpOptionEnabled{};
    bool m_helpOptionSynthesised{};
};

/**
 * @brief Writes a command's own help page to a stream.
 * @param out Where to write it.
 * @param command The command to describe.
 * @return out, so insertions can be chained.
 */
std::ostream& operator<<(std::ostream& out, const command_description& command);

} // namespace tca::engine::platform::terminal
