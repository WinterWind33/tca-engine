// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/virtual-resolution.hpp"

// The layout maths is pure, like camera_2d: it must build with no renderer at all. Checked right
// after the header (and before anything else can pull ImGui in), so any ImGui include reaching it -
// directly or transitively - fails this translation unit.
#if defined(IMGUI_VERSION)
#error "rendering/scene/virtual-resolution.hpp must not include an ImGui header"
#endif

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Third-party
#include <Eigen/Geometry>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "core/math/vector.hpp"

namespace {

using tca::engine::math::vector2;
using tca::engine::scene::compute_pixel_perfect_layout;
using tca::engine::scene::pixel_perfect_layout;
using tca::engine::scene::screen_rect;
using tca::engine::scene::screen_to_virtual;
using tca::engine::scene::virtual_size;
using tca::engine::scene::virtual_to_screen;

using Catch::Matchers::WithinAbs;

constexpr double EPSILON{1e-9};

constexpr virtual_size VIRTUAL_320x180{.width = 320, .height = 180};

// ======================================= Helpers =======================================

void require_near(const double actual, const double expected, const double epsilon = EPSILON) {
    REQUIRE_THAT(actual, WithinAbs(expected, epsilon));
}

void require_near(const Eigen::Vector2d& actual, const Eigen::Vector2d& expected,
                  const double epsilon = EPSILON) {
    require_near(actual.x(), expected.x(), epsilon);
    require_near(actual.y(), expected.y(), epsilon);
}

void require_same_rect(const screen_rect& actual, const screen_rect& expected) {
    require_near(actual.min(), expected.min());
    require_near(actual.max(), expected.max());
}

screen_rect box(const double minX, const double minY, const double maxX, const double maxY) {
    return screen_rect{
        Eigen::Vector2d{minX, minY},
        Eigen::Vector2d{maxX, maxY}
    };
}

screen_rect sized(const double width, const double height) {
    return box(0.0, 0.0, width, height);
}

bool is_whole(const double value) {
    return std::floor(value) == value;
}

// The area two rectangles share. Not Eigen's intersection().volume(): an empty intersection has a
// negative size there, which would read as a negative area.
double overlap_area(const screen_rect& first, const screen_rect& second) {
    const Eigen::Vector2d low{first.min().cwiseMax(second.min())};
    const Eigen::Vector2d high{first.max().cwiseMin(second.max())};
    const Eigen::Vector2d extent{(high - low).cwiseMax(Eigen::Vector2d::Zero())};
    return extent.x() * extent.y();
}

double area(const screen_rect& rect) {
    return overlap_area(rect, rect);
}

bool contains(const screen_rect& outer, const screen_rect& inner) {
    return (inner.min().array() >= outer.min().array() - EPSILON).all() &&
           (inner.max().array() <= outer.max().array() + EPSILON).all();
}

// Whether the bars and the visible part of the content tile the available area exactly: no gap, no
// overlap, nothing outside it. Returns what is wrong, or an empty string.
std::string tiling_violation(const pixel_perfect_layout& layout, const screen_rect& available) {
    std::ostringstream problem{};
    double covered{overlap_area(layout.content, available)};
    for (std::size_t bar{0}; bar < layout.letterbox.size(); ++bar) {
        const screen_rect& rect{layout.letterbox[bar]};
        if (!(area(rect) > 0.0)) {
            problem << "bar " << bar << " is empty; ";
        }
        if (!contains(available, rect)) {
            problem << "bar " << bar << " spills out of the available area; ";
        }
        if (overlap_area(rect, layout.content) > EPSILON) {
            problem << "bar " << bar << " overlaps the content; ";
        }
        for (std::size_t other{bar + 1}; other < layout.letterbox.size(); ++other) {
            if (overlap_area(rect, layout.letterbox[other]) > EPSILON) {
                problem << "bars " << bar << " and " << other << " overlap; ";
            }
        }
        covered += area(rect);
    }
    const double expected{area(available)};
    if (std::abs(covered - expected) > 1e-6 * std::max(1.0, expected)) {
        problem << "covered " << covered << " of " << expected << " pixels; ";
    }
    return problem.str();
}

void require_tiles(const pixel_perfect_layout& layout, const screen_rect& available) {
    const std::string violation{tiling_violation(layout, available)};
    CAPTURE(violation);
    REQUIRE(violation.empty());
}

// The bars, compared as a set: their order is not part of the contract.
void require_letterbox(const pixel_perfect_layout& layout, std::vector<screen_rect> expected) {
    REQUIRE(layout.letterbox.size() == expected.size());
    for (const screen_rect& bar : layout.letterbox) {
        const auto found{std::ranges::find_if(expected, [&bar](const screen_rect& candidate) {
            return (candidate.min() - bar.min()).cwiseAbs().maxCoeff() < EPSILON &&
                   (candidate.max() - bar.max()).cwiseAbs().maxCoeff() < EPSILON;
        })};
        CAPTURE(bar.min().x(), bar.min().y(), bar.max().x(), bar.max().y());
        REQUIRE(found != expected.end());
        expected.erase(found);
    }
}

// The whole pixels of an area: its corners rounded inwards.
screen_rect whole_pixels_of(const screen_rect& available) {
    const Eigen::Vector2d low{available.min().array().ceil().matrix()};
    const Eigen::Vector2d high{available.max().array().floor().matrix().cwiseMax(low)};
    return screen_rect{low, high};
}

// Everything the layout promises for one available area, checked without stopping at the first
// assertion so the exhaustive sweeps stay fast. Returns what is wrong, or an empty string.
std::string layout_violation(const pixel_perfect_layout& layout, const screen_rect& available,
                             const virtual_size resolution) {
    std::ostringstream problem{};
    const screen_rect usable{whole_pixels_of(available)};
    const Eigen::Vector2d usableSize{usable.sizes()};
    const Eigen::Vector2d virtualExtent{static_cast<double>(resolution.width),
                                        static_cast<double>(resolution.height)};

    if (layout.resolution != resolution) {
        problem << "wrong resolution; ";
    }
    if (layout.scale < 1u) {
        problem << "scale below 1; ";
    }

    const double scale{static_cast<double>(layout.scale)};
    const bool fits{(virtualExtent * scale).x() <= usableSize.x() &&
                    (virtualExtent * scale).y() <= usableSize.y()};
    if (layout.scale > 1u && !fits) {
        problem << "scale " << layout.scale << " does not fit; ";
    }
    const bool nextFits{(virtualExtent * (scale + 1.0)).x() <= usableSize.x() &&
                        (virtualExtent * (scale + 1.0)).y() <= usableSize.y()};
    if (nextFits) {
        problem << "scale " << layout.scale + 1 << " would fit too; ";
    }

    if (layout.content.sizes() != virtualExtent * scale) {
        problem << "content is not resolution times scale; ";
    }
    if (!is_whole(layout.content.min().x()) || !is_whole(layout.content.min().y())) {
        problem << "content does not start on a whole pixel; ";
    }

    // Centred, with the odd pixel (if any) going to the right or bottom.
    const Eigen::Vector2d before{layout.content.min() - usable.min()};
    const Eigen::Vector2d after{usable.max() - layout.content.max()};
    for (int axis{0}; axis < 2; ++axis) {
        const double imbalance{after[axis] - before[axis]};
        if (imbalance != 0.0 && imbalance != 1.0) {
            problem << "not centred on axis " << axis << " (" << before[axis] << " before, "
                    << after[axis] << " after); ";
        }
    }

    problem << tiling_violation(layout, available);
    return problem.str();
}

} // namespace

// ================================= Compile-time contract =================================

static_assert(std::same_as<screen_rect, Eigen::AlignedBox2d>,
              "screen rectangles reuse Eigen's box type, like world_rect");

static_assert(std::is_aggregate_v<virtual_size> && std::equality_comparable<virtual_size>,
              "a virtual resolution is a plain value, built as {.width, .height}");
static_assert(std::same_as<decltype(virtual_size::width), std::uint32_t> &&
                  std::same_as<decltype(virtual_size::height), std::uint32_t>,
              "a virtual resolution is in whole pixels");

static_assert(std::same_as<decltype(pixel_perfect_layout::scale), std::uint32_t>,
              "the scale is a whole number: that is what keeps every pixel the same size");
static_assert(std::copyable<pixel_perfect_layout>);

static_assert(std::same_as<decltype(compute_pixel_perfect_layout(std::declval<screen_rect>(),
                                                                 std::declval<virtual_size>())),
                           pixel_perfect_layout>);
static_assert(std::same_as<decltype(screen_to_virtual(std::declval<const pixel_perfect_layout&>(),
                                                      std::declval<const vector2&>())),
                           std::optional<vector2>>,
              "a point in a letterbox bar maps to no virtual point at all");

// ======================================== Scale ========================================

TEST_CASE("compute_pixel_perfect_layout() at exact fits",
          "[tca][engine][rendering][scene][virtual_resolution][scale]") {
    const std::uint32_t scale = GENERATE(1u, 2u, 3u, 4u, 5u, 6u, 8u, 12u);
    CAPTURE(scale);

    SECTION("An area exactly scale times the resolution is filled edge to edge") {
        const screen_rect available{sized(320.0 * scale, 180.0 * scale)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.resolution == VIRTUAL_320x180);
        REQUIRE(layout.scale == scale);
        require_same_rect(layout.content, available);
        REQUIRE(layout.letterbox.empty());
    }

    SECTION("The layout follows the area's position, not just its size") {
        const screen_rect available{box(100.0, 50.0, 100.0 + 320.0 * scale, 50.0 + 180.0 * scale)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == scale);
        require_same_rect(layout.content, available);
        REQUIRE(layout.letterbox.empty());
    }
}

TEST_CASE("compute_pixel_perfect_layout() one pixel short of the next scale",
          "[tca][engine][rendering][scene][virtual_resolution][scale]") {
    const std::uint32_t scale = GENERATE(1u, 2u, 3u, 4u, 5u);
    CAPTURE(scale);
    const double next{static_cast<double>(scale + 1)};

    SECTION("One pixel short in width keeps the lower scale, letterboxed left and right") {
        const screen_rect available{sized(320.0 * next - 1.0, 180.0 * next)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == scale);
        // 319 pixels to spare across, 180 * next - 180 * scale = 180 down: the odd pixel goes to
        // the right.
        const Eigen::Vector2d offset{159.0, 90.0};
        require_same_rect(layout.content,
                          screen_rect{
                              offset, offset + Eigen::Vector2d{320.0 * scale, 180.0 * scale}
        });
        require_tiles(layout, available);
    }

    SECTION("One pixel short in height keeps the lower scale, letterboxed top and bottom") {
        const screen_rect available{sized(320.0 * next, 180.0 * next - 1.0)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == scale);
        const Eigen::Vector2d offset{160.0, 89.0};
        require_same_rect(layout.content,
                          screen_rect{
                              offset, offset + Eigen::Vector2d{320.0 * scale, 180.0 * scale}
        });
        require_tiles(layout, available);
    }

    SECTION("Exactly the next scale's size reaches it") {
        const pixel_perfect_layout layout{
            compute_pixel_perfect_layout(sized(320.0 * next, 180.0 * next), VIRTUAL_320x180)};

        REQUIRE(layout.scale == scale + 1);
    }
}

TEST_CASE("compute_pixel_perfect_layout() on common window sizes",
          "[tca][engine][rendering][scene][virtual_resolution][scale]") {
    struct expectation {
        double width;
        double height;
        std::uint32_t scale;
        Eigen::Vector2d contentMin;
    };

    // Windowed and fullscreen sizes a game will meet, at a 320x180 virtual resolution.
    const std::array cases{
        expectation{1280.0, 720.0,  4,  Eigen::Vector2d{0.0, 0.0}  },
        expectation{1920.0, 1080.0, 6,  Eigen::Vector2d{0.0, 0.0}  },
        expectation{2560.0, 1440.0, 8,  Eigen::Vector2d{0.0, 0.0}  },
        expectation{3840.0, 2160.0, 12, Eigen::Vector2d{0.0, 0.0}  },
        expectation{1366.0, 768.0,  4,  Eigen::Vector2d{43.0, 24.0}},
        expectation{1280.0, 800.0,  4,  Eigen::Vector2d{0.0, 40.0} },
        expectation{1680.0, 1050.0, 5,  Eigen::Vector2d{40.0, 75.0}},
        expectation{960.0,  540.0,  3,  Eigen::Vector2d{0.0, 0.0}  },
    };

    for (const expectation& expected : cases) {
        CAPTURE(expected.width, expected.height);
        const screen_rect available{sized(expected.width, expected.height)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == expected.scale);
        require_near(layout.content.min(), expected.contentMin);
        require_near(layout.content.sizes(),
                     Eigen::Vector2d{320.0 * expected.scale, 180.0 * expected.scale});
        require_tiles(layout, available);
    }
}

TEST_CASE("compute_pixel_perfect_layout() picks the axis that limits the scale",
          "[tca][engine][rendering][scene][virtual_resolution][scale]") {
    SECTION("A wide window is limited by its height and letterboxed left and right") {
        const screen_rect available{sized(2560.0, 720.0)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == 4u);
        require_same_rect(layout.content, box(640.0, 0.0, 1920.0, 720.0));
        require_letterbox(layout, {box(0.0, 0.0, 640.0, 720.0), box(1920.0, 0.0, 2560.0, 720.0)});
    }

    SECTION("A portrait window is limited by its width and letterboxed on every side") {
        // 720 / 320 = 2.25 across, 1280 / 180 = 7.1 down: the width wins.
        const screen_rect available{sized(720.0, 1280.0)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == 2u);
        require_same_rect(layout.content, box(40.0, 460.0, 680.0, 820.0));
        REQUIRE(layout.letterbox.size() >= 2);
        require_tiles(layout, available);
    }

    SECTION("A tall virtual resolution in a landscape window is letterboxed left and right") {
        const virtual_size portrait{.width = 180, .height = 320};
        const screen_rect available{sized(1920.0, 1080.0)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, portrait)};

        // 1920 / 180 = 10.7 across, 1080 / 320 = 3.4 down.
        REQUIRE(layout.scale == 3u);
        require_same_rect(layout.content, box(690.0, 60.0, 1230.0, 1020.0));
        require_tiles(layout, available);
    }

    SECTION("A window too small on one axis only overflows on that axis") {
        // 400 wide fits 320 once; 100 tall does not fit 180 at all.
        const screen_rect available{sized(400.0, 100.0)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == 1u);
        require_same_rect(layout.content, box(40.0, -40.0, 360.0, 140.0));
        require_letterbox(layout, {box(0.0, 0.0, 40.0, 100.0), box(360.0, 0.0, 400.0, 100.0)});
    }
}

TEST_CASE("compute_pixel_perfect_layout() in very small areas",
          "[tca][engine][rendering][scene][virtual_resolution][scale]") {
    SECTION("The scale clamps to 1, never to 0, and the content stays centred") {
        const screen_rect available{sized(100.0, 50.0)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == 1u);
        require_same_rect(layout.content, box(-110.0, -65.0, 210.0, 115.0));
    }

    SECTION("Content overflowing on both axes leaves no letterbox") {
        const pixel_perfect_layout layout{
            compute_pixel_perfect_layout(sized(100.0, 50.0), VIRTUAL_320x180)};

        REQUIRE(layout.letterbox.empty());
    }

    SECTION("A zero-sized area, like a minimised window, is still laid out") {
        const screen_rect available{box(10.0, 10.0, 10.0, 10.0)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == 1u);
        require_same_rect(layout.content, box(-150.0, -80.0, 170.0, 100.0));
        REQUIRE(layout.letterbox.empty());
    }

    SECTION("A one-by-one resolution scales to the whole area") {
        const pixel_perfect_layout layout{
            compute_pixel_perfect_layout(sized(37.0, 23.0), virtual_size{.width = 1, .height = 1})};

        REQUIRE(layout.scale == 23u);
        require_same_rect(layout.content, box(7.0, 0.0, 30.0, 23.0));
    }
}

// ================================ Placement on whole pixels ================================

TEST_CASE("compute_pixel_perfect_layout() keeps the content on whole pixels",
          "[tca][engine][rendering][scene][virtual_resolution][pixels]") {
    SECTION("An odd leftover puts the extra pixel on the right and bottom") {
        // 1023 - 960 = 63 across, 543 - 540 = 3 down.
        const pixel_perfect_layout layout{
            compute_pixel_perfect_layout(sized(1023.0, 543.0), VIRTUAL_320x180)};

        REQUIRE(layout.scale == 3u);
        require_same_rect(layout.content, box(31.0, 1.0, 991.0, 541.0));
    }

    SECTION("An area with fractional corners only counts its whole pixels") {
        // Its whole pixels are (41, 31)-(1000, 600): 959x569, which fits 320x180 twice. 319 and
        // 209 pixels are left over, split 159 | 160 and 104 | 105.
        const screen_rect available{box(40.5, 30.25, 1000.75, 600.6)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        REQUIRE(layout.scale == 2u);
        require_same_rect(layout.content, box(200.0, 135.0, 840.0, 495.0));
    }

    SECTION("The fractional slivers at the edges are still covered by the letterbox") {
        const screen_rect available{box(40.5, 30.25, 1000.75, 600.6)};

        const pixel_perfect_layout layout{compute_pixel_perfect_layout(available, VIRTUAL_320x180)};

        require_tiles(layout, available);
    }

    SECTION("A fractional area that would fit the next scale before rounding does not reach it") {
        // 639.9 wide, but its whole pixels run from 1 to 640: only 639 of them.
        const pixel_perfect_layout layout{
            compute_pixel_perfect_layout(box(0.5, 0.0, 640.4, 360.0), VIRTUAL_320x180)};

        REQUIRE(layout.scale == 1u);
    }
}

TEST_CASE("compute_pixel_perfect_layout() over every window size",
          "[tca][engine][rendering][scene][virtual_resolution][property]") {
    // Parenthesised rather than braced: GENERATE would split a braced initialiser at the comma.
    const virtual_size resolution = GENERATE(values<virtual_size>({
        {320, 180},
        {256, 224},
        {7,   3  },
        {1,   1  }
    }));
    const Eigen::Vector2d origin = GENERATE(Eigen::Vector2d(0.0, 0.0), Eigen::Vector2d(13.0, 7.0));
    CAPTURE(resolution.width, resolution.height, origin.x(), origin.y());

    // Every width up to past 4K with a spread of heights, then the other way round, so each axis
    // is swept a pixel at a time.
    std::size_t checked{0};
    auto check{[&](const double width, const double height) {
        const screen_rect available{
            origin, origin + Eigen::Vector2d{width, height}
        };
        const std::string violation{layout_violation(
            compute_pixel_perfect_layout(available, resolution), available, resolution)};
        if (!violation.empty()) {
            CAPTURE(width, height, violation);
            FAIL("the layout breaks its contract");
        }
        ++checked;
    }};

    for (int width{0}; width <= 4000; ++width) {
        for (int height{0}; height <= 2400; height += 97) {
            check(width, height);
        }
    }
    for (int height{0}; height <= 2400; ++height) {
        for (int width{0}; width <= 4000; width += 151) {
            check(width, height);
        }
    }
    REQUIRE(checked > 0);
}

TEST_CASE("compute_pixel_perfect_layout() over random fractional areas",
          "[tca][engine][rendering][scene][virtual_resolution][property]") {
    const std::uint32_t seed = GENERATE(1u, 42u, 1337u, 0xC0FFEEu);
    std::mt19937 generator{seed};
    std::uniform_real_distribution<double> coordinate{-500.0, 500.0};
    std::uniform_real_distribution<double> extent{0.0, 3000.0};
    std::uniform_int_distribution<std::uint32_t> resolutionExtent{1, 640};

    for (int sample{0}; sample < 2000; ++sample) {
        const Eigen::Vector2d origin{coordinate(generator), coordinate(generator)};
        const screen_rect available{
            origin, origin + Eigen::Vector2d{extent(generator), extent(generator)}
        };
        const virtual_size resolution{.width = resolutionExtent(generator),
                                      .height = resolutionExtent(generator)};

        const std::string violation{layout_violation(
            compute_pixel_perfect_layout(available, resolution), available, resolution)};
        if (!violation.empty()) {
            CAPTURE(seed, sample, origin.x(), origin.y(), available.max().x(), available.max().y(),
                    resolution.width, resolution.height, violation);
            FAIL("the layout breaks its contract");
        }
    }
}

// ===================================== Invalid input =====================================

TEST_CASE("compute_pixel_perfect_layout() rejects what cannot be laid out",
          "[tca][engine][rendering][scene][virtual_resolution][errors]") {
    constexpr double NaN{std::numeric_limits<double>::quiet_NaN()};
    constexpr double INF{std::numeric_limits<double>::infinity()};

    SECTION("A resolution of zero on either axis") {
        const virtual_size resolution = GENERATE(values<virtual_size>({
            {0,   180},
            {320, 0  },
            {0,   0  }
        }));
        CAPTURE(resolution.width, resolution.height);

        REQUIRE_THROWS_AS(compute_pixel_perfect_layout(sized(1280.0, 720.0), resolution),
                          std::invalid_argument);
    }

    SECTION("An area that is not finite") {
        REQUIRE_THROWS_AS(
            compute_pixel_perfect_layout(box(NaN, 0.0, 100.0, 100.0), VIRTUAL_320x180),
            std::invalid_argument);
        REQUIRE_THROWS_AS(compute_pixel_perfect_layout(box(0.0, 0.0, INF, 100.0), VIRTUAL_320x180),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(
            compute_pixel_perfect_layout(box(0.0, -INF, 100.0, 100.0), VIRTUAL_320x180),
            std::invalid_argument);
    }

    SECTION("An area with its minimum past its maximum, such as a default-constructed one") {
        REQUIRE_THROWS_AS(compute_pixel_perfect_layout(screen_rect{}, VIRTUAL_320x180),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(
            compute_pixel_perfect_layout(box(100.0, 0.0, 50.0, 100.0), VIRTUAL_320x180),
            std::invalid_argument);
    }
}

// =================================== Pointer mapping ===================================

TEST_CASE("virtual_to_screen() and screen_to_virtual()",
          "[tca][engine][rendering][scene][virtual_resolution][mapping]") {
    // 1366x768 at 320x180: scale 4, content from (43, 24) to (1323, 744).
    const pixel_perfect_layout layout{
        compute_pixel_perfect_layout(sized(1366.0, 768.0), VIRTUAL_320x180)};
    REQUIRE(layout.scale == 4u);
    require_near(layout.content.min(), Eigen::Vector2d{43.0, 24.0});

    SECTION("A virtual point lands scale pixels per virtual pixel from the content's corner") {
        require_near(virtual_to_screen(layout, vector2{0.0, 0.0}), Eigen::Vector2d{43.0, 24.0});
        require_near(virtual_to_screen(layout, vector2{1.0, 0.0}), Eigen::Vector2d{47.0, 24.0});
        require_near(virtual_to_screen(layout, vector2{320.0, 180.0}),
                     Eigen::Vector2d{1323.0, 744.0});
        require_near(virtual_to_screen(layout, vector2{10.5, 3.25}), Eigen::Vector2d{85.0, 37.0});
    }

    SECTION("A virtual point outside the virtual screen still maps, linearly") {
        require_near(virtual_to_screen(layout, vector2{-1.0, 200.0}), Eigen::Vector2d{39.0, 824.0});
    }

    SECTION("A screen point inside the content maps back, unrounded") {
        const std::optional<vector2> topLeft{screen_to_virtual(layout, vector2{43.0, 24.0})};
        REQUIRE(topLeft.has_value());
        require_near(*topLeft, Eigen::Vector2d{0.0, 0.0});

        const std::optional<vector2> inside{screen_to_virtual(layout, vector2{85.0, 37.0})};
        REQUIRE(inside.has_value());
        require_near(*inside, Eigen::Vector2d{10.5, 3.25});

        const std::optional<vector2> lastPixel{screen_to_virtual(layout, vector2{1322.5, 743.5})};
        REQUIRE(lastPixel.has_value());
        require_near(*lastPixel, Eigen::Vector2d{319.875, 179.875});
    }

    SECTION("Any screen pixel inside a virtual pixel maps into that virtual pixel") {
        for (int column{0}; column < 4; ++column) {
            const double screenX{43.0 + 4.0 * 100.0 + column + 0.5};
            const std::optional<vector2> mapped{screen_to_virtual(layout, vector2{screenX, 30.5})};
            REQUIRE(mapped.has_value());
            REQUIRE(std::floor(mapped->x()) == 100.0);
            REQUIRE(std::floor(mapped->y()) == 1.0);
        }
    }

    SECTION("A screen point in a letterbox bar or past the area maps to nothing") {
        REQUIRE_FALSE(screen_to_virtual(layout, vector2{42.9, 100.0}).has_value());
        REQUIRE_FALSE(screen_to_virtual(layout, vector2{500.0, 23.9}).has_value());
        REQUIRE_FALSE(screen_to_virtual(layout, vector2{1330.0, 100.0}).has_value());
        REQUIRE_FALSE(screen_to_virtual(layout, vector2{500.0, 760.0}).has_value());
        REQUIRE_FALSE(screen_to_virtual(layout, vector2{-5.0, -5.0}).has_value());
        REQUIRE_FALSE(screen_to_virtual(layout, vector2{5000.0, 5000.0}).has_value());
    }

    SECTION("The content's right and bottom edges are outside it") {
        REQUIRE_FALSE(screen_to_virtual(layout, vector2{1323.0, 100.0}).has_value());
        REQUIRE_FALSE(screen_to_virtual(layout, vector2{500.0, 744.0}).has_value());
    }

    SECTION("A screen point that is not finite maps to nothing") {
        constexpr double NaN{std::numeric_limits<double>::quiet_NaN()};
        REQUIRE_FALSE(screen_to_virtual(layout, vector2{NaN, 100.0}).has_value());
    }
}

TEST_CASE("screen_to_virtual() inverts virtual_to_screen()",
          "[tca][engine][rendering][scene][virtual_resolution][mapping][property]") {
    const std::uint32_t seed = GENERATE(1u, 42u, 1337u);
    std::mt19937 generator{seed};
    std::uniform_real_distribution<double> extent{1.0, 4000.0};
    std::uniform_int_distribution<std::uint32_t> resolutionExtent{1, 640};

    for (int sample{0}; sample < 500; ++sample) {
        const virtual_size resolution{.width = resolutionExtent(generator),
                                      .height = resolutionExtent(generator)};
        const pixel_perfect_layout layout{
            compute_pixel_perfect_layout(sized(extent(generator), extent(generator)), resolution)};

        std::uniform_real_distribution<double> virtualX{0.0, static_cast<double>(resolution.width)};
        std::uniform_real_distribution<double> virtualY{0.0,
                                                        static_cast<double>(resolution.height)};
        const vector2 virtualPoint{virtualX(generator), virtualY(generator)};
        CAPTURE(seed, sample, resolution.width, resolution.height, virtualPoint.x(),
                virtualPoint.y());

        const std::optional<vector2> roundTrip{
            screen_to_virtual(layout, virtual_to_screen(layout, virtualPoint))};
        REQUIRE(roundTrip.has_value());
        require_near(*roundTrip, virtualPoint, 1e-6);
    }
}
