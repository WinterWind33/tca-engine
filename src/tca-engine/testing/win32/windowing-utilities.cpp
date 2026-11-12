// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "testing/win32/windowing-utilities.hpp"

// C++ STL
#include <cstddef>

#include "platform/win32/windows.hpp"

namespace tca::engine::testing {

std::size_t remove_wm_quit_messages_from_queue() {
    std::size_t removedCount{};
    for (MSG msg{}; removedCount < WM_QUIT_DRAIN_LIMIT &&
                    ::PeekMessage(&msg, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE);
         ++removedCount) {
        // Do nothing, just remove the message from the queue.
    }

    return removedCount;
}

void simulate_user_closing_application(const wil::unique_hwnd& windowHandle) {
    // Simulate the user closing the application by sending a WM_CLOSE message to the window.
    ::PostMessage(windowHandle.get(), WM_CLOSE, 0, 0);
}

} // namespace tca::engine::testing
