// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <string>
#include <string_view>

#include "rendering/gui/widget.hpp"
#include "rendering/theming/theme-roles.hpp"

namespace tca::engine::widgets {

/**
 * @brief A label is a simple widget that renders a text. It can be used to display static
 * information or to label other widgets.
 *
 * The text of the label can be set and retrieved, and it will be rendered as is, so it should
 * already be formatted as desired before being set. If the text is empty, the label will not render
 * anything, so it can be used to hide the label without having to remove it from the GUI layout.
 *
 * A label is described by the *roles* its text plays in the interface - how it is sized, and what
 * kind of content it is - and never by a size or a colour. Its appearance therefore comes from the
 * active theme (see theming::get_active_theme()), so restyling the application restyles every
 * label with it, and two labels sharing a role cannot drift apart.
 */
class label : public rendering::renderable {
public:
    /**
     * @brief Construct an empty label object. The label will not render anything until the text is
     * set, so this can be useful if we want to create a label and set its text later on.
     */
    label() noexcept = default;

    /**
     * @brief Constructs a new label object with the given text. The text should already be
     * formatted as desired, since the label will just render it as is.
     *
     * @param text The text to render in the label.
     * @param textRole How the text is sized: what it is in the page (body copy, a caption, a
     * heading). Defaults to body copy.
     * @param textColorRole What the text is painted with. Defaults to the primary content colour
     * on a surface.
     */
    explicit label(std::string_view text,
                   theming::typography_role textRole = theming::typography_role::body,
                   theming::color_role textColorRole = theming::color_role::on_surface);

    /**
     * @brief Sets the text of the label. The text should already be formatted as desired, since the
     * label will just render it as is.
     *
     * @param newText The new text to render in the label.
     */
    void set_text(std::string_view newText);

    /**
     * @brief Retrieves the current text of the label.
     *
     * @return std::string_view The current text of the label.
     */
    std::string_view get_text() const noexcept {
        return m_text;
    }

    /**
     * @brief Changes how this label's text is sized, by naming what it is rather than how big it
     * should be.
     *
     * @param textRole The typography role to resolve against the active theme from now on.
     */
    void set_typography_role(const theming::typography_role textRole) noexcept {
        m_typographyRole = textRole;
    }

    /**
     * @brief Retrieves the typography role this label's text is sized by.
     *
     * @return theming::typography_role The label's typography role.
     */
    theming::typography_role get_typography_role() const noexcept {
        return m_typographyRole;
    }

    /**
     * @brief Changes what this label's text is painted with, by naming its part in the interface
     * rather than a colour.
     *
     * @param textColorRole The colour role to resolve against the active theme from now on.
     */
    void set_color_role(const theming::color_role textColorRole) noexcept {
        m_colorRole = textColorRole;
    }

    /**
     * @brief Retrieves the colour role this label's text is painted with.
     *
     * @return theming::color_role The label's colour role.
     */
    theming::color_role get_color_role() const noexcept {
        return m_colorRole;
    }

protected:
    std::string m_text{};
    theming::typography_role m_typographyRole{theming::typography_role::body};
    theming::color_role m_colorRole{theming::color_role::on_surface};

    void render() override;
};

static_assert(widget<label>);

} // namespace tca::engine::widgets
