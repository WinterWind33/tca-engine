// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Rendering
#include "rendering/rendering-queues/image-render-queue.hpp"

namespace tca::engine::rendering {

/**
 * @brief No-op implementation of image_render_queue, for applications that don't own any
 * rendering manager (e.g. headless/console applications). Submitted images are simply discarded.
 */
class null_image_render_queue final : public image_render_queue {
public:
    constexpr enqueued_image enqueue_image(platform::images::decoded_image) override {
        return enqueued_image{};
    }

    constexpr void dequeue_image(image_handle) noexcept override {}
};

} // namespace tca::engine::rendering
