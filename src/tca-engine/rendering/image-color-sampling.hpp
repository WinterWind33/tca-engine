// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Platform
#include "platform/images/image-decoder.hpp"

// Rendering
#include "rendering/colors.hpp"

namespace tca::engine::rendering {

/**
 * @brief Returns the unweighted average of decodedImage's pixels' RGB components, ignoring
 * alpha. Meant for deriving a representative accent color from an already-decoded image (e.g. a
 * wallpaper, decoded once for GPU upload) rather than for any kind of precise color analysis, and
 * without requiring the image to be decoded a second time just to sample it.
 *
 * @param decodedImage The decoded image to sample.
 * @return color The average color of the image, with alpha always fully opaque.
 */
[[nodiscard]] color average_color(const platform::images::decoded_image& decodedImage);

} // namespace tca::engine::rendering
