// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <functional>

#include "platform/engine-type-traits.hpp"
#include "platform/win32/win32-application-window.hpp"

namespace tca::engine::rendering::win32 {

/**
 * @brief Class for windows using ImGui on Win32.
 * This class is rendering backend agnostic, it only sets Win32 API specific parameters for
 * creation.
 */
class imgui_win32_window : public platform::win32::win32_application_window {
public:
    /**
     * @brief Should return true if the window message is fully handled by the callback, false
     * otherwise.
     */
    using on_window_size = std::function<bool(const platform::win32::details::wparam_t,
                                              const platform::win32::details::lparam_t)>;

    /**
     * @brief Should be called when the DPI scale of the window changes, with the new DPI scale as a
     * parameter.
     */
    using on_dpi_scale_changed = std::function<void(float)>;

    /**
     * @brief Construct a new ImGui Win32 Window object with the given title.
     *
     * @param title The title of the window.
     * @param on_size (Optional) A callback function that will be called when the window receives a
     * WM_SIZE message, allowing the caller to handle window resizing events.
     * @param on_dpi_scale_changed (Optional) A callback function that will be called when the DPI
     * scale of the window changes, allowing the caller to handle DPI scaling events.
     * @param win32WindowClassStyle (Optional) The Win32 window class style to be used for the
     * window class. If not provided, it will default to CS_CLASSDC, which is a common style for
     * windows created with ImGui.
     * @param creationParams (Optional) The parameters for creating the window (e.g. its size and
     * WS_* style), forwarded as-is to win32_application_window. If not provided, the window is
     * created with win32_application_window's own defaults (WS_OVERLAPPEDWINDOW, OS-default
     * size/position).
     */
    explicit imgui_win32_window(const platform::platform_preferred_string_view_t title,
                                on_window_size on_size = {}, on_dpi_scale_changed on_dpi = {},
                                std::uint32_t win32WindowClassStyle = CS_CLASSDC,
                                platform::win32::partial_window_creation_params creationParams = {});

protected:
    route_result_t routed_window_procedure(
        HWND hwnd, platform::win32::windows_messages message,
        platform::win32::details::wparam_t wparam,
        platform::win32::details::lparam_t lparam) noexcept override;

private:
    on_window_size m_onSizeCallback{};
    on_dpi_scale_changed m_onDpiScaleChangedCallback{};
};

} // namespace tca::engine::rendering::win32
