// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>

// Windows
#include "platform/win32/windows.hpp"

namespace tests::utilities {

/**
 * @brief Builds the LPARAM of a client-area mouse message (WM_MOUSEMOVE, WM_*BUTTON*) the way
 * Windows lays it out: x in the low word, y in the high word, each a signed 16-bit value, so
 * positions left of or above the client area (a captured drag) come out negative.
 *
 * Built as a 32-bit value and zero-extended, which is how Windows hands it over on 64-bit.
 */
constexpr LPARAM mouse_lparam(const std::int16_t x, const std::int16_t y) noexcept {
    const auto low{static_cast<std::uint32_t>(static_cast<std::uint16_t>(x))};
    const auto high{static_cast<std::uint32_t>(static_cast<std::uint16_t>(y))};
    return static_cast<LPARAM>(low | (high << 16U));
}

/**
 * @brief Builds the WPARAM of a WM_MOUSEWHEEL / WM_MOUSEHWHEEL: the held-buttons flags (MK_*) in
 * the low word and the signed wheel delta (a multiple or a fraction of WHEEL_DELTA) in the high
 * word.
 */
constexpr WPARAM wheel_wparam(const std::int16_t delta, const std::uint16_t keyFlags = 0) noexcept {
    const auto high{static_cast<std::uint32_t>(static_cast<std::uint16_t>(delta))};
    return static_cast<WPARAM>(static_cast<std::uint32_t>(keyFlags) | (high << 16U));
}

/**
 * @brief Builds the WPARAM of a WM_XBUTTONDOWN / WM_XBUTTONUP / WM_XBUTTONDBLCLK: the held-buttons
 * flags (MK_*) in the low word and which X button changed (XBUTTON1 or XBUTTON2) in the high word.
 */
constexpr WPARAM x_button_wparam(const std::uint16_t xButton,
                                 const std::uint16_t keyFlags = 0) noexcept {
    return static_cast<WPARAM>(static_cast<std::uint32_t>(keyFlags) |
                               (static_cast<std::uint32_t>(xButton) << 16U));
}

} // namespace tests::utilities
