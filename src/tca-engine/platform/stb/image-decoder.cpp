// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/images/image-decoder.hpp"

// C++ STL
#include <cstddef>
#include <stdexcept>
#include <string>

// Third-party
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace tca::engine::platform::images {

namespace {

constexpr std::int32_t REQUESTED_CHANNELS{4}; // Always force RGBA8, regardless of source format.

} // namespace

decoded_image decode_image_from_file(const std::filesystem::path& path) {
    std::int32_t width{};
    std::int32_t height{};
    std::int32_t sourceChannels{};

    stbi_uc* const decodedPixels{stbi_load(path.c_str(), &width, &height, &sourceChannels,
                                           REQUESTED_CHANNELS)};
    if (decodedPixels == nullptr) {
        throw std::runtime_error{std::string{"Failed to decode image '"} + path.string() +
                                 "': " + stbi_failure_reason()};
    }

    decoded_image result{
        .pixels = std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(decodedPixels),
            reinterpret_cast<const std::byte*>(decodedPixels) +
                (static_cast<std::size_t>(width) * height * REQUESTED_CHANNELS)),
        .width = static_cast<std::uint32_t>(width),
        .height = static_cast<std::uint32_t>(height),
    };

    stbi_image_free(decodedPixels);

    return result;
}

} // namespace tca::engine::platform::images
