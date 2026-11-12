// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Third-party
#include <imgui.h>

namespace tca::engine::testing {

/**
 * @brief RAII helper that stands up a minimal ImGui context with no rendering backend attached, so
 * that widget code exercising ImGui::Begin()/End(), ImGui::GetMainViewport() etc. can be driven
 * directly from unit tests, without a real window or GPU.
 *
 * @note Frames must be driven explicitly: call new_frame() before doing any ImGui work for that
 * frame, and end_frame() once done, mirroring how a real application drives ImGui frame by frame.
 * set_display_size() can be called in between frames to simulate the main application window
 * being resized/maximized.
 */
class headless_imgui_context final {
public:
    /**
     * @brief Creates the context with the given simulated main viewport size.
     *
     * @param displaySize The initial simulated main viewport size.
     */
    explicit headless_imgui_context(ImVec2 displaySize = ImVec2{1920.0f, 1080.0f});
    ~headless_imgui_context();

    headless_imgui_context(const headless_imgui_context&) = delete;
    headless_imgui_context& operator=(const headless_imgui_context&) = delete;

    /**
     * @brief Changes the simulated main viewport size, taking effect from the next new_frame()
     * call.
     *
     * @param displaySize The new simulated main viewport size.
     */
    void set_display_size(ImVec2 displaySize) noexcept;

    /**
     * @brief Starts a new simulated ImGui frame.
     */
    void new_frame();

    /**
     * @brief Ends the current simulated ImGui frame.
     */
    void end_frame();

private:
    ImGuiContext* m_context;
    ImVec2 m_displaySize;
    bool m_frameOpen{false};
};

} // namespace tca::engine::testing
