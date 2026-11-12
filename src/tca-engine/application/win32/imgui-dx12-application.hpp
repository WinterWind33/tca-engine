// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Windows
#include "platform/win32/windows.hpp"

// C++ STL
#include <memory>

// Third-party
#include <backends/imgui_impl_dx12.h>

#include "application/win32/directx12-application.hpp"
#include "rendering/win32/directx-12/dx12-additive-blending.hpp"

namespace tca::engine::application::win32 {

/**
 * @brief Application class that integrates Dear ImGui with DirectX 12 rendering. This class extends
 * the directx12_application class to provide functionality for initializing and rendering Dear
 * ImGui interfaces using DirectX 12.
 */
class imgui_dx12_application : public directx12_application {
public:
    /**
     * @brief Construct a new imgui dx12 application object initializing the command list filling
     * for DirectX 12 rendering.
     */
    imgui_dx12_application();

private:
    float m_mainScale{};
    ImGui_ImplDX12_InitInfo m_imguiInitInfo{};
    // Tracks whether initialize_imgui_rendering_backend() completed successfully, so close() knows
    // whether the ImGui Win32/DX12 backends actually need shutting down. This matters because
    // close() can also run after a partial initialize() failure (e.g. thrown before the backends
    // were ever initialized), in which case calling their Shutdown() functions would be invalid.
    bool m_imguiBackendInitialized{};
    // What the scene draws additively with: it needs ImGui's backend set up, and goes before it.
    std::unique_ptr<rendering::win32::directx12::additive_blend_pipelines> m_additiveBlending{};

    void initialize() override;
    void initialize_imgui_rendering_backend();

    bool pre_logic_tick() override;
    void post_logic_tick() override;
    void close() override;

    void fill_command_list(const wil::com_ptr<ID3D12GraphicsCommandList>& commandList);
};

} // namespace tca::engine::application::win32
