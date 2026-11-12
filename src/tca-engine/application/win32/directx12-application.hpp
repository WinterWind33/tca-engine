// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <functional>
#include <memory>

// Windows
#include "platform/win32/windows.hpp"

// third-party
#include <d3d12.h>
#include <wil/com.h>

// Engine
#include "platform/win32/win32-application-window.hpp" // for wparam_t and lparam_t
#include "rendering/win32/directx-12/directx12-rendering-manager.hpp"
#include "rendering/win32/directx-12/dx12-image-render-queue.hpp"

#include "application/win32/win32-application.hpp"

namespace tca::engine::application::win32 {

/**
 * @brief A DirectX 12 application implementation for Win32. This class extends the
 * win32_application class to provide functionality for initializing and managing DirectX 12
 * resources, handling window resizing, and filling command lists for rendering.
 */
class directx12_application : public win32_application {
public:
    /**
     * @brief Type alias for the callback function used to fill the DirectX 12 command list.
     * Implementations may have different ways of filling the command list, so we provide a callback
     * that can be set by the application logic to fill the command list with the appropriate
     * rendering commands for each frame.
     */
    using fill_command_list_callback_t =
        std::function<void(const wil::com_ptr<ID3D12GraphicsCommandList>&)>;

    /**
     * @brief Construct a new directx12 application object. This won't fill any command list when
     * rendering.
     */
    directx12_application() = default;

    /**
     * @brief Construct a new directx12 application object with the provided callback function for
     * filling the command list.
     *
     * @param fillCommandListFunc The callback function used to fill the DirectX 12 command list.
     */
    explicit directx12_application(fill_command_list_callback_t fillCommandListFunc);

protected:
    std::unique_ptr<rendering::win32::directx12::directx12_rendering_manager> m_renderingManager{};

    /**
     * @brief Initializes all the DirectX 12 resources but the window should be set first,
     * otherwise it will throw an exception. Implementations should create a Win32 window before
     * initializing the DirectX 12 resources, since the rendering manager needs a valid window to
     * create the DirectX 12 device and swap chain.
     */
    void initialize() override;

    void on_window_size(platform::win32::details::wparam_t wparam,
                        platform::win32::details::lparam_t lparam);

    /**
     * @brief Pushes the theme's window background role into the rendering manager's clear color,
     * so that re-theming the application at runtime also changes what the window is cleared to.
     */
    void on_theme_changed() override;

    bool pre_logic_tick() override;
    void post_logic_tick() override;

    /**
     * @brief Lazily constructs (on first call) and returns a DirectX 12-backed image render
     * queue, bound to this application's rendering manager. Must only be called after
     * initialize() has completed, since the rendering manager doesn't exist before that.
     */
    rendering::image_render_queue& get_or_create_image_render_queue() override;

private:
    std::unique_ptr<rendering::win32::directx12::dx12_image_render_queue> m_imageRenderQueue{};
    fill_command_list_callback_t m_fillCommandListFunc{};
};

} // namespace tca::engine::application::win32
