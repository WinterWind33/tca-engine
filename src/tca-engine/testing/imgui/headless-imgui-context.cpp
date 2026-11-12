// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "testing/imgui/headless-imgui-context.hpp"

namespace tca::engine::testing {

headless_imgui_context::headless_imgui_context(const ImVec2 displaySize)
    : m_context{ImGui::CreateContext()},
      m_displaySize{displaySize} {
    ImGui::SetCurrentContext(m_context);

    ImGuiIO& io{ImGui::GetIO()};
    io.DisplaySize = m_displaySize;
    io.DeltaTime = 1.0f / 60.0f;

    // ImGui otherwise writes an imgui.ini into the working directory when the context is
    // destroyed, and reads it back on the next run. A test helper has no business leaving files
    // behind, and window positions restored from a previous run would make test cases depend on
    // the order they last ran in.
    io.IniFilename = nullptr;

    // No rendering backend is attached in headless tests, so the font atlas must be built
    // explicitly here, otherwise ImGui::NewFrame() asserts that it isn't built yet.
    io.Fonts->Build();
}

headless_imgui_context::~headless_imgui_context() {
    ImGui::SetCurrentContext(m_context);
    // ImGui::EndFrame() asserts that a frame is currently open (i.e. new_frame() was called and
    // end_frame() wasn't yet), so it must only be called here if a test left a frame open when it
    // finished, rather than unconditionally.
    if (m_frameOpen) {
        ImGui::EndFrame();
    }
    ImGui::DestroyContext(m_context);
}

void headless_imgui_context::set_display_size(const ImVec2 displaySize) noexcept {
    m_displaySize = displaySize;
    ImGui::SetCurrentContext(m_context);
    ImGui::GetIO().DisplaySize = m_displaySize;
}

void headless_imgui_context::new_frame() {
    ImGui::SetCurrentContext(m_context);
    ImGui::NewFrame();
    m_frameOpen = true;
}

void headless_imgui_context::end_frame() {
    ImGui::SetCurrentContext(m_context);
    ImGui::EndFrame();
    m_frameOpen = false;
}

} // namespace tca::engine::testing
