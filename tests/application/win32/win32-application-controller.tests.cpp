// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "application/win32/win32-application.hpp"

// C++ STL
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

// Platform
#include "platform/win32/win32-application-window.hpp"

// Utilities
#include "utilities/win32/wm-quit-drain.hpp"

// Test doubles
#include "test-doubles/application/application-logic.test-double.hpp"

namespace {

using tca::engine::application::application_controller;
using tca::engine::application::fullscreen_resize;
using tca::engine::application::window_content_size;
using tca::engine::application::window_display_settings;
using tca::engine::application::window_placement;

/**
 * @brief Makes this thread create (and talk to) windows with the given DPI awareness until it goes
 * out of scope. A window keeps the awareness of the thread that created it, so this has to outlive
 * every window created under it.
 */
class scoped_thread_dpi_awareness final {
public:
    explicit scoped_thread_dpi_awareness(const DPI_AWARENESS_CONTEXT context) noexcept
        : m_previous{::SetThreadDpiAwarenessContext(context)} {}

    ~scoped_thread_dpi_awareness() noexcept {
        if (m_previous != nullptr) {
            ::SetThreadDpiAwarenessContext(m_previous);
        }
    }

    scoped_thread_dpi_awareness(const scoped_thread_dpi_awareness&) = delete;
    scoped_thread_dpi_awareness& operator=(const scoped_thread_dpi_awareness&) = delete;

    [[nodiscard]] bool is_applied() const noexcept {
        return m_previous != nullptr;
    }

private:
    DPI_AWARENESS_CONTEXT m_previous{};
};

struct dpi_awareness_case final {
    const char* name{};
    DPI_AWARENESS_CONTEXT context{};
};

/**
 * @brief A win32_application that opens its window like a rendering application's initialize()
 * would (from window_display_settings, via make_window_creation_params()), but without entering
 * the main loop, so that tests can drive the controller the logic is handed step by step and
 * inspect the window in between.
 */
class windowed_application_tss final : public tca::engine::application::win32::win32_application {
public:
    windowed_application_tss() {
        set_logic(std::make_unique<tca::engine::application::test_doubles::application_logic_fake>(
            std::function<bool()>{}, std::function<void()>{},
            [this](application_controller& injected) {
                m_controller = &injected;
            }));
    }

    /**
     * @brief Opens (and shows) the application window with its top-left corner at the given
     * screen position.
     */
    void open_window(const POINT topLeft) {
        tca::engine::platform::win32::partial_window_creation_params params{
            make_window_creation_params()};
        params.position = {.x = topLeft.x, .y = topLeft.y};

        m_window =
            tca::engine::platform::win32::win32_application_window::two_phase_window_creation<
                tca::engine::platform::win32::win32_application_window>(
                TEXT("Controller test window"), tca::engine::platform::win32::partial_wndclassex{},
                params);
        m_window->show();
    }

    [[nodiscard]] HWND get_hwnd() const {
        return m_window->get_native_handle().get();
    }

    [[nodiscard]] application_controller& get_controller() const {
        REQUIRE(m_controller != nullptr);
        return *m_controller;
    }

private:
    application_controller* m_controller{};
};

[[nodiscard]] MONITORINFO get_monitor_info(const HMONITOR monitor) {
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    REQUIRE(::GetMonitorInfo(monitor, &info) != 0);
    return info;
}

[[nodiscard]] HMONITOR get_monitor_of(const HWND hwnd) {
    return ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
}

[[nodiscard]] RECT get_work_area_of(const HWND hwnd) {
    return get_monitor_info(get_monitor_of(hwnd)).rcWork;
}

[[nodiscard]] RECT get_primary_work_area() {
    return get_monitor_info(::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY)).rcWork;
}

[[nodiscard]] RECT get_window_rect(const HWND hwnd) {
    RECT rect{};
    REQUIRE(::GetWindowRect(hwnd, &rect) != 0);
    return rect;
}

[[nodiscard]] SIZE get_client_size(const HWND hwnd) {
    RECT rect{};
    REQUIRE(::GetClientRect(hwnd, &rect) != 0);
    return SIZE{rect.right - rect.left, rect.bottom - rect.top};
}

[[nodiscard]] POINT top_left_inside(const RECT& workArea) {
    constexpr std::int32_t OFFSET{50};
    return POINT{workArea.left + OFFSET, workArea.top + OFFSET};
}

void require_client_size(const HWND hwnd, const window_content_size expected) {
    const SIZE clientSize{get_client_size(hwnd)};
    REQUIRE(clientSize.cx == expected.width);
    REQUIRE(clientSize.cy == expected.height);
}

void require_inside(const RECT& inner, const RECT& outer) {
    REQUIRE(inner.left >= outer.left);
    REQUIRE(inner.top >= outer.top);
    REQUIRE(inner.right <= outer.right);
    REQUIRE(inner.bottom <= outer.bottom);
}

/**
 * @brief Requires the window's outer rectangle to be centred in the given area. Allows one pixel of
 * slack per axis, since an odd leftover cannot be split evenly.
 */
void require_centred_in(const HWND hwnd, const RECT& area) {
    const RECT windowRect{get_window_rect(hwnd)};
    const std::int32_t leftGap{windowRect.left - area.left};
    const std::int32_t rightGap{area.right - windowRect.right};
    const std::int32_t topGap{windowRect.top - area.top};
    const std::int32_t bottomGap{area.bottom - windowRect.bottom};

    REQUIRE(std::abs(leftGap - rightGap) <= 1);
    REQUIRE(std::abs(topGap - bottomGap) <= 1);
}

/**
 * @brief Requires the window to cover the whole of its monitor, which is what the borderless
 * go_fullscreen() does.
 */
void require_covers_monitor(const HWND hwnd) {
    const RECT windowRect{get_window_rect(hwnd)};
    const RECT monitorRect{get_monitor_info(get_monitor_of(hwnd)).rcMonitor};
    REQUIRE(windowRect.left == monitorRect.left);
    REQUIRE(windowRect.top == monitorRect.top);
    REQUIRE(windowRect.right == monitorRect.right);
    REQUIRE(windowRect.bottom == monitorRect.bottom);
}

/**
 * @brief Whether a window with the given client size, in the window's current style, would fit
 * inside its work area at all. Tests needing an exact size use it to skip on unusually small
 * displays rather than fail for a reason that has nothing to do with the code under test.
 */
[[nodiscard]] bool fits_in_work_area(const HWND hwnd, const window_content_size size) {
    RECT outer{0, 0, size.width, size.height};
    const auto style{static_cast<DWORD>(::GetWindowLongPtr(hwnd, GWL_STYLE))};
    const auto exStyle{static_cast<DWORD>(::GetWindowLongPtr(hwnd, GWL_EXSTYLE))};
    if (::AdjustWindowRectExForDpi(&outer, style, FALSE, exStyle, ::GetDpiForWindow(hwnd)) == 0) {
        return false;
    }

    const RECT workArea{get_work_area_of(hwnd)};
    return (outer.right - outer.left) <= (workArea.right - workArea.left) &&
           (outer.bottom - outer.top) <= (workArea.bottom - workArea.top);
}

/**
 * @brief Finds a monitor other than the primary one, if the machine has any.
 */
[[nodiscard]] std::optional<HMONITOR> find_secondary_monitor() {
    std::vector<HMONITOR> monitors{};
    ::EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM userData) -> BOOL {
            reinterpret_cast<std::vector<HMONITOR>*>(userData)->push_back(monitor);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&monitors));

    for (const HMONITOR monitor : monitors) {
        if ((get_monitor_info(monitor).dwFlags & MONITORINFOF_PRIMARY) == 0U) {
            return monitor;
        }
    }

    return std::nullopt;
}

} // namespace

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

template <>
struct Catch::StringMaker<fullscreen_resize> {
    static std::string convert(const fullscreen_resize fullscreen) {
        switch (fullscreen) {
            case fullscreen_resize::ignore:
                return "ignore";
            case fullscreen_resize::defer_until_windowed:
                return "defer_until_windowed";
        }
        return "unknown";
    }
};

// Every leaf section (THEN, or a GENERATE value) re-runs the setup above it, which here means
// opening a new window. To keep the number of windows down, each scenario drives one window through
// all of its variations and checks them in a single leaf rather than one leaf per check.

TEST_CASE("win32_application controller: is_fullscreen()",
          "[tca][engine][application][win32][win32_application][application_controller]") {
    tests::utilities::drain_wm_quit_messages();

    SECTION("An application with no window") {
        windowed_application_tss app{};
        application_controller& controller{app.get_controller()};

        THEN("It is not fullscreen, even after go_fullscreen(), since there is no window") {
            REQUIRE_FALSE(controller.is_fullscreen());
            controller.go_fullscreen();
            REQUIRE_FALSE(controller.is_fullscreen());
        }
    }

    SECTION("An application with a window") {
        windowed_application_tss app{};
        app.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{app.get_controller()};

        THEN("is_fullscreen() tracks every transition between windowed and fullscreen") {
            REQUIRE_FALSE(controller.is_fullscreen());

            controller.go_windowed();
            REQUIRE_FALSE(controller.is_fullscreen());

            controller.go_fullscreen();
            REQUIRE(controller.is_fullscreen());
            require_covers_monitor(app.get_hwnd());

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
}

TEST_CASE("win32_application controller: go_fullscreen() and go_windowed()",
          "[tca][engine][application][win32][win32_application][application_controller]") {
    tests::utilities::drain_wm_quit_messages();

    SECTION("An application with a window") {
        windowed_application_tss app{};
        app.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{app.get_controller()};
        const HWND hwnd{app.get_hwnd()};

        // Somewhere that is neither the top-left corner of the screen nor where the window was
        // opened, so that a restore to either of those would show.
        controller.set_content_size({.width = 400, .height = 300},
                                    {.placement = window_placement::center});
        const RECT windowedRect{get_window_rect(hwnd)};

        WHEN("go_windowed() is requested while already windowed") {
            controller.go_windowed();

            THEN("The window does not move or change size") {
                REQUIRE_FALSE(controller.is_fullscreen());
                const RECT rect{get_window_rect(hwnd)};
                REQUIRE(rect.left == windowedRect.left);
                REQUIRE(rect.top == windowedRect.top);
                REQUIRE(rect.right == windowedRect.right);
                REQUIRE(rect.bottom == windowedRect.bottom);
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
                    require_covers_monitor(hwnd);

                    controller.go_windowed();
                    REQUIRE_FALSE(controller.is_fullscreen());
                    const RECT rect{get_window_rect(hwnd)};
                    REQUIRE(rect.left == windowedRect.left);
                    REQUIRE(rect.top == windowedRect.top);
                    REQUIRE(rect.right == windowedRect.right);
                    REQUIRE(rect.bottom == windowedRect.bottom);
                    require_client_size(hwnd, {.width = 400, .height = 300});
                }
            }
        }
    }
}

TEST_CASE("win32_application controller: set_content_size()",
          "[tca][engine][application][win32][win32_application][application_controller]") {
    tests::utilities::drain_wm_quit_messages();

    SECTION("An application with no window") {
        windowed_application_tss app{};
        application_controller& controller{app.get_controller()};

        THEN("The request is ignored without failing") {
            REQUIRE_NOTHROW(controller.set_content_size({.width = 640, .height = 360}, {}));
            REQUIRE_NOTHROW(controller.set_content_size({.width = 640, .height = 360},
                                                        {.placement = window_placement::center}));
            REQUIRE_FALSE(controller.is_fullscreen());
        }
    }

    SECTION("An application with a window, in windowed mode") {
        // The client area has to come out at exactly the requested size whatever DPI handling the
        // window was created with: DPI-unaware windows are measured in (virtualized) logical
        // pixels, per-monitor aware ones in physical pixels, and the chrome around the client
        // area is a different size at every DPI.
        const dpi_awareness_case dpiCase{
            GENERATE(dpi_awareness_case{"unaware", DPI_AWARENESS_CONTEXT_UNAWARE},
                     dpi_awareness_case{"system aware", DPI_AWARENESS_CONTEXT_SYSTEM_AWARE},
                     dpi_awareness_case{"per-monitor aware v2",
                                        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2})};
        const bool resizable{GENERATE(true, false)};

        CAPTURE(dpiCase.name, resizable);

        const scoped_thread_dpi_awareness dpiAwareness{dpiCase.context};
        REQUIRE(dpiAwareness.is_applied());

        windowed_application_tss app{};
        app.set_window_display_settings(window_display_settings{
            .content_size = {.width = 400, .height = 300},
            .resizable = resizable,
            .maximizable = resizable,
        });
        app.open_window(top_left_inside(get_primary_work_area()));

        application_controller& controller{app.get_controller()};
        const HWND hwnd{app.get_hwnd()};
        const HMONITOR initialMonitor{get_monitor_of(hwnd)};

        WHEN("The content size is changed several times in a row, like a window scale option") {
            THEN("Every step produces exactly the requested size, placed as requested, on screen") {
                // Grows, shrinks back, and ends on an odd size the chrome cannot hide rounding in.
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
                        if (!fits_in_work_area(hwnd, requestedSize)) {
                            continue;
                        }

                        const RECT windowRectBefore{get_window_rect(hwnd)};
                        controller.set_content_size(requestedSize, {.placement = placement});

                        const RECT windowRectAfter{get_window_rect(hwnd)};
                        const RECT workArea{get_work_area_of(hwnd)};
                        require_client_size(hwnd, requestedSize);
                        REQUIRE_FALSE(controller.is_fullscreen());
                        REQUIRE(get_monitor_of(hwnd) == initialMonitor);
                        require_inside(windowRectAfter, workArea);

                        if (placement == window_placement::center) {
                            require_centred_in(hwnd, workArea);
                        } else if (windowRectBefore.left +
                                           (windowRectAfter.right - windowRectAfter.left) <=
                                       workArea.right &&
                                   windowRectBefore.top +
                                           (windowRectAfter.bottom - windowRectAfter.top) <=
                                       workArea.bottom) {
                            // The top-left corner only has to stay put when the window still fits
                            // from there; otherwise it is moved back inside the work area.
                            REQUIRE(windowRectAfter.left == windowRectBefore.left);
                            REQUIRE(windowRectAfter.top == windowRectBefore.top);
                        }
                    }
                }

                // The window's style is left alone, so a fixed-size window stays fixed-size.
                const auto style{static_cast<DWORD>(::GetWindowLongPtr(hwnd, GWL_STYLE))};
                REQUIRE(((style & WS_THICKFRAME) != 0U) == resizable);
                REQUIRE(((style & WS_MAXIMIZEBOX) != 0U) == resizable);
            }
        }

        WHEN("A content size larger than the work area is requested") {
            THEN(
                "The window is clamped to the work area, on its monitor, with a smaller but "
                "non-empty client area") {
                const RECT workArea{get_work_area_of(hwnd)};
                const window_content_size hugeSize{.width = (workArea.right - workArea.left) * 3,
                                                   .height = (workArea.bottom - workArea.top) * 3};

                for (const window_placement placement :
                     {window_placement::keep_top_left, window_placement::center}) {
                    CAPTURE(placement);
                    controller.set_content_size(hugeSize, {.placement = placement});

                    require_inside(get_window_rect(hwnd), get_work_area_of(hwnd));
                    REQUIRE(get_monitor_of(hwnd) == initialMonitor);

                    const SIZE clientSize{get_client_size(hwnd)};
                    REQUIRE(clientSize.cx > 0);
                    REQUIRE(clientSize.cy > 0);
                    REQUIRE(clientSize.cx < hugeSize.width);
                    REQUIRE(clientSize.cy < hugeSize.height);
                }
            }
        }

        WHEN("Empty content sizes are requested") {
            THEN("Each request is ignored and the window keeps its rectangle") {
                const RECT before{get_window_rect(hwnd)};

                for (const window_content_size emptySize : {
                         window_content_size{.width = 0,   .height = 0  },
                         window_content_size{.width = 0,   .height = 300},
                         window_content_size{.width = 400, .height = -1 }
                }) {
                    CAPTURE(emptySize.width, emptySize.height);
                    controller.set_content_size(emptySize, {.placement = window_placement::center});

                    const RECT after{get_window_rect(hwnd)};
                    REQUIRE(after.left == before.left);
                    REQUIRE(after.top == before.top);
                    REQUIRE(after.right == before.right);
                    REQUIRE(after.bottom == before.bottom);
                }
            }
        }

        WHEN("The window sits near the bottom-right of the work area and is made bigger") {
            const RECT workArea{get_work_area_of(hwnd)};
            const RECT windowRect{get_window_rect(hwnd)};
            const std::int32_t windowWidth{windowRect.right - windowRect.left};
            const std::int32_t windowHeight{windowRect.bottom - windowRect.top};
            REQUIRE(::SetWindowPos(hwnd, nullptr, workArea.right - windowWidth,
                                   workArea.bottom - windowHeight, 0, 0,
                                   SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != 0);

            constexpr window_content_size BIGGER_SIZE{.width = 640, .height = 480};
            if (!fits_in_work_area(hwnd, BIGGER_SIZE)) {
                SKIP("The requested size does not fit this display's work area");
            }

            controller.set_content_size(BIGGER_SIZE, {});

            THEN("It gets exactly the requested size and is moved back inside the work area") {
                require_client_size(hwnd, BIGGER_SIZE);
                require_inside(get_window_rect(hwnd), get_work_area_of(hwnd));
                REQUIRE(get_monitor_of(hwnd) == initialMonitor);
            }
        }
    }

    SECTION("An application with a window, in fullscreen mode") {
        windowed_application_tss app{};
        app.set_window_display_settings(window_display_settings{
            .content_size = {.width = 400, .height = 300}
        });
        app.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{app.get_controller()};
        const HWND hwnd{app.get_hwnd()};

        const SIZE clientSizeBeforeFullscreen{get_client_size(hwnd)};

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
                require_covers_monitor(hwnd);

                controller.go_windowed();
                REQUIRE_FALSE(controller.is_fullscreen());
                const SIZE clientSizeAfter{get_client_size(hwnd)};
                REQUIRE(clientSizeAfter.cx == clientSizeBeforeFullscreen.cx);
                REQUIRE(clientSizeAfter.cy == clientSizeBeforeFullscreen.cy);
            }
        }

        WHEN("A new content size is requested with fullscreen_resize::defer_until_windowed") {
            controller.set_content_size(REQUESTED_SIZE,
                                        {.fullscreen = fullscreen_resize::defer_until_windowed});

            THEN("The window stays fullscreen, and the size is applied once, by go_windowed()") {
                REQUIRE(controller.is_fullscreen());
                require_covers_monitor(hwnd);

                controller.go_windowed();
                REQUIRE_FALSE(controller.is_fullscreen());
                require_client_size(hwnd, REQUESTED_SIZE);
                require_inside(get_window_rect(hwnd), get_work_area_of(hwnd));

                // Going fullscreen and back with no new request keeps the size the window had.
                controller.go_fullscreen();
                controller.go_windowed();
                require_client_size(hwnd, REQUESTED_SIZE);
            }
        }

        WHEN("A centred content size is deferred") {
            controller.set_content_size(REQUESTED_SIZE,
                                        {.placement = window_placement::center,
                                         .fullscreen = fullscreen_resize::defer_until_windowed});

            THEN("The window stays fullscreen, and the placement is applied by go_windowed() too") {
                REQUIRE(controller.is_fullscreen());
                require_covers_monitor(hwnd);

                controller.go_windowed();
                require_client_size(hwnd, REQUESTED_SIZE);
                require_centred_in(hwnd, get_work_area_of(hwnd));
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
                require_client_size(hwnd, REQUESTED_SIZE);
            }
        }
    }
}

TEST_CASE("win32_application controller: center_window()",
          "[tca][engine][application][win32][win32_application][application_controller]") {
    tests::utilities::drain_wm_quit_messages();

    SECTION("An application with no window") {
        windowed_application_tss app{};

        THEN("The request is ignored without failing") {
            REQUIRE_NOTHROW(app.get_controller().center_window());
        }
    }

    SECTION("An application with a window, in windowed mode") {
        const dpi_awareness_case dpiCase{
            GENERATE(dpi_awareness_case{"unaware", DPI_AWARENESS_CONTEXT_UNAWARE},
                     dpi_awareness_case{"per-monitor aware v2",
                                        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2})};
        const bool resizable{GENERATE(true, false)};
        CAPTURE(dpiCase.name, resizable);

        const scoped_thread_dpi_awareness dpiAwareness{dpiCase.context};
        REQUIRE(dpiAwareness.is_applied());

        windowed_application_tss app{};
        app.set_window_display_settings(window_display_settings{
            .content_size = {.width = 400, .height = 300},
            .resizable = resizable,
            .maximizable = resizable,
        });
        app.open_window(top_left_inside(get_primary_work_area()));

        application_controller& controller{app.get_controller()};
        const HWND hwnd{app.get_hwnd()};
        const HMONITOR initialMonitor{get_monitor_of(hwnd)};

        WHEN("center_window() is requested") {
            const SIZE clientSizeBefore{get_client_size(hwnd)};
            controller.center_window();

            THEN(
                "The window is centred on its monitor, keeping its size, and does not move when "
                "centred again") {
                require_centred_in(hwnd, get_work_area_of(hwnd));
                REQUIRE(get_monitor_of(hwnd) == initialMonitor);
                REQUIRE_FALSE(controller.is_fullscreen());

                const SIZE clientSizeAfter{get_client_size(hwnd)};
                REQUIRE(clientSizeAfter.cx == clientSizeBefore.cx);
                REQUIRE(clientSizeAfter.cy == clientSizeBefore.cy);

                const RECT windowRectBefore{get_window_rect(hwnd)};
                controller.center_window();
                const RECT windowRectAfter{get_window_rect(hwnd)};
                REQUIRE(windowRectAfter.left == windowRectBefore.left);
                REQUIRE(windowRectAfter.top == windowRectBefore.top);
            }
        }

        WHEN("The window is resized and then centred, like a game applying a window scale") {
            THEN("Each time it has the requested client area and is centred in the work area") {
                for (const window_content_size requestedSize : {
                         window_content_size{.width = 320, .height = 180},
                         window_content_size{.width = 960, .height = 540}
                }) {
                    CAPTURE(requestedSize.width, requestedSize.height);
                    if (!fits_in_work_area(hwnd, requestedSize)) {
                        continue;
                    }

                    controller.set_content_size(requestedSize, {});
                    controller.center_window();

                    require_client_size(hwnd, requestedSize);
                    require_centred_in(hwnd, get_work_area_of(hwnd));
                }
            }
        }

        WHEN("The window is centred after returning from fullscreen") {
            controller.go_fullscreen();
            controller.go_windowed();
            controller.center_window();

            THEN("The window is centred in the work area of its monitor") {
                REQUIRE_FALSE(controller.is_fullscreen());
                require_centred_in(hwnd, get_work_area_of(hwnd));
            }
        }
    }

    SECTION("An application with a window, in fullscreen mode") {
        windowed_application_tss app{};
        app.open_window(top_left_inside(get_primary_work_area()));
        application_controller& controller{app.get_controller()};

        controller.go_fullscreen();
        REQUIRE(controller.is_fullscreen());

        WHEN("center_window() is requested") {
            controller.center_window();

            THEN("The window stays fullscreen, covering its monitor") {
                REQUIRE(controller.is_fullscreen());
                require_covers_monitor(app.get_hwnd());
            }
        }
    }
}

TEST_CASE("win32_application controller: window controls on a secondary monitor",
          "[tca][engine][application][win32][win32_application][application_controller]"
          "[multi-monitor]") {
    tests::utilities::drain_wm_quit_messages();

    const scoped_thread_dpi_awareness dpiAwareness{DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2};
    REQUIRE(dpiAwareness.is_applied());

    const std::optional<HMONITOR> secondaryMonitor{find_secondary_monitor()};
    if (!secondaryMonitor.has_value()) {
        SKIP("This machine has a single monitor");
    }

    const RECT secondaryWorkArea{get_monitor_info(*secondaryMonitor).rcWork};

    windowed_application_tss app{};
    app.set_window_display_settings(window_display_settings{
        .content_size = {.width = 400, .height = 300}
    });
    app.open_window(top_left_inside(secondaryWorkArea));

    application_controller& controller{app.get_controller()};
    const HWND hwnd{app.get_hwnd()};
    REQUIRE(get_monitor_of(hwnd) == *secondaryMonitor);

    constexpr window_content_size REQUESTED_SIZE{.width = 640, .height = 360};
    if (!fits_in_work_area(hwnd, REQUESTED_SIZE)) {
        SKIP("The requested size does not fit the secondary monitor's work area");
    }

    // Every step below works on the secondary monitor's work area, never the primary one's.

    // set_content_size() keeps the window on the secondary monitor.
    controller.set_content_size(REQUESTED_SIZE, {});
    REQUIRE(get_monitor_of(hwnd) == *secondaryMonitor);
    require_client_size(hwnd, REQUESTED_SIZE);
    require_inside(get_window_rect(hwnd), secondaryWorkArea);

    // A centred resize centres the window on the secondary monitor.
    controller.set_content_size({.width = 400, .height = 300},
                                {.placement = window_placement::center});
    REQUIRE(get_monitor_of(hwnd) == *secondaryMonitor);
    require_client_size(hwnd, {.width = 400, .height = 300});
    require_centred_in(hwnd, secondaryWorkArea);

    // A size larger than the secondary monitor's work area is clamped to it.
    controller.set_content_size({.width = (secondaryWorkArea.right - secondaryWorkArea.left) * 3,
                                 .height = (secondaryWorkArea.bottom - secondaryWorkArea.top) * 3},
                                {});
    REQUIRE(get_monitor_of(hwnd) == *secondaryMonitor);
    require_inside(get_window_rect(hwnd), secondaryWorkArea);

    // center_window() centres the window on the secondary monitor.
    controller.set_content_size(REQUESTED_SIZE, {});
    controller.center_window();
    REQUIRE(get_monitor_of(hwnd) == *secondaryMonitor);
    require_centred_in(hwnd, secondaryWorkArea);

    // go_fullscreen() covers the secondary monitor, not the primary one, and go_windowed() brings
    // the window back to where it was on the secondary monitor, still centred.
    controller.go_fullscreen();
    REQUIRE(controller.is_fullscreen());
    REQUIRE(get_monitor_of(hwnd) == *secondaryMonitor);
    require_covers_monitor(hwnd);

    controller.go_windowed();
    REQUIRE_FALSE(controller.is_fullscreen());
    REQUIRE(get_monitor_of(hwnd) == *secondaryMonitor);
    require_client_size(hwnd, REQUESTED_SIZE);
    require_centred_in(hwnd, secondaryWorkArea);

    // A centred size deferred while fullscreen is applied on the secondary monitor too.
    controller.go_fullscreen();
    controller.set_content_size({.width = 400, .height = 300},
                                {.placement = window_placement::center,
                                 .fullscreen = fullscreen_resize::defer_until_windowed});
    controller.go_windowed();
    REQUIRE(get_monitor_of(hwnd) == *secondaryMonitor);
    require_client_size(hwnd, {.width = 400, .height = 300});
    require_centred_in(hwnd, secondaryWorkArea);
}
