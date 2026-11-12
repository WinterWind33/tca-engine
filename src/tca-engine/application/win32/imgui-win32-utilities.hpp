// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>

// Engine
#include "input/input-frame.hpp"
#include "platform/engine-type-traits.hpp"

namespace tca::engine::application::win32 {

/**
 * @brief Initializes the ImGui context for the application, setting up the necessary configuration
 * for proper scaling and rendering on high-DPI displays. Also, sets up the ImGui style and fonts.
 *
 * @param appName The name of the application, used for logging purposes.
 * @param mainScale The main scale factor for the application, used to scale ImGui styles and fonts
 * accordingly.
 */
void initialize_imgui_context(platform::win32::type_traits::win32_string_view_t appName,
                              float mainScale);

/**
 * @brief Handles changes in DPI scale, updating the ImGui style and fonts accordingly to ensure
 * proper rendering on high-DPI displays. This function should be called whenever the DPI scale
 * changes, such as when the application is moved to a different monitor with a different DPI
 * setting or when the system DPI settings are changed.
 *
 * @param appName The name of the application, used for logging purposes.
 * @param newDpiScale The new DPI scale factor for the application, used to scale ImGui styles and
 * fonts accordingly.
 * @return float The updated DPI scale factor.
 */
float on_dpi_scale_changed(platform::win32::type_traits::win32_string_view_t appName,
                           float newDpiScale);

/**
 * @brief Hides the whole keyboard and the typed text from the logic when ImGui wants the keyboard
 * (`io.WantCaptureKeyboard`), so typing in a widget doesn't also drive the game. ImGui
 * still gets every message. Call it after ImGui::NewFrame(), which updates the flag, and before
 * the logic's tick(). A key held while ImGui had the keyboard stays hidden until pressed again.
 *
 * @param input The application's input frame.
 */
void consume_input_captured_by_imgui(input::input_view input) noexcept;

} // namespace tca::engine::application::win32
