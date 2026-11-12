// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/image-color-sampling.hpp"

// C++ STL
#include <cstddef>
#include <cstdint>

namespace tca::engine::rendering {

namespace {
constexpr std::size_t BYTES_PER_PIXEL{4};
} // namespace

color average_color(const platform::images::decoded_image& decodedImage) {
    const std::size_t pixelCount{decodedImage.pixels.size() / BYTES_PER_PIXEL};
    if (pixelCount == 0) {
        return colors::BLACK;
    }

    // Sums are accumulated in 64-bit integers since even a small wallpaper (e.g. 1920x1080) can
    // overflow a 32-bit sum of 8-bit components well before all pixels are visited.
    std::uint64_t sumR{};
    std::uint64_t sumG{};
    std::uint64_t sumB{};

    for (std::size_t pixel{}; pixel < pixelCount; ++pixel) {
        const std::size_t offset{pixel * BYTES_PER_PIXEL};
        sumR += std::to_integer<std::uint8_t>(decodedImage.pixels[offset + 0]);
        sumG += std::to_integer<std::uint8_t>(decodedImage.pixels[offset + 1]);
        sumB += std::to_integer<std::uint8_t>(decodedImage.pixels[offset + 2]);
    }

    return rgb(static_cast<std::uint8_t>(sumR / pixelCount),
               static_cast<std::uint8_t>(sumG / pixelCount),
               static_cast<std::uint8_t>(sumB / pixelCount));
}

} // namespace tca::engine::rendering
