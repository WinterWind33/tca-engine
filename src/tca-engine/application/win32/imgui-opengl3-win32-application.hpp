// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

#include "application/win32/opengl3-win32-application.hpp"

namespace tca::engine::application::win32 {

/**
 * @brief An application class that integrates ImGui with an OpenGL 3 rendering backend on the Win32
 * platform. This class extends the basic OpenGL 3 Win32 application to include ImGui context
 * initialization, rendering, and cleanup. It also handles DPI scaling changes to ensure that the
 * ImGui interface remains properly scaled on high-DPI displays.
 */
class imgui_opengl3_win32_application : public opengl3_win32_application {
private:
    float m_mainScale{};
    // Tracks whether initialize_rendering_backends() completed successfully, so close() knows
    // whether the ImGui Win32/OpenGL3 backends actually need shutting down. This matters because
    // close() can also run after a partial initialize() failure, in which case calling their
    // Shutdown() functions would be invalid.
    bool m_imguiBackendInitialized{};

    void initialize() override;
    void initialize_rendering_backends();
    bool pre_logic_tick() override;
    void post_logic_tick() override;
    void close() override;
};

} // namespace tca::engine::application::win32
