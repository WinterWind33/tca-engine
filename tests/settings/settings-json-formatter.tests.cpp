// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "settings/settings-json-formatter.hpp"

// Engine testing support
#include "testing/scoped-temp-directory.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

// C++ STL
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "core/utilities/ranged-value.hpp"
#include "settings/settings-io.hpp"

using namespace tca::engine::settings;
using tca::engine::settings::formatters::settings_json_formatter;

namespace {

using volume_type = tca::engine::core::ranged_value<int, 0, 100>;

struct audio_settings {
    volume_type master_volume{};
    bool muted{};
};

// Two nested structs sharing field names, so what keeps them apart is the scope they sit in
// rather than the names themselves.
struct window_settings {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct hud_settings {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct game_settings {
    window_settings window{};
    hud_settings hud{};
    audio_settings audio{};
    std::string player_name{};
};

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

template <>
struct tca::engine::settings::settings_schema<audio_settings> {
    static constexpr auto fields{std::make_tuple(
        field{.name = "master_volume",
              .binding = &audio_settings::master_volume,
              .default_value = volume_type{80}},
        field{.name = "muted", .binding = &audio_settings::muted, .default_value = false})};
};

template <>
struct tca::engine::settings::settings_schema<window_settings> {
    static constexpr auto fields{std::make_tuple(
        field{.name = "width", .binding = &window_settings::width, .default_value = 1920U},
        field{.name = "height", .binding = &window_settings::height, .default_value = 1080U})};
};

template <>
struct tca::engine::settings::settings_schema<hud_settings> {
    static constexpr auto fields{std::make_tuple(
        field{.name = "width", .binding = &hud_settings::width, .default_value = 320U},
        field{.name = "height", .binding = &hud_settings::height, .default_value = 200U})};
};

template <>
struct tca::engine::settings::settings_schema<game_settings> {
    static constexpr auto fields{
        std::make_tuple(field{.name = "window", .binding = &game_settings::window},
                        field{.name = "hud", .binding = &game_settings::hud},
                        field{.name = "audio", .binding = &game_settings::audio},
                        // No default: a std::string cannot live in a constexpr schema, so string
                        // fields are left value-initialized unless the file has something for
                        // them.
                        field{.name = "player_name", .binding = &game_settings::player_name})};
};

// ---------------------------------------------------------------------------
// Static tests
// ---------------------------------------------------------------------------

static_assert(settings_document_formatter<settings_json_formatter>);

// The primitives the document stores directly.
static_assert(settings_formatter_for<settings_json_formatter, bool>);
static_assert(settings_formatter_for<settings_json_formatter, std::int64_t>);
static_assert(settings_formatter_for<settings_json_formatter, std::uint32_t>);
static_assert(settings_formatter_for<settings_json_formatter, float>);
static_assert(settings_formatter_for<settings_json_formatter, double>);
static_assert(settings_formatter_for<settings_json_formatter, std::string>);
static_assert(settings_formatter_for<settings_json_formatter, std::vector<std::string>>);

// A ranged value is reached through its settings_value_traits specialization, which names the
// primitive it is stored as.
static_assert(settings_formatter_for<settings_json_formatter, volume_type>);

// A type naming no primitive to be stored as is rejected, rather than reaching the JSON library
// behind the formatter's back.
static_assert(!settings_formatter_for<settings_json_formatter, std::vector<int>>);

// One formatter serves every schema whose leaves it covers, nesting included.
static_assert(settings_formatter<settings_json_formatter, audio_settings>);
static_assert(settings_formatter<settings_json_formatter, game_settings>);

// ---------------------------------------------------------------------------
// Runtime tests
// ---------------------------------------------------------------------------

TEST_CASE("settings_json_formatter tests", "[tca][engine][settings][settings_json_formatter]") {
    const settings_json_formatter formatter{};

    const auto documentOf{[&formatter](const std::string_view contents) {
        std::istringstream inputStream{std::string{contents}};

        return formatter.parse(inputStream);
    }};

    SECTION("Values are written under the key they were given") {
        auto document{formatter.make_document()};
        const auto scope{formatter.root(document)};

        REQUIRE(formatter.write(scope, "width", 1280U));
        REQUIRE(formatter.write(scope, "title", std::string{"The Clinical Aspect"}));

        std::ostringstream outputStream{};
        REQUIRE(formatter.serialize(outputStream, document));

        REQUIRE(outputStream.str() ==
                "{\n"
                "    \"title\": \"The Clinical Aspect\",\n"
                "    \"width\": 1280\n"
                "}\n");
    }

    SECTION("A subdocument is a nested object, not a longer key") {
        auto document{formatter.make_document()};
        const auto windowScope{formatter.subdocument(formatter.root(document), "window")};

        REQUIRE(formatter.write(windowScope, "width", 1280U));

        std::ostringstream outputStream{};
        REQUIRE(formatter.serialize(outputStream, document));

        REQUIRE(outputStream.str() ==
                "{\n"
                "    \"window\": {\n"
                "        \"width\": 1280\n"
                "    }\n"
                "}\n");
    }

    SECTION("Reading back a key the document does not hold comes back empty") {
        auto document{formatter.make_document()};
        const auto scope{formatter.root(document)};

        REQUIRE_FALSE(formatter.read<std::uint32_t>(scope, "width").has_value());
    }

    SECTION("A value of the wrong type is reported as unreadable rather than thrown") {
        auto document{documentOf(R"({"width": "not a number"})")};

        REQUIRE(document.has_value());
        REQUIRE_FALSE(
            formatter.read<std::uint32_t>(formatter.root(document.value()), "width").has_value());
    }

    SECTION("A number too big for the field it is read into is reported as unreadable") {
        auto document{documentOf(R"({"width": 5000000000, "depth": -1})")};

        REQUIRE(document.has_value());

        const auto scope{formatter.root(document.value())};

        REQUIRE_FALSE(formatter.read<std::uint32_t>(scope, "width").has_value());
        REQUIRE_FALSE(formatter.read<std::uint32_t>(scope, "depth").has_value());
        REQUIRE(formatter.read<std::int64_t>(scope, "width").value() == 5000000000);
    }

    SECTION("A whole number is a good enough value for a floating-point field") {
        auto document{documentOf(R"({"sensitivity": 2})")};

        REQUIRE(document.has_value());

        const auto loadedValue{
            formatter.read<float>(formatter.root(document.value()), "sensitivity")};

        REQUIRE(loadedValue.has_value());
        REQUIRE(loadedValue.value() == 2.0F);
    }

    SECTION("A scope the document does not hold reads as empty instead of being created") {
        auto document{documentOf(R"({"window": 42})")};

        REQUIRE(document.has_value());

        const auto scope{formatter.subdocument(formatter.root(document.value()), "window")};

        REQUIRE_FALSE(formatter.read<std::uint32_t>(scope, "width").has_value());
    }

    SECTION("A ranged value is stored as its underlying type") {
        auto document{formatter.make_document()};
        const auto scope{formatter.root(document)};

        REQUIRE(formatter.write(scope, "master_volume", volume_type{55}));

        std::ostringstream outputStream{};
        REQUIRE(formatter.serialize(outputStream, document));
        REQUIRE(outputStream.str() ==
                "{\n"
                "    \"master_volume\": 55\n"
                "}\n");

        const auto loadedValue{formatter.read<volume_type>(scope, "master_volume")};

        REQUIRE(loadedValue.has_value());
        REQUIRE(loadedValue.value() == 55);
    }

    SECTION("A ranged value the file pushes out of range is reported as unreadable") {
        auto document{documentOf(R"({"master_volume": 500})")};

        REQUIRE(document.has_value());
        REQUIRE_FALSE(formatter.read<volume_type>(formatter.root(document.value()), "master_volume")
                          .has_value());
    }

    SECTION("Parsing something that is not a JSON object fails instead of throwing") {
        REQUIRE_FALSE(documentOf("{ this is not json").has_value());
        REQUIRE_FALSE(documentOf("[1, 2, 3]").has_value());
        REQUIRE_FALSE(documentOf("").has_value());
    }

    SECTION("A document round-trips through a stream") {
        auto document{formatter.make_document()};

        REQUIRE(formatter.write(formatter.root(document), "width", 1280U));

        std::ostringstream outputStream{};
        REQUIRE(formatter.serialize(outputStream, document));

        auto reloaded{documentOf(outputStream.str())};

        REQUIRE(reloaded.has_value());

        const auto loadedValue{
            formatter.read<std::uint32_t>(formatter.root(reloaded.value()), "width")};

        REQUIRE(loadedValue.has_value());
        REQUIRE(loadedValue.value() == 1280U);
    }
}

TEST_CASE("settings_json_formatter string array tests",
          "[tca][engine][settings][settings_json_formatter][string_array]") {
    const settings_json_formatter formatter{};

    const auto documentOf{[&formatter](const std::string_view contents) {
        std::istringstream inputStream{std::string{contents}};

        return formatter.parse(inputStream);
    }};

    SECTION("A list of strings is written as a JSON array, in order") {
        auto document{formatter.make_document()};

        REQUIRE(formatter.write(formatter.root(document), "jump",
                                std::vector<std::string>{"keyboard:space", "keyboard:w"}));

        std::ostringstream outputStream{};
        REQUIRE(formatter.serialize(outputStream, document));

        REQUIRE(outputStream.str() ==
                "{\n"
                "    \"jump\": [\n"
                "        \"keyboard:space\",\n"
                "        \"keyboard:w\"\n"
                "    ]\n"
                "}\n");
    }

    SECTION("An empty list is written as an empty array, not left out") {
        auto document{formatter.make_document()};

        REQUIRE(formatter.write(formatter.root(document), "jump", std::vector<std::string>{}));

        std::ostringstream outputStream{};
        REQUIRE(formatter.serialize(outputStream, document));

        REQUIRE(outputStream.str() ==
                "{\n"
                "    \"jump\": []\n"
                "}\n");
    }

    SECTION("An array of strings reads back in order") {
        auto document{documentOf(R"({"jump": ["keyboard:space", "keyboard:w"]})")};

        REQUIRE(document.has_value());

        const auto loadedValue{
            formatter.read<std::vector<std::string>>(formatter.root(document.value()), "jump")};

        REQUIRE(loadedValue.has_value());
        REQUIRE(loadedValue.value() == std::vector<std::string>{"keyboard:space", "keyboard:w"});
    }

    SECTION("An empty array reads back as an empty list, not as missing") {
        auto document{documentOf(R"({"jump": []})")};

        REQUIRE(document.has_value());

        const auto loadedValue{
            formatter.read<std::vector<std::string>>(formatter.root(document.value()), "jump")};

        REQUIRE(loadedValue.has_value());
        REQUIRE(loadedValue.value().empty());
    }

    SECTION("A list round-trips through a stream") {
        const std::vector<std::string> written{"a", "", "keyboard:\xC3\xA9", "a"};

        auto document{formatter.make_document()};
        REQUIRE(formatter.write(formatter.root(document), "list", written));

        std::ostringstream outputStream{};
        REQUIRE(formatter.serialize(outputStream, document));

        auto reloaded{documentOf(outputStream.str())};
        REQUIRE(reloaded.has_value());

        const auto loadedValue{
            formatter.read<std::vector<std::string>>(formatter.root(reloaded.value()), "list")};

        REQUIRE(loadedValue.has_value());
        REQUIRE(loadedValue.value() == written);
    }

    SECTION("A key the document does not hold comes back empty") {
        auto document{formatter.make_document()};

        REQUIRE_FALSE(
            formatter.read<std::vector<std::string>>(formatter.root(document), "jump").has_value());
    }

    SECTION("Something other than an array is unreadable, a lone string included") {
        auto document{documentOf(R"({"string": "keyboard:space", "number": 1, "object": {}})")};

        REQUIRE(document.has_value());

        const auto scope{formatter.root(document.value())};

        REQUIRE_FALSE(formatter.read<std::vector<std::string>>(scope, "string").has_value());
        REQUIRE_FALSE(formatter.read<std::vector<std::string>>(scope, "number").has_value());
        REQUIRE_FALSE(formatter.read<std::vector<std::string>>(scope, "object").has_value());
    }

    SECTION("One element that is not a string makes the whole array unreadable") {
        auto document{documentOf(R"({"jump": ["keyboard:space", 42], "nested": [["a"]]})")};

        REQUIRE(document.has_value());

        const auto scope{formatter.root(document.value())};

        REQUIRE_FALSE(formatter.read<std::vector<std::string>>(scope, "jump").has_value());
        REQUIRE_FALSE(formatter.read<std::vector<std::string>>(scope, "nested").has_value());
    }

    SECTION("An array is not a string, so a string field holding one is unreadable") {
        auto document{documentOf(R"({"name": ["a"]})")};

        REQUIRE(document.has_value());
        REQUIRE_FALSE(
            formatter.read<std::string>(formatter.root(document.value()), "name").has_value());
    }
}

TEST_CASE("settings_store JSON tests", "[tca][engine][settings][settings_store]") {
    using game_settings_store = settings_store<game_settings, settings_json_formatter>;

    const tca::engine::testing::scoped_temp_directory tempDirectory{"tca-engine-json-settings"};
    const auto settingsFilePath{tempDirectory.path() / "game.settings.json"};

    SECTION("A missing file is created from the schema defaults, nested ones included") {
        const game_settings_store store{settingsFilePath};

        REQUIRE(std::filesystem::exists(settingsFilePath));
        REQUIRE(store.settings().window.width == 1920U);
        REQUIRE(store.settings().hud.width == 320U);
        REQUIRE(store.settings().audio.master_volume == 80);
        REQUIRE_FALSE(store.settings().audio.muted);
        REQUIRE(store.settings().player_name.empty());
    }

    SECTION("The file it writes is a nested JSON object") {
        { const game_settings_store store{settingsFilePath}; }

        REQUIRE(file_contents(settingsFilePath) ==
                "{\n"
                "    \"audio\": {\n"
                "        \"master_volume\": 80,\n"
                "        \"muted\": false\n"
                "    },\n"
                "    \"hud\": {\n"
                "        \"height\": 200,\n"
                "        \"width\": 320\n"
                "    },\n"
                "    \"player_name\": \"\",\n"
                "    \"window\": {\n"
                "        \"height\": 1080,\n"
                "        \"width\": 1920\n"
                "    }\n"
                "}\n");
    }

    SECTION("Two nested structs sharing a field name do not collide") {
        {
            game_settings_store store{settingsFilePath};

            store.settings().window.width = 2560U;
            store.settings().hud.width = 640U;
        }

        const game_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.settings().window.width == 2560U);
        REQUIRE(reloaded.settings().hud.width == 640U);
    }

    SECTION("Settings written through the store round-trip through the file") {
        {
            game_settings_store store{settingsFilePath};

            store.settings().audio.master_volume = 25;
            store.settings().audio.muted = true;
            store.settings().player_name = "The Clinical Aspect";
        }

        const game_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.settings().audio.master_volume == 25);
        REQUIRE(reloaded.settings().audio.muted);
        REQUIRE(reloaded.settings().player_name == "The Clinical Aspect");
    }

    SECTION("A file written by an older version keeps its fields and defaults the new ones") {
        write_file(settingsFilePath, R"({"player_name": "Returning player",
                                        "window": {"width": 800, "height": 600},
                                        "audio": {"master_volume": 10, "muted": true}})");

        game_settings_store store{settingsFilePath};

        // The whole "hud" scope is missing, so the file is reported as incomplete...
        REQUIRE_FALSE(store.load());
        REQUIRE(store.settings().hud.width == 320U);
        REQUIRE(store.settings().hud.height == 200U);

        // ...while everything that is there is read, wherever it sits in the file.
        REQUIRE(store.settings().player_name == "Returning player");
        REQUIRE(store.settings().window.width == 800U);
        REQUIRE(store.settings().window.height == 600U);
        REQUIRE(store.settings().audio.master_volume == 10);
        REQUIRE(store.settings().audio.muted);
    }

    SECTION("Keys the schema does not know are ignored") {
        write_file(settingsFilePath,
                   R"({"player_name": "Returning player",
                       "vsync": true,
                       "window": {"width": 800, "height": 600, "borderless": true},
                       "hud": {"width": 320, "height": 200},
                       "audio": {"master_volume": 10, "muted": false}})");

        game_settings_store store{settingsFilePath};

        REQUIRE(store.load());
        REQUIRE(store.settings().window.width == 800U);
        REQUIRE(store.settings().player_name == "Returning player");
    }

    SECTION("A file that is not JSON at all leaves every default in place") {
        write_file(settingsFilePath, "this is not json");

        game_settings_store store{settingsFilePath};

        REQUIRE_FALSE(store.load());
        REQUIRE(store.settings().window.width == 1920U);
        REQUIRE(store.settings().audio.master_volume == 80);
        REQUIRE(store.settings().player_name.empty());
    }

    SECTION("A volume the file pushes out of range keeps the default instead of throwing") {
        write_file(settingsFilePath, R"({"audio": {"master_volume": 500, "muted": false}})");

        game_settings_store store{settingsFilePath};

        REQUIRE_FALSE(store.load());
        REQUIRE(store.settings().window.width == 1920U);
        REQUIRE(store.settings().audio.master_volume == 80);
        REQUIRE_FALSE(store.settings().audio.muted);
    }
}

TEST_CASE("settings_store damaged file tests", "[tca][engine][settings][settings_store]") {
    using game_settings_store = settings_store<game_settings, settings_json_formatter>;

    const tca::engine::testing::scoped_temp_directory tempDirectory{"tca-engine-damaged-settings"};
    const auto settingsFilePath{tempDirectory.path() / "game.settings.json"};
    const auto damagedFilePath{tempDirectory.path() / "game.settings.json.corrupt"};

    SECTION("A file nothing can be read from reads as unreadable") {
        game_settings_store store{settingsFilePath};

        // Damaged behind the store's back, the way a crash mid-write would: the constructor has
        // already had its look at the file, so this is load_file() reading it, not the move
        // aside.
        write_file(settingsFilePath, R"({"window": {"width": 12)");

        REQUIRE(store.load_file() == settings_load_result::unreadable);
        REQUIRE(store.settings().window.width == 1920U);
    }

    SECTION("A damaged file is kept, contents and all, rather than written over") {
        constexpr std::string_view damagedContents{R"({"window": {"width": 12)"};
        write_file(settingsFilePath, damagedContents);

        { const game_settings_store store{settingsFilePath}; }

        REQUIRE(std::filesystem::exists(damagedFilePath));
        REQUIRE(file_contents(damagedFilePath) == damagedContents);
    }

    SECTION("The defaults take the place of the damaged file once it has been moved aside") {
        write_file(settingsFilePath, "this is not json");

        { const game_settings_store store{settingsFilePath}; }

        const game_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.settings().window.width == 1920U);
        REQUIRE(reloaded.settings().audio.master_volume == 80);
        REQUIRE(std::filesystem::exists(damagedFilePath));
    }

    SECTION("A truncated file is kept too: it is the one worth looking at") {
        write_file(settingsFilePath, R"({"audio": {"master_volume": 3)");

        { const game_settings_store store{settingsFilePath}; }

        REQUIRE(std::filesystem::exists(damagedFilePath));
    }

    SECTION("A file that reads only in part is not damaged, and is left where it is") {
        write_file(settingsFilePath, R"({"audio": {"master_volume": 30}})");

        {
            game_settings_store store{settingsFilePath};

            REQUIRE(store.load_file() == settings_load_result::incomplete);
            REQUIRE(store.settings().audio.master_volume == 30);
        }

        REQUIRE_FALSE(std::filesystem::exists(damagedFilePath));
    }

    SECTION("A file only holding values their own type refuses is not damaged either") {
        write_file(settingsFilePath, R"({"audio": {"master_volume": 500, "muted": false}})");

        { const game_settings_store store{settingsFilePath}; }

        REQUIRE_FALSE(std::filesystem::exists(damagedFilePath));
    }
}
