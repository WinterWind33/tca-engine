// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <string>
#include <string_view>

#ifdef _WIN32
#include "platform/win32/windows-type-traits.hpp"
#endif // _WIN32

namespace tca::engine::platform {

/**
 * @brief Alias for the character type used by the engine, determined by the platform-specific type
 * traits. This alias will be used throughout the engine to ensure that the correct character type
 * is used based on the platform's requirements, such as using wchar_t for Windows when Unicode is
 * enabled, and char for other platforms or when Unicode is not enabled. This allows for better
 * compatibility and performance across different platforms while maintaining a consistent interface
 * for character handling within the engine.
 */
#ifdef _WIN32
using platform_preferred_char_t = win32::type_traits::win32_char_t;
#else
using platform_preferred_char_t = char;
#endif // _WIN32

using platform_preferred_string_t = std::basic_string<platform_preferred_char_t>;
using platform_preferred_string_view_t = std::basic_string_view<platform_preferred_char_t>;

/**
 * @brief Same as platform_preferred_char_t, but with shorter and more convenient names.
 *
 * @note Clients should use this alias prefixing it with "platform::" to avoid confusion with other
 * engine types.
 */
using char_t = platform_preferred_char_t;

/**
 * @brief Same as platform_preferred_string_t, but with shorter and more convenient names.
 *
 * @note Clients should use this alias prefixing it with "platform::" to avoid confusion with other
 * engine types.
 */
using string_t = platform_preferred_string_t;

/**
 * @brief Same as platform_preferred_string_view_t, but with shorter and more convenient names.
 *
 * @note Clients should use this alias prefixing it with "platform::" to avoid confusion with other
 * engine types.
 */
using string_view_t = platform_preferred_string_view_t;

} // namespace tca::engine::platform
