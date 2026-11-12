// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/image-factory.hpp"

// C++ STL
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

// Platform
#include "platform/images/image-decoder.hpp"

// Rendering
#include "rendering/image-color-sampling.hpp"
#include "rendering/rendering-queues/image-render-queue.hpp" // for enqueued_image

namespace tca::engine::rendering {

namespace {

constexpr std::size_t BYTES_PER_PIXEL{4};

// Throws std::invalid_argument unless decodedImage has non-zero dimensions and a pixel buffer of
// exactly width * height * 4 bytes, so a malformed buffer never reaches the GPU (every backend
// copies width * height * 4 bytes out of it).
void validate_pixels(const platform::images::decoded_image& decodedImage) {
    const std::size_t width{decodedImage.width};
    const std::size_t height{decodedImage.height};

    if (width == 0 || height == 0) {
        throw std::invalid_argument{"Invalid image size: width and height must be non-zero"};
    }

    // Checked on the whole byte count, not just width * height: the product must not wrap, or a
    // short buffer could match the wrapped size.
    if (width > std::numeric_limits<std::size_t>::max() / BYTES_PER_PIXEL / height) {
        throw std::invalid_argument{"Invalid image size: width * height * 4 overflows"};
    }

    if (decodedImage.pixels.size() != width * height * BYTES_PER_PIXEL) {
        throw std::invalid_argument{
            "Invalid image buffer: its size is not width * height * 4 bytes"};
    }
}

} // namespace

image image_factory::load_from_file(const std::filesystem::path& path) const {
    return create_from_pixels(platform::images::decode_image_from_file(path));
}

image image_factory::create_from_pixels(platform::images::decoded_image decodedImage) const {
    validate_pixels(decodedImage);

    // Captured/computed before the pixel data is moved into enqueue_image() below, since
    // decoded_image's fields are only guaranteed valid on the source side of that move.
    const std::uint32_t width{decodedImage.width};
    const std::uint32_t height{decodedImage.height};
    const color averageColor{average_color(decodedImage)};

    const enqueued_image enqueued{m_queue.enqueue_image(std::move(decodedImage))};
    return image{m_queue, enqueued.handle, enqueued.texture_id, width, height, averageColor};
}

} // namespace tca::engine::rendering
