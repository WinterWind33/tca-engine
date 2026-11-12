// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <concepts>
#include <cstddef>
#include <utility>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <wil/resource.h>

// Engine
#include "application/win32/win32-application.hpp"

namespace tca::engine::testing {

/**
 * @brief Upper bound on how many WM_QUIT messages remove_wm_quit_messages_from_queue() will drain
 * before giving up, so that a queue that somehow never empties cannot hang a test run.
 */
inline constexpr std::size_t WM_QUIT_DRAIN_LIMIT{10'000};

/**
 * @brief Since these tests create and destroy windows, we need to be sure that, in-between test
 * runs, any lingering WM_QUIT messages are removed from the message queue to prevent interference
 * with subsequent tests, otherwise message loops in subsequent tests might receive a WM_QUIT
 * message that was posted when a window was destroyed in a previous test, which would cause the
 * message loop to exit immediately, which would cause the tests to fail.
 *
 * @return std::size_t How many WM_QUIT messages were drained. A value equal to
 * WM_QUIT_DRAIN_LIMIT means the safety limit was hit rather than the queue running dry, which
 * should never happen in practice; callers are expected to assert against it with whatever test
 * framework they use, since this module deliberately depends on none.
 */
[[nodiscard]] std::size_t remove_wm_quit_messages_from_queue();

/**
 * @brief Constrains application_tss to types that actually are Win32 engine applications, so that
 * the protected member it reaches for is guaranteed to exist.
 */
template <typename ApplicationType>
concept windows_application =
    std::derived_from<ApplicationType, tca::engine::application::win32::win32_application>;

/**
 * @brief Test-specific subclass exposing the window a Win32 application created, which the
 * application itself keeps protected. Tests need the handle to drive the application the way a
 * user would (posting WM_CLOSE, checking window styles) without the application having to widen
 * its production API for their benefit.
 *
 * @tparam ApplicationType The Win32 engine application under test.
 */
template <windows_application ApplicationType>
class application_tss : public ApplicationType {
public:
    template <typename... Args>
    explicit application_tss(Args&&... args) : ApplicationType(std::forward<Args>(args)...) {}

    const wil::unique_hwnd& get_window_handle() const noexcept {
        return ApplicationType::m_window->get_native_handle();
    }
};

/**
 * @brief Simulates the user closing the application by sending a WM_CLOSE message.
 */
void simulate_user_closing_application(const wil::unique_hwnd& windowHandle);

} // namespace tca::engine::testing
