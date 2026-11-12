// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <algorithm>
#include <any>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

namespace tca::engine::platform::terminal {

/**
 * @brief How many values an option takes when it appears on the command line.
 */
enum class value_arity : std::uint8_t {
    /// @brief A pure flag. It never takes a value, and attaching one ("--verbose=yes") is an error.
    none,

    /// @brief The option must carry a value, either attached ("--port=80") or space-separated
    /// ("--port 80").
    required,

    /// @brief The option may carry an attached value ("--fetch=3"); written bare it falls back to
    /// its implicit value. An option of this arity never adopts a space-separated value, because
    /// "--fetch 3" cannot be told apart from a bare "--fetch" followed by a positional.
    optional
};

/**
 * @brief The type-erased half of a value specification: everything option_description and the
 * validator need from a typed_value<T> without knowing what T is.
 * @note Implemented by typed_value<T>. Callers never build one directly: they go through value<T>()
 * and hand the result to option_description::with_value().
 */
class value_semantics {
public:
    virtual ~value_semantics() = default;

    /**
     * @brief
     * @return How many values the option this specification belongs to takes.
     */
    [[nodiscard]] virtual value_arity arity() const noexcept = 0;

    /**
     * @brief
     * @return true when the option must appear on the command line at all.
     */
    [[nodiscard]] virtual bool is_required() const noexcept = 0;

    /**
     * @brief
     * @return The placeholder the help page renders inside angle brackets, "arg" unless named.
     */
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /**
     * @brief
     * @return true when a value applies while the option is absent from the line.
     */
    [[nodiscard]] virtual bool has_default_value() const noexcept = 0;

    /**
     * @brief
     * @note Precondition: has_default_value() is true.
     * @return The default value as the help page should spell it out.
     */
    [[nodiscard]] virtual std::string default_value_text() const = 0;

    /**
     * @brief
     * @return true when a value applies while the option appears with no value of its own.
     */
    [[nodiscard]] virtual bool has_implicit_value() const noexcept = 0;

    /**
     * @brief
     * @note Precondition: has_implicit_value() is true.
     * @return The implicit value rendered as text.
     */
    [[nodiscard]] virtual std::string implicit_value_text() const = 0;

    /**
     * @brief Converts one command-line value into the declared type.
     * @param text The raw value text, with quoting already resolved by the tokenizer.
     * @return The converted value boxed as the declared type, or an empty optional when text does
     * not name a value of that type (including when it is empty and the type needs characters).
     */
    [[nodiscard]] virtual std::optional<std::any> parse(std::string_view text) const = 0;

    /**
     * @brief
     * @return The default value boxed as the declared type, or an empty optional when there is
     * none.
     */
    [[nodiscard]] virtual std::optional<std::any> boxed_default_value() const = 0;

    /**
     * @brief
     * @return The implicit value boxed as the declared type, or an empty optional when there is
     * none.
     */
    [[nodiscard]] virtual std::optional<std::any> boxed_implicit_value() const = 0;

protected:
    value_semantics() = default;
    value_semantics(const value_semantics&) = default;
    value_semantics(value_semantics&&) noexcept = default;
    value_semantics& operator=(const value_semantics&) = default;
    value_semantics& operator=(value_semantics&&) noexcept = default;
};

/**
 * @brief Declares what an option's value looks like: its type, whether it is mandatory, and the
 * values that stand in for it when it is left out.
 * @note Every setter returns *this so a whole specification reads as one chained expression, the
 * way boost::program_options' value<T>() does.
 */
template <typename T>
class typed_value final : public value_semantics {
public:
    /**
     * @brief Sets the value used when the option does not appear on the command line at all.
     * @param value The value to fall back to.
     * @return A reference to this specification.
     */
    typed_value& default_value(T value);

    /**
     * @brief Sets the value used when the option does not appear, along with the way the help page
     * should spell it out.
     * @param value The value to fall back to.
     * @param text How to render that value, for when the raw one reads badly (e.g. "any free port"
     * instead of "0").
     * @return A reference to this specification.
     */
    typed_value& default_value(T value, std::string text);

    /**
     * @brief Sets the value used when the option appears with no value of its own, which is what
     * makes the value optional.
     * @param value The value to fall back to.
     * @return A reference to this specification.
     */
    typed_value& implicit_value(T value);

    /**
     * @brief Makes the option itself mandatory: leaving it out is an error.
     * @return A reference to this specification.
     */
    typed_value& required();

    /**
     * @brief Names the value placeholder the help page renders, e.g. "ref" for "--track <ref>".
     * @param name The placeholder, without angle brackets.
     * @return A reference to this specification.
     */
    typed_value& with_name(std::string name);

    [[nodiscard]] value_arity arity() const noexcept override;
    [[nodiscard]] bool is_required() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] bool has_default_value() const noexcept override;
    [[nodiscard]] std::string default_value_text() const override;
    [[nodiscard]] bool has_implicit_value() const noexcept override;
    [[nodiscard]] std::string implicit_value_text() const override;
    [[nodiscard]] std::optional<std::any> parse(std::string_view text) const override;
    [[nodiscard]] std::optional<std::any> boxed_default_value() const override;
    [[nodiscard]] std::optional<std::any> boxed_implicit_value() const override;

private:
    // Renders one value the way the help page spells it out. Bools read as words, because
    // std::to_string() would turn them into the "1"/"0" a reader has to decode.
    [[nodiscard]] static std::string to_text(const T& value);

    // The value standing in for an absent option, and the text the help page shows for it. The
    // text is only stored when the caller overrode it; otherwise it is rendered on demand.
    std::optional<T> m_defaultValue{};
    std::optional<std::string> m_defaultValueText{};

    // The value standing in for an option written with no value of its own.
    std::optional<T> m_implicitValue{};

    // The help-page placeholder, "arg" until named.
    std::string m_name{"arg"};

    bool m_required{};
};

/**
 * @brief Starts a value specification for an option that carries a T.
 * @return A fresh, unconstrained specification to chain the wanted traits onto.
 */
template <typename T>
[[nodiscard]] typed_value<T> value();

/**
 * @brief Declares one option of a command: how it is written, what it means, and what its value
 * looks like.
 * @note An option always has a long name ("--verbose") unless it was built with short_only(), and
 * may have a short one. The short name is a whole string rather than a single character so that
 * nmap-style options such as "-sV" can be declared; the parser reads the whole hyphen run as one
 * name and never bundles single letters.
 */
class option_description {
public:
    /**
     * @brief Declares an option written in long form.
     * @param long_name The name as it appears after the "--", e.g. "verbose".
     */
    explicit option_description(std::string long_name);

    /**
     * @brief Declares an option that only ever appears in short form, such as nmap's "-sV".
     * @param short_name The name as it appears after the single '-'.
     * @return The declared option.
     */
    [[nodiscard]] static option_description short_only(std::string short_name);

    /**
     * @brief Gives the option a short form.
     * @param short_name The name as it appears after the single '-', with no leading hyphen.
     * @return A reference to this option.
     */
    option_description& with_short_name(std::string short_name);

    /**
     * @brief Sets the one-line text the help page shows next to the option.
     * @param description What the option does.
     * @return A reference to this option.
     */
    option_description& with_description(std::string description);

    /**
     * @brief Declares that this option carries a value, and what that value looks like.
     * @param value The specification built through value<T>().
     * @return A reference to this option.
     */
    template <typename T>
    option_description& with_value(typed_value<T> value);

    /**
     * @brief
     * @return The long name without its "--", empty for a short-only option.
     */
    [[nodiscard]] std::string_view long_name() const noexcept;

    /**
     * @brief
     * @return true unless the option was built with short_only().
     */
    [[nodiscard]] bool has_long_name() const noexcept;

    /**
     * @brief
     * @return The short name without its '-', empty when the option has none.
     */
    [[nodiscard]] std::string_view short_name() const noexcept;

    /**
     * @brief
     * @return true when the option can be written in short form.
     */
    [[nodiscard]] bool has_short_name() const noexcept;

    /**
     * @brief
     * @return What the option does, empty when it was never described.
     */
    [[nodiscard]] std::string_view description() const noexcept;

    /**
     * @brief
     * @return true when the option was given a value specification, false for a pure flag.
     */
    [[nodiscard]] bool takes_value() const noexcept;

    /**
     * @brief
     * @return How many values the option takes; value_arity::none for a flag.
     */
    [[nodiscard]] value_arity arity() const noexcept;

    /**
     * @brief
     * @return true when leaving the option out of the command line is an error.
     */
    [[nodiscard]] bool is_required() const noexcept;

    /**
     * @brief
     * @return true when a value applies while the option is absent.
     */
    [[nodiscard]] bool has_default_value() const noexcept;

    /**
     * @brief
     * @note Precondition: has_default_value() is true.
     * @return The default value as the help page spells it out.
     */
    [[nodiscard]] std::string default_value_text() const;

    /**
     * @brief
     * @return true when a value applies while the option is written with no value of its own.
     */
    [[nodiscard]] bool has_implicit_value() const noexcept;

    /**
     * @brief
     * @note Precondition: has_implicit_value() is true.
     * @return The implicit value rendered as text.
     */
    [[nodiscard]] std::string implicit_value_text() const;

    /**
     * @brief
     * @return The placeholder the help page renders inside angle brackets ("arg" unless named), or
     * an empty view for a flag, which has nothing to render.
     */
    [[nodiscard]] std::string_view value_name() const noexcept;

    /**
     * @brief
     * @note This is what makes "-v" and "--verbose" land on the same entry in the resolved values.
     * @return The name the resolved value is stored under: the long name when there is one, the
     * short name otherwise.
     */
    [[nodiscard]] std::string_view key() const noexcept;

    /**
     * @brief
     * @return The value specification, or nullptr when the option is a pure flag.
     */
    [[nodiscard]] const value_semantics* value_spec() const noexcept;

private:
    // Left private so that a nameless option cannot be built by accident: short_only() is the only
    // way to get an option with no long name.
    option_description() = default;

    std::string m_longName{};
    std::string m_shortName{};
    std::string m_description{};

    // Shared rather than owned outright because option_description is copied by value into (and
    // along with) command_description, and a specification never changes once built.
    std::shared_ptr<const value_semantics> m_value{};
};

// ---------------------------------------------------------------------------------------------
// Template definitions.
// ---------------------------------------------------------------------------------------------

template <typename T>
typed_value<T>& typed_value<T>::default_value(T value) {
    m_defaultValue = std::move(value);

    // This overload asks for the value to be rendered as it stands, so any rendering left over
    // from an earlier call would spell out a value that is no longer there.
    m_defaultValueText.reset();
    return *this;
}

template <typename T>
typed_value<T>& typed_value<T>::default_value(T value, std::string text) {
    m_defaultValue = std::move(value);
    m_defaultValueText = std::move(text);
    return *this;
}

template <typename T>
typed_value<T>& typed_value<T>::implicit_value(T value) {
    m_implicitValue = std::move(value);
    return *this;
}

template <typename T>
typed_value<T>& typed_value<T>::required() {
    m_required = true;
    return *this;
}

template <typename T>
typed_value<T>& typed_value<T>::with_name(std::string name) {
    m_name = std::move(name);
    return *this;
}

template <typename T>
value_arity typed_value<T>::arity() const noexcept {
    // How many values the option takes has nothing to do with whether the option itself is
    // mandatory: that is what is_required() answers. A specification always carries a value, so
    // the only question left is whether the option may be written bare, and it may exactly when
    // there is an implicit value to fall back to. value_arity::none belongs to a pure flag, which
    // has no specification at all.
    return m_implicitValue.has_value() ? value_arity::optional : value_arity::required;
}

template <typename T>
bool typed_value<T>::is_required() const noexcept {
    return m_required;
}

template <typename T>
std::string_view typed_value<T>::name() const noexcept {
    return m_name;
}

template <typename T>
bool typed_value<T>::has_default_value() const noexcept {
    return m_defaultValue.has_value();
}

template <typename T>
std::string typed_value<T>::to_text(const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else {
        return std::to_string(value);
    }
}

template <typename T>
std::string typed_value<T>::default_value_text() const {
    // A rendering the caller wrote by hand wins over the raw value, since it was asked for
    // precisely because the raw one reads badly.
    if (m_defaultValueText.has_value()) {
        return m_defaultValueText.value();
    }

    return has_default_value() ? to_text(m_defaultValue.value()) : std::string{};
}

template <typename T>
bool typed_value<T>::has_implicit_value() const noexcept {
    return m_implicitValue.has_value();
}

template <typename T>
std::string typed_value<T>::implicit_value_text() const {
    return has_implicit_value() ? to_text(m_implicitValue.value()) : std::string{};
}

template <typename T>
std::optional<std::any> typed_value<T>::parse(const std::string_view text) const {
    static_assert(
        std::is_same_v<T, std::string> || std::is_same_v<T, bool> || std::is_arithmetic_v<T>,
        "typed_value<T>::parse() only knows how to read strings, bools and numbers.");

    // Nothing is rejected up front on the grounds of being empty: a type that needs characters is
    // turned down by its own branch below ("--port=" names no number), while a string is perfectly
    // happy to hold none at all ("--output=" names the empty string).
    if constexpr (std::is_same_v<T, std::string>) {
        // Any run of characters spells out a string, so there is nothing left to check.
        return std::make_any<std::string>(text);
    } else if constexpr (std::is_same_v<T, bool>) {
        // A bool is written as a word rather than a number, and the writing is matched without
        // regard to case, so the text is folded down before being looked up.
        constexpr std::array<std::string_view, 4> trueSpellings{"true", "1", "yes", "on"};
        constexpr std::array<std::string_view, 4> falseSpellings{"false", "0", "no", "off"};

        std::string lowered{text};
        std::ranges::transform(lowered, std::begin(lowered), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

        if (std::ranges::find(trueSpellings, lowered) != std::cend(trueSpellings)) {
            return std::make_any<bool>(true);
        }

        if (std::ranges::find(falseSpellings, lowered) != std::cend(falseSpellings)) {
            return std::make_any<bool>(false);
        }

        // The text names something else entirely, e.g. "--color=maybe".
        return std::nullopt;
    } else {
        // from_chars works on raw characters rather than iterators, so the view is handed over as
        // the pointer pair it wraps.
        const char* const firstCharacter{text.data()};
        const char* const lastCharacter{text.data() + text.size()};

        T parsedValue{};
        const auto [unreadCharacters,
                    errorCode]{std::from_chars(firstCharacter, lastCharacter, parsedValue)};

        // The conversion only counts when the text is a number the type can hold *and* the whole
        // of it was read: anything left over is junk trailing a number, as in "80abc".
        if (errorCode != std::errc{} || unreadCharacters != lastCharacter) {
            return std::nullopt;
        }

        return std::make_any<T>(parsedValue);
    }
}

template <typename T>
std::optional<std::any> typed_value<T>::boxed_default_value() const {
    // Written as a statement rather than a conditional expression on purpose: std::any swallows
    // anything, std::nullopt included, so a ternary here would hand back an engaged optional
    // holding an any that merely *contains* a nullopt_t.
    if (!m_defaultValue.has_value()) {
        return std::nullopt;
    }

    return std::make_any<T>(m_defaultValue.value());
}

template <typename T>
std::optional<std::any> typed_value<T>::boxed_implicit_value() const {
    if (!m_implicitValue.has_value()) {
        return std::nullopt;
    }

    return std::make_any<T>(m_implicitValue.value());
}

template <typename T>
typed_value<T> value() {
    return typed_value<T>{};
}

template <typename T>
option_description& option_description::with_value(typed_value<T> value) {
    m_value = std::static_pointer_cast<const value_semantics>(
        std::make_shared<const typed_value<T>>(std::move(value)));
    return *this;
}

} // namespace tca::engine::platform::terminal
