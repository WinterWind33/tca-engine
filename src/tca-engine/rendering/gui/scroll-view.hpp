// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <string>
#include <string_view>

#include "core/math/vector.hpp"
#include "rendering/gui/callback-renderable.hpp"
#include "rendering/gui/widget.hpp"
#include "rendering/tickables.hpp"

namespace tca::engine::widgets {

/**
 * @brief A container widget whose content can be scrolled (via mouse wheel or scrollbar drag) if
 * it doesn't fit within the given size, backed by ImGui's own child-window scrolling.
 */
class scroll_view final : public rendering::pausable_renderables_view {
public:
    using render_callback = callback_renderable::render_callback;

    /**
     * @brief Construct a new scroll view object.
     *
     * @param id Unique ID identifying this scroll view's underlying ImGui child region. Must be
     * unique among sibling widgets rendered within the same ImGui window, exactly like any other
     * ImGui ID.
     * @param size The size of the scrollable region. Zero on either axis stretches to fill the
     * remaining available content space on that axis (see ImGui::BeginChild()).
     * @param alwaysShowVerticalScrollbar Whether the vertical scrollbar should always take up its
     * space, even while the content doesn't overflow (see
     * ImGuiWindowFlags_AlwaysVerticalScrollbar). Off by default, matching plain ImGui child
     * scrolling behavior.
     */
    explicit scroll_view(std::string_view id, math::vector2 size = math::vector2::zero(),
                         bool alwaysShowVerticalScrollbar = false);

    void render() override;

private:
    std::string m_id{};
    math::vector2 m_size{};
    bool m_alwaysShowVerticalScrollbar{false};
};

static_assert(widget<scroll_view>);

} // namespace tca::engine::widgets
