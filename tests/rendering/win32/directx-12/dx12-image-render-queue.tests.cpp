// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/directx-12/dx12-image-render-queue.hpp"

// C++ STL
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <unordered_set>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

// Platform
#include "platform/win32/win32-application-window.hpp"

// Rendering
#include "rendering/win32/directx-12/directx12-rendering-manager.hpp"

namespace {

using namespace tca::engine;
using namespace rendering::win32::directx12;

constexpr std::uint32_t DX12_TEST_IMAGE_WIDTH{2};
constexpr std::uint32_t DX12_TEST_IMAGE_HEIGHT{2};
constexpr std::uint32_t DX12_BYTES_PER_PIXEL{4};

// Builds a tiny, otherwise-irrelevant RGBA8 decoded_image: dx12_image_render_queue only cares
// about its pixel buffer being width * height * 4 bytes, not about what it actually contains.
platform::images::decoded_image make_dx12_test_decoded_image() {
    return platform::images::decoded_image{
        .pixels = std::vector<std::byte>(DX12_TEST_IMAGE_WIDTH * DX12_TEST_IMAGE_HEIGHT *
                                         DX12_BYTES_PER_PIXEL),
        .width = DX12_TEST_IMAGE_WIDTH,
        .height = DX12_TEST_IMAGE_HEIGHT,
    };
}

} // namespace

TEST_CASE("dx12_image_render_queue functional tests",
          "[functional][tca][engine][rendering][win32][directx-12][dx12-image-render-queue]") {
    GIVEN("A Win32 application window and a DirectX 12 rendering manager") {
        auto window = platform::win32::win32_application_window::two_phase_window_creation<
            platform::win32::win32_application_window>(TEXT("DX12 Image Render Queue Test Window"));

        constexpr rendering_config config{
            .frames_in_flight_count = 2,
            .back_buffers_count = 2,
            .srv_heap_size = 64,
        };

        directx12_rendering_manager renderingManager{window.get(), config};

        AND_GIVEN("A dx12_image_render_queue bound to that rendering manager") {
            dx12_image_render_queue queue{renderingManager};

            WHEN("A decoded image is enqueued") {
                const rendering::enqueued_image result{
                    queue.enqueue_image(make_dx12_test_decoded_image())};

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
                    queue.enqueue_image(make_dx12_test_decoded_image())};
                const rendering::enqueued_image second{
                    queue.enqueue_image(make_dx12_test_decoded_image())};

                THEN("Each is assigned a distinct handle and texture identifier") {
                    REQUIRE(first.handle != second.handle);
                    REQUIRE(first.texture_id != second.texture_id);
                }

                queue.dequeue_image(first.handle);
                queue.dequeue_image(second.handle);
            }

            WHEN("Images are enqueued and dequeued concurrently from multiple threads") {
                // This is the scenario dx12_image_render_queue's internal mutex exists for (see
                // its class-level @note): concurrent enqueue_image()/dequeue_image() calls must
                // not corrupt the shared texture map/handle counter or the descriptor heap
                // allocator's free list.
                constexpr std::size_t THREAD_COUNT{4};
                constexpr std::size_t IMAGES_PER_THREAD{8};

                std::vector<std::thread> threads{};
                std::vector<std::vector<rendering::image_handle>> handlesPerThread(THREAD_COUNT);
                std::atomic<bool> anyThrew{false};

                for (std::size_t threadIndex{}; threadIndex < THREAD_COUNT; ++threadIndex) {
                    threads.emplace_back([&queue, &handlesPerThread, &anyThrew, threadIndex]() {
                        try {
                            for (std::size_t i{}; i < IMAGES_PER_THREAD; ++i) {
                                const rendering::enqueued_image enqueued{
                                    queue.enqueue_image(make_dx12_test_decoded_image())};
                                handlesPerThread[threadIndex].push_back(enqueued.handle);
                            }

                            for (const rendering::image_handle handle :
                                 handlesPerThread[threadIndex]) {
                                queue.dequeue_image(handle);
                            }
                        } catch (...) {
                            anyThrew.store(true);
                        }
                    });
                }

                for (std::thread& thread : threads) {
                    thread.join();
                }

                THEN("No thread observes an exception") {
                    REQUIRE_FALSE(anyThrew.load());
                }

                AND_THEN("Every handle handed out across threads is unique") {
                    std::unordered_set<rendering::image_handle> allHandles{};
                    for (const std::vector<rendering::image_handle>& handles : handlesPerThread) {
                        for (const rendering::image_handle handle : handles) {
                            const auto [_, inserted] = allHandles.insert(handle);
                            REQUIRE(inserted);
                        }
                    }

                    REQUIRE(allHandles.size() == THREAD_COUNT * IMAGES_PER_THREAD);
                }
            }
        }
    }
}
