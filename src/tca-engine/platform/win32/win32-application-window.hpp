// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <wil/resource.h>

#include "input/input-collector.hpp"

#include "platform/application-window.hpp"
#include "platform/engine-type-traits.hpp"
#include "platform/win32/windows-messages.hpp"
#include "platform/win32/windows-type-traits.hpp"

namespace tca::engine::platform::win32 {

namespace details {
using lresult_t = LRESULT;
using message_id_t = UINT;
using wparam_t = WPARAM;
using lparam_t = LPARAM;

/**
 * @brief Sets the size of the structure and the instance handle of the module for a WNDCLASSEX
 * struct. The instance is retrieved using GetModuleHandle with a null pointer.
 *
 * @param wndClass The WNDCLASSEX struct to be initialized.
 */
void common_init_wndclassex(WNDCLASSEX* wndClass) noexcept;

/**
 * @brief Used within wil::unique_struct to properly destroy a WNDCLASSEX struct when
 *  the unique_struct goes out of scope.
 *
 * @note If the given WNDCLASSEX has an invalid class name, it won't do anything, since the
 * UnregisterClass function will fail.
 *
 * @param windowClass The WNDCLASSEX struct to be destroyed.
 */
void destroy_unique_wndclassex(WNDCLASSEX* windowClass) noexcept;

/**
 * @brief Concept representing functions that can be used to initialize a WNDCLASSEX struct.
 *
 * @tparam WndClassInitFunc The type of the function to be used to initialize the WNDCLASSEX struct.
 */
template <typename WndClassInitFunc>
concept WndClassInitializerFunction = std::invocable<WndClassInitFunc, WNDCLASSEX*>;

static_assert(WndClassInitializerFunction<decltype(&common_init_wndclassex)>);

/**
 * @brief Partial alias for a unique_struct that manages a WNDCLASSEX struct.
 *
 * @tparam InitFunc The type of the initialization function to be used to initialize the WNDCLASSEX
 * struct when it is created. This function should take a pointer to a WNDCLASSEX struct as a
 * parameter.
 * @tparam initFunc The initialization function to be used to initialize the WNDCLASSEX struct when
 * it is created. This function should take a pointer to a WNDCLASSEX struct as a parameter.
 */
template <WndClassInitializerFunction InitFunc, InitFunc initFunc>
using unique_wndclassex = wil::unique_struct<WNDCLASSEX, decltype(&destroy_unique_wndclassex),
                                             &destroy_unique_wndclassex, InitFunc, initFunc>;

} // namespace details

/**
 * @brief A structure representing some fields of the WNDCLASSEX structure.
 *  This is used to avoid Win32 Window classes being filled with wrong window procedures or
 *  instances. Derived Win32 Window classes can, eventually, set other fields to properly customize
 *  the window class.
 */
struct partial_wndclassex final {
    std::uint32_t style{};
    std::int32_t class_extra_bytes{};

    /**
     * @brief Note on this: this can be completely ignored by different window classes, since we
     * need to have the extra bytes for the pointer to the application window instance in many
     * implementation, to properly route the window procedure to the appropriate instance of the
     * application window class.
     */
    std::int32_t window_extra_bytes{};
    type_traits::non_owning_resource<HICON> icon{};
    type_traits::non_owning_resource<HCURSOR> cursor{};
    type_traits::non_owning_resource<HBRUSH> background_brush{};
    platform_preferred_string_t menu_name{};
    platform_preferred_string_t class_name{};
    type_traits::non_owning_resource<HICON> small_icon{};
};

/**
 * @brief Helper structure to set the size of the window when creating it.
 *  Each member is defaulted to CW_USEDEFAULT, so that if the inheriting class doesn't want to set a
 *  specific size for the window, it can just use the default constructor of this struct.
 */
struct window_creation_size final {
    std::int32_t width{CW_USEDEFAULT};
    std::int32_t height{CW_USEDEFAULT};
};

/**
 * @brief Helper structure to set the position of the window when creating it.
 *  Each member is defaulted to CW_USEDEFAULT, so that if the inheriting class doesn't want to set a
 * specific position for the window, it can just use the default constructor of this struct.
 */
struct window_creation_position final {
    std::int32_t x{CW_USEDEFAULT};
    std::int32_t y{CW_USEDEFAULT};
};

class win32_application_window;

/**
 * @brief Concept representing types that are win32 application windows, meaning that they are
 * either of type win32_application_window or they derive from win32_application_window.
 *
 * @tparam Win32ApplicationWindowType The type to be checked.
 */
template <typename Win32ApplicationWindowType>
concept Win32ApplicationWindow =
    std::derived_from<Win32ApplicationWindowType, win32_application_window>;

/**
 * @brief Helper structure to set the creation parameters of the window when creating it.
 *  The default constructor of this struct sets the style to WS_OVERLAPPEDWINDOW, which is a common
 *   style for windows, and leaves the other members defaulted to their default constructors, which
 *   means that the window will be created without a parent, without a menu, and with the default
 *   size and position.
 */
struct partial_window_creation_params final {
    std::uint32_t style{WS_OVERLAPPEDWINDOW};
    window_creation_position position{};
    window_creation_size size{};
    type_traits::non_owning_resource<HMENU> menu_handle{};
    type_traits::non_owning_resource<HWND> parent_window_handle{};
    bool use_pure_raii{};
};

/**
 * @brief Computes a WS_OVERLAPPEDWINDOW-based window style with WS_THICKFRAME (drag-to-resize
 * border) and/or WS_MAXIMIZEBOX (maximize button/double-click title bar/OS snap-to-maximize)
 * cleared, as requested. WS_CAPTION, WS_SYSMENU, WS_MINIMIZEBOX and WS_OVERLAPPED are always
 * kept, so minimizing and closing the window remain available regardless of the arguments.
 *
 * @param resizable Whether the player should be able to drag-resize the window.
 * @param maximizable Whether the window should be maximizable.
 * @return std::uint32_t The computed window style, suitable for
 * partial_window_creation_params::style or a direct SetWindowLongPtr(GWL_STYLE, ...) call.
 */
[[nodiscard]] std::uint32_t compute_window_style(bool resizable, bool maximizable) noexcept;

/**
 * @brief Base class for application windows that will be created by the engine on the Win32
 * platform. This class can be used to inherit from it and create platform-specific,
 * framework-specific application windows (e.g. ImGui application window, DirectX application
 * window, and so on).
 * This class has window procedure routing capabilities, so that each instance of the application
 * window class (and inheriting classes) can have its own window procedure, each with its own state,
 * logic etc.
 *
 * @note This class is not properly RAII since the creation of the window must be done manually
 * after the construction of the object. If you want pure RAII, you need to set the use_pure_raii
 * flag of the partial_window_creation_params struct to true, and then the window will be created in
 * the constructor of this class. This won't, though, route WM_CREATE messages and other creation
 * messages.
 */
class win32_application_window : public platform_specific_window<wil::unique_hwnd> {
protected:
    /**
     * @brief Create the window, making system calls with the given window class data and creation
     * parameters.
     */
    void create_window();

public:
    /**
     * @brief Constructs a new Win32 Application Window object with the given title and, optionally,
     *  with the description of the window class and the parameters for creating the window.
     *
     * @note  If the window creation parameters are not provided, default parameters will be used,
     * which will create a window without a parent, without a menu, and with the default size and
     * position. The window will be created with the WS_OVERLAPPEDWINDOW style, which is a common
     * style for windows.
     *
     * @note Please, note that if you pass a window class which is already registered but with
     * different data, the new data will be completely ignored and the window will be created with
     * the already registered class data.
     *
     * @note This constructor only registers the window class without creating the window because it
     * would not be possible, otherwise, to route creation messages to the appropriate instance of
     * the application window class, since the window procedure is set at the window class level,
     * and not at the window level. You need to manually call create_window() after construction.
     *
     * @param title The title of the window.
     * @param windowClassData (Optional) The data for the window class.
     * @param creationParams (Optional) The parameters for creating the window.
     */
    explicit win32_application_window(const platform_preferred_string_view_t title,
                                      partial_wndclassex windowClassData = {},
                                      partial_window_creation_params creationParams = {});
    ~win32_application_window() noexcept override;

    win32_application_window(const win32_application_window&) = delete;
    /**
     * @brief Moves the native window and its input collector to the new object, which then
     * receives the window's messages. The moved-from object is left without a window or collector.
     *
     * @note Move assignment destroys the target's previous window without routing its destruction
     * messages into the target.
     */
    win32_application_window(win32_application_window&& other) noexcept;
    win32_application_window& operator=(const win32_application_window&) = delete;
    win32_application_window& operator=(win32_application_window&& other) noexcept;

    /**
     * @brief Shows the application window with the normal mode.
     *
     * @note Can throw if UpdateWindow fails.
     */
    void show() override;

    /**
     * @brief Helper factory function used to create a window object ensuring a RAII-like behavior.
     * In this case, the window will be created immediately after the construction of the object, so
     * that we don't have to worry about calling create_window() manually after the construction,
     * and we can ensure that the window is properly initialized before use.
     *
     * @tparam Win32ApplicationWindowType The type of the window to be created. This type must
     * satisfy the Win32ApplicationWindow concept, meaning that it must be either of type
     * win32_application_window or it must derive from win32_application_window.
     *
     * @tparam Args The types of the arguments to be passed to the constructor of the window.
     * @param args The arguments to be passed to the constructor of the window.
     * @return std::unique_ptr<Win32ApplicationWindowType> A unique pointer to the created window
     * object.
     */
    template <Win32ApplicationWindow Win32ApplicationWindowType, typename... Args>
    [[nodiscard]] static std::unique_ptr<Win32ApplicationWindowType> two_phase_window_creation(
        Args&&... args) {
        auto window = std::make_unique<Win32ApplicationWindowType>(std::forward<Args>(args)...);
        window->create_window();
        return window;
    }

    void set_input_collector(input::input_collector* const inputCollector) {
        m_inputCollector = inputCollector;
    }

protected:
    /**
     * @brief Result type of a routed window procedure.
     *  If the routed window procedure handles the message, it should return an optional containing
     *  the result of the window procedure. If it doesn't handle the message, it should return an
     *  empty optional.
     */
    using route_result_t = std::optional<details::lresult_t>;

    /**
     * @brief Object-specific window procedure. This function is called by the static window
     * procedure after routing the message to the appropriate instance of the application window
     * class. If the object-specific window procedure handles the message, it should return an
     * optional containing the result of the window procedure. If it doesn't handle the message, it
     * should return an empty optional.
     *
     * @param hwnd The handle of the window receiving the message.
     * @param message The message being received.
     * @param wparam The WPARAM of the message being received.
     * @param lparam The LPARAM of the message being received.
     * @return route_result_t The result of the routed window procedure. Nullopt if the message was
     * not handled by the object-specific window procedure.
     */
    virtual route_result_t routed_window_procedure(HWND hwnd, windows_messages message,
                                                   details::wparam_t wparam,
                                                   details::lparam_t lparam) noexcept;

    /**
     * @brief Handler for implementations, called when create_window() is called.
     *
     * When this function is called, the window resources are guaranteed to be allocated and valid,
     * though the window is not visible yet. This can be useful to perform some initialization that
     * requires valid window resources, such as setting up the rendering manager with the window
     * handle, or other similar operations.
     *
     * @note The show() function should not be called within this function to ensure proper
     * resources initialization before showing.
     *
     * @note This function is only called if the use_pure_raii flag of the
     * partial_window_creation_params struct is set to false since C++ does not guarantee the proper
     * call of virtual methods during the construction of the object.
     */
    virtual void on_create();

private:
    partial_wndclassex m_windowClassData{};
    partial_window_creation_params m_creationParams{};
    input::input_collector* m_inputCollector{};
    // Whether Print Screen's key-down arrived: Windows usually sends only its key-up.
    bool m_printScreenDown{};
    // A WM_CHAR high surrogate waiting for its low one (0 if none).
    char16_t m_pendingHighSurrogate{};
    // The mouse buttons that went down and not up yet, one bit per mouse_button_index(): the
    // mouse stays captured while any is set.
    std::uint8_t m_heldMouseButtons{};
    // Whether the pointer is over the client area, as last reported to the input collector.
    bool m_pointerInside{};

    /**
     * @brief Win32 Application Window specific initializer function for WNDCLASSEX structs.
     * This function will set the following fields of the WNDCLASSEX struct:
     * - cbSize
     * - hInstance
     * - lpfnWndProc (set to the window procedure of this class)
     * - cbWndExtra (set to the size of a pointer to this class)
     *
     * @param wndClass The WNDCLASSEX struct to be initialized.
     */
    static void init_wndclassex(WNDCLASSEX* wndClass) noexcept;
    static_assert(details::WndClassInitializerFunction<decltype(&init_wndclassex)>);

    /**
     * @brief Helper alias used to identify a WINDCLASSEX structure with managed initialization and
     * destruction behavior.
     */
    using unique_wndclassex_t =
        details::unique_wndclassex<decltype(&win32_application_window::init_wndclassex),
                                   &win32_application_window::init_wndclassex>;

    /**
     * @brief Searches for a WNDCLASSEX struct with the given class name. If it finds one, it
     * returns a reference to it. If it doesn't find one, it creates a new WNDCLASSEX struct with
     * the given class name and data, initializes it and returns a reference to it.
     *
     * @note It will register the window class with the default hInstance and the window_procedure
     * of this class, so that the window procedure can be routed to the appropriate instance of the
     * application window class.
     *
     * @param className The name of the window class to search for or create.
     * @param windowClassData The data to initialize the WNDCLASSEX struct with if a new one is
     * created. This parameter is ignored if a WNDCLASSEX struct with the given class name is found.
     * @return const WNDCLASSEX& A reference to the WNDCLASSEX struct with the given class name.
     */
    static const WNDCLASSEX& get_or_create_wndclassex_by_name(
        platform_preferred_string_view_t className, const partial_wndclassex& windowClassData = {});

    /**
     * @brief Create a window object
     */
    void create_window_impl();

    /**
     * @brief The window procedure for this application window class.
     *
     * @return details::lresult_t The result of the window procedure.
     */
    static details::lresult_t CALLBACK window_procedure(HWND, details::message_id_t,
                                                        details::wparam_t, details::lparam_t);

    /**
     * @brief Routes the window procedure to the appropriate instance of the application window
     * class. This can be useful so that each object of the application window class (and inheriting
     * classes) can have its own window procedure, each with its own state, logic etc.
     *
     * @param hwnd The handle of the window receiving the message.
     * @param message The message being received.
     * @param wparam The WPARAM of the message being received.
     * @param lparam The LPARAM of the message being received.
     *
     * @return route_result_t The result of the routed window procedure. Nullopt if the message was
     * not handled by the object-specific window procedure.
     */
    static route_result_t route_window_procedure(HWND hwnd, windows_messages message,
                                                 details::wparam_t wparam,
                                                 details::lparam_t lparam) noexcept;

    void update_userdata() noexcept;
    void clear_userdata() noexcept;
};

static_assert(Win32ApplicationWindow<win32_application_window>);

} // namespace tca::engine::platform::win32
