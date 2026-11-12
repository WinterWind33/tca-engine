// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>

// Third-party
#include <imgui.h>

// Platform
#include "platform/images/image-decoder.hpp"

namespace tca::engine::rendering {

/**
 * @brief Opaque handle identifying an image submitted to an image_render_queue. Carries no
 * meaning outside of the image_render_queue implementation that produced it.
 */
enum class image_handle : std::uint32_t {};

/**
 * @brief Result of a successful enqueue_image() call: the handle identifying the uploaded image
 * for release purposes, and the backend-specific texture identifier needed to actually draw it
 * with ImGui::Image(). Handed to callers so that drawing (see the image class) no longer needs to
 * go through the queue at all - rendering is not the queue's concern, only GPU resource
 * lifecycle (upload/release) is.
 */
struct enqueued_image final {
    image_handle handle{};
    ImTextureID texture_id{ImTextureID_Invalid};
};

/**
 * @brief Abstract entry point through which application/game logic can submit decoded images for
 * GPU upload, without ever depending on which rendering backend (DirectX 12, OpenGL3, ...) is
 * actually active. This is intentionally kept separate from application_controller: the
 * application layer only deals with lifecycle concerns (exit, fullscreen, ...), while this
 * interface is the one and only place where images cross from game logic into the rendering
 * pipeline. It is deliberately limited to upload/release; drawing is the image class's job (see
 * image.hpp), using the texture_id handed back by enqueue_image().
 */
struct image_render_queue {
    virtual ~image_render_queue() noexcept = default;

    /**
     * @brief Uploads a decoded image to the GPU and returns a handle identifying it, plus the
     * texture identifier needed to draw it. The caller keeps the returned handle for as long as
     * it wants the image to remain drawable, and must eventually release it via
     * dequeue_image().
     *
     * @note Blocks the calling thread until the upload has completed on the GPU. Whether it is
     * safe to call this off the main/render thread is implementation-defined; see the concrete
     * backend for its actual guarantees.
     * @todo Give every backend a documented, real thread-safety guarantee for enqueue_image()/
     * dequeue_image(), so callers dealing with expensive image loading can offload it to a worker
     * thread instead of stalling the render thread. See the per-backend @todo notes for the
     * concrete plan for each backend.
     *
     * @param image The decoded image to upload. Its pixel data is consumed by this call.
     * @return enqueued_image The handle and texture identifier for the uploaded image.
     */
    virtual enqueued_image enqueue_image(platform::images::decoded_image image) = 0;

    /**
     * @brief Releases the GPU resources associated with the given handle. After this call, the
     * handle must no longer be used.
     *
     * @note Same threading caveat as enqueue_image(): whether this is safe to call off the
     * main/render thread is implementation-defined.
     *
     * @param handle The handle to release, as returned by a previous call to enqueue_image().
     */
    virtual void dequeue_image(image_handle handle) noexcept = 0;
};

} // namespace tca::engine::rendering
