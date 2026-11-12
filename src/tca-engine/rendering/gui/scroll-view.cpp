// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/scroll-view.hpp"

// C++ STL
#include <utility>

// Third-party
#include <imgui.h>

#include "rendering/gui/imgui/imgui-math-helpers.hpp"

namespace tca::engine::widgets {

scroll_view::scroll_view(const std::string_view id, const math::vector2 size,
                         const bool alwaysShowVerticalScrollbar)
    : m_id{id},
      m_size{size},
      m_alwaysShowVerticalScrollbar{alwaysShowVerticalScrollbar} {}

void scroll_view::render() {
    const ImGuiWindowFlags windowFlags{m_alwaysShowVerticalScrollbar
                                           ? ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysVerticalScrollbar
                                           : ImGuiWindowFlags_::ImGuiWindowFlags_None};

    // BeginChild() returns false when the child region is clipped/collapsed, in which case
    // rendering its content would be wasted work: ImGui's own convention is to skip it in that
    // case, still calling EndChild() unconditionally regardless of the return value.
    if (ImGui::BeginChild(m_id.c_str(), imgui::to_imvec2(m_size), ImGuiChildFlags_::ImGuiChildFlags_None,
                          windowFlags)) {
        // Explicitly qualified to pausable_renderables_view (rather than left as a plain virtual
        // call, which would already resolve here since this is scroll_view's own render()) so a
        // reader doesn't mistake this for a call to the non-pause-aware renderable_view::render(),
        // which would silently ignore any pause_renderable()/resume_renderable() calls on children
        // added to this view.
        pausable_renderables_view::render();
    }
    ImGui::EndChild();
}

} // namespace tca::engine::widgets
