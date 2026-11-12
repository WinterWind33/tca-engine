// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "settings/settings-schema.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

// C++ STL
#include <concepts>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>

using namespace tca::engine::settings;

namespace {

// A settings struct in the shape client code is expected to declare it: a plain aggregate of
// values, with no engine-specific base class or macro.
struct video_settings {
    std::uint32_t width{};
    std::uint32_t height{};
    bool fullscreen{};
    std::string window_title{};
};

// A literal-type settings struct, used to check that a schema is usable in constant expressions.
struct audio_settings {
    float master_volume{};
    float music_volume{};
    bool muted{};
};

// A settings struct deliberately left without a schema specialization, to check that has_schema
// rejects it.
struct unregistered_settings {
    int value{};
};

} // namespace

// The schema specializations are written the way client code would write them: in the engine
// namespace, exposing a `fields` tuple of `field` objects.
template <>
struct tca::engine::settings::settings_schema<video_settings> {
    static constexpr auto fields{std::make_tuple(
        field{.name = "width", .binding = &video_settings::width, .default_value = 1920U},
        field{.name = "height", .binding = &video_settings::height, .default_value = 1080U},
        field{.name = "fullscreen", .binding = &video_settings::fullscreen, .default_value = false},
        field{.name = "window_title", .binding = &video_settings::window_title})};
};

template <>
struct tca::engine::settings::settings_schema<audio_settings> {
    static constexpr auto fields{
        std::make_tuple(field{.name = "master_volume",
                              .binding = &audio_settings::master_volume,
                              .default_value = 0.8F},
                        field{.name = "music_volume",
                              .binding = &audio_settings::music_volume,
                              .default_value = 0.5F},
                        field{.name = "muted", .binding = &audio_settings::muted})};
};

// ---------------------------------------------------------------------------
// Static tests
// ---------------------------------------------------------------------------

// struct_field_type_t peels the class off a pointer-to-data-member and yields the field type.
static_assert(
    std::same_as<details::struct_field_type_t<std::uint32_t video_settings::*>, std::uint32_t>);
static_assert(std::same_as<details::struct_field_type_t<bool video_settings::*>, bool>);
static_assert(
    std::same_as<details::struct_field_type_t<std::string video_settings::*>, std::string>);

// It also sees through cv-qualification of the pointer-to-member type itself.
static_assert(std::same_as<details::struct_field_type_t<std::uint32_t video_settings::* const>,
                           std::uint32_t>);

// field exposes the bound field's type, not the pointer-to-member type.
static_assert(std::same_as<field<std::uint32_t video_settings::*>::value_type, std::uint32_t>);
static_assert(std::same_as<field<std::string video_settings::*>::value_type, std::string>);

// field is constrained on std::is_member_object_pointer_v, so a plain value or a plain pointer
// cannot be used as a binding. The constraint is checked directly here rather than through a
// requires-expression, since a failed class-template constraint is a hard error, not SFINAE.
static_assert(std::is_member_object_pointer_v<decltype(&video_settings::width)>);
static_assert(!std::is_member_object_pointer_v<std::uint32_t>);
static_assert(!std::is_member_object_pointer_v<std::uint32_t*>);

// Class template argument deduction picks the pointer-to-member type up from the binding.
static_assert(std::same_as<decltype(field{.name = "width", .binding = &video_settings::width}),
                           field<std::uint32_t video_settings::*>>);

// A field is usable in constant expressions, its optional default included.
static_assert(field{.name = "width", .binding = &video_settings::width}.name == "width");
static_assert(!field{.name = "width", .binding = &video_settings::width}.default_value.has_value());
static_assert(field{.name = "width", .binding = &video_settings::width, .default_value = 1920U}
                  .default_value.value() == 1920U);

// has_schema is satisfied only by types with a settings_schema specialization exposing fields.
static_assert(has_schema<video_settings>);
static_assert(has_schema<audio_settings>);
static_assert(!has_schema<unregistered_settings>);
static_assert(!has_schema<int>);

// The schema itself is a constant expression too.
static_assert(std::tuple_size_v<decltype(settings_schema<video_settings>::fields)> == 4);
static_assert(std::get<0>(settings_schema<video_settings>::fields).name == "width");
static_assert(std::get<3>(settings_schema<video_settings>::fields).name == "window_title");
static_assert(!std::get<3>(settings_schema<video_settings>::fields).default_value.has_value());

namespace {

// Example usage: walk a schema and write every field that declares a default back into an
// instance of the settings struct. This is the kind of generic algorithm the schema exists for.
template <has_schema SettingsType>
constexpr void apply_defaults(SettingsType& settings) {
    std::apply(
        [&settings](const auto&... schemaFields) {
            const auto applyOne{[&settings](const auto& schemaField) {
                if (schemaField.default_value.has_value()) {
                    settings.*(schemaField.binding) = schemaField.default_value.value();
                }
            }};

            (applyOne(schemaFields), ...);
        },
        settings_schema<SettingsType>::fields);
}

// Example usage: look a field's value up by its schema name.
template <typename ValueType, has_schema SettingsType>
constexpr ValueType read_field(const SettingsType& settings, const fixed_string name) {
    ValueType result{};

    std::apply(
        [&settings, name, &result](const auto&... schemaFields) {
            const auto readOne{[&settings, name, &result](const auto& schemaField) {
                using field_value_type =
                    typename std::remove_cvref_t<decltype(schemaField)>::value_type;

                if constexpr (std::same_as<field_value_type, ValueType>) {
                    if (schemaField.name == name) {
                        result = settings.*(schemaField.binding);
                    }
                }
            }};

            (readOne(schemaFields), ...);
        },
        settings_schema<SettingsType>::fields);

    return result;
}

// A schema walk done at compile time: only possible because both `field` and the `fields` tuple
// are literal types.
consteval audio_settings make_default_audio_settings() {
    audio_settings settings{};
    apply_defaults(settings);

    return settings;
}

} // namespace

static_assert(make_default_audio_settings().master_volume == 0.8F);
static_assert(make_default_audio_settings().music_volume == 0.5F);
static_assert(!make_default_audio_settings().muted);

// ---------------------------------------------------------------------------
// Runtime tests
// ---------------------------------------------------------------------------

TEST_CASE("settings_schema example usage tests", "[tca][engine][settings][settings_schema]") {
    SECTION("Applying the schema defaults fills every field that declares one") {
        video_settings settings{};

        apply_defaults(settings);

        REQUIRE(settings.width == 1920U);
        REQUIRE(settings.height == 1080U);
        REQUIRE_FALSE(settings.fullscreen);
    }

    SECTION("Fields without a default are left untouched") {
        video_settings settings{.window_title = "The Clinical Aspect"};

        apply_defaults(settings);

        REQUIRE(settings.window_title == "The Clinical Aspect");
    }

    SECTION("Applying the defaults overwrites previously set values") {
        video_settings settings{.width = 800U, .height = 600U, .fullscreen = true};

        apply_defaults(settings);

        REQUIRE(settings.width == 1920U);
        REQUIRE(settings.height == 1080U);
        REQUIRE_FALSE(settings.fullscreen);
    }

    SECTION("Fields can be read back through their schema name") {
        const video_settings settings{.width = 1280U,
                                      .height = 720U,
                                      .fullscreen = true,
                                      .window_title = "The Clinical Aspect"};

        REQUIRE(read_field<std::uint32_t>(settings, "width") == 1280U);
        REQUIRE(read_field<std::uint32_t>(settings, "height") == 720U);
        REQUIRE(read_field<bool>(settings, "fullscreen"));
        REQUIRE(read_field<std::string>(settings, "window_title") == "The Clinical Aspect");
    }

    SECTION("Reading an unknown field name yields a value-initialized result") {
        const video_settings settings{.width = 1280U};

        REQUIRE(read_field<std::uint32_t>(settings, "depth") == 0U);
    }

    SECTION("Bindings write through to the settings instance") {
        video_settings settings{};

        constexpr auto widthField{std::get<0>(settings_schema<video_settings>::fields)};
        settings.*(widthField.binding) = 2560U;

        REQUIRE(settings.width == 2560U);
    }
}
