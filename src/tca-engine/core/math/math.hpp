// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cmath>
#include <concepts>
#include <limits>

namespace tca::engine::math {

/**
 * @brief Compares two floating-point numbers for near-equality, using a specified epsilon value to
 * determine the tolerance for comparison.
 *
 * @tparam T The type of the floating-point numbers to compare.
 * @param a The first floating-point number to compare.
 * @param b The second floating-point number to compare.
 * @param epsilon The tolerance for comparison, which determines how close the two numbers must be
 * to be considered nearly equal. The default value is the machine epsilon for the type T.
 *
 * @return True if the two numbers are nearly equal, false otherwise.
 */
template <typename T>
    requires(std::floating_point<T>)
bool is_nearly_equal(const T a, const T b, const T epsilon = std::numeric_limits<T>::epsilon()) {
    return std::abs(a - b) <= epsilon;
}

/**
 * @brief Compares a floating-point number to zero for near-equality, using a specified epsilon
 * value to determine the tolerance for comparison.
 *
 * @tparam T The type of the floating-point number to compare.
 * @param value The floating-point number to compare to zero.
 * @param epsilon The tolerance for comparison, which determines how close the number must be
 * to zero to be considered nearly zero. The default value is the machine epsilon for the type T.
 * @return True if the number is nearly zero, false otherwise.
 */
template <typename T>
    requires(std::floating_point<T>)
bool is_nearly_zero(const T value, const T epsilon = std::numeric_limits<T>::epsilon()) {
    return is_nearly_equal(value, std::decay_t<T>{}, epsilon);
}

} // namespace tca::engine::math
