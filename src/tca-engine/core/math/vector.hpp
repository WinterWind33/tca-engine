// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Engine
#include "core/math/math.hpp"

// C++ STL
#include <limits>

// Third-party
#include <Eigen/Dense>

namespace tca::engine::math {

/**
 * @brief Represents a 2D vector, implemented using double precision floating point numbers. It
 * extends the Eigen::Vector2d class, so it inherits all its functionalities and operators, and adds
 * some utility functions and constructors.
 */
class vector2 : public Eigen::Vector2d {
public:
    vector2() noexcept(noexcept(Eigen::Vector2d{0.0, 0.0})) : Eigen::Vector2d{0.0, 0.0} {}

    /**
     * @brief Constructs a new vector2 object with the given x and y components.
     *
     * @param x The x component of the vector.
     * @param y The y component of the vector.
     */
    explicit vector2(const double x, const double y) noexcept(noexcept(Eigen::Vector2d{0.0, 0.0}))
        : Eigen::Vector2d(x, y) {}

    /**
     * @brief Constructs a new vector2 object from an Eigen::Vector2d object, by moving it to avoid
     * unnecessary copying.
     *
     * @param vec The Eigen::Vector2d object to move and construct the vector2 object from.
     */
    explicit vector2(Eigen::Vector2d vec) noexcept : Eigen::Vector2d(std::move(vec)) {}

    vector2(const vector2&) noexcept(noexcept(Eigen::Vector2d{Eigen::Vector2d{}})) = default;
    vector2(vector2&&) noexcept = default;
    vector2& operator=(const vector2&) noexcept(noexcept(Eigen::Vector2d{})) = default;
    vector2& operator=(vector2&&) noexcept = default;

    using Eigen::Vector2d::operator=;
    using Eigen::Vector2d::x;
    using Eigen::Vector2d::y;
    using Eigen::Vector2d::operator+;
    using Eigen::Vector2d::operator-;
    using Eigen::Vector2d::operator*;
    using Eigen::Vector2d::operator/;

    /**
     * @brief Returns a vector2 object with both components set to zero.
     *
     * @return vector2 A vector2 object with both components set to zero.
     */
    static vector2 zero() noexcept(noexcept(Eigen::Vector2d{})) {
        return vector2{0.0, 0.0};
    }

    /**
     * @brief Returns a vector2 object representing the x-axis unit vector (1, 0).
     *
     * @return vector2 A vector2 object representing the x-axis unit vector (1, 0).
     */
    static vector2 x_axis_vector() noexcept(noexcept(Eigen::Vector2d{1.0, 0.0})) {
        return vector2{1.0, 0.0};
    }

    /**
     * @brief Returns a vector2 object representing the y-axis unit vector (0, 1).
     *
     * @return vector2 A vector2 object representing the y-axis unit vector (0, 1).
     */
    static vector2 y_axis_vector() noexcept(noexcept(Eigen::Vector2d{0.0, 1.0})) {
        return vector2{0.0, 1.0};
    }
};

/**
 * @brief Compares two vector2 objects for near-equality, component by component, using a specified
 * epsilon value to determine the tolerance for comparison.
 *
 * @param a The first vector to compare.
 * @param b The second vector to compare.
 * @param epsilon The tolerance applied to each component. The default value is the machine epsilon
 * for double.
 * @return True if both the x and y components are nearly equal, false otherwise.
 */
inline bool is_nearly_equal(const vector2& a, const vector2& b,
                            const double epsilon = std::numeric_limits<double>::epsilon()) {
    return is_nearly_equal(a.x(), b.x(), epsilon) && is_nearly_equal(a.y(), b.y(), epsilon);
}

/**
 * @brief Compares a vector2 object to the zero vector for near-equality, component by component,
 * using a specified epsilon value to determine the tolerance for comparison.
 *
 * @param value The vector to compare to the zero vector.
 * @param epsilon The tolerance applied to each component. The default value is the machine epsilon
 * for double.
 * @return True if both the x and y components are nearly zero, false otherwise.
 */
inline bool is_nearly_zero(const vector2& value,
                           const double epsilon = std::numeric_limits<double>::epsilon()) {
    return is_nearly_equal(value, vector2::zero(), epsilon);
}

/**
 * @brief Represents a 3D vector, implemented using double precision floating point numbers.
 * Simple alias of Eigen::Vector3d for now.
 */
using vector3 = Eigen::Vector3d;

} // namespace tca::engine::math
