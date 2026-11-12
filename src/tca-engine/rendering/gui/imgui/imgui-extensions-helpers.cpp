// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/gui/imgui/imgui-extensions-helpers.hpp"

// C++ STL
#include <array>
#include <cstddef>

// Third-party
#include <imgui.h>
#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif // _WIN32
#include <imgui_markdown.h>
#ifndef _WIN32
#pragma GCC diagnostic pop
#endif // _WIN32

namespace tca::engine::imgui {

namespace {

// Heading sizes expressed as multiples of the body text size, so the hierarchy holds at any DPI:
// the sizes are always derived from the same body size the surrounding paragraphs use. The values
// stay close to how an editor's Markdown preview lays out a document - a prominent H1, a clearly
// smaller H2, and an H3 only slightly larger than body text.
constexpr float H1_SIZE_RATIO{1.6f};
constexpr float H2_SIZE_RATIO{1.3f};
constexpr float H3_SIZE_RATIO{1.15f};

// Per-level heading appearance, indexed H1, H2, H3. Only the top two levels get an underline, so a
// short H3 does not end up dominated by a full-width rule beneath it.
struct heading_appearance {
    float size_ratio{};
    bool separator{};
};
constexpr std::array<heading_appearance, ImGui::MarkdownConfig::NUMHEADINGS> HEADING_APPEARANCES{{
    {H1_SIZE_RATIO, true},
    {H2_SIZE_RATIO, true},
    {H3_SIZE_RATIO, false},
}};

// The heading font last registered through set_markdown_heading_font(). Kept at translation-unit
// scope because the ImGui font atlas it points into is process-wide global state, and markdown()
// has no other channel through which the caller could hand it a font.
markdown_heading_font g_headingFont{};

} // namespace

void set_markdown_heading_font(const markdown_heading_font& headingFont) noexcept {
    g_headingFont = headingFont;
}

void markdown(const std::string_view markdownText, const markdown_flavor flavor) {
    ImGui::MarkdownConfig config{};

    // Translate the requested dialect into the extension's parsing flags. The extension defaults
    // to its own legacy pseudo-Markdown for backwards compatibility, which keeps every redundant
    // newline: text read straight out of a .md file would then render with a blank line between
    // every paragraph and a spurious one before every heading.
    config.formatFlags = (flavor == markdown_flavor::github)
                             ? ImGuiMarkdownFormatFlags_GithubStyle
                             : ImGuiMarkdownFormatFlags_None;

    // Give headings a real size hierarchy instead of the extension's default, which draws every
    // heading in the current (body) font: each level gets the registered heading font scaled to a
    // multiple of the body size. Skipped when no body size has been registered yet, leaving the
    // extension's flat defaults in place rather than collapsing every heading to zero size.
    if (g_headingFont.body_font_size > 0.0f) {
        for (std::size_t level{0}; level < HEADING_APPEARANCES.size(); ++level) {
            ImGui::MarkdownHeadingFormat& headingFormat{config.headingFormats[level]};
            headingFormat.font = g_headingFont.font;
            headingFormat.separator = HEADING_APPEARANCES[level].separator;
            headingFormat.fontSize = g_headingFont.body_font_size * HEADING_APPEARANCES[level].size_ratio;
        }
    }

    // The extension takes an explicit length, so markdownText does not need to be
    // null-terminated: data() is never read past markdownText's own end.
    ImGui::Markdown(markdownText.data(), markdownText.length(), config);
}

} // namespace tca::engine::imgui
