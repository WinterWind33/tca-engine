// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <concepts>
#include <memory>
#include <optional>
#include <span>
#include <utility>

// Third-party
#include <GLFW/glfw3.h>
#include <cstddef>
#include <cstdint>
#include <gsl/gsl>
#include <gsl/pointers>

// Engine
#include "core/events.hpp"
#include "input/input-collector.hpp"
#include "platform/application-window.hpp"
#include "platform/engine-type-traits.hpp"
#include "platform/images/image-decoder.hpp"

namespace tca::engine::platform::posix_linux {

/**
 * @brief RAII, move-only owner of a GLFWwindow*, modeled after std::unique_ptr. Destroys the
 * wrapped window (via glfwDestroyWindow()) when it goes out of scope or is reset/reassigned, so
 * callers never have to remember to call glfwDestroyWindow() themselves.
 */
class unique_glfw_window final {
public:
    using value_type = GLFWwindow;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    /**
     * @brief Constructs an empty (null) unique_glfw_window, owning no window.
     */
    unique_glfw_window() noexcept = default;

    /**
     * @brief Takes ownership of an existing GLFW window handle.
     *
     * @param windowPtr The GLFW window handle to take ownership of. Must have been created with
     * glfwCreateWindow() (or be null), as it will be destroyed with glfwDestroyWindow().
     */
    explicit unique_glfw_window(const pointer windowPtr) noexcept : m_windowPtr{windowPtr} {}

    /**
     * @brief Constructs an empty (null) unique_glfw_window, owning no window.
     */
    unique_glfw_window(std::nullptr_t) noexcept {}

    unique_glfw_window(const unique_glfw_window& other) noexcept = delete;
    unique_glfw_window& operator=(const unique_glfw_window& other) noexcept = delete;

    /**
     * @brief Move-constructs this instance, transferring ownership of the window from other.
     * other is left owning no window.
     *
     * @param other The instance to move from.
     */
    unique_glfw_window(unique_glfw_window&& other) noexcept;

    /**
     * @brief Move-assigns this instance, destroying any window it currently owns and then
     * transferring ownership of the window from other. other is left owning no window.
     *
     * @param other The instance to move from.
     * @return unique_glfw_window& A reference to this instance.
     */
    unique_glfw_window& operator=(unique_glfw_window&& other) noexcept;

    /**
     * @brief Destroys the owned window, if any, via glfwDestroyWindow().
     */
    ~unique_glfw_window() noexcept;

    /**
     * @brief Retrieves the owned GLFW window handle without releasing ownership.
     *
     * @return const_pointer The owned window handle, or nullptr if this instance owns no window.
     */
    pointer get() const noexcept {
        return m_windowPtr;
    }

    /**
     * @brief Swaps the owned window handles of this instance and other.
     *
     * @param other The instance to swap with.
     */
    void swap(unique_glfw_window& other) noexcept;

    /**
     * @brief Destroys the currently owned window, if any, and replaces it with newPtr.
     *
     * @param newPtr The new window handle to take ownership of. Defaults to nullptr, so calling
     * reset() with no arguments just destroys the currently owned window.
     */
    void reset(pointer newPtr = nullptr) noexcept;

    /**
     * @brief Checks whether this instance currently owns a window.
     *
     * @return true if this instance owns a non-null window handle, false otherwise.
     */
    bool is_valid() const noexcept {
        return m_windowPtr != nullptr;
    }

    /**
     * @brief Checks whether this instance currently owns a window. Equivalent to is_valid().
     */
    explicit operator bool() const noexcept {
        return is_valid();
    }

    /**
     * @brief Releases ownership of the owned window without destroying it, leaving this instance
     * owning no window. The caller becomes responsible for eventually calling
     * glfwDestroyWindow() on the returned handle.
     *
     * @return pointer The previously owned window handle, or nullptr if this instance owned no
     * window.
     */
    pointer release() noexcept {
        pointer temp{m_windowPtr};
        m_windowPtr = nullptr;
        return temp;
    }

private:
    pointer m_windowPtr{};
};

struct window_creation_size final {
    std::int32_t width{1280};
    std::int32_t height{720};
};

/**
 * @brief Position of the window's content area, in screen coordinates, when creating it.
 */
struct window_creation_position final {
    std::int32_t x{};
    std::int32_t y{};
};

/**
 * @brief Everything linux_application_window needs to create its GLFW window. Each flag is applied
 * as the matching GLFW window hint on every creation, so stale global hints never leak in.
 */
struct window_creation_params final {
    window_creation_size initial_size{};

    /**
     * @brief Where to place the window. Empty lets GLFW (the window manager) pick.
     */
    std::optional<window_creation_position> initial_position{};

    /** @brief GLFW_RESIZABLE: whether the user can drag-resize the window. */
    bool resizable{true};

    /** @brief GLFW_DECORATED: whether the window has a border, title bar and close button. */
    bool decorated{true};

    /**
     * @brief GLFW_VISIBLE: whether the window is visible as soon as it is created. Defaults to
     * false, like win32_application_window, which is created hidden and shown through show().
     */
    bool visible{false};

    /** @brief GLFW_MAXIMIZED: whether the window starts maximized. */
    bool maximized{false};

    /** @brief GLFW_FOCUSED: whether the window takes input focus when created visible. */
    bool focused{true};

    /** @brief GLFW_FOCUS_ON_SHOW: whether the window takes input focus whenever show() is called.
     */
    bool focus_on_show{true};

    /**
     * @brief Whether the window is created fullscreen on the primary monitor, at that monitor's
     * current video mode (initial_size is then ignored, and the display mode is left alone). With
     * no monitor connected, the window is created windowed instead.
     */
    bool fullscreen{false};

    /**
     * @brief Candidate icon images (glfwSetWindowIcon()), typically from
     * images::decode_image_from_file(); the system picks the closest size. They are only read
     * while the window is being created. Empty keeps the default icon.
     */
    std::span<const images::decoded_image> icons{};
};

class linux_application_window;

/**
 * @brief Concept representing types that are linux application windows, meaning that they are
 * either of type linux_application_window or they derive from linux_application_window.
 *
 * @tparam LinuxApplicationWindowType The type to be checked.
 */
template <typename LinuxApplicationWindowType>
concept LinuxApplicationWindow =
    std::derived_from<LinuxApplicationWindowType, linux_application_window>;

/**
 * @brief Base class for application windows on Linux, backed by a GLFW window.
 *
 * @note The constructor always creates the GLFW window, so a constructed object always owns a
 * valid handle. Subclasses that need on_create() must be built through create(), since a virtual
 * call made from the base constructor would never reach them.
 */
class linux_application_window : public platform_specific_window<unique_glfw_window> {
public:
    /**
     * @brief Event fired when the window's client area is resized, carrying the new width and
     * height (in that order). Subscribe via on_resize().
     */
    using resize_event_t = core::event<std::int32_t, std::int32_t>;

    /**
     * @brief Creates the GLFW window. GLFW must be initialized.
     *
     * @throws std::runtime_error if GLFW fails to create the window, for example with no display
     * to connect to or an invalid size.
     */
    explicit linux_application_window(platform_preferred_string_view_t title,
                                      const window_creation_params& size);

    linux_application_window(const linux_application_window&) = delete;
    linux_application_window& operator=(const linux_application_window&) = delete;

    /**
     * @brief Takes over other's GLFW window, input collector and resize subscribers, and points
     * the GLFW window's callbacks at this instance. other is left owning no window.
     */
    linux_application_window(linux_application_window&& other) noexcept;

    /**
     * @brief Destroys the GLFW window this instance owned, then takes over other's as the move
     * constructor does.
     */
    linux_application_window& operator=(linux_application_window&& other) noexcept;

    ~linux_application_window() noexcept override = default;

    /**
     * @brief Shows the window, making it visible on screen.
     */
    void show() override;

    /**
     * @brief Reports whether the window has been asked to close, mirroring GLFW's
     * glfwWindowShouldClose().
     *
     * @return true if the window should close, false otherwise.
     */
    [[nodiscard]] bool should_close() const noexcept;

    /**
     * @brief Requests that the window close, mirroring GLFW's glfwSetWindowShouldClose().
     * should_close() reports true afterwards. This is the program's own decision, so it does not
     * go through on_close_requested().
     */
    void request_close() noexcept;

    /**
     * @brief Grants access to the resize event's subscriber, so callers can subscribe/unsubscribe
     * handlers to be invoked whenever the window's client area is resized. Deliberately does not
     * expose the resize_event_t itself: only this class should be able to fire it (via
     * execute()), everyone else may only listen.
     *
     * @return resize_event_t::subscriber& The resize event's subscriber.
     */
    [[nodiscard]] resize_event_t::subscriber& on_resize() noexcept;

    /**
     * @brief Sets the collector this window feeds its input to: keys, typed text, mouse buttons,
     * the pointer, the wheel and focus loss. While it is null, input is dropped.
     *
     * @param inputCollector The collector to feed, or null to stop feeding one. It must outlive
     * the window, or be detached before it is destroyed.
     */
    void set_input_collector(input::input_collector* const inputCollector) noexcept {
        m_inputCollector = inputCollector;
    }

    /**
     * @brief Constructs a window of the given type and then calls its on_create(). The window is
     * created by the constructor as usual; on_create() runs only once the whole object, subclass
     * included, has been constructed.
     *
     * @tparam LinuxApplicationWindowType The type of the window to construct: either
     * linux_application_window or a type deriving from it.
     * @tparam Args The types of the arguments to be passed to the constructor of the window.
     * @param args The arguments to be passed to the constructor of the window.
     * @return std::unique_ptr<LinuxApplicationWindowType> The constructed window.
     */
    template <LinuxApplicationWindow LinuxApplicationWindowType, typename... Args>
    [[nodiscard]] static std::unique_ptr<LinuxApplicationWindowType> create(Args&&... args) {
        auto window{std::make_unique<LinuxApplicationWindowType>(std::forward<Args>(args)...)};
        static_cast<linux_application_window&>(*window).on_create();
        return window;
    }

protected:
    /**
     * @brief Handler for implementations, called by create() once the object is fully
     * constructed.
     *
     * When this function is called, the GLFW window is guaranteed to be valid, though it is not
     * shown yet (unless window_creation_params::visible asked for it). This can be useful to
     * perform initialization that requires the native window, such as setting up the rendering
     * backend with it.
     *
     * @note The show() function should not be called within this function to ensure proper
     * resources initialization before showing.
     *
     * @note This function is not called on a window constructed directly, since C++ does not
     * dispatch virtual calls made during construction to the derived class.
     */
    virtual void on_create();

    /**
     * @brief Called when the user asks to close the window, for example with its close button
     * (glfwSetWindowCloseCallback()). The counterpart of WM_CLOSE on Win32.
     *
     * Unlike Win32, nothing is destroyed here: should_close() starts reporting true, and whoever
     * runs the loop stops it, so the window is destroyed with its owner. request_close() does not
     * come through here.
     *
     * @note Must not throw: GLFW calls it from C, so an escaping exception terminates the
     * program.
     *
     * @return true (the default) to let the window close, false to veto the request, which
     * leaves should_close() reporting false.
     */
    [[nodiscard]] virtual bool on_close_requested();

    /**
     * @name Native event hooks
     * Called with each GLFW event, as GLFW reports it, for subclasses to react to: the counterpart
     * of Win32's routed_window_procedure(). The default implementations do nothing. Mouse events
     * have already fed the input collector (see set_input_collector()) when their hook is called,
     * so an override doesn't need to call the base. Keyboard and focus have no hook: they only
     * feed the collector, and resizes fire on_resize(). Overrides must not throw: GLFW calls them
     * from C, so an escaping exception terminates the program.
     * @{
     */

    /**
     * @brief A mouse button was pressed or released (glfwSetMouseButtonCallback()).
     *
     * @param button The GLFW_MOUSE_BUTTON_* that changed.
     * @param action GLFW_PRESS or GLFW_RELEASE.
     * @param mods The GLFW_MOD_* flags held at the time.
     */
    virtual void on_mouse_button(int button, int action, int mods);

    /**
     * @brief The cursor moved (glfwSetCursorPosCallback()).
     *
     * @param x The new position, in screen coordinates relative to the content area's left edge.
     * @param y The new position, in screen coordinates relative to the content area's top edge.
     */
    virtual void on_cursor_moved(double x, double y);

    /**
     * @brief The mouse wheel or touchpad scrolled (glfwSetScrollCallback()).
     *
     * @param xOffset The horizontal scroll offset.
     * @param yOffset The vertical scroll offset.
     */
    virtual void on_scroll(double xOffset, double yOffset);

    /**
     * @brief The window was minimized or restored from minimized (glfwSetWindowIconifyCallback()).
     *
     * @param iconified true if it was minimized, false if it was restored.
     */
    virtual void on_iconify(bool iconified);

    /**
     * @brief The framebuffer was resized (glfwSetFramebufferSizeCallback()). Its size is in
     * pixels, so it can differ from the content size on_resize() reports in screen coordinates.
     *
     * @param width The new framebuffer width, in pixels.
     * @param height The new framebuffer height, in pixels.
     */
    virtual void on_framebuffer_resized(std::int32_t width, std::int32_t height);

    /**
     * @brief The window's content scale changed, for example when it moved to a monitor with a
     * different DPI (glfwSetWindowContentScaleCallback()).
     *
     * @param xScale The new horizontal content scale.
     * @param yScale The new vertical content scale.
     */
    virtual void on_content_scale_changed(float xScale, float yScale);

    /** @} */

    void instantiate_window(const window_creation_params& creationParams);
    void create_window(const window_creation_params& creationParams);

private:
    resize_event_t m_resizeEvent{};
    input::input_collector* m_inputCollector{};

    void rebind_native_window() noexcept;

    static void window_key_callback(GLFWwindow* window, int key, int scanCode, int action,
                                    int mods) noexcept;
    void routed_key_event_handler(int key, int scanCode, int action, int mods) noexcept;

    static void window_unicode_char_callback(GLFWwindow* window, unsigned int codepoint) noexcept;
    void routed_unicode_char_handler(char32_t codepoint) noexcept;

    static void window_focus_changed_callback(GLFWwindow* window, int focusState) noexcept;
    void on_focus_acquired() noexcept;
    void on_focus_lost() noexcept;

    void routed_mouse_button_handler(int button, int action) noexcept;
    void routed_cursor_position_handler(double x, double y) noexcept;
    void routed_cursor_enter_handler(bool entered) noexcept;
    void routed_scroll_handler(double xOffset, double yOffset) noexcept;
};

} // namespace tca::engine::platform::posix_linux
