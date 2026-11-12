// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/theming/active-theme.hpp"

// C++ STL
#include <utility>

namespace tca::engine::theming {

namespace {

// The one theme the application wears. A function-local static rather than a namespace-scope one
// so it is initialized the first time anything asks for it, which keeps it usable from code
// running before main() and removes any ordering question between translation units.
//
// Deliberately not synchronized: it mirrors how the rest of the rendering path works, where widget
// rendering and the ImGui context it drives are single-threaded anyway. Changing the theme from a
// thread other than the one rendering is not supported.
theme& active_theme_storage() {
    static theme activeTheme{};
    return activeTheme;
}

} // namespace

const theme& get_active_theme() noexcept {
    return active_theme_storage();
}

void set_active_theme(theme newTheme) {
    active_theme_storage() = std::move(newTheme);
}

rendering::color color_for(const color_role role) noexcept {
    return get_active_theme().resolve(role);
}

double dimension_for(const dimension_role role) noexcept {
    return get_active_theme().resolve(role);
}

typography typography_for(const typography_role role) noexcept {
    return get_active_theme().resolve(role);
}

scoped_active_theme::scoped_active_theme(theme newTheme) : m_previousTheme{get_active_theme()} {
    set_active_theme(std::move(newTheme));
}

scoped_active_theme::~scoped_active_theme() {
    set_active_theme(std::move(m_previousTheme));
}

} // namespace tca::engine::theming
