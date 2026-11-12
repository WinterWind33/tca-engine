// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/application.hpp"

// C++ STL
#include <utility>

// Third-party
#include <gsl/gsl>

#include "input/input-frame.hpp"
#include "rendering/gui/imgui/imgui-theme.hpp"

#ifdef _WIN32
#include "application/win32/imgui-dx12-application.hpp"
#include "application/win32/imgui-opengl3-win32-application.hpp"
#else
#include "application/simple-console-application.hpp"
#endif // _WIN32

namespace tca::engine::application {

namespace {
// We insert the ifdefs here to avoid polluting the public functions with platform-specific code.

#ifdef _WIN32
// On Windows, we create a DirectX 12-based application.
std::unique_ptr<application> create_platform_application() {
    return std::make_unique<win32::imgui_dx12_application>();
}

std::unique_ptr<application> create_platform_opengl3_application() {
    return std::make_unique<win32::imgui_opengl3_win32_application>();
}

#else
// To make the engine compile even on platforms where we haven't implemented the application class
// yet, we return a simple console application.
std::unique_ptr<application> create_platform_application() {
    return std::make_unique<simple_console_application>();
}

std::unique_ptr<application> create_platform_opengl3_application() {
    return create_platform_application();
}
#endif // _WIN32

input::input_frame& get_empty_input_frame() noexcept {
    static input::input_frame empty{};
    return empty;
}

} // namespace

application& application_base::set_theme(theming::theme newTheme) {
    m_theme = newTheme;

    // Also makes it the ambient theme every widget resolves against, and mirrors it into the GUI
    // backend's style when there is one, so that a theme set after startup is not left applying to
    // the clear colour alone.
    imgui::use_theme(std::move(newTheme));
    return *this;
}

application& application_base::set_window_display_settings(window_display_settings&& settings) {
    m_windowDisplaySettings = std::move(settings);
    return *this;
}

application& application_base::set_app_name(const std::string_view name) {
    m_appName = name;
    return *this;
}

application& application_base::set_company_name(const std::string_view name) {
    m_companyName = name;
    return *this;
}

application& application_base::set_logic(std::unique_ptr<application_logic> logic) {
    Expects(logic != nullptr &&
            "Application logic cannot be set to a null pointer. Please provide a valid application "
            "logic instance.");
    m_logic = std::move(logic);
    m_logic->set_app_controller(get_or_create_controller());
    return *this;
}

void application_logic_base::set_input(input::input_view input) {
    m_inputView.emplace(std::move(input));
}

input::input_view application_logic_base::get_input() const noexcept {
    return m_inputView.value_or(input::input_view{get_empty_input_frame()});
}

std::unique_ptr<application> application_factory::create_platform_preferred_application() {
    return create_platform_application();
}

std::unique_ptr<application> application_factory::create_opengl3_application() {
    return create_platform_opengl3_application();
}

std::unique_ptr<application> application_factory::create_minimal_graphics_application() {
    return create_opengl3_application();
}

} // namespace tca::engine::application
