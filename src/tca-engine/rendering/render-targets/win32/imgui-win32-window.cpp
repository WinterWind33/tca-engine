// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/render-targets/win32/imgui-win32-window.hpp"

// C++ STL
#include <cstdint>
#include <format>

// Third-party
#include <backends/imgui_impl_win32.h>

#include "platform/platform-logger.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

namespace tca::engine::rendering::win32 {

namespace {
constexpr auto IMGUI_WNDCLASS_NAME_BASE{TEXT("ImGui Win32 Window Class")};

/**
 * @brief Simple utility function to generate a unique window class name for the ImGui Win32 Window,
 * based on the Win32 window style. This is necessary to avoid conflicts in the system when multiple
 * ImGui Win32 Windows are created with different styles, since the window class is registered at
 * the system level, and not at the window level.
 *
 * @param win32WindowStyle The Win32 window style.
 * @return platform::platform_preferred_string_t The generated window class name.
 */
platform::platform_preferred_string_t get_imgui_window_class_name(
    const std::uint32_t win32WindowStyle) {
    return std::format(TEXT("{} - Style {}"), IMGUI_WNDCLASS_NAME_BASE, win32WindowStyle);
}

/**
 * @brief Get the wndclass object based on the given Win32 window style.
 *
 * @param win32WindowStyle The Win32 window style.
 * @return platform::win32::partial_wndclassex The wndclass object.
 */
platform::win32::partial_wndclassex get_wndclass(const std::uint32_t win32WindowStyle) {
    return platform::win32::partial_wndclassex{
        .style = win32WindowStyle,
        .class_name = get_imgui_window_class_name(win32WindowStyle),
    };
}

} // namespace

imgui_win32_window::imgui_win32_window(const platform::platform_preferred_string_view_t title,
                                       on_window_size on_size, on_dpi_scale_changed on_dpi,
                                       const std::uint32_t win32WindowStyle,
                                       platform::win32::partial_window_creation_params creationParams)
    : platform::win32::win32_application_window{title, get_wndclass(win32WindowStyle),
                                                std::move(creationParams)},
      m_onSizeCallback{std::move(on_size)},
      m_onDpiScaleChangedCallback{std::move(on_dpi)} {}

auto imgui_win32_window::routed_window_procedure(
    const HWND hwnd, const platform::win32::windows_messages message,
    const platform::win32::details::wparam_t wparam,
    const platform::win32::details::lparam_t lparam) noexcept -> route_result_t {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, platform::win32::to_underlying(message), wparam,
                                       lparam) > 0) {
        return route_result_t{1};
    }

    switch (message) {
        case platform::win32::windows_messages::syscommand:
            // Disable ALT application menu
            if ((wparam & 0xfff0) == SC_KEYMENU)
                return route_result_t{0};
            break;
        case platform::win32::windows_messages::size:
            if (m_onSizeCallback && m_onSizeCallback(wparam, lparam)) {
                return route_result_t{0};
            }
            break;
        case platform::win32::windows_messages::dpi_scale_changed: {
            const float newScale{ImGui_ImplWin32_GetDpiScaleForMonitor(
                ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST))};
            platform::get_platform_logger()->info("DPI scale changed, new scale: {}", newScale);
            if (m_onDpiScaleChangedCallback) {
                m_onDpiScaleChangedCallback(newScale);
            }
        } break;
        default:
            break;
    }

    return win32_application_window::routed_window_procedure(hwnd, message, wparam, lparam);
}

} // namespace tca::engine::rendering::win32
