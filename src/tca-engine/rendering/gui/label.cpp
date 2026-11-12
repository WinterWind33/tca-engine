// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/label.hpp"

// Third-party
#include <imgui.h>

#include "rendering/gui/imgui/imgui-color-helpers.hpp"
#include "rendering/theming/active-theme.hpp"

namespace tca::engine::widgets {

label::label(const std::string_view text, const theming::typography_role textRole,
             const theming::color_role textColorRole)
    : m_text{text},
      m_typographyRole{textRole},
      m_colorRole{textColorRole} {}

void label::render() {
    if (m_text.empty()) {
        // If there is no text to render, we can just skip rendering the label.
        return;
    }

    // Resolved at render time rather than cached at construction, so a theme swapped at runtime
    // reaches labels that were built long before it.
    const imgui::scoped_style_color textColor{ImGuiCol_Text, m_colorRole};
    const theming::typography textTypography{theming::typography_for(m_typographyRole)};

    // Keep the current font (null) and push the size the role asks for, as a multiple of the
    // style's *unscaled* base size: ImGui applies the global factors (FontScaleMain,
    // FontScaleDpi) on top of what is pushed, so deriving the size from the already-scaled
    // ImGui::GetFontSize() would apply DPI scaling twice.
    //
    // The size has to be passed to PushFont() rather than written into the font afterwards. ImGui
    // bakes the size when the font is pushed, so a scale assigned to ImFont::Scale after the push
    // does not reach this label at all - it lands on whichever label is pushed next, which sized
    // every label by its predecessor's role.
    ImGui::PushFont(nullptr,
                    ImGui::GetStyle().FontSizeBase * static_cast<float>(textTypography.scale));

    // Using "%s" format to avoid compilers throwing errors.
    ImGui::Text("%s", m_text.c_str());

    ImGui::PopFont();
}

void label::set_text(const std::string_view newText) {
    m_text = newText;
}

} // namespace tca::engine::widgets
