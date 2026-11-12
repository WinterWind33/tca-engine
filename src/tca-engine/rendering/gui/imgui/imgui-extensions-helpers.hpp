// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <string_view>

// Forward declaration of the ImGui font handle, so this header does not have to pull in <imgui.h>
// just to name it.
struct ImFont;

namespace tca::engine::imgui {

/**
 * @brief Font and metrics markdown() uses to give headings a visible size hierarchy instead of
 * drawing every heading at body-text size.
 */
struct markdown_heading_font {
    /**
     * @brief Font applied to every heading, typically a bold face so headings read as headings and
     * not merely as larger body text. When null, headings keep the current font and only their
     * size changes.
     */
    ImFont* font{nullptr};

    /**
     * @brief Pixel size of the body text the headings are measured against; each heading level is
     * sized as a fixed multiple of this. When zero, headings fall back to their font's own default
     * size and gain no hierarchy, which restores the previous flat rendering.
     */
    float body_font_size{0.0f};
};

/**
 * @brief Registers the font and body base size markdown() should use for headings.
 *
 * @note The ImFont pointer is only valid for the currently built font atlas, so this must be
 * called once the atlas has been built and again whenever it is rebuilt (for example after a DPI
 * change clears and rebuilds the fonts).
 *
 * @param headingFont The heading font and body base size. A default-constructed value disables the
 * heading hierarchy, restoring flat, body-sized headings.
 */
void set_markdown_heading_font(const markdown_heading_font& headingFont) noexcept;

/**
 * @brief Selects which Markdown dialect markdown() should assume the given text is written in.
 */
enum class markdown_flavor : std::uint8_t {
    /**
     * @brief The Markdown extension's own legacy "pseudo-Markdown": every newline is significant,
     * so the blank line separating two paragraphs in a real .md file is rendered as an actual
     * empty line. Only meaningful for text authored against that dialect.
     */
    pseudo,

    /**
     * @brief Standard, CommonMark/GitHub-accurate parsing: redundant newlines are collapsed, so
     * text read straight out of a .md file lays out the way it reads in the file.
     */
    github,
};

/**
 * @brief Renders Markdown text using the ImGui Markdown extension, wrapped to the current ImGui
 * window's content width.
 *
 * @param markdownText The Markdown text to render. Does not need to be null-terminated.
 * @param flavor The Markdown dialect markdownText is written in. Defaults to github, since text
 * meant for this function is normally authored as an ordinary .md file.
 */
void markdown(std::string_view markdownText, markdown_flavor flavor = markdown_flavor::github);

} // namespace tca::engine::imgui
