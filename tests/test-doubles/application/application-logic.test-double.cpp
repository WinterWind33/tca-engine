// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "test-doubles/application/application-logic.test-double.hpp"

// C++ STL
#include <utility>

namespace tca::engine::application::test_doubles {

application_logic_fake::application_logic_fake(
    std::function<bool()> tickCallback, std::function<void()> onCloseCallback,
    std::function<void(application_controller&)> setAppControllerCallback,
    std::function<void(rendering::image_factory&)> setImageFactoryCallback,
    std::function<void(input::input_view)> setInputCallback)
    : m_tickCallback(std::move(tickCallback)),
      m_onCloseCallback(std::move(onCloseCallback)),
      m_setAppControllerCallback(std::move(setAppControllerCallback)),
      m_setImageFactoryCallback(std::move(setImageFactoryCallback)),
      m_setInputCallback(std::move(setInputCallback)) {}

bool application_logic_fake::tick() {
    m_tickCallCount++;
    if (m_tickCallback) {
        return m_tickCallback();
    }
    return false;
}

void application_logic_fake::on_close() {
    m_onCloseCount++;
    if (m_onCloseCallback) {
        m_onCloseCallback();
    }
}

void application_logic_fake::reset() noexcept {
    m_tickCallCount = 0;
    m_onCloseCount = 0;
    m_appControllerSetCount = 0;
    m_imageFactorySetCount = 0;
    m_inputSetCount = 0;
}

void application_logic_fake::set_app_controller(application_controller& controller) {
    m_appControllerSetCount++;
    if (m_setAppControllerCallback) {
        m_setAppControllerCallback(controller);
    }
}

void application_logic_fake::set_image_factory(rendering::image_factory& factory) {
    m_imageFactorySetCount++;
    if (m_setImageFactoryCallback) {
        m_setImageFactoryCallback(factory);
    }
}

void application_logic_fake::set_input(input::input_view input) {
    m_inputSetCount++;
    if (m_setInputCallback) {
        m_setInputCallback(input);
    }
}

} // namespace tca::engine::application::test_doubles
