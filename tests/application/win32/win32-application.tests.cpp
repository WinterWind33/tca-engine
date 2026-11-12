// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/win32-application.hpp"

#include "rendering/theming/active-theme.hpp"
#include "rendering/theming/theme.hpp"

// C++ STL
#include <algorithm> // for std::find
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Platform
#include "platform/win32/win32-application-window.hpp"

// Rendering
#include "rendering/image-factory.hpp"

// Input
#include "input/input-collector.hpp"
#include "input/input-frame.hpp"
#include "input/key.hpp"

// Utilities
#include "utilities/win32/keyboard-messages.hpp"
#include "utilities/win32/wm-quit-drain.hpp"

// Test doubles
#include "test-doubles/application/application-logic.test-double.hpp"

namespace tests {

enum class order_of_calls {
    initialize,
    logic_tick,
    logic_on_close,
    close,
};

void test_correct_calls_order(const std::vector<order_of_calls>& callsOrder) {
    // We cannot be sure about the logic calls because tests may not set it.
    REQUIRE(callsOrder.size() >= 2);
    if (callsOrder.size() == 2) {
        REQUIRE(callsOrder[0] == order_of_calls::initialize);
        REQUIRE(callsOrder[1] == order_of_calls::close);
    } else if (callsOrder.size() == 3) {
        REQUIRE(callsOrder[0] == order_of_calls::initialize);
        REQUIRE(callsOrder[1] == order_of_calls::logic_tick);
        REQUIRE(callsOrder[2] == order_of_calls::close);
    } else if (callsOrder.size() == 4) {
        REQUIRE(callsOrder[0] == order_of_calls::initialize);
        REQUIRE(callsOrder[1] == order_of_calls::logic_tick);
        REQUIRE(callsOrder[2] == order_of_calls::logic_on_close);
        REQUIRE(callsOrder[3] == order_of_calls::close);
    }
}

class win32_console_application_tss final
    : public tca::engine::application::win32::win32_application {
public:
    win32_console_application_tss(std::vector<order_of_calls>& callsOrder)
        : m_callsOrder(callsOrder) {}

    tca::engine::platform::platform_preferred_string_view_t get_app_name() const noexcept {
        return win32_application::get_app_name();
    }

    bool is_initialize_called() const noexcept {
        return m_initializeCalled;
    }

    bool is_close_called() const noexcept {
        return m_closeCalled;
    }

    tca::engine::input::input_collector& get_input_collector_pub() noexcept {
        return win32_application::get_input_collector();
    }

protected:
    void initialize() override {
        win32_application::initialize();
        m_initializeCalled = true;
        m_callsOrder.push_back(order_of_calls::initialize);
    }

    void close() override {
        win32_application::close();
        m_closeCalled = true;
        m_callsOrder.push_back(order_of_calls::close);
    }

private:
    std::vector<order_of_calls>& m_callsOrder;
    bool m_initializeCalled{};
    bool m_closeCalled{};
};

class win32_windowed_application_tss final
    : public tca::engine::application::win32::win32_application {
public:
    win32_windowed_application_tss(std::vector<order_of_calls>& callsOrder)
        : m_callsOrder(callsOrder) {}

    const auto& get_native_handle() const {
        return m_window->get_native_handle();
    }

    void set_window_title(const tca::engine::platform::platform_preferred_string_view_t title) {
        m_windowTitle = title;
    }

    tca::engine::platform::platform_preferred_string_view_t get_window_title() const noexcept {
        return m_windowTitle;
    }

    bool is_initialize_called() const noexcept {
        return m_initializeCalled;
    }

    bool is_close_called() const noexcept {
        return m_closeCalled;
    }

    tca::engine::rendering::color get_background_color_pub() const noexcept {
        return win32_application::get_background_color();
    }

    const tca::engine::application::window_display_settings& get_window_display_settings_pub()
        const noexcept {
        return win32_application::get_window_display_settings();
    }

    tca::engine::platform::win32::partial_window_creation_params make_window_creation_params_pub()
        const {
        return win32_application::make_window_creation_params();
    }

    auto get_app_name_pub() const noexcept {
        return win32_application::get_app_name();
    }

    auto& get_logic_pub() const {
        return win32_application::get_logic();
    }

    std::size_t get_theme_changed_call_count() const noexcept {
        return m_themeChangedCount;
    }

    tca::engine::input::input_collector& get_input_collector_pub() noexcept {
        return win32_application::get_input_collector();
    }

    /**
     * @brief Runs the given callback in pre_logic_tick(), after the base one, and returns what it
     * returns: false skips the logic tick() for that frame.
     */
    void set_pre_logic_tick_callback(std::function<bool()> callback) {
        m_preLogicTickCallback = std::move(callback);
    }

protected:
    bool pre_logic_tick() override {
        if (!win32_application::pre_logic_tick()) {
            return false;
        }

        return m_preLogicTickCallback ? m_preLogicTickCallback() : true;
    }

    void on_theme_changed() override {
        win32_application::on_theme_changed();
        ++m_themeChangedCount;
    }

    void initialize() override {
        win32_application::initialize();
        m_initializeCalled = true;
        m_callsOrder.push_back(order_of_calls::initialize);
        auto window{
            tca::engine::platform::win32::win32_application_window::two_phase_window_creation<
                tca::engine::platform::win32::win32_application_window>(get_app_name())};
        window->set_input_collector(&get_input_collector());
        m_window = std::move(window);
    }

    void close() override {
        win32_application::close();
        m_closeCalled = true;
        m_callsOrder.push_back(order_of_calls::close);
    }

private:
    std::vector<order_of_calls>& m_callsOrder;
    tca::engine::platform::platform_preferred_string_t m_windowTitle{};
    bool m_initializeCalled{};
    bool m_closeCalled{};
    std::size_t m_themeChangedCount{};
    std::function<bool()> m_preLogicTickCallback{};
};

class win32_console_application_throwing_init_tss final
    : public tca::engine::application::win32::win32_application {
public:
    bool is_close_called() const noexcept {
        return m_closeCalled;
    }

protected:
    void initialize() override {
        win32_application::initialize();
        throw std::runtime_error{"Simulated initialization failure"};
    }

    void close() override {
        win32_application::close();
        m_closeCalled = true;
    }

private:
    bool m_closeCalled{};
};

// What the logic saw of the space key and the typed text during one tick().
struct seen_input {
    bool spaceDown{};
    bool spacePressed{};
    bool spaceReleased{};
    tca::engine::input::keyboard_state::edge_count spacePresses{};
    tca::engine::input::keyboard_state::edge_count spaceReleases{};
    std::string text{};
};

inline seen_input see(const tca::engine::input::input_view input) {
    using tca::engine::input::key;

    const auto keyboard{input.keyboard()};
    return seen_input{
        .spaceDown = keyboard.is_down(key::space),
        .spacePressed = keyboard.was_pressed(key::space),
        .spaceReleased = keyboard.was_released(key::space),
        .spacePresses = keyboard.press_count(key::space),
        .spaceReleases = keyboard.release_count(key::space),
        .text = std::string{input.text()},
    };
}

} // namespace tests

TEST_CASE("win32_application class tests", "[tca][engine][application][win32][win32_application]") {
    using namespace tca::engine::application;
    using namespace tca::engine::application::win32;

    // This is executed before each test case.
    tests::utilities::drain_wm_quit_messages();

    SECTION("Basic getters") {
        // Applying a theme changes the ambient one, which outlives this test case, so it is put
        // back on the way out.
        const tca::engine::theming::scoped_active_theme themeIsolation{
            tca::engine::theming::get_active_theme()};

        std::vector<tests::order_of_calls> callsOrder{};
        GIVEN("A win32_application instance") {
            tests::win32_windowed_application_tss app{callsOrder};

            WHEN("Setting a theme whose window background role is a known color") {
                constexpr tca::engine::rendering::color BACKGROUND_COLOR{
                    tca::engine::rendering::colors::CYAN};
                REQUIRE_NOTHROW(app.set_theme(tca::engine::theming::theme{}.set(
                    tca::engine::theming::color_role::window_background, BACKGROUND_COLOR)));

                THEN("The background color should come from that role") {
                    REQUIRE(app.get_background_color_pub() == BACKGROUND_COLOR);
                }
            }

            WHEN("Setting a theme through the application") {
                REQUIRE_NOTHROW(app.set_theme(tca::engine::theming::theme{}));

                THEN("Whatever holds resources derived from the theme is told about it") {
                    REQUIRE(app.get_theme_changed_call_count() == 1);
                }
            }

            WHEN("Setting a theme through the controller the game logic is handed") {
                using tca::engine::application::test_doubles::application_logic_fake;

                tca::engine::application::application_controller* controller{};
                app.set_logic(std::make_unique<application_logic_fake>(
                    []() {
                        return false;
                    },
                    []() {},
                    [&controller](tca::engine::application::application_controller& injected) {
                        controller = &injected;
                    }));
                REQUIRE(controller != nullptr);

                constexpr tca::engine::rendering::color BACKGROUND_COLOR{
                    tca::engine::rendering::colors::MAGENTA};
                controller->use_theme(tca::engine::theming::theme{}.set(
                    tca::engine::theming::color_role::window_background, BACKGROUND_COLOR));

                THEN(
                    "It takes the same path as set_theme(), rather than a shorter one of its own") {
                    // The controller must not stop at the ambient theme: the application's own
                    // stored theme, and everything deriving from it, has to move with it too.
                    REQUIRE(app.get_theme_changed_call_count() == 1);
                    REQUIRE(app.get_background_color_pub() == BACKGROUND_COLOR);
                    REQUIRE(tca::engine::theming::color_for(
                                tca::engine::theming::color_role::window_background) ==
                            BACKGROUND_COLOR);
                }
            }

            WHEN("Setting the application name") {
                const std::string_view appName{"Test application"};
                REQUIRE_NOTHROW(app.set_app_name(appName));

                THEN("The application name should be set correctly") {
                    REQUIRE(app.get_app_name_pub() == TEXT("Test application"));
                }
            }

            WHEN("Setting the window display settings") {
                constexpr window_display_settings NEW_SETTINGS{
                    .content_size = {.width = 800, .height = 600},
                    .resizable = false,
                    .maximizable = false,
                };
                REQUIRE_NOTHROW(
                    app.set_window_display_settings(window_display_settings{NEW_SETTINGS}));

                THEN("The window display settings should be set correctly") {
                    const window_display_settings& settings{app.get_window_display_settings_pub()};
                    REQUIRE(settings.content_size.width == NEW_SETTINGS.content_size.width);
                    REQUIRE(settings.content_size.height == NEW_SETTINGS.content_size.height);
                    REQUIRE(settings.resizable == NEW_SETTINGS.resizable);
                    REQUIRE(settings.maximizable == NEW_SETTINGS.maximizable);
                }
            }

            THEN(
                "The window display settings default to a resizable, maximizable 1280x720 window") {
                const window_display_settings& settings{app.get_window_display_settings_pub()};
                REQUIRE(settings.content_size.width == 1280);
                REQUIRE(settings.content_size.height == 720);
                REQUIRE(settings.resizable);
                REQUIRE(settings.maximizable);
            }

            WHEN("Setting an application logic") {
                auto logic{std::make_unique<test_doubles::application_logic_fake>()};
                const auto* const logicPtr{logic.get()};
                REQUIRE_NOTHROW(app.set_logic(std::move(logic)));

                THEN("The application logic should be set correctly") {
                    REQUIRE(&app.get_logic_pub() == logicPtr);
                }
            }

            WHEN("Computing window creation params for a fixed content size") {
                constexpr window_display_settings NEW_SETTINGS{
                    .content_size = {.width = 640, .height = 480},
                    .resizable = false,
                    .maximizable = false,
                };
                app.set_window_display_settings(window_display_settings{NEW_SETTINGS});

                const tca::engine::platform::win32::partial_window_creation_params creationParams{
                    app.make_window_creation_params_pub()};

                THEN(
                    "The computed outer size is larger than the requested content size, to make "
                    "room for the OS-drawn title bar/borders") {
                    REQUIRE(creationParams.size.width >= NEW_SETTINGS.content_size.width);
                    REQUIRE(creationParams.size.height > NEW_SETTINGS.content_size.height);
                }

                AND_WHEN("A real window is created with these params") {
                    tests::utilities::drain_wm_quit_messages();

                    tca::engine::platform::win32::win32_application_window testWindow{
                        TEXT("Test Window"),
                        {},
                        tca::engine::platform::win32::partial_window_creation_params{
                                         .style = creationParams.style,
                                         .size = creationParams.size,
                                         .use_pure_raii = true,
                                         }
                    };
                    const HWND hwnd{testWindow.get_native_handle().get()};
                    REQUIRE(hwnd != nullptr);

                    THEN(
                        "Its client (content) area matches the originally requested content size") {
                        RECT clientRect{};
                        REQUIRE(::GetClientRect(hwnd, &clientRect) != 0);
                        REQUIRE(clientRect.right - clientRect.left ==
                                NEW_SETTINGS.content_size.width);
                        REQUIRE(clientRect.bottom - clientRect.top ==
                                NEW_SETTINGS.content_size.height);
                    }
                }
            }
        }
    }

    SECTION("No builder pattern configuration") {
        GIVEN("A win32_application instance") {
            win32_application app{};

            WHEN("run() is called without setting any application logic") {
                THEN("run() should throw a std::runtime_error") {
                    REQUIRE_THROWS_AS(app.run(), std::runtime_error);
                }
            }
        }
    }

    SECTION("initialize() throws partway through") {
        GIVEN(
            "A win32_application instance whose initialize() throws after the base "
            "initialization has already run") {
            tests::win32_console_application_throwing_init_tss app{};
            app.set_logic(std::make_unique<test_doubles::application_logic_fake>());

            WHEN("run() is called") {
                REQUIRE_THROWS_AS(app.run(), std::runtime_error);

                THEN(
                    "close() should still be called, to clean up any partially-initialized "
                    "state, before the failure propagates") {
                    REQUIRE(app.is_close_called());
                }
            }
        }
    }

    SECTION("Builder pattern configuration - No window configuration") {
        std::vector<tests::order_of_calls> callsOrder{};
        GIVEN("A win32_application instance") {
            tests::win32_console_application_tss app{callsOrder};

            WHEN("A valid logic is called with tick() returning false") {
                // Counts are captured from within on_close() - the last point the logic object
                // is guaranteed to still be alive, since run() now destroys it right after
                // on_close() returns (see win32_application::run()) - rather than read through a
                // raw pointer after run() returns, which would dangle.
                std::size_t finalTickCallCount{};
                std::size_t finalOnCloseCallCount{};
                std::size_t finalSetImageFactoryCallCount{};
                test_doubles::application_logic_fake* logicPtr{};
                auto logic{std::make_unique<test_doubles::application_logic_fake>(
                    std::function<bool()>{}, [&]() {
                        finalTickCallCount = logicPtr->get_tick_call_count();
                        finalOnCloseCallCount = logicPtr->get_on_close_call_count();
                        finalSetImageFactoryCallCount =
                            logicPtr->get_set_image_factory_call_count();
                    })};
                logicPtr = logic.get();

                app.set_logic(std::move(logic));

                AND_WHEN("run() is called") {
                    REQUIRE_NOTHROW(app.run());

                    THEN("initialize() should be called") {
                        REQUIRE(app.is_initialize_called());
                    }

                    THEN("The app name should have a default value") {
                        REQUIRE_FALSE(app.get_app_name().empty());
                    }

                    THEN("The tick() method of the logic should be called exactly once") {
                        REQUIRE(finalTickCallCount == 1);
                    }

                    THEN("The on_close() method of the logic should be called exactly once") {
                        REQUIRE(finalOnCloseCallCount == 1);
                    }

                    THEN("The image factory should be injected into the logic exactly once") {
                        REQUIRE(finalSetImageFactoryCallCount == 1);
                    }

                    THEN("close() should be called") {
                        REQUIRE(app.is_close_called());
                    }

                    THEN("The calls order should be correct") {
                        tests::test_correct_calls_order(callsOrder);
                    }
                }
            }

            WHEN("A valid logic is called with tick() returning true") {
                std::size_t tickCallCount{1};
                std::size_t finalTickCallCount{};
                std::size_t finalOnCloseCallCount{};
                std::size_t finalSetImageFactoryCallCount{};
                test_doubles::application_logic_fake* logicPtr{};
                auto logic{std::make_unique<test_doubles::application_logic_fake>(
                    [&tickCallCount, &callsOrder]() {
                        if (callsOrder.back() != tests::order_of_calls::logic_tick) {
                            callsOrder.push_back(tests::order_of_calls::logic_tick);
                        }
                        return tickCallCount++ < 5;
                    },
                    [&]() {
                        finalTickCallCount = logicPtr->get_tick_call_count();
                        finalOnCloseCallCount = logicPtr->get_on_close_call_count();
                        finalSetImageFactoryCallCount =
                            logicPtr->get_set_image_factory_call_count();
                    })};
                logicPtr = logic.get();

                app.set_logic(std::move(logic));

                AND_WHEN("run() is called") {
                    REQUIRE_NOTHROW(app.run());

                    THEN("initialize() should be called") {
                        REQUIRE(app.is_initialize_called());
                    }

                    THEN("The app name should have a default value") {
                        REQUIRE_FALSE(app.get_app_name().empty());
                    }

                    THEN("The tick() method of the logic should be called exactly 5 times") {
                        REQUIRE(finalTickCallCount == 5);
                    }

                    THEN("The on_close() method of the logic should be called exactly once") {
                        REQUIRE(finalOnCloseCallCount == 1);
                    }

                    THEN("The image factory should be injected into the logic exactly once") {
                        REQUIRE(finalSetImageFactoryCallCount == 1);
                    }

                    THEN("close() should be called") {
                        REQUIRE(app.is_close_called());
                    }

                    THEN("The calls order should be correct") {
                        tests::test_correct_calls_order(callsOrder);
                    }
                }
            }

            WHEN("A valid logic is set which captures the injected image factory") {
                tca::engine::rendering::image_factory* injectedFactory{};
                bool wasFactorySetBeforeFirstTick{};
                auto logic{std::make_unique<test_doubles::application_logic_fake>(
                    [&injectedFactory, &wasFactorySetBeforeFirstTick]() -> bool {
                        if (!wasFactorySetBeforeFirstTick) {
                            wasFactorySetBeforeFirstTick = injectedFactory != nullptr;
                        }
                        return false;
                    },
                    std::function<void()>{}, std::function<void(application_controller&)>{},
                    [&injectedFactory](tca::engine::rendering::image_factory& factory) {
                        injectedFactory = &factory;
                    })};

                app.set_logic(std::move(logic));

                AND_WHEN("run() is called") {
                    REQUIRE_NOTHROW(app.run());

                    THEN("The image factory should be injected into the logic") {
                        REQUIRE(injectedFactory != nullptr);
                    }

                    THEN("The image factory should be set before the first tick() call") {
                        REQUIRE(wasFactorySetBeforeFirstTick);
                    }
                }
            }

            WHEN("A valid logic is set which requests a manual exit") {
                application_controller* controller{};
                std::size_t tickCallCount{};
                std::size_t finalOnCloseCallCount{};
                std::size_t finalTickCallCount{};
                test_doubles::application_logic_fake* logicPtr{};
                auto logic{std::make_unique<test_doubles::application_logic_fake>(
                    [&controller, &tickCallCount]() -> bool {
                        tickCallCount++;
                        if (tickCallCount == 3) {
                            REQUIRE_NOTHROW(controller->exit());
                        }
                        // Just a guard if we are still ticking after the exit request, which
                        // shouldn't happen.
                        if (tickCallCount > 3) {
                            FAIL(
                                "tick() should not be called after the application has been "
                                "requested to exit.");
                        }
                        return true;
                    },
                    [&]() {
                        finalOnCloseCallCount = logicPtr->get_on_close_call_count();
                        finalTickCallCount = logicPtr->get_tick_call_count();
                    },
                    [&controller](application_controller& appController) {
                        controller = &appController;
                    })};
                logicPtr = logic.get();

                REQUIRE_NOTHROW(app.set_logic(std::move(logic)));
                REQUIRE_NOTHROW(app.run());

                THEN("on_close() should be called") {
                    REQUIRE(finalOnCloseCallCount == 1);
                }

                THEN("The tick() method of the logic should be called 3 times") {
                    REQUIRE(finalTickCallCount == 3);
                }
            }
        }
    }

    SECTION("Builder pattern configuration - Window configuration") {
        std::vector<tests::order_of_calls> callsOrder{};
        GIVEN("A win32_application instance with a window") {
            tests::win32_windowed_application_tss app{callsOrder};
            app.set_app_name("Test window");

            WHEN("A valid logic is called with tick() - Closing the first time it is called") {
                bool isWMCloseSent{};
                std::size_t finalTickCallCount{};
                std::size_t finalOnCloseCallCount{};
                std::size_t finalSetImageFactoryCallCount{};
                test_doubles::application_logic_fake* logicPtr{};
                auto logic{std::make_unique<test_doubles::application_logic_fake>(
                    [&app, &isWMCloseSent, &callsOrder]() {
                        if (callsOrder.back() != tests::order_of_calls::logic_tick) {
                            callsOrder.push_back(tests::order_of_calls::logic_tick);
                        }
                        // Simulate the user closing the application window by sending a close
                        // message to the window.
                        if (!isWMCloseSent) {
                            // We do this only the first time the tick() is called because
                            // Windows can send many messages in between the WM_CLOSE and WM_QUIT
                            // messages, even internal messages not documented by Microsoft.
                            ::SendMessage(app.get_native_handle().get(), WM_CLOSE, 0, 0);
                            isWMCloseSent = true;
                        }
                        return true;
                    },
                    [&]() {
                        if (callsOrder.back() != tests::order_of_calls::logic_on_close) {
                            callsOrder.push_back(tests::order_of_calls::logic_on_close);
                        }
                        finalTickCallCount = logicPtr->get_tick_call_count();
                        finalOnCloseCallCount = logicPtr->get_on_close_call_count();
                        finalSetImageFactoryCallCount =
                            logicPtr->get_set_image_factory_call_count();
                    })};
                logicPtr = logic.get();

                app.set_logic(std::move(logic));

                AND_WHEN("run() is called") {
                    REQUIRE_NOTHROW(app.run());

                    THEN("initialize() should be called") {
                        REQUIRE(app.is_initialize_called());
                    }

                    THEN("The tick() method of the logic should be called at least once") {
                        REQUIRE(finalTickCallCount >= 1);
                    }

                    THEN("The on_close() method of the logic should be called exactly once") {
                        REQUIRE(finalOnCloseCallCount == 1);
                    }

                    THEN("The image factory should be injected into the logic exactly once") {
                        REQUIRE(finalSetImageFactoryCallCount == 1);
                    }

                    THEN("close() should be called") {
                        REQUIRE(app.is_close_called());
                    }

                    THEN("The calls order should be correct") {
                        tests::test_correct_calls_order(callsOrder);
                    }
                }
            }

            WHEN("A valid logic is called with tick() - Closing the window after a while") {
                std::size_t tickCallCount{1};
                std::size_t finalTickCallCount{};
                std::size_t finalOnCloseCallCount{};
                std::size_t finalSetImageFactoryCallCount{};
                test_doubles::application_logic_fake* logicPtr{};
                auto logic{std::make_unique<test_doubles::application_logic_fake>(
                    [&app, &tickCallCount, &callsOrder]() {
                        if (callsOrder.back() != tests::order_of_calls::logic_tick) {
                            callsOrder.push_back(tests::order_of_calls::logic_tick);
                        }

                        if (tickCallCount++ == 20) {
                            // Simulate the user closing the application window by sending a close
                            // message to the window.
                            ::SendMessage(app.get_native_handle().get(), WM_CLOSE, 0, 0);
                        }

                        if (tickCallCount == 10) {
                            // While we are at it, we also check that the window title is correct.
                            tca::engine::platform::platform_preferred_string_t expectedTitle(
                                sizeof(TEXT("Test window")) /
                                    sizeof(tca::engine::platform::platform_preferred_char_t),
                                0);
                            ::GetWindowText(app.get_native_handle().get(), expectedTitle.data(),
                                            static_cast<std::int32_t>(expectedTitle.size()));
                            // Trim any null terminator from the end of the string.
                            expectedTitle.erase(
                                std::find(std::begin(expectedTitle), std::end(expectedTitle), 0),
                                std::end(expectedTitle));
                            app.set_window_title(expectedTitle);
                        }
                        return true;
                    },
                    [&]() {
                        if (callsOrder.back() != tests::order_of_calls::logic_on_close) {
                            callsOrder.push_back(tests::order_of_calls::logic_on_close);
                        }
                        finalTickCallCount = logicPtr->get_tick_call_count();
                        finalOnCloseCallCount = logicPtr->get_on_close_call_count();
                        finalSetImageFactoryCallCount =
                            logicPtr->get_set_image_factory_call_count();
                    })};
                logicPtr = logic.get();

                app.set_logic(std::move(logic));

                AND_WHEN("run() is called") {
                    REQUIRE_NOTHROW(app.run());

                    THEN("initialize() should be called") {
                        REQUIRE(app.is_initialize_called());
                    }

                    THEN("The tick() method of the logic should be called at least 20 times") {
                        REQUIRE(finalTickCallCount >= 20);
                    }

                    THEN("The on_close() method of the logic should be called exactly once") {
                        REQUIRE(finalOnCloseCallCount == 1);
                    }

                    THEN("The window title should be correct") {
                        REQUIRE(app.get_window_title() == TEXT("Test window"));
                    }

                    THEN("The image factory should be injected into the logic exactly once") {
                        REQUIRE(finalSetImageFactoryCallCount == 1);
                    }

                    THEN("close() should be called") {
                        REQUIRE(app.is_close_called());
                    }

                    THEN("The calls order should be correct") {
                        tests::test_correct_calls_order(callsOrder);
                    }
                }
            }

            WHEN("Trying to exit the application from the application controller") {
                application_controller* controller{};
                std::int32_t tickCallCount{10};
                auto logic{std::make_unique<test_doubles::application_logic_fake>(
                    [&controller, &tickCallCount]() -> bool {
                        tickCallCount--;
                        if (tickCallCount == 0) {
                            REQUIRE_NOTHROW(controller->exit());
                        }
                        return true;
                    },
                    std::function<void()>{},
                    [&controller](application_controller& ctrl) {
                        controller = &ctrl;
                    })};

                app.set_logic(std::move(logic));

                AND_WHEN("run() is called") {
                    REQUIRE_NOTHROW(app.run());

                    THEN("The application controller should be set in the logic") {
                        REQUIRE(controller != nullptr);
                    }

                    THEN("close() should be called") {
                        REQUIRE(app.is_close_called());
                    }
                }
            }
        }
    }
}

TEST_CASE("win32_application input",
          "[tca][engine][application][win32][win32_application][input]") {
    using namespace tca::engine;
    using namespace tca::engine::application;

    // This is executed before each test case.
    tests::utilities::drain_wm_quit_messages();

    std::vector<tests::order_of_calls> callsOrder{};

    SECTION("Without a window") {
        tests::win32_console_application_tss app{callsOrder};

        std::optional<input::input_view> injectedInput{};
        bool wasInputSetBeforeFirstTick{};
        std::size_t finalSetInputCallCount{};
        std::vector<tests::seen_input> seen{};
        test_doubles::application_logic_fake* logicPtr{};

        auto logic{std::make_unique<test_doubles::application_logic_fake>(
            [&]() -> bool {
                if (seen.empty()) {
                    wasInputSetBeforeFirstTick = injectedInput.has_value();
                }
                if (!injectedInput) {
                    return false;
                }

                // Without a window there is no message pump, so the test writes through the
                // collector while the tick runs: the tick sees it in the same frame.
                auto& collector{app.get_input_collector_pub()};
                switch (seen.size()) {
                    case 0:
                        collector.on_key_down(input::key::space, false);
                        collector.on_char(U'a');
                        break;
                    case 2:
                        collector.on_key_up(input::key::space);
                        break;
                    default:
                        break;
                }

                seen.push_back(tests::see(*injectedInput));
                return seen.size() < 3;
            },
            [&]() {
                finalSetInputCallCount = logicPtr->get_set_input_call_count();
            },
            std::function<void(application_controller&)>{},
            std::function<void(rendering::image_factory&)>{},
            [&injectedInput](const input::input_view input) {
                injectedInput = input;
            })};
        logicPtr = logic.get();

        app.set_logic(std::move(logic));
        REQUIRE_NOTHROW(app.run());

        THEN("The input is set exactly once, before the first tick()") {
            REQUIRE(finalSetInputCallCount == 1);
            REQUIRE(wasInputSetBeforeFirstTick);
        }

        THEN("The logic views the frame the collector writes into") {
            REQUIRE(seen.size() == 3);
            REQUIRE(seen[0].spaceDown);
            REQUIRE(seen[0].spacePressed);
            REQUIRE(seen[0].text == "a");
        }

        THEN("The tick is ended after every tick(): edges and text are gone, held keys stay") {
            REQUIRE(seen.size() == 3);
            REQUIRE(seen[1].spaceDown);
            REQUIRE_FALSE(seen[1].spacePressed);
            REQUIRE(seen[1].text.empty());
        }

        THEN("A release in a later tick is reported in that tick") {
            REQUIRE(seen.size() == 3);
            REQUIRE_FALSE(seen[2].spaceDown);
            REQUIRE(seen[2].spaceReleased);
            REQUIRE(seen[2].spaceReleases == 1);
            REQUIRE_FALSE(seen[2].spacePressed);
        }
    }

    SECTION("With a window") {
        tests::win32_windowed_application_tss app{callsOrder};
        app.set_app_name("Test window");

        std::optional<input::input_view> injectedInput{};
        bool wasInputSetBeforeFirstTick{};
        bool isWMCloseSent{};
        std::size_t finalSetInputCallCount{};
        std::size_t preLogicTickCallCount{};
        std::vector<tests::seen_input> seen{};
        test_doubles::application_logic_fake* logicPtr{};

        // Frames 1 and 2 are skipped by pre_logic_tick(), with input arriving in both: their
        // edges must add up and reach the first tick() that runs (frame 3).
        app.set_pre_logic_tick_callback([&app, &preLogicTickCallCount]() -> bool {
            auto& collector{app.get_input_collector_pub()};
            switch (++preLogicTickCallCount) {
                case 1:
                    collector.on_key_down(input::key::space, false);
                    collector.on_char(U'a');
                    return false;
                case 2:
                    collector.on_key_up(input::key::space);
                    collector.on_key_down(input::key::space, false);
                    collector.on_char(U'b');
                    return false;
                default:
                    return true;
            }
        });

        std::size_t tickCallCount{};
        auto logic{std::make_unique<test_doubles::application_logic_fake>(
            [&]() -> bool {
                if (++tickCallCount == 1) {
                    wasInputSetBeforeFirstTick = injectedInput.has_value();
                }
                if (injectedInput) {
                    seen.push_back(tests::see(*injectedInput));
                }

                // Close once two ticks have run, whether or not the input was set, so a missing
                // set_input() fails the checks below instead of hanging the loop. Only once:
                // Windows may deliver more messages, and run more ticks, between WM_CLOSE and
                // WM_QUIT.
                if (tickCallCount >= 2 && !isWMCloseSent) {
                    ::SendMessage(app.get_native_handle().get(), WM_CLOSE, 0, 0);
                    isWMCloseSent = true;
                }
                return true;
            },
            [&]() {
                finalSetInputCallCount = logicPtr->get_set_input_call_count();
            },
            std::function<void(application_controller&)>{},
            std::function<void(rendering::image_factory&)>{},
            [&injectedInput](const input::input_view input) {
                injectedInput = input;
            })};
        logicPtr = logic.get();

        app.set_logic(std::move(logic));
        REQUIRE_NOTHROW(app.run());

        THEN("The input is set exactly once, before the first tick()") {
            REQUIRE(finalSetInputCallCount == 1);
            REQUIRE(wasInputSetBeforeFirstTick);
        }

        THEN("The first tick() that runs gets the edges and text of the frames skipped before it") {
            REQUIRE(seen.size() >= 2);
            REQUIRE(seen[0].spaceDown);
            REQUIRE(seen[0].spacePresses == 2);
            REQUIRE(seen[0].spaceReleases == 1);
            REQUIRE(seen[0].text == "ab");
        }

        THEN("The tick is ended after the tick() that ran: the next one keeps only the held key") {
            REQUIRE(seen.size() >= 2);
            REQUIRE(seen[1].spaceDown);
            REQUIRE_FALSE(seen[1].spacePressed);
            REQUIRE_FALSE(seen[1].spaceReleased);
            REQUIRE(seen[1].text.empty());
        }
    }

    SECTION("With a window, its keyboard messages reach the logic") {
        tests::win32_windowed_application_tss app{callsOrder};
        app.set_app_name("Test window");

        std::optional<input::input_view> injectedInput{};
        bool isWMCloseSent{};
        std::vector<tests::seen_input> seen{};

        // Each tick posts what the next one must see: posted messages are pumped between ticks,
        // the way real key presses arrive.
        auto logic{std::make_unique<test_doubles::application_logic_fake>(
            [&]() -> bool {
                if (injectedInput) {
                    seen.push_back(tests::see(*injectedInput));
                }

                const HWND hwnd{app.get_native_handle().get()};
                switch (seen.size()) {
                    case 1:
                        ::PostMessage(hwnd, WM_KEYDOWN, VK_SPACE,
                                      tests::utilities::key_down_lparam(0x39));
                        ::PostMessage(hwnd, WM_CHAR, L'a', 1);
                        break;
                    case 2:
                        ::PostMessage(hwnd, WM_KEYUP, VK_SPACE,
                                      tests::utilities::key_up_lparam(0x39));
                        break;
                    default:
                        break;
                }

                // Close once three ticks have run, whatever the input did, so a missing feed
                // fails the checks below instead of hanging the loop.
                if (seen.size() >= 3 && !isWMCloseSent) {
                    ::SendMessage(hwnd, WM_CLOSE, 0, 0);
                    isWMCloseSent = true;
                }
                return true;
            },
            std::function<void()>{}, std::function<void(application_controller&)>{},
            std::function<void(rendering::image_factory&)>{},
            [&injectedInput](const input::input_view input) {
                injectedInput = input;
            })};

        app.set_logic(std::move(logic));
        REQUIRE_NOTHROW(app.run());
        REQUIRE(seen.size() >= 3);

        THEN("Nothing was pressed before the first message") {
            REQUIRE_FALSE(seen[0].spacePressed);
        }

        THEN("The key-down and the character reach the next tick") {
            // Not checking the level: if the window lost focus in between, the key was released.
            REQUIRE(seen[1].spacePressed);
            REQUIRE(seen[1].spacePresses == 1);
            // TranslateMessage() may add the space's own WM_CHAR.
            REQUIRE(seen[1].text.find('a') != std::string::npos);
        }

        THEN("The key-up reaches the tick after") {
            REQUIRE_FALSE(seen[2].spaceDown);
            REQUIRE_FALSE(seen[2].spacePressed);
        }
    }
}
