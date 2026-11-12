// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

#ifndef _WIN32
#error "This header should only be included on Windows platforms."
#endif // _WIN32

// C++ STL
#include <string>
#include <string_view>
#include <type_traits>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <wil/stl.h>

namespace tca::engine::platform::win32::type_traits {

namespace details {

/**
 * @brief Type trait to determine the character type used by the Windows API functions based on
 * whether Unicode (wide) character encoding is used or not. This type trait uses the
 * char_type_traits struct to define the char_type alias, which will be either char or wchar_t
 * depending on the value of use_unicode() from the build_details namespace.
 */
template <bool UseUnicode>
struct char_type_traits {
    using char_type = char;
};

template <>
struct char_type_traits<true> {
    using char_type = wchar_t;
};

} // namespace details

/**
 * @brief Alias for the character type used by the Windows API functions, determined by the
 * char_type_traits type trait based on the value of use_unicode() from the build_details namespace.
 * This alias will be either char or wchar_t depending on whether Unicode (wide) character encoding
 * is used or not.
 */
using win32_char_t = typename details::char_type_traits<build_details::use_unicode()>::char_type;

/**
 * @brief Aliases for the string types used by the Windows API functions, which are based on the
 *  win32_char_t type. The win32_string_t is a std::basic_string that uses win32_char_t as the
 *  character type.
 */
using win32_string_t = std::basic_string<win32_char_t>;

/**
 * @brief Alias for the string view type used by the Windows API functions, which is based on
 * the win32_char_t type. The win32_string_view_t is a std::basic_string_view that uses
 * win32_char_t as the character type.
 */
using win32_string_view_t = std::basic_string_view<win32_char_t>;

/**
 * @brief Utility function to convert a std::string_view to a win32_string_t, which is the string
 * type used by the Windows API functions. This function will perform the necessary conversion based
 * on whether Unicode (wide) character encoding is used or not.
 *
 * @note Marking nodiscard since the function performs a conversion and returns a new string and it
 * may be expensive.
 *
 * @param str The input string view to be converted.
 * @return win32_string_t The converted string in the format used by the Windows API functions.
 */
[[nodiscard]] win32_string_t to_win32_string(std::string_view str);

/**
 * @brief Utility function to convert a win32_string_view_t to a std::string, which is the standard
 * string type used in C++. This function will perform the necessary conversion based on whether
 * Unicode (wide) character encoding is used or not.
 *
 * @param str The input string view to be converted.
 * @return std::string The converted string in the standard C++ string format.
 */
[[nodiscard]] std::string from_win32_string_view(win32_string_view_t str);

/**
 * @brief Concept representing raw Win32 API resources. They are pointers.
 */
template <typename TWin32Resource>
concept Win32PointerResource = std::is_pointer_v<TWin32Resource>;

static_assert(Win32PointerResource<HWND>);
static_assert(Win32PointerResource<HINSTANCE>);
static_assert(Win32PointerResource<HMODULE>);
static_assert(Win32PointerResource<HDC>);
static_assert(Win32PointerResource<HANDLE>);

/**
 * @brief Alias for non-owning Win32 resources. This alias is used to indicate that the
 * resource should only be used as a non-owning handle, just to interact with the Windows API
 * functions and the like and that the lifetime of the resource is managed elsewhere.
 *
 * @tparam TResource The type of the Win32 resource, which must satisfy the Win32PointerResource
 * concept (be a pointer type).
 */
template <Win32PointerResource TResource>
using non_owning_resource = TResource;

} // namespace tca::engine::platform::win32::type_traits
