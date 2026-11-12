// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <unordered_set>
#include <vector>

// Engine
#include "rendering/rendering-queues/image-render-queue.hpp"

namespace tca::engine::rendering::test_doubles {

/**
 * @brief Fake implementation of the image_render_queue interface for testing purposes. Hands out
 * distinct handles/texture identifiers and tracks how enqueue_image()/dequeue_image() have been
 * called, without touching any real GPU resource.
 */
class image_render_queue_fake final : public image_render_queue {
public:
    enqueued_image enqueue_image(platform::images::decoded_image image) override;
    void dequeue_image(image_handle handle) noexcept override;

    std::size_t get_enqueue_call_count() const noexcept {
        return m_enqueueCallCount;
    }

    /**
     * @brief Retrieves a copy of every decoded_image handed to enqueue_image() so far, in call
     * order. Lets tests check exactly which pixels would have reached the GPU.
     */
    const std::vector<platform::images::decoded_image>& get_enqueued_images() const noexcept {
        return m_enqueuedImages;
    }

    /**
     * @brief Retrieves every handle that has been dequeued so far, in dequeue_image() call order.
     * Includes duplicate/unknown handles if dequeue_image() was called with them.
     */
    const std::vector<image_handle>& get_dequeued_handles() const noexcept {
        return m_dequeuedHandles;
    }

    /**
     * @brief Checks whether handle is currently registered, i.e. it was returned by
     * enqueue_image() and has not been dequeued since.
     */
    bool is_registered(image_handle handle) const noexcept {
        return m_registeredHandles.contains(handle);
    }

private:
    std::size_t m_enqueueCallCount{};
    std::uint32_t m_nextHandleValue{1};
    std::unordered_set<image_handle> m_registeredHandles{};
    std::vector<image_handle> m_dequeuedHandles{};
    std::vector<platform::images::decoded_image> m_enqueuedImages{};
};

} // namespace tca::engine::rendering::test_doubles
