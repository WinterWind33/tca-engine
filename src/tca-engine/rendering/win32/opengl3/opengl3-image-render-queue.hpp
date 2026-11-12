// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

#include "platform/images/image-decoder.hpp"

// Rendering
#include "rendering/rendering-queues/image-render-queue-base.hpp"

namespace tca::engine::rendering::win32::opengl3 {

/**
 * @brief OpenGL 3 implementation of image_render_queue. Uploads decoded images as plain GL
 * textures on the calling thread's current GL context, and hands back the texture identifier
 * needed to draw them (drawing itself is the image class's job, not this queue's - see
 * image.hpp).
 *
 * @note enqueue_image()/dequeue_image() MUST be called from the same thread that owns the
 * render thread's GL context (the one made current by opengl3_rendering_manager). Unlike DX12,
 * this is not just a missing-synchronization gap: a WGL rendering context can only be current on
 * one thread at a time, so calling GL from any other thread without more work simply fails.
 * @todo Support calling enqueue_image()/dequeue_image() from a worker thread, so expensive image
 * loading doesn't have to stall the render thread. This needs a second HGLRC created via
 * wglCreateContext() against the same pixel format as the main window's HDC (see
 * opengl3_rendering_manager::initialize() in opengl3-rendering-manager.cpp), linked to the main
 * context via wglShareLists() so texture objects it creates are visible to the main context that
 * actually draws them. That upload context would then be made current only for the duration of
 * enqueue_image()/dequeue_image(), guarded by a mutex (also covering the base class's texture
 * map/handle counter, see image_render_queue_base) so only one thread uses it, and released
 * afterwards.
 */
class opengl3_image_render_queue final : public image_render_queue_base {
public:
    constexpr opengl3_image_render_queue() noexcept = default;

    enqueued_image enqueue_image(platform::images::decoded_image image) override;
};

} // namespace tca::engine::rendering::win32::opengl3
