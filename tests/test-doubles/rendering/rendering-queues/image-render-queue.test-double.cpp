// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "test-doubles/rendering/rendering-queues/image-render-queue.test-double.hpp"

// C++ STL
#include <utility>

namespace tca::engine::rendering::test_doubles {

enqueued_image image_render_queue_fake::enqueue_image(platform::images::decoded_image image) {
    ++m_enqueueCallCount;
    m_enqueuedImages.push_back(std::move(image));

    const image_handle handle{static_cast<image_handle>(m_nextHandleValue++)};
    m_registeredHandles.insert(handle);
    return enqueued_image{handle, static_cast<ImTextureID>(m_nextHandleValue)};
}

void image_render_queue_fake::dequeue_image(const image_handle handle) noexcept {
    m_registeredHandles.erase(handle);
    m_dequeuedHandles.push_back(handle);
}

} // namespace tca::engine::rendering::test_doubles
