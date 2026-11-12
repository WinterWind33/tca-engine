// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/image.hpp"

// C++ STL
#include <cstdint>

// Third-party
#include <imgui.h>

// Rendering
#include "rendering/gui/imgui/imgui-math-helpers.hpp"
#include "rendering/rendering-queues/image-render-queue.hpp"

namespace tca::engine::rendering {

image::image(image_render_queue& queue, const image_handle handle, const ImTextureID textureId,
             const std::uint32_t width, const std::uint32_t height,
             const color averageColor) noexcept
    : m_queue{&queue},
      m_handle{handle},
      m_textureId{textureId},
      m_originalWidth{width},
      m_originalHeight{height},
      m_averageColor{averageColor} {}

image::image(image&& other) noexcept
    : m_queue{std::exchange(other.m_queue, nullptr)},
      m_handle{std::exchange(other.m_handle, image_handle{})},
      m_textureId{std::exchange(other.m_textureId, ImTextureID_Invalid)},
      m_originalWidth{std::exchange(other.m_originalWidth, std::uint32_t{})},
      m_originalHeight{std::exchange(other.m_originalHeight, std::uint32_t{})},
      m_averageColor{std::exchange(other.m_averageColor, color{})} {}

image& image::operator=(image&& other) noexcept {
    if (this != &other) {
        release();
        m_queue = std::exchange(other.m_queue, nullptr);
        m_handle = std::exchange(other.m_handle, image_handle{});
        m_textureId = std::exchange(other.m_textureId, ImTextureID_Invalid);
        m_originalWidth = std::exchange(other.m_originalWidth, std::uint32_t{});
        m_originalHeight = std::exchange(other.m_originalHeight, std::uint32_t{});
        m_averageColor = std::exchange(other.m_averageColor, color{});
    }

    return *this;
}

image::~image() noexcept {
    release();
}

void image::render() {
    if (m_queue) {
        ImGui::Image(m_textureId, imgui::to_imvec2(m_size));
    }
}

void image::set_size(math::vector2 size) {
    m_size = std::move(size);
}

scene::sprite_texture image::as_sprite_texture() const noexcept {
    return scene::sprite_texture{
        .id = m_textureId,
        .width = m_originalWidth,
        .height = m_originalHeight,
    };
}

void image::release() noexcept {
    if (m_queue) {
        m_queue->dequeue_image(m_handle);
        m_queue = nullptr;
    }
}

} // namespace tca::engine::rendering
