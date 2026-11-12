// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/command-validator.hpp"

// C++ STL
#include <algorithm>
#include <any>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// Project files
#include "platform/terminal/argument-parser.hpp"
#include "platform/terminal/command-description.hpp"
#include "platform/terminal/option-description.hpp"

namespace tca::engine::platform::terminal {

namespace {

// One option occurrence resolved against the declarations: what declares it, and which command of
// the chain the declaration was found on. The command matters because only the command that owns a
// declaration knows whether it is the help option.
struct option_lookup {
    const option_description* declaration{};
    const command_description* command{};
};

// Looks one option occurrence up along the command chain, deepest command first. Walking upwards is
// what makes an option declared at the root reach every sub-command, while stopping at the first
// match is what makes the deepest declaration of a name shadow the ones above it. Long forms only
// ever match long names, short forms only short ones.
[[nodiscard]] option_lookup find_declaration(const std::vector<const command_description*>& chain,
                                             const parsed_option& option) {
    for (const command_description* const command : chain | std::views::reverse) {
        const option_description* const found{option.long_form
                                                  ? command->find_option(option.name)
                                                  : command->find_short_option(option.name)};

        if (found != nullptr) {
            return option_lookup{.declaration = found, .command = command};
        }
    }

    return option_lookup{};
}

// Tells whether a resolved occurrence is the help option, which is the one thing that
// short-circuits the whole validation.
[[nodiscard]] bool is_help_option(const option_lookup& lookup) noexcept {
    return lookup.declaration != nullptr && lookup.command->has_help_option() &&
           lookup.declaration->has_long_name() &&
           lookup.declaration->long_name() == command_description::help_option_long_name;
}

// Gathers every option the chain reaches, root-first in declaration order, keeping for each name
// the deepest declaration of it. This is what the defaults are seeded from and what the required
// options are checked against, so it must agree with find_declaration() on which declaration wins.
[[nodiscard]] std::vector<const option_description*> collect_visible_options(
    const std::vector<const command_description*>& chain) {
    std::vector<const option_description*> visible{};

    // Where in the result each name already sits, so that a deeper declaration replaces the one it
    // shadows instead of being appended next to it.
    std::map<std::string, std::size_t, std::less<>> positionsByKey{};

    for (const command_description* const command : chain) {
        for (const option_description& option : command->options()) {
            const auto knownIt{positionsByKey.find(option.key())};
            if (knownIt != std::cend(positionsByKey)) {
                visible.at(knownIt->second) = &option;
                continue;
            }

            positionsByKey.emplace(std::string{option.key()}, visible.size());
            visible.push_back(&option);
        }
    }

    return visible;
}

// Finds the positional an option of value_arity::required may adopt as its space-separated value,
// as an index into parsed.positionals. Only the positional directly following the option qualifies:
// nothing is adopted across another option, from past a "--" terminator, or from a positional an
// earlier occurrence already took.
[[nodiscard]] std::optional<std::size_t> find_adoptable_positional(
    const parsed_command_line& parsed, const std::size_t option_index,
    const std::vector<bool>& adopted) {
    const parsed_option& option{parsed.options.at(option_index)};

    // Options and positionals are both recorded in line order, so the first positional written past
    // the option is the only candidate there is.
    const auto candidateIt{
        std::ranges::find_if(parsed.positionals, [&option](const parsed_positional& positional) {
            return positional.position > option.position;
        })};

    if (candidateIt == std::cend(parsed.positionals) || candidateIt->after_terminator) {
        return std::nullopt;
    }

    const auto candidateIndex{
        static_cast<std::size_t>(std::distance(std::cbegin(parsed.positionals), candidateIt))};

    if (adopted.at(candidateIndex)) {
        return std::nullopt;
    }

    // Another option written in between means the candidate does not directly follow this one, as
    // in "--port --verbose 8080".
    const std::size_t nextOptionIndex{option_index + 1U};
    if (nextOptionIndex < parsed.options.size() &&
        parsed.options.at(nextOptionIndex).position < candidateIt->position) {
        return std::nullopt;
    }

    return candidateIndex;
}

// Builds a failure, which is verbose enough to be worth naming.
[[nodiscard]] validation_result fail(const validation_error_code code, const std::string_view name,
                                     const std::string_view text, const std::size_t position) {
    return validation_result::failure(validation_error{
        .code = code, .name = std::string{name}, .text = std::string{text}, .position = position});
}

} // namespace

bool option_values::empty() const noexcept {
    return m_values.empty();
}

bool option_values::contains(std::string_view name) const noexcept {
    return m_values.contains(name);
}

std::size_t option_values::count(std::string_view name) const noexcept {
    const auto valueIt{m_values.find(name)};
    return valueIt != std::cend(m_values) ? valueIt->second.occurrences : 0;
}

void option_values::set(std::string name, std::any value, std::size_t occurrences) {
    m_values.insert_or_assign(std::move(name),
                              entry{.value = std::move(value), .occurrences = occurrences});
}

const std::vector<std::string>& resolved_command_line::command_path() const noexcept {
    return m_commandPath;
}

bool resolved_command_line::help_requested() const noexcept {
    return m_helpRequested;
}

const option_values& resolved_command_line::options() const noexcept {
    return m_options;
}

std::span<const std::string> resolved_command_line::positionals(
    std::string_view name) const noexcept {
    const auto positionalIt{m_positionals.find(name)};
    return positionalIt != std::cend(m_positionals)
               ? std::span<const std::string>{positionalIt->second}
               : std::span<const std::string>{};
}

const std::vector<std::string>& resolved_command_line::all_positionals() const noexcept {
    return m_allPositionals;
}

void resolved_command_line::set_command_path(std::vector<std::string> command_path) {
    m_commandPath = std::move(command_path);
}

void resolved_command_line::set_help_requested(bool requested) noexcept {
    m_helpRequested = requested;
}

void resolved_command_line::set_options(option_values values) {
    m_options = std::move(values);
}

void resolved_command_line::bind_positional(std::string name, std::vector<std::string> values) {
    m_positionals.insert_or_assign(std::move(name), std::move(values));
}

void resolved_command_line::set_all_positionals(std::vector<std::string> values) {
    m_allPositionals = std::move(values);
}

validation_result::validation_result(
    std::variant<resolved_command_line, validation_error> result) noexcept
    : m_result{std::move(result)} {}

validation_result validation_result::success(resolved_command_line&& resolved) noexcept {
    return validation_result{
        std::variant<resolved_command_line, validation_error>{std::move(resolved)}};
}

validation_result validation_result::failure(validation_error&& error) noexcept {
    return validation_result{
        std::variant<resolved_command_line, validation_error>{std::move(error)}};
}

bool validation_result::has_value() const noexcept {
    return std::holds_alternative<resolved_command_line>(m_result);
}

const resolved_command_line& validation_result::value() const noexcept {
    return std::get<resolved_command_line>(m_result);
}

const validation_error& validation_result::error() const noexcept {
    return std::get<validation_error>(m_result);
}

validation_result validate_command_line(const command_description& root,
                                        const parsed_command_line& parsed) {
    // 1. The chain the line resolved to, root first and the invoked command last, is what every
    // declaration is looked up through.
    std::vector<const command_description*> chain{root.resolve_path(parsed.command_path)};
    if (chain.empty()) {
        // The parser resolved the path against this very tree, so a path that does not resolve
        // cannot normally happen; falling back to the root keeps the walk below well-defined.
        chain.push_back(&root);
    }

    // 2. A help option written anywhere on the line wins over everything else, so it is answered
    // before a single value is looked at: printing a help page must not be made impossible by the
    // very mistake the player is trying to look up.
    for (const parsed_option& option : parsed.options) {
        if (is_help_option(find_declaration(chain, option))) {
            resolved_command_line helped{};
            helped.set_command_path(parsed.command_path);
            helped.set_help_requested(true);
            return validation_result::success(std::move(helped));
        }
    }

    // Which positionals were taken as an option's space-separated value, and so stopped being
    // positionals at all.
    std::vector<bool> adopted(parsed.positionals.size(), false);

    // 3. Every occurrence, in the order it was written, is bound to the value it carries.
    option_values values{};
    for (std::size_t optionIndex{0}; optionIndex < parsed.options.size(); ++optionIndex) {
        const parsed_option& option{parsed.options.at(optionIndex)};

        const option_lookup lookup{find_declaration(chain, option)};
        if (lookup.declaration == nullptr) {
            return fail(validation_error_code::unknown_option, option.name, {}, option.position);
        }

        const option_description& declaration{*lookup.declaration};
        const std::string_view key{declaration.key()};

        // A pure flag carries nothing: it resolves to true and is only ever counted, which is what
        // lets "-v -v -v" read as a verbosity level.
        if (declaration.arity() == value_arity::none) {
            if (option.value.has_value()) {
                return fail(validation_error_code::unexpected_value, key, option.value.value(),
                            option.position);
            }

            values.set(std::string{key}, std::make_any<bool>(true), values.count(key) + 1U);
            continue;
        }

        const value_semantics& specification{*declaration.value_spec()};

        // The attached value comes first; failing that, an option that must carry a value adopts
        // the positional directly following it, which is what makes "--port 8080" work.
        std::optional<std::string> text{option.value};
        if (!text.has_value() && declaration.arity() == value_arity::required) {
            const std::optional<std::size_t> adoptable{
                find_adoptable_positional(parsed, optionIndex, adopted)};

            if (adoptable.has_value()) {
                adopted.at(adoptable.value()) = true;
                text = parsed.positionals.at(adoptable.value()).value;
            }
        }

        if (!text.has_value()) {
            // Nothing was written and nothing could be adopted, so the only value left is the
            // implicit one an option of value_arity::optional falls back to.
            std::optional<std::any> implicitValue{specification.boxed_implicit_value()};
            if (!implicitValue.has_value()) {
                return fail(validation_error_code::missing_value, key, {}, option.position);
            }

            values.set(std::string{key}, std::move(implicitValue).value(), values.count(key) + 1U);
            continue;
        }

        std::optional<std::any> converted{specification.parse(text.value())};
        if (!converted.has_value()) {
            return fail(validation_error_code::invalid_value, key, text.value(), option.position);
        }

        // A repeated option keeps the last value written, every occurrence still being counted.
        values.set(std::string{key}, std::move(converted).value(), values.count(key) + 1U);
    }

    // 4. Whatever the line left out either stands in for itself with a default value, or is missing
    // outright when it had to be written.
    for (const option_description* const declaration : collect_visible_options(chain)) {
        const std::string_view key{declaration->key()};
        if (values.contains(key)) {
            continue;
        }

        if (declaration->is_required()) {
            // Nothing on the line to point a caret at, hence a position of 0.
            return fail(validation_error_code::missing_required_option, key, {}, 0);
        }

        if (declaration->has_default_value()) {
            values.set(std::string{key}, declaration->value_spec()->boxed_default_value().value(),
                       0);
        }
    }

    resolved_command_line resolved{};
    resolved.set_command_path(parsed.command_path);
    resolved.set_options(std::move(values));

    // Everything that survived adoption is what the declared positionals are bound from, and what
    // a free-form command such as "echo" reads its arguments from.
    std::vector<const parsed_positional*> remaining{};
    for (std::size_t positionalIndex{0}; positionalIndex < parsed.positionals.size();
         ++positionalIndex) {
        if (!adopted.at(positionalIndex)) {
            remaining.push_back(&parsed.positionals.at(positionalIndex));
        }
    }

    std::vector<std::string> flattened{};
    flattened.reserve(remaining.size());
    for (const parsed_positional* const positional : remaining) {
        flattened.push_back(positional->value);
    }

    // 5. The declared positionals take those values in declaration order, a repeated one swallowing
    // everything left over.
    const command_description& target{*chain.back()};
    std::size_t nextValueIndex{0};
    for (const positional_description& declaration : target.positionals()) {
        std::vector<std::string> bound{};
        if (declaration.is_repeated()) {
            for (; nextValueIndex < flattened.size(); ++nextValueIndex) {
                bound.push_back(flattened.at(nextValueIndex));
            }
        } else if (nextValueIndex < flattened.size()) {
            bound.push_back(flattened.at(nextValueIndex));
            ++nextValueIndex;
        }

        if (bound.empty() && !declaration.is_optional()) {
            return fail(validation_error_code::missing_required_positional, declaration.name(), {},
                        0);
        }

        resolved.bind_positional(std::string{declaration.name()}, std::move(bound));
    }

    // A command that declares no positional at all lets every one of them through unchecked, which
    // is what keeps a free-form command usable; one that declares some has to account for them all.
    if (!target.positionals().empty() && nextValueIndex < flattened.size()) {
        return fail(validation_error_code::too_many_positionals, {}, flattened.at(nextValueIndex),
                    remaining.at(nextValueIndex)->position);
    }

    resolved.set_all_positionals(std::move(flattened));
    return validation_result::success(std::move(resolved));
}

} // namespace tca::engine::platform::terminal
