// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/win32/win32-application-window.hpp"

// C++ STL
#include <functional> // for std::function
#include <tuple>      // for std::ignore
#include <unordered_map>

// Third-party
#include <catch2/catch_test_macros.hpp>

#include "utilities/win32/wm-quit-drain.hpp"

namespace tests {

namespace epn = tca::engine::platform;
namespace epw32n = epn::win32;

/**
 * @brief Test-specific subclass of win32_application_window that allows us to set a custom window
 * procedure for testing purposes. This is useful to test the window procedure routing capabilities
 * of the win32_application_window class, by allowing us to set a custom window procedure that will
 * handle specific messages and set flags when they are handled, so that we can check in our tests
 * if the messages were properly routed to the object-specific window procedure and handled there.
 */
class win32_application_window_tss final : public epw32n::win32_application_window {
public:
    using custom_window_procedure_t = std::function<route_result_t(
        const HWND, const epw32n::windows_messages, const epw32n::details::wparam_t,
        const epw32n::details::lparam_t)>;

    explicit win32_application_window_tss(
        const epn::platform_preferred_string_view_t title, custom_window_procedure_t testWindowProc,
        epw32n::partial_wndclassex windowClassData = {},
        epw32n::partial_window_creation_params creationParams = {},
        std::function<void()> onCreateCallback = {})
        : win32_application_window(title, std::move(windowClassData), std::move(creationParams)),
          m_testSpecificWindowProcedure{std::move(testWindowProc)},
          m_onCreateCallback{std::move(onCreateCallback)} {}

protected:
    route_result_t routed_window_procedure(
        const HWND hwnd, const epw32n::windows_messages message,
        const epw32n::details::wparam_t wparam,
        const epw32n::details::lparam_t lparam) noexcept override {
        if (m_testSpecificWindowProcedure) {
            return m_testSpecificWindowProcedure(hwnd, message, wparam, lparam);
        }

        return std::nullopt;
    }

    void on_create() override {
        win32_application_window::on_create();
        if (m_onCreateCallback) {
            m_onCreateCallback();
        }
    }

private:
    custom_window_procedure_t m_testSpecificWindowProcedure{};
    std::function<void()> m_onCreateCallback{};
};

/**
 * @brief Simulates a peek message loop, with a fixed number of iterations, that processes messages
 * for the given window. When the outermost loop iterations are 0, we post a quit message,
 * simulating the user closing the window, which should cause the message loop to exit.
 *
 * If simulateUserClosingWindow is true, instead of destroying the window when the outermost loop
 * iterations are 0, we post a WM_CLOSE message to the window, simulating the user closing the
 * window by clicking the close button on the title bar, which should cause the window to receive a
 * WM_CLOSE message.
 *
 * @note This function uses iteration control to prevent infinite loops in case something goes
 * wrong.
 *
 * @param testWindow The window for which we want to process messages in the message loop.
 * @param simulateUserClosingWindow If true, when the outermost loop iterations are 0, we post a
 * WM_CLOSE message to the window.
 */
void simulate_peek_message_loop(std::unique_ptr<win32_application_window_tss>& testWindow,
                                const bool simulateUserClosingWindow = false) {
    const HWND hwnd{testWindow->get_native_handle().get()};
    REQUIRE(hwnd != nullptr);

    MSG msg{};
    HWND hwndToCheck{hwnd};
    bool closeRequested{};

    constexpr std::size_t MAX_LOOP_ITERATIONS{100};
    constexpr std::size_t USER_CLOSE_LOOP_ITERATION{10};

    // Here we simulate a message loop, using PeekMessage as message pump.
    // When the outermost loop iterations are 0, we post a quit message,
    // simulating the user closing the window, which should cause the message
    // loop to exit.
    for (std::size_t loopIterations{}; loopIterations < MAX_LOOP_ITERATIONS; ++loopIterations) {
        while (::PeekMessage(&msg, hwndToCheck, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                break;
            }
        }

        if (msg.message == WM_QUIT) {
            break;
        }

        if (closeRequested) {
            // We should not be here because we should have already received a WM_DESTROY and a
            // WM_QUIT message, but just in case, we break the loop to prevent infinite loops.
            CHECK_FALSE(closeRequested);
            break;
        }

        if (loopIterations == USER_CLOSE_LOOP_ITERATION && !simulateUserClosingWindow) {
            // Destroy the window
            testWindow.reset();

            // We set this to nullptr because after the window is destroyed, we
            // won't receive any message for it, so we can stop checking for
            // messages for that window, and just check for messages for any
            // window, since we just want to process the WM_QUIT message that
            // will be posted when the window is destroyed.
            hwndToCheck = nullptr;
        } else if (loopIterations == USER_CLOSE_LOOP_ITERATION && simulateUserClosingWindow &&
                   !closeRequested) {
            // Simulate the user closing the window by posting a WM_CLOSE message
            ::PostMessage(hwnd, WM_CLOSE, 0, 0);
            // Just a safety measure to prevent infinite loops when the windows do not send a
            // WM_DESTROY message they are supposed to send when they receive a WM_CLOSE message,
            // which would cause the message loop to never receive the WM_QUIT message that is
            // posted when the window is destroyed.
            closeRequested = true;
            hwndToCheck = nullptr;
        }
    }
}

} // namespace tests

/**
 * These tests will create a window in the system. Be careful when running them.
 */
TEST_CASE("win32_application_window tests - Pure RAII",
          "[tca][engine][platform][win32][win32_application_window]") {
    using namespace tca::engine::platform::win32;

    static constexpr partial_window_creation_params defaultCreationParams{.use_pure_raii = true};

    SECTION("Creating a win32_application_window with default parameters should succeed") {
        REQUIRE_NOTHROW(win32_application_window{L"Test Window", {}, defaultCreationParams});
    }

    SECTION("Window procedure routing") {
        SECTION("Window creation") {
            GIVEN("A window with standard class and creation data") {
                std::unique_ptr<tests::win32_application_window_tss> testWindow{};
                WHEN("The window is created") {
                    bool testWindowProcCalled{};
                    REQUIRE_NOTHROW(
                        testWindow = std::make_unique<tests::win32_application_window_tss>(
                            L"Test Window", [&testWindowProcCalled](
                                                const HWND, const windows_messages,
                                                const details::wparam_t, const details::lparam_t) {
                                testWindowProcCalled = true;
                                return std::nullopt;
                            }));
                    THEN("No message should be handled by the object-specific window procedure") {
                        // This is because it may be called when the object is still being
                        // constructed, the virtual table might not be properly set up.
                        CHECK_FALSE(testWindowProcCalled);
                    }
                }
            }
        }

        SECTION("Sending custom Windows message") {
            tests::utilities::drain_wm_quit_messages();
            GIVEN("A window with a custom window procedure that handles a custom message") {
                std::unique_ptr<tests::win32_application_window_tss> testWindow{};
                constexpr windows_messages customMessage{windows_messages::user};
                bool customMessageHandled{};
                REQUIRE_NOTHROW(
                    testWindow = std::make_unique<tests::win32_application_window_tss>(
                        L"Test Window",
                        [&customMessageHandled](
                            const HWND, const windows_messages message, details::wparam_t,
                            details::lparam_t) -> std::optional<details::lresult_t> {
                            if (message == customMessage) {
                                customMessageHandled = true;
                                return std::optional<details::lresult_t>{0};
                            }

                            return std::nullopt;
                        },
                        partial_wndclassex{}, defaultCreationParams));

                // We should not have handled the custom message yet.
                REQUIRE_FALSE(customMessageHandled);

                WHEN("We send the custom message to the window") {
                    const HWND hwnd{testWindow->get_native_handle().get()};
                    REQUIRE(hwnd != nullptr);

                    ::SendMessage(hwnd, to_underlying(customMessage), 0, 0);

                    THEN("The custom window procedure should handle the message") {
                        REQUIRE(customMessageHandled);
                    }
                }
            }
        }

        SECTION("Showing a window and simulating a message loop with PeekMessage") {
            // Remove all the WM_QUIT messages that where posted by other tests when the window were
            // destroyed.
            tests::utilities::drain_wm_quit_messages();

            GIVEN("A window with a custom window procedure that handles a custom message") {
                std::unique_ptr<tests::win32_application_window_tss> testWindow{};
                std::unordered_map<windows_messages, bool> handledMessages{};

                REQUIRE_NOTHROW(
                    testWindow = std::make_unique<tests::win32_application_window_tss>(
                        L"Test Window",
                        [&handledMessages](const HWND, const windows_messages message,
                                           details::wparam_t,
                                           details::lparam_t) -> std::optional<details::lresult_t> {
                            handledMessages[message] = true;

                            return std::nullopt;
                        },
                        partial_wndclassex{}, defaultCreationParams));

                // We should not have handled any message yet.
                REQUIRE(handledMessages.empty());

                WHEN("We show the window") {
                    REQUIRE_NOTHROW(testWindow->show());

                    AND_WHEN("We simulate a message loop - PeekMessage") {
                        tests::simulate_peek_message_loop(testWindow);
                    }

                    THEN("The window procedure should have handled the WM_SIZE message") {
                        REQUIRE(handledMessages.contains(windows_messages::size));
                        REQUIRE(handledMessages[windows_messages::size]);
                    }
                }
            }
        }
    }
}

TEST_CASE("win32_application_window tests - Two-phased creation",
          "[tca][engine][platform][win32][win32_application_window]") {
    using namespace tca::engine::platform::win32;

    static constexpr partial_window_creation_params defaultCreationParams{.use_pure_raii = false};

    SECTION("Creating a win32_application_window with default parameters should succeed") {
        REQUIRE_NOTHROW(
            std::ignore =
                win32_application_window::two_phase_window_creation<win32_application_window>(
                    L"Test Window", partial_wndclassex{}, defaultCreationParams));
    }

    SECTION("Window procedure routing") {
        // Remove all the WM_QUIT messages that where posted by other tests when the window were
        // destroyed.
        tests::utilities::drain_wm_quit_messages();

        SECTION("Routing at window creation") {
            GIVEN("A window created with two-phased creation") {
                std::unordered_map<windows_messages, bool> handledMessages{};
                std::unique_ptr<tests::win32_application_window_tss> testWindow{};
                bool onCreateCallbackCalled{};
                REQUIRE_NOTHROW(
                    testWindow = win32_application_window::two_phase_window_creation<
                        tests::win32_application_window_tss>(
                        L"Test Window",
                        [&handledMessages](const HWND, const windows_messages message,
                                           details::wparam_t,
                                           details::lparam_t) -> std::optional<details::lresult_t> {
                            handledMessages[message] = true;
                            return std::nullopt;
                        },
                        partial_wndclassex{}, defaultCreationParams,
                        [&onCreateCallbackCalled]() {
                            onCreateCallbackCalled = true;
                        }));

                THEN("The object-specific window procedure should have handled some messages") {
                    // This is because the window is created immediately after the construction of
                    // the object, so the virtual table should be properly set up when we receive
                    // messages for that window.
                    REQUIRE_FALSE(handledMessages.empty());
                }

                THEN(
                    "The object-specific window procedure should have handled the WM_CREATE "
                    "message") {
                    REQUIRE(handledMessages.contains(windows_messages::create));
                    REQUIRE(handledMessages[windows_messages::create]);
                }

                THEN(
                    "The object-specific window procedure should have handled the WM_NCCALCSIZE "
                    "message") {
                    REQUIRE(handledMessages.contains(windows_messages::nc_calc_size));
                    REQUIRE(handledMessages[windows_messages::nc_calc_size]);
                }

                THEN(
                    "The object-specific window procedure should not have handled the WM_NCCREATE "
                    "message") {
                    // This is because win32_application_window already handles the WM_NCCREATE
                    // message. This message contains important data for the overall behavior of the
                    // window, so it should not be handled by the object-specific window procedure,
                    // otherwise we might have unexpected behavior.
                    REQUIRE_FALSE(handledMessages.contains(windows_messages::nc_create));
                }

                THEN("The on_create callback should have been called") {
                    REQUIRE(onCreateCallbackCalled);
                }
            }
        }

        SECTION(
            "Showing a window created with two-phased creation and simulating a message loop with "
            "PeekMessage") {
            GIVEN("A window created with two-phased creation") {
                std::unique_ptr<tests::win32_application_window_tss> testWindow{};
                std::unordered_map<windows_messages, bool> handledMessages{};

                REQUIRE_NOTHROW(
                    testWindow = win32_application_window::two_phase_window_creation<
                        tests::win32_application_window_tss>(
                        L"Test Window",
                        [&handledMessages](
                            const HWND, const windows_messages message, const details::wparam_t,
                            const details::lparam_t) -> std::optional<details::lresult_t> {
                            handledMessages[message] = true;

                            return std::nullopt;
                        },
                        partial_wndclassex{}, defaultCreationParams));

                WHEN("We show the window") {
                    REQUIRE_NOTHROW(testWindow->show());

                    AND_WHEN("We simulate a message loop - PeekMessage") {
                        tests::simulate_peek_message_loop(testWindow);

                        THEN("The window procedure should have handled the WM_SIZE message") {
                            REQUIRE(handledMessages.contains(windows_messages::size));
                            REQUIRE(handledMessages[windows_messages::size]);
                        }

                        THEN("The window procedure should have handled the WM_SHOW message") {
                            REQUIRE(handledMessages.contains(windows_messages::show_window));
                            REQUIRE(handledMessages[windows_messages::show_window]);
                        }

                        THEN("The window procedure should have handled the WM_PAINT message") {
                            REQUIRE(handledMessages.contains(windows_messages::paint));
                            REQUIRE(handledMessages[windows_messages::paint]);
                        }

                        THEN("The window procedure should have handled the WM_ACTIVATE message") {
                            REQUIRE(handledMessages.contains(windows_messages::activate));
                            REQUIRE(handledMessages[windows_messages::activate]);
                        }
                    }
                }
            }
        }

        SECTION("Simulating the user closes the window clicking on the X button") {
            GIVEN("A window created with two-phased creation") {
                std::unordered_map<windows_messages, bool> handledMessages{};
                std::unique_ptr<tests::win32_application_window_tss> testWindow{};
                REQUIRE_NOTHROW(
                    testWindow = win32_application_window::two_phase_window_creation<
                        tests::win32_application_window_tss>(
                        L"Test Window",
                        [&handledMessages](
                            const HWND, const windows_messages message, const details::wparam_t,
                            const details::lparam_t) -> std::optional<details::lresult_t> {
                            handledMessages[message] = true;
                            return std::nullopt;
                        },
                        partial_wndclassex{}, defaultCreationParams));

                WHEN("We show the window and simulate the user closing it") {
                    REQUIRE_NOTHROW(testWindow->show());
                    tests::simulate_peek_message_loop(testWindow, true);

                    THEN("The window procedure should have handled the WM_CLOSE message") {
                        REQUIRE(handledMessages.contains(windows_messages::close));
                        REQUIRE(handledMessages[windows_messages::close]);
                    }

                    THEN("The window procedure should have handled the WM_DESTROY message") {
                        REQUIRE(handledMessages.contains(windows_messages::destroy));
                        REQUIRE(handledMessages[windows_messages::destroy]);
                    }
                }
            }
        }
    }
}

TEST_CASE("compute_window_style tests",
          "[tca][engine][platform][win32][win32_application_window]") {
    using namespace tca::engine::platform::win32;

    GIVEN("A resizable, maximizable window is requested") {
        THEN("The computed style is WS_OVERLAPPEDWINDOW, unchanged") {
            REQUIRE(compute_window_style(true, true) ==
                    static_cast<std::uint32_t>(WS_OVERLAPPEDWINDOW));
        }
    }

    GIVEN("A non-resizable, maximizable window is requested") {
        const std::uint32_t style{compute_window_style(false, true)};

        THEN("WS_THICKFRAME is cleared") {
            REQUIRE((style & WS_THICKFRAME) == 0U);
        }

        THEN("WS_MAXIMIZEBOX is kept") {
            REQUIRE((style & WS_MAXIMIZEBOX) != 0U);
        }
    }

    GIVEN("A resizable, non-maximizable window is requested") {
        const std::uint32_t style{compute_window_style(true, false)};

        THEN("WS_THICKFRAME is kept") {
            REQUIRE((style & WS_THICKFRAME) != 0U);
        }

        THEN("WS_MAXIMIZEBOX is cleared") {
            REQUIRE((style & WS_MAXIMIZEBOX) == 0U);
        }
    }

    GIVEN("A non-resizable, non-maximizable window is requested") {
        const std::uint32_t style{compute_window_style(false, false)};

        THEN("Both WS_THICKFRAME and WS_MAXIMIZEBOX are cleared") {
            REQUIRE((style & WS_THICKFRAME) == 0U);
            REQUIRE((style & WS_MAXIMIZEBOX) == 0U);
        }

        THEN("WS_CAPTION, WS_SYSMENU and WS_MINIMIZEBOX are still kept") {
            REQUIRE((style & WS_CAPTION) != 0U);
            REQUIRE((style & WS_SYSMENU) != 0U);
            REQUIRE((style & WS_MINIMIZEBOX) != 0U);
        }
    }
}

TEST_CASE("win32_application_window tests - creationParams style/size are honored",
          "[tca][engine][platform][win32][win32_application_window]") {
    using namespace tca::engine::platform::win32;

    GIVEN("Creation params requesting a non-resizable, non-maximizable window at a fixed size") {
        const partial_window_creation_params creationParams{
            .style = compute_window_style(false, false),
            .size = window_creation_size{640, 480},
            .use_pure_raii = true,
        };

        WHEN("The window is created") {
            win32_application_window testWindow{L"Test Window", partial_wndclassex{},
                                                creationParams};
            const HWND hwnd{testWindow.get_native_handle().get()};
            REQUIRE(hwnd != nullptr);

            THEN("The resulting window style lacks WS_THICKFRAME and WS_MAXIMIZEBOX") {
                const auto actualStyle{
                    static_cast<std::uint32_t>(::GetWindowLongPtr(hwnd, GWL_STYLE))};
                REQUIRE((actualStyle & WS_THICKFRAME) == 0U);
                REQUIRE((actualStyle & WS_MAXIMIZEBOX) == 0U);
            }

            THEN("The resulting window's outer size matches the requested size") {
                // GetWindowRect(), not GetClientRect(): partial_window_creation_params::size is
                // passed straight through to CreateWindowEx() as the outer window size (including
                // the OS-drawn title bar/borders), not the client area size.
                RECT windowRect{};
                REQUIRE(::GetWindowRect(hwnd, &windowRect) != 0);
                REQUIRE(windowRect.right - windowRect.left == 640);
                REQUIRE(windowRect.bottom - windowRect.top == 480);
            }
        }
    }
}
