// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/settings/action-map-settings.hpp"

#include "input/actions/action-map.hpp"
#include "input/actions/action-schema.hpp"
#include "input/key.hpp"
#include "input/mouse-button.hpp"
#include "settings/settings-io.hpp"
#include "settings/settings-json-formatter.hpp"
#include "settings/settings-schema.hpp"
#include "settings/settings-value-traits.hpp"

// Engine testing support
#include "testing/scoped-temp-directory.hpp"

// C++ STL
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace {

enum class test_action {
    jump,
    pause,
    unbound,
    undeclared
};

} // namespace

template <>
struct tca::engine::input::action_schema<test_action> {
    static constexpr auto actions{
        std::make_tuple(
            action{.name = "jump",
                   .id = test_action::jump,
                   .default_bindings = std::array{binding{key::space}, binding{key::w}}},
            action{.name = "pause",
                   .id = test_action::pause,
                   .default_bindings = std::array{binding{key::escape}}},
            action{.name = "unbound",
                   .id = test_action::unbound,
                   .default_bindings = std::array<binding, 0>{}}
            )
    };
    // test_action::undeclared is deliberately missing.
};

namespace {

// Bindings next to another setting, as a game would keep them. Declared after the action schema:
// action_map<test_action> can't be named before it.
struct controls_settings {
    std::uint32_t version{};
    tca::engine::input::action_map<test_action> bindings{};
};

} // namespace

template <>
struct tca::engine::settings::settings_schema<controls_settings> {
    static constexpr auto fields{std::make_tuple(
        field{.name = "version", .binding = &controls_settings::version, .default_value = 1U},
        group_field{.name = "bindings", .binding = &controls_settings::bindings})};
};

namespace {

using tca::engine::input::action_map;
using tca::engine::input::binding;
using tca::engine::input::key;
using tca::engine::input::mouse_button;
using tca::engine::settings::settings_value_traits;
using tca::engine::settings::formatters::settings_json_formatter;

using binding_traits = settings_value_traits<binding>;
using binding_list_traits = settings_value_traits<std::vector<binding>>;
using controls_settings_store =
    tca::engine::settings::settings_store<controls_settings, settings_json_formatter>;

[[nodiscard]] bool has_bindings(std::span<const binding> actual,
                                std::initializer_list<binding> expected) {
    return std::ranges::equal(actual, expected);
}

[[nodiscard]] std::string file_contents(const std::filesystem::path& filePath) {
    std::ifstream inputStream{filePath};

    return std::string{std::istreambuf_iterator<char>{inputStream},
                       std::istreambuf_iterator<char>{}};
}

void write_file(const std::filesystem::path& filePath, const std::string_view contents) {
    std::ofstream outputStream{filePath};
    outputStream << contents;
}

} // namespace

// ---------------------------------------------------------------------------
// Static tests
// ---------------------------------------------------------------------------

// A binding and a list of them are leaves the JSON formatter stores, through their traits.
static_assert(tca::engine::settings::settings_formatter_for<settings_json_formatter, binding>);
static_assert(
    tca::engine::settings::settings_formatter_for<settings_json_formatter, std::vector<binding>>);

// An action map is a group, so a settings struct can hold one as a field.
static_assert(tca::engine::settings::has_group_traits<action_map<test_action>>);
static_assert(
    tca::engine::settings::settings_formatter<settings_json_formatter, controls_settings>);

// ---------------------------------------------------------------------------
// Runtime tests
// ---------------------------------------------------------------------------

TEST_CASE("binding settings traits", "[tca][engine][input][actions][action-map-settings]") {
    SECTION("A key binding is stored as its device and the key's name") {
        REQUIRE(binding_traits::to_storage(binding{key::space}) == "keyboard:space");
        REQUIRE(binding_traits::to_storage(binding{key::arrow_up}) == "keyboard:arrow_up");
        REQUIRE(binding_traits::to_storage(binding{key::delete_key}) == "keyboard:delete");
    }

    SECTION("Every key binding round-trips") {
        for (std::size_t index{0}; index < tca::engine::input::key_count; ++index) {
            const binding original{static_cast<key>(index)};

            REQUIRE(binding_traits::from_storage(binding_traits::to_storage(original)) == original);
        }
    }

    SECTION("A mouse button binding is stored as its device and the button's name") {
        REQUIRE(binding_traits::to_storage(binding{mouse_button::left}) == "mouse:left");
        REQUIRE(binding_traits::to_storage(binding{mouse_button::x1}) == "mouse:x1");
    }

    SECTION("Every mouse button binding round-trips") {
        for (std::size_t index{0}; index < tca::engine::input::MOUSE_BUTTONS_COUNT; ++index) {
            const binding original{static_cast<mouse_button>(index)};

            REQUIRE(binding_traits::from_storage(binding_traits::to_storage(original)) == original);
        }
    }

    SECTION("A key and a button of the same underlying value stay apart") {
        // key::a and mouse_button::left are both the first enumerator of their enum.
        REQUIRE(binding_traits::from_storage("keyboard:a") == binding{key::a});
        REQUIRE(binding_traits::from_storage("mouse:left") == binding{mouse_button::left});
        REQUIRE(binding{key::a} != binding{mouse_button::left});
    }

    SECTION("Anything that isn't a binding is refused") {
        for (const std::string_view stored : {
                 "",
                 "space",
                 "keyboard:",
                 "keyboard:not_a_key",
                 "keyboard:Space",
                 "keyboard: space",
                 "keyboard:space ",
                 "Keyboard:space",
                 "keyboard::space",
                 "gamepad:a",
                 ":space",
                 "mouse:",
                 "mouse:not_a_button",
                 "mouse:Left",
                 "mouse:space",
                 "keyboard:left",
                 "Mouse:left",
             }) {
            INFO("stored: \"" << stored << '"');
            REQUIRE_THROWS_AS(binding_traits::from_storage(std::string{stored}),
                              std::invalid_argument);
        }
    }

    SECTION("A list of bindings is stored in order, one string each") {
        REQUIRE(binding_list_traits::to_storage({binding{key::w}, binding{key::arrow_up}}) ==
                std::vector<std::string>{"keyboard:w", "keyboard:arrow_up"});
        REQUIRE(binding_list_traits::to_storage({}).empty());
    }

    SECTION("A list of bindings is read back in order") {
        REQUIRE(binding_list_traits::from_storage({"keyboard:arrow_up", "keyboard:w"}) ==
                std::vector<binding>{binding{key::arrow_up}, binding{key::w}});
        REQUIRE(binding_list_traits::from_storage({}).empty());
    }

    SECTION("A list can mix keys and buttons, in order") {
        const std::vector<binding> mixed{binding{mouse_button::right}, binding{key::f}};

        REQUIRE(binding_list_traits::to_storage(mixed) ==
                std::vector<std::string>{"mouse:right", "keyboard:f"});
        REQUIRE(binding_list_traits::from_storage({"mouse:right", "keyboard:f"}) == mixed);
    }

    SECTION("One string that isn't a binding refuses the whole list") {
        REQUIRE_THROWS_AS(
            binding_list_traits::from_storage({"keyboard:w", "keyboard:not_a_key", "keyboard:a"}),
            std::invalid_argument);
    }
}

TEST_CASE("action_map through settings_store",
          "[tca][engine][input][actions][action-map-settings]") {
    const tca::engine::testing::scoped_temp_directory tempDirectory{
        "tca-engine-action-map-settings"};
    const auto settingsFilePath{tempDirectory.path() / "controls.json"};

    SECTION("A missing file is created with every action on its default bindings") {
        const controls_settings_store store{settingsFilePath};

        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::jump),
                             {binding{key::space}, binding{key::w}}));
        REQUIRE(file_contents(settingsFilePath) == R"({
    "bindings": {
        "jump": [
            "keyboard:space",
            "keyboard:w"
        ],
        "pause": [
            "keyboard:escape"
        ],
        "unbound": []
    },
    "version": 1
}
)");
    }

    SECTION("Rebound actions are saved and read back") {
        {
            controls_settings_store store{settingsFilePath};
            store.settings().bindings.rebind(test_action::jump,
                                             {binding{key::arrow_up}, binding{key::space}});
            store.settings().bindings.rebind(test_action::unbound, {binding{key::f1}});
        }

        controls_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.load_file() == tca::engine::settings::settings_load_result::complete);
        REQUIRE(has_bindings(reloaded.settings().bindings.bindings_of(test_action::jump),
                             {binding{key::arrow_up}, binding{key::space}}));
        REQUIRE(has_bindings(reloaded.settings().bindings.bindings_of(test_action::pause),
                             {binding{key::escape}}));
        REQUIRE(has_bindings(reloaded.settings().bindings.bindings_of(test_action::unbound),
                             {binding{key::f1}}));
    }

    SECTION("An action rebound to a mouse button is saved and read back") {
        {
            controls_settings_store store{settingsFilePath};
            store.settings().bindings.rebind(test_action::jump,
                                             {binding{mouse_button::left}, binding{key::space}});
        }

        REQUIRE(file_contents(settingsFilePath).find(R"("mouse:left")") != std::string::npos);

        controls_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.load_file() == tca::engine::settings::settings_load_result::complete);
        REQUIRE(has_bindings(reloaded.settings().bindings.bindings_of(test_action::jump),
                             {binding{mouse_button::left}, binding{key::space}}));
    }

    SECTION("An action the player unbound stays unbound, rather than going back to its default") {
        {
            controls_settings_store store{settingsFilePath};
            store.settings().bindings.rebind(test_action::pause, {});
        }

        REQUIRE(file_contents(settingsFilePath).find(R"("pause": [])") != std::string::npos);

        const controls_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.settings().bindings.bindings_of(test_action::pause).empty());
    }

    SECTION("An action the file doesn't have keeps its default, and the load is incomplete") {
        write_file(settingsFilePath, R"({
            "version": 1,
            "bindings": {
                "jump": ["keyboard:enter"]
            }
        })");

        controls_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == tca::engine::settings::settings_load_result::incomplete);
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::jump),
                             {binding{key::enter}}));
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::pause),
                             {binding{key::escape}}));
    }

    SECTION("An unknown key name keeps that action on its default, the others are read") {
        write_file(settingsFilePath, R"({
            "version": 1,
            "bindings": {
                "jump": ["keyboard:enter", "keyboard:not_a_key"],
                "pause": ["keyboard:p"],
                "unbound": []
            }
        })");

        controls_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == tca::engine::settings::settings_load_result::incomplete);
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::jump),
                             {binding{key::space}, binding{key::w}}));
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::pause),
                             {binding{key::p}}));
    }

    SECTION("An unknown device keeps that action on its default") {
        write_file(settingsFilePath, R"({
            "version": 1,
            "bindings": {
                "jump": ["gamepad:a"],
                "pause": ["keyboard:escape"],
                "unbound": []
            }
        })");

        controls_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == tca::engine::settings::settings_load_result::incomplete);
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::jump),
                             {binding{key::space}, binding{key::w}}));
    }

    SECTION("An action that isn't a list keeps its default") {
        write_file(settingsFilePath, R"({
            "version": 1,
            "bindings": {
                "jump": "keyboard:enter",
                "pause": ["keyboard:escape"],
                "unbound": []
            }
        })");

        controls_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == tca::engine::settings::settings_load_result::incomplete);
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::jump),
                             {binding{key::space}, binding{key::w}}));
    }

    SECTION("Actions the file has and the schema doesn't are ignored") {
        write_file(settingsFilePath, R"({
            "version": 1,
            "bindings": {
                "jump": ["keyboard:enter"],
                "pause": ["keyboard:escape"],
                "unbound": [],
                "dash": ["keyboard:shift_left"]
            }
        })");

        controls_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == tca::engine::settings::settings_load_result::complete);
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::jump),
                             {binding{key::enter}}));
        REQUIRE(store.settings().bindings.bindings_of(test_action::undeclared).empty());
    }

    SECTION("A file without bindings leaves every action on its default") {
        write_file(settingsFilePath, R"({"version": 2})");

        controls_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == tca::engine::settings::settings_load_result::incomplete);
        REQUIRE(store.settings().version == 2U);
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::jump),
                             {binding{key::space}, binding{key::w}}));
    }

    SECTION("Loading again drops rebinds that were never saved") {
        write_file(settingsFilePath, R"({
            "version": 1,
            "bindings": {
                "pause": ["keyboard:p"]
            }
        })");

        controls_settings_store store{settingsFilePath};
        store.settings().bindings.rebind(test_action::jump, {binding{key::enter}});
        store.settings().bindings.rebind(test_action::pause, {binding{key::q}});

        REQUIRE(store.load_file() == tca::engine::settings::settings_load_result::incomplete);
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::jump),
                             {binding{key::space}, binding{key::w}}));
        REQUIRE(has_bindings(store.settings().bindings.bindings_of(test_action::pause),
                             {binding{key::p}}));
    }
}
