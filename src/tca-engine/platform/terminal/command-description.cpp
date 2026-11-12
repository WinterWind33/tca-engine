// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/command-description.hpp"

// C++ STL
#include <algorithm>
#include <cstddef>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Project files
#include "platform/terminal/argument-parser.hpp"
#include "platform/terminal/option-description.hpp"

namespace tca::engine::platform::terminal {

namespace {

// The room a "-x, " short form takes up, so that the long name of an option with no short form
// still lines up with the ones that have it.
constexpr std::size_t short_form_width{4};

// How the help option is written, and what it says when the caller did not write anything. The long
// name lives on command_description itself, since the validator has to match against the same one.
constexpr std::string_view help_option_short_name{"h"};
constexpr std::string_view stock_help_description{"Print this help page"};

// The placeholder standing in for whichever sub-command follows on the usage line.
constexpr std::string_view subcommand_placeholder{"<command>"};

// The placeholder standing in for the options a command reaches, on the usage line.
constexpr std::string_view options_placeholder{"[options]"};

// One line of a help section: the label on the left, and the text describing it on the right.
struct help_entry {
    std::string label{};
    std::string description{};
};

// Splits a text into the lines it takes once wrapped at the given width. A width of 0 switches
// wrapping off, and a word wider than the width overflows rather than being broken apart.
std::vector<std::string> wrap_text(const std::string_view text, const std::size_t width) {
    std::vector<std::string> lines{};
    if (text.empty()) {
        return lines;
    }

    if (width == 0) {
        lines.emplace_back(text);
        return lines;
    }

    // Words are packed onto the current line for as long as they fit. The first word of a line is
    // always taken, whatever its length, which is what lets an over-long word overflow instead of
    // being cut in half.
    std::string currentLine{};
    std::size_t wordStart{0};
    while (wordStart < text.size()) {
        const std::size_t wordEnd{std::min(text.find(' ', wordStart), text.size())};
        const std::string_view word{text.substr(wordStart, wordEnd - wordStart)};

        // Runs of spaces produce empty words, which name nothing and are simply skipped.
        if (!word.empty()) {
            if (currentLine.empty()) {
                currentLine = std::string{word};
            } else if (currentLine.size() + 1U + word.size() <= width) {
                currentLine.push_back(' ');
                currentLine.append(word);
            } else {
                lines.push_back(std::move(currentLine));
                currentLine = std::string{word};
            }
        }

        wordStart = wordEnd + 1U;
    }

    if (!currentLine.empty()) {
        lines.push_back(std::move(currentLine));
    }

    return lines;
}

// Appends a parenthesised note to a description, e.g. " (required)", keeping the separating space
// out of the way when there is no description to separate it from.
void append_note(std::string& description, const std::string_view note) {
    if (!description.empty()) {
        description.push_back(' ');
    }

    description.push_back('(');
    description.append(note);
    description.push_back(')');
}

// Renders how an option is written: its short form, its long form, and the value it carries.
std::string make_option_label(const option_description& option) {
    std::string label{};

    if (option.has_short_name()) {
        label.push_back('-');
        label.append(option.short_name());

        if (option.has_long_name()) {
            label.append(", ");
        }
    } else if (option.has_long_name()) {
        // No short form to write, so its width is left blank to keep the long names aligned.
        label.append(short_form_width, ' ');
    }

    if (option.has_long_name()) {
        label.append("--");
        label.append(option.long_name());
    }

    // The brackets are what tell a value that may be left out from one that must be written.
    switch (option.arity()) {
        case value_arity::required:
            label.append(" <");
            label.append(option.value_name());
            label.push_back('>');
            break;
        case value_arity::optional:
            label.append(" [<");
            label.append(option.value_name());
            label.append(">]");
            break;
        case value_arity::none:
            break;
    }

    return label;
}

// Renders what an option does, along with the traits a reader cannot guess from its label alone.
std::string make_option_description(const option_description& option) {
    std::string description{option.description()};

    if (option.is_required()) {
        append_note(description, "required");
    }

    // The implicit value is deliberately left out: the brackets around the placeholder already say
    // the value may be dropped, so spelling it out too would only add noise.
    if (option.has_default_value()) {
        std::string note{"default: "};
        note.append(option.default_value_text());
        append_note(description, note);
    }

    return description;
}

// Renders how a positional argument is written on the usage line.
std::string make_positional_placeholder(const positional_description& positional) {
    std::string placeholder{"<"};
    placeholder.append(positional.name());
    placeholder.push_back('>');

    // The ellipsis belongs to the argument itself, so it stays inside the brackets an optional one
    // is wrapped in: "<paths>..." but "[<extras>...]".
    if (positional.is_repeated()) {
        placeholder.append("...");
    }

    if (positional.is_optional()) {
        placeholder.insert(std::cbegin(placeholder), '[');
        placeholder.push_back(']');
    }

    return placeholder;
}

// Writes one entry of a section: the label, padded out to the description column, then the
// description wrapped under that very column.
void append_entry(std::string& page, const help_entry& entry, const std::size_t description_column,
                  const help_format& format) {
    page.append(format.indent, ' ');
    page.append(entry.label);

    // An entry nobody described stops right after its label: padding it out to the column would
    // leave nothing behind but trailing spaces.
    if (entry.description.empty()) {
        page.push_back('\n');
        return;
    }

    page.append(description_column - format.indent - entry.label.size(), ' ');

    // Wrapping only means something while there is room left after the description column; when
    // there is none, the description simply runs on past the requested width.
    const std::size_t availableWidth{
        format.line_width > description_column ? format.line_width - description_column : 0U};

    bool firstLine{true};
    for (const std::string& line : wrap_text(entry.description, availableWidth)) {
        // Continuation lines hang under the description column rather than under the label.
        if (!firstLine) {
            page.append(description_column, ' ');
        }

        page.append(line);
        page.push_back('\n');
        firstLine = false;
    }
}

// Writes one titled section, or nothing at all when it holds no entry.
void append_section(std::string& page, const std::string_view title,
                    const std::vector<help_entry>& entries, const help_format& format) {
    if (entries.empty()) {
        return;
    }

    // Whatever came before is always closed by a blank line.
    if (!page.empty()) {
        page.push_back('\n');
    }

    page.append(title);
    page.append(":\n");

    // The description column is shared by the whole section, and measured against the widest label
    // of that section alone: sections do not line up with one another.
    std::size_t widestLabel{0};
    for (const help_entry& entry : entries) {
        widestLabel = std::max(widestLabel, entry.label.size());
    }

    const std::size_t descriptionColumn{format.indent + widestLabel + format.column_gap};
    for (const help_entry& entry : entries) {
        append_entry(page, entry, descriptionColumn, format);
    }
}

// Gathers the options a command inherits from its ancestors, in root-first declaration order.
// An ancestor's option is dropped as soon as a command closer to the target declares the same name,
// since the deepest declaration is the one that applies.
std::vector<const option_description*> collect_inherited_options(
    const std::vector<const command_description*>& chain) {
    std::vector<const option_description*> inherited{};

    // Tells whether any command lying below the given depth declares the same name, which is what
    // hides an ancestor's declaration.
    const auto isShadowed{[&chain](const std::size_t ancestorIndex,
                                   const option_description& option) {
        for (std::size_t deeperIndex{ancestorIndex + 1U}; deeperIndex < chain.size();
             ++deeperIndex) {
            const bool declaredDeeper{std::ranges::any_of(
                chain.at(deeperIndex)->options(), [&option](const option_description& candidate) {
                    return candidate.key() == option.key();
                })};

            if (declaredDeeper) {
                return true;
            }
        }

        return false;
    }};

    // The target itself is not an ancestor: its own options belong to the "Options:" section.
    for (std::size_t ancestorIndex{0}; ancestorIndex + 1U < chain.size(); ++ancestorIndex) {
        for (const option_description& option : chain.at(ancestorIndex)->options()) {
            if (!isShadowed(ancestorIndex, option)) {
                inherited.push_back(&option);
            }
        }
    }

    return inherited;
}

// Renders the help page of the deepest command of a chain. The chain is what supplies the usage
// line and the inherited options: a command alone knows nothing of its ancestors.
std::string render_help(const std::vector<const command_description*>& chain,
                        const help_format& format) {
    const command_description& target{*chain.back()};
    const std::vector<const option_description*> inheritedOptions{collect_inherited_options(chain)};

    std::string page{};

    // The description opens the page as a paragraph of its own, wrapped against the whole width
    // since nothing indents it.
    for (const std::string& line : wrap_text(target.description(), format.line_width)) {
        page.append(line);
        page.push_back('\n');
    }

    if (!page.empty()) {
        page.push_back('\n');
    }

    // The usage line spells out the whole chain, then what may follow it, in the order it has to be
    // written: the sub-command first, then the options, then the positional arguments.
    page.append("Usage:\n");
    page.append(format.indent, ' ');

    bool firstKeyword{true};
    for (const command_description* command : chain) {
        if (!firstKeyword) {
            page.push_back(' ');
        }

        page.append(command->name());
        firstKeyword = false;
    }

    if (!target.subcommands().empty()) {
        page.push_back(' ');
        page.append(subcommand_placeholder);
    }

    if (!target.options().empty() || !inheritedOptions.empty()) {
        page.push_back(' ');
        page.append(options_placeholder);
    }

    for (const positional_description& positional : target.positionals()) {
        page.push_back(' ');
        page.append(make_positional_placeholder(positional));
    }

    page.push_back('\n');

    std::vector<help_entry> commandEntries{};
    for (const command_description& subcommand : target.subcommands()) {
        commandEntries.push_back(
            help_entry{std::string{subcommand.name()}, std::string{subcommand.description()}});
    }

    append_section(page, "Commands", commandEntries, format);

    // The usage line already names every positional, so a section that merely repeats them adds
    // nothing: it is only worth writing once at least one of them has something to say.
    std::vector<help_entry> argumentEntries{};
    const bool anyPositionalDescribed{
        std::ranges::any_of(target.positionals(), [](const positional_description& positional) {
            return !positional.description().empty();
        })};

    if (anyPositionalDescribed) {
        for (const positional_description& positional : target.positionals()) {
            std::string label{"<"};
            label.append(positional.name());
            label.push_back('>');
            argumentEntries.push_back(
                help_entry{std::move(label), std::string{positional.description()}});
        }
    }

    append_section(page, "Arguments", argumentEntries, format);

    std::vector<help_entry> optionEntries{};
    for (const option_description& option : target.options()) {
        optionEntries.push_back(
            help_entry{make_option_label(option), make_option_description(option)});
    }

    append_section(page, "Options", optionEntries, format);

    std::vector<help_entry> inheritedEntries{};
    for (const option_description* option : inheritedOptions) {
        inheritedEntries.push_back(
            help_entry{make_option_label(*option), make_option_description(*option)});
    }

    append_section(page, "Global options", inheritedEntries, format);

    return page;
}

} // namespace

positional_description::positional_description(std::string name) : m_name{std::move(name)} {}

positional_description& positional_description::with_description(std::string description) {
    m_description = std::move(description);
    return *this;
}

positional_description& positional_description::optional() {
    m_optional = true;
    return *this;
}

positional_description& positional_description::repeated() {
    m_repeated = true;
    return *this;
}

std::string_view positional_description::name() const noexcept {
    return m_name;
}

std::string_view positional_description::description() const noexcept {
    return m_description;
}

bool positional_description::is_optional() const noexcept {
    return m_optional;
}

bool positional_description::is_repeated() const noexcept {
    return m_repeated;
}

command_description::command_description(std::string name, std::string description)
    : m_name{std::move(name)},
      m_description{std::move(description)} {}

command_description& command_description::with_description(std::string description) {
    m_description = std::move(description);
    return *this;
}

command_description& command_description::add_option(option_description option) {
    // The synthesised help option is always the last entry, so an option declared after it has to
    // slip in front rather than land at the back.
    if (m_helpOptionSynthesised) {
        m_options.insert(std::cend(m_options) - 1, std::move(option));
        return *this;
    }

    m_options.push_back(std::move(option));
    return *this;
}

command_description& command_description::add_subcommand(command_description subcommand) {
    m_subcommands.push_back(std::move(subcommand));
    return *this;
}

command_description& command_description::add_positional(positional_description positional) {
    m_positionals.push_back(std::move(positional));
    return *this;
}

command_description& command_description::with_help_option(std::string description) {
    // Whatever happens to the entry itself, help is reachable on this command from now on.
    m_helpOptionEnabled = true;

    const std::string helpDescription{description.empty() ? std::string{stock_help_description}
                                                          : std::move(description)};

    const auto existingHelp{std::ranges::find_if(m_options, [](const option_description& option) {
        return option.long_name() == help_option_long_name;
    })};

    if (existingHelp != std::end(m_options)) {
        // An option declared by hand belongs to its author and is left exactly as it was; only the
        // one synthesised here may be re-described, which is what keeps a second call from adding a
        // second entry.
        if (m_helpOptionSynthesised) {
            existingHelp->with_description(helpDescription);
        }

        return *this;
    }

    option_description helpOption{std::string{help_option_long_name}};
    helpOption.with_description(helpDescription);

    // "-h" is only taken when nothing else already answers to it: an option declared by hand keeps
    // the short form it was given, and the help option settles for "--help" alone.
    if (find_short_option(help_option_short_name) == nullptr) {
        helpOption.with_short_name(std::string{help_option_short_name});
    }

    m_options.push_back(std::move(helpOption));
    m_helpOptionSynthesised = true;
    return *this;
}

command_description& command_description::without_help_option() {
    // Only the synthesised entry is ours to take away. One declared by hand is left alone, and
    // since it still answers to "--help", help stays reachable.
    if (!m_helpOptionSynthesised) {
        return *this;
    }

    const auto removed{std::ranges::remove_if(m_options, [](const option_description& option) {
        return option.long_name() == help_option_long_name;
    })};
    m_options.erase(std::begin(removed), std::end(removed));

    m_helpOptionSynthesised = false;
    m_helpOptionEnabled = false;
    return *this;
}

std::string_view command_description::name() const noexcept {
    return m_name;
}

std::string_view command_description::description() const noexcept {
    return m_description;
}

const std::vector<option_description>& command_description::options() const noexcept {
    return m_options;
}

const std::vector<positional_description>& command_description::positionals() const noexcept {
    return m_positionals;
}

const std::vector<command_description>& command_description::subcommands() const noexcept {
    return m_subcommands;
}

bool command_description::has_help_option() const noexcept {
    return m_helpOptionEnabled;
}

const option_description* command_description::find_option(std::string_view name) const noexcept {
    // A short-only option has no long name at all, so it must never answer a long-name lookup, not
    // even one made with an empty name.
    const auto found{std::ranges::find_if(m_options, [name](const option_description& option) {
        return option.has_long_name() && option.long_name() == name;
    })};

    return found != std::cend(m_options) ? &(*found) : nullptr;
}

const option_description* command_description::find_short_option(
    std::string_view name) const noexcept {
    const auto found{std::ranges::find_if(m_options, [name](const option_description& option) {
        return option.has_short_name() && option.short_name() == name;
    })};

    return found != std::cend(m_options) ? &(*found) : nullptr;
}

const command_description* command_description::find_subcommand(
    std::string_view name) const noexcept {
    const auto found{std::ranges::find_if(m_subcommands, [name](const command_description& sub) {
        return sub.name() == name;
    })};

    return found != std::cend(m_subcommands) ? &(*found) : nullptr;
}

const positional_description* command_description::find_positional(
    std::string_view name) const noexcept {
    const auto found{
        std::ranges::find_if(m_positionals, [name](const positional_description& positional) {
            return positional.name() == name;
        })};

    return found != std::cend(m_positionals) ? &(*found) : nullptr;
}

const command_description* command_description::find_command(
    std::span<const std::string> command_path) const noexcept {
    // The path is the chain as the parser reports it, so it opens on this very command: a path
    // naming anything else describes a tree this one is no part of.
    if (command_path.empty() || command_path.front() != m_name) {
        return nullptr;
    }

    const command_description* current{this};
    for (const std::string& keyword : command_path.subspan(1)) {
        current = current->find_subcommand(keyword);
        if (current == nullptr) {
            return nullptr;
        }
    }

    return current;
}

std::vector<const command_description*> command_description::resolve_path(
    std::span<const std::string> command_path) const {
    if (command_path.empty() || command_path.front() != m_name) {
        return {};
    }

    // The same walk as find_command(), except that every command crossed on the way down is kept:
    // this is the only way back up to the ancestors, which the nodes themselves do not know.
    std::vector<const command_description*> chain{};
    chain.push_back(this);

    const command_description* current{this};
    for (const std::string& keyword : command_path.subspan(1)) {
        current = current->find_subcommand(keyword);
        if (current == nullptr) {
            return {};
        }

        chain.push_back(current);
    }

    return chain;
}

std::string command_description::help(const help_format& format) const {
    // A chain of one: rendered on its own, a command has no ancestor to write into the usage line
    // and no option to inherit from one.
    const std::vector<const command_description*> chain{this};
    return render_help(chain, format);
}

std::string command_description::help(std::span<const std::string> command_path,
                                      const help_format& format) const {
    const std::vector<const command_description*> chain{resolve_path(command_path)};
    if (chain.empty()) {
        return {};
    }

    return render_help(chain, format);
}

void command_description::print_help(std::ostream& out, const help_format& format) const {
    out << help(format);
}

void command_description::print_help(std::ostream& out, std::span<const std::string> command_path,
                                     const help_format& format) const {
    out << help(command_path, format);
}

command_spec command_description::to_command_spec() const {
    // The structural parser only ever asks how deep the leading keyword chain runs, so everything
    // but the keywords is dropped on the way out.
    command_spec spec{m_name};
    for (const command_description& subcommand : m_subcommands) {
        spec.add_subcommand(subcommand.to_command_spec());
    }

    return spec;
}

std::ostream& operator<<(std::ostream& out, const command_description& command) {
    command.print_help(out);
    return out;
}

} // namespace tca::engine::platform::terminal
