// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "testing/linux/windowing-utilities.hpp"

// C++ STL
#include <stdexcept>

namespace tca::engine::testing {

scoped_null_glfw_context::scoped_null_glfw_context() {
    ::glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
    if (::glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to initialize GLFW with the null platform.");
    }
}

scoped_null_glfw_context::~scoped_null_glfw_context() {
    ::glfwTerminate();
}

void apply_headless_glfw_window_hints() {
    ::glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    ::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

GLFWwindow* create_headless_glfw_window(const char* const title) {
    apply_headless_glfw_window_hints();
    GLFWwindow* const window{::glfwCreateWindow(1, 1, title, nullptr, nullptr)};
    if (window == nullptr) {
        throw std::runtime_error("Failed to create a headless GLFW window.");
    }

    return window;
}

} // namespace tca::engine::testing
