// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/directx-12/dx12-additive-blending.hpp"

// C++ STL
#include <array>
#include <climits>
#include <cstddef>
#include <string_view>

// Third-party
#include <d3dcompiler.h>
#include <wil/result.h>
#include <gsl/gsl>

#include <backends/imgui_impl_dx12.h>
#include <imgui.h>

// Engine
#include "rendering/gui/imgui/imgui-blend-callbacks.hpp"

namespace tca::engine::rendering::win32::directx12 {

namespace {

// The ImGui DirectX 12 backend's own shaders (imgui_impl_dx12.cpp), so additive draws are shaded
// exactly like every other ImGui draw: the vertex colour times the texture.
constexpr std::string_view VERTEX_SHADER{R"(
cbuffer vertexBuffer : register(b0) {
    float4x4 ProjectionMatrix;
};
struct VS_INPUT {
    float2 pos : POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};
PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));
    output.col = input.col;
    output.uv  = input.uv;
    return output;
})"};

constexpr std::string_view PIXEL_SHADER{R"(
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};
SamplerState sampler0 : register(s0);
Texture2D texture0 : register(t0);
float4 main(PS_INPUT input) : SV_Target {
    return input.col * texture0.Sample(sampler0, input.uv);
})"};

// The root parameters ImGui's backend binds: the projection as 16 constants, then the texture.
constexpr UINT PROJECTION_ROOT_PARAMETER{0};
constexpr UINT PROJECTION_CONSTANTS{16};

// The pipelines currently installed: the callbacks have no other way to reach them.
const additive_blend_pipelines* g_installedPipelines{nullptr};

wil::com_ptr<ID3DBlob> compile_shader(const std::string_view source, const char* const target) {
    wil::com_ptr<ID3DBlob> shader{};
    THROW_IF_FAILED_MSG(D3DCompile(source.data(), source.size(), nullptr, nullptr, nullptr, "main",
                                   target, 0, 0, &shader, nullptr),
                        "Failed to compile the additive blending %hs shader.", target);
    return shader;
}

// The same layout as ImGui's root signature, so its draw loop binds textures into it unchanged,
// with a static sampler of the given filter.
wil::com_ptr<ID3D12RootSignature> create_root_signature(ID3D12Device& device,
                                                        const D3D12_FILTER filter) {
    const D3D12_DESCRIPTOR_RANGE textureRange{
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
        .OffsetInDescriptorsFromTableStart = 0,
    };

    std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants = {
        .ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = PROJECTION_CONSTANTS};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &textureRange};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    const D3D12_STATIC_SAMPLER_DESC sampler{
        .Filter = filter,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .MipLODBias = 0.0f,
        .MaxAnisotropy = 0,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
        .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
        .MinLOD = 0.0f,
        .MaxLOD = D3D12_FLOAT32_MAX,
        .ShaderRegister = 0,
        .RegisterSpace = 0,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    };

    const D3D12_ROOT_SIGNATURE_DESC desc{
        .NumParameters = static_cast<UINT>(parameters.size()),
        .pParameters = parameters.data(),
        .NumStaticSamplers = 1,
        .pStaticSamplers = &sampler,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS,
    };

    wil::com_ptr<ID3DBlob> serialized{};
    THROW_IF_FAILED_MSG(
        D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, nullptr),
        "Failed to serialize the additive blending root signature.");

    wil::com_ptr<ID3D12RootSignature> rootSignature{};
    THROW_IF_FAILED(device.CreateRootSignature(0, serialized->GetBufferPointer(),
                                               serialized->GetBufferSize(),
                                               IID_PPV_ARGS(&rootSignature)));
    return rootSignature;
}

wil::com_ptr<ID3D12PipelineState> create_pipeline(ID3D12Device& device,
                                                  ID3D12RootSignature& rootSignature,
                                                  const DXGI_FORMAT rtvFormat,
                                                  ID3DBlob& vertexShader, ID3DBlob& pixelShader) {
    static constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 3> INPUT_LAYOUT{
        {
         {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
             static_cast<UINT>(offsetof(ImDrawVert, pos)),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
         {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
             static_cast<UINT>(offsetof(ImDrawVert, uv)),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
         {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,
             static_cast<UINT>(offsetof(ImDrawVert, col)),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
         }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.NodeMask = 1;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.pRootSignature = &rootSignature;
    desc.SampleMask = UINT_MAX;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = rtvFormat;
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    desc.VS = {vertexShader.GetBufferPointer(), vertexShader.GetBufferSize()};
    desc.PS = {pixelShader.GetBufferPointer(), pixelShader.GetBufferSize()};
    desc.InputLayout = {INPUT_LAYOUT.data(), static_cast<UINT>(INPUT_LAYOUT.size())};

    // The colour, weighted by its alpha, is added to what is there. The target's alpha is left as
    // it is: nothing reads it back.
    D3D12_RENDER_TARGET_BLEND_DESC& blend{desc.BlendState.RenderTarget[0]};
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_ONE;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ZERO;
    blend.DestBlendAlpha = D3D12_BLEND_ONE;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC& rasterizer{desc.RasterizerState};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC& depthStencil{desc.DepthStencilState};
    depthStencil.DepthEnable = FALSE;
    depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depthStencil.StencilEnable = FALSE;
    depthStencil.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencil.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencil.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthStencil.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depthStencil.BackFace = depthStencil.FrontFace;

    wil::com_ptr<ID3D12PipelineState> pipeline{};
    THROW_IF_FAILED_MSG(device.CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline)),
                        "Failed to create the additive blending pipeline state.");
    return pipeline;
}

// ImGui's orthographic projection of the draw data being rendered, as its backend builds it.
std::array<float, PROJECTION_CONSTANTS> projection_of(const ImDrawData& drawData) noexcept {
    const float left{drawData.DisplayPos.x};
    const float right{drawData.DisplayPos.x + drawData.DisplaySize.x};
    const float top{drawData.DisplayPos.y};
    const float bottom{drawData.DisplayPos.y + drawData.DisplaySize.y};
    return {
        2.0f / (right - left),
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        2.0f / (top - bottom),
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.5f,
        0.0f,
        (right + left) / (left - right),
        (top + bottom) / (bottom - top),
        0.5f,
        1.0f,
    };
}

} // namespace

additive_blend_pipelines::additive_blend_pipelines(ID3D12Device& device,
                                                   const DXGI_FORMAT rtvFormat) {
    Expects(g_installedPipelines == nullptr);

    const wil::com_ptr<ID3DBlob> vertexShader{compile_shader(VERTEX_SHADER, "vs_5_0")};
    const wil::com_ptr<ID3DBlob> pixelShader{compile_shader(PIXEL_SHADER, "ps_5_0")};

    m_rootSignatures[linear_sampler] =
        create_root_signature(device, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    m_rootSignatures[nearest_sampler] =
        create_root_signature(device, D3D12_FILTER_MIN_MAG_MIP_POINT);
    for (std::size_t sampler{0}; sampler < sampler_count; ++sampler) {
        m_pipelines[sampler] = create_pipeline(device, *m_rootSignatures[sampler], rtvFormat,
                                               *vertexShader, *pixelShader);
    }

    g_installedPipelines = this;
    imgui::set_additive_blend_callbacks(imgui::additive_blend_callbacks{
        .set_additive_linear = set_additive_linear,
        .set_additive_nearest = set_additive_nearest,
    });
}

additive_blend_pipelines::~additive_blend_pipelines() noexcept {
    imgui::set_additive_blend_callbacks({});
    g_installedPipelines = nullptr;
}

void additive_blend_pipelines::set_additive_linear(const ImDrawList* /*drawList*/,
                                                   const ImDrawCmd* /*command*/) {
    if (g_installedPipelines != nullptr) {
        g_installedPipelines->bind(linear_sampler);
    }
}

void additive_blend_pipelines::set_additive_nearest(const ImDrawList* /*drawList*/,
                                                    const ImDrawCmd* /*command*/) {
    if (g_installedPipelines != nullptr) {
        g_installedPipelines->bind(nearest_sampler);
    }
}

// Only ever called by ImGui's draw loop, from a callback: the backend's render state is there.
void additive_blend_pipelines::bind(const sampler_index sampler) const {
    const auto* const renderState{static_cast<const ImGui_ImplDX12_RenderState*>(
        ImGui::GetPlatformIO().Renderer_RenderState)};
    const ImDrawData* const drawData{ImGui::GetDrawData()};
    if (renderState == nullptr || drawData == nullptr) {
        return;
    }

    ID3D12GraphicsCommandList& commandList{*renderState->CommandList};
    commandList.SetPipelineState(m_pipelines[sampler].get());
    commandList.SetGraphicsRootSignature(m_rootSignatures[sampler].get());

    // A root signature switch drops every root argument. The texture is bound again by ImGui before
    // each draw, but the projection is only set when ImGui sets up its render state.
    const std::array<float, PROJECTION_CONSTANTS> projection{projection_of(*drawData)};
    commandList.SetGraphicsRoot32BitConstants(PROJECTION_ROOT_PARAMETER, PROJECTION_CONSTANTS,
                                              projection.data(), 0);
}

} // namespace tca::engine::rendering::win32::directx12
