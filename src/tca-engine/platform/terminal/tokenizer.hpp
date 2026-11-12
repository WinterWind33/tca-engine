// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tca::engine::platform::terminal {

/**
 * @brief A single lexical token extracted from a terminal command line.
 */
struct token {
    /// @brief The token's textual value, with quoting and escaping already resolved.
    std::string value;

    /// @brief The offset, in the original line, of the first character that produced this token
    /// (the opening quote character if the token started with one, otherwise the first raw
    /// character). Meant to let callers point a caret back at the source line (e.g. "unknown
    /// argument at column N").
    std::size_t position{};

    bool operator==(const token&) const noexcept = default;
};

/**
 * @brief The reasons tokenize_command_line() can fail to fully tokenize a line.
 */
enum class tokenizer_error_code : std::uint8_t {
    /// @brief A '"' was opened but never closed before the end of the line.
    unterminated_double_quote,

    /// @brief A '\'' was opened but never closed before the end of the line.
    unterminated_single_quote,

    /// @brief A '\\' escape character appeared with no following character to escape (i.e. it was
    /// the very last character of the line).
    dangling_escape
};

/**
 * @brief Describes why tokenize_command_line() failed.
 */
struct tokenizer_error {
    /// @brief Why tokenization failed.
    tokenizer_error_code code{};

    /// @brief The offset, in the original line, of the character that caused the failure (the
    /// opening quote for an unterminated quote, the backslash for a dangling escape).
    std::size_t position{};

    constexpr bool operator==(const tokenizer_error&) const noexcept = default;
};

/**
 * @brief The outcome of tokenize_command_line(): either the resolved list of tokens or the error
 * that stopped tokenization. Hand-rolled rather than std::expected since the engine targets C++20.
 */
class tokenize_result {
public:
    /**
     * @brief
     * @param tokens The tokens extracted from the line.
     * @return A successful result wrapping tokens.
     */
    [[nodiscard]] static tokenize_result success(std::vector<token>&& tokens) noexcept;

    /**
     * @brief
     * @param error The reason tokenization failed.
     * @return A failed result wrapping error.
     */
    [[nodiscard]] static tokenize_result failure(tokenizer_error&& error) noexcept;

    /**
     * @brief
     * @return true if tokenization succeeded and value() can be called.
     */
    [[nodiscard]] bool has_value() const noexcept;

    /**
     * @brief
     * @note Precondition: has_value() is true.
     * @return The tokens extracted from the line.
     */
    [[nodiscard]] const std::vector<token>& value() const noexcept;

    /**
     * @brief
     * @note Precondition: has_value() is false.
     * @return The reason tokenization failed.
     */
    [[nodiscard]] const tokenizer_error& error() const noexcept;

private:
    explicit tokenize_result(std::variant<std::vector<token>, tokenizer_error> result) noexcept;

    std::variant<std::vector<token>, tokenizer_error> m_result;
};

/**
 * @brief Splits a single terminal command line into tokens, following POSIX-shell-like rules:
 * - Tokens are separated by runs of whitespace (space or tab); leading/trailing whitespace is
 *   ignored and never produces empty tokens.
 * - An unquoted '#' that begins a token (i.e. at the start of the line, or immediately after
 *   whitespace) starts a comment that runs to the end of the line, matching common shell
 *   convention; a '#' occurring in the middle of a token is a literal character.
 * - Text inside "..." is taken literally except '\"' and '\\', which are unescaped to '"' and '\'
 *   respectively; any other '\' inside double quotes is kept as-is (both characters survive).
 * - Text inside '...' is taken completely literally; there is no escape mechanism inside single
 *   quotes, and '#' has no special meaning there either.
 * - Outside quotes, '\' escapes the very next character literally (so "\ " embeds a space in a
 *   token instead of splitting it, and "\#" embeds a literal '#' instead of starting a comment).
 * - Adjacent quoted/unquoted segments with no whitespace between them concatenate into a single
 *   token (e.g. foo"bar baz"qux is one token).
 * - A quoted empty string (e.g. "" or '') still produces a token, unlike bare whitespace.
 *
 * @param line The raw command line to tokenize.
 * @return The extracted tokens, or the error that stopped tokenization.
 */
[[nodiscard]] tokenize_result tokenize_command_line(std::string_view line);

} // namespace tca::engine::platform::terminal
