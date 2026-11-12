// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <memory>
#include <stop_token>

#include "application/application.hpp"
#include "input/input-frame.hpp"

namespace tca::engine::application {

/**
 * @brief A console-based application implementation. This application will run in the
 * terminal and will not create any windows or use any graphical APIs. It will simply run the
 * application logic in a loop until the application is closed, and it will use standard input and
 * output for any user interaction. This can be useful for testing purposes, or for running
 * applications that don't require a graphical interface, such as command-line tools or server
 * applications.
 */
class simple_console_application : public application_base {
public:
    /**
     * @brief If an application logic is set, this method will run the application logic in a loop
     * until the tick() method returns false, indicating that the application should be closed.
     *
     * @note Throws if no application logic is set before calling this method.
     */
    void run() override;

private:
    std::unique_ptr<application_controller> m_controller{};
    std::stop_token m_exitToken{};

    // A console has no keyboard of its own to read: nothing writes into this frame, so the logic
    // sees it empty every tick.
    input::input_frame m_noInput{};

    application_controller& get_or_create_controller() override;
};

} // namespace tca::engine::application
