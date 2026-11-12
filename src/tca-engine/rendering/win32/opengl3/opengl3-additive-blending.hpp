// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

namespace tca::engine::rendering::win32::opengl3 {

/**
 * @brief Installs the ImGui OpenGL 3 backend's additive blend callbacks (see
 *  imgui::additive_blend_callbacks): ImGui's own sampler switch, then glBlendFunc(GL_SRC_ALPHA,
 *  GL_ONE). ImGui's render state reset puts its alpha blending back.
 *
 *  Called after ImGui_ImplOpenGL3_Init(), which installs the sampler callbacks these rely on.
 */
void install_additive_blending() noexcept;

/**
 * @brief Removes the callbacks again, before ImGui_ImplOpenGL3_Shutdown().
 */
void uninstall_additive_blending() noexcept;

} // namespace tca::engine::rendering::win32::opengl3
