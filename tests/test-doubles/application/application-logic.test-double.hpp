// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <functional>

#include "application/application.hpp"
#include "input/input-frame.hpp"

namespace tca::engine::application::test_doubles {

/**
 * @brief Fake implementation of the application_logic interface for testing purposes. This class
 * allows the user to provide custom callbacks for the tick() and on_close() methods, and it also
 * keeps track of how many times these methods have been called.
 */
class application_logic_fake final : public application_logic {
public:
    application_logic_fake() = default;

    /**
     * @brief Construct a new application logic fake object with the provided callbacks for the
     * tick() and on_close() methods.
     *
     * @param tickCallback The callback to be called when the tick() method is called.
     * @param onCloseCallback The callback to be called when the on_close() method is called.
     * @param setAppControllerCallback The callback to be called when the set_app_controller()
     * method is called.
     * @param setImageFactoryCallback The callback to be called when the set_image_factory()
     * method is called.
     * @param setInputCallback The callback to be called when the set_input() method is called.
     */
    explicit application_logic_fake(
        std::function<bool()> tickCallback, std::function<void()> onCloseCallback,
        std::function<void(application_controller&)> setAppControllerCallback = {},
        std::function<void(rendering::image_factory&)> setImageFactoryCallback = {},
        std::function<void(input::input_view)> setInputCallback = {});

    bool tick() override;
    void on_close() override;
    void set_app_controller(application_controller& controller) override;
    void set_image_factory(rendering::image_factory& factory) override;
    void set_input(input::input_view input) override;

    std::size_t get_tick_call_count() const noexcept {
        return m_tickCallCount;
    }

    std::size_t get_on_close_call_count() const noexcept {
        return m_onCloseCount;
    }

    std::size_t get_set_app_controller_call_count() const noexcept {
        return m_appControllerSetCount;
    }

    std::size_t get_set_image_factory_call_count() const noexcept {
        return m_imageFactorySetCount;
    }

    std::size_t get_set_input_call_count() const noexcept {
        return m_inputSetCount;
    }

    /**
     * @brief Only reset calls count.
     */
    void reset() noexcept;

private:
    std::function<bool()> m_tickCallback{};
    std::function<void()> m_onCloseCallback{};
    std::function<void(application_controller&)> m_setAppControllerCallback{};
    std::function<void(rendering::image_factory&)> m_setImageFactoryCallback{};
    std::function<void(input::input_view)> m_setInputCallback{};
    std::size_t m_tickCallCount{};
    std::size_t m_onCloseCount{};
    std::size_t m_appControllerSetCount{};
    std::size_t m_imageFactorySetCount{};
    std::size_t m_inputSetCount{};
};

} // namespace tca::engine::application::test_doubles
