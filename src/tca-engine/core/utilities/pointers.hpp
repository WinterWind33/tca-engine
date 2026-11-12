// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

/**
 * This header file contains utility structures, aliases and functions related to pointers,
 * such as smart pointers, and other pointer-related utilities that can be used throughout the
 * engine.
 */

// C++ STL
#include <concepts>
#include <memory>
#include <utility>

// Third-party
#include <gsl/pointers>

namespace tca::engine::core {

/**
 * @brief Non-owning pointer type alias. This is a simple alias for raw pointers, used to indicate
 * that the pointer does not own the object it points to, and that the lifetime of the pointed-to
 * object is managed elsewhere.
 *
 * @tparam T The type of the object being pointed to.
 */
template <typename T>
using non_owning_ptr = T*;

/**
 * @brief A pointer type alias for non-owning pointers that cannot be null. This is an alias for
 * gsl::not_null<non_owning_ptr<T>>. It is used to indicate that the pointer is a non-owning pointer
 * that must always point to a valid object, and cannot be null. This can help improve code safety
 * by providing a clear indication of the pointer's intended usage.
 *
 * @tparam T The type of the object being pointed to.
 */
template <typename T>
using ptr_ref_to = gsl::not_null<non_owning_ptr<T>>;

/**
 * @brief Returns a reference to the object owned by ptr, lazily constructing it first via
 * factory() if ptr is currently empty. Useful for classes that own a std::unique_ptr<T> member
 * built on first use (e.g. a resource that can only be created once some prerequisite is ready).
 *
 * @tparam T The type of the object owned by ptr.
 * @tparam Factory A callable returning a std::unique_ptr<T>, invoked only if ptr is empty.
 * @param ptr The owning pointer to lazily initialize and dereference.
 * @param factory The callable used to construct the owned object if ptr is currently empty.
 * @return T& A reference to the object owned by ptr, guaranteed to be initialized.
 */
template <typename T, std::regular_invocable Factory>
    requires std::convertible_to<std::invoke_result_t<Factory>, std::unique_ptr<T>>
T& get_or_create(std::unique_ptr<T>& ptr, Factory&& factory) {
    if (!ptr) {
        ptr = std::forward<Factory>(factory)();
    }

    return *ptr;
}

} // namespace tca::engine::core
