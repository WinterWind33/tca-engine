// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/opengl3/opengl3-rendering-manager.hpp"

// C++ STL
#include <array>
#include <cstdint>
#include <stdexcept>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <GL/GL.h>

namespace tca::engine::rendering::win32::opengl3 {

namespace {
#ifdef NDEBUG
constexpr bool DEBUG_CONFIGURATION = false;
#else
constexpr bool DEBUG_CONFIGURATION = true;
#endif
} // namespace

opengl3_rendering_manager::opengl3_rendering_manager(const window_type& window) : m_window{window} {
    initialize(window);
    THROW_LAST_ERROR_IF_MSG(
        !::wglMakeCurrent(m_windowHdc.get(), m_openglContext.get()),
        "Failed to make the OpenGL context current for the window's device context.");
}

opengl3_rendering_manager::~opengl3_rendering_manager() noexcept {
    cleanup();
}

void opengl3_rendering_manager::on_window_resize(const std::uint32_t newWidth,
                                                 const std::uint32_t newHeight) noexcept {
    m_windowWidth = newWidth;
    m_windowHeight = newHeight;
}

void opengl3_rendering_manager::render() {
    ::glViewport(0, 0, m_windowWidth, m_windowHeight);

    const std::array<float, 4> normalizedClearColor{
        rendering::normalize_color_component(m_clearColor.r),
        rendering::normalize_color_component(m_clearColor.g),
        rendering::normalize_color_component(m_clearColor.b),
        rendering::normalize_color_component(m_clearColor.a),
    };

    ::glClearColor(normalizedClearColor[0], normalizedClearColor[1], normalizedClearColor[2],
                   normalizedClearColor[3]);
    ::glClear(GL_COLOR_BUFFER_BIT);
}

void opengl3_rendering_manager::present() {
    if constexpr (DEBUG_CONFIGURATION) {
        THROW_LAST_ERROR_IF(::SwapBuffers(m_windowHdc.get()) == FALSE);
    } else {
        ::SwapBuffers(m_windowHdc.get());
    }
}

bool opengl3_rendering_manager::is_window_occluded() const noexcept {
    return ::IsIconic(m_window.get_native_handle().get());
}

void opengl3_rendering_manager::initialize(const window_type& window) {
    const auto& windowHandle{window.get_native_handle()};

    {
        wil::unique_hdc windowHdc{::GetDC(windowHandle.get())};
        PIXELFORMATDESCRIPTOR pfd{
            .nSize = sizeof(PIXELFORMATDESCRIPTOR),
            .nVersion = 1,
            .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            .iPixelType = PFD_TYPE_RGBA,
            .cColorBits = 32,
        };

        const std::int32_t pixelFormat{::ChoosePixelFormat(windowHdc.get(), &pfd)};
        THROW_LAST_ERROR_IF_MSG(pixelFormat == 0,
                                "Failed to choose a pixel format for the window's device context.");
        THROW_LAST_ERROR_IF_MSG(::SetPixelFormat(windowHdc.get(), pixelFormat, &pfd) == FALSE,
                                "Failed to set the pixel format for the window's device context.");
    }

    m_windowHdc = wil::unique_hdc{::GetDC(windowHandle.get())};
    THROW_LAST_ERROR_IF_NULL_MSG(m_windowHdc.get(),
                                 "Failed to get the device context for the window.");

    if (!m_openglContext) {
        m_openglContext = details::unique_hglrc{::wglCreateContext(m_windowHdc.get())};
        THROW_LAST_ERROR_IF_NULL_MSG(m_openglContext.get(), "Failed to create the OpenGL context.");
    }
}

void opengl3_rendering_manager::cleanup() noexcept {
    ::wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(m_window.get_native_handle().get(), m_windowHdc.release());
}

} // namespace tca::engine::rendering::win32::opengl3
