// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

// Engine
#include "core/math/size.hpp"
#include "input/input-frame.hpp"
#include "rendering/colors.hpp"
#include "rendering/image-factory.hpp"
#include "rendering/theming/theme.hpp"

namespace tca::engine::application {

/**
 * @brief A simple width/height pair, in screen coordinates.
 */
using window_size = math::size2i;

/**
 * @brief The size of a window's content (client) area, in screen coordinates: what the game draws
 * into, excluding the title bar and borders the OS draws around it.
 */
using window_content_size = math::size2i;

/**
 * @brief Where a window ends up after application_controller::set_content_size() resizes it.
 */
enum class window_placement {
    /**
     * @brief The top-left corner stays where it was, unless the window would then no longer fit
     * in its monitor's work area, in which case it is moved back inside it.
     */
    keep_top_left,

    /**
     * @brief The window is centred in the work area of the monitor it is on, as
     * application_controller::center_window() does.
     */
    center,
};

/**
 * @brief What application_controller::set_content_size() does when called while the application
 * is fullscreen, where the window covers its whole monitor and has no content size of its own.
 */
enum class fullscreen_resize {
    /**
     * @brief The request is dropped: the next go_windowed() restores the window as it was before
     * going fullscreen.
     */
    ignore,

    /**
     * @brief The request is remembered and applied, with its placement, by the next
     * go_windowed(). A later request made while still fullscreen replaces it.
     */
    defer_until_windowed,
};

/**
 * @brief How application_controller::set_content_size() applies a new content size. Every field
 * has a default, so callers can designated-initialize just the ones they care about (e.g.
 * `{.placement = window_placement::center}`), or pass `{}`.
 */
struct content_size_options final {
    /**
     * @brief Where the window ends up after resizing.
     */
    window_placement placement{window_placement::keep_top_left};

    /**
     * @brief What happens if the application is fullscreen at the time of the request. Has no
     * effect in windowed mode.
     */
    fullscreen_resize fullscreen{fullscreen_resize::ignore};
};

/**
 * @brief Describes the desired size/resizability of the application's window in windowed mode.
 * Platform-agnostic on purpose (this header has no platform dependency): concrete application
 * implementations (e.g. win32_application) translate this into platform-specific window
 * style/size at window-creation time.
 *
 * Every field has a valid default, so callers can designated-initializer just the fields they
 * care about (e.g. `{.resizable = false}`) and leave the rest at their defaults.
 */
struct window_display_settings final {
    /**
     * @brief A simple width/height pair, in screen coordinates.
     */
    using size = window_size;

    /**
     * @brief Desired windowed-mode window size, in screen coordinates. Defaults to the engine's
     * traditional 1280x720.
     */
    size content_size{1280, 720};

    /**
     * @brief Whether the player can drag-resize the window in windowed mode.
     */
    bool resizable{true};

    /**
     * @brief Whether the window can be maximized (maximize button, double-click title bar, OS
     * snap-to-maximize).
     */
    bool maximizable{true};
};

/**
 * @brief Abstract class representing a controller for the owner application. This class can be used
 * to control the application from the application logic, for example to signal the application to
 * exit etc
 */
struct application_controller {
    virtual ~application_controller() noexcept = default;

    /**
     * @brief Signals the application to exit. This method can be called from anywhere in the code
     * to signal the application to exit, and the engine will perform any necessary cleanup tasks
     * before actually closing the application.
     *
     * @note Whether the application will close immediately after this method is called, or if it
     * will wait message processing to finish, or if it will wait for the current frame to finish,
     * is implementation specific and can vary between different platforms and application types.
     * The only guarantee is that the application will eventually close after this method is called.
     */
    virtual void exit() = 0;

    /**
     * @brief Switches the application to fullscreen mode. This method can be called from anywhere
     * in the code to switch the application to fullscreen mode, and the engine will perform any
     * necessary tasks to perform the switch. The window covers the whole of the monitor it is on.
     * Can do nothing if the application implementation does not support or require a fullscreen
     * mode.
     */
    virtual void go_fullscreen() = 0;

    /**
     * @brief Switches the application to windowed mode. This method can be called from anywhere in
     * the code to switch the application to windowed mode, and the engine will perform any
     * necessary tasks to perform the switch. The window goes back to the position and size it had
     * before go_fullscreen(), and nothing happens if it is already windowed. Can do nothing if the
     * application implementation does not support or require a windowed mode.
     */
    virtual void go_windowed() = 0;

    /**
     * @brief Sets a new application theme, completely overriding the active one. Changes
     *  take effect instantly.
     *
     * @param newTheme New theme to apply.
     */
    virtual void use_theme(theming::theme newTheme) = 0;

    /**
     * @brief Resizes the window's content (client) area while the application is running, e.g. to
     * apply a "window scale" option without a restart. The OS-drawn frame grows or shrinks around
     * it, so the content area ends up exactly the requested size, whatever the DPI scale of the
     * monitor the window is on. Works for windows created non-resizable
     * (window_display_settings::resizable = false) too: those stop the player from drag-resizing
     * the window, not the game.
     *
     * The window stays on the monitor it is on. A size too large for that monitor's work area is
     * clamped to it, so the window is never left partly off screen.
     *
     * While fullscreen the window stays fullscreen, and the request is either dropped or applied
     * by the next go_windowed(), as options.fullscreen says (see fullscreen_resize).
     *
     * @note Can do nothing if the application implementation has no window. An empty size (a zero
     * or negative width or height) is ignored.
     *
     * @param contentSize The requested content area size, in screen coordinates.
     * @param options Where the window ends up, and what happens while fullscreen: see
     * content_size_options.
     */
    virtual void set_content_size(window_content_size contentSize,
                                  content_size_options options = {}) = 0;

    /**
     * @brief Tells whether the application is currently fullscreen, i.e. whether go_fullscreen()
     * is in effect, so that e.g. a single key can toggle between go_fullscreen() and go_windowed().
     *
     * @return true if the application is fullscreen, false if it is windowed or has no window.
     */
    virtual bool is_fullscreen() const noexcept = 0;

    /**
     * @brief Centres the window in the work area of the monitor it is on, keeping its size.
     *
     * @note Does nothing while fullscreen, since a fullscreen window already covers its whole
     * monitor, or if the application implementation has no window.
     */
    virtual void center_window() = 0;
};

/**
 * @brief Abstract class representing the logic of the application that will be run by the engine.
 *  The user should implement the tick() method to define the behavior of the application at each
 *  frame, and the on_close() method to define the behavior when the application is closed.
 */
struct application_logic {
    virtual ~application_logic() noexcept = default;

    /**
     * @brief Sets the application controller for this logic. This can be used even from the user
     * to specify a custom application controller with custom behavior, instead of using the one
     * provided by application implementations.
     *
     * @note This is guaranteed to be set when set_logic() is called on the application.
     * Implementations will pass a custom application controller to this method. Users may call
     * this method to set a custom application controller after calling set_logic() on the
     * application.
     *
     * @param controller The application controller to be used by this logic.
     */
    virtual void set_app_controller(application_controller& controller) = 0;

    /**
     * @brief Sets the image factory for this logic. This is the only place where "things to
     * draw" (e.g. decoded images) cross from application/game logic into the rendering pipeline,
     * kept intentionally separate from the application_controller, which only deals with
     * lifecycle concerns.
     *
     * @note This is guaranteed to be set once the application's rendering resources have been
     * initialized, which happens after set_app_controller() but before the first tick().
     *
     * @param factory The image factory to be used by this logic.
     */
    virtual void set_image_factory(rendering::image_factory& factory) = 0;

    /**
     * @brief Hands this logic its input: a view of the application's persistent
     * input_frame, the same frame for the whole run, holding the input of the tick being run.
     *
     * @note This is guaranteed to be set before the first tick(). Applications without input
     * (e.g. console applications) hand a view of a frame nothing writes into, so a logic never has
     * to check where it runs.
     *
     * @param input The view of the application's input frame.
     */
    virtual void set_input(input::input_view input) = 0;

    /**
     * @brief Called whenever the application is updated, i.e., at each frame. This method should
     * contain the logic that defines the behavior of the application at each frame, such as
     * updating the game state, processing user input, and rendering the scene.
     * Implementations should return true if the application should continue running, or false if
     * the application should be closed.
     *
     * @note The return value of this method will be completely ignored if the application uses a
     * message pump to process window messages, such as a Windows-based application, since in that
     * case the application will be closed when the window receives a close message, regardless of
     * the return value of this method.
     *
     * @note This method is called in a loop by the engine, and it should return as quickly as
     * possible to avoid blocking the main thread. If the application needs to perform long-running
     * tasks, it should offload them to a separate thread or use asynchronous programming techniques
     * to avoid blocking the main thread.
     *
     * @throws std::runtime_error Implementations should throw this exception if any error occurs
     * during the execution of the application logic, such as an unhandled exception or a critical
     * error that prevents the application from continuing to run.
     *
     * @return true if the application should continue running, or false if the application should
     * be closed. Can be ignored if the application uses a managed message pump.
     */
    virtual bool tick() = 0;

    /**
     * @brief Called once when the application is closing. Resources are guaranteed to be still
     * available when this method is called, so it can be used to perform cleanup tasks, such as
     * saving the game state, releasing resources, and performing any necessary finalization before
     * the application is closed.
     */
    virtual void on_close() = 0;
};

/**
 * @brief A default implementation of the application logic. Does nothing on tick and on close
 * but provides the application controller to derived classes.
 */
class application_logic_base : public application_logic {
public:
    constexpr bool tick() override {
        return false;
    }
    constexpr void on_close() override {}

    /**
     * @brief Set the app controller for this logic.
     *
     * @param controller The application controller to be used by this logic.
     */
    void set_app_controller(application_controller& controller) final {
        m_controller = &controller;
    }

    void set_image_factory(rendering::image_factory& factory) final {
        m_imageFactory = &factory;
    }

    void set_input(input::input_view input) final;

protected:
    /**
     * @brief Retrieves the application controller for this logic.
     *
     * @return application_controller& A reference to the application controller for this logic.
     */
    application_controller& get_application_controller() const {
        if (!m_controller) {
            throw std::logic_error(
                "Application controller has not been set for this application logic. Please set "
                "the application controller before trying to access it.");
        }

        return *m_controller;
    }

    /**
     * @brief Retrieves the image_factory bound to this logic. This is the preferred way for game
     * logic to load images: it never has to deal with the underlying image_render_queue itself,
     * only with image_factory::load_from_file()/create_from_pixels() and the resulting image
     * objects.
     *
     * @return rendering::image_factory& A reference to the image factory for this logic.
     */
    rendering::image_factory& get_image_factory() const {
        if (!m_imageFactory) {
            throw std::logic_error(
                "Image factory has not been set for this application logic. Please set "
                "the image factory before trying to access it.");
        }

        return *m_imageFactory;
    }

    /**
     * @brief The input of the tick being run, as handed by set_input(). Until it is set, a view of
     * an empty frame, so it never throws.
     *
     * @return input::input_view A view of the application's input frame.
     */
    [[nodiscard]] input::input_view get_input() const noexcept;

private:
    application_controller* m_controller{};
    rendering::image_factory* m_imageFactory{};
    std::optional<input::input_view> m_inputView{};
};

/**
 * @brief Abstract class representing the application that will be run by the engine.
 *  Implementations should implement the builder pattern to allow for a more flexible
 * configuration of the application instance.
 */
struct application {
    virtual ~application() noexcept = default;

    /**
     * @brief Sets the theme the application wears: the single place its colours, dimensions and
     * typography are defined. Everything resolving a role picks it up - engine widgets, game
     * widgets, the GUI backend's style, and the colour the window is cleared to before each frame
     * (theming::color_role::window_background) - so this is the one call that changes how the
     * whole application looks.
     *
     * @note Unlike the other builder methods here, this one may also be called after run(): the
     * new theme takes effect from the next frame, so a game can offer a theme setting without
     * asking the player to restart.
     *
     * @param newTheme The theme to wear.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    virtual application& set_theme(theming::theme newTheme) = 0;

    /**
     * @brief Sets the desired size/resizability of the application's window in windowed mode
     * (see window_display_settings). Implementations without a window (e.g. console applications)
     * may store this without acting on it.
     *
     * @note Should be called before run(), like the other builder pattern methods here: this
     * configures the window that will be created during initialize(), so setting it afterward has
     * no effect on the initial window creation (though it may still affect a later
     * go_windowed()/go_fullscreen() toggle, which is implementation specific).
     *
     * @param settings The desired window display settings.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    virtual application& set_window_display_settings(window_display_settings&& settings) = 0;

    /**
     * @brief Sets the name of the application.
     *
     * @param name The name to set for the application.
     * @return A reference to the application instance, allowing for method chaining.
     */
    virtual application& set_app_name(std::string_view name) = 0;

    /**
     * @brief Sets the name of the company publishing the application, so that its per-user folders
     * (the log folder, for one) sit under the company's own: "...\<company>\<app>\logs".
     *
     * @note Like the other builder pattern methods, it is read when run() starts, so it can be
     * called before or after set_app_name(). It is optional: without it, the company folder is left
     * out.
     *
     * @param name The name of the company.
     * @return A reference to the application instance, allowing for method chaining.
     */
    virtual application& set_company_name(std::string_view name) = 0;

    /**
     * @brief Sets the main application logic that will be run by the engine. This method takes
     * ownership of the application logic instance, and the engine will call the tick() and
     * on_close() methods of the provided application logic instance at the appropriate times during
     * the application's lifecycle.
     *
     * @param logic The application logic instance to set.
     * @return A reference to the application instance, allowing for method chaining.
     */
    virtual application& set_logic(std::unique_ptr<application_logic> logic) = 0;

    /**
     * @brief Starts the application and runs the main loop. This method will block until the
     * application is closed, and it will call the tick() method of the application logic instance
     * at each frame, and the on_close() method when the application is closed.
     *
     * @note Implementations should ensure this method is called after configuring the application
     * instance using the builder pattern methods (e.g., set_app_name() and set_logic()). Once run()
     * is called, the application will start and the main loop will begin, so any further
     * configuration of the application instance should be done before calling run().
     *
     * @throws std::runtime_error Implementations should throw this exception if the application
     * logic has not been set before calling run(), or if any other error occurs during the
     * execution of the application.
     */
    virtual void run() = 0;
};

/**
 * @brief Really basic implementation of the application class that doesn't do anything if not
 * setting the base fields. This can be used as a base class for more complex application
 * implementations.
 */
class application_base : public application {
public:
    /**
     * @brief Sets the theme of the application.
     *
     * @param newTheme The theme to wear.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_theme(theming::theme newTheme) final;

    /**
     * @brief Set the window display settings of the application.
     *
     * @param settings The desired window display settings.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_window_display_settings(window_display_settings&& settings) final;

    /**
     * @brief Set the app name.
     *
     * @param name The name to set for the application.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_app_name(std::string_view name) final;

    /**
     * @brief Set the company name.
     *
     * @param name The name of the company publishing the application.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_company_name(std::string_view name) final;

    /**
     * @brief Set the logic of the application.
     *
     * @param logic The application logic instance to set. This method takes ownership of the
     * application logic instance, and the engine will call the tick() and on_close() methods of the
     * provided application logic instance at the appropriate times during the application's
     * lifecycle.
     * @return application& A reference to the application instance, allowing for method chaining.
     */
    application& set_logic(std::unique_ptr<application_logic> logic) final;

    /**
     * @brief Doesn't do anything.
     */
    void run() override {}

protected:
    /**
     * @brief Retrieves or creates the application controller for this application.
     * This method is called by set_logic(). Implementations should provide a way to create an
     * application controller if one doesn't exist yet and return a reference to it.
     *
     * @return application_controller& Instance to implementation-defined application controller.
     */
    virtual application_controller& get_or_create_controller() = 0;

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
     * @brief Get the window display settings of the application.
     *
     * @return const window_display_settings& The window display settings of the application.
     */
    const window_display_settings& get_window_display_settings() const noexcept {
        return m_windowDisplaySettings;
    }

    /**
     * @brief Retrieves the name of the application.
     *
     * @return std::string_view The name of the application.
     */
    std::string_view get_app_name() const noexcept {
        return m_appName;
    }

    /**
     * @brief Retrieves the name of the company publishing the application.
     *
     * @return std::string_view The name of the company, empty if none was set.
     */
    std::string_view get_company_name() const noexcept {
        return m_companyName;
    }

    /**
     * @brief Retrieves the application logic instance that is set for this application.
     * If no logic has been set, this method will return a null pointer.
     *
     * @return application_logic* A pointer to the application logic instance that is set for this
     * application, or a null pointer if no logic has been set.
     */
    application_logic* get_logic() const noexcept {
        return m_logic.get();
    }

private:
    theming::theme m_theme{};
    window_display_settings m_windowDisplaySettings{};
    std::string m_appName{};
    std::string m_companyName{};
    std::unique_ptr<application_logic> m_logic{};
};

/**
 * @brief Factory class for creating application instances. This class provides static methods for
 * creating application instances based on the platform or other criteria.
 *
 * The create_platform_preferred_application() will ensure to create an application instance that is
 * optimized for the current platform, such as a DirectX 12-based application for Windows, or a
 * Vulkan-based application for Linux.
 */
class application_factory final {
public:
    /**
     * @brief Creates an application instance that is optimized for the current platform. The
     * specific type of application created will depend on the platform the engine is running on,
     * and it will be designed to take advantage of the features and capabilities of that platform
     * to provide the best possible performance and user experience.
     *
     * For example, on Windows, it might create a DirectX 12-based application, while on Linux, it
     * might create a Vulkan-based application.
     *
     * @return A unique pointer to the created application instance.
     */
    static std::unique_ptr<application> create_platform_preferred_application();

    /**
     * @brief Create an opengl3-based application instance. This method can be used to create an
     * application instance that is based on the OpenGL 3 rendering backend, which is a widely
     * supported and cross-platform rendering API.
     *
     * @return std::unique_ptr<application> A unique pointer to the created OpenGL 3-based
     * application instance.
     */
    static std::unique_ptr<application> create_opengl3_application();

    /**
     * @brief Create a minimal application instance. This method will instance a GUI application
     * that has the lowest graphical-resource-consumption possible. This can be used for testing,
     * creating application that do not require high graphical performance.
     *
     * @return std::unique_ptr<application> A unique pointer to the created minimal application
     * instance.
     */
    static std::unique_ptr<application> create_minimal_graphics_application();

private:
    application_factory() = default;
};

} // namespace tca::engine::application
