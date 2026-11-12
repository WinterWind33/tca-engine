// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <utility>

#include "platform/engine-type-traits.hpp"

namespace tca::engine::platform {

/**
 * @brief Abstract class representing the application window that will be created by the engine.
 */
struct application_window {
    virtual ~application_window() noexcept = default;

    /**
     * @brief Shows the application window.
     * Full behavior of this function is platform and implementation specific, but it
     * should at least make the window visible on the screen, and it may also perform other actions
     * such as updating the window, or setting the focus to the window.
     */
    virtual void show() = 0;
};

/**
 * @brief Template class representing a platform-specific application window. This class is designed
 * to be used as a base class for platform-specific window implementations, allowing for the storage
 * of a native window handle of a specific type, which can be used to interact with the underlying
 * windowing system of the platform.
 *
 * @tparam TWindowNativeHandle The type of the native window handle associated with this application
 * window.
 */
template <typename TWindowNativeHandle>
class platform_specific_window : public application_window {
public:
    /**
     * @brief Constructs a new platform-specific application window with the given title.
     *
     * @param title The title of the application window.
     */
    explicit platform_specific_window(const platform_preferred_string_view_t title)
        : m_windowTitle{title} {}

    /**
     * @brief Retrieves the native window handle associated with this application window.
     *
     * @return const TWindowNativeHandle&
     */
    const TWindowNativeHandle& get_native_handle() const noexcept {
        return m_windowHandle;
    }

protected:
    /**
     * @brief Assigns a new native window handle to this application window.
     *
     * @param handle The new native window handle to be assigned.
     */
    void assign_native_handle(TWindowNativeHandle&& handle) {
        m_windowHandle = std::move(handle);
    }

    /**
     * @brief Retrieves the original title of the window, which is the title that was passed to the
     * constructor of this class.
     *
     * @return platform_preferred_string_view_t The original title of the window.
     */
    platform_preferred_string_view_t get_window_title() const noexcept {
        return m_windowTitle;
    }

private:
    platform_preferred_string_t m_windowTitle{};
    TWindowNativeHandle m_windowHandle{};
};

} // namespace tca::engine::platform
