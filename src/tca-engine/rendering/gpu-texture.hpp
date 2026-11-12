// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <functional>
#include <utility>

// Third-party
#include <imgui.h>

namespace tca::engine::rendering {

/**
 * @brief RAII handle to a texture uploaded to the GPU. Move-only: owns the backend-specific
 * texture identifier and the callback that releases the underlying GPU resource, so callers never
 * need to know which rendering backend (DirectX 12, OpenGL3, ...) produced it.
 */
class gpu_texture final {
public:
    using texture_id = ImTextureID;

    /**
     * @brief Constructs an empty gpu_texture, owning no GPU resource.
     */
    constexpr gpu_texture() = default;

    /**
     * @brief Construct a new gpu texture object, taking ownership of an already-uploaded GPU
     * texture.
     *
     * @param id The backend-specific texture identifier, usable directly with ImGui::Image().
     * @param width The width of the texture, in pixels.
     * @param height The height of the texture, in pixels.
     * @param releaser Callback invoked exactly once, when this gpu_texture is destroyed or
     * reassigned, to release the underlying GPU resource. Can be empty if there is nothing to
     * release.
     */
    gpu_texture(const texture_id id, const std::uint32_t width, const std::uint32_t height,
                std::function<void()> releaser) noexcept
        : m_id{id},
          m_width{width},
          m_height{height},
          m_releaser{std::move(releaser)} {}

    gpu_texture(const gpu_texture&) = delete;
    gpu_texture& operator=(const gpu_texture&) = delete;

    gpu_texture(gpu_texture&& other) noexcept
        : m_id{std::exchange(other.m_id, ImTextureID_Invalid)},
          m_width{std::exchange(other.m_width, std::uint32_t{})},
          m_height{std::exchange(other.m_height, std::uint32_t{})},
          m_releaser{std::exchange(other.m_releaser, {})} {}

    gpu_texture& operator=(gpu_texture&& other) noexcept {
        if (this != &other) {
            release();
            m_id = std::exchange(other.m_id, ImTextureID_Invalid);
            m_width = std::exchange(other.m_width, std::uint32_t{});
            m_height = std::exchange(other.m_height, std::uint32_t{});
            m_releaser = std::exchange(other.m_releaser, {});
        }

        return *this;
    }

    ~gpu_texture() noexcept {
        release();
    }

    /**
     * @brief Retrieves the backend-specific texture identifier, usable directly with
     * ImGui::Image().
     *
     * @return texture_id The texture identifier.
     */
    constexpr texture_id get_id() const noexcept {
        return m_id;
    }

    /**
     * @brief Retrieves the width of the texture, in pixels.
     *
     * @return std::uint32_t The texture's width.
     */
    constexpr std::uint32_t get_width() const noexcept {
        return m_width;
    }

    /**
     * @brief Retrieves the height of the texture, in pixels.
     *
     * @return std::uint32_t The texture's height.
     */
    constexpr std::uint32_t get_height() const noexcept {
        return m_height;
    }

private:
    texture_id m_id{ImTextureID_Invalid};
    std::uint32_t m_width{};
    std::uint32_t m_height{};
    std::function<void()> m_releaser{};

    void release() noexcept {
        if (m_releaser) {
            m_releaser();
            m_releaser = {};
        }
    }
};

} // namespace tca::engine::rendering
