// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Engine
#include "core/math/vector.hpp"

// C++ STL
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

// Third-party
#include <gsl/gsl>

namespace tca::engine::math {

/**
 * @brief A width/height pair. A plain aggregate on purpose, so it can be designated-initialized
 * (e.g. `{.width = 640, .height = 360}`), and unlike vector2 it carries no vector arithmetic: a
 * size describes an extent, not a displacement.
 *
 * @tparam T The type of each dimension, any arithmetic type.
 */
template <typename T>
    requires(std::is_arithmetic_v<T>)
struct size2 final {
    using value_type = T;

    value_type width{};
    value_type height{};

    constexpr bool operator==(const size2&) const noexcept = default;

    /**
     * @brief Tells whether the size covers no area, i.e. whether either dimension is zero or
     * negative. Floating-point dimensions are compared exactly: use is_nearly_zero() to also treat
     * dimensions within an epsilon of zero as zero.
     *
     * @return true if the width or the height is not positive.
     */
    constexpr bool is_empty() const noexcept {
        return width <= value_type{} || height <= value_type{};
    }

    /**
     * @brief Accesses a dimension by index, like std::array: 0 is the width, 1 is the height.
     *
     * @param index The dimension's index. Must be 0 or 1.
     * @return value_type& The width when index is 0, the height when it is 1.
     */
    constexpr value_type& operator[](const std::size_t index) noexcept {
        Expects(index < 2);
        return index == 0 ? width : height;
    }

    /**
     * @copydoc operator[](std::size_t)
     */
    constexpr const value_type& operator[](const std::size_t index) const noexcept {
        Expects(index < 2);
        return index == 0 ? width : height;
    }
};

/**
 * @brief Accesses a dimension at compile time, like std::get on a std::pair: 0 is the width, 1 is
 * the height. Together with the std::tuple_size and std::tuple_element specializations below, it
 * makes size2 usable with structured bindings and generic code written against get<I>().
 *
 * @tparam I The dimension's index, 0 or 1.
 * @param size The size to access.
 * @return The width when I is 0, the height when it is 1.
 */
template <std::size_t I, typename T>
    requires(I < 2)
constexpr T& get(size2<T>& size) noexcept {
    if constexpr (I == 0) {
        return size.width;
    } else {
        return size.height;
    }
}

/**
 * @copydoc get(size2<T>&)
 */
template <std::size_t I, typename T>
    requires(I < 2)
constexpr const T& get(const size2<T>& size) noexcept {
    if constexpr (I == 0) {
        return size.width;
    } else {
        return size.height;
    }
}

/**
 * @copydoc get(size2<T>&)
 */
template <std::size_t I, typename T>
    requires(I < 2)
constexpr T&& get(size2<T>&& size) noexcept {
    return std::move(get<I>(size));
}

/**
 * @brief A size in whole signed units, e.g. screen coordinates.
 */
using size2i = size2<std::int32_t>;

/**
 * @brief A size in whole unsigned units, e.g. texels.
 */
using size2u = size2<std::uint32_t>;

/**
 * @brief A size in single precision units.
 */
using size2f = size2<float>;

/**
 * @brief A size in double precision units, matching vector2's precision.
 */
using size2d = size2<double>;

/**
 * @brief Compares two floating-point sizes for near-equality, dimension by dimension, using a
 * specified epsilon value to determine the tolerance for comparison.
 *
 * @tparam T The floating-point type of each dimension.
 * @param a The first size to compare.
 * @param b The second size to compare.
 * @param epsilon The tolerance applied to each dimension. The default value is the machine epsilon
 * for the type T.
 * @return True if both the widths and the heights are nearly equal, false otherwise.
 */
template <typename T>
    requires(std::floating_point<T>)
bool is_nearly_equal(const size2<T> a, const size2<T> b,
                     const T epsilon = std::numeric_limits<T>::epsilon()) {
    return is_nearly_equal(a.width, b.width, epsilon) &&
           is_nearly_equal(a.height, b.height, epsilon);
}

/**
 * @brief Compares a floating-point size to the zero size for near-equality, dimension by
 * dimension, using a specified epsilon value to determine the tolerance for comparison.
 *
 * @tparam T The floating-point type of each dimension.
 * @param value The size to compare to the zero size.
 * @param epsilon The tolerance applied to each dimension. The default value is the machine epsilon
 * for the type T.
 * @return True if both the width and the height are nearly zero, false otherwise.
 */
template <typename T>
    requires(std::floating_point<T>)
bool is_nearly_zero(const size2<T> value, const T epsilon = std::numeric_limits<T>::epsilon()) {
    return is_nearly_equal(value, size2<T>{}, epsilon);
}

/**
 * @brief Converts a size into a vector2, with the width as the x component and the height as the
 * y component.
 *
 * @param size The size to convert.
 * @return vector2 The vector (width, height).
 */
template <typename T>
vector2 to_vector2(const size2<T> size) {
    return vector2{static_cast<double>(size.width), static_cast<double>(size.height)};
}

/**
 * @brief Converts a vector2 into a size, with the x component as the width and the y component as
 * the height. When T is an integral type each component is rounded to the nearest integer (halves
 * away from zero), so a value like 959.9999 coming out of a floating-point computation becomes 960
 * rather than being truncated to 959.
 *
 * @tparam T The type of each dimension of the resulting size.
 * @param vec The vector to convert. Each rounded component must be representable in T.
 * @return size2<T> The size {x, y}.
 */
template <typename T>
    requires(std::is_arithmetic_v<T>)
size2<T> to_size2(const vector2& vec) {
    if constexpr (std::integral<T>) {
        return size2<T>{.width = static_cast<T>(std::round(vec.x())),
                        .height = static_cast<T>(std::round(vec.y()))};
    } else {
        return size2<T>{.width = static_cast<T>(vec.x()), .height = static_cast<T>(vec.y())};
    }
}

} // namespace tca::engine::math

/**
 * @brief size2 has two elements, the width and the height.
 */
template <typename T>
struct std::tuple_size<tca::engine::math::size2<T>> : std::integral_constant<std::size_t, 2> {};

/**
 * @brief Both of size2's elements are of its value_type.
 */
template <std::size_t I, typename T>
    requires(I < 2)
struct std::tuple_element<I, tca::engine::math::size2<T>> {
    using type = T;
};
