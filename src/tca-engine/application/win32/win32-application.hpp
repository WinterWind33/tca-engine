// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

#ifndef _WIN32
#error "This header should only be included on Windows platforms."
#endif // This header is only for Windows platforms, and it should not be included on other
       // platforms.

// C++ STL
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <wil/resource.h>

// Engine
#include "application/application.hpp"
#include "input/input-collector.hpp"
#include "input/input-frame.hpp"
#include "platform/application-window.hpp"
#include "platform/engine-type-traits.hpp"
#include "platform/win32/win32-application-window.hpp"
#include "rendering/colors.hpp"
#include "rendering/image-factory.hpp"
#include "rendering/rendering-queues/null-image-render-queue.hpp"

namespace tca::engine::application::win32 {

/**
 * @brief Base class for all the Windows-specific application implementations.
 *
 * This class provides the common interface and functionality for all Windows-based applications,
 * and it can be extended to create specific types of applications, such as DirectX 12-based
 * applications, by implementing the necessary logic in the derived classes. The win32_application
 * class itself does not implement any specific functionality, but it serves as a common base for
 * all Windows-based application implementations, allowing for a consistent interface and shared
 * functionality across different types of Windows applications.
 *
 */
class win32_application : public application {
public:
    /**
     * @brief Sets the theme the application wears. See application::set_theme(): it may be called
     * after run() too, in which case the new theme reaches the GUI style and the window clear
     * colour from the next frame.
     *
     * @param newTheme The theme to wear.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_theme(theming::theme newTheme) override;

    /**
     * @brief Sets the window display settings of the application.
     *
     * @param settings The desired window display settings.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_window_display_settings(window_display_settings&& settings) override;

    /**
     * @brief Sets the app name.
     *  Non overridable anymore since there shouldn't be any strange logic behind this.
     *
     * @param name The name to set for the application.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_app_name(std::string_view name) final;

    /**
     * @brief Sets the company name.
     *  Non overridable anymore since there shouldn't be any strange logic behind this.
     *
     * @param name The name of the company publishing the application.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_company_name(std::string_view name) final;

    /**
     * @brief Sets the logic object.
     *  Non overridable anymore since there shouldn't be any strange logic behind this.
     *
     * @param logic The logic object to set for the application.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_logic(std::unique_ptr<application_logic> logic) final;

    void run() override;

protected:
    /**
     * @brief Get the theme the application is wearing.
     *
     * @return const theming::theme& The application's theme.
     */
    const theming::theme& get_theme() const noexcept {
        return m_theme;
    }

    /**
     * @brief Get the color the window is cleared to before rendering each frame, which is the
     * theme's theming::color_role::window_background and not a value of its own.
     *
     * @return rendering::color The background color of the application.
     */
    rendering::color get_background_color() const noexcept {
        return m_theme.resolve(theming::color_role::window_background);
    }

    /**
     * @brief Called after the application's theme has changed, so that a derived class holding
     * resources derived from it (a rendering manager's clear color, say) can refresh them. Does
     * nothing by default.
     *
     * @note Called for every set_theme(), including the ones happening before the application is
     * initialized, so an override must cope with its resources not existing yet.
     */
    virtual void on_theme_changed() {}

    /**
     * @brief Get the window display settings of the application.
     *
     * @return const window_display_settings& The window display settings of the application.
     */
    const window_display_settings& get_window_display_settings() const noexcept {
        return m_windowDisplaySettings;
    }

    /**
     * @brief Translates get_window_display_settings() into the Win32-specific window creation
     * parameters (WS_* style and size) expected by win32_application_window/imgui_win32_window.
     * Derived classes should call this once, right before constructing their application window,
     * so the WS_*-flag/size translation logic lives in exactly one place.
     *
     * @return platform::win32::partial_window_creation_params The computed window creation
     * parameters.
     */
    [[nodiscard]] platform::win32::partial_window_creation_params make_window_creation_params()
        const;

    /**
     * @brief Optional application window. This member can be used by derived classes to store the
     * application window associated with the application.
     *
     * @note If the window is nullptr at the time of the run() method, the application will be
     * run without a window, which can be useful for applications that do not require a graphical
     * interface or for testing purposes.
     */
    std::unique_ptr<platform::platform_specific_window<wil::unique_hwnd>> m_window{};

    auto get_app_name() const noexcept -> platform::platform_preferred_string_view_t;
    application_logic& get_logic() const;

    /**
     * @brief Initializes the application. This method is called (or should be called) by the run()
     * method before starting the main loop. Implementations can override this method to perform any
     * necessary initialization tasks, such as creating the application window, setting up the
     * rendering context, and initializing any necessary resources before the main loop starts.
     *
     * This will set the app name if not given and it will also check that the application logic has
     * been set, throwing a std::runtime_error if it has not been set, since the application logic
     * is essential for the application to run properly.
     *
     * @note If something goes wrong during the initialization, implementations should throw a
     * std::runtime_error with a descriptive error message to indicate what went wrong.
     */
    virtual void initialize();

    /**
     * @brief Retrieves (lazily creating it if needed) the image render queue owned by this
     * application, used to inject it into the application logic once rendering resources are
     * ready (i.e. after initialize() has run). Derived classes that own a concrete rendering
     * manager (e.g. directx12_application, opengl3_win32_application) should override this to
     * lazily construct and return their backend-specific image_render_queue implementation.
     *
     * @note The base implementation returns a no-op queue, since a plain win32_application (with
     * no rendering manager of its own) has nothing to upload images to. This keeps
     * win32_application usable on its own (e.g. headless/console-style applications, or in
     * tests) without forcing every derived class to implement image rendering.
     */
    virtual rendering::image_render_queue& get_or_create_image_render_queue();

    /**
     * @brief Called during run() execution when the application is closing. This method can be
     * overridden by derived classes to perform any necessary cleanup tasks, such as releasing
     * resources, saving the application state, and performing any other necessary finalization
     * before the application is closed.
     *
     * This is called AFTER the application logic's on_close() method is called, so the application
     * logic can perform any necessary cleanup tasks before the application itself performs its
     * cleanup tasks while having resources still available.
     */
    virtual void close();

    /**
     * @brief Called before the application logic's tick() method is called at each frame.
     * Must return true if the loop iteration should continue, or false if the loop iteration should
     * be skipped.
     *
     * If this method returns false, the logic tick() and the post_logic_tick() will be skipped for
     * that frame.
     *
     * @return true if the loop iteration should continue, or false if the loop iteration should be
     * skipped.
     */
    virtual bool pre_logic_tick();

    /**
     * @brief Called after the application logic's tick() method is called at each frame. This
     * method can be overridden by derived classes to perform any necessary tasks after the
     * application logic is executed at each frame, such as rendering the scene, updating the
     * display, or performing any other necessary tasks that should be done after the application
     * logic is executed.
     */
    virtual void post_logic_tick();

    /**
     * @brief The collector writing into the input frame the logic reads: platform events go
     * through it. What it collects before a tick is seen by that tick and gone after it (held keys
     * stay down); when pre_logic_tick() skips a frame, its input reaches the next tick that runs.
     *
     * @return input::input_collector& The application's input collector.
     */
    input::input_collector& get_input_collector() noexcept;

    /**
     * @brief A view of the input frame the logic reads, the same one it gets through set_input().
     * Lets derived applications take input before the logic sees it, e.g. consume_all() when an
     * overlay has the keyboard.
     *
     * @return input::input_view A view of the application's input frame.
     */
    input::input_view get_input() noexcept;

private:
    theming::theme m_theme{};
    window_display_settings m_windowDisplaySettings{};
    platform::platform_preferred_string_t m_appName{};
    platform::platform_preferred_string_t m_companyName{};
    std::unique_ptr<application_logic> m_logic{};
    std::unique_ptr<application_controller> m_controller{};
    std::stop_token m_stopToken{};
    rendering::null_image_render_queue m_nullImageRenderQueue{};
    std::optional<rendering::image_factory> m_imageFactory{};

    // The persistent frame every tick's input goes into, and what writes into it.
    input::input_frame m_inputFrame{};
    input::input_collector m_inputCollector{m_inputFrame};

    void loop_impl_no_window();
    void loop_impl_with_window();
};

} // namespace tca::engine::application::win32
