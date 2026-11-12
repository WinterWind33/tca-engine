// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <any>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Project files
#include "platform/terminal/argument-parser.hpp"
#include "platform/terminal/command-description.hpp"

namespace tca::engine::platform::terminal {

/**
 * @brief The reasons validate_command_line() can reject a parsed command line.
 */
enum class validation_error_code : std::uint8_t {
    /// @brief The line named an option no command of the resolved chain declares.
    unknown_option,

    /// @brief A value was attached to an option that takes none.
    unexpected_value,

    /// @brief An option that must carry a value was written without one, and no value followed it
    /// that could be adopted.
    missing_value,

    /// @brief An option's value could not be converted to its declared type.
    invalid_value,

    /// @brief An option marked required() never appeared.
    missing_required_option,

    /// @brief A positional argument that is not optional was left out.
    missing_required_positional,

    /// @brief More positional arguments were written than the command declares.
    too_many_positionals
};

/**
 * @brief Describes why validate_command_line() rejected a command line.
 * @note Only the first problem found is reported, the way parse_error does.
 */
struct validation_error {
    /// @brief Why validation failed.
    validation_error_code code{};

    /// @brief The option or positional the failure concerns, keyed the way
    /// option_description::key() spells it. Empty when no single declaration is to blame.
    std::string name{};

    /// @brief The offending text, when the failure was caused by one (a value that would not
    /// convert, a positional nobody declared). Empty otherwise.
    std::string text{};

    /// @brief The offset, in the original line, most relevant to the failure. 0 when the failure is
    /// about something that is simply not there, since there is nothing to point a caret at.
    std::size_t position{};

    bool operator==(const validation_error&) const noexcept = default;
};

/**
 * @brief The options of a validated command line, converted to their declared types and keyed by
 * option_description::key().
 * @note An option with a default value is present here even when the line never mentioned it; the
 * occurrence count is what tells the two apart.
 */
class option_values {
public:
    /**
     * @brief
     * @return true when no option resolved to anything at all.
     */
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief
     * @param name The option's key.
     * @return true when a value resolved for that option, whether written on the line or defaulted.
     */
    [[nodiscard]] bool contains(std::string_view name) const noexcept;

    /**
     * @brief
     * @param name The option's key.
     * @return How many times the option was written on the line: 0 when its value came from a
     * default, and more than 1 when it was repeated.
     */
    [[nodiscard]] std::size_t count(std::string_view name) const noexcept;

    /**
     * @brief Reads a resolved value.
     * @note Precondition: contains(name) is true and T is the type the option was declared with.
     * @param name The option's key.
     * @return The converted value.
     */
    template <typename T>
    [[nodiscard]] T get(std::string_view name) const;

    /**
     * @brief Reads a resolved value without demanding it be there.
     * @param name The option's key.
     * @return The converted value, or an empty optional when nothing resolved under that key or the
     * option was declared with some other type.
     */
    template <typename T>
    [[nodiscard]] std::optional<T> try_get(std::string_view name) const;

    /**
     * @brief Records a resolved value.
     * @note Meant for validate_command_line(), which is the only thing that fills an option set.
     * @param name The option's key.
     * @param value The converted value, boxed as its declared type.
     * @param occurrences How many times the option was written on the line, 0 for a default.
     */
    void set(std::string name, std::any value, std::size_t occurrences);

private:
    struct entry {
        std::any value{};
        std::size_t occurrences{};
    };

    // Transparent comparator so lookups can take a string_view without building a string first.
    std::map<std::string, entry, std::less<>> m_values{};
};

/**
 * @brief One command line, resolved against the declarations: which command was invoked, its typed
 * option values and its named positional arguments.
 */
class resolved_command_line {
public:
    /**
     * @brief
     * @return The resolved command chain, carried over from the parser.
     */
    [[nodiscard]] const std::vector<std::string>& command_path() const noexcept;

    /**
     * @brief
     * @note When this is true nothing else on the line was looked at, so no other accessor carries
     * anything meaningful: the caller is meant to print the help page of command_path() and stop.
     * @return true when a help option was written on the line.
     */
    [[nodiscard]] bool help_requested() const noexcept;

    /**
     * @brief
     * @return Every option that resolved to a value.
     */
    [[nodiscard]] const option_values& options() const noexcept;

    /**
     * @brief Reads the values bound to one declared positional argument.
     * @param name The name the argument was declared with.
     * @return Its values: none when it was left out, one for an ordinary argument, and as many as
     * were written for a repeated one. Empty for a name nobody declared.
     */
    [[nodiscard]] std::span<const std::string> positionals(std::string_view name) const noexcept;

    /**
     * @brief
     * @note This is what a free-form command such as "echo", which declares no positional at all,
     * reads its arguments from.
     * @return Every positional argument of the line, in order, minus the ones adopted as option
     * values.
     */
    [[nodiscard]] const std::vector<std::string>& all_positionals() const noexcept;

    /**
     * @brief Sets the resolved command chain.
     * @note Meant for validate_command_line(), which is the only thing that fills a resolved line.
     * @param command_path The chain, root first.
     */
    void set_command_path(std::vector<std::string> command_path);

    /**
     * @brief Records that the line asked for the help page.
     * @param requested Whether help was asked for.
     */
    void set_help_requested(bool requested) noexcept;

    /**
     * @brief Sets the resolved option values.
     * @param values The whole option set.
     */
    void set_options(option_values values);

    /**
     * @brief Binds a declared positional argument to the values it took.
     * @param name The name the argument was declared with.
     * @param values Its values, in order.
     */
    void bind_positional(std::string name, std::vector<std::string> values);

    /**
     * @brief Sets the flat list of positional arguments.
     * @param values Every positional of the line, in order.
     */
    void set_all_positionals(std::vector<std::string> values);

private:
    std::vector<std::string> m_commandPath{};
    option_values m_options{};

    // Transparent comparator so lookups can take a string_view without building a string first.
    std::map<std::string, std::vector<std::string>, std::less<>> m_positionals{};

    std::vector<std::string> m_allPositionals{};
    bool m_helpRequested{};
};

/**
 * @brief The outcome of validate_command_line(): either the resolved command line or the first
 * problem found. Hand-rolled rather than std::expected since the engine targets C++20.
 */
class validation_result {
public:
    /**
     * @brief
     * @param resolved The resolved command line.
     * @return A successful result wrapping resolved.
     */
    [[nodiscard]] static validation_result success(resolved_command_line&& resolved) noexcept;

    /**
     * @brief
     * @param error The reason validation failed.
     * @return A failed result wrapping error.
     */
    [[nodiscard]] static validation_result failure(validation_error&& error) noexcept;

    /**
     * @brief
     * @return true if validation succeeded and value() can be called.
     */
    [[nodiscard]] bool has_value() const noexcept;

    /**
     * @brief
     * @note Precondition: has_value() is true.
     * @return The resolved command line.
     */
    [[nodiscard]] const resolved_command_line& value() const noexcept;

    /**
     * @brief
     * @note Precondition: has_value() is false.
     * @return The reason validation failed.
     */
    [[nodiscard]] const validation_error& error() const noexcept;

private:
    explicit validation_result(
        std::variant<resolved_command_line, validation_error> result) noexcept;

    std::variant<resolved_command_line, validation_error> m_result;
};

/**
 * @brief Checks a parsed command line against the declarations and turns it into typed values.
 *
 * The command chain parse_command_line() resolved is walked down from root, giving the commands
 * whose declarations apply. Each option written on the line is then looked up along that chain from
 * the deepest command upwards, so an option declared at the root reaches every sub-command and the
 * deepest declaration of a name shadows the ones above it. Long forms only ever match long names
 * and short forms only short ones.
 *
 * A value is bound to its option in one of three ways:
 * - attached, as in "--port=8080" or "-p=8080";
 * - space-separated, as in "--port 8080": an option of value_arity::required with no attached value
 *   adopts the positional that directly follows it, which the offsets recorded by the parser are
 *   what makes recoverable. Nothing is adopted when another option sits in between ("--port -v
 * 80"), when the line ends there, or when the candidate came after a "--" terminator. An option of
 *   value_arity::optional never adopts, since "--fetch 3" cannot be told apart from a bare
 *   "--fetch" followed by a positional; write "--fetch=3" for those. Note that "--port -1" adopts
 *   nothing either, the parser having read "-1" as a short option;
 * - implicitly, when an option of value_arity::optional is written bare.
 * An option that is absent altogether takes its default value when it declared one, and is simply
 * missing otherwise. Repeating an option keeps the last value written and counts every occurrence.
 *
 * Whatever positional is left after adoption is bound to the declared positionals in order, the
 * last of which takes everything remaining when it was declared repeated(). A command that declares
 * no positional at all lets every one of them through unchecked, which is what keeps a free-form
 * command such as "echo" usable.
 *
 * @note A help option written anywhere on the line short-circuits all of the above: the result is a
 * success carrying nothing but the command path and help_requested(), with no required option, no
 * required positional and no conversion checked. Printing a help page must not be made impossible
 * by the very mistake the player is trying to look up.
 * @param root The command tree the line was parsed against, from its root.
 * @param parsed The structured command line to check.
 * @return The resolved command line, or the first problem found.
 */
[[nodiscard]] validation_result validate_command_line(const command_description& root,
                                                      const parsed_command_line& parsed);

// ---------------------------------------------------------------------------------------------
// Template definitions. They live here rather than in command-validator.cpp because a template is
// only compiled where it is instantiated, which is in whatever translation unit reads a value.
// ---------------------------------------------------------------------------------------------

template <typename T>
T option_values::get(std::string_view name) const {
    // Both preconditions hold by contract, so neither the lookup nor the unboxing is checked here;
    // try_get() is the guarded way in.
    return std::any_cast<T>(m_values.find(name)->second.value);
}

template <typename T>
std::optional<T> option_values::try_get(std::string_view name) const {
    const auto valueIt{m_values.find(name)};
    if (valueIt == std::cend(m_values)) {
        return std::nullopt;
    }

    // The pointer form of any_cast answers with nullptr rather than throwing, which is what turns
    // "the option was declared with some other type" into an empty optional.
    const T* const value{std::any_cast<T>(&valueIt->second.value)};
    if (value == nullptr) {
        return std::nullopt;
    }

    return *value;
}

} // namespace tca::engine::platform::terminal
