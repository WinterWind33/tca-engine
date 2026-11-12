// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "input/mouse-button.hpp"

// C++ STL
#include <cstddef>
#include <optional>
#include <string_view>

namespace tca::engine::input {

std::string_view mouse_button_name(const mouse_button b) noexcept {
    // No default on purpose: -Wswitch turns a button added without a name into a build error.
    switch (b) {
        case mouse_button::left:
            return "left";
        case mouse_button::right:
            return "right";
        case mouse_button::middle:
            return "middle";
        case mouse_button::x1:
            return "x1";
        case mouse_button::x2:
            return "x2";
    }

    return {};
}

std::optional<mouse_button> mouse_button_from_name(const std::string_view name) noexcept {
    // A linear scan over mouse_button_name() keeps the two spellings from ever drifting apart; it
    // runs only when bindings are loaded, so the cost doesn't matter.
    for (std::size_t index{0}; index < MOUSE_BUTTONS_COUNT; ++index) {
        const auto candidate{static_cast<mouse_button>(index)};
        if (mouse_button_name(candidate) == name) {
            return candidate;
        }
    }

    return std::nullopt;
}

} // namespace tca::engine::input
