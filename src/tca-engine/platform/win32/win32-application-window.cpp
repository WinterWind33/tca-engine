// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/win32/win32-application-window.hpp"

// C++ STL
#include <cstddef>
#include <cstdint>
#include <cstring> // for std::memset
#include <mutex>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <utility>

// Windows
#include "platform/win32/windows-messages.hpp"
#include "platform/win32/windows.hpp"

// Third-party
#include <wil/resource.h>
#include <wil/result.h>
#include <winuser.h>
#include <gsl/gsl>

#include "input/mouse-button.hpp"
#include "input/mouse-state.hpp"

#include "platform/platform-logger.hpp"
#include "platform/win32/win32-keyboard.hpp"
#include "platform/win32/windows-type-traits.hpp"

namespace tca::engine::platform::win32 {

namespace details {

void common_init_wndclassex(WNDCLASSEX* const wndClass) noexcept {
    // Zero the memory of the WNDCLASSEX struct to avoid junk memory and set the cbSize and
    // hInstance fields, which are common for all the WNDCLASSEX structs used in this application.
    std::memset(wndClass, 0, sizeof(WNDCLASSEX));

    wndClass->cbSize = sizeof(WNDCLASSEX);
    wndClass->hInstance = ::GetModuleHandle(nullptr);
}

void destroy_unique_wndclassex(WNDCLASSEX* const windowClass) noexcept {
    Expects(windowClass != nullptr);
    if (windowClass->lpszClassName == nullptr) {
        // If the class name is null, we can't unregister the class, since UnregisterClass needs a
        // valid class name to work.
        return;
    }

    // Unregister the window class associated with the given WNDCLASSEX struct.
    ::UnregisterClass(windowClass->lpszClassName, windowClass->hInstance);
}
} // namespace details

namespace {

/**
 * @brief Helper function to fill a WNDCLASSEX struct with the data from a partial_wndclassex
 * struct. This is used to avoid code duplication when we need to fill a WNDCLASSEX struct with
 * the data from a partial_wndclassex struct in different places.
 *
 * It uses the given map key class name to set the class name of the WNDCLASSEX struct, since the
 * WNDCLASSEX struct needs a pointer to a value which is valid until it goes out of scope.
 * Same goes for the menu name string, but it will also set the name.
 *
 * @param partialData The partial_wndclassex struct containing the data to fill the WNDCLASSEX
 * struct with.
 * @param mapKeyClassName The key used to store the WNDCLASSEX struct in the map.
 * @param wndClass The WNDCLASSEX struct to be filled with the data from the partial_wndclassex
 * struct.
 * @param menuName The menu name string to be set in the WNDCLASSEX struct.
 */
void fill_wndclass_with_partial_data(const partial_wndclassex& partialData,
                                     const platform_preferred_string_view_t mapKeyClassName,
                                     WNDCLASSEX& wndClass,
                                     platform_preferred_string_t& menuName) noexcept {
    // NOTE: we completely ignore the user defined window extra bytes, since we need to have the
    // extra bytes for the pointer to the application window instance.
    wndClass.style = partialData.style;
    wndClass.cbClsExtra = partialData.class_extra_bytes;
    wndClass.hIcon = partialData.icon;
    wndClass.hCursor = partialData.cursor;
    wndClass.hbrBackground = partialData.background_brush;
    menuName = partialData.menu_name;
    wndClass.lpszMenuName = menuName.empty() ? nullptr : menuName.c_str();
    wndClass.lpszClassName = mapKeyClassName.data();
    wndClass.hIconSm = partialData.small_icon;
}

constexpr auto KEYDOWN_LPARAM_PREVIOUS_KEY_STATE_BIT{30};

constexpr scan_code SCAN_CODE_EXTENDED_PREFIX{0xE000};

// Virtual keys that only exist on extended keys, but that MAPVK_VK_TO_VSC_EX maps to their
// numpad twin's scan code without the 0xE0 prefix (VK_LEFT gives 0x4B, numpad 4).
[[nodiscard]] constexpr bool is_extended_only_virtual_key(const UINT virtualKey) noexcept {
    switch (virtualKey) {
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_INSERT:
        case VK_DELETE:
            return true;
        default:
            return false;
    }
}

constexpr scan_code NUM_LOCK_PAUSE_SCAN_CODE{0x45};
constexpr scan_code LEFT_CONTROL_SCAN_CODE{0x1D};
constexpr scan_code RIGHT_ALT_SCAN_CODE{0xE038};

scan_code scan_code_from_message(details::wparam_t wparam, details::lparam_t lparam) noexcept {
    const auto virtualKey{gsl::narrow_cast<UINT>(wparam)};

    // Pause and Num Lock share scan code 0x45 (and MAPVK_VK_TO_VSC_EX gives Pause 0xE11D, its
    // E1-prefixed make code), so only the virtual key tells them apart.
    if (virtualKey == VK_PAUSE) {
        return NUM_LOCK_PAUSE_SCAN_CODE;
    }
    if (virtualKey == VK_NUMLOCK) {
        return SCAN_CODE_EXTENDED_PREFIX | NUM_LOCK_PAUSE_SCAN_CODE;
    }

    // Test the scan code byte alone: synthetic input with KEYEVENTF_EXTENDEDKEY but no scan code
    // comes out as 0xE000.
    const auto lparamScanCode{scan_code_from_lparam(lparam)};
    if ((lparamScanCode & 0xFF) != 0) {
        return lparamScanCode;
    }

    // Injected input (e.g. SendInput with only a virtual key) carries no scan code: recover it
    // from the virtual key. MAPVK_VK_TO_VSC_EX puts the 0xE0 prefix in the high byte for most
    // extended keys (right Ctrl, numpad divide), but not for the navigation cluster.
    auto scanCode{gsl::narrow_cast<scan_code>(::MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC_EX))};
    if (scanCode != 0 && (is_extended_only_virtual_key(virtualKey) ||
                          (lparamScanCode & SCAN_CODE_EXTENDED_PREFIX))) {
        scanCode |= SCAN_CODE_EXTENDED_PREFIX;
    }

    return scanCode;
}

[[nodiscard]] constexpr bool is_high_surrogate(const char16_t codeUnit) noexcept {
    return codeUnit >= 0xD800 && codeUnit <= 0xDBFF;
}

[[nodiscard]] constexpr bool is_low_surrogate(const char16_t codeUnit) noexcept {
    return codeUnit >= 0xDC00 && codeUnit <= 0xDFFF;
}

[[nodiscard]] constexpr bool is_key_down_message(const UINT message) noexcept {
    return message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
}

[[nodiscard]] constexpr bool is_key_up_message(const UINT message) noexcept {
    return message == WM_KEYUP || message == WM_SYSKEYUP;
}

/**
 * @brief True if this left Ctrl is the fake one Windows sends in front of AltGr: the message
 *  queued right behind it is right Alt going the same way, with the same message time. A sent
 *  message, with nothing queued behind it, is always a real left Ctrl.
 */
[[nodiscard]] bool is_altgr_fake_left_control(const HWND hwnd, const bool isKeyDown,
                                              const scan_code scanCode) noexcept {
    if (scanCode != LEFT_CONTROL_SCAN_CODE) {
        return false;
    }

    MSG next{};
    if (::PeekMessageW(&next, hwnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE | PM_NOYIELD) == 0) {
        return false;
    }

    const bool sameDirection{isKeyDown ? is_key_down_message(next.message)
                                       : is_key_up_message(next.message)};
    return sameDirection && next.wParam == VK_MENU &&
           scan_code_from_lparam(next.lParam) == RIGHT_ALT_SCAN_CODE &&
           static_cast<LONG>(next.time) == ::GetMessageTime();
}

/**
 * @brief The virtual key of the other Shift, if the given key is a Shift.
 */
[[nodiscard]] std::optional<int> other_shift_virtual_key(const input::key k) noexcept {
    switch (k) {
        case input::key::shift_left:
            return VK_RSHIFT;
        case input::key::shift_right:
            return VK_LSHIFT;
        default:
            return std::nullopt;
    }
}

constexpr bool is_app_activated(const details::wparam_t wparam) noexcept {
    return wparam == 1;
}

static_assert(input::MOUSE_BUTTONS_COUNT <= 8, "Every mouse button needs a bit of a std::uint8_t");

/**
 * @brief The bit of the given button in win32_application_window's held-buttons mask.
 */
[[nodiscard]] constexpr std::uint8_t mouse_button_bit(const input::mouse_button button) noexcept {
    return static_cast<std::uint8_t>(1U << input::mouse_button_index(button));
}

/**
 * @brief The position a client-area mouse message carries: two signed 16-bit words (what
 *  GET_X_LPARAM() / GET_Y_LPARAM() read), negative left of or above the client area during a
 *  capture. Never LOWORD()/HIWORD() alone, which would turn those into large positive values.
 */
[[nodiscard]] input::pointer_position pointer_position_from_lparam(
    const details::lparam_t lparam) noexcept {
    return {.x = static_cast<std::int16_t>(LOWORD(lparam)),
            .y = static_cast<std::int16_t>(HIWORD(lparam))};
}

/**
 * @brief How far a WM_MOUSEWHEEL / WM_MOUSEHWHEEL turned, in notches: Windows reports multiples
 *  (or, for smooth wheels and touchpads, fractions) of WHEEL_DELTA.
 */
[[nodiscard]] float wheel_notches_from_wparam(const details::wparam_t wparam) noexcept {
    return static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / static_cast<float>(WHEEL_DELTA);
}

/**
 * @brief Which side button a WM_XBUTTON* message is about, or nothing if it names neither.
 */
[[nodiscard]] std::optional<input::mouse_button> x_button_from_wparam(
    const details::wparam_t wparam) noexcept {
    switch (GET_XBUTTON_WPARAM(wparam)) {
        case XBUTTON1:
            return input::mouse_button::x1;
        case XBUTTON2:
            return input::mouse_button::x2;
        default:
            return std::nullopt;
    }
}

} // namespace

std::uint32_t compute_window_style(const bool resizable, const bool maximizable) noexcept {
    std::uint32_t style{WS_OVERLAPPEDWINDOW};

    if (!resizable) {
        style &= ~static_cast<std::uint32_t>(WS_THICKFRAME);
    }

    if (!maximizable) {
        style &= ~static_cast<std::uint32_t>(WS_MAXIMIZEBOX);
    }

    return style;
}

win32_application_window::win32_application_window(const platform_preferred_string_view_t title,
                                                   partial_wndclassex windowClassData,
                                                   partial_window_creation_params creationParams)
    : platform_specific_window<wil::unique_hwnd>{title},
      m_windowClassData{std::move(windowClassData)},
      m_creationParams{std::move(creationParams)} {
    if (m_creationParams.use_pure_raii) {
        create_window();
    }
}

win32_application_window::win32_application_window(win32_application_window&& other) noexcept
    : platform_specific_window{std::move(other)},
      m_windowClassData{std::move(other.m_windowClassData)},
      m_creationParams{std::move(other.m_creationParams)},
      m_inputCollector{std::exchange(other.m_inputCollector, nullptr)},
      m_printScreenDown{std::exchange(other.m_printScreenDown, false)},
      m_pendingHighSurrogate{std::exchange(other.m_pendingHighSurrogate, 0)},
      m_heldMouseButtons{std::exchange(other.m_heldMouseButtons, 0)},
      m_pointerInside{std::exchange(other.m_pointerInside, false)} {
    update_userdata();
}

win32_application_window::~win32_application_window() noexcept {
    // Remove the window long pointer data to avoid it being accessed after the window is destroyed,
    // since the window procedure may still receive messages after the window is destroyed, and it
    // may try to access the window long pointer data, which would be a dangling pointer at that
    // point.
    clear_userdata();
}

void win32_application_window::create_window() {
    if (get_native_handle().is_valid()) {
        get_platform_logger()->warn(
            "Trying to create a window with title '{}' but it already exists. Skipping the "
            "creation.",
            type_traits::from_win32_string_view(get_window_title()));
        // The window is already created, so we don't need to create it again.
        return;
    }

    create_window_impl();
    get_platform_logger()->info("Created a new window with title '{}'",
                                type_traits::from_win32_string_view(get_window_title()));
}

void win32_application_window::show() {
    const auto& nativeHandle{get_native_handle()};

    Expects(nativeHandle.get() != nullptr &&
            "Cannot show the application window because the native window handle is null.");

    // No need to check the return value of ShowWindow, since it can return false even if the window
    // was shown successfully. We can check the return value of UpdateWindow instead, which returns
    // false only if it fails to update the window.
    ::ShowWindow(nativeHandle.get(), SW_SHOW);
    THROW_LAST_ERROR_IF(!::UpdateWindow(nativeHandle.get()));
}

void win32_application_window::init_wndclassex(WNDCLASSEX* const wndClass) noexcept {
    details::common_init_wndclassex(wndClass);

    // Set the window procedure of this class.
    wndClass->lpfnWndProc = &win32_application_window::window_procedure;
    // We need extra bytes in the window class to store a pointer to the application window
    // instance, so we set the window extra bytes to the size of a pointer to the
    // win32_application_window class. This is needed to properly route the window procedure to
    // the appropriate instance of the application window class, since the window procedure is
    // set at the window class level, and not at the window level.
    wndClass->cbWndExtra = sizeof(win32_application_window*);
}

const WNDCLASSEX& win32_application_window::get_or_create_wndclassex_by_name(
    const platform_preferred_string_view_t className, const partial_wndclassex& windowClassData) {
    static std::mutex wndClassesMutex{};
    std::scoped_lock lock{wndClassesMutex};

    // This is an helper pair used as a value of the unordered map that stores the WNDCLASSEX
    // structs. It contains the WNDCLASSEX struct and the menu name associated with it. This is
    // needed to prevent junk memory when accessing the same WNDCLASSEX struct more than one
    // time (the internal strings would point to the old windowClassData memory, which is not
    // valid anymore).
    using map_value_pair = std::pair<unique_wndclassex_t, platform_preferred_string_t>;

    // We can use a static unordered map to store the WNDCLASSEX structs that we create, so that we
    // can return a reference to them when we need to.
    static std::unordered_map<platform_preferred_string_t, map_value_pair> wndClasses{};

    const platform_preferred_string_t classNameStr{className};

    // Try to find a WNDCLASSEX struct with the given class name in the map. If we find one, we
    // return a reference to it.
    if (const auto it{wndClasses.find(classNameStr)}; it != std::cend(wndClasses)) {
        return std::get<0>(std::get<1>(*it));
    }

    // Otherwise, we need to create a new WNDCLASSEX struct with the given class name and data,
    // initialize it and return a reference to it.
    // Firstly, create it in the map so that the initialization function can pre-fill the WNDCLASSEX
    // struct with the common data before we fill it with the data from the partial_wndclassex
    // struct.
    const auto [it, _]{wndClasses.emplace(
        classNameStr, map_value_pair{unique_wndclassex_t{}, platform_preferred_string_t{}})};

    const auto& keyClassName{std::get<0>(*it)};
    auto& wndClass{std::get<0>(std::get<1>(*it))};
    auto& menuName{std::get<1>(std::get<1>(*it))};

    fill_wndclass_with_partial_data(windowClassData, keyClassName, wndClass, menuName);

    // Then, register the class within the system.
    THROW_LAST_ERROR_IF(!::RegisterClassEx(&wndClass));
    return wndClass;
}

auto win32_application_window::routed_window_procedure(const HWND hwnd,
                                                       const windows_messages message,
                                                       const details::wparam_t wparam,
                                                       const details::lparam_t lparam) noexcept
    -> route_result_t {
    // A window nobody collects input from (e.g. one created on its own, outside an application)
    // simply drops its input messages.
    if (m_inputCollector == nullptr) {
        return std::nullopt;
    }

    auto processKeyDownEvent = [this, hwnd](const details::wparam_t wparam,
                                            const details::lparam_t lparam) {
        const scan_code scanCode{scan_code_from_message(wparam, lparam)};
        if (is_altgr_fake_left_control(hwnd, true, scanCode)) {
            return;
        }

        if (const auto optKey{key_from_scan_code(scanCode)}; optKey) {
            if (optKey.value() == input::key::print_screen) {
                m_printScreenDown = true;
            }

            m_inputCollector->on_key_down(optKey.value(),
                                          (lparam & (1 << KEYDOWN_LPARAM_PREVIOUS_KEY_STATE_BIT)));
        }
    };

    auto processKeyUpEvent = [this, hwnd](const details::wparam_t wparam,
                                          const details::lparam_t lparam) {
        const scan_code scanCode{scan_code_from_message(wparam, lparam)};
        if (is_altgr_fake_left_control(hwnd, false, scanCode)) {
            return;
        }

        const auto optKey{key_from_scan_code(scanCode)};
        if (!optKey) {
            return;
        }

        // Windows usually sends Print Screen's key-up alone (it keeps the key-down for its
        // hotkey): synthesize the press it never sent.
        if (optKey.value() == input::key::print_screen) {
            if (!m_printScreenDown) {
                m_inputCollector->on_key_down(input::key::print_screen, false);
            }
            m_printScreenDown = false;
        }

        m_inputCollector->on_key_up(optKey.value());

        // With both Shifts held, Windows sends a single key-up, for the last one let go. If the
        // other Shift isn't down any more for Windows either, its key-up is never coming.
        if (const auto otherShift{other_shift_virtual_key(optKey.value())};
            otherShift && (::GetKeyState(otherShift.value()) & 0x8000) == 0) {
            m_inputCollector->on_key_up(optKey.value() == input::key::shift_left
                                            ? input::key::shift_right
                                            : input::key::shift_left);
        }
    };

    // WM_CHAR carries UTF-16 code units: a code point above U+FFFF arrives as a high surrogate,
    // then a low one, in two messages. A half without its other half is dropped.
    auto processCharacterEvent = [this](const details::wparam_t wparam) {
        const auto codeUnit{gsl::narrow_cast<char16_t>(wparam)};
        if (is_high_surrogate(codeUnit)) {
            // A second high surrogate replaces the first, which never got its low one.
            m_pendingHighSurrogate = codeUnit;
            return;
        }

        if (is_low_surrogate(codeUnit)) {
            if (const char16_t highSurrogate{std::exchange(m_pendingHighSurrogate, char16_t{})};
                highSurrogate != 0) {
                m_inputCollector->on_char(0x10000 + ((char32_t{highSurrogate} - 0xD800) << 10) +
                                          (char32_t{codeUnit} - 0xDC00));
            }
            return;
        }

        m_inputCollector->on_char(char32_t{codeUnit});
    };

    // Every client-area mouse message says where the pointer is, and that is also where it tells
    // whether the pointer entered or left the client area (a captured drag keeps reporting it
    // outside). Windows only reports the leave when asked, once per request: TrackMouseEvent()
    // asks again on every entry.
    auto processPointerPosition = [this, hwnd](const details::lparam_t lparam) {
        const auto position{pointer_position_from_lparam(lparam)};
        m_inputCollector->on_pointer_moved(position);

        RECT client{};
        const bool inside{::GetClientRect(hwnd, &client) != 0 &&
                          ::PtInRect(&client, POINT{.x = position.x, .y = position.y}) != 0};
        if (inside == m_pointerInside) {
            return;
        }

        m_pointerInside = inside;
        if (inside) {
            TRACKMOUSEEVENT tracking{
                .cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = hwnd};
            std::ignore = ::TrackMouseEvent(&tracking);
            m_inputCollector->on_pointer_entered();
        } else {
            m_inputCollector->on_pointer_left();
        }
    };

    // The mouse is captured from the first button down to the last button up, so a drag that
    // leaves the window still gets its button-up.
    auto processMouseButtonDown = [this, hwnd, &processPointerPosition](
                                      const input::mouse_button button,
                                      const details::lparam_t lparam) {
        processPointerPosition(lparam);
        if (m_heldMouseButtons == 0) {
            ::SetCapture(hwnd);
        }

        m_heldMouseButtons =
            static_cast<std::uint8_t>(m_heldMouseButtons | mouse_button_bit(button));
        m_inputCollector->on_mouse_button_down(button);
    };

    auto processMouseButtonUp = [this, hwnd, &processPointerPosition](
                                    const input::mouse_button button,
                                    const details::lparam_t lparam) {
        processPointerPosition(lparam);
        m_heldMouseButtons =
            static_cast<std::uint8_t>(m_heldMouseButtons & ~mouse_button_bit(button));
        m_inputCollector->on_mouse_button_up(button);

        // Released after the button-up is recorded: the WM_CAPTURECHANGED it sends finds nothing
        // held, so it releases nothing twice.
        if (m_heldMouseButtons == 0 && ::GetCapture() == hwnd) {
            ::ReleaseCapture();
        }
    };

    auto processXButton = [](const details::wparam_t wparam, const auto& process,
                             const details::lparam_t lparam) {
        if (const auto button{x_button_from_wparam(wparam)}; button) {
            process(button.value(), lparam);
        }
    };

    // Another window took the capture (or the system did, e.g. for Alt+Tab): the button-ups of
    // the held buttons go there, so they are released here.
    auto processCaptureChanged = [this, hwnd](const details::lparam_t lparam) {
        if (reinterpret_cast<HWND>(lparam) == hwnd) {
            return;
        }

        for (std::size_t index{0}; index < input::MOUSE_BUTTONS_COUNT; ++index) {
            const auto button{static_cast<input::mouse_button>(index)};
            if ((m_heldMouseButtons & mouse_button_bit(button)) != 0) {
                m_inputCollector->on_mouse_button_up(button);
            }
        }
        m_heldMouseButtons = 0;
    };

    // On focus loss the collector releases every held button itself: forget them here first, so
    // the WM_CAPTURECHANGED that ReleaseCapture() sends has nothing left to release.
    auto releaseMouseCapture = [this, hwnd] {
        m_heldMouseButtons = 0;
        if (::GetCapture() == hwnd) {
            ::ReleaseCapture();
        }
    };

    // Let's process some input message
    switch (message) {
        case windows_messages::kill_focus:
            // To prevent unhandled input, undefine behaviors and games
            // going crazy over sticky keys, let's release all the held keys
            // in case the window loses focus.
            m_printScreenDown = false;
            m_pendingHighSurrogate = 0;
            releaseMouseCapture();
            m_inputCollector->on_focus_lost();
            break;
        case windows_messages::activate_app:
            if (!is_app_activated(wparam)) {
                releaseMouseCapture();
                m_inputCollector->on_focus_lost();
            }
            break;
        case windows_messages::mouse_move:
            processPointerPosition(lparam);
            break;
        case windows_messages::mouse_leave:
            if (m_pointerInside) {
                m_pointerInside = false;
                m_inputCollector->on_pointer_left();
            }
            break;
        // A double-click message takes the place of the second button-down.
        case windows_messages::left_button_down:
        case windows_messages::left_button_double_click:
            processMouseButtonDown(input::mouse_button::left, lparam);
            break;
        case windows_messages::left_button_up:
            processMouseButtonUp(input::mouse_button::left, lparam);
            break;
        case windows_messages::right_button_down:
        case windows_messages::right_button_double_click:
            processMouseButtonDown(input::mouse_button::right, lparam);
            break;
        case windows_messages::right_button_up:
            processMouseButtonUp(input::mouse_button::right, lparam);
            break;
        case windows_messages::middle_button_down:
        case windows_messages::middle_button_double_click:
            processMouseButtonDown(input::mouse_button::middle, lparam);
            break;
        case windows_messages::middle_button_up:
            processMouseButtonUp(input::mouse_button::middle, lparam);
            break;
        case windows_messages::x_button_down:
        case windows_messages::x_button_double_click:
            processXButton(wparam, processMouseButtonDown, lparam);
            break;
        case windows_messages::x_button_up:
            processXButton(wparam, processMouseButtonUp, lparam);
            break;
        case windows_messages::capture_changed:
            processCaptureChanged(lparam);
            break;
        // The wheel messages carry the pointer in screen coordinates: it isn't moved from them.
        case windows_messages::mouse_wheel:
            m_inputCollector->on_scroll({.vertical = wheel_notches_from_wparam(wparam)});
            break;
        case windows_messages::mouse_horizontal_wheel:
            m_inputCollector->on_scroll({.horizontal = wheel_notches_from_wparam(wparam)});
            break;
        case windows_messages::character:
            processCharacterEvent(wparam);
            break;
        case windows_messages::key_down:
        case windows_messages::sys_key_down:
            processKeyDownEvent(wparam, lparam);
            break;
        case windows_messages::key_up:
        case windows_messages::sys_key_up:
            processKeyUpEvent(wparam, lparam);
            break;
        default:
            break;
    }

    // By default, the routed window procedure doesn't handle any message, so we return an empty
    // optional.
    return std::nullopt;
}

void win32_application_window::create_window_impl() {
    // Firstly, if the window class data doesn't contain a class name, we give it the default
    // one.
    constexpr platform_preferred_string_view_t defaultClassName{TEXT("TCA_DEFAULT_WINDOW_CLASS")};
    if (m_windowClassData.class_name.empty()) {
        get_platform_logger()->warn(
            "No window class name provided for the window with title '{}'. Using the default class "
            "name '{}'.",
            type_traits::from_win32_string_view(get_window_title()),
            type_traits::from_win32_string_view(defaultClassName));
        m_windowClassData.class_name = defaultClassName;
    }

    // Search in the internal wndclass registry if the requested
    // wndclass already exists. If it doesn't exists, this call will create it and register it.
    const auto& wndclass =
        get_or_create_wndclassex_by_name(m_windowClassData.class_name, m_windowClassData);

    // We can create the window pointer now.
    const platform_preferred_string_t windowTitle{get_window_title()};
    wil::unique_hwnd windowHandle{::CreateWindowEx(
        0, wndclass.lpszClassName, windowTitle.c_str(), m_creationParams.style,
        m_creationParams.position.x, m_creationParams.position.y, m_creationParams.size.width,
        m_creationParams.size.height, m_creationParams.parent_window_handle,
        m_creationParams.menu_handle, wndclass.hInstance, static_cast<LPVOID>(this))};

    THROW_LAST_ERROR_IF_NULL(windowHandle.get());

    // We can finally assign the window handle to the application window instance.
    assign_native_handle(std::move(windowHandle));

    // We can call the on_create callback now, since the window is created and we can safely call
    // it without worrying about the window not being created yet.
    if (!m_creationParams.use_pure_raii) {
        on_create();
    }
}

details::lresult_t CALLBACK win32_application_window::window_procedure(
    const HWND hwnd, const details::message_id_t message, const details::wparam_t wparam,
    const details::lparam_t lparam) {
    // Route the window procedure to the appropriate instance of the application window class.
    // If the routed window procedure handles the message, it should return an optional
    // containing the result of the window procedure. If it doesn't handle the message, it
    // should return an empty optional.
    if (const auto routeResult{
            route_window_procedure(hwnd, to_windows_message(message), wparam, lparam)};
        routeResult.has_value()) {
        return routeResult.value();
    }

    // If the message wasn't properly handled by the routed window procedure, we handle it here.
    switch (message) {
        case WM_CLOSE:
            // We can call DestroyWindow here to trigger the destruction of the window, which will
            // cause a WM_DESTROY message to be sent, which we handle in the next case. This is
            // needed to properly destroy the window when it receives a WM_CLOSE message, which is
            // sent when the user tries to close the window, for example by clicking the close
            // button on the title bar.
            ::DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }

    return ::DefWindowProc(hwnd, message, wparam, lparam);
}

auto win32_application_window::route_window_procedure(const HWND hwnd,
                                                      const windows_messages message,
                                                      const details::wparam_t wparam,
                                                      const details::lparam_t lparam) noexcept
    -> route_result_t {
    // Retrieve the pointer to the application window instance associated with the given window
    // handle. This pointer should have been set as the user data of the window when the window
    // was created.
    if (const auto windowPtr{
            reinterpret_cast<win32_application_window*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA))};
        windowPtr != nullptr) {
        // Call the object-specific window procedure. If the object-specific window procedure
        // handles the message, it should return an optional containing the result of the window
        // procedure. If it doesn't handle the message, it should return an empty optional.
        return windowPtr->routed_window_procedure(hwnd, message, wparam, lparam);
    } else {
        // Try to intercept the NCCREATE message, which is the first message received by the window
        // procedure when a window is created, to set the user data of the window to the pointer to
        // the application window instance being created.
        if (message == windows_messages::nc_create) {
            const gsl::not_null<LPCREATESTRUCT> createStructPtr{
                reinterpret_cast<LPCREATESTRUCT>(lparam)};
            const gsl::not_null<win32_application_window*> createdWindowPtr{
                reinterpret_cast<win32_application_window*>(createStructPtr->lpCreateParams)};
            ::SetWindowLongPtr(hwnd, GWLP_USERDATA,
                               reinterpret_cast<LONG_PTR>(createdWindowPtr.get()));
        }
    }

    // If we couldn't retrieve a valid pointer to an application window instance, we return an
    // empty optional.
    return std::nullopt;
}

void win32_application_window::on_create() {}

void win32_application_window::update_userdata() noexcept {
    const auto& handle{get_native_handle()};
    if (!handle) {
        return;
    }

    ::SetWindowLongPtrW(handle.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

void win32_application_window::clear_userdata() noexcept {
    const auto& handle{get_native_handle()};
    if (!handle) {
        return;
    }

    ::SetWindowLongPtrW(handle.get(), GWLP_USERDATA, 0);
}

win32_application_window& win32_application_window::operator=(
    win32_application_window&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    // The base assignment destroys our old window: detach it first, so its destruction messages
    // aren't routed into this object while it is halfway through being overwritten.
    clear_userdata();
    platform_specific_window<wil::unique_hwnd>::operator=(std::move(other));
    m_windowClassData = std::move(other.m_windowClassData);
    m_creationParams = std::move(other.m_creationParams);
    m_inputCollector = std::exchange(other.m_inputCollector, nullptr);
    m_printScreenDown = std::exchange(other.m_printScreenDown, false);
    m_pendingHighSurrogate = std::exchange(other.m_pendingHighSurrogate, 0);
    m_heldMouseButtons = std::exchange(other.m_heldMouseButtons, 0);
    m_pointerInside = std::exchange(other.m_pointerInside, false);
    update_userdata();

    return *this;
}

} // namespace tca::engine::platform::win32
