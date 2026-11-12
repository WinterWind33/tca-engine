// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "settings/settings-io.hpp"

// Engine testing support
#include "testing/scoped-temp-directory.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

// C++ STL
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <istream>
#include <iterator>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using namespace tca::engine::settings;

namespace {

// The settings structs the formatters below are checked against: one mixing arithmetic and
// string fields, one purely arithmetic.
struct graphics_settings {
    std::uint32_t width{};
    std::uint32_t height{};
    bool fullscreen{};
    std::string window_title{};
};

struct gameplay_settings {
    float mouse_sensitivity{};
    std::uint32_t difficulty{};
};

// Two nested structs that deliberately share field names, so that the store's scoping is what
// keeps them apart rather than the names themselves.
struct window_settings {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct hud_settings {
    std::uint32_t width{};
    std::uint32_t height{};
    bool visible{};
};

struct interface_settings {
    window_settings window{};
    hud_settings hud{};
    std::string language{};
};

// A settings struct deliberately left without a schema specialization.
struct unregistered_settings {
    int value{};
};

} // namespace

template <>
struct tca::engine::settings::settings_schema<graphics_settings> {
    static constexpr auto fields{std::make_tuple(
        field{.name = "width", .binding = &graphics_settings::width, .default_value = 1920U},
        field{.name = "height", .binding = &graphics_settings::height, .default_value = 1080U},
        field{.name = "fullscreen",
              .binding = &graphics_settings::fullscreen,
              .default_value = false},
        field{.name = "window_title", .binding = &graphics_settings::window_title})};
};

template <>
struct tca::engine::settings::settings_schema<gameplay_settings> {
    static constexpr auto fields{
        std::make_tuple(field{.name = "mouse_sensitivity",
                              .binding = &gameplay_settings::mouse_sensitivity,
                              .default_value = 1.0F},
                        field{.name = "difficulty",
                              .binding = &gameplay_settings::difficulty,
                              .default_value = 1U})};
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
        field{.name = "height", .binding = &hud_settings::height, .default_value = 200U},
        field{.name = "visible", .binding = &hud_settings::visible, .default_value = true})};
};

template <>
struct tca::engine::settings::settings_schema<interface_settings> {
    static constexpr auto fields{
        std::make_tuple(field{.name = "window", .binding = &interface_settings::window},
                        field{.name = "hud", .binding = &interface_settings::hud},
                        // No default: a std::string cannot live in a constexpr schema, so string
                        // fields are left value-initialized unless the file has something for them.
                        field{.name = "language", .binding = &interface_settings::language})};
};

namespace {

[[nodiscard]] std::string_view trim(const std::string_view text) noexcept {
    constexpr std::string_view blanks{" \t\r"};

    const auto first{text.find_first_not_of(blanks)};
    if (first == std::string_view::npos) {
        return {};
    }

    return text.substr(first, text.find_last_not_of(blanks) - first + 1U);
}

// A flat `key = value` document: a vector rather than a map so the fields come out in schema
// order, which is what makes a settings file readable.
using text_document = std::vector<std::pair<std::string, std::string>>;

// A scope in a flat document is a key prefix, so `interface.hud.width` and
// `interface.window.width` stay apart without either struct having to rename a field.
struct text_scope {
    text_document* entries{nullptr};
    std::string prefix{};
};

[[nodiscard]] std::optional<text_document> parse_text(std::istream& inputStream,
                                                      const char separator) {
    text_document document{};

    std::string line{};
    while (std::getline(inputStream, line)) {
        const auto separatorPosition{line.find(separator)};
        if (separatorPosition == std::string::npos) {
            // A line that is not a pair is skipped: the fields it should have carried are
            // reported as missing when they are looked up, and keep their defaults.
            continue;
        }

        document.emplace_back(trim(std::string_view{line}.substr(0U, separatorPosition)),
                              trim(std::string_view{line}.substr(separatorPosition + 1U)));
    }

    return document;
}

bool serialize_text(std::ostream& outputStream, const text_document& document,
                    const std::string_view separator) {
    for (const auto& [key, value] : document) {
        outputStream << key << separator << value << '\n';
    }

    return static_cast<bool>(outputStream);
}

bool put_text(const text_scope scope, const fixed_string key, std::string value) {
    if (scope.entries == nullptr) {
        return false;
    }

    scope.entries->emplace_back(scope.prefix + std::string{key}, std::move(value));

    return true;
}

[[nodiscard]] const std::string* find_text(const text_scope scope, const fixed_string key) {
    if (scope.entries == nullptr) {
        return nullptr;
    }

    const std::string fullKey{scope.prefix + std::string{key}};
    const auto entry{std::ranges::find(*scope.entries, fullKey, &text_document::value_type::first)};

    return entry == scope.entries->end() ? nullptr : &entry->second;
}

template <typename ValueType>
[[nodiscard]] std::optional<ValueType> parse_value(const std::string_view valueText) {
    ValueType value{};

    if constexpr (std::same_as<ValueType, std::string>) {
        value = valueText;
    } else if constexpr (std::same_as<ValueType, bool>) {
        if (valueText != "true" && valueText != "false") {
            return std::nullopt;
        }

        value = valueText == "true";
    } else {
        std::istringstream valueParser{std::string{valueText}};
        if (!(valueParser >> value)) {
            return std::nullopt;
        }
    }

    return value;
}

template <typename ValueType>
[[nodiscard]] std::string format_value(const ValueType& value) {
    if constexpr (std::same_as<ValueType, bool>) {
        return value ? "true" : "false";
    } else {
        std::ostringstream valueFormatter{};
        valueFormatter << value;

        return std::move(valueFormatter).str();
    }
}

// The formatter a game would realistically write when it wants something other than JSON: one
// `key = value` line per setting, nested scopes flattened into dotted keys. write and read are
// templates on the value type, which is what settings_formatter asks for.
struct line_formatter {
    using document_type = text_document;
    using scope_type = text_scope;

    [[nodiscard]] document_type make_document() const {
        return {};
    }

    [[nodiscard]] std::optional<document_type> parse(std::istream& inputStream) const {
        return parse_text(inputStream, '=');
    }

    bool serialize(std::ostream& outputStream, const document_type& document) const {
        return serialize_text(outputStream, document, " = ");
    }

    [[nodiscard]] scope_type root(document_type& document) const {
        return scope_type{.entries = &document};
    }

    [[nodiscard]] scope_type subdocument(const scope_type scope, const fixed_string key) const {
        return scope_type{.entries = scope.entries,
                          .prefix = scope.prefix + std::string{key} + '.'};
    }

    template <typename ValueType>
    bool write(const scope_type scope, const fixed_string key, const ValueType& value) const {
        return put_text(scope, key, format_value(value));
    }

    template <typename ValueType>
    [[nodiscard]] std::optional<ValueType> read(const scope_type scope,
                                                const fixed_string key) const {
        const auto* const valueText{find_text(scope, key)};

        return valueText == nullptr ? std::nullopt : parse_value<ValueType>(*valueText);
    }
};

// A formatter that only knows how to handle arithmetic values: enough for gameplay_settings,
// not for the string field of graphics_settings.
struct numeric_formatter {
    using document_type = text_document;
    using scope_type = text_scope;

    [[nodiscard]] document_type make_document() const {
        return {};
    }

    [[nodiscard]] std::optional<document_type> parse(std::istream& inputStream) const {
        return parse_text(inputStream, '=');
    }

    bool serialize(std::ostream& outputStream, const document_type& document) const {
        return serialize_text(outputStream, document, "=");
    }

    [[nodiscard]] scope_type root(document_type& document) const {
        return scope_type{.entries = &document};
    }

    [[nodiscard]] scope_type subdocument(const scope_type scope, const fixed_string key) const {
        return scope_type{.entries = scope.entries,
                          .prefix = scope.prefix + std::string{key} + '.'};
    }

    template <typename ValueType>
        requires std::is_arithmetic_v<ValueType>
    bool write(const scope_type scope, const fixed_string key, const ValueType& value) const {
        return put_text(scope, key, format_value(value));
    }

    template <typename ValueType>
        requires std::is_arithmetic_v<ValueType>
    [[nodiscard]] std::optional<ValueType> read(const scope_type scope,
                                                const fixed_string key) const {
        const auto* const valueText{find_text(scope, key)};

        return valueText == nullptr ? std::nullopt : parse_value<ValueType>(*valueText);
    }
};

// Serializes but never reads back.
struct write_only_formatter : line_formatter {
    template <typename ValueType>
    [[nodiscard]] std::optional<ValueType> read(scope_type, fixed_string) const = delete;
};

// Has both members, but write reports nothing back to the caller.
struct silent_write_formatter : line_formatter {
    template <typename ValueType>
    void write(const scope_type scope, const fixed_string key, const ValueType& value) const {
        static_cast<void>(put_text(scope, key, format_value(value)));
    }
};

// Not a template: it handles exactly one value type, so it cannot serve a whole schema.
struct uint32_only_formatter : line_formatter {
    bool write(const scope_type scope, const fixed_string key, const std::uint32_t value) const {
        return put_text(scope, key, format_value(value));
    }

    std::optional<std::uint32_t> read(const scope_type scope, const fixed_string key) const {
        const auto* const valueText{find_text(scope, key)};

        return valueText == nullptr ? std::nullopt : parse_value<std::uint32_t>(*valueText);
    }
};

// Handles values, but has no document to put them in.
struct scopeless_formatter {
    template <typename ValueType>
    bool write(int, const fixed_string, const ValueType&) const {
        return true;
    }

    template <typename ValueType>
    std::optional<ValueType> read(int, const fixed_string) const {
        return std::nullopt;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Static tests
// ---------------------------------------------------------------------------

// A formatter has to bring a document along before any of its values can be written: the store
// builds one, hands out scopes of it and serializes it in one go.
static_assert(settings_document_formatter<line_formatter>);
static_assert(settings_document_formatter<numeric_formatter>);
static_assert(!settings_document_formatter<scopeless_formatter>);
static_assert(!settings_document_formatter<int>);

// settings_formatter_for pins down a single value type at a time.
static_assert(settings_formatter_for<line_formatter, std::uint32_t>);
static_assert(settings_formatter_for<line_formatter, bool>);
static_assert(settings_formatter_for<line_formatter, float>);
static_assert(settings_formatter_for<line_formatter, std::string>);

// A constrained member template satisfies it only for the types it accepts.
static_assert(settings_formatter_for<numeric_formatter, float>);
static_assert(settings_formatter_for<numeric_formatter, std::uint32_t>);
static_assert(!settings_formatter_for<numeric_formatter, std::string>);

// `read` is called as `formatter.template read<ValueType>(scope, key)`, so a formatter whose
// members are plain overloads is rejected even for the one type it does handle: the concept asks
// for the templated form, not for an overload set that happens to cover a given type.
static_assert(!settings_formatter_for<uint32_only_formatter, std::uint32_t>);
static_assert(!settings_formatter_for<uint32_only_formatter, float>);

// Both halves of the round trip are required, with their exact return types.
static_assert(!settings_formatter_for<write_only_formatter, std::uint32_t>);
static_assert(!settings_formatter_for<silent_write_formatter, std::uint32_t>);
static_assert(!settings_formatter_for<scopeless_formatter, std::uint32_t>);
static_assert(!settings_formatter_for<int, std::uint32_t>);

// settings_formatter folds the above over the field types the schema actually declares, so a
// formatter is only ever asked for the types its settings struct uses.
static_assert(settings_formatter<line_formatter, graphics_settings>);
static_assert(settings_formatter<line_formatter, gameplay_settings>);

// numeric_formatter covers every field of gameplay_settings, but not the window title.
static_assert(settings_formatter<numeric_formatter, gameplay_settings>);
static_assert(!settings_formatter<numeric_formatter, graphics_settings>);

// A nested field is walked into rather than handed to the formatter, so what has to be checked
// is the leaf types the nested schemas declare. numeric_formatter handles every field of
// window_settings and hud_settings, but interface_settings adds a string of its own.
static_assert(settings_formatter<line_formatter, interface_settings>);
static_assert(settings_formatter<numeric_formatter, window_settings>);
static_assert(settings_formatter<numeric_formatter, hud_settings>);
static_assert(!settings_formatter<numeric_formatter, interface_settings>);

// The same goes schema-wide: a formatter that is not templated serves no schema at all.
static_assert(!settings_formatter<uint32_only_formatter, gameplay_settings>);
static_assert(!settings_formatter<uint32_only_formatter, graphics_settings>);

// An incomplete formatter is rejected whatever the schema.
static_assert(!settings_formatter<write_only_formatter, gameplay_settings>);
static_assert(!settings_formatter<silent_write_formatter, gameplay_settings>);
static_assert(!settings_formatter<scopeless_formatter, gameplay_settings>);
static_assert(!settings_formatter<int, gameplay_settings>);

// A settings struct without a schema has no field types to check against, so nothing satisfies
// the concept for it.
static_assert(!settings_formatter<line_formatter, unregistered_settings>);
static_assert(!settings_formatter<line_formatter, int>);

// A store owns its settings and its file, so it moves but does not copy: two stores saving to
// the same path would fight over it.
static_assert(!std::is_copy_constructible_v<settings_store<graphics_settings, line_formatter>>);
static_assert(!std::is_copy_assignable_v<settings_store<graphics_settings, line_formatter>>);
static_assert(std::is_move_constructible_v<settings_store<graphics_settings, line_formatter>>);
static_assert(std::is_move_assignable_v<settings_store<graphics_settings, line_formatter>>);

namespace {

// Example usage: the generic save/load pair the concept exists to constrain. Both build the
// formatter's document once and walk the schema over it.
template <has_schema SettingsType, settings_formatter<SettingsType> FormatterType>
bool save_settings(std::ostream& outputStream, const SettingsType& settings,
                   const FormatterType& formatter) {
    auto document{formatter.make_document()};
    const auto scope{formatter.root(document)};

    const auto saved{std::apply(
        [&settings, &formatter, scope](const auto&... schemaFields) {
            const auto saveOne{[&settings, &formatter, scope](const auto& schemaField) {
                return formatter.write(scope, schemaField.name, settings.*(schemaField.binding));
            }};

            return (saveOne(schemaFields) && ...);
        },
        settings_schema<SettingsType>::fields)};

    return saved && formatter.serialize(outputStream, document);
}

template <has_schema SettingsType, settings_formatter<SettingsType> FormatterType>
bool load_settings(std::istream& inputStream, SettingsType& settings,
                   const FormatterType& formatter) {
    auto document{formatter.parse(inputStream)};
    if (!document.has_value()) {
        return false;
    }

    const auto scope{formatter.root(document.value())};

    return std::apply(
        [&settings, &formatter, scope](const auto&... schemaFields) {
            const auto loadOne{[&settings, &formatter, scope](const auto& schemaField) {
                using field_value_type =
                    typename std::remove_cvref_t<decltype(schemaField)>::value_type;

                const auto loadedValue{
                    formatter.template read<field_value_type>(scope, schemaField.name)};
                if (!loadedValue.has_value()) {
                    return false;
                }

                settings.*(schemaField.binding) = loadedValue.value();

                return true;
            }};

            return (loadOne(schemaFields) && ...);
        },
        settings_schema<SettingsType>::fields);
}

} // namespace

// ---------------------------------------------------------------------------
// Runtime tests
// ---------------------------------------------------------------------------

TEST_CASE("settings_formatter example usage tests", "[tca][engine][settings][settings_formatter]") {
    const line_formatter formatter{};

    SECTION("A schema walk hands every field to the formatter as a key-value pair") {
        const graphics_settings settings{.width = 1280U,
                                         .height = 720U,
                                         .fullscreen = true,
                                         .window_title = "The Clinical Aspect"};

        std::ostringstream outputStream{};

        REQUIRE(save_settings(outputStream, settings, formatter));
        REQUIRE(outputStream.str() ==
                "width = 1280\nheight = 720\nfullscreen = true\n"
                "window_title = The Clinical Aspect\n");
    }

    SECTION("Settings round-trip through the formatter") {
        const graphics_settings written{.width = 2560U,
                                        .height = 1440U,
                                        .fullscreen = false,
                                        .window_title = "The Clinical Aspect"};

        std::ostringstream outputStream{};
        REQUIRE(save_settings(outputStream, written, formatter));

        std::istringstream inputStream{outputStream.str()};
        graphics_settings read{};

        REQUIRE(load_settings(inputStream, read, formatter));
        REQUIRE(read.width == written.width);
        REQUIRE(read.height == written.height);
        REQUIRE(read.fullscreen == written.fullscreen);
        REQUIRE(read.window_title == written.window_title);
    }

    SECTION("Loading from an empty stream fails instead of inventing values") {
        std::istringstream inputStream{};
        gameplay_settings settings{.mouse_sensitivity = 2.5F, .difficulty = 3U};

        REQUIRE_FALSE(load_settings(inputStream, settings, formatter));
    }

    SECTION("A formatter that only handles the types its schema uses is still usable") {
        const gameplay_settings settings{.mouse_sensitivity = 1.5F, .difficulty = 2U};

        std::ostringstream outputStream{};

        REQUIRE(save_settings(outputStream, settings, numeric_formatter{}));
        REQUIRE(outputStream.str() == "mouse_sensitivity=1.5\ndifficulty=2\n");
    }
}

TEST_CASE("settings_store tests", "[tca][engine][settings][settings_store]") {
    using graphics_settings_store = settings_store<graphics_settings, line_formatter>;

    const tca::engine::testing::scoped_temp_directory tempDirectory{"tca-engine-settings-store"};
    const auto settingsFilePath{tempDirectory.path() / "graphics.settings"};

    SECTION("A store built on a missing file creates it from the schema defaults") {
        const graphics_settings_store store{settingsFilePath};

        REQUIRE(std::filesystem::exists(settingsFilePath));
        REQUIRE(store.settings().width == 1920U);
        REQUIRE(store.settings().height == 1080U);
        REQUIRE_FALSE(store.settings().fullscreen);
        REQUIRE(store.settings().window_title.empty());
    }

    SECTION("The file is created along with the folders leading to it") {
        const auto nestedFilePath{tempDirectory.path() / "profiles" / "player-1" /
                                  "graphics.settings"};

        const graphics_settings_store store{nestedFilePath};

        REQUIRE(std::filesystem::exists(nestedFilePath));
    }

    SECTION("Settings written through the store reach the file when it goes out of scope") {
        {
            graphics_settings_store store{settingsFilePath};

            store.settings().width = 2560U;
            store.settings().height = 1440U;
            store.settings().fullscreen = true;
            store.settings().window_title = "The Clinical Aspect";
        }

        const graphics_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.settings().width == 2560U);
        REQUIRE(reloaded.settings().height == 1440U);
        REQUIRE(reloaded.settings().fullscreen);
        REQUIRE(reloaded.settings().window_title == "The Clinical Aspect");
    }

    SECTION("Saving explicitly does not wait for the destructor") {
        graphics_settings_store store{settingsFilePath};
        store.settings().width = 800U;

        REQUIRE(store.save());

        graphics_settings other{};
        std::ifstream inputStream{settingsFilePath};

        REQUIRE(load_settings(inputStream, other, line_formatter{}));
        REQUIRE(other.width == 800U);
    }

    SECTION("Loading again drops in-memory changes that were never saved") {
        graphics_settings_store store{settingsFilePath};
        store.settings().window_title = "unsaved";

        REQUIRE(store.load());
        REQUIRE(store.settings().window_title.empty());
        REQUIRE(store.settings().width == 1920U);
    }

    SECTION(
        "A malformed line is skipped, and the fields it should have carried keep their "
        "defaults") {
        {
            std::ofstream outputStream{settingsFilePath};
            outputStream << "width = 1280\n"
                         << "this line is not a key-value pair\n";
        }

        graphics_settings_store store{settingsFilePath};

        REQUIRE_FALSE(store.load());
        REQUIRE(store.settings().width == 1280U);
        REQUIRE(store.settings().height == 1080U);
        REQUIRE_FALSE(store.settings().fullscreen);
    }

    SECTION("A store with a formatter that only handles its own field types works too") {
        const auto gameplayFilePath{tempDirectory.path() / "gameplay.settings"};

        {
            settings_store<gameplay_settings, numeric_formatter> store{gameplayFilePath};
            store.settings().mouse_sensitivity = 2.5F;
            store.settings().difficulty = 3U;
        }

        std::ifstream inputStream{gameplayFilePath};
        std::string contents{};
        std::getline(inputStream, contents);

        REQUIRE(contents == "mouse_sensitivity=2.5");
    }

    SECTION("A settings file that is already there is loaded, not overwritten") {
        {
            std::ofstream outputStream{settingsFilePath};
            outputStream << "width = 1024\n"
                         << "height = 768\n"
                         << "fullscreen = true\n"
                         << "window_title = Previous session\n";
        }

        const graphics_settings_store store{settingsFilePath};

        REQUIRE(store.settings().width == 1024U);
        REQUIRE(store.settings().height == 768U);
        REQUIRE(store.settings().fullscreen);
        REQUIRE(store.settings().window_title == "Previous session");
    }

    SECTION("Fields are looked up by key, so the order they sit in the file does not matter") {
        {
            std::ofstream outputStream{settingsFilePath};
            outputStream << "window_title = Previous session\n"
                         << "fullscreen = true\n"
                         << "width = 1024\n"
                         << "height = 768\n";
        }

        graphics_settings_store store{settingsFilePath};

        REQUIRE(store.load());
        REQUIRE(store.settings().width == 1024U);
        REQUIRE(store.settings().height == 768U);
        REQUIRE(store.settings().fullscreen);
        REQUIRE(store.settings().window_title == "Previous session");
    }

    SECTION("A key the file does not have costs its own field only") {
        {
            std::ofstream outputStream{settingsFilePath};
            outputStream << "width = 1024\n"
                         << "fullscreen = true\n"
                         << "window_title = Previous session\n";
        }

        graphics_settings_store store{settingsFilePath};

        // "height" is missing, so load() reports the file as incomplete...
        REQUIRE_FALSE(store.load());
        REQUIRE(store.settings().height == 1080U);

        // ...while every other field is read as usual.
        REQUIRE(store.settings().width == 1024U);
        REQUIRE(store.settings().fullscreen);
        REQUIRE(store.settings().window_title == "Previous session");
    }

    SECTION("Keys the schema does not know are ignored") {
        {
            std::ofstream outputStream{settingsFilePath};
            outputStream << "width = 1024\n"
                         << "height = 768\n"
                         << "vsync = true\n"
                         << "fullscreen = true\n"
                         << "window_title = Previous session\n";
        }

        graphics_settings_store store{settingsFilePath};

        REQUIRE(store.load());
        REQUIRE(store.settings().width == 1024U);
    }

    SECTION("Loading a file that is not there fails and leaves the defaults") {
        graphics_settings_store store{settingsFilePath};
        store.settings().width = 800U;

        std::filesystem::remove(settingsFilePath);

        REQUIRE_FALSE(store.load());
        REQUIRE(store.settings().width == 1920U);
    }

    SECTION("Saving to a path that cannot be written reports failure instead of throwing") {
        const auto directoryPath{tempDirectory.path() / "not-a-file"};
        std::filesystem::create_directories(directoryPath);

        const graphics_settings_store store{directoryPath};

        REQUIRE_FALSE(store.save());
    }

    SECTION("The store remembers the file it is bound to") {
        const graphics_settings_store store{settingsFilePath};

        REQUIRE(store.file_path() == settingsFilePath);
    }

    SECTION("A moved-to store keeps the settings and the file of the one it came from") {
        graphics_settings_store store{settingsFilePath};
        store.settings().window_title = "The Clinical Aspect";

        const graphics_settings_store movedStore{std::move(store)};

        REQUIRE(movedStore.file_path() == settingsFilePath);
        REQUIRE(movedStore.settings().window_title == "The Clinical Aspect");
    }
}

TEST_CASE("settings_store nested schema tests", "[tca][engine][settings][settings_store]") {
    using interface_settings_store = settings_store<interface_settings, line_formatter>;

    const tca::engine::testing::scoped_temp_directory tempDirectory{"tca-engine-nested-settings"};
    const auto settingsFilePath{tempDirectory.path() / "interface.settings"};

    SECTION("A nested struct takes its defaults from its own schema") {
        const interface_settings_store store{settingsFilePath};

        REQUIRE(store.settings().window.width == 1920U);
        REQUIRE(store.settings().window.height == 1080U);
        REQUIRE(store.settings().hud.width == 320U);
        REQUIRE(store.settings().hud.height == 200U);
        REQUIRE(store.settings().hud.visible);
        REQUIRE(store.settings().language.empty());
    }

    SECTION("Every nested field is written under a scope of its own") {
        interface_settings_store store{settingsFilePath};
        store.settings().language = "en";

        REQUIRE(store.save());

        std::ifstream inputStream{settingsFilePath};
        const std::string contents{std::istreambuf_iterator<char>{inputStream},
                                   std::istreambuf_iterator<char>{}};

        REQUIRE(contents ==
                "window.width = 1920\n"
                "window.height = 1080\n"
                "hud.width = 320\n"
                "hud.height = 200\n"
                "hud.visible = true\n"
                "language = en\n");
    }

    SECTION("Two nested structs sharing a field name do not collide") {
        {
            interface_settings_store store{settingsFilePath};

            store.settings().window.width = 2560U;
            store.settings().hud.width = 640U;
        }

        const interface_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.settings().window.width == 2560U);
        REQUIRE(reloaded.settings().hud.width == 640U);
    }

    SECTION("A nested scope the file does not hold leaves that struct on its defaults") {
        {
            std::ofstream outputStream{settingsFilePath};
            outputStream << "window.width = 800\n"
                         << "window.height = 600\n"
                         << "language = it\n";
        }

        interface_settings_store store{settingsFilePath};

        REQUIRE_FALSE(store.load());
        REQUIRE(store.settings().window.width == 800U);
        REQUIRE(store.settings().window.height == 600U);
        REQUIRE(store.settings().language == "it");
        REQUIRE(store.settings().hud.width == 320U);
        REQUIRE(store.settings().hud.visible);
    }
}

namespace {

// A formatter that gets part-way through writing and then gives up: what a full disk, or a
// crash between two of the writes a save is made of, looks like from here.
struct interrupted_formatter : line_formatter {
    bool serialize(std::ostream& outputStream, const document_type& document) const {
        // Half a line, so a file left in this state would not even parse as the format it claims
        // to be.
        outputStream << document.front().first << " = ";

        return false;
    }
};

// The same, but leaving through an exception rather than a return value.
struct throwing_formatter : line_formatter {
    bool serialize(std::ostream& outputStream, const document_type& document) const {
        outputStream << document.front().first << " = ";

        throw std::runtime_error{"interrupted part-way through the save"};
    }
};

[[nodiscard]] std::size_t files_in(const std::filesystem::path& directoryPath) {
    return static_cast<std::size_t>(std::distance(
        std::filesystem::directory_iterator{directoryPath}, std::filesystem::directory_iterator{}));
}

} // namespace

TEST_CASE("settings_store crash safety tests", "[tca][engine][settings][settings_store]") {
    using graphics_settings_store = settings_store<graphics_settings, line_formatter>;
    using interrupted_settings_store = settings_store<graphics_settings, interrupted_formatter>;
    using throwing_settings_store = settings_store<graphics_settings, throwing_formatter>;

    const tca::engine::testing::scoped_temp_directory tempDirectory{
        "tca-engine-settings-crash-safety"};
    const auto settingsFilePath{tempDirectory.path() / "graphics.settings"};

    // The settings a player would be sorry to lose, saved before anything goes wrong.
    const auto save_previous_settings{[&settingsFilePath] {
        graphics_settings_store store{settingsFilePath};
        store.settings().width = 2560U;
        store.settings().window_title = "Previous session";

        REQUIRE(store.save());
    }};

    SECTION("A save that gives up part-way through leaves the previous settings readable") {
        save_previous_settings();

        {
            interrupted_settings_store store{settingsFilePath};
            store.settings().width = 800U;

            REQUIRE_FALSE(store.save());
        }

        const graphics_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.settings().width == 2560U);
        REQUIRE(reloaded.settings().window_title == "Previous session");
    }

    SECTION("A save that throws part-way through leaves the previous settings readable") {
        save_previous_settings();

        {
            throwing_settings_store store{settingsFilePath};
            store.settings().width = 800U;

            REQUIRE_FALSE(store.save());
        }

        const graphics_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.settings().width == 2560U);
        REQUIRE(reloaded.settings().window_title == "Previous session");
    }

    SECTION("An interrupted save leaves no half-written file behind") {
        save_previous_settings();

        {
            const interrupted_settings_store store{settingsFilePath};

            REQUIRE_FALSE(store.save());
        }

        // The settings file, and nothing else: no temporary the next save would have to reason
        // about, and no file a backup would pick up.
        REQUIRE(files_in(tempDirectory.path()) == 1U);
        REQUIRE(std::filesystem::exists(settingsFilePath));
    }

    SECTION("A first save that is interrupted creates no settings file at all") {
        {
            const interrupted_settings_store store{settingsFilePath};

            REQUIRE_FALSE(store.save());
        }

        REQUIRE_FALSE(std::filesystem::exists(settingsFilePath));
        REQUIRE(files_in(tempDirectory.path()) == 0U);
    }
}

TEST_CASE("settings_store load result tests", "[tca][engine][settings][settings_store]") {
    using graphics_settings_store = settings_store<graphics_settings, line_formatter>;

    const tca::engine::testing::scoped_temp_directory tempDirectory{
        "tca-engine-settings-load-result"};
    const auto settingsFilePath{tempDirectory.path() / "graphics.settings"};

    SECTION("A file holding every field of the schema reads as complete") {
        {
            graphics_settings_store store{settingsFilePath};
            store.settings().window_title = "The Clinical Aspect";
        }

        graphics_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == settings_load_result::complete);
    }

    SECTION("A file written before a field existed reads as incomplete, not as a failure") {
        {
            std::ofstream outputStream{settingsFilePath};
            outputStream << "width = 1024\n"
                         << "height = 768\n"
                         << "fullscreen = true\n";
        }

        graphics_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == settings_load_result::incomplete);
        REQUIRE(store.settings().width == 1024U);
        REQUIRE(store.settings().window_title.empty());
    }

    SECTION("A file that is not there reads as missing") {
        graphics_settings_store store{settingsFilePath};
        std::filesystem::remove(settingsFilePath);

        REQUIRE(store.load_file() == settings_load_result::missing);
        REQUIRE(store.settings().width == 1920U);
    }
}

namespace {

// A group in the shape of an input action map: a fixed set of named entries with no
// settings_schema of its own, each entry a leaf the formatter stores.
struct channel_levels {
    static constexpr std::array<std::string_view, 3> names{"red", "green", "blue"};

    std::array<std::uint32_t, 3> levels{10U, 20U, 30U};
};

// The same, with entries a numeric formatter cannot store.
struct channel_labels {
    static constexpr std::array<std::string_view, 2> names{"primary", "secondary"};

    std::array<std::string, 2> labels{};
};

struct palette_settings {
    std::uint32_t version{};
    channel_levels levels{};
};

struct labelled_palette_settings {
    channel_labels labels{};
};

// Shared by both test groups: writes every named entry, reads back the ones the file has.
template <typename GroupType, auto EntriesMember>
struct indexed_group_traits {
    template <typename EntryWriter>
    static bool write_entries(const GroupType& group, EntryWriter&& write) {
        for (std::size_t index{0}; index < GroupType::names.size(); ++index) {
            if (!write(GroupType::names.at(index), (group.*EntriesMember).at(index))) {
                return false;
            }
        }

        return true;
    }

    template <typename EntryReader>
    static bool read_entries(GroupType& group, EntryReader&& read) {
        bool everyEntryRead{true};
        for (std::size_t index{0}; index < GroupType::names.size(); ++index) {
            const auto entry{read(GroupType::names.at(index))};
            if (entry.has_value()) {
                (group.*EntriesMember).at(index) = entry.value();
            } else {
                everyEntryRead = false;
            }
        }

        return everyEntryRead;
    }
};

[[nodiscard]] std::vector<std::string> lines_of(const std::filesystem::path& filePath) {
    std::ifstream inputStream{filePath};

    std::vector<std::string> lines{};
    std::string line{};
    while (std::getline(inputStream, line)) {
        lines.push_back(line);
    }

    return lines;
}

} // namespace

template <>
struct tca::engine::settings::settings_group_traits<channel_levels>
    : indexed_group_traits<channel_levels, &channel_levels::levels> {
    using entry_type = std::uint32_t;
};

template <>
struct tca::engine::settings::settings_group_traits<channel_labels>
    : indexed_group_traits<channel_labels, &channel_labels::labels> {
    using entry_type = std::string;
};

template <>
struct tca::engine::settings::settings_schema<palette_settings> {
    static constexpr auto fields{std::make_tuple(
        field{.name = "version", .binding = &palette_settings::version, .default_value = 1U},
        group_field{.name = "levels", .binding = &palette_settings::levels})};
};

template <>
struct tca::engine::settings::settings_schema<labelled_palette_settings> {
    static constexpr auto fields{std::make_tuple(
        group_field{.name = "labels", .binding = &labelled_palette_settings::labels})};
};

// A group is recognised by its traits, and only by them.
static_assert(has_group_traits<channel_levels>);
static_assert(has_group_traits<channel_labels>);
static_assert(!has_group_traits<std::uint32_t>);
static_assert(!has_group_traits<window_settings>);

// A group is declared with group_field, which has no default_value, and only a group is.
template <typename FieldBindingType>
concept declarable_as_field = requires { typename field<FieldBindingType>; };

template <typename FieldBindingType>
concept declarable_as_group_field = requires { typename group_field<FieldBindingType>; };

static_assert(!declarable_as_field<decltype(&palette_settings::levels)>);
static_assert(declarable_as_group_field<decltype(&palette_settings::levels)>);
static_assert(declarable_as_field<decltype(&palette_settings::version)>);
static_assert(!declarable_as_group_field<decltype(&palette_settings::version)>);

// What a formatter has to handle for a group is its entry type, not the group itself.
static_assert(settings_formatter<line_formatter, palette_settings>);
static_assert(settings_formatter<numeric_formatter, palette_settings>);
static_assert(settings_formatter<line_formatter, labelled_palette_settings>);
static_assert(!settings_formatter<numeric_formatter, labelled_palette_settings>);

TEST_CASE("settings_store group tests", "[tca][engine][settings][settings_store][settings_group]") {
    using palette_settings_store = settings_store<palette_settings, line_formatter>;

    const tca::engine::testing::scoped_temp_directory tempDirectory{"tca-engine-settings-group"};
    const auto settingsFilePath{tempDirectory.path() / "palette.settings"};

    const auto writeFile{[&settingsFilePath](const std::string_view contents) {
        std::ofstream outputStream{settingsFilePath};
        outputStream << contents;
    }};

    SECTION("Every entry of a group is written under a scope of its own, in the traits' order") {
        const palette_settings_store store{settingsFilePath};

        REQUIRE(lines_of(settingsFilePath) == std::vector<std::string>{
                                                  "version = 1",
                                                  "levels.red = 10",
                                                  "levels.green = 20",
                                                  "levels.blue = 30",
                                              });
    }

    SECTION("A group round-trips through the file") {
        {
            palette_settings_store store{settingsFilePath};
            store.settings().levels.levels = {1U, 2U, 3U};
        }

        palette_settings_store reloaded{settingsFilePath};

        REQUIRE(reloaded.load_file() == settings_load_result::complete);
        REQUIRE(reloaded.settings().levels.levels == std::array{1U, 2U, 3U});
    }

    SECTION("An entry the file does not have keeps its default, and the load is incomplete") {
        writeFile(
            "version = 1\n"
            "levels.red = 99\n"
            "levels.blue = 5\n");

        palette_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == settings_load_result::incomplete);
        REQUIRE(store.settings().levels.levels == std::array{99U, 20U, 5U});
    }

    SECTION("An entry the formatter cannot read keeps its default") {
        writeFile(
            "version = 1\n"
            "levels.red = 1\n"
            "levels.green = not a number\n"
            "levels.blue = 3\n");

        palette_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == settings_load_result::incomplete);
        REQUIRE(store.settings().levels.levels == std::array{1U, 20U, 3U});
    }

    SECTION("A file without the group leaves it on its defaults and keeps the other fields") {
        writeFile("version = 2\n");

        palette_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == settings_load_result::incomplete);
        REQUIRE(store.settings().version == 2U);
        REQUIRE(store.settings().levels.levels == std::array{10U, 20U, 30U});
    }

    SECTION("Keys under the group's scope the traits never ask for are ignored") {
        writeFile(
            "version = 1\n"
            "levels.red = 1\n"
            "levels.green = 2\n"
            "levels.blue = 3\n"
            "levels.alpha = 4\n");

        palette_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == settings_load_result::complete);
        REQUIRE(store.settings().levels.levels == std::array{1U, 2U, 3U});
    }

    SECTION("An entry of the same name outside the group's scope is not one of its entries") {
        writeFile(
            "version = 1\n"
            "red = 1\n"
            "levels.green = 2\n"
            "levels.blue = 3\n");

        palette_settings_store store{settingsFilePath};

        REQUIRE(store.load_file() == settings_load_result::incomplete);
        REQUIRE(store.settings().levels.levels == std::array{10U, 2U, 3U});
    }

    SECTION("Loading again drops unsaved changes to entries the file does not have") {
        writeFile(
            "version = 1\n"
            "levels.red = 1\n"
            "levels.blue = 3\n");

        palette_settings_store store{settingsFilePath};
        store.settings().levels.levels = {7U, 8U, 9U};

        REQUIRE(store.load_file() == settings_load_result::incomplete);
        REQUIRE(store.settings().levels.levels == std::array{1U, 20U, 3U});
    }

    SECTION("A group of string entries round-trips through a formatter that stores strings") {
        const auto labelsFilePath{tempDirectory.path() / "labels.settings"};

        {
            settings_store<labelled_palette_settings, line_formatter> store{labelsFilePath};
            store.settings().labels.labels = {"crimson", "teal"};
        }

        REQUIRE(lines_of(labelsFilePath) == std::vector<std::string>{
                                                "labels.primary = crimson",
                                                "labels.secondary = teal",
                                            });

        const settings_store<labelled_palette_settings, line_formatter> reloaded{labelsFilePath};

        REQUIRE(reloaded.settings().labels.labels == std::array<std::string, 2>{"crimson", "teal"});
    }
}
