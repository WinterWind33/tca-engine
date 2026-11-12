// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "settings/settings-json-formatter.hpp"

// Third-party
#include <nlohmann/json.hpp>

// C++ STL
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace tca::engine::settings::formatters {

// The JSON library reaches no further than this file: everything the rest of the engine sees of
// a document is primitives and std::optional.
struct settings_json_document::json_tree {
    // Copy-initialised rather than brace-initialised: nlohmann::json has an initializer-list
    // constructor, so `nlohmann::json root{nlohmann::json::object()}` builds an *array* holding
    // one empty object instead of the empty object itself.
    nlohmann::json root = nlohmann::json::object();
};

settings_json_document::settings_json_document() : m_tree{std::make_unique<json_tree>()} {}

settings_json_document::~settings_json_document() = default;

settings_json_document::settings_json_document(settings_json_document&& other) noexcept = default;

settings_json_document& settings_json_document::operator=(settings_json_document&& other) noexcept =
    default;

namespace {

// A scope names a node of a document that outlives it, so the pointer only ever makes the round
// trip back to the type it was made from. Only an object can hold key-value pairs: a scope
// naming anything else, or naming nothing at all, is one every read comes back empty from and
// every write refuses.
[[nodiscard]] nlohmann::json* object_at(void* const node) noexcept {
    auto* const jsonNode{static_cast<nlohmann::json*>(node)};

    return (jsonNode != nullptr && jsonNode->is_object()) ? jsonNode : nullptr;
}

[[nodiscard]] const nlohmann::json* find_in(void* const node, const fixed_string key) {
    auto* const object{object_at(node)};
    if (object == nullptr) {
        return nullptr;
    }

    const auto storedValue{object->find(std::string{key})};

    return storedValue == object->end() ? nullptr : &storedValue.value();
}

template <typename StorageType>
bool put_in(void* const node, const fixed_string key, StorageType&& value) {
    auto* const object{object_at(node)};
    if (object == nullptr) {
        return false;
    }

    try {
        (*object)[std::string{key}] = std::forward<StorageType>(value);
    } catch (...) {
        return false;
    }

    return true;
}

} // namespace

settings_json_document settings_json_formatter::make_document() const {
    return {};
}

std::optional<settings_json_document> settings_json_formatter::parse(
    std::istream& inputStream) const {
    // Parsing is asked not to throw: a settings file someone has hand-edited into nonsense is an
    // ordinary outcome here, not an error worth an exception. Comments are allowed through, so a
    // file a player has annotated still loads.
    // Assigned rather than brace-initialised, for the same reason json_tree::root is: braces
    // around a nlohmann::json pick the initializer-list constructor and wrap it in an array.
    auto parsedRoot = nlohmann::json::parse(inputStream, nullptr, false, true);
    if (parsedRoot.is_discarded() || !parsedRoot.is_object()) {
        return std::nullopt;
    }

    settings_json_document document{};
    document.m_tree->root = std::move(parsedRoot);

    return document;
}

bool settings_json_formatter::serialize(std::ostream& outputStream,
                                        const settings_json_document& document) const {
    outputStream << document.m_tree->root.dump(m_indentation) << '\n';

    return static_cast<bool>(outputStream);
}

settings_json_scope settings_json_formatter::root(settings_json_document& document) const noexcept {
    return settings_json_scope{&document.m_tree->root};
}

settings_json_scope settings_json_formatter::subdocument(const settings_json_scope scope,
                                                         const fixed_string key) const {
    auto* const object{object_at(scope.m_node)};
    if (object == nullptr) {
        return {};
    }

    auto& child{(*object)[std::string{key}]};
    if (child.is_null()) {
        // Either the field is being written for the first time, or the file holds no scope of
        // this name and the reads about to come are meant to find nothing. An empty object
        // covers both.
        child = nlohmann::json::object();
    }

    if (!child.is_object()) {
        return {};
    }

    return settings_json_scope{&child};
}

bool settings_json_formatter::write_boolean(const settings_json_scope scope, const fixed_string key,
                                            const bool value) const {
    return put_in(scope.m_node, key, value);
}

bool settings_json_formatter::write_signed(const settings_json_scope scope, const fixed_string key,
                                           const std::int64_t value) const {
    return put_in(scope.m_node, key, value);
}

bool settings_json_formatter::write_unsigned(const settings_json_scope scope,
                                             const fixed_string key,
                                             const std::uint64_t value) const {
    return put_in(scope.m_node, key, value);
}

bool settings_json_formatter::write_floating(const settings_json_scope scope,
                                             const fixed_string key, const double value) const {
    return put_in(scope.m_node, key, value);
}

bool settings_json_formatter::write_string(const settings_json_scope scope, const fixed_string key,
                                           const std::string_view value) const {
    return put_in(scope.m_node, key, std::string{value});
}

bool settings_json_formatter::write_string_array(const settings_json_scope scope,
                                                 const fixed_string key,
                                                 const std::vector<std::string>& value) const {
    // Built element by element: an empty vector assigned straight to a nlohmann::json would be
    // stored as null rather than as an empty array.
    nlohmann::json array = nlohmann::json::array();
    for (const auto& element : value) {
        array.push_back(element);
    }

    return put_in(scope.m_node, key, std::move(array));
}

std::optional<bool> settings_json_formatter::read_boolean(const settings_json_scope scope,
                                                          const fixed_string key) const {
    const auto* const storedValue{find_in(scope.m_node, key)};

    return (storedValue != nullptr && storedValue->is_boolean())
               ? std::optional<bool>{storedValue->get<bool>()}
               : std::nullopt;
}

std::optional<std::int64_t> settings_json_formatter::read_signed(const settings_json_scope scope,
                                                                 const fixed_string key) const {
    const auto* const storedValue{find_in(scope.m_node, key)};
    if (storedValue == nullptr || !storedValue->is_number_integer()) {
        return std::nullopt;
    }

    // A whole number the file holds may still be one no signed type can carry.
    if (storedValue->is_number_unsigned() &&
        storedValue->get<std::uint64_t>() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }

    return storedValue->get<std::int64_t>();
}

std::optional<std::uint64_t> settings_json_formatter::read_unsigned(const settings_json_scope scope,
                                                                    const fixed_string key) const {
    const auto* const storedValue{find_in(scope.m_node, key)};
    if (storedValue == nullptr || !storedValue->is_number_integer()) {
        return std::nullopt;
    }

    if (!storedValue->is_number_unsigned() && storedValue->get<std::int64_t>() < 0) {
        return std::nullopt;
    }

    return storedValue->get<std::uint64_t>();
}

std::optional<double> settings_json_formatter::read_floating(const settings_json_scope scope,
                                                             const fixed_string key) const {
    const auto* const storedValue{find_in(scope.m_node, key)};

    // A whole number in the file is a perfectly good value for a floating-point field: writing 1
    // rather than 1.0 by hand should not cost the setting.
    return (storedValue != nullptr && storedValue->is_number())
               ? std::optional<double>{storedValue->get<double>()}
               : std::nullopt;
}

std::optional<std::string> settings_json_formatter::read_string(const settings_json_scope scope,
                                                                const fixed_string key) const {
    const auto* const storedValue{find_in(scope.m_node, key)};

    return (storedValue != nullptr && storedValue->is_string())
               ? std::optional<std::string>{storedValue->get<std::string>()}
               : std::nullopt;
}

std::optional<std::vector<std::string>> settings_json_formatter::read_string_array(
    const settings_json_scope scope, const fixed_string key) const {
    const auto* const storedValue{find_in(scope.m_node, key)};
    if (storedValue == nullptr || !storedValue->is_array()) {
        return std::nullopt;
    }

    std::vector<std::string> elements{};
    elements.reserve(storedValue->size());
    for (const auto& element : *storedValue) {
        if (!element.is_string()) {
            return std::nullopt;
        }

        elements.push_back(element.get<std::string>());
    }

    return elements;
}

} // namespace tca::engine::settings::formatters
