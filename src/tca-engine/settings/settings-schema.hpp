// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <optional>
#include <string_view>
#include <type_traits>

namespace tca::engine::settings {

namespace details {

template <typename T>
struct struct_field_type;

template <typename Class, typename Field>
struct struct_field_type<Field Class::*> {
    using type = Field;
};

template <typename T>
using struct_field_type_t = typename struct_field_type<std::remove_cv_t<T>>::type;

} // namespace details

using fixed_string = std::string_view;

/**
 * @brief The extension point for a settings value made of named entries whose names are only
 *  known at run time, or come from somewhere other than a settings_schema (an input action
 *  map, say). Such a value is declared in a schema with group_field, and written as a
 *  subdocument, like a nested settings struct, with one key per entry; every entry is a leaf of
 *  the same entry_type, which the formatter has to handle as it would any field.
 *
 * A specialization provides:
 *
 * @code
 *  template <>
 *  struct tca::engine::settings::settings_group_traits<my_group> {
 *      // The type every entry is stored as: a leaf the formatter can write and read.
 *      using entry_type = std::vector<std::string>;
 *
 *      // Calls write(name, entry) once per entry, name a fixed_string and entry an entry_type,
 *      // and stops at the first one returning false. Returns whether every write succeeded.
 *      template <typename EntryWriter>
 *      static bool write_entries(const my_group& group, EntryWriter&& write);
 *
 *      // Calls read(name) once per entry it expects, getting back a std::optional<entry_type>:
 *      // empty when the file has no such entry or one the formatter could not read. Returns
 *      // whether every expected entry was read. The group it is given is already at its
 *      // default, so an entry it cannot read can simply be left alone.
 *      template <typename EntryReader>
 *      static bool read_entries(my_group& group, EntryReader&& read);
 *  };
 * @endcode
 *
 * Keys in the file that read_entries() never asks for are ignored, as for a nested struct.
 *
 * @tparam GroupType The settings field type.
 */
template <typename GroupType>
struct settings_group_traits {};

/**
 * @brief Satisfied by a type with a settings_group_traits specialization.
 */
template <typename T>
concept has_group_traits = requires { typename settings_group_traits<T>::entry_type; };

/**
 * @brief One field of a settings_schema: a leaf value, or a nested settings struct.
 *
 *  A group (a type with settings_group_traits) is declared with group_field instead, and is
 *  refused here.
 */
template <typename FieldBindingType>
    requires std::is_member_object_pointer_v<FieldBindingType> &&
             (!has_group_traits<details::struct_field_type_t<FieldBindingType>>)
struct field {
    using value_type = details::struct_field_type_t<FieldBindingType>;

    fixed_string name{};
    FieldBindingType binding{};
    std::optional<value_type> default_value{};
};

/**
 * @brief One field of a settings_schema holding a group (a type with settings_group_traits).
 *
 *  It has no default_value: a group's default is a value-initialized group, which the store
 *  resets it to before every load, so in-memory changes that were never saved don't survive a
 *  load. That also keeps the group itself out of the constexpr schema, which a type with a
 *  non-trivial destructor (an input::action_map, say) can't live in.
 */
template <typename FieldBindingType>
    requires std::is_member_object_pointer_v<FieldBindingType> &&
             has_group_traits<details::struct_field_type_t<FieldBindingType>>
struct group_field {
    using value_type = details::struct_field_type_t<FieldBindingType>;

    fixed_string name{};
    FieldBindingType binding{};
};

template <typename SettingsStructType>
struct settings_schema {};

template <typename T>
concept has_schema = requires { settings_schema<T>::fields; };

} // namespace tca::engine::settings
