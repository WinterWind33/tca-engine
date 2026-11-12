// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/argument-parser.hpp"

// C++ STL
#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// Terminal
#include "platform/terminal/tokenizer.hpp"

namespace tca::engine::platform::terminal {

// === command_spec ===

command_spec::command_spec(std::string name) : m_name{std::move(name)} {}

command_spec& command_spec::add_subcommand(command_spec subcommand) {
    m_subcommands.push_back(std::move(subcommand));
    return *this;
}

std::string_view command_spec::name() const noexcept {
    return m_name;
}

const std::vector<command_spec>& command_spec::subcommands() const noexcept {
    return m_subcommands;
}

const command_spec* command_spec::find_subcommand(const std::string_view name) const noexcept {
    if (const auto it{std::ranges::find(m_subcommands, name, &command_spec::name)};
        it != std::ranges::cend(m_subcommands)) {
        return std::to_address(it);
    }
    return nullptr;
}

// === parse_result ===

parse_result::parse_result(std::variant<parsed_command_line, parse_error> result) noexcept
    : m_result{std::move(result)} {}

parse_result parse_result::success(parsed_command_line&& parsed) noexcept {
    return parse_result{std::variant<parsed_command_line, parse_error>{std::move(parsed)}};
}

parse_result parse_result::failure(parse_error&& error) noexcept {
    return parse_result{std::variant<parsed_command_line, parse_error>{std::move(error)}};
}

bool parse_result::has_value() const noexcept {
    return std::holds_alternative<parsed_command_line>(m_result);
}

const parsed_command_line& parse_result::value() const noexcept {
    return std::get<parsed_command_line>(m_result);
}

const parse_error& parse_result::error() const noexcept {
    return std::get<parse_error>(m_result);
}

// === parse_command_line ===

namespace {

/// @brief The length of the hyphen run that introduces a short option ("-x").
constexpr std::size_t shortOptionHyphenCount{1};

/// @brief The length of the hyphen run that introduces a long option ("--foo"). Also the longest
/// run that means anything: three or more hyphens are not an option starter.
constexpr std::size_t longOptionHyphenCount{2};

} // namespace

parse_result parse_command_line(const std::span<const command_spec> commands,
                                const std::string_view line) {
    // Firstly, we need to tokenize the line so that we can ease up the parsing later.
    const tokenize_result tokenizeResult{tokenize_command_line(line)};
    if (!tokenizeResult.has_value()) {
        auto error{tokenizeResult.error()};
        return parse_result::failure(parse_error{.code = parse_error_code::tokenization_failed,
                                                 .position = error.position,
                                                 .tokenizer = error});
    }

    const auto& tokens{tokenizeResult.value()};
    if (tokens.empty()) {
        return parse_result::failure(parse_error{.code = parse_error_code::empty_input});
    }

    // The first token must name one of the known commands, otherwise we cannot dispatch at all.
    // Duplicate keywords are not diagnosed here: the first spec that matches wins.
    const token& head{tokens.front()};
    const auto commandIt{std::ranges::find_if(commands, [&head](const command_spec& command) {
        return head.value == command.name();
    })};
    if (commandIt == std::ranges::cend(commands)) {
        return parse_result::failure(
            parse_error{.code = parse_error_code::unknown_command, .position = head.position});
    }

    std::vector<std::string> commandPath{};
    std::vector<parsed_option> parsedOptions{};
    std::vector<parsed_positional> positionals{};
    positionals.reserve(tokens.size() - 1);

    commandPath.push_back(std::string{commandIt->name()});

    bool allPositionals{};
    bool stopSubCommandsSearch{};
    const command_spec* currentSubCommand{std::to_address(commandIt)};
    for (const token& currentToken : tokens | std::views::drop(1)) {
        const std::string_view tokenValue{currentToken.value};

        // Past a "--" terminator nothing is interpreted any more, not even a later "--".
        if (allPositionals) {
            positionals.push_back(parsed_positional{.value = currentToken.value,
                                                    .position = currentToken.position,
                                                    .after_terminator = true});
            continue;
        }

        if (tokenValue == "--") {
            allPositionals = true;
            continue;
        }

        // Find where the leading run of hyphens ends: its length is what tells an option from a
        // positional. Only a single '-' (short) or a double '--' (long) starts an option, and the
        // run must be followed by something. So a bare '-' (the conventional stdin placeholder) and
        // any run of three or more hyphens ("---foo", "----") are not options at all and fall
        // through to the positionals below.
        const std::size_t hyphenCount{tokenValue.find_first_not_of('-')};
        if (hyphenCount != std::string_view::npos && hyphenCount >= shortOptionHyphenCount &&
            hyphenCount <= longOptionHyphenCount) {
            // Stop subcommands to be searched even if a valid value could be found.
            // From now on, all the subcommands are positionals.
            stopSubCommandsSearch = true;

            // Split on the first '=' so we can have the option name and its value. Without an '='
            // the option is a bare flag and carries no value at all.
            const std::string_view optionBody{tokenValue.substr(hyphenCount)};
            const std::size_t firstEqual{optionBody.find('=')};

            std::optional<std::string> optValue{};
            if (firstEqual != std::string_view::npos) {
                optValue = std::string{optionBody.substr(firstEqual + 1)};
            }

            // This is an option for sure.
            parsedOptions.push_back(parsed_option{
                .name = std::string{optionBody.substr(0, firstEqual)},
                .long_form = (hyphenCount == longOptionHyphenCount),
                .value = std::move(optValue),
                .position = currentToken.position,
            });

            continue;
        }

        if (!stopSubCommandsSearch) {
            if (const command_spec* const subCommand{
                    currentSubCommand->find_subcommand(currentToken.value)};
                subCommand != nullptr) {
                currentSubCommand = subCommand;
                commandPath.push_back(std::string{currentSubCommand->name()});
                continue;
            }
        }

        stopSubCommandsSearch = true;
        positionals.push_back(parsed_positional{.value = currentToken.value,
                                                .position = currentToken.position,
                                                .after_terminator = false});
    }

    return parse_result::success(parsed_command_line{
        .command_path = std::move(commandPath),
        .options = std::move(parsedOptions),
        .positionals = std::move(positionals),
    });
}

} // namespace tca::engine::platform::terminal
