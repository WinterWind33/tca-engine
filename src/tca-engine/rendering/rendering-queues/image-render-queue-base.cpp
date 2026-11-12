// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/rendering-queues/image-render-queue-base.hpp"

namespace tca::engine::rendering {

enqueued_image image_render_queue_base::register_texture(gpu_texture texture) {
    const image_handle handle{static_cast<image_handle>(m_nextHandleValue++)};
    const ImTextureID textureId{texture.get_id()};
    m_textures.emplace(handle, std::move(texture));
    return enqueued_image{handle, textureId};
}

void image_render_queue_base::dequeue_image(const image_handle handle) noexcept {
    // Erasing the map entry destroys its gpu_texture, which releases the underlying GPU
    // resource (see gpu_texture's releaser callback).
    m_textures.erase(handle);
}

} // namespace tca::engine::rendering
