// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <concepts>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace tca::engine::core {

/**
 * @brief Concept satisfied by those types a ranged_value can wrap: totally ordered (so a value can
 *  be checked against a range) and cheaply, safely copyable (a ranged_value hands out copies of
 *  its value through noexcept accessors).
 *
 * @tparam ValueType The type to check.
 */
template <typename ValueType>
concept RangeableType = std::totally_ordered<ValueType> && std::copyable<ValueType> &&
                        std::is_nothrow_copy_constructible_v<ValueType>;

static_assert(RangeableType<int>);
static_assert(RangeableType<float>);
static_assert(RangeableType<double>);

/**
 * @brief Concept satisfied by the type of a ranged_value bound: ValueType must be constructible
 *  from it. That lets a bound be given as a plain constant even when ValueType itself can not be a
 *  non-type template argument (e.g. `1` for a std::chrono::nanoseconds range).
 *
 * @tparam BoundType The type of the bound.
 * @tparam ValueType The type the ranged_value wraps.
 */
template <typename BoundType, typename ValueType>
concept RangeBoundFor = std::constructible_from<ValueType, BoundType>;

/**
 * @brief Represents the policy to be applied to a ranged value.
 *  It is used to check the value against the given range applying the
 *  correct comparison operator.
 */
enum class rv_bound_policy {
    /**
     * @brief If closed is specified, every value is checked against a closed
     *  range, i.e. [min, max] with min and max included (x == max will pass the check).
     */
    closed,

    /**
     * @brief If max_open is specified, every value is checked against a half-open
     *  range, where the max value is non inclusive: [min, max) => x == max will fail.
     */
    max_open,

    /**
     * @brief If min_open is specified, every value is checked against a half-open
     *  range, where the min value is non inclusive: (min, max] => x == min will fail.
     */
    min_open,

    /**
     * @brief If open is specified, every value is checked against an open range, where
     *  neither bound is inclusive: (min, max) => both x == min and x == max will fail.
     */
    open,
};

/**
 * @brief Thrown when a ranged_value is constructed or assigned a value outside the range its
 *  bounds and bound policy describe.
 */
class ranged_value_exception : public std::out_of_range {
public:
    /**
     * @brief Constructs the exception with a descriptive message.
     * @param message Explains which value was rejected and why.
     */
    explicit ranged_value_exception(const std::string_view message)
        : std::out_of_range{message.data()} {}
};

namespace details {

// Whether val falls in [min, max] (closed), [min, max) (max_open), (min, max] (min_open) or
// (min, max) (open).
template <RangeableType Type>
constexpr bool is_within_range(const Type& val, const Type& min, const Type& max,
                               const rv_bound_policy policy) noexcept {
    switch (policy) {
        case rv_bound_policy::closed:
            return val >= min && val <= max;
        case rv_bound_policy::max_open:
            return val >= min && val < max;
        case rv_bound_policy::min_open:
            return val > min && val <= max;
        case rv_bound_policy::open:
            return val > min && val < max;
    }
    return false;
}

// Whether a value-initialized Type (zero for arithmetic types and durations) belongs to the
// range. Picks the ranged_value_impl specialization, as only then is there a valid default value.
template <typename Type, auto Min, auto Max, rv_bound_policy Policy>
concept value_initialized_within_range =
    std::default_initializable<Type> && is_within_range(Type{}, Type{Min}, Type{Max}, Policy);

// Includes the rejected value in the message when Type can be formatted.
template <RangeableType Type>
std::string out_of_range_message(const Type& val) {
    if constexpr (std::formattable<Type, char>) {
        return std::format("The given value - {} - is out of range", val);
    } else {
        return "The given value is out of range";
    }
}

// Min and Max are plain `auto` here (and on everything else in `details`) rather than
// `RangeBoundFor<Type> auto`: MSVC fails to match partial specializations declared with a
// constrained placeholder, leaving every ranged_value_impl an undefined type. The public
// ranged_value / debug_ranged_value aliases still check RangeBoundFor.
template <RangeableType Type, auto Min, auto Max, rv_bound_policy Policy>
class ranged_value_impl;

/**
 * @brief A value of value_type constrained to [Min, Max] - or to [Min, Max), (Min, Max] or
 *  (Min, Max) depending on Policy. Throws ranged_value_exception when constructed or assigned a
 * value outside that range. Implicitly convertible back to value_type.
 * @note This specialization is picked when a value-initialized value_type is outside the range
 *  (or value_type can not be default-constructed), so it has no default constructor (there would
 *  be no valid default value to give it).
 */
template <RangeableType Type, auto Min, auto Max, rv_bound_policy Policy>
    requires(!value_initialized_within_range<Type, Min, Max, Policy>)
class ranged_value_impl<Type, Min, Max, Policy> final {
public:
    using value_type = Type;

    /**
     * @brief The lower bound of the allowed range, inclusive unless the bound policy is
     *  rv_bound_policy::min_open or rv_bound_policy::open.
     */
    constexpr static value_type MIN() noexcept {
        return value_type{Min};
    }

    /**
     * @brief The upper bound of the allowed range, inclusive unless the bound policy is
     *  rv_bound_policy::max_open or rv_bound_policy::open.
     */
    constexpr static value_type MAX() noexcept {
        return value_type{Max};
    }

    /**
     * @brief The policy deciding whether MIN() and MAX() are part of the allowed range.
     */
    constexpr static rv_bound_policy BOUND_POLICY() noexcept {
        return Policy;
    }

    /**
     * @brief Constructs a ranged value.
     * @param val Must be within the allowed range, otherwise ranged_value_exception is thrown.
     */
    constexpr ranged_value_impl(const value_type val) : m_val{val} {
        if (!is_within_range(val, MIN(), MAX(), BOUND_POLICY())) {
            throw ranged_value_exception(out_of_range_message(val));
        }
    }

    /**
     * @brief Implicitly converts back to the underlying value.
     */
    constexpr operator value_type() const noexcept {
        return m_val;
    }

    /**
     * @brief Retrieves the underlying value.
     */
    constexpr value_type get() const noexcept {
        return m_val;
    }

    /**
     * @brief Compares two ranged values of the same type.
     */
    constexpr auto operator<=>(const ranged_value_impl&) const noexcept = default;

    // Templated rather than fixed to `value_type` so a literal of a different-but-comparable type
    // (e.g. comparing against a plain `int`) binds directly instead of ambiguously competing with
    // the implicit conversion to value_type (MSVC C2666). Excluded for Other == ranged_value_impl,
    // which the defaulted <=> above already handles (and which would otherwise trip a GCC bug).
    template <typename Other>
        requires(!std::same_as<Other, ranged_value_impl>) &&
                std::three_way_comparable_with<value_type, Other>
    constexpr auto operator<=>(const Other& val) const noexcept {
        return m_val <=> val;
    }

    // Same rationale as operator<=> above.
    template <typename Other>
        requires(!std::same_as<Other, ranged_value_impl>) &&
                std::equality_comparable_with<value_type, Other>
    constexpr bool operator==(const Other& val) const noexcept {
        return m_val == val;
    }

    /**
     * @brief Assigns a new value.
     * @param val Must be within the allowed range, otherwise ranged_value_exception is thrown.
     */
    ranged_value_impl& operator=(const value_type val) {
        if (!is_within_range(val, MIN(), MAX(), BOUND_POLICY())) {
            throw ranged_value_exception(out_of_range_message(val));
        }
        m_val = val;
        return *this;
    }

private:
    value_type m_val;
};

/**
 * @brief A value of value_type constrained to [Min, Max] - or to [Min, Max), (Min, Max] or
 *  (Min, Max) depending on Policy. Throws ranged_value_exception when constructed or assigned a
 * value outside that range. Implicitly convertible back to value_type.
 * @note This specialization is picked when a value-initialized value_type (zero for arithmetic
 *  types and durations) falls within the range, so it is also default-constructible, defaulting to
 *  that value.
 */
template <RangeableType Type, auto Min, auto Max, rv_bound_policy Policy>
    requires(value_initialized_within_range<Type, Min, Max, Policy>)
class ranged_value_impl<Type, Min, Max, Policy> final {
public:
    using value_type = Type;

    /**
     * @brief The lower bound of the allowed range, inclusive unless the bound policy is
     *  rv_bound_policy::min_open or rv_bound_policy::open.
     */
    constexpr static value_type MIN() noexcept {
        return value_type{Min};
    }

    /**
     * @brief The upper bound of the allowed range, inclusive unless the bound policy is
     *  rv_bound_policy::max_open or rv_bound_policy::open.
     */
    constexpr static value_type MAX() noexcept {
        return value_type{Max};
    }

    /**
     * @brief The policy deciding whether MIN() and MAX() are part of the allowed range.
     */
    constexpr static rv_bound_policy BOUND_POLICY() noexcept {
        return Policy;
    }

    /**
     * @brief Constructs a ranged value holding a value-initialized value_type.
     */
    constexpr ranged_value_impl() noexcept = default;

    /**
     * @brief Constructs a ranged value.
     * @param val Must be within the allowed range, otherwise ranged_value_exception is thrown.
     */
    constexpr ranged_value_impl(const value_type val) : m_val{val} {
        if (!is_within_range(val, MIN(), MAX(), BOUND_POLICY())) {
            throw ranged_value_exception(out_of_range_message(val));
        }
    }

    /**
     * @brief Implicitly converts back to the underlying value.
     */
    constexpr operator value_type() const noexcept {
        return m_val;
    }

    /**
     * @brief Retrieves the underlying value.
     */
    constexpr value_type get() const noexcept {
        return m_val;
    }

    /**
     * @brief Compares two ranged values of the same type.
     */
    constexpr auto operator<=>(const ranged_value_impl&) const noexcept = default;

    // See the other specialization's operator<=> for the rationale (same here).
    template <typename Other>
        requires(!std::same_as<Other, ranged_value_impl>) &&
                std::three_way_comparable_with<value_type, Other>
    constexpr auto operator<=>(const Other& val) const noexcept {
        return m_val <=> val;
    }

    template <typename Other>
        requires(!std::same_as<Other, ranged_value_impl>) &&
                std::equality_comparable_with<value_type, Other>
    constexpr bool operator==(const Other& val) const noexcept {
        return m_val == val;
    }

    /**
     * @brief Assigns a new value.
     * @param val Must be within the allowed range, otherwise ranged_value_exception is thrown.
     */
    ranged_value_impl& operator=(const value_type val) {
        if (!is_within_range(val, MIN(), MAX(), BOUND_POLICY())) {
            throw ranged_value_exception(out_of_range_message(val));
        }
        m_val = val;
        return *this;
    }

private:
    // Value-initialized so that `ranged_value<...> value;` holds the same in-range default as
    // `ranged_value<...> value{};`, rather than an indeterminate one.
    value_type m_val{};
};

// Integral arithmetic that would leave the type's own range can't be range-checked afterwards:
// an unsigned value wraps around (0 - 1 is max(), which a [0, max()] range accepts) and a signed
// one is undefined behaviour. These tell, before stepping, whether val + delta / val - delta would
// do that. Other types are left to their own arithmetic.
template <std::integral Type>
constexpr bool addition_overflows(const Type val, const Type delta) noexcept {
    if constexpr (std::is_unsigned_v<Type>) {
        return delta > std::numeric_limits<Type>::max() - val;
    } else {
        return (delta > 0 && val > std::numeric_limits<Type>::max() - delta) ||
               (delta < 0 && val < std::numeric_limits<Type>::min() - delta);
    }
}

template <std::integral Type>
constexpr bool subtraction_overflows(const Type val, const Type delta) noexcept {
    if constexpr (std::is_unsigned_v<Type>) {
        return delta > val;
    } else {
        return (delta < 0 && val > std::numeric_limits<Type>::max() + delta) ||
               (delta > 0 && val < std::numeric_limits<Type>::min() + delta);
    }
}

[[noreturn]] inline void throw_overflow(const std::string_view operation) {
    throw ranged_value_exception(
        std::format("{} would overflow the ranged_value's value_type", operation));
}

// Defined inside `details` (rather than the enclosing namespace) so ADL can find them through
// the `ranged_value` alias regardless of the caller's `using` declarations. Each is available
// whenever value_type has the matching operator of its own, which is what it steps the value
// with, and re-validates the result against the allowed range (leaving the value untouched when
// it throws).

// Increments by one step, re-validating the new value against the allowed range.
template <RangeableType Type, auto Min, auto Max, rv_bound_policy Policy>
    requires requires(Type val) { ++val; }
constexpr ranged_value_impl<Type, Min, Max, Policy>& operator++(
    ranged_value_impl<Type, Min, Max, Policy>& rv) {
    Type val{rv.get()};
    if constexpr (std::integral<Type>) {
        if (val == std::numeric_limits<Type>::max()) {
            throw_overflow("Incrementing");
        }
    }
    ++val;
    rv = val;
    return rv;
}

// Increments by one step, returning the pre-increment value.
template <RangeableType Type, auto Min, auto Max, rv_bound_policy Policy>
    requires requires(Type val) { ++val; }
constexpr ranged_value_impl<Type, Min, Max, Policy> operator++(
    ranged_value_impl<Type, Min, Max, Policy>& rv, int) {
    const ranged_value_impl<Type, Min, Max, Policy> copy{rv};
    ++rv;
    return copy;
}

// Decrements by one step, re-validating the new value against the allowed range.
template <RangeableType Type, auto Min, auto Max, rv_bound_policy Policy>
    requires requires(Type val) { --val; }
constexpr ranged_value_impl<Type, Min, Max, Policy>& operator--(
    ranged_value_impl<Type, Min, Max, Policy>& rv) {
    Type val{rv.get()};
    if constexpr (std::integral<Type>) {
        if (val == std::numeric_limits<Type>::min()) {
            throw_overflow("Decrementing");
        }
    }
    --val;
    rv = val;
    return rv;
}

// Decrements by one step, returning the pre-decrement value.
template <RangeableType Type, auto Min, auto Max, rv_bound_policy Policy>
    requires requires(Type val) { --val; }
constexpr ranged_value_impl<Type, Min, Max, Policy> operator--(
    ranged_value_impl<Type, Min, Max, Policy>& rv, int) {
    const ranged_value_impl<Type, Min, Max, Policy> copy{rv};
    --rv;
    return copy;
}

// Adds delta, re-validating the new value against the allowed range.
template <RangeableType Type, auto Min, auto Max, rv_bound_policy Policy>
    requires requires(Type val, Type delta) { val += delta; }
constexpr ranged_value_impl<Type, Min, Max, Policy>& operator+=(
    ranged_value_impl<Type, Min, Max, Policy>& rv, const std::type_identity_t<Type>& delta) {
    Type val{rv.get()};
    if constexpr (std::integral<Type>) {
        if (addition_overflows(val, delta)) {
            throw_overflow("Adding");
        }
    }
    val += delta;
    rv = val;
    return rv;
}

// Subtracts delta, re-validating the new value against the allowed range.
template <RangeableType Type, auto Min, auto Max, rv_bound_policy Policy>
    requires requires(Type val, Type delta) { val -= delta; }
constexpr ranged_value_impl<Type, Min, Max, Policy>& operator-=(
    ranged_value_impl<Type, Min, Max, Policy>& rv, const std::type_identity_t<Type>& delta) {
    Type val{rv.get()};
    if constexpr (std::integral<Type>) {
        if (subtraction_overflows(val, delta)) {
            throw_overflow("Subtracting");
        }
    }
    val -= delta;
    rv = val;
    return rv;
}

} // namespace details

/**
 * @brief A value of Type constrained to [Min, Max] - or to [Min, Max), (Min, Max] or (Min, Max)
 *  depending on Policy. Throws ranged_value_exception when constructed or assigned a value
 *  outside that range.
 *
 * @tparam Type Totally ordered, copyable type of the wrapped value (see RangeableType).
 * @tparam Min Lower bound, inclusive unless Policy says otherwise. Any constant Type can be
 *  constructed from, so it need not be a Type itself: e.g. `1` for a std::chrono::nanoseconds
 *  range, whose type can not be a template argument.
 * @tparam Max Upper bound, inclusive unless Policy says otherwise. Same rules as Min.
 * @tparam Policy Whether Min and Max are part of the range. Defaults to rv_bound_policy::closed.
 * @note The bounds are part of the type as given, so `ranged_value<float, 0, 1>` and
 *  `ranged_value<float, 0.f, 1.f>` describe the same range but are distinct types.
 */
template <RangeableType Type, RangeBoundFor<Type> auto Min, RangeBoundFor<Type> auto Max,
          rv_bound_policy Policy = rv_bound_policy::closed>
using ranged_value = details::ranged_value_impl<Type, Min, Max, Policy>;

static_assert(ranged_value<int, 0, 1>::MIN() == 0);
static_assert(ranged_value<int, 0, 1>::MAX() == 1);

// Bounds of another type are converted to Type.
static_assert(std::same_as<decltype(ranged_value<double, 0, 1>::MAX()), double>);
static_assert(ranged_value<double, 0, 1>::MAX() == 1.0);

// The range is closed unless the caller asks for something else.
static_assert(ranged_value<int, 0, 1>::BOUND_POLICY() == rv_bound_policy::closed);
static_assert((ranged_value<int, 0, 1, rv_bound_policy::max_open>::BOUND_POLICY()) ==
              rv_bound_policy::max_open);

// The policies really do differ on which bounds they include.
static_assert(details::is_within_range(0, 0, 5, rv_bound_policy::closed));
static_assert(details::is_within_range(5, 0, 5, rv_bound_policy::closed));
static_assert(details::is_within_range(0, 0, 5, rv_bound_policy::max_open));
static_assert(!details::is_within_range(5, 0, 5, rv_bound_policy::max_open));
static_assert(!details::is_within_range(0, 0, 5, rv_bound_policy::min_open));
static_assert(details::is_within_range(5, 0, 5, rv_bound_policy::min_open));
static_assert(!details::is_within_range(0, 0, 5, rv_bound_policy::open));
static_assert(!details::is_within_range(5, 0, 5, rv_bound_policy::open));
static_assert(details::is_within_range(3, 0, 5, rv_bound_policy::open));

// The two specializations must be picked correctly depending on whether zero
// falls inside the range: only the zero-inclusive one may be default-constructed.
static_assert(std::default_initializable<ranged_value<int, 0, 5>>);
static_assert(!std::default_initializable<ranged_value<int, 5, 10>>);
static_assert(std::default_initializable<ranged_value<float, -1.f, 1.f>>);
static_assert(!std::default_initializable<ranged_value<float, 1.f, 10.f>>);

// The bound policy takes part in that choice: zero is the upper bound here, so it only
// belongs to the range - and only makes the type default-constructible - when it is closed.
static_assert(std::default_initializable<ranged_value<int, -5, 0>>);
static_assert(!std::default_initializable<ranged_value<int, -5, 0, rv_bound_policy::max_open>>);
// Likewise for zero as the lower bound, which min_open and open leave out.
static_assert(std::default_initializable<ranged_value<int, 0, 5, rv_bound_policy::max_open>>);
static_assert(!std::default_initializable<ranged_value<int, 0, 5, rv_bound_policy::min_open>>);
static_assert(!std::default_initializable<ranged_value<int, 0, 5, rv_bound_policy::open>>);
static_assert(std::default_initializable<ranged_value<int, -5, 5, rv_bound_policy::open>>);

// Default-constructed value (when available) must be zero.
static_assert(ranged_value<int, 0, 5>{} == 0);

// A value inside [Min, Max) must be constructible at compile time and round-trip
// unchanged through the implicit conversion back to value_type.
static_assert(ranged_value<int, 0, 5>{3} == 3);
static_assert(ranged_value<int, -10, 10>{-5} == -5);

// The upper bound itself is a valid value under the default (closed) policy.
static_assert(ranged_value<int, 0, 5>{5} == 5);

// The constructor can throw (out-of-range values are only caught here, not rejected
// at compile time unless the construction itself is a constant expression), so it
// must not be noexcept.
static_assert(!noexcept(ranged_value<int, 0, 5>{3}));

// debug_ranged_value is ranged_value in debug builds (values are range-checked), and a plain
// Type alias in release builds (NDEBUG), for zero overhead.
#ifdef NDEBUG
template <RangeableType Type, RangeBoundFor<Type> auto Min, RangeBoundFor<Type> auto Max,
          rv_bound_policy Policy = rv_bound_policy::closed>
using debug_ranged_value = Type;
#else // NDEBUG
template <RangeableType Type, RangeBoundFor<Type> auto Min, RangeBoundFor<Type> auto Max,
          rv_bound_policy Policy = rv_bound_policy::closed>
using debug_ranged_value = details::ranged_value_impl<Type, Min, Max, Policy>;

#endif // NDEBUG

#ifdef NDEBUG
static_assert(std::same_as<debug_ranged_value<int, 0, 5>, int>);
#else
static_assert(!std::same_as<debug_ranged_value<int, 0, 5>, int>);
static_assert(std::same_as<debug_ranged_value<int, 0, 5>, ranged_value<int, 0, 5>>);
#endif

} // namespace tca::engine::core
