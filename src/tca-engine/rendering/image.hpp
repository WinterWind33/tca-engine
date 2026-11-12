// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <utility>

// Third-party
#include <imgui.h>

// Core
#include "core/math/vector.hpp"

// Rendering
#include "rendering/colors.hpp"
#include "rendering/rendering-queues/image-render-queue.hpp"
#include "rendering/scene/sprite.hpp"
#include "rendering/tickables.hpp"

namespace tca::engine::rendering {

class image_factory;

/**
 * @brief RAII handle to an image uploaded through an image_render_queue. Move-only: owns the
 * queue handle for as long as it lives, releasing the underlying GPU resource when destroyed (or
 * reassigned). Drawing is this class's own responsibility - not the queue's - via draw(), which
 * calls ImGui::Image() directly using the texture identifier captured at load time. Callers never
 * construct this type directly - see image_factory::load_from_file() and
 * image_factory::create_from_pixels().
 */
class image : public renderable {
public:
    /**
     * @brief Constructs an empty image, owning no GPU resource. Safe to draw() (a no-op) or
     * destroy.
     */
    constexpr image() noexcept = default;

    image(const image&) = delete;
    image& operator=(const image&) = delete;

    image(image&& other) noexcept;
    image& operator=(image&& other) noexcept;

    ~image() noexcept;

    /**
     * @brief Draws this image, at the current ImGui cursor position. Does
     * nothing if this image is empty (e.g. default-constructed or moved-from). Unlike
     * enqueue_image()/dequeue_image(), this never touches the image_render_queue - it draws
     * directly from the texture identifier captured when this image was loaded.
     */
    void render() override;

    /**
     * @brief Sets the size with which the image will be rendered.
     *
     * @param size The new size to set.
     */
    void set_size(math::vector2 size);

    /**
     * @brief Retrieves the width of the image, in pixels.
     *
     * @return std::uint32_t The image's width, or 0 if this image is empty.
     */
    constexpr std::uint32_t get_original_width() const noexcept {
        return m_originalWidth;
    }

    /**
     * @brief Retrieves the height of the image, in pixels.
     *
     * @return std::uint32_t The image's height, or 0 if this image is empty.
     */
    constexpr std::uint32_t get_original_height() const noexcept {
        return m_originalHeight;
    }

    /**
     * @brief Retrieves the average color of this image's pixels, sampled once at load time from
     * the same decode used to upload the image to the GPU. Useful for deriving a representative
     * accent color from an image (e.g. a wallpaper) without decoding the file a second time.
     *
     * @return color The image's average color, or a default-constructed (black) color if this
     * image is empty (e.g. default-constructed or moved-from).
     */
    constexpr color get_average_color() const noexcept {
        return m_averageColor;
    }

    /**
     * @brief Hands out a view of this image's texture, to draw it as a sprite or build a
     * scene::texture_atlas over it. The view owns nothing: this image must outlive every draw that
     * uses it.
     *
     * @return scene::sprite_texture The uploaded texture's identifier and this image's size in
     * pixels, or an empty texture if this image is empty (e.g. default-constructed or moved-from).
     */
    scene::sprite_texture as_sprite_texture() const noexcept;

private:
    friend class image_factory;

    /**
     * @brief Construct a new image object, taking ownership of an already-uploaded queue handle
     * and its texture identifier. Only callable by image_factory.
     *
     * @param queue The queue that produced handle, used to release it on destruction. Must
     * outlive this image.
     * @param handle The handle to take ownership of, as returned by queue.enqueue_image().
     * @param textureId The texture identifier to draw with, as returned by
     * queue.enqueue_image().
     * @param width The width of the image, in pixels.
     * @param height The height of the image, in pixels.
     * @param averageColor The image's average color, as sampled by image_factory at load time.
     */
    image(image_render_queue& queue, const image_handle handle, const ImTextureID textureId,
          const std::uint32_t width, const std::uint32_t height, const color averageColor) noexcept;

    image_render_queue* m_queue{};
    image_handle m_handle{};
    ImTextureID m_textureId{ImTextureID_Invalid};
    std::uint32_t m_originalWidth{};
    std::uint32_t m_originalHeight{};
    color m_averageColor{};
    math::vector2 m_size{};

    void release() noexcept;
};

} // namespace tca::engine::rendering
