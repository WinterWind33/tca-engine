// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/rendering-queues/image-render-queue-base.hpp"

// C++ STL
#include <cstdint>
#include <utility>

// Third-party
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace tca::engine::rendering;

// Minimal concrete image_render_queue_base for testing the base class's bookkeeping in
// isolation, without depending on any real GPU backend. enqueue_image() ignores the given
// decoded_image entirely and just registers a fake texture, tracking how many times the
// releaser callback (constructed by the test) has been invoked.
class fake_image_render_queue final : public image_render_queue_base {
public:
    enqueued_image enqueue_image(tca::engine::platform::images::decoded_image) override {
        ++m_enqueueCallCount;

        gpu_texture texture{static_cast<gpu_texture::texture_id>(m_enqueueCallCount), 1, 1,
                            [this]() {
                                ++m_releaseCallCount;
                            }};

        return register_texture(std::move(texture));
    }

    std::uint32_t m_enqueueCallCount{};
    std::uint32_t m_releaseCallCount{};
};

} // namespace

TEST_CASE("image_render_queue_base tests",
          "[tca][engine][rendering][rendering-queues][image_render_queue_base]") {
    GIVEN("An empty fake_image_render_queue") {
        fake_image_render_queue queue{};

        WHEN("Two images are enqueued") {
            const image_handle firstHandle{queue.enqueue_image({}).handle};
            const image_handle secondHandle{queue.enqueue_image({}).handle};

            THEN("Each enqueue_image() call is assigned a distinct handle") {
                REQUIRE(firstHandle != secondHandle);
            }

            AND_THEN("Neither texture has been released yet") {
                REQUIRE(queue.m_releaseCallCount == 0);
            }

            AND_WHEN("The first handle is dequeued") {
                queue.dequeue_image(firstHandle);

                THEN("Only the first texture's releaser has been invoked") {
                    REQUIRE(queue.m_releaseCallCount == 1);
                }

                AND_WHEN("The same handle is dequeued again") {
                    queue.dequeue_image(firstHandle);

                    THEN("The releaser is not invoked a second time") {
                        REQUIRE(queue.m_releaseCallCount == 1);
                    }
                }
            }
        }

        WHEN("An unknown handle is dequeued") {
            THEN("It is simply ignored") {
                REQUIRE_NOTHROW(queue.dequeue_image(static_cast<image_handle>(1234)));
            }
        }
    }
}
