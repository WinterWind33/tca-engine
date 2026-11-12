// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/win32/windows-type-traits.hpp"

// C++ STL
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>

namespace tca::engine::platform::win32::type_traits {

namespace {
void throw_if_failed(std::invocable auto&& func) {
    const errno_t result{std::forward<decltype(func)>(func)()};
    if (result != 0) {
        throw std::runtime_error(
            "Failed to convert string to wide or narrow characters. Error code: " +
            std::to_string(result));
    }
}
} // namespace

win32_string_t to_win32_string(const std::string_view str) {
    if (str.empty()) {
        return win32_string_t{};
    }

    if constexpr (build_details::use_unicode()) {
        // If Unicode is used, we need to convert the string to wide characters.
        // We use the mbstowcs_s function to perform the conversion, which is a safer version of
        // mbstowcs. Documentation can be found at:
        // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/mbstowcs-s-mbstowcs-s-l?view=msvc-170
        std::size_t neededSize{};
        // We need a null-terminated string for mbstowcs_s, so we create a
        // temporary one with the data of the input string and a null terminator.
        std::string zstr{str.data(), str.size()};
        throw_if_failed([&neededSize, &zstr]() -> errno_t {
            return mbstowcs_s(&neededSize, nullptr, 0, zstr.data(), zstr.size());
        });

        win32_string_t result(neededSize, TEXT('\0'));
        throw_if_failed([&result, &zstr, &neededSize]() -> errno_t {
            return mbstowcs_s(&neededSize, result.data(), result.size(), zstr.data(), zstr.size());
        });

        // Trim the null terminator added by mbstowcs_s, if present.
        if (!result.empty() && result.back() == TEXT('\0')) {
            result.pop_back();
        }

        return result;
    } else {
        // If Unicode is not used, we can directly construct the string.
        return win32_string_t(std::cbegin(str), std::cend(str));
    }
}

std::string from_win32_string_view(const win32_string_view_t str) {
    if (str.empty()) {
        return std::string{};
    }

    if constexpr (build_details::use_unicode()) {
        // If Unicode is used, we need to convert the string from wide characters to narrow
        // characters. We use the wcstombs_s function to perform the conversion, which is a safer
        // version of wcstombs. Documentation can be found at:
        // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-s-wcstombs-s-l?view=msvc-170
        std::size_t neededSize{};
        win32_string_t zstr{str.data(), str.size()};
        throw_if_failed([&neededSize, &zstr]() -> errno_t {
            return wcstombs_s(&neededSize, nullptr, 0, zstr.data(), zstr.size());
        });

        std::string result(neededSize, '\0');
        throw_if_failed([&result, &zstr, &neededSize]() -> errno_t {
            return wcstombs_s(&neededSize, result.data(), result.size(), zstr.data(), zstr.size());
        });

        // Trim the null terminator added by wcstombs_s, if present.
        if (!result.empty() && result.back() == '\0') {
            result.pop_back();
        }

        return result;
    } else {
        // If Unicode is not used, we can directly construct the string.
        return std::string(std::cbegin(str), std::cend(str));
    }
}

} // namespace tca::engine::platform::win32::type_traits
