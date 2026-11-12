// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT

#include "application/win32/win32-application.hpp"

// C++ STL
#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string_view>
#include <utility>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <wil/resource.h>
#include <gsl/gsl>

// Platform module
#include "platform/engine-type-traits.hpp"
#include "platform/win32/windows-type-traits.hpp"

#include "input/input-frame.hpp"
#include "rendering/gui/imgui/imgui-theme.hpp"

#include "application/application-logger.hpp"

namespace tca::engine::application::win32 {

namespace {
// The name an application gets when the game never set one.
constexpr std::string_view DEFAULT_APP_NAME{"TCA Application"};
} // namespace

/**
 * @brief Custom application controller implementation for Win32 applications.
 * This will post a WM_CLOSE message to the application window when the exit() method is called.
 */
class win32_application_controller final : public application_controller {
public:
    explicit win32_application_controller(
        std::function<HWND()> windowHandleGetter,
        std::function<const window_display_settings&()> settingsGetter,
        std::function<void(theming::theme)> themeSetter, std::stop_token& stopToken)
        : m_windowHandleGetter{std::move(windowHandleGetter)},
          m_settingsGetter{std::move(settingsGetter)},
          m_themeSetter{std::move(themeSetter)} {
        stopToken = m_stopSource.get_token();
    }

    void exit() override {
        Expects(m_windowHandleGetter != nullptr && "Window handle getter function cannot be null.");

        // The architecture of the application doesn't guarantee that the application has a window,
        // so we need to check if the window handle is valid before posting the WM_CLOSE message.
        if (const HWND windowHandle{m_windowHandleGetter()}; windowHandle != nullptr) {
            get_application_logger()->info(
                "Manual exit signal received. Posting WM_CLOSE message to the application window.");
            ::PostMessage(windowHandle, WM_CLOSE, 0, 0);
        } else {
            get_application_logger()->info(
                "Manual exit signal received. Requesting application with no window to exit.");
            m_stopSource.request_stop();
        }
    }

    void go_fullscreen() override {
        const HWND windowHandle{m_windowHandleGetter()};
        if (windowHandle == nullptr) {
            get_application_logger()->warn(
                "Cannot go fullscreen because the application window is not provided. Ignoring "
                "the request.");
            return;
        }

        // Remember where the window is and how big it is, so that go_windowed() can put it back
        // there.
        //
        // We only capture it when the window is currently in a windowed style; if it is already
        // fullscreen/borderless, we must NOT overwrite the stored rectangle, otherwise returning
        // to windowed mode would restore to the fullscreen one.
        if (RECT rect{}; is_windowed() && ::GetWindowRect(windowHandle, &rect)) {
            m_windowedRect = rect;
        }

        // The window covers the whole of the monitor it is on, which is not necessarily the
        // primary one.
        RECT monitorRect{0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN)};
        if (MONITORINFO monitorInfo{.cbSize = sizeof(MONITORINFO)}; ::GetMonitorInfo(
                ::MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
            monitorRect = monitorInfo.rcMonitor;
        } else {
            get_application_logger()->warn(
                "Cannot retrieve the monitor the window is on. Going fullscreen on the primary "
                "monitor.");
        }

        get_application_logger()->info("Setting the application to fullscreen.");
        ::SetWindowLongPtr(windowHandle, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        ::SetWindowPos(windowHandle, HWND_TOP, monitorRect.left, monitorRect.top,
                       monitorRect.right - monitorRect.left, monitorRect.bottom - monitorRect.top,
                       SWP_FRAMECHANGED);
    }

    void go_windowed() override {
        const HWND windowHandle{m_windowHandleGetter()};
        if (windowHandle == nullptr) {
            get_application_logger()->warn(
                "Cannot go windowed because the application window is not provided. Ignoring "
                "the request.");
            return;
        }

        // Already windowed: restyling and repositioning the window would only move it around.
        if (is_windowed()) {
            return;
        }

        const window_display_settings& settings{m_settingsGetter()};

        get_application_logger()->info("Setting the application to windowed.");
        ::SetWindowLongPtr(
            windowHandle, GWL_STYLE,
            platform::win32::compute_window_style(settings.resizable, settings.maximizable) |
                WS_VISIBLE);

        // The window goes back where it was before going fullscreen, as long as that is still on
        // a monitor (it may have been disconnected in the meantime). Otherwise it gets the
        // configured content size, centred on the monitor it is on now.
        if (m_windowedRect.has_value() &&
            ::MonitorFromRect(&*m_windowedRect, MONITOR_DEFAULTTONULL) != nullptr) {
            const RECT& rect{*m_windowedRect};
            ::SetWindowPos(windowHandle, HWND_TOP, rect.left, rect.top, rect.right - rect.left,
                           rect.bottom - rect.top, SWP_FRAMECHANGED);
        } else {
            get_application_logger()->warn(
                "The window's position from before going fullscreen is not available. Using the "
                "configured window display settings' content size ({}x{}), centred.",
                settings.content_size.width, settings.content_size.height);
            ::SetWindowPos(windowHandle, HWND_TOP, 0, 0, 0, 0,
                           SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
            apply_content_size(windowHandle, settings.content_size, window_placement::center);
        }

        // A content size requested while fullscreen with fullscreen_resize::defer_until_windowed
        // is applied once, now that the window has a content size of its own again. Applying it
        // after the restore means keep_top_left keeps the top-left corner from before going
        // fullscreen.
        if (const auto deferred{std::exchange(m_deferredContentSize, std::nullopt)};
            deferred.has_value()) {
            apply_content_size(windowHandle, deferred->contentSize, deferred->placement);
        }
    }

    void use_theme(theming::theme newTheme) final {
        Expects(m_themeSetter != nullptr && "Theme setter function cannot be null.");

        // Routed back through the application rather than only setting the ambient theme here: a
        // theme is more than what widgets resolve against. It also decides the ImGui style and the
        // colour the window is cleared to, and the application is the one place that knows how to
        // push it into all three (see win32_application::set_theme()).
        m_themeSetter(std::move(newTheme));
    }

    void set_content_size(window_content_size contentSize, content_size_options options) override {
        const HWND windowHandle{m_windowHandleGetter()};
        if (windowHandle == nullptr) {
            get_application_logger()->warn(
                "Cannot set the content size because the application window is not provided. "
                "Ignoring the request.");
            return;
        }

        if (contentSize.is_empty()) {
            get_application_logger()->warn(
                "Cannot set the content size to {}x{} because it is empty. Ignoring the request.",
                contentSize.width, contentSize.height);
            return;
        }

        // A fullscreen window covers its whole monitor, so there is no content size to change:
        // the request is either dropped or kept for the next go_windowed(). An ignored request
        // leaves an earlier deferred one in place.
        if (is_fullscreen()) {
            if (options.fullscreen == fullscreen_resize::defer_until_windowed) {
                get_application_logger()->info(
                    "The application is fullscreen. Deferring the content size {}x{} until it goes "
                    "windowed.",
                    contentSize.width, contentSize.height);
                m_deferredContentSize = deferred_content_size{contentSize, options.placement};
            } else {
                get_application_logger()->info(
                    "The application is fullscreen. Ignoring the content size {}x{}.",
                    contentSize.width, contentSize.height);
            }
            return;
        }

        apply_content_size(windowHandle, contentSize, options.placement);
    }

    bool is_fullscreen() const noexcept override {
        return m_windowHandleGetter() != nullptr && !is_windowed();
    }

    void center_window() override {
        const HWND windowHandle{m_windowHandleGetter()};
        if (windowHandle == nullptr) {
            get_application_logger()->warn(
                "Cannot center the window because the application window is not provided. "
                "Ignoring the request.");
            return;
        }

        if (is_fullscreen()) {
            get_application_logger()->warn(
                "Skipping window centering request since the window is fullscreen.");
            return;
        }

        RECT clientRect{};
        if (::GetClientRect(windowHandle, &clientRect) == 0) {
            get_application_logger()->warn(
                "Cannot center the window because its content size could not be measured. "
                "Ignoring the request.");
            return;
        }

        apply_content_size(windowHandle,
                           window_content_size{.width = clientRect.right - clientRect.left,
                                               .height = clientRect.bottom - clientRect.top},
                           window_placement::center);
    }

private:
    std::function<HWND()> m_windowHandleGetter{};
    std::function<const window_display_settings&()> m_settingsGetter{};
    std::function<void(theming::theme)> m_themeSetter{};
    std::stop_source m_stopSource{};

    // The window's outer rectangle, in screen coordinates, the last time go_fullscreen() was
    // called while windowed. go_windowed() restores it.
    std::optional<RECT> m_windowedRect{};

    struct deferred_content_size final {
        window_content_size contentSize{};
        window_placement placement{};
    };

    // A set_content_size() request made while fullscreen with
    // fullscreen_resize::defer_until_windowed, applied (once) by the next go_windowed().
    std::optional<deferred_content_size> m_deferredContentSize{};

    bool is_windowed() const noexcept {
        const HWND windowHandle{m_windowHandleGetter()};
        const auto currentStyle{
            static_cast<std::uint32_t>(::GetWindowLongPtr(windowHandle, GWL_STYLE))};
        return (currentStyle & WS_OVERLAPPEDWINDOW) != 0U;
    }

    static void apply_content_size(const HWND windowHandle, const window_content_size contentSize,
                                   const window_placement placement) {
        // The frame around the content area depends on the window's styles and on the DPI of the
        // monitor it is on, so the outer size is computed with AdjustWindowRectExForDpi() rather
        // than AdjustWindowRect(), which assumes the system DPI and no extended styles.
        const auto style{static_cast<DWORD>(::GetWindowLongPtr(windowHandle, GWL_STYLE))};
        const auto exStyle{static_cast<DWORD>(::GetWindowLongPtr(windowHandle, GWL_EXSTYLE))};
        RECT frameRect{0, 0, contentSize.width, contentSize.height};
        RECT windowRect{};
        MONITORINFO monitorInfo{.cbSize = sizeof(MONITORINFO)};
        if (::AdjustWindowRectExForDpi(&frameRect, style, FALSE, exStyle,
                                       ::GetDpiForWindow(windowHandle)) == 0 ||
            ::GetWindowRect(windowHandle, &windowRect) == 0 ||
            ::GetMonitorInfo(::MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST),
                             &monitorInfo) == 0) {
            get_application_logger()->warn(
                "Cannot set the content size to {}x{} because the window or its monitor could not "
                "be measured. Ignoring the request.",
                contentSize.width, contentSize.height);
            return;
        }

        // The window has to stay fully inside its monitor's work area, so a size too large for it
        // is clamped, leaving the content area smaller than requested.
        const RECT& workArea{monitorInfo.rcWork};
        const LONG workWidth{workArea.right - workArea.left};
        const LONG workHeight{workArea.bottom - workArea.top};
        const LONG width{std::min(frameRect.right - frameRect.left, workWidth)};
        const LONG height{std::min(frameRect.bottom - frameRect.top, workHeight)};

        POINT topLeft{};
        switch (placement) {
            case window_placement::keep_top_left:
                // The top-left corner only moves if the resized window would otherwise hang off
                // the work area.
                topLeft = {std::clamp(windowRect.left, workArea.left, workArea.right - width),
                           std::clamp(windowRect.top, workArea.top, workArea.bottom - height)};
                break;
            case window_placement::center:
                topLeft = {workArea.left + ((workWidth - width) / 2),
                           workArea.top + ((workHeight - height) / 2)};
                break;
        }

        ::SetWindowPos(windowHandle, nullptr, topLeft.x, topLeft.y, width, height,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    }
};

application& win32_application::set_theme(theming::theme newTheme) {
    m_theme = newTheme;

    // Also makes it the ambient theme every widget resolves against, and mirrors it into the ImGui
    // style when a context already exists, so that a theme set after startup is not left applying
    // to the clear colour alone.
    imgui::use_theme(std::move(newTheme));
    on_theme_changed();
    return *this;
}

application& win32_application::set_window_display_settings(window_display_settings&& settings) {
    m_windowDisplaySettings = std::move(settings);
    return *this;
}

application& win32_application::set_app_name(std::string_view name) {
    m_appName = platform::win32::type_traits::to_win32_string(name);
    return *this;
}

application& win32_application::set_company_name(std::string_view name) {
    m_companyName = platform::win32::type_traits::to_win32_string(name);
    return *this;
}

application& win32_application::set_logic(std::unique_ptr<application_logic> logic) {
    Expects(logic != nullptr && "Logic object cannot be null.");
    m_logic = std::move(logic);

    if (!m_controller) {
        // Create a new win32 application controller if it hasn't been created yet.
        m_controller = std::make_unique<win32_application_controller>(
            [this]() -> HWND {
                if (!m_window || !m_window->get_native_handle().is_valid()) {
                    return nullptr;
                }
                return m_window->get_native_handle().get();
            },
            // A live reference rather than a captured-by-value snapshot, so this reflects
            // whichever window_display_settings was last set on this application regardless of
            // whether set_window_display_settings() is called before or after set_logic().
            [this]() -> const window_display_settings& {
                return m_windowDisplaySettings;
            },
            // set_theme() is what carries a theme all the way through: the stored theme the clear
            // colour is resolved from, the ImGui style, and on_theme_changed() so the rendering
            // manager picks the new clear colour up. Going through it keeps the controller's
            // use_theme() and the builder's set_theme() from drifting into two different meanings
            // of "apply a theme".
            [this](theming::theme newTheme) {
                set_theme(std::move(newTheme));
            },
            m_stopToken);
    }

    // Inject the application controller to client application logic so that
    // they can control application lifetime easily.
    m_logic->set_app_controller(*m_controller);

    // Set the input view.
    m_logic->set_input(get_input());

    return *this;
}

platform::platform_preferred_string_view_t win32_application::get_app_name() const noexcept {
    return m_appName;
}

platform::win32::partial_window_creation_params win32_application::make_window_creation_params()
    const {
    const std::uint32_t style{platform::win32::compute_window_style(
        m_windowDisplaySettings.resizable, m_windowDisplaySettings.maximizable)};

    // window_display_settings::content_size describes the desired CLIENT (content) area size, but
    // partial_window_creation_params::size feeds straight into CreateWindowEx()'s nWidth/nHeight,
    // which describe the OUTER window size (title bar and borders included) - passing content_size
    // through unchanged would silently make the actual content area smaller than requested, by
    // however much space the OS-drawn chrome takes up. AdjustWindowRectEx() computes how much
    // bigger the outer window needs to be, for this style, to yield exactly the requested client
    // area.
    RECT windowRect{0, 0, m_windowDisplaySettings.content_size.width,
                    m_windowDisplaySettings.content_size.height};
    platform::win32::window_creation_size size{m_windowDisplaySettings.content_size.width,
                                               m_windowDisplaySettings.content_size.height};
    if (::AdjustWindowRectEx(&windowRect, style, FALSE, 0)) {
        size = platform::win32::window_creation_size{windowRect.right - windowRect.left,
                                                     windowRect.bottom - windowRect.top};
    } else {
        get_application_logger()->warn(
            "Failed to compute the outer window size needed to fit the requested content size "
            "({}x{}); the window's content area may end up smaller than requested.",
            m_windowDisplaySettings.content_size.width,
            m_windowDisplaySettings.content_size.height);
    }

    return platform::win32::partial_window_creation_params{
        .style = style,
        .size = size,
    };
}

application_logic& win32_application::get_logic() const {
    Expects(m_logic != nullptr && "Logic object has not been set.");

    return *m_logic;
}

void win32_application::initialize() {}

rendering::image_render_queue& win32_application::get_or_create_image_render_queue() {
    return m_nullImageRenderQueue;
}

void win32_application::close() {
    // By default, the close() method doesn't do anything, but derived classes can override it
    // to perform any necessary cleanup tasks when the application is closing.
}

bool win32_application::pre_logic_tick() {
    // By default, the pre_logic_tick() method doesn't do anything, but derived classes can
    // override it to perform any necessary tasks before the application logic is executed at
    // each frame.
    return true;
}

void win32_application::post_logic_tick() {
    // By default, the post_logic_tick() method doesn't do anything, but derived classes can
    // override it to perform any necessary tasks after the application logic is executed at
    // each frame.
}

input::input_collector& win32_application::get_input_collector() noexcept {
    return m_inputCollector;
}

input::input_view win32_application::get_input() noexcept {
    return input::input_view{m_inputFrame};
}

void win32_application::run() {
    // If the name of the application is empty, we give it a default name. This happens before the
    // log starts, since the name is what names the log folder: without it, a nameless application
    // would write no log file at all.
    const bool usesDefaultName{m_appName.empty()};
    if (usesDefaultName) {
        m_appName = platform::win32::type_traits::to_win32_string(DEFAULT_APP_NAME);
    }

    begin_application_log(platform::win32::type_traits::from_win32_string_view(m_appName),
                          platform::win32::type_traits::from_win32_string_view(m_companyName));

    const auto logger{get_application_logger()};

    if (m_logic == nullptr) {
        logger->critical(
            "Cannot run the application because no application logic has been set. Please set "
            "the "
            "application logic before running the application.");
        throw std::runtime_error(
            "Application logic has not been set. Please set the application logic before "
            "running "
            "the application.");
    }

    logger->info("TCA Engine: Welcome! Begin logging");

    if (usesDefaultName) {
        logger->warn("Application name has not been set. Using default name '{}'.",
                     DEFAULT_APP_NAME);
    }

    logger->info("Starting application '{}'",
                 platform::win32::type_traits::from_win32_string_view(m_appName));

    // Initialize the application.
    // This should create rendering resources, application window, and so on.
    logger->info("[ {} ] Initializing the application",
                 platform::win32::type_traits::from_win32_string_view(m_appName));
    try {
        initialize();
    } catch (...) {
        // If initialization fails partway through, some derived-class state (e.g. an ImGui
        // backend already initialized) may still need tearing down, so we still call close()
        // before propagating the failure.
        logger->critical("[ {} ] Initialization failed, cleaning up before propagating the failure",
                         platform::win32::type_traits::from_win32_string_view(m_appName));
        close();
        throw;
    }

    // Rendering resources (and therefore a concrete image render queue) only exist once
    // initialize() has completed successfully, so the image factory wrapping it is injected here
    // rather than in set_logic(). m_imageFactory is owned by this application (not the logic
    // object) since the logic only ever gets a reference to it.
    m_imageFactory.emplace(get_or_create_image_render_queue());
    m_logic->set_image_factory(*m_imageFactory);

    const bool hasWindow{m_window != nullptr};

    // Check whether or not the application window has been created.
    if (hasWindow) {
        Expects(m_window->get_native_handle().is_valid() &&
                "Application window has not been created properly.");
        m_window->show();
    }

    logger->info("[ {} ] Starting the main loop of the application",
                 platform::win32::type_traits::from_win32_string_view(m_appName));
    if (hasWindow) {
        loop_impl_with_window();
    } else {
        loop_impl_no_window();
    }

    logger->info("[ {} ] Application is closing",
                 platform::win32::type_traits::from_win32_string_view(m_appName));
    m_logic->on_close();

    // Destroy the application logic now, while rendering resources are still available (close(),
    // which tears them down, hasn't run yet below). The logic may own GPU-backed resources (e.g.
    // images loaded through the injected image_factory) whose destructors reach back into the
    // image render queue. That queue is owned by a derived class (e.g. directx12_application),
    // so in the normal C++ destruction order it would be destroyed as part of tearing down the
    // derived object - which happens before this class's own m_logic member is destroyed, since
    // m_logic lives here in the base class. Resetting it explicitly here, before close() runs,
    // avoids that use-after-free.
    m_logic.reset();

    // Call implementation-specific cleanup tasks after the application logic's on_close()
    // method is called, so the application logic can perform any necessary cleanup tasks while
    // having resources still available.
    close();
}

void win32_application::loop_impl_no_window() {
    while (!m_stopToken.stop_requested()) {
        const bool keepRunning{m_logic->tick()};
        m_inputFrame.end_tick();
        if (!keepRunning) {
            break;
        }
    }
}

void win32_application::loop_impl_with_window() {
    for (bool done{}; !done;) {
        for (MSG msg{}; ::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE);) {
            if (msg.message == WM_QUIT) {
                done = true;
                break;
            }

            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        if (done) {
            break;
        }

        if (pre_logic_tick()) {
            m_logic->tick();
            post_logic_tick();
            m_inputFrame.end_tick();
        }
    }
}

} // namespace tca::engine::application::win32
