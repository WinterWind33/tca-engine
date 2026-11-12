// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Third-party
#include <GLFW/glfw3.h>

namespace tca::engine::testing {

/**
 * @brief RAII helper that initializes GLFW using its "null" platform (GLFW_PLATFORM_NULL,
 * available since GLFW 3.4), which creates real GLFWwindow handles without talking to an actual
 * windowing system. This lets GLFW-backed engine code be exercised against real handles in
 * headless CI, the same way it would run on a developer machine with a display.
 *
 * @note Throws std::runtime_error if glfwInit() fails, since this module deliberately depends on
 * no test framework to report the failure itself.
 */
class scoped_null_glfw_context final {
public:
    scoped_null_glfw_context();

    scoped_null_glfw_context(const scoped_null_glfw_context&) = delete;
    scoped_null_glfw_context& operator=(const scoped_null_glfw_context&) = delete;
    scoped_null_glfw_context(scoped_null_glfw_context&&) = delete;
    scoped_null_glfw_context& operator=(scoped_null_glfw_context&&) = delete;

    ~scoped_null_glfw_context();
};

/**
 * @brief Applies GLFW window hints (GLFW_VISIBLE = false, GLFW_CLIENT_API = GLFW_NO_API) that make
 * a subsequently created window work headlessly in CI: hidden, and with no OpenGL implementation
 * to fail on, since the null platform has none of its own (it would otherwise try to load OSMesa,
 * which CI does not have installed).
 *
 * @note Window hints are global GLFW state consumed by the *next* glfwCreateWindow() call, so call
 * this right before creating a window - whether directly with glfwCreateWindow(), or indirectly by
 * constructing an engine window type (e.g. linux_application_window) that creates its own.
 */
void apply_headless_glfw_window_hints();

/**
 * @brief Creates a real, hidden GLFWwindow with no client API attached, suitable for exercising
 * GLFW-backed ownership code in headless CI. Applies apply_headless_glfw_window_hints() itself.
 * Requires an active scoped_null_glfw_context.
 *
 * @note Throws std::runtime_error if glfwCreateWindow() fails.
 *
 * @param title The window title.
 * @return GLFWwindow* The created window, never nullptr.
 */
[[nodiscard]] GLFWwindow* create_headless_glfw_window(const char* title = "test window");

} // namespace tca::engine::testing
