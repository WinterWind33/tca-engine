// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/tokenizer.hpp"

// C++ STL
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// Third-party
#include <gsl/gsl>

namespace tca::engine::platform::terminal {

// === tokenize_result ===

tokenize_result::tokenize_result(std::variant<std::vector<token>, tokenizer_error> result) noexcept
    : m_result{std::move(result)} {}

tokenize_result tokenize_result::success(std::vector<token>&& tokens) noexcept {
    return tokenize_result{std::variant<std::vector<token>, tokenizer_error>{std::move(tokens)}};
}

tokenize_result tokenize_result::failure(tokenizer_error&& error) noexcept {
    return tokenize_result{std::variant<std::vector<token>, tokenizer_error>{std::move(error)}};
}

bool tokenize_result::has_value() const noexcept {
    return std::holds_alternative<std::vector<token>>(m_result);
}

const std::vector<token>& tokenize_result::value() const noexcept {
    return std::get<std::vector<token>>(m_result);
}

const tokenizer_error& tokenize_result::error() const noexcept {
    return std::get<tokenizer_error>(m_result);
}

// =======================
// === tokenize_command_line ===

namespace {

/// @brief Tells whether ch is whitespace (space or tab), used as the token separator.
bool is_space(const char ch) noexcept {
    return std::isspace(gsl::narrow_cast<unsigned char>(ch)) > 0;
}

/// @brief The character that begins an end-of-line comment when it starts an unquoted token.
constexpr char comment_char{'#'};

/// @brief The escape character used both outside quotes and inside double quotes.
constexpr char escape_char{'\\'};

/// @brief The two quoting characters recognised by the tokenizer.
constexpr char double_quote{'\"'};
constexpr char single_quote{'\''};

/**
 * @brief Scans the interior of a double-quoted segment, appending the resolved characters to value.
 * @note Inside "..." only \" and \\ are escapes (unescaped to '"' and '\'); any other backslash is
 * kept verbatim together with the character that follows it.
 * @param line The full command line being tokenized.
 * @param openPos The offset of the opening '"' (used for the error position and to resume after it).
 * @param value The token buffer that accumulates the resolved characters.
 * @return The offset just past the closing '"' on success, or the error that stopped scanning.
 */
std::variant<std::size_t, tokenizer_error> scan_double_quoted(const std::string_view line,
                                                              const std::size_t openPos,
                                                              std::string& value) {
    // Start right after the opening quote and walk until the matching closing quote.
    for (std::size_t i{openPos + 1}; i < line.size(); ++i) {
        const char ch{line[i]};
        if (ch == double_quote) {
            return i + 1;
        }

        if (ch == escape_char) {
            // A backslash with nothing after it is a dangling escape, reported before the
            // (also missing) closing quote since we hit it first while scanning.
            if (i + 1 >= line.size()) {
                return tokenizer_error{tokenizer_error_code::dangling_escape, i};
            }

            const char next{line[i + 1]};
            if (next == double_quote || next == escape_char) {
                // Only \" and \\ are unescaped; the backslash is dropped and its target kept.
                value.push_back(next);
                ++i;
            } else {
                // Any other backslash is literal: both it and the next char survive untouched
                // (the next char is handled by the following loop iteration).
                value.push_back(escape_char);
            }
            continue;
        }

        value.push_back(ch);
    }

    // Ran off the end of the line without ever seeing the closing quote.
    return tokenizer_error{tokenizer_error_code::unterminated_double_quote, openPos};
}

/**
 * @brief Scans the interior of a single-quoted segment, appending its characters to value.
 * @note Inside '...' everything is completely literal: there is no escape mechanism at all.
 * @param line The full command line being tokenized.
 * @param openPos The offset of the opening '\'' (used for the error position and to resume).
 * @param value The token buffer that accumulates the literal characters.
 * @return The offset just past the closing '\'' on success, or the error that stopped scanning.
 */
std::variant<std::size_t, tokenizer_error> scan_single_quoted(const std::string_view line,
                                                              const std::size_t openPos,
                                                              std::string& value) {
    // Copy everything verbatim until the matching closing quote; no character is special here.
    for (std::size_t i{openPos + 1}; i < line.size(); ++i) {
        const char ch{line[i]};
        if (ch == single_quote) {
            return i + 1;
        }

        value.push_back(ch);
    }

    // Ran off the end of the line without ever seeing the closing quote.
    return tokenizer_error{tokenizer_error_code::unterminated_single_quote, openPos};
}

} // namespace

tokenize_result tokenize_command_line(const std::string_view line) {
    std::vector<token> tokens{};

    // Single pass over the line. Each outer iteration extracts exactly one token: it first skips
    // the whitespace separating tokens, then accumulates characters (resolving quotes and escapes)
    // until the next unquoted whitespace or the end of the line.
    for (std::size_t i{0}; i < line.size();) {
        // Skip the run of whitespace that separates tokens; runs collapse to a single separator.
        while (i < line.size() && is_space(line[i])) {
            ++i;
        }

        if (i >= line.size()) {
            // Only trailing whitespace remained.
            break;
        }

        // A '#' that begins an (unquoted) token opens a comment running to the end of the line, so
        // there is nothing more to tokenize.
        if (line[i] == comment_char) {
            break;
        }

        // Remember where this token starts in the original line so callers can point a caret back
        // at it. For a token that opens with a quote this is the quote character itself.
        const std::size_t tokenStart{i};
        std::string value{};

        // Accumulate the token until an unquoted whitespace (or end of line) closes it.
        while (i < line.size() && !is_space(line[i])) {
            const char ch{line[i]};

            if (ch == double_quote) {
                auto scanned{scan_double_quoted(line, i, value)};
                if (const auto* error{std::get_if<tokenizer_error>(&scanned)}) {
                    return tokenize_result::failure(tokenizer_error{*error});
                }
                i = std::get<std::size_t>(scanned);
                continue;
            }

            if (ch == single_quote) {
                auto scanned{scan_single_quoted(line, i, value)};
                if (const auto* error{std::get_if<tokenizer_error>(&scanned)}) {
                    return tokenize_result::failure(tokenizer_error{*error});
                }
                i = std::get<std::size_t>(scanned);
                continue;
            }

            if (ch == escape_char) {
                // Outside quotes a backslash escapes the very next character literally (so "\ "
                // embeds a space and "\#" embeds a literal '#'); with nothing after it, it dangles.
                if (i + 1 >= line.size()) {
                    return tokenize_result::failure(
                        tokenizer_error{tokenizer_error_code::dangling_escape, i});
                }
                value.push_back(line[i + 1]);
                i += 2;
                continue;
            }

            // An ordinary unquoted character (including a '#' in the middle of a token, which is
            // not a comment) is taken literally.
            value.push_back(ch);
            ++i;
        }

        tokens.push_back(token{.value = std::move(value), .position = tokenStart});
    }

    return tokenize_result::success(std::move(tokens));
}

} // namespace tca::engine::platform::terminal
