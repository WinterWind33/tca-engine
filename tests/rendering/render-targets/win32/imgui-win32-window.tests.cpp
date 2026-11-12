// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/render-targets/win32/imgui-win32-window.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("imgui_win32_window tests",
          "[tca][engine][rendering][render-targets][win32][imgui-win32-window]") {
    using namespace tca::engine::rendering::win32;
    using namespace tca::engine::platform::win32;

    SECTION("imgui_win32_window creation should not throw any exceptions") {
        REQUIRE_NOTHROW(imgui_win32_window{TEXT("Test ImGui Win32 Window")});
    }

    SECTION("An explicit partial_window_creation_params is forwarded to the created window") {
        const partial_window_creation_params creationParams{
            .style = compute_window_style(false, false),
            .size = window_creation_size{640, 480},
            .use_pure_raii = true,
        };

        imgui_win32_window window{TEXT("Test ImGui Win32 Window"), {}, {}, CS_CLASSDC,
                                  creationParams};
        const HWND hwnd{window.get_native_handle().get()};
        REQUIRE(hwnd != nullptr);

        THEN("The resulting window style matches the requested one") {
            const auto actualStyle{static_cast<std::uint32_t>(::GetWindowLongPtr(hwnd, GWL_STYLE))};
            REQUIRE((actualStyle & WS_THICKFRAME) == 0U);
            REQUIRE((actualStyle & WS_MAXIMIZEBOX) == 0U);
        }

        THEN("The resulting window's outer size matches the requested one") {
            RECT windowRect{};
            REQUIRE(::GetWindowRect(hwnd, &windowRect) != 0);
            REQUIRE(windowRect.right - windowRect.left == 640);
            REQUIRE(windowRect.bottom - windowRect.top == 480);
        }
    }
}
