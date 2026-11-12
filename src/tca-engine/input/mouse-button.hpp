// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace tca::engine::input {

/**
 * @brief A physical mouse button. x1 and x2 are the two side buttons ("back" and "forward" on most
 *  mice).
 *
 * @note The enumerators are contiguous from zero, so a button can index an array of
 *  MOUSE_BUTTONS_COUNT elements (see mouse_button_index()). Their numeric values are not stable
 *  across engine versions: persist a button by mouse_button_name(), never by its value.
 */
enum class mouse_button : std::uint8_t {
    left,
    right,
    middle,
    x1,
    x2,
};

/**
 * @brief Number of buttons in the mouse_button enum, for sizing per-button arrays and bitsets.
 */
inline constexpr std::size_t MOUSE_BUTTONS_COUNT{static_cast<std::size_t>(mouse_button::x2) + 1};

/**
 * @brief Position of the given mouse button in an array of MOUSE_BUTTONS_COUNT elements.
 */
[[nodiscard]] constexpr std::size_t mouse_button_index(const mouse_button b) noexcept {
    return static_cast<std::size_t>(std::to_underlying(b));
}

/**
 * @brief Stable, lowercase name of the given mouse button, spelled like its enumerator (e.g.
 *  "left", "x1"). Meant for logs and for persisting bindings, so a name never changes once
 *  released.
 */
[[nodiscard]] std::string_view mouse_button_name(mouse_button b) noexcept;

/**
 * @brief The mouse button whose mouse_button_name() is the given name, so that saved bindings can
 *  be read back. Matched exactly: a name in another case, with surrounding spaces or of no button
 *  is not one.
 *
 * @return The mouse button, or nothing if no button has that name.
 */
[[nodiscard]] std::optional<mouse_button> mouse_button_from_name(std::string_view name) noexcept;

} // namespace tca::engine::input
