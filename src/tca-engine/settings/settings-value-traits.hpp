// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <utility>

#include "core/utilities/ranged-value.hpp"

namespace tca::engine::settings {

/**
 * @brief The leaf extension point of the settings system: how a value type is reduced to
 *  something a formatter knows how to write, and rebuilt from what a formatter read back.
 *
 * Nested settings structs are not this: those are handled by giving them their own
 * settings_schema, which settings_store recurses into. This is for the leaves — a value that
 * is conceptually one setting, but whose C++ type no formatter can be expected to know.
 *
 * The primary template hands the value straight through, so a type a formatter already
 * handles (an arithmetic type, a string, or anything a JSON formatter's own hooks cover)
 * needs nothing here.
 *
 * to_storage() only has to return something the formatter can store as a storage_type: this
 * one returns a reference into the value it was given, since there is nothing to convert and
 * a string is worth not copying. Callers use the result there and then.
 *
 * @tparam ValueType The settings field type.
 */
template <typename ValueType>
struct settings_value_traits {
    using storage_type = ValueType;

    static constexpr const storage_type& to_storage(const ValueType& value) noexcept {
        return value;
    }

    static constexpr ValueType from_storage(storage_type value) {
        return value;
    }
};

/**
 * @brief Stores a ranged value as its underlying value type.
 *
 * from_storage() throws core::ranged_value_exception when the file holds a value outside the
 * range. Formatters are expected to catch it and report the field as unreadable, which leaves
 * the schema default in place — so a hand-edited file cannot push a setting out of bounds.
 */
// Plain `auto` bounds: MSVC fails to match a partial specialization declared with
// `RangeBoundFor<Type> auto` ones (see core::details::ranged_value_impl).
template <core::RangeableType Type, auto Min, auto Max, core::rv_bound_policy Policy>
struct settings_value_traits<core::ranged_value<Type, Min, Max, Policy>> {
    using value_type = core::ranged_value<Type, Min, Max, Policy>;
    using storage_type = Type;

    static constexpr storage_type to_storage(const value_type& value) noexcept {
        return value.get();
    }

    static constexpr value_type from_storage(const storage_type value) {
        return value_type{value};
    }
};

} // namespace tca::engine::settings
