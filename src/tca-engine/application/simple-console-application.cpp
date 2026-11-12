// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/simple-console-application.hpp"

// C++ STL
#include <functional>
#include <stdexcept>
#include <stop_token>
#include <utility>

#include "application/application-logger.hpp"

namespace tca::engine::application {

namespace {
class simple_console_application_controller : public application_controller {
public:
    simple_console_application_controller(std::function<void(theming::theme)> themeSetter,
                                          std::stop_token& exitToken)
        : m_themeSetter{std::move(themeSetter)} {
        exitToken = m_exitSource.get_token();
    }

    void exit() override {
        m_exitSource.request_stop();
    }

    void go_fullscreen() override {
        // This application doesn't have a window, so we can't go fullscreen. We simply ignore this
        // request.
    }

    void go_windowed() override {
        // This application doesn't have a window, so we can't go windowed. We simply ignore this
        // request.
    }

    void use_theme(theming::theme newTheme) override {
        // This application has no window and no GUI backend, so there is no style and no clear
        // colour for a theme to reach. It is still routed through the application rather than
        // dropped: the theme a console application wears is what get_theme() reports and what any
        // role resolved by game code resolves against, and silently ignoring it here would make
        // the same call mean two different things depending on which application is running.
        m_themeSetter(std::move(newTheme));
    }

    void set_content_size([[maybe_unused]] window_content_size contentSize,
                          [[maybe_unused]] content_size_options options) override {
        // This application doesn't have a window, so there is no content area to resize. We simply
        // ignore this request.
    }

    bool is_fullscreen() const noexcept override {
        // This application doesn't have a window, so it can never be fullscreen.
        return false;
    }

    void center_window() override {
        // This application doesn't have a window, so there is nothing to centre. We simply ignore
        // this request.
    }

private:
    std::function<void(theming::theme)> m_themeSetter{};
    std::stop_source m_exitSource{};
};
} // namespace

application_controller& simple_console_application::get_or_create_controller() {
    if (m_controller == nullptr) {
        m_controller = std::make_unique<simple_console_application_controller>(
            [this](theming::theme newTheme) {
                set_theme(std::move(newTheme));
            },
            m_exitToken);
    }

    return *m_controller;
}

void simple_console_application::run() {
    begin_application_log(get_app_name(), get_company_name());

    auto* const logic{get_logic()};

    const auto logger{get_application_logger()};
    logger->info("TCA Engine: Welcome! Begin logging");

    if (logic == nullptr) {
        logger->critical(
            "Cannot run the application because no application logic has been set. Please set the "
            "application logic before running the application.");
        throw std::runtime_error(
            "Application logic has not been set. Please set the application logic before running "
            "the application.");
    }

    // Set the input view to the logic.
    logic->set_input(input::input_view{m_noInput});

    logger->info("Starting simple console application.");

    while (!m_exitToken.stop_requested() && logic->tick()) {
        // We simply run the application logic in a loop until the tick() method returns false,
        // indicating that the application should be closed.
    }

    logger->info("Application is closing.");
    logic->on_close();
}

} // namespace tca::engine::application
