// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/terminal/option-description.hpp"

// C++ STL
#include <string>
#include <string_view>
#include <utility>

namespace tca::engine::platform::terminal {

option_description::option_description(std::string long_name) : m_longName{std::move(long_name)} {}

option_description option_description::short_only(std::string short_name) {
    option_description desc{};
    desc.m_shortName = std::move(short_name);
    return desc;
}

option_description& option_description::with_short_name(std::string short_name) {
    m_shortName = std::move(short_name);
    return *this;
}

option_description& option_description::with_description(std::string description) {
    m_description = std::move(description);
    return *this;
}

std::string_view option_description::long_name() const noexcept {
    return m_longName;
}

bool option_description::has_long_name() const noexcept {
    return !m_longName.empty();
}

std::string_view option_description::short_name() const noexcept {
    return m_shortName;
}

bool option_description::has_short_name() const noexcept {
    return !m_shortName.empty();
}

std::string_view option_description::description() const noexcept {
    return m_description;
}

bool option_description::takes_value() const noexcept {
    return m_value != nullptr;
}

value_arity option_description::arity() const noexcept {
    return m_value != nullptr ? m_value->arity() : value_arity::none;
}

bool option_description::is_required() const noexcept {
    return m_value != nullptr && m_value->is_required();
}

bool option_description::has_default_value() const noexcept {
    return m_value != nullptr && m_value->has_default_value();
}

std::string option_description::default_value_text() const {
    return m_value != nullptr ? m_value->default_value_text() : std::string{};
}

bool option_description::has_implicit_value() const noexcept {
    return m_value != nullptr && m_value->has_implicit_value();
}

std::string option_description::implicit_value_text() const {
    return m_value != nullptr ? m_value->implicit_value_text() : std::string{};
}

std::string_view option_description::value_name() const noexcept {
    // A flag renders as just "--verbose", and the empty view is what tells the help page it has no
    // angle brackets to print. The "arg" fallback for an unnamed value lives in the specification
    // itself, so there is nothing to invent here.
    return takes_value() ? m_value->name() : std::string_view{};
}

std::string_view option_description::key() const noexcept {
    return m_longName.empty() ? m_shortName : m_longName;
}

const value_semantics* option_description::value_spec() const noexcept {
    return m_value.get();
}

} // namespace tca::engine::platform::terminal
