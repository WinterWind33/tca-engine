// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <filesystem>

// Platform
#include "platform/images/image-decoder.hpp"

// Rendering
#include "rendering/image.hpp"
#include "rendering/rendering-queues/image-render-queue.hpp"

namespace tca::engine::rendering {

/**
 * @brief Uploads images through an image_render_queue, handing back ready-to-draw image objects.
 * Images come either from a file on disk (load_from_file()) or from RGBA8 pixels the caller built
 * in memory (create_from_pixels()), e.g. an atlas packed at load time or a texture generated at
 * run time. This is the only engine type that needs to know about the image_render_queue:
 * callers only ever deal with image_factory and image, never with the queue or an image_handle
 * directly.
 */
class image_factory final {
public:
    /**
     * @brief Construct a new image factory object bound to the given queue. The queue must
     * outlive this factory.
     *
     * @param queue The image render queue used to upload every image loaded through this
     * factory.
     */
    constexpr explicit image_factory(image_render_queue& queue) noexcept : m_queue{queue} {}

    /**
     * @brief Decodes the image file at path and uploads it through the bound image_render_queue,
     * returning a ready-to-draw image owning the result. The returned image's
     * get_average_color() reflects this same decode, so callers that need a representative color
     * (e.g. to derive a UI theme from a wallpaper) never need to decode the file a second time.
     * Equivalent to create_from_pixels(platform::images::decode_image_from_file(path)).
     *
     * @param path The path of the image file to load.
     * @return image The loaded, uploaded image.
     * @throws std::runtime_error if the file cannot be decoded, propagated from
     * platform::images::decode_image_from_file(). Whatever the concrete image_render_queue
     * implementation throws on upload failure is propagated as well.
     * @throws std::invalid_argument if the decoder hands back a malformed image, see
     * create_from_pixels().
     */
    [[nodiscard]] image load_from_file(const std::filesystem::path& path) const;

    /**
     * @brief Uploads pixels built in memory through the bound image_render_queue, returning a
     * ready-to-draw image owning the result, exactly as load_from_file() does for a file. The
     * returned image's get_average_color() is sampled from these same pixels. A typical flow is
     * to decode source art with platform::images::decode_image_from_file(), edit or pack it, and
     * upload the result here.
     *
     * @param decodedImage The pixels to upload, as 32-bit RGBA, row-major, top-down, with no
     * padding between rows (see platform::images::decoded_image). Its pixel data is consumed by
     * this call: pass an rvalue to avoid copying it.
     * @return image The uploaded image.
     * @throws std::invalid_argument if width or height is zero, or if pixels.size() is not
     * exactly width * height * 4. Nothing reaches the image_render_queue in that case. Whatever
     * the concrete image_render_queue implementation throws on upload failure is propagated.
     */
    [[nodiscard]] image create_from_pixels(platform::images::decoded_image decodedImage) const;

private:
    image_render_queue& m_queue;
};

} // namespace tca::engine::rendering
