// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/opengl3/opengl3-image-render-queue.hpp"

// C++ STL
#include <cstddef>
#include <cstdint>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Platform
#include "platform/win32/win32-application-window.hpp"

// Rendering
#include "rendering/win32/opengl3/opengl3-rendering-manager.hpp"

namespace {

using namespace tca::engine;
using namespace rendering::win32::opengl3;

constexpr std::uint32_t OPENGL3_TEST_IMAGE_WIDTH{2};
constexpr std::uint32_t OPENGL3_TEST_IMAGE_HEIGHT{2};
constexpr std::uint32_t OPENGL3_BYTES_PER_PIXEL{4};

// Builds a tiny, otherwise-irrelevant RGBA8 decoded_image: opengl3_image_render_queue only cares
// about its pixel buffer being width * height * 4 bytes, not about what it actually contains.
platform::images::decoded_image make_opengl3_test_decoded_image() {
    return platform::images::decoded_image{
        .pixels = std::vector<std::byte>(OPENGL3_TEST_IMAGE_WIDTH * OPENGL3_TEST_IMAGE_HEIGHT *
                                         OPENGL3_BYTES_PER_PIXEL),
        .width = OPENGL3_TEST_IMAGE_WIDTH,
        .height = OPENGL3_TEST_IMAGE_HEIGHT,
    };
}

} // namespace

TEST_CASE("opengl3_image_render_queue functional tests",
          "[functional][tca][engine][rendering][win32][opengl3][opengl3-image-render-queue]") {
    GIVEN("A Win32 application window with a current OpenGL 3 context") {
        auto window = platform::win32::win32_application_window::two_phase_window_creation<
            platform::win32::win32_application_window>(
            TEXT("OpenGL 3 Image Render Queue Test Window"));

        // Constructing the rendering manager creates the GL context and makes it current on this
        // (the test) thread - see opengl3_rendering_manager's constructor. That is exactly the
        // precondition opengl3_image_render_queue documents for enqueue_image()/dequeue_image():
        // they must run on the thread owning the current GL context.
        opengl3_rendering_manager renderingManager{*window};

        AND_GIVEN("An opengl3_image_render_queue") {
            opengl3_image_render_queue queue{};

            WHEN("A decoded image is enqueued") {
                const rendering::enqueued_image result{
                    queue.enqueue_image(make_opengl3_test_decoded_image())};

                THEN("It is assigned a texture identifier usable to draw it") {
                    REQUIRE(result.texture_id != ImTextureID_Invalid);
                }

                AND_WHEN("It is dequeued") {
                    THEN("It does not throw") {
                        REQUIRE_NOTHROW(queue.dequeue_image(result.handle));
                    }
                }

                AND_WHEN("The same handle is dequeued twice") {
                    queue.dequeue_image(result.handle);

                    THEN("The second dequeue is simply ignored") {
                        REQUIRE_NOTHROW(queue.dequeue_image(result.handle));
                    }
                }
            }

            WHEN("Two decoded images are enqueued") {
                const rendering::enqueued_image first{
                    queue.enqueue_image(make_opengl3_test_decoded_image())};
                const rendering::enqueued_image second{
                    queue.enqueue_image(make_opengl3_test_decoded_image())};

                THEN("Each is assigned a distinct handle and texture identifier") {
                    REQUIRE(first.handle != second.handle);
                    REQUIRE(first.texture_id != second.texture_id);
                }

                queue.dequeue_image(first.handle);
                queue.dequeue_image(second.handle);
            }

            WHEN("An unknown handle is dequeued") {
                THEN("It is simply ignored") {
                    REQUIRE_NOTHROW(
                        queue.dequeue_image(static_cast<rendering::image_handle>(1234)));
                }
            }
        }
    }
}
