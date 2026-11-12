// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <unordered_map>

// Third-party
#include <imgui.h>

// Rendering
#include "rendering/gpu-texture.hpp"
#include "rendering/rendering-queues/image-render-queue.hpp"

namespace tca::engine::rendering {

/**
 * @brief Base implementation of image_render_queue providing the handle bookkeeping shared by
 * every backend (DirectX 12, OpenGL3, ...): handle allocation, storage of uploaded gpu_texture
 * instances (kept alive until released), and releasing them on dequeue_image(). Derived classes
 * only need to implement enqueue_image(), uploading the image to the GPU with their backend and
 * registering the resulting gpu_texture via register_texture().
 */
class image_render_queue_base : public image_render_queue {
public:
    void dequeue_image(image_handle handle) noexcept override;

protected:
    /**
     * @brief Takes ownership of an already-uploaded GPU texture and assigns it a fresh handle.
     * Backends should call this at the end of their enqueue_image() implementation, once the
     * texture has been uploaded to the GPU.
     *
     * @param texture The uploaded texture to register, taking ownership of it.
     * @return enqueued_image The handle newly assigned to the texture, and its texture
     * identifier.
     */
    enqueued_image register_texture(gpu_texture texture);

private:
    std::unordered_map<image_handle, gpu_texture> m_textures{};
    std::uint32_t m_nextHandleValue{1};
};

} // namespace tca::engine::rendering
