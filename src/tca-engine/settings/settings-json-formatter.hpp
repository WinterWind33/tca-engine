// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <concepts>
#include <cstdint>
#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "settings/settings-io.hpp"
#include "settings/settings-schema.hpp"
#include "settings/settings-value-traits.hpp"

namespace tca::engine::settings::formatters {

/**
 * @brief A settings file held in memory as a JSON tree.
 *
 * What the tree is made of is deliberately not said here: the JSON library lives in the
 * formatter's translation unit, so nothing that includes this header takes a dependency on it,
 * or on the version of it the engine happens to be built against.
 *
 * The document is move-only, since it owns the tree.
 */
class settings_json_document {
public:
    settings_json_document();
    ~settings_json_document();

    settings_json_document(const settings_json_document& other) = delete;
    settings_json_document& operator=(const settings_json_document& other) = delete;

    settings_json_document(settings_json_document&& other) noexcept;
    settings_json_document& operator=(settings_json_document&& other) noexcept;

private:
    friend class settings_json_formatter;

    struct json_tree;

    std::unique_ptr<json_tree> m_tree{};
};

/**
 * @brief One object of a settings_json_document: where a settings struct's own fields sit.
 *
 * A default-constructed scope names no object at all, which is what a nested struct the file
 * does not hold comes back as: every read through it is empty, so those fields keep their
 * defaults. Only the formatter can make a scope that names something.
 */
class settings_json_scope {
public:
    settings_json_scope() noexcept = default;

private:
    friend class settings_json_formatter;

    explicit settings_json_scope(void* const node) noexcept : m_node{node} {}

    void* m_node{nullptr};
};

namespace details {

// The types the document stores directly. Everything else has to name one of them as its
// settings_value_traits::storage_type to be writable.
template <typename StorageType>
concept json_storage_type =
    std::same_as<StorageType, bool> || std::integral<StorageType> ||
    std::floating_point<StorageType> || std::same_as<StorageType, std::string> ||
    std::same_as<StorageType, std::vector<std::string>>;

template <typename ValueType>
concept json_settings_value =
    json_storage_type<typename settings_value_traits<ValueType>::storage_type>;

} // namespace details

/**
 * @brief Writes the settings out as a JSON object, one member per field, and reads them back by
 *  key.
 *
 * Nested settings structs become nested JSON objects, so two of them may each hold a `width`
 * without the file having to spell out a path. Nothing here knows about nesting though: the
 * store asks for a subdocument, and this formatter hands back the child object.
 *
 * The JSON library is an implementation detail of this class: it appears in no signature and in
 * no header, so a game that includes this pays for neither. What crosses the boundary is a
 * closed set of primitives — bool, the integral and floating-point types, std::string, and
 * std::vector<std::string> as a JSON array of strings — and
 * a field of any other type reaches them through a settings_value_traits specialization, as
 * core::ranged_value does. That is the one extension point: a game adds a type to its settings
 * by saying which primitive stores it, not by deriving from this class.
 *
 * A key that is not in the file, or that holds something the field cannot be made from, is
 * reported as unreadable rather than thrown: settings_store leaves the schema default in place
 * for it.
 */
class settings_json_formatter {
public:
    using document_type = settings_json_document;
    using scope_type = settings_json_scope;

    settings_json_formatter() noexcept = default;

    /**
     * @brief Constructs the formatter.
     * @param indentation How many spaces each nesting level is indented by when writing, or -1
     *  for the most compact form.
     */
    explicit settings_json_formatter(const int indentation) noexcept : m_indentation{indentation} {}

    /**
     * @brief An empty JSON object, ready to take the settings.
     */
    [[nodiscard]] document_type make_document() const;

    /**
     * @brief Reads a JSON object back from the stream.
     * @return The document, or nothing if the stream does not hold well-formed JSON, or holds
     *  something other than an object at its root.
     */
    [[nodiscard]] std::optional<document_type> parse(std::istream& inputStream) const;

    /**
     * @brief Writes the document out, indented as the constructor was told to.
     */
    bool serialize(std::ostream& outputStream, const document_type& document) const;

    /**
     * @brief The document's root object, where the top-level fields live.
     */
    [[nodiscard]] scope_type root(document_type& document) const noexcept;

    /**
     * @brief The child object under the given key, created if it is not there yet.
     * @return A scope naming nothing when the parent is not an object, or when the key is taken
     *  by something that is not one: every field under it then keeps its default.
     */
    [[nodiscard]] scope_type subdocument(scope_type scope, fixed_string key) const;

    /**
     * @brief Stores the value under the given key of the scope.
     */
    template <typename ValueType>
        requires details::json_settings_value<ValueType>
    bool write(const scope_type scope, const fixed_string key, const ValueType& value) const {
        using value_traits = settings_value_traits<ValueType>;
        using storage_type = typename value_traits::storage_type;

        // Bound to a reference so that traits returning one do not copy, and traits building a
        // fresh value still outlive the call below.
        const storage_type& storedValue{value_traits::to_storage(value)};

        // Dispatched by hand rather than by overload resolution: an int is convertible to every
        // one of the writers below, so letting the compiler choose would be ambiguous at best
        // and silently wrong at worst (a string literal, say, prefers bool over a string).
        if constexpr (std::same_as<storage_type, bool>) {
            return write_boolean(scope, key, storedValue);
        } else if constexpr (std::same_as<storage_type, std::string>) {
            return write_string(scope, key, storedValue);
        } else if constexpr (std::same_as<storage_type, std::vector<std::string>>) {
            return write_string_array(scope, key, storedValue);
        } else if constexpr (std::floating_point<storage_type>) {
            return write_floating(scope, key, static_cast<double>(storedValue));
        } else if constexpr (std::is_signed_v<storage_type>) {
            return write_signed(scope, key, static_cast<std::int64_t>(storedValue));
        } else {
            return write_unsigned(scope, key, static_cast<std::uint64_t>(storedValue));
        }
    }

    /**
     * @brief Reads back the value stored under the given key of the scope.
     * @return Nothing when the key is not there, when it holds something the value cannot be
     *  made from, or when the value is one its own type rejects (an out-of-range ranged_value,
     *  say).
     */
    template <typename ValueType>
        requires details::json_settings_value<ValueType>
    [[nodiscard]] std::optional<ValueType> read(const scope_type scope,
                                                const fixed_string key) const {
        using value_traits = settings_value_traits<ValueType>;

        const auto storedValue{read_storage<typename value_traits::storage_type>(scope, key)};
        if (!storedValue.has_value()) {
            return std::nullopt;
        }

        try {
            return value_traits::from_storage(storedValue.value());
        } catch (...) {
            // The stored value is one the field's own type will not take.
            return std::nullopt;
        }
    }

private:
    // The whole of the boundary to the JSON library: named rather than overloaded, so that the
    // dispatch above says which one it means instead of leaving it to conversions.
    bool write_boolean(scope_type scope, fixed_string key, bool value) const;
    bool write_signed(scope_type scope, fixed_string key, std::int64_t value) const;
    bool write_unsigned(scope_type scope, fixed_string key, std::uint64_t value) const;
    bool write_floating(scope_type scope, fixed_string key, double value) const;
    bool write_string(scope_type scope, fixed_string key, std::string_view value) const;
    bool write_string_array(scope_type scope, fixed_string key,
                            const std::vector<std::string>& value) const;

    [[nodiscard]] std::optional<bool> read_boolean(scope_type scope, fixed_string key) const;
    [[nodiscard]] std::optional<std::int64_t> read_signed(scope_type scope, fixed_string key) const;
    [[nodiscard]] std::optional<std::uint64_t> read_unsigned(scope_type scope,
                                                             fixed_string key) const;
    [[nodiscard]] std::optional<double> read_floating(scope_type scope, fixed_string key) const;
    [[nodiscard]] std::optional<std::string> read_string(scope_type scope, fixed_string key) const;

    // Nothing unless the key holds an array whose every element is a string: one element of any
    // other type makes the whole array unreadable, rather than a shorter list.
    [[nodiscard]] std::optional<std::vector<std::string>> read_string_array(scope_type scope,
                                                                            fixed_string key) const;

    template <typename StorageType>
    [[nodiscard]] std::optional<StorageType> read_storage(const scope_type scope,
                                                          const fixed_string key) const {
        if constexpr (std::same_as<StorageType, bool>) {
            return read_boolean(scope, key);
        } else if constexpr (std::same_as<StorageType, std::string>) {
            return read_string(scope, key);
        } else if constexpr (std::same_as<StorageType, std::vector<std::string>>) {
            return read_string_array(scope, key);
        } else if constexpr (std::floating_point<StorageType>) {
            return narrowed<StorageType>(read_floating(scope, key));
        } else if constexpr (std::is_signed_v<StorageType>) {
            return narrowed<StorageType>(read_signed(scope, key));
        } else {
            return narrowed<StorageType>(read_unsigned(scope, key));
        }
    }

    // A number the file holds is only usable if the field can hold it: one that cannot is
    // treated the way a string in its place would be, so the field keeps its default rather
    // than wrapping around.
    template <typename TargetType, typename StoredType>
    [[nodiscard]] static std::optional<TargetType> narrowed(
        const std::optional<StoredType> storedValue) {
        if (!storedValue.has_value()) {
            return std::nullopt;
        }

        if constexpr (std::integral<TargetType>) {
            if (!std::in_range<TargetType>(storedValue.value())) {
                return std::nullopt;
            }
        }

        return static_cast<TargetType>(storedValue.value());
    }

    int m_indentation{DEFAULT_INDENTATION};

    // Settings files get read and edited by hand often enough to be worth indenting.
    static constexpr int DEFAULT_INDENTATION{4};
};

} // namespace tca::engine::settings::formatters
