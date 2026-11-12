// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <array>
#include <cstdint>

// Windows
#include "platform/win32/windows.hpp"

// Engine
#include "platform/win32/win32-keyboard.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace tests::utilities {

using tca::engine::platform::win32::scan_code;

/**
 * @brief Builds the LPARAM of a WM_(SYS)KEYDOWN / WM_(SYS)KEYUP the way Windows lays it out:
 * repeat count 1 in bits 0-15, scan code in 16-23, extended flag in 24, context (Alt held) in 29,
 * previous state in 30 and transition in 31. A 0xE0 prefix in the scan code's high byte becomes
 * the extended flag.
 *
 * Built as a 32-bit value and zero-extended, which is how Windows hands it over on 64-bit.
 */
constexpr LPARAM make_key_lparam(const scan_code scanCode, const bool previouslyDown = false,
                                 const bool goingUp = false, const bool altHeld = false) noexcept {
    std::uint32_t bits{1U};
    bits |= static_cast<std::uint32_t>(scanCode & 0xFFU) << 16U;
    if ((scanCode & 0xFF00U) == 0xE000U) {
        bits |= 1U << 24U;
    }
    if (altHeld) {
        bits |= 1U << 29U;
    }
    if (previouslyDown) {
        bits |= 1U << 30U;
    }
    if (goingUp) {
        bits |= 1U << 31U;
    }

    return static_cast<LPARAM>(bits);
}

/**
 * @brief LPARAM of a first key-down (not a repeat).
 */
constexpr LPARAM key_down_lparam(const scan_code scanCode, const bool altHeld = false) noexcept {
    return make_key_lparam(scanCode, false, false, altHeld);
}

/**
 * @brief LPARAM of an auto-repeat key-down (previous state bit set).
 */
constexpr LPARAM key_repeat_lparam(const scan_code scanCode, const bool altHeld = false) noexcept {
    return make_key_lparam(scanCode, true, false, altHeld);
}

/**
 * @brief LPARAM of a key-up (previous state and transition bits set).
 */
constexpr LPARAM key_up_lparam(const scan_code scanCode, const bool altHeld = false) noexcept {
    return make_key_lparam(scanCode, true, true, altHeld);
}

/**
 * @brief Overrides this thread's keyboard state table (what GetKeyState() reads) for the lifetime
 * of the object, restoring the original one on destruction. Messages sent with SendMessage() never
 * touch that table, so tests use this to say which keys Windows believes are physically down.
 */
class scoped_keyboard_state final {
public:
    scoped_keyboard_state() {
        REQUIRE(::GetKeyboardState(m_saved.data()) != 0);
        m_current = m_saved;
    }

    ~scoped_keyboard_state() {
        ::SetKeyboardState(m_saved.data());
    }

    scoped_keyboard_state(const scoped_keyboard_state&) = delete;
    scoped_keyboard_state& operator=(const scoped_keyboard_state&) = delete;

    /**
     * @brief Marks the given virtual key down or up in the thread's keyboard state.
     */
    void set_down(const std::uint8_t virtualKey, const bool down) {
        m_current[virtualKey] = down ? BYTE{0x80} : BYTE{0x00};
        REQUIRE(::SetKeyboardState(m_current.data()) != 0);
    }

private:
    std::array<BYTE, 256> m_saved{};
    std::array<BYTE, 256> m_current{};
};

/**
 * @brief Dispatches every message queued for the given window, translating them like the
 * application's message loop does.
 */
inline void pump_window_messages(const HWND hwnd) {
    for (MSG msg{}; ::PeekMessage(&msg, hwnd, 0U, 0U, PM_REMOVE);) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
}

} // namespace tests::utilities
