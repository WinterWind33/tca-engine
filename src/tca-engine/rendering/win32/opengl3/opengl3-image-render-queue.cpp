// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/opengl3/opengl3-image-render-queue.hpp"

// C++ STL
#include <cstdint>

// Platform
#include "platform/win32/windows.hpp"

// Third-party
#include <GL/GL.h>

namespace tca::engine::rendering::win32::opengl3 {

namespace {

// GL_CLAMP_TO_EDGE is part of OpenGL 1.2 and is not declared by the legacy <GL/GL.h> (OpenGL 1.1)
// header this engine builds against, but every driver accepts it since it's a core enum value.
constexpr GLenum GL_CLAMP_TO_EDGE_VALUE{0x812F};

// Deletes the GL texture identified by textureId. Used as a gpu_texture releaser, so it must not
// throw.
void delete_gl_texture(const GLuint textureId) noexcept {
    ::glDeleteTextures(1, &textureId);
}

} // namespace

enqueued_image opengl3_image_render_queue::enqueue_image(
    const platform::images::decoded_image image) {
    GLuint textureId{};
    ::glGenTextures(1, &textureId);
    ::glBindTexture(GL_TEXTURE_2D, textureId);

    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_VALUE);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_VALUE);

    ::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(image.width),
                   static_cast<GLsizei>(image.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   image.pixels.data());

    ::glBindTexture(GL_TEXTURE_2D, 0);

    gpu_texture texture{static_cast<ImTextureID>(textureId), image.width, image.height,
                        [textureId]() {
                            delete_gl_texture(textureId);
                        }};

    return register_texture(std::move(texture));
}

} // namespace tca::engine::rendering::win32::opengl3
