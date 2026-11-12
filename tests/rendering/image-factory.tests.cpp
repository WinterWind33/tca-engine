// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/image-factory.hpp"

// C++ STL
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

// Platform
#include "platform/images/image-decoder.hpp"

// Rendering
#include "rendering/image-color-sampling.hpp"
#include "rendering/rendering-queues/null-image-render-queue.hpp"

// Utilities
#include "utilities/test-assets.hpp"

// Test doubles
#include "test-doubles/rendering/rendering-queues/image-render-queue.test-double.hpp"

namespace {

using tca::engine::platform::images::decoded_image;

// Same 4x2 fixture used by tests/platform/image-decoder.tests.cpp.
constexpr std::uint32_t FIXTURE_WIDTH{4};
constexpr std::uint32_t FIXTURE_HEIGHT{2};
constexpr std::size_t BYTES_PER_PIXEL{4};

struct pixel_rgba final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{};
};

constexpr pixel_rgba OPAQUE_RED{255, 0, 0, 255};
constexpr pixel_rgba OPAQUE_GREEN{0, 255, 0, 255};
constexpr pixel_rgba OPAQUE_BLUE{0, 0, 255, 255};

void write_pixel(decoded_image& image, const std::uint32_t x, const std::uint32_t y,
                 const pixel_rgba color) {
    const std::size_t offset{(static_cast<std::size_t>(y) * image.width + x) * BYTES_PER_PIXEL};
    image.pixels[offset + 0] = std::byte{color.r};
    image.pixels[offset + 1] = std::byte{color.g};
    image.pixels[offset + 2] = std::byte{color.b};
    image.pixels[offset + 3] = std::byte{color.a};
}

pixel_rgba read_pixel(const decoded_image& image, const std::uint32_t x, const std::uint32_t y) {
    const std::size_t offset{(static_cast<std::size_t>(y) * image.width + x) * BYTES_PER_PIXEL};
    return pixel_rgba{
        std::to_integer<std::uint8_t>(image.pixels[offset + 0]),
        std::to_integer<std::uint8_t>(image.pixels[offset + 1]),
        std::to_integer<std::uint8_t>(image.pixels[offset + 2]),
        std::to_integer<std::uint8_t>(image.pixels[offset + 3]),
    };
}

decoded_image make_blank_image(const std::uint32_t width, const std::uint32_t height) {
    return decoded_image{
        .pixels =
            std::vector<std::byte>(static_cast<std::size_t>(width) * height * BYTES_PER_PIXEL),
        .width = width,
        .height = height,
    };
}

// Builds, in memory, the exact pixels of res/images/test-pattern.png: a 4x2 image whose top row
// is opaque red and whose bottom row is opaque blue.
decoded_image make_test_pattern_in_memory() {
    decoded_image image{make_blank_image(FIXTURE_WIDTH, FIXTURE_HEIGHT)};
    for (std::uint32_t x{}; x < FIXTURE_WIDTH; ++x) {
        write_pixel(image, x, 0, OPAQUE_RED);
        write_pixel(image, x, 1, OPAQUE_BLUE);
    }

    return image;
}

// Builds a non-square image where every byte differs from its neighbours, so that any
// reordering, row flipping, channel swizzling or width/height swap on the way to the queue shows
// up as a mismatch.
decoded_image make_distinct_bytes_image(const std::uint32_t width, const std::uint32_t height) {
    decoded_image image{make_blank_image(width, height)};
    for (std::size_t i{}; i < image.pixels.size(); ++i) {
        image.pixels[i] = static_cast<std::byte>((i * 37 + 11) % 256);
    }

    return image;
}

bool are_identical(const decoded_image& lhs, const decoded_image& rhs) {
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.pixels == rhs.pixels;
}

// An image_render_queue whose upload always fails, standing in for a backend error (e.g. device
// removed, descriptor heap exhausted).
class throwing_image_render_queue final : public tca::engine::rendering::image_render_queue {
public:
    tca::engine::rendering::enqueued_image enqueue_image(decoded_image) override {
        throw std::runtime_error{"Simulated upload failure"};
    }

    void dequeue_image(tca::engine::rendering::image_handle) noexcept override {}
};

// A pixel buffer that create_from_pixels() must refuse before it ever reaches the queue.
struct malformed_input final {
    const char* description{};
    std::size_t pixel_byte_count{};
    std::uint32_t width{};
    std::uint32_t height{};
};

// clang-format off
const std::vector<malformed_input> MALFORMED_INPUTS{
    {"a buffer one byte short of width * height * 4",                      4 * 2 * 4 - 1, 4, 2},
    {"a buffer one byte longer than width * height * 4",                   4 * 2 * 4 + 1, 4, 2},
    {"a buffer one whole pixel short",                                     4 * 2 * 4 - 4, 4, 2},
    {"a buffer one whole row longer",                                      4 * 3 * 4,     4, 2},
    {"a tightly packed RGB buffer (3 bytes per pixel)",                    4 * 2 * 3,     4, 2},
    {"an empty buffer with non-zero dimensions",                           0,             4, 2},
    {"a zero width with a zero-sized buffer",                              0,             0, 2},
    {"a zero height with a zero-sized buffer",                             0,             4, 0},
    {"zero width and height with a zero-sized buffer",                     0,             0, 0},
    {"a zero width with a non-empty buffer",                               4 * 2 * 4,     0, 2},
    {"a zero height with a non-empty buffer",                              4 * 2 * 4,     4, 0},
    // width * height * 4 wraps to 0 in 32-bit arithmetic: an unchecked std::uint32_t product
    // would wrongly accept an empty buffer for a 65536x65536 image.
    {"dimensions whose 32-bit byte count wraps to zero, with an empty buffer", 0, 0x10000, 0x10000},
    // width * height fits in 32 bits here, only the final * 4 wraps: checking width * height
    // alone for overflow is not enough.
    {"dimensions whose 32-bit pixel count fits but byte count wraps to zero", 0, 0x8000, 0x8000},
    // 32769 * 32769 * 4 wraps to 262148 in 32-bit arithmetic: a buffer of exactly that wrapped
    // size must still be rejected.
    {"a buffer matching the 32-bit wrapped byte count", 262148, 0x8001, 0x8001},
    // width * height * 4 wraps to 0 in 64-bit arithmetic too: an unchecked std::size_t product
    // would wrongly accept an empty buffer for a 2^31 x 2^31 image.
    {"dimensions whose 64-bit byte count wraps to zero, with an empty buffer", 0, 0x80000000, 0x80000000},
};
// clang-format on

decoded_image make_malformed_image(const malformed_input& input) {
    return decoded_image{
        .pixels = std::vector<std::byte>(input.pixel_byte_count),
        .width = input.width,
        .height = input.height,
    };
}

} // namespace

TEST_CASE("image_factory tests", "[tca][engine][rendering][image_factory]") {
    using namespace tca::engine::rendering;

    test_doubles::image_render_queue_fake queue{};
    const image_factory factory{queue};

    GIVEN("A valid image file") {
        const std::filesystem::path testPatternPath{tca::tests::utilities::get_test_pattern_path()};
        REQUIRE(std::filesystem::exists(testPatternPath));

        WHEN("Loading it through the factory") {
            image loadedImage{factory.load_from_file(testPatternPath)};

            THEN("The returned image reports the fixture's dimensions") {
                REQUIRE(loadedImage.get_original_width() == FIXTURE_WIDTH);
                REQUIRE(loadedImage.get_original_height() == FIXTURE_HEIGHT);
            }

            AND_THEN(
                "The returned image reports the average color of the fixture (half red, half "
                "blue)") {
                const color averageColor{loadedImage.get_average_color()};
                REQUIRE(averageColor.r == 127);
                REQUIRE(averageColor.g == 0);
                REQUIRE(averageColor.b == 127);
            }

            AND_THEN("The queue has registered exactly one image") {
                REQUIRE(queue.get_enqueue_call_count() == 1);
            }

            WHEN("The image is destroyed") {
                { image scoped{std::move(loadedImage)}; }

                THEN("The queue's dequeue_image() has been called exactly once") {
                    REQUIRE(queue.get_dequeued_handles().size() == 1);
                }
            }

            WHEN(
                "The image is moved from, and both the moved-from and moved-to instances are "
                "then destroyed") {
                {
                    image moved{std::move(loadedImage)};
                    // loadedImage is now empty; destroying it here must not dequeue anything.
                }
                // moved has now also been destroyed, releasing the one handle it took ownership
                // of.

                THEN("dequeue_image() was called exactly once overall") {
                    REQUIRE(queue.get_dequeued_handles().size() == 1);
                }
            }
        }
    }

    GIVEN("A nonexistent image path") {
        const std::filesystem::path missingPath{"this-file-does-not-exist.png"};

        THEN("Loading it throws and nothing is registered with the queue") {
            REQUIRE_THROWS(factory.load_from_file(missingPath));
            REQUIRE(queue.get_enqueue_call_count() == 0);
        }
    }
}

TEST_CASE("image_factory::create_from_pixels tests",
          "[tca][engine][rendering][image_factory][create_from_pixels]") {
    using namespace tca::engine::rendering;

    test_doubles::image_render_queue_fake queue{};
    const image_factory factory{queue};

    SECTION("Uploading a well-formed pixel buffer") {
        GIVEN("A non-square RGBA8 buffer built in memory, every byte distinct") {
            constexpr std::uint32_t WIDTH{3};
            constexpr std::uint32_t HEIGHT{5};
            const decoded_image sourcePixels{make_distinct_bytes_image(WIDTH, HEIGHT)};

            WHEN("Creating an image from a copy of it") {
                image createdImage{factory.create_from_pixels(sourcePixels)};

                THEN("The image reports the buffer's width and height, not swapped") {
                    REQUIRE(createdImage.get_original_width() == WIDTH);
                    REQUIRE(createdImage.get_original_height() == HEIGHT);
                }

                AND_THEN("The queue has uploaded exactly one image") {
                    REQUIRE(queue.get_enqueue_call_count() == 1);
                }

                AND_THEN("The queue received the very same pixels, dimensions and byte order") {
                    REQUIRE(queue.get_enqueued_images().size() == 1);
                    REQUIRE(are_identical(queue.get_enqueued_images().front(), sourcePixels));
                }

                AND_THEN("The average color is sampled the same way load_from_file() samples it") {
                    const color expected{average_color(sourcePixels)};
                    const color actual{createdImage.get_average_color()};
                    REQUIRE(actual.r == expected.r);
                    REQUIRE(actual.g == expected.g);
                    REQUIRE(actual.b == expected.b);
                    REQUIRE(actual.a == expected.a);
                }

                AND_THEN("Nothing has been released yet") {
                    REQUIRE(queue.get_dequeued_handles().empty());
                }

                WHEN("The image is destroyed") {
                    { image scoped{std::move(createdImage)}; }

                    THEN("Its handle is released through the queue exactly once") {
                        REQUIRE(queue.get_dequeued_handles().size() == 1);
                    }
                }

                WHEN("The image is moved into another, and both are then destroyed") {
                    {
                        image moved{std::move(createdImage)};
                        image movedAgain{};
                        movedAgain = std::move(moved);
                    }

                    THEN("dequeue_image() was called exactly once overall") {
                        REQUIRE(queue.get_dequeued_handles().size() == 1);
                    }
                }
            }
        }

        GIVEN("A single-pixel buffer") {
            decoded_image onePixel{make_blank_image(1, 1)};
            write_pixel(onePixel, 0, 0, OPAQUE_GREEN);

            WHEN("Creating an image from it") {
                const image createdImage{factory.create_from_pixels(std::move(onePixel))};

                THEN("It is accepted as the smallest valid image") {
                    REQUIRE(createdImage.get_original_width() == 1);
                    REQUIRE(createdImage.get_original_height() == 1);
                    REQUIRE(queue.get_enqueue_call_count() == 1);
                }

                AND_THEN("Its average color is the color of its only pixel") {
                    const color averageColor{createdImage.get_average_color()};
                    REQUIRE(averageColor.r == 0);
                    REQUIRE(averageColor.g == 255);
                    REQUIRE(averageColor.b == 0);
                }
            }
        }

        GIVEN("A single-row and a single-column buffer") {
            WHEN("Creating an image from each") {
                const image row{factory.create_from_pixels(make_distinct_bytes_image(7, 1))};
                const image column{factory.create_from_pixels(make_distinct_bytes_image(1, 7))};

                THEN("Both are accepted with their own dimensions") {
                    REQUIRE(row.get_original_width() == 7);
                    REQUIRE(row.get_original_height() == 1);
                    REQUIRE(column.get_original_width() == 1);
                    REQUIRE(column.get_original_height() == 7);
                }

                AND_THEN("Each is uploaded separately") {
                    REQUIRE(queue.get_enqueue_call_count() == 2);
                }
            }
        }

        GIVEN("Two images created from pixels") {
            std::vector<image> images{};
            images.push_back(factory.create_from_pixels(make_distinct_bytes_image(2, 2)));
            images.push_back(factory.create_from_pixels(make_distinct_bytes_image(2, 2)));

            WHEN("Both are destroyed") {
                images.clear();

                THEN("Two distinct handles are released") {
                    const std::vector<image_handle>& dequeued{queue.get_dequeued_handles()};
                    REQUIRE(dequeued.size() == 2);
                    REQUIRE(dequeued[0] != dequeued[1]);
                }
            }
        }
    }

    SECTION("An image created from pixels matches the same pixels loaded from a PNG") {
        const std::filesystem::path testPatternPath{tca::tests::utilities::get_test_pattern_path()};
        REQUIRE(std::filesystem::exists(testPatternPath));

        GIVEN("The test pattern built by hand in memory, and the same pattern on disk as a PNG") {
            WHEN("Creating one image from the in-memory pixels and loading the other from file") {
                const image fromPixels{factory.create_from_pixels(make_test_pattern_in_memory())};
                const image fromFile{factory.load_from_file(testPatternPath)};

                THEN("Both report the same dimensions") {
                    REQUIRE(fromPixels.get_original_width() == fromFile.get_original_width());
                    REQUIRE(fromPixels.get_original_height() == fromFile.get_original_height());
                }

                AND_THEN("Both report the same average color") {
                    const color pixelsColor{fromPixels.get_average_color()};
                    const color fileColor{fromFile.get_average_color()};
                    REQUIRE(pixelsColor.r == fileColor.r);
                    REQUIRE(pixelsColor.g == fileColor.g);
                    REQUIRE(pixelsColor.b == fileColor.b);
                    REQUIRE(pixelsColor.a == fileColor.a);
                }

                AND_THEN("The queue received byte-identical uploads for both") {
                    const std::vector<decoded_image>& uploads{queue.get_enqueued_images()};
                    REQUIRE(uploads.size() == 2);
                    REQUIRE(are_identical(uploads[0], uploads[1]));
                }
            }
        }

        GIVEN("The test pattern decoded from disk through the public decoder") {
            const decoded_image decoded{
                tca::engine::platform::images::decode_image_from_file(testPatternPath)};

            WHEN("Uploading the decoded pixels unchanged, and loading the file directly") {
                const image fromPixels{factory.create_from_pixels(decoded)};
                const image fromFile{factory.load_from_file(testPatternPath)};

                THEN("The queue received byte-identical uploads for both") {
                    const std::vector<decoded_image>& uploads{queue.get_enqueued_images()};
                    REQUIRE(uploads.size() == 2);
                    REQUIRE(are_identical(uploads[0], decoded));
                    REQUIRE(are_identical(uploads[1], decoded));
                }

                AND_THEN("Both images report the same dimensions and average color") {
                    REQUIRE(fromPixels.get_original_width() == fromFile.get_original_width());
                    REQUIRE(fromPixels.get_original_height() == fromFile.get_original_height());
                    REQUIRE(fromPixels.get_average_color().r == fromFile.get_average_color().r);
                    REQUIRE(fromPixels.get_average_color().g == fromFile.get_average_color().g);
                    REQUIRE(fromPixels.get_average_color().b == fromFile.get_average_color().b);
                }
            }

            WHEN("Editing the decoded pixels (the blue row turned green) before uploading them") {
                decoded_image edited{decoded};
                for (std::uint32_t x{}; x < edited.width; ++x) {
                    write_pixel(edited, x, 1, OPAQUE_GREEN);
                }

                const image editedImage{factory.create_from_pixels(edited)};

                THEN("The edited pixels, not the ones from the file, are what reaches the queue") {
                    REQUIRE(queue.get_enqueued_images().size() == 1);
                    const decoded_image& uploaded{queue.get_enqueued_images().front()};
                    for (std::uint32_t x{}; x < uploaded.width; ++x) {
                        const pixel_rgba top{read_pixel(uploaded, x, 0)};
                        const pixel_rgba bottom{read_pixel(uploaded, x, 1)};
                        REQUIRE(top.r == 255);
                        REQUIRE(top.b == 0);
                        REQUIRE(bottom.g == 255);
                        REQUIRE(bottom.b == 0);
                    }
                }

                AND_THEN("The average color reflects the edit (half red, half green)") {
                    const color averageColor{editedImage.get_average_color()};
                    REQUIRE(averageColor.r == 127);
                    REQUIRE(averageColor.g == 127);
                    REQUIRE(averageColor.b == 0);
                }
            }
        }
    }

    SECTION("Rejecting a malformed pixel buffer") {
        const malformed_input input{GENERATE(Catch::Generators::from_range(MALFORMED_INPUTS))};

        GIVEN(std::string{"A decoded_image with "} + input.description) {
            WHEN("Creating an image from it") {
                THEN("It throws std::invalid_argument") {
                    REQUIRE_THROWS_AS(factory.create_from_pixels(make_malformed_image(input)),
                                      std::invalid_argument);
                }

                AND_THEN("Nothing ever reaches the queue") {
                    try {
                        [[maybe_unused]] const image rejected{
                            factory.create_from_pixels(make_malformed_image(input))};
                    } catch (const std::invalid_argument&) {}

                    REQUIRE(queue.get_enqueue_call_count() == 0);
                    REQUIRE(queue.get_enqueued_images().empty());
                    REQUIRE(queue.get_dequeued_handles().empty());
                }
            }
        }
    }

    SECTION("The factory stays usable after rejecting a malformed buffer") {
        GIVEN("A rejected malformed buffer") {
            REQUIRE_THROWS_AS(factory.create_from_pixels(make_malformed_image(MALFORMED_INPUTS[0])),
                              std::invalid_argument);

            WHEN("A well-formed buffer is then uploaded") {
                const image createdImage{factory.create_from_pixels(make_test_pattern_in_memory())};

                THEN("It is accepted and uploaded normally") {
                    REQUIRE(createdImage.get_original_width() == FIXTURE_WIDTH);
                    REQUIRE(createdImage.get_original_height() == FIXTURE_HEIGHT);
                    REQUIRE(queue.get_enqueue_call_count() == 1);
                }
            }
        }
    }
}

TEST_CASE("image_factory::create_from_pixels error propagation tests",
          "[tca][engine][rendering][image_factory][create_from_pixels]") {
    using namespace tca::engine::rendering;

    GIVEN("A factory bound to a queue whose upload always fails") {
        throwing_image_render_queue queue{};
        const image_factory factory{queue};

        WHEN("Creating an image from a well-formed buffer") {
            THEN("The exception thrown by the queue propagates, as with load_from_file()") {
                REQUIRE_THROWS_AS(factory.create_from_pixels(make_test_pattern_in_memory()),
                                  std::runtime_error);
            }
        }

        WHEN("Creating an image from a malformed buffer") {
            THEN("Validation fails first, before the queue is ever reached") {
                REQUIRE_THROWS_AS(
                    factory.create_from_pixels(make_malformed_image(MALFORMED_INPUTS[0])),
                    std::invalid_argument);
            }
        }
    }
}

TEST_CASE("image_factory::create_from_pixels with the null image render queue",
          "[tca][engine][rendering][image_factory][create_from_pixels][null_image_render_queue]") {
    using namespace tca::engine::rendering;

    null_image_render_queue queue{};
    const image_factory factory{queue};

    GIVEN("A well-formed buffer built in memory") {
        WHEN("Creating an image from it") {
            image createdImage{factory.create_from_pixels(make_test_pattern_in_memory())};

            THEN("The image reports the buffer's dimensions") {
                REQUIRE(createdImage.get_original_width() == FIXTURE_WIDTH);
                REQUIRE(createdImage.get_original_height() == FIXTURE_HEIGHT);
            }

            AND_THEN("The image reports the buffer's average color (half red, half blue)") {
                const color averageColor{createdImage.get_average_color()};
                REQUIRE(averageColor.r == 127);
                REQUIRE(averageColor.g == 0);
                REQUIRE(averageColor.b == 127);
            }

            AND_THEN("Destroying it, moved or not, is safe") {
                REQUIRE_NOTHROW([&createdImage]() {
                    image moved{std::move(createdImage)};
                }());
            }
        }
    }

    GIVEN("A malformed buffer") {
        const malformed_input input{GENERATE(Catch::Generators::from_range(MALFORMED_INPUTS))};

        THEN(std::string{"Creating an image from "} + input.description + " still throws") {
            REQUIRE_THROWS_AS(factory.create_from_pixels(make_malformed_image(input)),
                              std::invalid_argument);
        }
    }
}
