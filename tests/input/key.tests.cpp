// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/key.hpp"

// C++ STL
#include <cstddef>
#include <set>
#include <string_view>

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("key enum and names", "[tca][engine][input][key]") {
    using namespace tca::engine::input;

    SECTION("key_index() spans [0, key_count)") {
        REQUIRE(key_index(key::a) == 0);
        REQUIRE(key_index(key::pause) == key_count - 1);
    }

    SECTION("Every key has a non-empty, unique name") {
        std::set<std::string_view> names{};
        for (std::size_t index{0}; index < key_count; ++index) {
            const auto name{key_name(static_cast<key>(index))};
            REQUIRE_FALSE(name.empty());
            REQUIRE(names.insert(name).second);
        }
    }

    SECTION("Names are spelled like the enumerators") {
        REQUIRE(key_name(key::a) == "a");
        REQUIRE(key_name(key::z) == "z");
        REQUIRE(key_name(key::digit_9) == "digit_9");
        REQUIRE(key_name(key::arrow_right) == "arrow_right");
        REQUIRE(key_name(key::space) == "space");
        REQUIRE(key_name(key::escape) == "escape");
        REQUIRE(key_name(key::alt_right) == "alt_right");
        REQUIRE(key_name(key::f10) == "f10");
        REQUIRE(key_name(key::numpad_enter) == "numpad_enter");
    }

    SECTION("key::delete_key is named after the key, not the C++ keyword workaround") {
        REQUIRE(key_name(key::delete_key) == "delete");
    }

    SECTION("Positions found only on ISO, JIS, Korean and ABNT keyboards are distinct keys") {
        const std::set<std::string_view> names{
            key_name(key::intl_backslash), key_name(key::intl_ro),     key_name(key::intl_yen),
            key_name(key::convert),        key_name(key::non_convert), key_name(key::kana_mode),
            key_name(key::lang_1),         key_name(key::lang_2),      key_name(key::numpad_comma),
        };
        REQUIRE(names.size() == 9);
    }
}

TEST_CASE("key_from_name", "[tca][engine][input][key]") {
    using namespace tca::engine::input;

    SECTION("Every key is found back from its name") {
        for (std::size_t index{0}; index < key_count; ++index) {
            const auto k{static_cast<key>(index)};
            const auto found{key_from_name(key_name(k))};
            REQUIRE(found.has_value());
            REQUIRE(found.value() == k);
        }
    }

    SECTION("key::delete_key is found by its saved name, not the enumerator's spelling") {
        REQUIRE(key_from_name("delete") == key::delete_key);
        REQUIRE_FALSE(key_from_name("delete_key").has_value());
    }

    SECTION("A name of no key is not one") {
        REQUIRE_FALSE(key_from_name("").has_value());
        REQUIRE_FALSE(key_from_name("not_a_key").has_value());
        REQUIRE_FALSE(key_from_name("keyboard:space").has_value());
    }

    SECTION("Names are matched exactly") {
        REQUIRE_FALSE(key_from_name("Space").has_value());
        REQUIRE_FALSE(key_from_name("SPACE").has_value());
        REQUIRE_FALSE(key_from_name(" space").has_value());
        REQUIRE_FALSE(key_from_name("space ").has_value());
        REQUIRE_FALSE(key_from_name("spac").has_value());
    }
}
