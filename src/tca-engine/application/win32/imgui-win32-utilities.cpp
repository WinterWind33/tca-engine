// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/imgui-win32-utilities.hpp"

// C++ STL
#include <filesystem>

// Third-party
#include <imgui.h>

#include "application/application-logger.hpp"
#include "rendering/gui/imgui/imgui-extensions-helpers.hpp"
#include "rendering/gui/imgui/imgui-theme.hpp"
#include "rendering/theming/active-theme.hpp"

namespace tca::engine::application::win32 {

namespace {
constexpr char CONSOLA_FONT_PATH[] = "C:/Windows/Fonts/consola.ttf";
constexpr char CONSOLA_BOLD_FONT_PATH[] = "C:/Windows/Fonts/consolab.ttf";
constexpr float DEFAULT_FONT_SIZE{14.0f};

void setup_font(const platform::win32::type_traits::win32_string_view_t appName,
                const float mainScale) {
    ImGuiIO& io{ImGui::GetIO()};
    // The body size every other text size is derived from, kept scaled by mainScale so it matches
    // whatever DPI the rest of the ImGui style was scaled for.
    const float bodyFontSize{DEFAULT_FONT_SIZE * mainScale};

    // Try to set consola.ttf as the default font for ImGui, since it is a monospaced font that is
    // included in Windows and it looks good in the ImGui interface.
    if (std::filesystem::exists(CONSOLA_FONT_PATH)) {
        get_application_logger()->info(
            "[ {} ] Setting Consolas as the default font for ImGui",
            platform::win32::type_traits::from_win32_string_view(appName));
        io.Fonts->AddFontFromFileTTF(CONSOLA_FONT_PATH, bodyFontSize);
    } else {
        get_application_logger()->warn(
            "[ {} ] Consolas font not found, using default ImGui font",
            platform::win32::type_traits::from_win32_string_view(appName));
    }

    // Load the bold Consolas face so Markdown headings render bold, not just larger. It is loaded
    // at the body size and later re-scaled per heading level by the Markdown helper, taking
    // advantage of ImGui's dynamic fonts (a single face renders at any size). When the bold face is
    // missing, headings still gain their size hierarchy through the null-font fallback below.
    ImFont* headingFont{nullptr};
    if (std::filesystem::exists(CONSOLA_BOLD_FONT_PATH)) {
        headingFont = io.Fonts->AddFontFromFileTTF(CONSOLA_BOLD_FONT_PATH, bodyFontSize);
    } else {
        get_application_logger()->warn(
            "[ {} ] Consolas Bold font not found, Markdown headings will not be bold",
            platform::win32::type_traits::from_win32_string_view(appName));
    }

    tca::engine::imgui::set_markdown_heading_font(tca::engine::imgui::markdown_heading_font{
        .font = headingFont,
        .body_font_size = bodyFontSize,
    });
}
} // namespace

void initialize_imgui_context(const platform::win32::type_traits::win32_string_view_t appName,
                              const float mainScale) {
    get_application_logger()->info("[ {} ] Initializing ImGui context for application",
                                   platform::win32::type_traits::from_win32_string_view(appName));
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Only an active widget (a text field being typed in, a modal) takes the keyboard from the
    // game: with keyboard navigation on, a merely focused ImGui window would otherwise set
    // WantCaptureKeyboard and starve the game of keys.
    io.ConfigNavCaptureKeyboard = false;
    // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    setup_font(appName, mainScale);

    // The application's theme is what reaches the ImGui style, rather than a hardcoded
    // StyleColorsDark(): whatever the game set through application::set_theme() is already the
    // active theme by this point, and a game that set none gets the engine's default. Sizes are
    // baked at mainScale here, so changing the DPI means applying the theme again - which is
    // exactly what on_dpi_scale_changed() below does.
    tca::engine::imgui::apply_theme(theming::get_active_theme(), mainScale);

    // Set initial font scale. (in docking branch: using
    // io.ConfigDpiScaleFonts=true automatically overrides this
    // for every window depending on the current monitor)
    ImGui::GetStyle().FontScaleDpi = mainScale;
}

float on_dpi_scale_changed(const platform::win32::type_traits::win32_string_view_t appName,
                           float newDpiScale) {
    get_application_logger()->info("[ {} ] DPI scale changed to {}, updating ImGui style and fonts",
                                   platform::win32::type_traits::from_win32_string_view(appName),
                                   newDpiScale);
    // Re-applied from the theme at the new scale rather than rescaled in place: the theme's sizes
    // are the authoritative unscaled ones, so this cannot accumulate rounding across DPI changes
    // the way repeated relative rescaling does.
    tca::engine::imgui::apply_theme(theming::get_active_theme(), newDpiScale);
    ImGui::GetStyle().FontScaleDpi = newDpiScale;

    ImGuiIO& io{ImGui::GetIO()};
    io.Fonts->Clear();
    setup_font(appName, newDpiScale);
    io.Fonts->Build();

    return newDpiScale;
}

void consume_input_captured_by_imgui(const input::input_view input) noexcept {
    if (ImGui::GetIO().WantCaptureKeyboard) {
        input.consume_all();
    }
}

} // namespace tca::engine::application::win32
