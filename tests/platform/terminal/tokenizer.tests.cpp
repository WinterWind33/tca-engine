// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/tokenizer.hpp"

// C++ STL
#include <cstddef>
#include <string_view>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("terminal tokenizer tests", "[tca][engine][platform][terminal][tokenizer]") {
    using namespace tca::engine::platform::terminal;

    SECTION("Empty and whitespace-only lines") {
        SECTION("Empty line") {
            const tokenize_result result{tokenize_command_line("")};

            REQUIRE(result.has_value());
            REQUIRE(result.value().empty());
        }

        SECTION("Only spaces") {
            const tokenize_result result{tokenize_command_line("     ")};

            REQUIRE(result.has_value());
            REQUIRE(result.value().empty());
        }

        SECTION("Only tabs and spaces mixed") {
            const tokenize_result result{tokenize_command_line(" \t \t")};

            REQUIRE(result.has_value());
            REQUIRE(result.value().empty());
        }
    }

    SECTION("Plain unquoted words") {
        SECTION("A single word") {
            const tokenize_result result{tokenize_command_line("help")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"help", 0}});
        }

        SECTION("Words separated by a single space") {
            const tokenize_result result{tokenize_command_line("nmap -sV")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"nmap", 0}, {"-sV", 5}});
        }

        SECTION("Words separated by multiple spaces collapse to one separator") {
            const tokenize_result result{tokenize_command_line("nmap    -sV")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"nmap", 0}, {"-sV", 8}});
        }

        SECTION("Words separated by tabs") {
            const tokenize_result result{tokenize_command_line("nmap\t-sV")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"nmap", 0}, {"-sV", 5}});
        }

        SECTION("Leading and trailing whitespace is trimmed") {
            const tokenize_result result{tokenize_command_line("  nmap -sV  ")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"nmap", 2}, {"-sV", 7}});
        }
    }

    SECTION("Double-quoted segments") {
        SECTION("A quoted phrase is kept as a single token") {
            const tokenize_result result{tokenize_command_line(R"(echo "hello there")")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"echo", 0}, {"hello there", 5}});
        }

        SECTION("An empty quoted string still produces a token") {
            const tokenize_result result{tokenize_command_line(R"(echo "")")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"echo", 0}, {"", 5}});
        }

        SECTION("Escaped double quote is unescaped to a literal quote") {
            const tokenize_result result{tokenize_command_line(R"("say \"hi\"")")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{R"(say "hi")", 0}});
        }

        SECTION("Escaped backslash is unescaped to a single backslash") {
            const tokenize_result result{tokenize_command_line(R"("C:\\path\\")")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{R"(C:\path\)", 0}});
        }

        SECTION("A backslash before any other character is kept as-is") {
            const tokenize_result result{tokenize_command_line(R"("a\nb")")};

            REQUIRE(result.has_value());
            // Both the backslash and the 'n' survive: only \" and \\ are special inside "...".
            REQUIRE(result.value() == std::vector<token>{{R"(a\nb)", 0}});
        }

        SECTION("An unterminated double quote is reported as an error") {
            const tokenize_result result{tokenize_command_line(R"(echo "hello)")};

            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error() ==
                    tokenizer_error{tokenizer_error_code::unterminated_double_quote, 5});
        }

        SECTION("A lone opening double quote is reported as an error") {
            const tokenize_result result{tokenize_command_line(R"(")")};

            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error() ==
                    tokenizer_error{tokenizer_error_code::unterminated_double_quote, 0});
        }
    }

    SECTION("Single-quoted segments") {
        SECTION("A quoted phrase is kept as a single token") {
            const tokenize_result result{tokenize_command_line("echo 'hello there'")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"echo", 0}, {"hello there", 5}});
        }

        SECTION("An empty quoted string still produces a token") {
            const tokenize_result result{tokenize_command_line("echo ''")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"echo", 0}, {"", 5}});
        }

        SECTION("There is no escape mechanism: backslashes are fully literal") {
            const tokenize_result result{tokenize_command_line(R"('a\nb')")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{R"(a\nb)", 0}});
        }

        SECTION("A '#' inside single quotes is literal, not a comment") {
            const tokenize_result result{tokenize_command_line("'a#b'")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"a#b", 0}});
        }

        SECTION("An unterminated single quote is reported as an error") {
            const tokenize_result result{tokenize_command_line("echo 'hello")};

            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error() ==
                    tokenizer_error{tokenizer_error_code::unterminated_single_quote, 5});
        }

        SECTION("A trailing backslash inside single quotes is literal, not a dangling escape") {
            // Single quotes have no escape mechanism at all: the backslash is just a character,
            // so what actually fails here is the missing closing quote.
            const tokenize_result result{tokenize_command_line(R"('abc\)")};

            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error() ==
                    tokenizer_error{tokenizer_error_code::unterminated_single_quote, 0});
        }

        SECTION("A lone opening single quote is reported as an error") {
            const tokenize_result result{tokenize_command_line("'")};

            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error() ==
                    tokenizer_error{tokenizer_error_code::unterminated_single_quote, 0});
        }
    }

    SECTION("Adjacent quoted and unquoted segments concatenate") {
        SECTION("Unquoted, then quoted, then unquoted with no separating whitespace") {
            const tokenize_result result{tokenize_command_line(R"(foo"bar baz"qux)")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"foobar bazqux", 0}});
        }

        SECTION("Single-quoted then double-quoted then unquoted") {
            const tokenize_result result{tokenize_command_line(R"('a'"b"c)")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"abc", 0}});
        }

        SECTION("Two adjacent quoted segments with no content between them") {
            const tokenize_result result{tokenize_command_line(R"("foo"'bar')")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"foobar", 0}});
        }
    }

    SECTION("Unquoted escaping") {
        SECTION("A backslash-space embeds a literal space instead of splitting the token") {
            const tokenize_result result{tokenize_command_line(R"(foo\ bar baz)")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"foo bar", 0}, {"baz", 9}});
        }

        SECTION("A backslash-quote embeds a literal quote without starting quoting") {
            const tokenize_result result{tokenize_command_line(R"(say\"hi\")")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{R"(say"hi")", 0}});
        }

        SECTION("A double backslash collapses to a single backslash") {
            const tokenize_result result{tokenize_command_line(R"(C:\\path)")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{R"(C:\path)", 0}});
        }

        SECTION("A backslash before an ordinary character just escapes that character") {
            const tokenize_result result{tokenize_command_line(R"(a\nb)")};

            REQUIRE(result.has_value());
            // The backslash is consumed and 'n' is taken literally: no newline is produced.
            REQUIRE(result.value() == std::vector<token>{{"anb", 0}});
        }

        SECTION("A dangling trailing backslash is reported as an error") {
            const tokenize_result result{tokenize_command_line(R"(foo\)")};

            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error() == tokenizer_error{tokenizer_error_code::dangling_escape, 3});
        }

        SECTION("A dangling backslash inside an open double quote is still a dangling escape") {
            // Hitting end-of-input right after the backslash is detected before the missing
            // closing quote would be, so dangling_escape takes precedence.
            const tokenize_result result{tokenize_command_line(R"("abc\)")};

            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error() == tokenizer_error{tokenizer_error_code::dangling_escape, 4});
        }
    }

    SECTION("Comments") {
        SECTION("An unquoted '#' starts a comment running to end of line") {
            const tokenize_result result{tokenize_command_line("ls -la # list files")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"ls", 0}, {"-la", 3}});
        }

        SECTION("A line that is only a comment produces no tokens") {
            const tokenize_result result{tokenize_command_line("# just a comment")};

            REQUIRE(result.has_value());
            REQUIRE(result.value().empty());
        }

        SECTION("Leading whitespace before a comment is fine") {
            const tokenize_result result{tokenize_command_line("   # comment")};

            REQUIRE(result.has_value());
            REQUIRE(result.value().empty());
        }

        SECTION("A '#' in the middle of a token is a literal character, not a comment") {
            const tokenize_result result{tokenize_command_line("foo#bar")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"foo#bar", 0}});
        }

        SECTION("An escaped '#' at the start of a token is literal, not a comment") {
            const tokenize_result result{tokenize_command_line(R"(\#comment)")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"#comment", 0}});
        }

        SECTION("A '#' inside double quotes is literal, not a comment") {
            const tokenize_result result{tokenize_command_line(R"("a#b")")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() == std::vector<token>{{"a#b", 0}});
        }
    }

    SECTION("Token positions point back into the original line") {
        SECTION("A quoted token's position is the opening quote, not the inner content") {
            const tokenize_result result{tokenize_command_line(R"(cmd  "hi there"  end)")};

            REQUIRE(result.has_value());
            REQUIRE(result.value() ==
                    std::vector<token>{{"cmd", 0}, {"hi there", 5}, {"end", 17}});
        }
    }
}
