// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <optional>

// Input
#include "input/key.hpp"

namespace tca::engine::platform::posix_linux {

/**
 * @brief A Linux kernel input event code (KEY_* in <linux/input-event-codes.h>): the physical key
 * as evdev reports it, whatever the display server or the active layout.
 */
using evdev_code = std::uint16_t;

/**
 * @brief The key at the position of the given GLFW key token (GLFW_KEY_*). GLFW names its tokens
 * after the US layout but derives them from the physical key, so they are layout independent.
 * GLFW_KEY_WORLD_1 and GLFW_KEY_WORLD_2 are both the ISO key next to left Shift: X11 reports it as
 * the first, Wayland as the second.
 *
 * @return The key, or nothing for GLFW_KEY_UNKNOWN and tokens with no position in input::key.
 */
[[nodiscard]] std::optional<input::key> key_from_glfw_key(int glfwKey) noexcept;

/**
 * @brief The key at the position of the given evdev code. It covers the keys GLFW has no token
 * for (the JIS, ABNT and Korean extras), which GLFW reports as GLFW_KEY_UNKNOWN.
 *
 * @return The key, or nothing for a code with no position in input::key.
 */
[[nodiscard]] std::optional<input::key> key_from_evdev_code(evdev_code code) noexcept;

/**
 * @brief The evdev code behind a GLFW scancode, which is platform specific: X11 keycodes are evdev
 * codes plus 8, Wayland scancodes are evdev codes, and any other platform (the null one included)
 * has scancodes of its own.
 *
 * @param scancode The scancode GLFW passed to the key callback.
 * @param glfwPlatform The platform GLFW runs on, as glfwGetPlatform() reports it.
 * @return The evdev code, or nothing if the platform's scancodes are not evdev codes or the
 * scancode is out of range.
 */
[[nodiscard]] std::optional<evdev_code> evdev_code_from_glfw_scancode(int scancode,
                                                                      int glfwPlatform) noexcept;

/**
 * @brief The key of a GLFW key event: from the token when it maps to a key, otherwise from the
 * scancode through its evdev code.
 *
 * @param glfwKey The key token GLFW passed to the key callback.
 * @param scancode The scancode GLFW passed to the key callback.
 * @param glfwPlatform The platform GLFW runs on, as glfwGetPlatform() reports it.
 * @return The key, or nothing if neither the token nor the scancode maps to one.
 */
[[nodiscard]] std::optional<input::key> key_from_glfw_key_event(int glfwKey, int scancode,
                                                                int glfwPlatform) noexcept;

} // namespace tca::engine::platform::posix_linux
