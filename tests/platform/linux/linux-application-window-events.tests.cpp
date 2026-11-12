// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/linux/linux-application-window.hpp"

// Third-party
#include <GLFW/glfw3.h>
#include <catch2/catch_test_macros.hpp>

// C++ STL
#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

// Engine
#include "testing/linux/windowing-utilities.hpp"

namespace {

namespace eplxn = tca::engine::platform::posix_linux;
using tca::engine::testing::apply_headless_glfw_window_hints;
using tca::engine::testing::scoped_null_glfw_context;

/**
 * @brief The callback GLFW currently has installed on the window, read through the setter's
 * return value and put straight back. The null platform has no way to inject mouse events or
 * close requests, so tests call the installed callbacks themselves, exactly as GLFW would.
 */
template <typename Callback>
[[nodiscard]] Callback installed_callback(Callback (*const setter)(GLFWwindow*, Callback),
                                          GLFWwindow* const window) {
    const Callback callback{setter(window, nullptr)};
    std::ignore = setter(window, callback);
    return callback;
}

/**
 * @brief What GLFW does when the user clicks the close button: raise the should-close flag, then
 * call the close callback.
 */
void send_close_request(GLFWwindow* const window) {
    const GLFWwindowclosefun callback{installed_callback(&::glfwSetWindowCloseCallback, window)};
    REQUIRE(callback != nullptr);
    ::glfwSetWindowShouldClose(window, GLFW_TRUE);
    callback(window);
}

/**
 * @brief Test-specific subclass recording every native event hook call.
 */
class recording_window final : public eplxn::linux_application_window {
public:
    using linux_application_window::linux_application_window;

    std::vector<std::tuple<int, int, int>> mouse_buttons{};
    std::vector<std::pair<double, double>> cursor_positions{};
    std::vector<std::pair<double, double>> scrolls{};
    std::vector<bool> iconifications{};
    std::vector<std::pair<std::int32_t, std::int32_t>> framebuffer_sizes{};
    std::vector<std::pair<float, float>> content_scales{};

    std::int32_t close_requests{};
    bool allow_close{true};

protected:
    bool on_close_requested() override {
        ++close_requests;
        return allow_close;
    }

    void on_mouse_button(const int button, const int action, const int mods) override {
        mouse_buttons.emplace_back(button, action, mods);
    }

    void on_cursor_moved(const double x, const double y) override {
        cursor_positions.emplace_back(x, y);
    }

    void on_scroll(const double xOffset, const double yOffset) override {
        scrolls.emplace_back(xOffset, yOffset);
    }

    void on_iconify(const bool iconified) override {
        iconifications.push_back(iconified);
    }

    void on_framebuffer_resized(const std::int32_t width, const std::int32_t height) override {
        framebuffer_sizes.emplace_back(width, height);
    }

    void on_content_scale_changed(const float xScale, const float yScale) override {
        content_scales.emplace_back(xScale, yScale);
    }
};

[[nodiscard]] std::unique_ptr<recording_window> make_recording_window(
    const eplxn::window_creation_params& creationParams = {}) {
    apply_headless_glfw_window_hints();
    return eplxn::linux_application_window::create<recording_window>("Events test window",
                                                                     creationParams);
}

// Calls every native event callback once, with fixed values.
void send_every_native_event(GLFWwindow* const window) {
    const auto mouseButton{installed_callback(&::glfwSetMouseButtonCallback, window)};
    const auto cursorPos{installed_callback(&::glfwSetCursorPosCallback, window)};
    const auto scroll{installed_callback(&::glfwSetScrollCallback, window)};
    const auto iconify{installed_callback(&::glfwSetWindowIconifyCallback, window)};
    const auto framebufferSize{installed_callback(&::glfwSetFramebufferSizeCallback, window)};
    const auto contentScale{installed_callback(&::glfwSetWindowContentScaleCallback, window)};
    REQUIRE(mouseButton != nullptr);
    REQUIRE(cursorPos != nullptr);
    REQUIRE(scroll != nullptr);
    REQUIRE(iconify != nullptr);
    REQUIRE(framebufferSize != nullptr);
    REQUIRE(contentScale != nullptr);

    mouseButton(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, GLFW_MOD_SHIFT);
    cursorPos(window, 12.5, 34.25);
    scroll(window, -1.0, 2.5);
    iconify(window, GLFW_TRUE);
    framebufferSize(window, 1600, 900);
    contentScale(window, 1.5F, 2.0F);
}

void require_every_native_event_recorded(const recording_window& window) {
    REQUIRE(window.mouse_buttons == std::vector<std::tuple<int, int, int>>{
                                        {GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, GLFW_MOD_SHIFT}
    });
    REQUIRE(window.cursor_positions == std::vector<std::pair<double, double>>{
                                           {12.5, 34.25}
    });
    REQUIRE(window.scrolls == std::vector<std::pair<double, double>>{
                                  {-1.0, 2.5}
    });
    REQUIRE(window.iconifications == std::vector<bool>{true});
    REQUIRE(window.framebuffer_sizes == std::vector<std::pair<std::int32_t, std::int32_t>>{
                                            {1600, 900}
    });
    REQUIRE(window.content_scales == std::vector<std::pair<float, float>>{
                                         {1.5F, 2.0F}
    });
}

} // namespace

TEST_CASE("linux_application_window tests - native event callbacks are wired",
          "[tca][engine][platform][linux][linux_application_window][events]") {
    scoped_null_glfw_context glfwContext{};

    GIVEN("A window constructed directly") {
        apply_headless_glfw_window_hints();
        eplxn::linux_application_window window{"Events test window",
                                               eplxn::window_creation_params{}};
        GLFWwindow* const handle{window.get_native_handle().get()};
        REQUIRE(handle != nullptr);

        THEN("The close and native event callbacks are installed") {
            REQUIRE(installed_callback(&::glfwSetWindowCloseCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetMouseButtonCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetCursorPosCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetScrollCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetWindowIconifyCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetFramebufferSizeCallback, handle) != nullptr);
            REQUIRE(installed_callback(&::glfwSetWindowContentScaleCallback, handle) != nullptr);
        }

        THEN("The default hooks take every event harmlessly") {
            REQUIRE_NOTHROW(send_every_native_event(handle));
        }
    }
}

TEST_CASE("linux_application_window tests - native event hooks",
          "[tca][engine][platform][linux][linux_application_window][events]") {
    scoped_null_glfw_context glfwContext{};
    const auto window{make_recording_window()};
    GLFWwindow* const handle{window->get_native_handle().get()};
    REQUIRE(handle != nullptr);

    SECTION("Each hook receives its event's values as GLFW reports them") {
        send_every_native_event(handle);

        require_every_native_event_recorded(*window);
    }

    SECTION("Minimizing and restoring reach on_iconify()") {
        // Real events: the null platform reports iconify changes itself.
        ::glfwIconifyWindow(handle);
        ::glfwRestoreWindow(handle);

        REQUIRE(window->iconifications == std::vector<bool>{true, false});
    }

    SECTION("Resizing the window reaches on_framebuffer_resized() and on_resize() alike") {
        bool resized{};
        window->on_resize().subscribe([&resized](const std::int32_t, const std::int32_t) {
            resized = true;
        });

        ::glfwSetWindowSize(handle, 800, 600);

        REQUIRE(resized);
        REQUIRE(window->framebuffer_sizes == std::vector<std::pair<std::int32_t, std::int32_t>>{
                                                 {800, 600}
        });
    }

    SECTION("A button release and its mods arrive untouched") {
        const auto mouseButton{installed_callback(&::glfwSetMouseButtonCallback, handle)};
        REQUIRE(mouseButton != nullptr);
        mouseButton(handle, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
        mouseButton(handle, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, GLFW_MOD_CONTROL | GLFW_MOD_ALT);

        REQUIRE(window->mouse_buttons ==
                std::vector<std::tuple<int, int, int>>{
                    {GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS,   0                              },
                    {GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, GLFW_MOD_CONTROL | GLFW_MOD_ALT}
        });
    }
}

TEST_CASE("linux_application_window tests - native event hooks follow a moved window",
          "[tca][engine][platform][linux][linux_application_window][events]") {
    scoped_null_glfw_context glfwContext{};

    // The source lives on the heap so it can be destroyed right after the move: any callback
    // still routed to it would then reach freed memory.
    auto source{make_recording_window()};
    GLFWwindow* const handle{source->get_native_handle().get()};
    REQUIRE(handle != nullptr);

    WHEN("It is move-constructed into another window and the source is destroyed") {
        recording_window target{std::move(*source)};
        source.reset();

        send_every_native_event(handle);
        send_close_request(handle);

        THEN("The new object's hooks receive the events") {
            require_every_native_event_recorded(target);
            REQUIRE(target.close_requests == 1);
        }
    }
}

TEST_CASE("linux_application_window tests - close requests",
          "[tca][engine][platform][linux][linux_application_window][events]") {
    scoped_null_glfw_context glfwContext{};

    GIVEN("A window whose on_close_requested() lets it close") {
        const auto window{make_recording_window()};
        GLFWwindow* const handle{window->get_native_handle().get()};
        REQUIRE_FALSE(window->should_close());

        WHEN("The user asks to close it") {
            send_close_request(handle);

            THEN("on_close_requested() is called once and the window should close") {
                REQUIRE(window->close_requests == 1);
                REQUIRE(window->should_close());
            }

            THEN("The window is not destroyed: its owner does that") {
                REQUIRE(window->get_native_handle().is_valid());
            }
        }
    }

    GIVEN("A window whose on_close_requested() vetoes the request") {
        const auto window{make_recording_window()};
        GLFWwindow* const handle{window->get_native_handle().get()};
        window->allow_close = false;

        WHEN("The user asks to close it") {
            send_close_request(handle);

            THEN("on_close_requested() is called once and the window should not close") {
                REQUIRE(window->close_requests == 1);
                REQUIRE_FALSE(window->should_close());
            }

            AND_WHEN("The user asks again and the window now allows it") {
                window->allow_close = true;
                send_close_request(handle);

                THEN("The window should close") {
                    REQUIRE(window->close_requests == 2);
                    REQUIRE(window->should_close());
                }
            }
        }

        WHEN("The program calls request_close()") {
            window->request_close();

            THEN("It closes without asking on_close_requested()") {
                REQUIRE(window->close_requests == 0);
                REQUIRE(window->should_close());
            }
        }
    }

    GIVEN("A window with the default on_close_requested()") {
        apply_headless_glfw_window_hints();
        const auto window{eplxn::linux_application_window::create<eplxn::linux_application_window>(
            "Events test window", eplxn::window_creation_params{})};

        WHEN("The user asks to close it") {
            send_close_request(window->get_native_handle().get());

            THEN("The window should close") {
                REQUIRE(window->should_close());
            }
        }
    }
}
