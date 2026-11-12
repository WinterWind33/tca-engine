// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <array>
#include <cstddef>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <d3d12.h>
#include <wil/com.h>

struct ImDrawList;
struct ImDrawCmd;

namespace tca::engine::rendering::win32::directx12 {

/**
 * @brief The pipelines the ImGui DirectX 12 backend draws additively with, installed as the
 *  engine's additive blend callbacks (see imgui::additive_blend_callbacks) for as long as the
 *  object lives.
 *
 * ImGui's backend bakes blending into its pipeline state and its sampler into its root signature,
 * so additive blending is one more pipeline per sampler: the same shaders, input layout and root
 * layout as ImGui's, with SrcBlend = SRC_ALPHA and DestBlend = ONE. ImGui's draw loop keeps binding
 * each command's texture to root parameter 1, so it draws through these unchanged.
 *
 * Only one can be installed at a time. Must be created after ImGui_ImplDX12_Init() and destroyed
 * before ImGui_ImplDX12_Shutdown(), once the GPU no longer uses it.
 */
class additive_blend_pipelines final {
public:
    /**
     * @brief Creates the pipelines and installs their callbacks.
     *
     * @param device The device ImGui's backend renders with.
     * @param rtvFormat The format of the render targets ImGui's backend renders into.
     * @throws wil::ResultException if a shader, root signature or pipeline cannot be created.
     */
    additive_blend_pipelines(ID3D12Device& device, DXGI_FORMAT rtvFormat);

    /**
     * @brief Removes the callbacks again.
     */
    ~additive_blend_pipelines() noexcept;

    additive_blend_pipelines(const additive_blend_pipelines&) = delete;
    additive_blend_pipelines& operator=(const additive_blend_pipelines&) = delete;
    additive_blend_pipelines(additive_blend_pipelines&&) = delete;
    additive_blend_pipelines& operator=(additive_blend_pipelines&&) = delete;

private:
    enum sampler_index : std::size_t {
        linear_sampler,
        nearest_sampler,
        sampler_count
    };

    // The callbacks: each binds the installed object's pipeline for its sampler.
    static void set_additive_linear(const ImDrawList* drawList, const ImDrawCmd* command);
    static void set_additive_nearest(const ImDrawList* drawList, const ImDrawCmd* command);

    void bind(sampler_index sampler) const;

    std::array<wil::com_ptr<ID3D12RootSignature>, sampler_count> m_rootSignatures{};
    std::array<wil::com_ptr<ID3D12PipelineState>, sampler_count> m_pipelines{};
};

} // namespace tca::engine::rendering::win32::directx12
