// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/opengl3/opengl3-additive-blending.hpp"

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <GL/GL.h>
#include <imgui.h>

// Engine
#include "rendering/gui/imgui/imgui-blend-callbacks.hpp"

namespace tca::engine::rendering::win32::opengl3 {

namespace {

// OpenGL keeps the sampler and the blending apart, so each callback is ImGui's sampler switch
// followed by the additive blend function. glBlendFunc() is OpenGL 1.1, so opengl32 has it with no
// loader.
void blend_additively(const ImDrawCallback samplerCallback, const ImDrawList* const drawList,
                      const ImDrawCmd* const command) {
    if (samplerCallback != nullptr) {
        samplerCallback(drawList, command);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

void set_additive_linear(const ImDrawList* const drawList, const ImDrawCmd* const command) {
    blend_additively(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, drawList, command);
}

void set_additive_nearest(const ImDrawList* const drawList, const ImDrawCmd* const command) {
    blend_additively(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, drawList, command);
}

} // namespace

void install_additive_blending() noexcept {
    imgui::set_additive_blend_callbacks(imgui::additive_blend_callbacks{
        .set_additive_linear = set_additive_linear,
        .set_additive_nearest = set_additive_nearest,
    });
}

void uninstall_additive_blending() noexcept {
    imgui::set_additive_blend_callbacks({});
}

} // namespace tca::engine::rendering::win32::opengl3
