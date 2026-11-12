// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Third-party
#include <imgui.h>

namespace tca::engine::imgui {

/**
 * @brief The draw callbacks a rendering backend provides to switch to additive blending.
 *
 * ImGui's platform IO has standard callbacks for the sampler (DrawCallback_SetSamplerLinear,
 * DrawCallback_SetSamplerNearest) but none for blending, so the backends the engine wires up
 * install these instead. Each one switches to additive blending sampled with its filter, whatever
 * the backend was doing before; ImGuiPlatformIO::DrawCallback_ResetRenderState switches back to
 * ImGui's own alpha blending, sampled linearly.
 */
struct additive_blend_callbacks {
    ImDrawCallback set_additive_linear{nullptr};
    ImDrawCallback set_additive_nearest{nullptr};

    /**
     * @brief Tells whether both callbacks are there: a backend has either both or none.
     */
    [[nodiscard]] constexpr bool available() const noexcept {
        return set_additive_linear != nullptr && set_additive_nearest != nullptr;
    }
};

/**
 * @brief The additive blending callbacks of the backend currently rendering, or none (two null
 *  callbacks) if it has none, as in a headless context.
 */
[[nodiscard]] const additive_blend_callbacks& get_additive_blend_callbacks() noexcept;

/**
 * @brief Installs the callbacks a backend draws additively with, or removes them with a
 *  default-constructed value. Called by a backend when it is set up and torn down.
 *
 *  Process-wide rather than per ImGui context: the engine runs one renderer at a time.
 */
void set_additive_blend_callbacks(const additive_blend_callbacks& callbacks) noexcept;

} // namespace tca::engine::imgui
