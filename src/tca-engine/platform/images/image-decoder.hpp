// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace tca::engine::platform::images {

/**
 * @brief Result of decoding an image file into memory. The pixel data is always stored as 32-bit
 * RGBA, top-down, row-major, with no row padding, regardless of the original file's pixel format,
 * so that callers (e.g. the rendering layer) never need to know anything about the source file.
 */
struct decoded_image final {
    /**
     * @brief The decoded pixel data, as 32-bit RGBA (4 bytes per pixel), row-major, top-down, with
     * no padding between rows. Its size is always width * height * 4.
     */
    std::vector<std::byte> pixels{};

    /**
     * @brief The width of the decoded image, in pixels.
     */
    std::uint32_t width{};

    /**
     * @brief The height of the decoded image, in pixels.
     */
    std::uint32_t height{};
};

/**
 * @brief Decodes an image file from disk into an in-memory RGBA8 pixel buffer. The decoding
 * backend is platform-specific (e.g. WIC on Windows), but the returned decoded_image is always in
 * the same, platform-independent format.
 *
 * Public API meant for games too, not only for the engine: a game can decode source art with it,
 * edit or pack the pixels (e.g. into an atlas), and upload the result with
 * rendering::image_factory::create_from_pixels(). See docs/rendering/images.md.
 *
 * @note Unlike most other platform functions, this is not noexcept: there is no sensible fallback
 * for a file the caller explicitly asked to decode, so failures are reported as exceptions
 * instead of being silently swallowed.
 *
 * @param path The path of the image file to decode.
 * @throws std::runtime_error if the file does not exist, is not a supported image format, or
 * could not be decoded for any other reason.
 * @return decoded_image The decoded image, as an RGBA8 pixel buffer.
 */
[[nodiscard]] decoded_image decode_image_from_file(const std::filesystem::path& path);

} // namespace tca::engine::platform::images
