// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/linux/linux-application-controller.hpp"

// C++ STL
#include <array>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

// Third-party
#include <GLFW/glfw3.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>

// Engine
#include "application/application.hpp"
#include "platform/linux/linux-application-window.hpp"
#include "rendering/colors.hpp"
#include "rendering/theming/theme.hpp"
#include "testing/linux/windowing-utilities.hpp"

// These tests mirror tests/application/win32/win32-application-controller.tests.cpp, so that both
// controllers are held to the same behaviour. They run on the GLFW null platform, which models a
// single 1920x1080 monitor whose work area starts 10 units down, and a 1/10/1/1 frame around
// decorated windows. Two things it cannot show:
// - a secondary monitor, so the Win32 "[multi-monitor]" scenario has no counterpart here;
// - the resize GLFW reports when a window goes fullscreen, which the null platform skips.
// Expected rectangles are computed from what GLFW reports rather than hard-coded.

namespace {

namespace eplxn = tca::engine::platform::posix_linux;
namespace theming = tca::engine::theming;
using tca::engine::application::application_controller;
using tca::engine::application::fullscreen_resize;
using tca::engine::application::window_content_size;
using tca::engine::application::window_display_settings;
using tca::engine::application::window_placement;
using tca::engine::application::posix_linux::linux_application_controller;
using tca::engine::testing::apply_headless_glfw_window_hints;
using tca::engine::testing::scoped_null_glfw_context;

struct screen_rect final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};

    bool operator==(const screen_rect&) const noexcept = default;

    [[nodiscard]] std::int32_t right() const noexcept {
        return x + width;
    }

    [[nodiscard]] std::int32_t bottom() const noexcept {
        return y + height;
    }
};

struct screen_point final {
    std::int32_t x{};
    std::int32_t y{};
};

/**
 * @brief A linux_application_controller wired the way a Linux application would wire it, to a
 * window the test opens (or none), to display settings and to a theme setter that records what it
 * is given.
 */
class controller_harness final {
public:
    explicit controller_harness(
        window_display_settings settings = {.content_size = {.width = 400, .height = 300}})
        : m_settings{settings},
          m_controller{[this]() -> GLFWwindow* {
                           return m_window ? m_window->get_native_handle().get() : nullptr;
                       },
                       [this]() -> const window_display_settings& {
                           return m_settings;
                       },
                       [this](theming::theme newTheme) {
                           ++m_themeSetterCalls;
                           m_appliedTheme = std::move(newTheme);
                       },
                       m_stopToken} {}

    /**
     * @brief Opens the application window from the display settings, as a Linux application would,
     * with its content area's top-left corner at the given screen position.
     */
    void open_window(const screen_point topLeft) {
        apply_headless_glfw_window_hints();
        m_window = std::make_unique<eplxn::linux_application_window>(
            "Controller test window",
            eplxn::window_creation_params{
                .initial_size = {.width = m_settings.content_size.width,
                                 .height = m_settings.content_size.height              },
                .initial_position = eplxn::window_creation_position{.x = topLeft.x,                         .y = topLeft.y},
                .resizable = m_settings.resizable
        });
    }

    /**
     * @brief Opens the application window fullscreen, so it has never been windowed.
     */
    void open_fullscreen_window() {
        apply_headless_glfw_window_hints();
        m_window = std::make_unique<eplxn::linux_application_window>(
            "Controller test window",
            eplxn::window_creation_params{
                .initial_size = {.width = m_settings.content_size.width,
                                 .height = m_settings.content_size.height},
                .resizable = m_settings.resizable,
                .fullscreen = true
        });
    }

    [[nodiscard]] eplxn::linux_application_window& platform_window() const {
        REQUIRE(m_window != nullptr);
        return *m_window;
    }

    [[nodiscard]] GLFWwindow* window() const {
        return platform_window().get_native_handle().get();
    }

    [[nodiscard]] application_controller& controller() noexcept {
        return m_controller;
    }

    [[nodiscard]] const std::stop_token& stop_token() const noexcept {
        return m_stopToken;
    }

    [[nodiscard]] std::int32_t theme_setter_calls() const noexcept {
        return m_themeSetterCalls;
    }

    [[nodiscard]] const std::optional<theming::theme>& applied_theme() const noexcept {
        return m_appliedTheme;
    }

private:
    window_display_settings m_settings{};
    std::unique_ptr<eplxn::linux_application_window> m_window{};
    std::int32_t m_themeSetterCalls{};
    std::optional<theming::theme> m_appliedTheme{};
    std::stop_token m_stopToken{};
    linux_application_controller m_controller;
};

[[nodiscard]] screen_rect get_content_rect(GLFWwindow* const window) {
    screen_rect rect{};
    ::glfwGetWindowPos(window, &rect.x, &rect.y);
    ::glfwGetWindowSize(window, &rect.width, &rect.height);
    return rect;
}

/**
 * @brief The window's outer rectangle, frame included, the counterpart of Win32's GetWindowRect().
 */
[[nodiscard]] screen_rect get_window_rect(GLFWwindow* const window) {
    const screen_rect content{get_content_rect(window)};

    int left{};
    int top{};
    int right{};
    int bottom{};
    ::glfwGetWindowFrameSize(window, &left, &top, &right, &bottom);

    return screen_rect{.x = content.x - left,
                       .y = content.y - top,
                       .width = content.width + left + right,
                       .height = content.height + top + bottom};
}

[[nodiscard]] screen_rect get_primary_work_area() {
    GLFWmonitor* const monitor{::glfwGetPrimaryMonitor()};
    REQUIRE(monitor != nullptr);

    screen_rect rect{};
    ::glfwGetMonitorWorkarea(monitor, &rect.x, &rect.y, &rect.width, &rect.height);
    return rect;
}

[[nodiscard]] screen_rect get_primary_monitor_rect() {
    GLFWmonitor* const monitor{::glfwGetPrimaryMonitor()};
    REQUIRE(monitor != nullptr);

    const GLFWvidmode* const mode{::glfwGetVideoMode(monitor)};
    REQUIRE(mode != nullptr);

    screen_rect rect{.width = mode->width, .height = mode->height};
    ::glfwGetMonitorPos(monitor, &rect.x, &rect.y);
    return rect;
}

[[nodiscard]] screen_point top_left_inside(const screen_rect& workArea) {
    constexpr std::int32_t OFFSET{50};
    return screen_point{.x = workArea.x + OFFSET, .y = workArea.y + OFFSET};
}

/**
 * @brief A point no monitor covers, since it lies past the bottom-right corner of the only one the
 * null platform has.
 */
[[nodiscard]] screen_point off_every_monitor() {
    const screen_rect monitor{get_primary_monitor_rect()};
    return screen_point{.x = monitor.right() + 2000, .y = monitor.bottom() + 2000};
}

void move_window_to(GLFWwindow* const window, const screen_point topLeft) {
    ::glfwSetWindowPos(window, topLeft.x, topLeft.y);
    const screen_rect content{get_content_rect(window)};
    REQUIRE(content.x == topLeft.x);
    REQUIRE(content.y == topLeft.y);
}

void require_content_size(GLFWwindow* const window, const window_content_size expected) {
    const screen_rect content{get_content_rect(window)};
    REQUIRE(content.width == expected.width);
    REQUIRE(content.height == expected.height);
}

void require_inside(const screen_rect& inner, const screen_rect& outer) {
    REQUIRE(inner.x >= outer.x);
    REQUIRE(inner.y >= outer.y);
    REQUIRE(inner.right() <= outer.right());
    REQUIRE(inner.bottom() <= outer.bottom());
}

/**
 * @brief Requires the window's outer rectangle to be centred in the given area. Allows one unit of
 * slack per axis, since an odd leftover cannot be split evenly.
 */
void require_centred_in(GLFWwindow* const window, const screen_rect& area) {
    const screen_rect windowRect{get_window_rect(window)};
    REQUIRE(std::abs((windowRect.x - area.x) - (area.right() - windowRect.right())) <= 1);
    REQUIRE(std::abs((windowRect.y - area.y) - (area.bottom() - windowRect.bottom())) <= 1);
}

/**
 * @brief Requires the window to be fullscreen on the primary monitor, covering the whole of it.
 */
void require_covers_monitor(GLFWwindow* const window) {
    REQUIRE(::glfwGetWindowMonitor(window) == ::glfwGetPrimaryMonitor());
    REQUIRE(get_content_rect(window) == get_primary_monitor_rect());
}

} // namespace

template <>
struct Catch::StringMaker<screen_rect> {
    static std::string convert(const screen_rect& rect) {
        return std::format("{{x = {}, y = {}, {}x{}}}", rect.x, rect.y, rect.width, rect.height);
    }
};

template <>
struct Catch::StringMaker<window_placement> {
    static std::string convert(const window_placement placement) {
        switch (placement) {
            case window_placement::keep_top_left:
                return "keep_top_left";
            case window_placement::center:
                return "center";
        }
        return "unknown";
    }
};

// Every leaf section (THEN) re-runs the setup above it, which here means opening a new window. As
// in the Win32 suite, each scenario drives one window through all of its variations and checks
// them in a single leaf rather than one leaf per check.

TEST_CASE(
    "linux_application_controller: exit()",
    "[tca][engine][application][linux][linux_application_controller][application_controller]") {
    scoped_null_glfw_context glfwContext{};

    SECTION("An application with no window") {
        controller_harness harness{};
        REQUIRE_FALSE(harness.stop_token().stop_requested());

        WHEN("exit() is requested") {
            harness.controller().exit();

            THEN("A stop is requested on the token the controller handed out") {
                REQUIRE(harness.stop_token().stop_requested());
            }
        }
    }

    SECTION("An application with a window") {
        controller_harness harness{};
        harness.open_window(top_left_inside(get_primary_work_area()));
        REQUIRE(::glfwWindowShouldClose(harness.window()) == GLFW_FALSE);

        WHEN("exit() is requested") {
            harness.controller().exit();

            THEN(
                "The window is asked to close, and no stop is requested, since closing the window "
                "is what ends the application") {
                REQUIRE(::glfwWindowShouldClose(harness.window()) == GLFW_TRUE);
                REQUIRE_FALSE(harness.stop_token().stop_requested());
            }
        }
    }
}

TEST_CASE(
    "linux_application_controller: use_theme()",
    "[tca][engine][application][linux][linux_application_controller][application_controller]") {
    scoped_null_glfw_context glfwContext{};
    controller_harness harness{};

    WHEN("A theme is applied through the controller") {
        constexpr tca::engine::rendering::color NEW_COLOR{tca::engine::rendering::colors::RED};
        harness.controller().use_theme(
            theming::theme{}.set(theming::color_role::window_background, NEW_COLOR));

        THEN("It is handed, once, to the application's theme setter") {
            REQUIRE(harness.theme_setter_calls() == 1);
            REQUIRE(harness.applied_theme().has_value());
            REQUIRE(harness.applied_theme()->resolve(theming::color_role::window_background) ==
                    NEW_COLOR);
        }
    }
}

TEST_CASE(
    "linux_application_controller: is_fullscreen()",
    "[tca][engine][application][linux][linux_application_controller][application_controller]") {
    scoped_null_glfw_context glfwContext{};

    SECTION("An application with no window") {
        controller_harness harness{};
        application_controller& controller{harness.controller()};

        THEN("It is not fullscreen, even after go_fullscreen(), since there is no window") {
            REQUIRE_FALSE(controller.is_fullscreen());
            REQUIRE_NOTHROW(controller.go_fullscreen());
            REQUIRE_FALSE(controller.is_fullscreen());
            REQUIRE_NOTHROW(controller.go_windowed());
            REQUIRE_FALSE(controller.is_fullscreen());
        }
    }

    SECTION("An application with a window") {
        controller_harness harness{};
        harness.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{harness.controller()};

        THEN("is_fullscreen() tracks every transition between windowed and fullscreen") {
            REQUIRE_FALSE(controller.is_fullscreen());

            controller.go_windowed();
            REQUIRE_FALSE(controller.is_fullscreen());

            controller.go_fullscreen();
            REQUIRE(controller.is_fullscreen());
            require_covers_monitor(harness.window());

            controller.go_fullscreen();
            REQUIRE(controller.is_fullscreen());

            for (std::int32_t toggle{}; toggle < 3; ++toggle) {
                CAPTURE(toggle);
                controller.go_windowed();
                REQUIRE_FALSE(controller.is_fullscreen());
                controller.go_fullscreen();
                REQUIRE(controller.is_fullscreen());
            }
        }
    }

    SECTION("An application whose window was created fullscreen") {
        controller_harness harness{};
        harness.open_fullscreen_window();

        THEN("It is fullscreen") {
            REQUIRE(harness.controller().is_fullscreen());
        }
    }
}

TEST_CASE(
    "linux_application_controller: go_fullscreen() and go_windowed()",
    "[tca][engine][application][linux][linux_application_controller][application_controller]") {
    scoped_null_glfw_context glfwContext{};

    SECTION("An application with a window") {
        controller_harness harness{};
        harness.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{harness.controller()};
        GLFWwindow* const window{harness.window()};

        // Somewhere that is neither the top-left corner of the screen nor where the window was
        // opened, so that a restore to either of those would show.
        controller.set_content_size({.width = 640, .height = 360},
                                    {.placement = window_placement::center});
        const screen_rect windowedRect{get_window_rect(window)};

        WHEN("go_windowed() is requested while already windowed") {
            controller.go_windowed();

            THEN("The window does not move or change size") {
                REQUIRE_FALSE(controller.is_fullscreen());
                REQUIRE(get_window_rect(window) == windowedRect);
            }
        }

        WHEN("The window goes fullscreen and back, several times") {
            THEN(
                "Each time it covers its monitor, then comes back exactly where and as big as it "
                "was") {
                for (std::int32_t toggle{}; toggle < 3; ++toggle) {
                    CAPTURE(toggle);
                    controller.go_fullscreen();
                    REQUIRE(controller.is_fullscreen());
                    require_covers_monitor(window);

                    controller.go_windowed();
                    REQUIRE_FALSE(controller.is_fullscreen());
                    REQUIRE(::glfwGetWindowMonitor(window) == nullptr);
                    REQUIRE(get_window_rect(window) == windowedRect);
                    require_content_size(window, {.width = 640, .height = 360});
                }
            }
        }

        WHEN("go_fullscreen() is requested twice, then go_windowed()") {
            controller.go_fullscreen();
            controller.go_fullscreen();
            controller.go_windowed();

            THEN(
                "The window comes back where it was before the first request, since the second "
                "one did not overwrite it with the fullscreen rectangle") {
                REQUIRE(get_window_rect(window) == windowedRect);
            }
        }

        WHEN("The window is moved, then goes fullscreen and back") {
            move_window_to(window, screen_point{.x = 300, .y = 200});
            const screen_rect movedRect{get_window_rect(window)};
            controller.go_fullscreen();
            controller.go_windowed();

            THEN("It comes back where it was moved to") {
                REQUIRE(get_window_rect(window) == movedRect);
            }
        }

        WHEN("A resize handler is subscribed, and the window goes fullscreen and back") {
            window_content_size observedSize{};
            harness.platform_window().on_resize().subscribe(
                [&observedSize](const std::int32_t width, const std::int32_t height) {
                    observedSize = {.width = width, .height = height};
                });

            controller.go_fullscreen();
            controller.go_windowed();

            THEN("The handler is last told the restored content size") {
                REQUIRE(observedSize.width == 640);
                REQUIRE(observedSize.height == 360);
            }
        }

        WHEN("The window overlaps no monitor and goes fullscreen") {
            move_window_to(window, off_every_monitor());
            controller.go_fullscreen();

            THEN("It goes fullscreen on the primary monitor") {
                REQUIRE(controller.is_fullscreen());
                require_covers_monitor(window);
            }

            THEN(
                "go_windowed() does not send it back off screen: it gets the configured content "
                "size, centred in the work area") {
                controller.go_windowed();
                REQUIRE_FALSE(controller.is_fullscreen());
                require_content_size(window, {.width = 400, .height = 300});
                require_inside(get_window_rect(window), get_primary_work_area());
                require_centred_in(window, get_primary_work_area());
            }
        }
    }

    SECTION("An application whose window is not resizable") {
        controller_harness harness{
            window_display_settings{
                                    .content_size = {.width = 400, .height = 300},
                                    .resizable = false,
                                    .maximizable = false,
                                    }
        };
        harness.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{harness.controller()};
        GLFWwindow* const window{harness.window()};
        const screen_rect windowedRect{get_window_rect(window)};

        WHEN("The window goes fullscreen and back") {
            controller.go_fullscreen();
            const bool wasFullscreen{controller.is_fullscreen()};
            controller.go_windowed();

            THEN("It covered its monitor, came back where it was, and is still not resizable") {
                REQUIRE(wasFullscreen);
                REQUIRE(get_window_rect(window) == windowedRect);
                REQUIRE(::glfwGetWindowAttrib(window, GLFW_RESIZABLE) == GLFW_FALSE);
            }
        }
    }

    SECTION("An application whose window was created fullscreen") {
        controller_harness harness{
            window_display_settings{.content_size = {.width = 400, .height = 300}}};
        harness.open_fullscreen_window();
        application_controller& controller{harness.controller()};
        GLFWwindow* const window{harness.window()};

        WHEN("go_windowed() is requested") {
            controller.go_windowed();

            THEN(
                "The window, which has no windowed rectangle to go back to, gets the configured "
                "content size, centred in the work area") {
                REQUIRE_FALSE(controller.is_fullscreen());
                require_content_size(window, {.width = 400, .height = 300});
                require_inside(get_window_rect(window), get_primary_work_area());
                require_centred_in(window, get_primary_work_area());
            }
        }

        WHEN("It goes windowed, is moved, then goes fullscreen and back") {
            controller.go_windowed();
            move_window_to(window, screen_point{.x = 120, .y = 80});
            const screen_rect movedRect{get_window_rect(window)};

            controller.go_fullscreen();
            controller.go_windowed();

            THEN("It comes back where it was moved to") {
                REQUIRE(get_window_rect(window) == movedRect);
            }
        }
    }
}

TEST_CASE(
    "linux_application_controller: set_content_size()",
    "[tca][engine][application][linux][linux_application_controller][application_controller]") {
    scoped_null_glfw_context glfwContext{};

    SECTION("An application with no window") {
        controller_harness harness{};
        application_controller& controller{harness.controller()};

        THEN("The request is ignored without failing") {
            REQUIRE_NOTHROW(controller.set_content_size({.width = 640, .height = 360}, {}));
            REQUIRE_NOTHROW(controller.set_content_size({.width = 640, .height = 360},
                                                        {.placement = window_placement::center}));
            REQUIRE_FALSE(controller.is_fullscreen());
        }
    }

    SECTION("An application with a window, in windowed mode") {
        // The content area has to come out at exactly the requested size whether or not the
        // window was created resizable.
        for (const bool resizable : {true, false}) {
            CAPTURE(resizable);

            controller_harness harness{
                window_display_settings{
                                        .content_size = {.width = 400, .height = 300},
                                        .resizable = resizable,
                                        .maximizable = resizable,
                                        }
            };
            harness.open_window(top_left_inside(get_primary_work_area()));
            application_controller& controller{harness.controller()};
            GLFWwindow* const window{harness.window()};

            // Grows, shrinks back, and ends on an odd size the frame cannot hide rounding in.
            constexpr std::array<window_content_size, 5> SIZES{
                window_content_size{.width = 320, .height = 180},
                window_content_size{.width = 640, .height = 360},
                window_content_size{.width = 960, .height = 540},
                window_content_size{.width = 320, .height = 180},
                window_content_size{.width = 401, .height = 299},
            };

            for (const window_placement placement :
                 {window_placement::keep_top_left, window_placement::center}) {
                for (const window_content_size& requestedSize : SIZES) {
                    CAPTURE(placement, requestedSize.width, requestedSize.height);

                    const screen_rect windowRectBefore{get_window_rect(window)};
                    controller.set_content_size(requestedSize, {.placement = placement});

                    const screen_rect windowRectAfter{get_window_rect(window)};
                    const screen_rect workArea{get_primary_work_area()};
                    require_content_size(window, requestedSize);
                    REQUIRE_FALSE(controller.is_fullscreen());
                    require_inside(windowRectAfter, workArea);

                    if (placement == window_placement::center) {
                        require_centred_in(window, workArea);
                    } else if (windowRectBefore.x + windowRectAfter.width <= workArea.right() &&
                               windowRectBefore.y + windowRectAfter.height <= workArea.bottom()) {
                        // The top-left corner only has to stay put when the window still fits
                        // from there; otherwise it is moved back inside the work area.
                        REQUIRE(windowRectAfter.x == windowRectBefore.x);
                        REQUIRE(windowRectAfter.y == windowRectBefore.y);
                    }
                }
            }

            // The window's attributes are left alone, so a fixed-size window stays fixed-size.
            REQUIRE((::glfwGetWindowAttrib(window, GLFW_RESIZABLE) == GLFW_TRUE) == resizable);
        }
    }

    SECTION("An application with a window, in windowed mode, at its edges") {
        controller_harness harness{};
        harness.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{harness.controller()};
        GLFWwindow* const window{harness.window()};

        WHEN("A content size larger than the work area is requested") {
            THEN(
                "The window exactly fills the work area, with a smaller but non-empty content "
                "area") {
                const screen_rect workArea{get_primary_work_area()};
                const window_content_size hugeSize{.width = workArea.width * 3,
                                                   .height = workArea.height * 3};

                for (const window_placement placement :
                     {window_placement::keep_top_left, window_placement::center}) {
                    CAPTURE(placement);
                    controller.set_content_size(hugeSize, {.placement = placement});

                    REQUIRE(get_window_rect(window) == workArea);

                    const screen_rect content{get_content_rect(window)};
                    REQUIRE(content.width > 0);
                    REQUIRE(content.height > 0);
                    REQUIRE(content.width < hugeSize.width);
                    REQUIRE(content.height < hugeSize.height);
                }
            }
        }

        WHEN("Only the width is larger than the work area") {
            controller.set_content_size({.width = 10000, .height = 400}, {});

            THEN("Only the width is clamped") {
                const screen_rect workArea{get_primary_work_area()};
                const screen_rect windowRect{get_window_rect(window)};
                require_inside(windowRect, workArea);
                REQUIRE(windowRect.width == workArea.width);
                REQUIRE(get_content_rect(window).height == 400);
            }
        }

        WHEN("Empty content sizes are requested") {
            THEN("Each request is ignored and the window keeps its rectangle") {
                const screen_rect before{get_window_rect(window)};

                for (const window_content_size emptySize : {
                         window_content_size{.width = 0,   .height = 0  },
                         window_content_size{.width = 0,   .height = 300},
                         window_content_size{.width = 400, .height = -1 },
                         window_content_size{.width = -5,  .height = -5 }
                }) {
                    CAPTURE(emptySize.width, emptySize.height);
                    controller.set_content_size(emptySize, {.placement = window_placement::center});
                    REQUIRE(get_window_rect(window) == before);
                }
            }
        }

        WHEN("The window sits near the bottom-right of the work area and is made bigger") {
            const screen_rect workArea{get_primary_work_area()};
            const screen_rect content{get_content_rect(window)};
            move_window_to(window, screen_point{.x = workArea.right() - content.width - 1,
                                                .y = workArea.bottom() - content.height - 1});

            constexpr window_content_size BIGGER_SIZE{.width = 640, .height = 480};
            controller.set_content_size(BIGGER_SIZE, {});

            THEN("It gets exactly the requested size and is moved back inside the work area") {
                require_content_size(window, BIGGER_SIZE);
                require_inside(get_window_rect(window), workArea);
            }
        }

        WHEN("The window sits above and left of the work area and is resized") {
            const screen_rect workArea{get_primary_work_area()};
            move_window_to(window, screen_point{.x = workArea.x - 200, .y = workArea.y - 100});

            constexpr window_content_size REQUESTED_SIZE{.width = 640, .height = 480};
            controller.set_content_size(REQUESTED_SIZE, {});

            THEN("It gets exactly the requested size and is moved back inside the work area") {
                require_content_size(window, REQUESTED_SIZE);
                require_inside(get_window_rect(window), workArea);
            }
        }
    }

    SECTION("An application with a window, in fullscreen mode") {
        controller_harness harness{
            window_display_settings{.content_size = {.width = 400, .height = 300}}};
        harness.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{harness.controller()};
        GLFWwindow* const window{harness.window()};

        const screen_rect contentBeforeFullscreen{get_content_rect(window)};

        controller.go_fullscreen();
        REQUIRE(controller.is_fullscreen());

        constexpr window_content_size REQUESTED_SIZE{.width = 640, .height = 360};

        WHEN("New content sizes are requested with fullscreen_resize::ignore, or the defaults") {
            controller.set_content_size(REQUESTED_SIZE,
                                        {.placement = window_placement::keep_top_left,
                                         .fullscreen = fullscreen_resize::ignore});
            controller.set_content_size(REQUESTED_SIZE, {.placement = window_placement::center,
                                                         .fullscreen = fullscreen_resize::ignore});
            controller.set_content_size(REQUESTED_SIZE, {});

            THEN(
                "The window stays fullscreen, and go_windowed() restores its size from before "
                "going fullscreen") {
                REQUIRE(controller.is_fullscreen());
                require_covers_monitor(window);

                controller.go_windowed();
                REQUIRE_FALSE(controller.is_fullscreen());
                REQUIRE(get_content_rect(window) == contentBeforeFullscreen);
            }
        }

        WHEN("A new content size is requested with fullscreen_resize::defer_until_windowed") {
            controller.set_content_size(REQUESTED_SIZE,
                                        {.fullscreen = fullscreen_resize::defer_until_windowed});

            THEN("The window stays fullscreen, and the size is applied once, by go_windowed()") {
                REQUIRE(controller.is_fullscreen());
                require_covers_monitor(window);

                controller.go_windowed();
                REQUIRE_FALSE(controller.is_fullscreen());
                require_content_size(window, REQUESTED_SIZE);
                require_inside(get_window_rect(window), get_primary_work_area());

                // keep_top_left keeps the top-left corner from before going fullscreen.
                const screen_rect content{get_content_rect(window)};
                REQUIRE(content.x == contentBeforeFullscreen.x);
                REQUIRE(content.y == contentBeforeFullscreen.y);

                // Going fullscreen and back with no new request keeps the size the window had.
                controller.go_fullscreen();
                controller.go_windowed();
                require_content_size(window, REQUESTED_SIZE);
            }
        }

        WHEN("A centred content size is deferred") {
            controller.set_content_size(REQUESTED_SIZE,
                                        {.placement = window_placement::center,
                                         .fullscreen = fullscreen_resize::defer_until_windowed});

            THEN("The window stays fullscreen, and the placement is applied by go_windowed() too") {
                REQUIRE(controller.is_fullscreen());
                require_covers_monitor(window);

                controller.go_windowed();
                require_content_size(window, REQUESTED_SIZE);
                require_centred_in(window, get_primary_work_area());
            }
        }

        WHEN("Several content sizes are deferred, then one is ignored, while fullscreen") {
            controller.set_content_size({.width = 960, .height = 540},
                                        {.fullscreen = fullscreen_resize::defer_until_windowed});
            controller.set_content_size(REQUESTED_SIZE,
                                        {.fullscreen = fullscreen_resize::defer_until_windowed});
            controller.set_content_size({.width = 320, .height = 180},
                                        {.fullscreen = fullscreen_resize::ignore});
            controller.go_windowed();

            THEN("The last deferred request wins, since an ignored request changes nothing") {
                require_content_size(window, REQUESTED_SIZE);
            }
        }

        WHEN("An empty content size is deferred") {
            controller.set_content_size({.width = 0, .height = 0},
                                        {.fullscreen = fullscreen_resize::defer_until_windowed});
            controller.go_windowed();

            THEN(
                "It is ignored like any empty size, so go_windowed() restores the size from "
                "before going fullscreen") {
                REQUIRE(get_content_rect(window) == contentBeforeFullscreen);
            }
        }
    }
}

TEST_CASE(
    "linux_application_controller: center_window()",
    "[tca][engine][application][linux][linux_application_controller][application_controller]") {
    scoped_null_glfw_context glfwContext{};

    SECTION("An application with no window") {
        controller_harness harness{};

        THEN("The request is ignored without failing") {
            REQUIRE_NOTHROW(harness.controller().center_window());
        }
    }

    SECTION("An application with a window, in windowed mode") {
        for (const bool resizable : {true, false}) {
            CAPTURE(resizable);

            controller_harness harness{
                window_display_settings{
                                        .content_size = {.width = 400, .height = 300},
                                        .resizable = resizable,
                                        .maximizable = resizable,
                                        }
            };
            harness.open_window(top_left_inside(get_primary_work_area()));
            application_controller& controller{harness.controller()};
            GLFWwindow* const window{harness.window()};

            // center_window() is requested: the window is centred on its monitor, keeping its
            // size, and does not move when centred again.
            const screen_rect contentBefore{get_content_rect(window)};
            controller.center_window();

            require_centred_in(window, get_primary_work_area());
            REQUIRE_FALSE(controller.is_fullscreen());
            require_content_size(window,
                                 {.width = contentBefore.width, .height = contentBefore.height});

            const screen_rect windowRectBefore{get_window_rect(window)};
            controller.center_window();
            REQUIRE(get_window_rect(window) == windowRectBefore);

            // The window is resized and then centred, like a game applying a window scale: each
            // time it has the requested content size and is centred in the work area.
            for (const window_content_size requestedSize : {
                     window_content_size{.width = 320, .height = 180},
                     window_content_size{.width = 960, .height = 540}
            }) {
                CAPTURE(requestedSize.width, requestedSize.height);
                controller.set_content_size(requestedSize, {});
                controller.center_window();

                require_content_size(window, requestedSize);
                require_centred_in(window, get_primary_work_area());
            }
        }
    }

    SECTION("An application with a window, centred after returning from fullscreen") {
        controller_harness harness{};
        harness.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{harness.controller()};

        controller.go_fullscreen();
        controller.go_windowed();
        controller.center_window();

        THEN("The window is centred in the work area of its monitor") {
            REQUIRE_FALSE(controller.is_fullscreen());
            require_centred_in(harness.window(), get_primary_work_area());
        }
    }

    SECTION("An application with a window that overlaps no monitor") {
        controller_harness harness{};
        harness.open_window(top_left_inside(get_primary_work_area()));
        move_window_to(harness.window(), off_every_monitor());

        harness.controller().center_window();

        THEN("The window is centred in the primary monitor's work area") {
            require_inside(get_window_rect(harness.window()), get_primary_work_area());
            require_centred_in(harness.window(), get_primary_work_area());
        }
    }

    SECTION("An application with a window, in fullscreen mode") {
        controller_harness harness{};
        harness.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{harness.controller()};

        controller.go_fullscreen();
        REQUIRE(controller.is_fullscreen());

        WHEN("center_window() is requested") {
            controller.center_window();

            THEN("The window stays fullscreen, covering its monitor") {
                REQUIRE(controller.is_fullscreen());
                require_covers_monitor(harness.window());
            }
        }
    }
}
