// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

/**
 * This header file serves only as a wrapper for Windows.h along with the macros defined in
 * windows-common-macros.hpp.
 */

#ifndef _WIN32
#error "This header should only be included on Windows platforms."
#endif // _WIN32

// Windows.h (_WINDOWS_ is its include guard) got in first without NOMINMAX, directly or through a
// header that includes it (WIL, <d3d12.h>, <wincodec.h>, ...). Its min/max macros then break any
// later std::numeric_limits<T>::max() or std::min/std::max, often in an unrelated header.
#if defined(_WINDOWS_) && (defined(min) || defined(max))
#error \
    "Windows.h was included before platform/win32/windows.hpp, leaving the min/max macros defined. Include platform/win32/windows.hpp before any header that pulls in Windows.h."
#endif

#include "platform/win32/windows-common-macros.hpp"

#include <Windows.h>

#ifndef LW32
#define LW32(x) TEXT(x)
#endif // LW32

namespace tca::engine::platform::win32::build_details {

#ifdef UNICODE
/**
 * @brief Determines whether the Windows API functions should use Unicode (wide) character encoding
 * or not.
 *
 * @return true if Unicode (wide) character encoding is used, false otherwise.
 */
consteval bool use_unicode() noexcept {
    return true;
}
#else
/**
 * @brief Determines whether the Windows API functions should use Unicode (wide) character encoding
 * or not.
 *
 * @return true if Unicode (wide) character encoding is used, false otherwise.
 */
consteval bool use_unicode() noexcept {
    return false;
}
#endif // UNICODE

} // namespace tca::engine::platform::win32::build_details
