// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/linux/linux-application-window.hpp"

// Third-party
#include <GLFW/glfw3.h>
#include <catch2/catch_test_macros.hpp>

// C++ STL
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

// Third-party
#include <spdlog/sinks/ostream_sink.h>

// Engine
#include "platform/images/image-decoder.hpp"
#include "platform/platform-logger.hpp"
#include "testing/linux/windowing-utilities.hpp"
#include "utilities/test-assets.hpp"

namespace tests {

using tca::engine::testing::apply_headless_glfw_window_hints;
using tca::engine::testing::create_headless_glfw_window;
using tca::engine::testing::scoped_null_glfw_context;

/**
 * @brief Test-specific subclass recording what it observed when on_create() was called.
 */
class linux_application_window_tss final
    : public tca::engine::platform::posix_linux::linux_application_window {
public:
    using linux_application_window::linux_application_window;

    std::int32_t on_create_calls{};
    bool handle_valid_on_create{};
    bool visible_on_create{};

protected:
    // At -O2 and above gcc speculatively devirtualizes the on_create() call in
    // create<linux_application_window>() to this override, the only one in this file, then warns
    // that its writes would overrun the plain window it allocated there. That guarded branch never
    // runs: the call only lands here on a linux_application_window_tss.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    void on_create() override {
        linux_application_window::on_create();

        ++on_create_calls;
        handle_valid_on_create = get_native_handle().is_valid();
        visible_on_create =
            handle_valid_on_create &&
            ::glfwGetWindowAttrib(get_native_handle().get(), GLFW_VISIBLE) == GLFW_TRUE;
    }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
};

/**
 * @brief Adds a sink to the platform logger for its lifetime, capturing what it logs.
 */
class scoped_platform_log_capture final {
public:
    scoped_platform_log_capture()
        : m_sink{std::make_shared<spdlog::sinks::ostream_sink_mt>(m_stream)} {
        m_sink->set_pattern("[%l] %v");
        tca::engine::platform::get_platform_logger()->sinks().push_back(m_sink);
    }

    scoped_platform_log_capture(const scoped_platform_log_capture&) = delete;
    scoped_platform_log_capture& operator=(const scoped_platform_log_capture&) = delete;
    scoped_platform_log_capture(scoped_platform_log_capture&&) = delete;
    scoped_platform_log_capture& operator=(scoped_platform_log_capture&&) = delete;

    ~scoped_platform_log_capture() {
        auto& sinks{tca::engine::platform::get_platform_logger()->sinks()};
        std::erase(sinks, m_sink);
    }

    [[nodiscard]] std::string text() const {
        return m_stream.str();
    }

private:
    std::ostringstream m_stream{};
    std::shared_ptr<spdlog::sinks::ostream_sink_mt> m_sink{};
};

} // namespace tests

TEST_CASE("unique_glfw_window tests - construction",
          "[tca][engine][platform][linux][unique_glfw_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};

    GIVEN("No window has been created") {
        WHEN("A unique_glfw_window is default constructed") {
            unique_glfw_window window{};

            THEN("It does not own a window") {
                REQUIRE(window.get() == nullptr);
                REQUIRE_FALSE(window.is_valid());
                REQUIRE_FALSE(static_cast<bool>(window));
            }
        }

        WHEN("A unique_glfw_window is constructed from nullptr") {
            unique_glfw_window window{nullptr};

            THEN("It does not own a window") {
                REQUIRE(window.get() == nullptr);
                REQUIRE_FALSE(window.is_valid());
                REQUIRE_FALSE(static_cast<bool>(window));
            }
        }
    }

    GIVEN("A GLFW window has been created") {
        GLFWwindow* const rawWindow{tests::create_headless_glfw_window()};

        WHEN("A unique_glfw_window takes ownership of it") {
            unique_glfw_window window{rawWindow};

            THEN("It owns the given window") {
                REQUIRE(window.get() == rawWindow);
                REQUIRE(window.is_valid());
                REQUIRE(static_cast<bool>(window));
            }
        }
    }
}

TEST_CASE("unique_glfw_window tests - copy and move traits",
          "[tca][engine][platform][linux][unique_glfw_window]") {
    using namespace tca::engine::platform::posix_linux;

    THEN("unique_glfw_window is not copy constructible") {
        STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<unique_glfw_window>);
    }

    THEN("unique_glfw_window is not copy assignable") {
        STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<unique_glfw_window>);
    }

    THEN("unique_glfw_window is move constructible") {
        STATIC_REQUIRE(std::is_move_constructible_v<unique_glfw_window>);
    }

    THEN("unique_glfw_window is move assignable") {
        STATIC_REQUIRE(std::is_move_assignable_v<unique_glfw_window>);
    }
}

TEST_CASE("unique_glfw_window tests - move construction",
          "[tca][engine][platform][linux][unique_glfw_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};

    GIVEN("A unique_glfw_window owning a window") {
        GLFWwindow* const rawWindow{tests::create_headless_glfw_window()};
        unique_glfw_window source{rawWindow};

        WHEN("It is move-constructed into a new unique_glfw_window") {
            unique_glfw_window destination{std::move(source)};

            THEN("The destination owns the original window") {
                REQUIRE(destination.get() == rawWindow);
                REQUIRE(destination.is_valid());
            }

            THEN("The source no longer owns any window") {
                REQUIRE(source.get() == nullptr); // NOLINT(*-use-after-move)
                REQUIRE_FALSE(source.is_valid()); // NOLINT(*-use-after-move)
            }
        }
    }
}

TEST_CASE("unique_glfw_window tests - move assignment",
          "[tca][engine][platform][linux][unique_glfw_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};

    GIVEN("Two unique_glfw_window instances, each owning a different window") {
        GLFWwindow* const firstWindow{tests::create_headless_glfw_window("first")};
        GLFWwindow* const secondWindow{tests::create_headless_glfw_window("second")};

        unique_glfw_window source{firstWindow};
        unique_glfw_window destination{secondWindow};

        WHEN("The source is move-assigned into the destination") {
            destination = std::move(source);

            THEN("The destination now owns the source's window") {
                REQUIRE(destination.get() == firstWindow);
                REQUIRE(destination.is_valid());
            }

            THEN("The source no longer owns any window") {
                REQUIRE(source.get() == nullptr); // NOLINT(*-use-after-move)
                REQUIRE_FALSE(source.is_valid()); // NOLINT(*-use-after-move)
            }

            // destination's original window (secondWindow) is expected to have been destroyed
            // as part of the assignment, so nothing else to assert on it here without a way to
            // observe GLFW's internal state from the outside.
        }
    }

    GIVEN("A unique_glfw_window owning a window") {
        GLFWwindow* const rawWindow{tests::create_headless_glfw_window()};
        unique_glfw_window window{rawWindow};

        WHEN("It is move-assigned to itself") {
            // Self-move-assignment is intentional here: this is exactly what
            // unique_glfw_window::operator=(unique_glfw_window&&) must guard against, so silence
            // the compiler's (correct, but unwanted here) warning about it.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
            window = std::move(window); // NOLINT(*-self-move)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

            THEN("It still owns the same window") {
                REQUIRE(window.get() == rawWindow);
                REQUIRE(window.is_valid());
            }
        }
    }
}

TEST_CASE("unique_glfw_window tests - reset()",
          "[tca][engine][platform][linux][unique_glfw_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};

    GIVEN("A unique_glfw_window owning a window") {
        GLFWwindow* const rawWindow{tests::create_headless_glfw_window()};
        unique_glfw_window window{rawWindow};

        WHEN("reset() is called with no arguments") {
            window.reset();

            THEN("It no longer owns any window") {
                REQUIRE(window.get() == nullptr);
                REQUIRE_FALSE(window.is_valid());
            }
        }

        WHEN("reset() is called with a different window") {
            GLFWwindow* const replacementWindow{tests::create_headless_glfw_window("replacement")};
            window.reset(replacementWindow);

            THEN("It owns the new window instead") {
                REQUIRE(window.get() == replacementWindow);
                REQUIRE(window.is_valid());
            }
        }
    }

    GIVEN("A default constructed unique_glfw_window") {
        unique_glfw_window window{};

        WHEN("reset() is called with no arguments") {
            THEN("It does not throw and still owns no window") {
                REQUIRE_NOTHROW(window.reset());
                REQUIRE(window.get() == nullptr);
            }
        }
    }
}

TEST_CASE("unique_glfw_window tests - release()",
          "[tca][engine][platform][linux][unique_glfw_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};

    GIVEN("A unique_glfw_window owning a window") {
        GLFWwindow* const rawWindow{tests::create_headless_glfw_window()};
        unique_glfw_window window{rawWindow};

        WHEN("release() is called") {
            GLFWwindow* const releasedWindow{window.release()};

            THEN("The returned pointer is the originally owned window") {
                REQUIRE(releasedWindow == rawWindow);
            }

            THEN("The unique_glfw_window no longer owns any window") {
                REQUIRE(window.get() == nullptr);
                REQUIRE_FALSE(window.is_valid());
            }

            // release() hands ownership back to the caller without destroying the window, so we
            // destroy it manually here to avoid leaking it for the rest of the test run.
            ::glfwDestroyWindow(releasedWindow);
        }
    }
}

TEST_CASE("unique_glfw_window tests - swap()",
          "[tca][engine][platform][linux][unique_glfw_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};

    GIVEN("Two unique_glfw_window instances, each owning a different window") {
        GLFWwindow* const firstWindow{tests::create_headless_glfw_window("first")};
        GLFWwindow* const secondWindow{tests::create_headless_glfw_window("second")};

        unique_glfw_window first{firstWindow};
        unique_glfw_window second{secondWindow};

        WHEN("They are swapped") {
            first.swap(second);

            THEN("Their owned windows are exchanged") {
                REQUIRE(first.get() == secondWindow);
                REQUIRE(second.get() == firstWindow);
            }
        }
    }

    GIVEN("An owning unique_glfw_window and a default constructed one") {
        GLFWwindow* const rawWindow{tests::create_headless_glfw_window()};

        unique_glfw_window owning{rawWindow};
        unique_glfw_window empty{};

        WHEN("They are swapped") {
            owning.swap(empty);

            THEN("Ownership moves to what used to be the empty instance") {
                REQUIRE(empty.get() == rawWindow);
                REQUIRE(owning.get() == nullptr);
            }
        }
    }
}

TEST_CASE("unique_glfw_window tests - destructor",
          "[tca][engine][platform][linux][unique_glfw_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};

    GIVEN("A window owned by a unique_glfw_window") {
        GLFWwindow* const rawWindow{tests::create_headless_glfw_window()};

        WHEN("The unique_glfw_window goes out of scope") {
            THEN("Destroying it does not crash the test binary") {
                unique_glfw_window window{rawWindow};
            }
        }
    }
}

/**
 * linux_application_window is pure RAII (unlike win32_application_window, which supports both
 * pure RAII and two-phase creation): its constructor always creates the underlying GLFW window
 * immediately, so there is no separate create_window()/two_phase_window_creation() step to test
 * here. on_create() is reached through create() instead, tested below.
 */
TEST_CASE("linux_application_window tests - construction",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Default creation parameters") {
        WHEN("A linux_application_window is constructed") {
            THEN("It does not throw and the underlying GLFW window is created immediately") {
                linux_application_window window{"Test Window", window_creation_params{}};

                REQUIRE(window.get_native_handle().is_valid());
            }
        }
    }

    GIVEN("Creation parameters requesting a specific initial size") {
        constexpr window_creation_params creationParams{
            .initial_size = window_creation_size{640, 480}
        };

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window is created with the requested size") {
                GLFWwindow* const rawWindow{window.get_native_handle().get()};
                REQUIRE(rawWindow != nullptr);

                int width{};
                int height{};
                ::glfwGetWindowSize(rawWindow, &width, &height);

                REQUIRE(width == 640);
                REQUIRE(height == 480);
            }
        }
    }
}

TEST_CASE("linux_application_window tests - copy and move traits",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    THEN("linux_application_window is not copy constructible") {
        STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<linux_application_window>);
    }

    THEN("linux_application_window is not copy assignable") {
        STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<linux_application_window>);
    }

    // These two are already green: application_window's user-declared destructor suppresses its
    // own implicit move members, but since it has no data members of its own, the (deprecated)
    // implicit copy constructor covers that empty base for free, and linux_application_window's
    // own move-only unique_glfw_window member is still moved normally. Asserted here as a
    // documented guarantee (mirroring win32_application_window's explicit move support) rather
    // than a gap to close.
    THEN("linux_application_window is move constructible") {
        STATIC_REQUIRE(std::is_move_constructible_v<linux_application_window>);
    }

    THEN("linux_application_window is move assignable") {
        STATIC_REQUIRE(std::is_move_assignable_v<linux_application_window>);
    }
}

TEST_CASE("linux_application_window tests - destructor",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("A constructed linux_application_window") {
        WHEN("It goes out of scope") {
            THEN("Destroying it does not crash the test binary") {
                linux_application_window window{"Test Window", window_creation_params{}};
            }
        }
    }
}

/**
 * @brief RED: linux_application_window::show() is currently a no-op stub, so this fails until it
 * actually calls glfwShowWindow() on the owned window.
 */
TEST_CASE("linux_application_window tests - show()",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("A window constructed hidden") {
        linux_application_window window{"Test Window", window_creation_params{}};
        GLFWwindow* const rawWindow{window.get_native_handle().get()};
        REQUIRE(rawWindow != nullptr);
        REQUIRE(::glfwGetWindowAttrib(rawWindow, GLFW_VISIBLE) == GLFW_FALSE);

        WHEN("show() is called") {
            window.show();

            THEN("The underlying GLFW window becomes visible") {
                REQUIRE(::glfwGetWindowAttrib(rawWindow, GLFW_VISIBLE) == GLFW_TRUE);
            }
        }
    }
}

/**
 * @brief RED: linux_application_window::should_close()/request_close() are currently stubs that
 * never touch GLFW's own should-close flag, so this fails until they are wired to
 * glfwWindowShouldClose()/glfwSetWindowShouldClose().
 */
TEST_CASE("linux_application_window tests - should_close() / request_close()",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("A newly constructed window") {
        linux_application_window window{"Test Window", window_creation_params{}};

        THEN("It has not been asked to close") {
            REQUIRE_FALSE(window.should_close());
        }

        WHEN("request_close() is called") {
            window.request_close();

            THEN("should_close() reports true") {
                REQUIRE(window.should_close());
            }

            THEN("GLFW's own should-close flag is set too") {
                GLFWwindow* const rawWindow{window.get_native_handle().get()};
                REQUIRE(rawWindow != nullptr);
                REQUIRE(::glfwWindowShouldClose(rawWindow) == GLFW_TRUE);
            }
        }
    }
}

/**
 * @brief RED: linux_application_window::on_resize() lets callers subscribe fine (that part is
 * just tca::engine::core::event plumbing, already implemented and tested on its own), but nothing
 * currently calls execute() on the underlying resize_event_t, so this fails until the window's
 * GLFW size callback is wired up to fire it.
 */
TEST_CASE("linux_application_window tests - resize event",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("A window with a handler subscribed to its resize event") {
        linux_application_window window{"Test Window", window_creation_params{}};

        bool handlerCalled{};
        std::int32_t observedWidth{};
        std::int32_t observedHeight{};
        window.on_resize().subscribe([&handlerCalled, &observedWidth, &observedHeight](
                                         const std::int32_t width, const std::int32_t height) {
            handlerCalled = true;
            observedWidth = width;
            observedHeight = height;
        });

        WHEN("The window's client area is resized") {
            GLFWwindow* const rawWindow{window.get_native_handle().get()};
            REQUIRE(rawWindow != nullptr);
            ::glfwSetWindowSize(rawWindow, 800, 600);

            THEN("The subscribed handler is invoked with the new size") {
                REQUIRE(handlerCalled);
                REQUIRE(observedWidth == 800);
                REQUIRE(observedHeight == 600);
            }
        }
    }
}

TEST_CASE("linux_application_window tests - window_creation_params defaults",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    constexpr window_creation_params defaultParams{};

    THEN("No initial position is requested, leaving placement to GLFW") {
        STATIC_REQUIRE_FALSE(defaultParams.initial_position.has_value());
    }

    THEN("The window is resizable and decorated") {
        STATIC_REQUIRE(defaultParams.resizable);
        STATIC_REQUIRE(defaultParams.decorated);
    }

    THEN("The window is created hidden, like win32_application_window, and not maximized") {
        STATIC_REQUIRE_FALSE(defaultParams.visible);
        STATIC_REQUIRE_FALSE(defaultParams.maximized);
    }

    THEN("The window takes focus when created visible and when shown") {
        STATIC_REQUIRE(defaultParams.focused);
        STATIC_REQUIRE(defaultParams.focus_on_show);
    }

    THEN("The window is windowed and keeps the default icon") {
        STATIC_REQUIRE_FALSE(defaultParams.fullscreen);
        STATIC_REQUIRE(defaultParams.icons.empty());
    }
}

/**
 * @brief RED: instantiate_window() only forwards initial_size to glfwCreateWindow(), so every other
 * window_creation_params field fails here until it is applied as the matching GLFW window hint
 * (or, for icons, through glfwSetWindowIcon()).
 */
TEST_CASE("linux_application_window tests - initial position",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Creation parameters requesting a specific initial position") {
        constexpr window_creation_params creationParams{
            .initial_position = window_creation_position{100, 200}
        };

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window is placed at the requested position") {
                int x{};
                int y{};
                ::glfwGetWindowPos(window.get_native_handle().get(), &x, &y);

                REQUIRE(x == 100);
                REQUIRE(y == 200);
            }
        }
    }
}

TEST_CASE("linux_application_window tests - resizable flag",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Creation parameters asking for a non-resizable window") {
        constexpr window_creation_params creationParams{.resizable = false};

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window is not resizable") {
                REQUIRE(::glfwGetWindowAttrib(window.get_native_handle().get(), GLFW_RESIZABLE) ==
                        GLFW_FALSE);
            }
        }
    }
}

TEST_CASE("linux_application_window tests - decorated flag",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Creation parameters asking for an undecorated window") {
        constexpr window_creation_params creationParams{.decorated = false};

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window has no decorations") {
                REQUIRE(::glfwGetWindowAttrib(window.get_native_handle().get(), GLFW_DECORATED) ==
                        GLFW_FALSE);
            }
        }
    }
}

TEST_CASE("linux_application_window tests - visible flag",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Creation parameters asking for a window visible on creation") {
        constexpr window_creation_params creationParams{.visible = true};

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window is visible without calling show()") {
                REQUIRE(::glfwGetWindowAttrib(window.get_native_handle().get(), GLFW_VISIBLE) ==
                        GLFW_TRUE);
            }
        }
    }
}

TEST_CASE("linux_application_window tests - maximized flag",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Creation parameters asking for a maximized window") {
        constexpr window_creation_params creationParams{.maximized = true};

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window is maximized") {
                REQUIRE(::glfwGetWindowAttrib(window.get_native_handle().get(), GLFW_MAXIMIZED) ==
                        GLFW_TRUE);
            }
        }
    }
}

TEST_CASE("linux_application_window tests - focused flags",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Creation parameters asking for a window visible and focused on creation") {
        constexpr window_creation_params creationParams{.visible = true, .focused = true};

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window has input focus") {
                REQUIRE(::glfwGetWindowAttrib(window.get_native_handle().get(), GLFW_FOCUSED) ==
                        GLFW_TRUE);
            }
        }
    }

    GIVEN("Creation parameters asking for a window visible but not focused on creation") {
        constexpr window_creation_params creationParams{.visible = true, .focused = false};

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window is visible") {
                REQUIRE(::glfwGetWindowAttrib(window.get_native_handle().get(), GLFW_VISIBLE) ==
                        GLFW_TRUE);
            }

            THEN("The underlying GLFW window does not have input focus") {
                REQUIRE(::glfwGetWindowAttrib(window.get_native_handle().get(), GLFW_FOCUSED) ==
                        GLFW_FALSE);
            }
        }
    }

    GIVEN("Creation parameters asking for a window that does not take focus on show()") {
        constexpr window_creation_params creationParams{.focus_on_show = false};

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window's focus-on-show attribute is off") {
                REQUIRE(::glfwGetWindowAttrib(window.get_native_handle().get(),
                                              GLFW_FOCUS_ON_SHOW) == GLFW_FALSE);
            }

            AND_WHEN("show() is called") {
                window.show();

                THEN("The underlying GLFW window becomes visible without taking input focus") {
                    GLFWwindow* const rawWindow{window.get_native_handle().get()};
                    REQUIRE(::glfwGetWindowAttrib(rawWindow, GLFW_VISIBLE) == GLFW_TRUE);
                    REQUIRE(::glfwGetWindowAttrib(rawWindow, GLFW_FOCUSED) == GLFW_FALSE);
                }
            }
        }
    }
}

TEST_CASE("linux_application_window tests - fullscreen on create",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Default creation parameters") {
        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", window_creation_params{}};

            THEN("The underlying GLFW window is windowed") {
                REQUIRE(::glfwGetWindowMonitor(window.get_native_handle().get()) == nullptr);
            }
        }
    }

    GIVEN("Creation parameters asking for a fullscreen window") {
        constexpr window_creation_params creationParams{.fullscreen = true};

        WHEN("A linux_application_window is constructed") {
            linux_application_window window{"Test Window", creationParams};

            THEN("The underlying GLFW window is fullscreen on the primary monitor") {
                GLFWmonitor* const primaryMonitor{::glfwGetPrimaryMonitor()};
                REQUIRE(primaryMonitor != nullptr);
                REQUIRE(::glfwGetWindowMonitor(window.get_native_handle().get()) == primaryMonitor);
            }
        }
    }
}

/**
 * @brief Smoke test only: the GLFW null platform implements glfwSetWindowIcon() as a no-op and GLFW
 * has no getter for the icon, so the most this can assert headlessly is that creating a window
 * with an icon succeeds. It is green before the icon is wired up.
 */
TEST_CASE("linux_application_window tests - icon",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;
    namespace images = tca::engine::platform::images;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Creation parameters carrying an icon decoded from an image file") {
        const std::filesystem::path iconPath{tca::tests::utilities::get_test_pattern_path()};
        REQUIRE(std::filesystem::exists(iconPath));

        const std::array<images::decoded_image, 1> icons{images::decode_image_from_file(iconPath)};
        const window_creation_params creationParams{.icons = icons};

        WHEN("A linux_application_window is constructed") {
            THEN("It does not throw and the underlying GLFW window is created") {
                linux_application_window window{"Test Window", creationParams};

                REQUIRE(window.get_native_handle().is_valid());
            }
        }
    }
}

/**
 * @brief RED: GLFW window hints are global and sticky, so a hint set by someone else before the
 * window is created must not override what window_creation_params asks for.
 */
TEST_CASE("linux_application_window tests - creation params override stale GLFW hints",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Global GLFW hints opposite to the default creation parameters") {
        ::glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        ::glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        ::glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        ::glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

        WHEN("A linux_application_window is constructed with default creation parameters") {
            linux_application_window window{"Test Window", window_creation_params{}};
            GLFWwindow* const rawWindow{window.get_native_handle().get()};

            THEN("The underlying GLFW window follows the creation parameters, not the hints") {
                REQUIRE(::glfwGetWindowAttrib(rawWindow, GLFW_VISIBLE) == GLFW_FALSE);
                REQUIRE(::glfwGetWindowAttrib(rawWindow, GLFW_RESIZABLE) == GLFW_TRUE);
                REQUIRE(::glfwGetWindowAttrib(rawWindow, GLFW_DECORATED) == GLFW_TRUE);
                REQUIRE(::glfwGetWindowAttrib(rawWindow, GLFW_MAXIMIZED) == GLFW_FALSE);
            }
        }
    }
}

TEST_CASE("linux_application_window tests - initial position does not carry over",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("A window created at a specific initial position") {
        constexpr window_creation_params positionedParams{
            .initial_position = window_creation_position{100, 200}
        };
        const linux_application_window positionedWindow{"Positioned Window", positionedParams};

        WHEN("Another window is created without an initial position") {
            linux_application_window window{"Test Window", window_creation_params{}};

            THEN("It is not placed at the previous window's position") {
                int x{};
                int y{};
                ::glfwGetWindowPos(window.get_native_handle().get(), &x, &y);

                REQUIRE_FALSE((x == 100 && y == 200));
            }
        }
    }
}

TEST_CASE("linux_application_window tests - on_create()",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("A subclass overriding on_create()") {
        WHEN("It is built through create()") {
            const std::unique_ptr<tests::linux_application_window_tss> window{
                linux_application_window::create<tests::linux_application_window_tss>(
                    "Test Window", window_creation_params{})};

            THEN("on_create() is called exactly once") {
                REQUIRE(window->on_create_calls == 1);
            }

            THEN("The native window is already valid inside on_create()") {
                REQUIRE(window->handle_valid_on_create);
            }

            THEN("The window is not shown yet inside on_create()") {
                REQUIRE_FALSE(window->visible_on_create);
            }
        }

        WHEN("It is constructed directly") {
            const tests::linux_application_window_tss window{"Test Window",
                                                             window_creation_params{}};

            THEN("on_create() is not called") {
                REQUIRE(window.on_create_calls == 0);
            }
        }
    }

    GIVEN("The base linux_application_window type") {
        WHEN("It is built through create()") {
            const std::unique_ptr<linux_application_window> window{
                linux_application_window::create<linux_application_window>(
                    "Test Window", window_creation_params{})};

            THEN("The window is created") {
                REQUIRE(window->get_native_handle().is_valid());
            }
        }
    }
}

TEST_CASE("linux_application_window tests - logging",
          "[tca][engine][platform][linux][linux_application_window][logging]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();
    const tests::scoped_platform_log_capture logCapture{};

    WHEN("A window is created") {
        const linux_application_window window{"Logged Window", window_creation_params{}};

        THEN("Its creation is logged as info, with its title") {
            REQUIRE(logCapture.text().find("[info] Created a new window with title 'Logged "
                                           "Window'") != std::string::npos);
        }

        AND_WHEN("The user asks to close it") {
            GLFWwindow* const handle{window.get_native_handle().get()};
            const GLFWwindowclosefun callback{::glfwSetWindowCloseCallback(handle, nullptr)};
            std::ignore = ::glfwSetWindowCloseCallback(handle, callback);
            REQUIRE(callback != nullptr);
            ::glfwSetWindowShouldClose(handle, GLFW_TRUE);
            callback(handle);

            THEN("The close request is logged as info") {
                REQUIRE(logCapture.text().find(
                            "[info] The window with title 'Logged Window' was asked to close") !=
                        std::string::npos);
            }
        }
    }

    WHEN("A window is created through create()") {
        const auto window{linux_application_window::create<linux_application_window>(
            "Factory Window", window_creation_params{})};

        THEN("Its creation is logged exactly once") {
            const std::string text{logCapture.text()};
            const auto first{text.find("Created a new window with title 'Factory Window'")};
            REQUIRE(first != std::string::npos);
            REQUIRE(text.find("Created a new window", first + 1) == std::string::npos);
        }
    }
}

TEST_CASE("linux_application_window tests - creation failure",
          "[tca][engine][platform][linux][linux_application_window]") {
    using namespace tca::engine::platform::posix_linux;

    tests::scoped_null_glfw_context glfwContext{};
    tests::apply_headless_glfw_window_hints();

    GIVEN("Creation parameters GLFW refuses (an empty size)") {
        constexpr window_creation_params emptySize{
            .initial_size = window_creation_size{.width = 0, .height = 0}
        };

        WHEN("A linux_application_window is constructed") {
            THEN("It throws instead of terminating the program") {
                REQUIRE_THROWS_AS((linux_application_window{"Test Window", emptySize}),
                                  std::runtime_error);
            }
        }
    }
}
