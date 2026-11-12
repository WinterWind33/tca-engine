// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/theming/theme.hpp"

// C++ STL
#include <string>

// Third-party
#include <gsl/gsl>

namespace tca::engine::theming {

theme& theme::set(const color_role role, const rendering::color value) {
    Expects(role != color_role::count && "color_role::count is not a role and cannot be set.");
    m_colors[to_role_index(role)] = value;
    return *this;
}

theme& theme::set(const dimension_role role, const double value) {
    Expects(role != dimension_role::count &&
            "dimension_role::count is not a role and cannot be set.");
    m_dimensions[to_role_index(role)] = value;
    return *this;
}

theme& theme::set(const typography_role role, const typography value) {
    Expects(role != typography_role::count &&
            "typography_role::count is not a role and cannot be set.");
    m_typography[to_role_index(role)] = value;
    return *this;
}

theme& theme::set_custom_color(const std::string_view name, const rendering::color value) {
    m_customColors.insert_or_assign(std::string{name}, value);
    return *this;
}

theme& theme::set_custom_dimension(const std::string_view name, const double value) {
    m_customDimensions.insert_or_assign(std::string{name}, value);
    return *this;
}

theme& theme::set_custom_typography(const std::string_view name, const typography value) {
    m_customTypography.insert_or_assign(std::string{name}, value);
    return *this;
}

rendering::color theme::resolve(const color_role role) const noexcept {
    Expects(role != color_role::count && "color_role::count is not a role and cannot be resolved.");
    return m_colors[to_role_index(role)];
}

double theme::resolve(const dimension_role role) const noexcept {
    Expects(role != dimension_role::count &&
            "dimension_role::count is not a role and cannot be resolved.");
    return m_dimensions[to_role_index(role)];
}

typography theme::resolve(const typography_role role) const noexcept {
    Expects(role != typography_role::count &&
            "typography_role::count is not a role and cannot be resolved.");
    return m_typography[to_role_index(role)];
}

std::optional<rendering::color> theme::resolve_custom_color(const std::string_view name) const {
    const auto found{m_customColors.find(name)};
    if (found == m_customColors.cend()) {
        return std::nullopt;
    }

    return found->second;
}

std::optional<double> theme::resolve_custom_dimension(const std::string_view name) const {
    const auto found{m_customDimensions.find(name)};
    if (found == m_customDimensions.cend()) {
        return std::nullopt;
    }

    return found->second;
}

std::optional<typography> theme::resolve_custom_typography(const std::string_view name) const {
    const auto found{m_customTypography.find(name)};
    if (found == m_customTypography.cend()) {
        return std::nullopt;
    }

    return found->second;
}

rendering::color theme::resolve_custom_color_or(const std::string_view name,
                                                const color_role fallback) const {
    return resolve_custom_color(name).value_or(resolve(fallback));
}

double theme::resolve_custom_dimension_or(const std::string_view name,
                                          const dimension_role fallback) const {
    return resolve_custom_dimension(name).value_or(resolve(fallback));
}

typography theme::resolve_custom_typography_or(const std::string_view name,
                                               const typography_role fallback) const {
    return resolve_custom_typography(name).value_or(resolve(fallback));
}

} // namespace tca::engine::theming
