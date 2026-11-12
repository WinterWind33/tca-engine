// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/application.hpp"

// C++ STL
#include <memory>
#include <string_view>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Engine
#include "input/input-frame.hpp"
#include "input/key.hpp"
#include "rendering/colors.hpp"
#include "rendering/theming/active-theme.hpp"

// Test doubles
#include "rendering/theming/theme.hpp"
#include "test-doubles/application/application-logic.test-double.hpp"

namespace {

class application_controller_fake final : public tca::engine::application::application_controller {
public:
    void exit() override {}
    void go_fullscreen() override {}
    void go_windowed() override {}
    void use_theme(tca::engine::theming::theme) override {}
    void set_content_size(tca::engine::application::window_content_size,
                          tca::engine::application::content_size_options) override {}
    bool is_fullscreen() const noexcept override {
        return false;
    }
    void center_window() override {}
};

class application_logic_base_tss final : public tca::engine::application::application_logic_base {
public:
    tca::engine::application::application_controller& get_controller() const {
        return get_application_controller();
    }

    tca::engine::input::input_view get_input_pub() const noexcept {
        return get_input();
    }
};

class application_base_tss final : public tca::engine::application::application_base {
public:
    explicit application_base_tss(
        std::unique_ptr<tca::engine::application::application_controller> appController)
        : m_controller{std::move(appController)} {}

    tca::engine::rendering::color get_background_color_pub() const noexcept {
        return application_base::get_background_color();
    }

    const tca::engine::application::window_display_settings& get_window_display_settings_pub()
        const noexcept {
        return application_base::get_window_display_settings();
    }

    std::string_view get_app_name_pub() const noexcept {
        return application_base::get_app_name();
    }

    std::string_view get_company_name_pub() const noexcept {
        return application_base::get_company_name();
    }

    tca::engine::application::application_logic* get_logic_pub() const noexcept {
        return application_base::get_logic();
    }

    tca::engine::application::application_controller& get_controller_pub() {
        return get_or_create_controller();
    }

private:
    tca::engine::application::application_controller& get_or_create_controller() override {
        return *m_controller;
    }

    std::unique_ptr<tca::engine::application::application_controller> m_controller{};
};

} // namespace

TEST_CASE("application_factory class tests", "[tca][engine][application][application_factory]") {
    using namespace tca::engine::application;

    SECTION("create_platform_preferred_application() method") {
        SECTION("Should create a non-null platform preferred application instance") {
            // Make sure that, even though the rendering only works on Windows, we can still
            // create an application on Unix systems.
            const auto app{application_factory::create_platform_preferred_application()};
            REQUIRE(app != nullptr);
        }
    }
}

TEST_CASE("application_logic_base class tests",
          "[tca][engine][application][application_logic_base]") {
    using namespace tca::engine::application;

    application_controller_fake controller;
    application_logic_base_tss logic;

    SECTION(
        "get_application_controller() method should return the application controller that was set "
        "with set_app_controller()") {
        logic.set_app_controller(controller);
        REQUIRE(&logic.get_controller() == &controller);
    }
}

TEST_CASE("application_logic_base get_input()",
          "[tca][engine][application][application_logic_base][input]") {
    using namespace tca::engine;

    application_logic_base_tss logic{};

    SECTION("Before set_input(), it views an empty frame and doesn't throw") {
        REQUIRE_NOTHROW(logic.get_input_pub());

        const input::input_view input{logic.get_input_pub()};
        REQUIRE_FALSE(input.keyboard().is_down(input::key::space));
        REQUIRE_FALSE(input.keyboard().was_pressed(input::key::space));
        REQUIRE(input.text().empty());

        REQUIRE_NOTHROW(input.consume(input::key::space));
        REQUIRE_NOTHROW(input.consume_all());
    }

    GIVEN("A hand-fed frame with a press and some text") {
        input::input_frame frame{};
        frame.keyboard().press(input::key::space);
        frame.append_text("hi");

        WHEN("A view of it is set on the logic") {
            logic.set_input(input::input_view{frame});

            THEN("get_input() sees the press and the text") {
                const input::input_view input{logic.get_input_pub()};
                REQUIRE(input.keyboard().is_down(input::key::space));
                REQUIRE(input.keyboard().was_pressed(input::key::space));
                REQUIRE(input.keyboard().press_count(input::key::space) == 1);
                REQUIRE(input.text() == "hi");
            }

            THEN("Input written into the frame later is seen too") {
                // The application writes into the same persistent frame every tick.
                frame.keyboard().press(input::key::enter);
                frame.append_text("!");

                REQUIRE(logic.get_input_pub().keyboard().was_pressed(input::key::enter));
                REQUIRE(logic.get_input_pub().text() == "hi!");
            }

            THEN("Ending the tick on the frame is seen too") {
                frame.end_tick();

                const input::input_view input{logic.get_input_pub()};
                REQUIRE(input.keyboard().is_down(input::key::space));
                REQUIRE_FALSE(input.keyboard().was_pressed(input::key::space));
                REQUIRE(input.text().empty());
            }

            THEN("Consuming through get_input() reaches the frame") {
                logic.get_input_pub().consume(input::key::space);

                REQUIRE_FALSE(frame.keyboard().was_pressed(input::key::space));
                REQUIRE_FALSE(logic.get_input_pub().keyboard().was_pressed(input::key::space));
                REQUIRE(frame.text() == "hi");
            }

            THEN("consume_all() through get_input() hides the keys and the text in the frame") {
                logic.get_input_pub().consume_all();

                REQUIRE_FALSE(frame.keyboard().is_down(input::key::space));
                REQUIRE(frame.text().empty());
            }

            AND_WHEN("A view of another frame is set") {
                input::input_frame other{};
                other.keyboard().press(input::key::a);
                logic.set_input(input::input_view{other});

                THEN("get_input() views the other frame only") {
                    const input::input_view input{logic.get_input_pub()};
                    REQUIRE(input.keyboard().was_pressed(input::key::a));
                    REQUIRE_FALSE(input.keyboard().was_pressed(input::key::space));
                    REQUIRE(input.text().empty());
                }
            }
        }
    }
}

TEST_CASE("application_base class tests", "[tca][engine][application][application_base]") {
    using namespace tca::engine;

    auto appController{std::make_unique<application_controller_fake>()};
    application_base_tss app{std::move(appController)};

    SECTION("set_theme() decides the window background color") {
        constexpr rendering::color NEW_COLOR{rendering::colors::RED};
        app.set_theme(theming::theme{}.set(theming::color_role::window_background, NEW_COLOR));

        THEN("The window is cleared to the theme's window background role") {
            REQUIRE(app.get_background_color_pub() == NEW_COLOR);
        }

        THEN("The theme also becomes the one every widget resolves against") {
            REQUIRE(theming::color_for(theming::color_role::window_background) == NEW_COLOR);
        }
    }

    SECTION("get_window_display_settings() method") {
        THEN("It defaults to a resizable, maximizable 1280x720 window") {
            const application::window_display_settings& settings{
                app.get_window_display_settings_pub()};
            REQUIRE(settings.content_size.width == 1280);
            REQUIRE(settings.content_size.height == 720);
            REQUIRE(settings.resizable);
            REQUIRE(settings.maximizable);
        }
    }

    SECTION("set_window_display_settings() and get_window_display_settings() methods") {
        constexpr application::window_display_settings NEW_SETTINGS{
            .content_size = {.width = 800, .height = 600},
            .resizable = false,
            .maximizable = false,
        };
        app.set_window_display_settings(application::window_display_settings{NEW_SETTINGS});

        const application::window_display_settings& settings{app.get_window_display_settings_pub()};
        REQUIRE(settings.content_size.width == NEW_SETTINGS.content_size.width);
        REQUIRE(settings.content_size.height == NEW_SETTINGS.content_size.height);
        REQUIRE(settings.resizable == NEW_SETTINGS.resizable);
        REQUIRE(settings.maximizable == NEW_SETTINGS.maximizable);
    }

    SECTION("set_app_name() and get_app_name() methods") {
        constexpr std::string_view NEW_NAME{"Test Application"};
        app.set_app_name(NEW_NAME);
        REQUIRE(app.get_app_name_pub() == NEW_NAME);
    }

    SECTION("set_company_name() and get_company_name() methods") {
        CHECK(app.get_company_name_pub().empty());

        constexpr std::string_view NEW_NAME{"Test Company"};
        app.set_company_name(NEW_NAME);
        REQUIRE(app.get_company_name_pub() == NEW_NAME);
    }

    SECTION("set_logic() and get_logic() methods") {
        WHEN("Setting a new logic object") {
            auto logic{
                std::make_unique<tca::engine::application::test_doubles::application_logic_fake>()};
            const auto* logicPtr{logic.get()};
            app.set_logic(std::move(logic));

            THEN("get_logic() should return a pointer to the set logic object") {
                REQUIRE(app.get_logic_pub() == logicPtr);
            }

            THEN("The application controller should be injected into the logic object") {
                REQUIRE(logicPtr->get_set_app_controller_call_count() == 1);
            }
        }
    }
}
