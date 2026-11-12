// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/image-factory.hpp"

// C++ STL
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

// Third-party
#include <imgui.h>
#include <catch2/catch_test_macros.hpp>

// Application
#include "application/win32/imgui-dx12-application.hpp"

// Engine testing support
#include "testing/win32/windowing-utilities.hpp"

// Platform
#include "platform/images/image-decoder.hpp"
#include "platform/win32/win32-application-window.hpp"

// Rendering
#include "rendering/win32/directx-12/directx12-rendering-manager.hpp"
#include "rendering/win32/directx-12/dx12-image-render-queue.hpp"

// Utilities
#include "utilities/test-assets.hpp"
#include "utilities/win32/wm-quit-drain.hpp"

// Test doubles
#include "test-doubles/application/application-logic.test-double.hpp"

namespace {

using namespace tca::engine;
using platform::images::decoded_image;

constexpr std::uint32_t FIXTURE_WIDTH{4};
constexpr std::uint32_t FIXTURE_HEIGHT{2};
constexpr std::size_t BYTES_PER_PIXEL{4};

// Builds, in memory, the exact pixels of res/images/test-pattern.png: a 4x2 image whose top row
// is opaque red and whose bottom row is opaque blue.
decoded_image make_test_pattern_in_memory() {
    decoded_image image{
        .pixels = std::vector<std::byte>(FIXTURE_WIDTH * FIXTURE_HEIGHT * BYTES_PER_PIXEL),
        .width = FIXTURE_WIDTH,
        .height = FIXTURE_HEIGHT,
    };

    for (std::uint32_t y{}; y < FIXTURE_HEIGHT; ++y) {
        for (std::uint32_t x{}; x < FIXTURE_WIDTH; ++x) {
            const std::size_t offset{(y * FIXTURE_WIDTH + x) * BYTES_PER_PIXEL};
            const bool isTopRow{y == 0};
            image.pixels[offset + 0] = std::byte{isTopRow ? std::uint8_t{255} : std::uint8_t{0}};
            image.pixels[offset + 1] = std::byte{0};
            image.pixels[offset + 2] = std::byte{isTopRow ? std::uint8_t{0} : std::uint8_t{255}};
            image.pixels[offset + 3] = std::byte{255};
        }
    }

    return image;
}

} // namespace

TEST_CASE("image_factory::create_from_pixels with a DirectX 12 image render queue",
          "[functional][tca][engine][rendering][win32][directx-12][image_factory]"
          "[create_from_pixels]") {
    using namespace rendering::win32::directx12;

    GIVEN("A factory bound to a dx12_image_render_queue in a real Win32 window") {
        auto window = platform::win32::win32_application_window::two_phase_window_creation<
            platform::win32::win32_application_window>(
            TEXT("DX12 Image Factory Create From Pixels Test Window"));

        constexpr rendering_config config{
            .frames_in_flight_count = 2,
            .back_buffers_count = 2,
            .srv_heap_size = 64,
        };

        directx12_rendering_manager renderingManager{window.get(), config};
        dx12_image_render_queue queue{renderingManager};
        const rendering::image_factory factory{queue};

        WHEN("Creating an image from pixels built in memory") {
            THEN("It uploads without throwing and reports the buffer's dimensions") {
                std::unique_ptr<rendering::image> created{};
                REQUIRE_NOTHROW(created = std::make_unique<rendering::image>(
                                    factory.create_from_pixels(make_test_pattern_in_memory())));
                REQUIRE(created->get_original_width() == FIXTURE_WIDTH);
                REQUIRE(created->get_original_height() == FIXTURE_HEIGHT);
            }

            AND_THEN("Destroying it releases the GPU texture without throwing") {
                REQUIRE_NOTHROW([&factory]() {
                    const rendering::image created{
                        factory.create_from_pixels(make_test_pattern_in_memory())};
                }());
            }
        }

        WHEN("Creating an image from pixels and loading the same pattern from its PNG") {
            const rendering::image fromPixels{
                factory.create_from_pixels(make_test_pattern_in_memory())};
            const rendering::image fromFile{
                factory.load_from_file(tca::tests::utilities::get_test_pattern_path())};

            THEN("Both describe the same image") {
                REQUIRE(fromPixels.get_original_width() == fromFile.get_original_width());
                REQUIRE(fromPixels.get_original_height() == fromFile.get_original_height());
                REQUIRE(fromPixels.get_average_color().r == fromFile.get_average_color().r);
                REQUIRE(fromPixels.get_average_color().g == fromFile.get_average_color().g);
                REQUIRE(fromPixels.get_average_color().b == fromFile.get_average_color().b);
            }
        }

        WHEN("Many images are created from pixels and released, beyond the SRV heap size") {
            // Each live image holds one SRV descriptor; creating and releasing more images than
            // the heap has slots proves create_from_pixels() hands every descriptor back.
            THEN("No upload ever fails") {
                REQUIRE_NOTHROW([&factory]() {
                    for (std::uint32_t i{}; i < config.srv_heap_size * 2; ++i) {
                        const rendering::image created{
                            factory.create_from_pixels(make_test_pattern_in_memory())};
                    }
                }());
            }
        }

        WHEN("Creating an image from a malformed buffer") {
            decoded_image malformed{make_test_pattern_in_memory()};
            malformed.pixels.pop_back();

            THEN("It is rejected before reaching the GPU") {
                REQUIRE_THROWS_AS(factory.create_from_pixels(std::move(malformed)),
                                  std::invalid_argument);
            }

            AND_THEN("The queue still uploads well-formed buffers afterwards") {
                REQUIRE_NOTHROW([&factory]() {
                    const rendering::image created{
                        factory.create_from_pixels(make_test_pattern_in_memory())};
                }());
            }
        }
    }
}

TEST_CASE("image_factory::create_from_pixels drawn in an imgui_dx12_application",
          "[functional][tca][engine][rendering][win32][directx-12][image_factory]"
          "[create_from_pixels][imgui_dx12_application]") {
    using namespace tca::engine::application;

    tests::utilities::drain_wm_quit_messages();

    GIVEN(
        "An imgui_dx12_application whose logic draws an image created from pixels next to the "
        "same image loaded from its PNG") {
        constexpr std::size_t FRAMES_TO_DRAW{20};

        std::vector<rendering::image> images{};
        std::size_t drawnFrameCount{};
        bool anyDrawThrew{};
        bool anyCreationThrew{};

        tca::engine::testing::application_tss<win32::imgui_dx12_application> app{};
        app.set_logic(std::make_unique<test_doubles::application_logic_fake>(
            [&app, &images, &drawnFrameCount, &anyDrawThrew]() -> bool {
                try {
                    ImGui::Begin("create_from_pixels");
                    for (rendering::image& image : images) {
                        image.set_size(
                            math::vector2{static_cast<double>(image.get_original_width()) * 16.0,
                                          static_cast<double>(image.get_original_height()) * 16.0});
                        image.render();
                        ImGui::SameLine();
                    }
                    ImGui::End();
                } catch (...) {
                    anyDrawThrew = true;
                }

                if (++drawnFrameCount >= FRAMES_TO_DRAW) {
                    tca::engine::testing::simulate_user_closing_application(
                        app.get_window_handle());
                }

                return true;
            },
            [&images]() {
                // Release every GPU texture while the DX12 queue is still alive.
                images.clear();
            },
            std::function<void(application_controller&)>{},
            [&images, &anyCreationThrew](rendering::image_factory& factory) {
                try {
                    images.push_back(factory.create_from_pixels(make_test_pattern_in_memory()));
                    images.push_back(
                        factory.load_from_file(tca::tests::utilities::get_test_pattern_path()));
                    images.push_back(
                        factory.create_from_pixels(platform::images::decode_image_from_file(
                            tca::tests::utilities::get_test_pattern_path())));
                } catch (...) {
                    anyCreationThrew = true;
                }
            }));
        app.set_app_name("Test create_from_pixels in ImGui Dx12 Application");

        WHEN("Running the application") {
            REQUIRE_NOTHROW(app.run());

            THEN("Every image was created through the injected factory") {
                REQUIRE_FALSE(anyCreationThrew);
            }

            AND_THEN("Every frame drew them without throwing") {
                REQUIRE(drawnFrameCount >= FRAMES_TO_DRAW);
                REQUIRE_FALSE(anyDrawThrew);
            }

            AND_THEN("Every image was released before the queue was torn down") {
                REQUIRE(images.empty());
            }
        }
    }
}
