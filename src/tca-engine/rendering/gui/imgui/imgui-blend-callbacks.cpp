// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/imgui/imgui-blend-callbacks.hpp"

namespace tca::engine::imgui {

namespace {

additive_blend_callbacks& installed_callbacks() noexcept {
    static additive_blend_callbacks callbacks{};
    return callbacks;
}

} // namespace

const additive_blend_callbacks& get_additive_blend_callbacks() noexcept {
    return installed_callbacks();
}

void set_additive_blend_callbacks(const additive_blend_callbacks& callbacks) noexcept {
    installed_callbacks() = callbacks;
}

} // namespace tca::engine::imgui
