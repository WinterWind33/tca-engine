// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <concepts>

#ifndef NDEBUG
#include <stdexcept>
#endif

namespace tca::engine::core {

/**
 * @brief Concept satisfied by those types that are either integrals or floating points.
 *
 * @tparam ValueType The type to check.
 */
template <typename ValueType>
concept NonZeroValueType = std::integral<ValueType> || std::floating_point<ValueType>;

static_assert(NonZeroValueType<int>);
static_assert(NonZeroValueType<float>);
static_assert(NonZeroValueType<double>);

#ifdef NDEBUG

/**
 * @brief In release builds non_zero_value is a plain alias for ValueType: no wrapping, no
 *  validation, zero overhead. The zero check only exists in debug builds.
 */
template <NonZeroValueType ValueType, ValueType... Tolerance>
using non_zero_value = ValueType;

#else

namespace details {

// CRTP mixin providing arithmetic operators for a wrapper type that is implicitly convertible to
// ValueType. Shared by every non_zero_value_impl specialization so the operators are written
// once. Results are plain ValueType, not the wrapper, since arithmetic does not preserve
// non-zero-ness (e.g. a - a == 0).
template <typename Derived, typename ValueType>
class arithmetic_ops {
public:
    friend constexpr ValueType operator+(const Derived& lhs, const Derived& rhs) noexcept {
        return static_cast<ValueType>(lhs) + static_cast<ValueType>(rhs);
    }

    friend constexpr ValueType operator+(const Derived& lhs, ValueType rhs) noexcept {
        return static_cast<ValueType>(lhs) + rhs;
    }

    friend constexpr ValueType operator+(ValueType lhs, const Derived& rhs) noexcept {
        return lhs + static_cast<ValueType>(rhs);
    }

    friend constexpr ValueType operator-(const Derived& lhs, const Derived& rhs) noexcept {
        return static_cast<ValueType>(lhs) - static_cast<ValueType>(rhs);
    }

    friend constexpr ValueType operator-(const Derived& lhs, ValueType rhs) noexcept {
        return static_cast<ValueType>(lhs) - rhs;
    }

    friend constexpr ValueType operator-(ValueType lhs, const Derived& rhs) noexcept {
        return lhs - static_cast<ValueType>(rhs);
    }

    friend constexpr ValueType operator*(const Derived& lhs, const Derived& rhs) noexcept {
        return static_cast<ValueType>(lhs) * static_cast<ValueType>(rhs);
    }

    friend constexpr ValueType operator*(const Derived& lhs, ValueType rhs) noexcept {
        return static_cast<ValueType>(lhs) * rhs;
    }

    friend constexpr ValueType operator*(ValueType lhs, const Derived& rhs) noexcept {
        return lhs * static_cast<ValueType>(rhs);
    }

    friend constexpr ValueType operator/(const Derived& lhs, const Derived& rhs) noexcept {
        return static_cast<ValueType>(lhs) / static_cast<ValueType>(rhs);
    }

    friend constexpr ValueType operator/(const Derived& lhs, ValueType rhs) noexcept {
        return static_cast<ValueType>(lhs) / rhs;
    }

    friend constexpr ValueType operator/(ValueType lhs, const Derived& rhs) noexcept {
        return lhs / static_cast<ValueType>(rhs);
    }

    friend constexpr ValueType operator-(const Derived& operand) noexcept {
        return -static_cast<ValueType>(operand);
    }
};

// Primary template is intentionally left undefined: Tolerance is a pack so that integral types
// can be specialized with zero non-type parameters (no tolerance at all), while floating point
// types can additionally be specialized with exactly one (an explicit tolerance).
template <typename ValueType, ValueType... Tolerance>
class non_zero_value_impl;

// Integral values must be exactly non-zero; no tolerance parameter exists for this type.
template <std::integral ValueType>
class non_zero_value_impl<ValueType>
    : public arithmetic_ops<non_zero_value_impl<ValueType>, ValueType> {
public:
    constexpr non_zero_value_impl(const ValueType value) : m_value{value} {
        if (m_value == ValueType{})
            throw std::invalid_argument{"non_zero_value: value must not be zero"};
    }

    constexpr operator ValueType() const noexcept {
        return m_value;
    }

private:
    ValueType m_value{};
};

// Floating point, no tolerance given: falls back to an exact-zero check.
template <std::floating_point ValueType>
class non_zero_value_impl<ValueType>
    : public arithmetic_ops<non_zero_value_impl<ValueType>, ValueType> {
public:
    constexpr non_zero_value_impl(const ValueType value) : m_value{value} {
        if (m_value == ValueType{})
            throw std::invalid_argument{"non_zero_value: value must not be zero"};
    }

    constexpr operator ValueType() const noexcept {
        return m_value;
    }

private:
    ValueType m_value{};
};

// Floating point with an explicit tolerance: values within [-Tolerance, Tolerance] are zero.
template <std::floating_point ValueType, ValueType Tolerance>
class non_zero_value_impl<ValueType, Tolerance>
    : public arithmetic_ops<non_zero_value_impl<ValueType, Tolerance>, ValueType> {
public:
    constexpr non_zero_value_impl(const ValueType value) : m_value{value} {
        if (m_value > -Tolerance && m_value < Tolerance)
            throw std::invalid_argument{"non_zero_value: value must not be zero"};
    }

    constexpr operator ValueType() const noexcept {
        return m_value;
    }

private:
    ValueType m_value{};
};

} // namespace details

/**
 * @brief Wraps a value that must never be zero. Constructing with a zero (or, for floating point
 *  types given an explicit Tolerance, near-zero) value throws std::invalid_argument. Converts
 *  implicitly back to ValueType so call sites are identical to release builds, where this is a
 *  plain alias.
 *
 * @tparam ValueType Integral or floating point type of the wrapped value.
 * @tparam Tolerance Only valid for floating point ValueType: values within [-Tolerance, Tolerance]
 *  are considered zero. Integral types do not accept a tolerance at all.
 */
template <NonZeroValueType ValueType, ValueType... Tolerance>
using non_zero_value = details::non_zero_value_impl<ValueType, Tolerance...>;

#endif // NDEBUG

} // namespace tca::engine::core
