// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/scene-grid.hpp"

// The grid maths is pure data, exactly like camera_2d: it must build with no renderer at all, so
// the surface's grid can be reasoned about (and tested) without ImGui. Checked right after the grid
// header, before anything else can pull ImGui in.
#if defined(IMGUI_VERSION)
#error "rendering/scene/scene-grid.hpp must not include an ImGui header"
#endif

// C++ STL
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "core/math/vector.hpp"
#include "rendering/scene/camera-2d.hpp"

namespace {

using tca::engine::math::vector2;
using tca::engine::scene::camera_2d;
using tca::engine::scene::grid_lines;
using tca::engine::scene::visible_grid_lines;

using Catch::Matchers::WithinAbs;

constexpr double EPSILON{1e-9};

void require_lines(const std::vector<double>& actual, const std::vector<double>& expected) {
    REQUIRE(actual.size() == expected.size());
    for (std::size_t i{0}; i < actual.size(); ++i) {
        CAPTURE(i);
        REQUIRE_THAT(actual[i], WithinAbs(expected[i], EPSILON));
    }
}

camera_2d make_camera(const vector2& viewportSize, const vector2& position = vector2::zero(),
                      const double zoom = 1.0) {
    camera_2d camera{viewportSize};
    camera.set_position(position);
    camera.set_zoom(zoom);
    return camera;
}

// Checks one axis of a grid_lines result against the visible range [minimum, maximum] on that
// axis: ascending, one spacing apart, all inside the range, and none missing at either end.
void require_complete_axis(const std::vector<double>& lines, const double minimum,
                           const double maximum, const double spacing) {
    // Relative tolerance: coordinates reach the tens of thousands at MIN_ZOOM.
    const double tolerance{std::max(1.0, std::abs(minimum) + std::abs(maximum)) * 1e-9};

    REQUIRE_FALSE(lines.empty());
    for (std::size_t i{0}; i < lines.size(); ++i) {
        CAPTURE(i, lines[i]);
        REQUIRE(lines[i] >= minimum - tolerance);
        REQUIRE(lines[i] <= maximum + tolerance);

        // Every line sits on a whole multiple of the spacing, so the grid is anchored to the world
        // rather than to the viewport and does not swim while the camera pans.
        const double steps{lines[i] / spacing};
        REQUIRE_THAT(steps, WithinAbs(std::round(steps), 1e-6));

        if (i > 0) {
            REQUIRE_THAT(lines[i] - lines[i - 1], WithinAbs(spacing, tolerance));
        }
    }

    REQUIRE(lines.front() - spacing < minimum + tolerance);
    REQUIRE(lines.back() + spacing > maximum - tolerance);
}

} // namespace

TEST_CASE("visible_grid_lines() on a camera at the origin",
          "[tca][engine][rendering][scene][scene_grid]") {
    SECTION("An unzoomed camera shows every line its viewport covers, edges included") {
        // 400x300 at the origin, zoom 1: world x in [-200, 200], world y in [-150, 150].
        const camera_2d camera{make_camera(vector2{400.0, 300.0})};

        const grid_lines lines{visible_grid_lines(camera, 100.0)};

        require_lines(lines.vertical, {-200.0, -100.0, 0.0, 100.0, 200.0});
        require_lines(lines.horizontal, {-100.0, 0.0, 100.0});
    }

    SECTION("Zooming in shows fewer lines, since less world is visible") {
        // Zoom 2: world x in [-100, 100], world y in [-75, 75].
        const camera_2d camera{make_camera(vector2{400.0, 300.0}, vector2::zero(), 2.0)};

        const grid_lines lines{visible_grid_lines(camera, 100.0)};

        require_lines(lines.vertical, {-100.0, 0.0, 100.0});
        require_lines(lines.horizontal, {0.0});
    }

    SECTION("Zooming out shows more lines") {
        // Zoom 0.5: world x in [-400, 400], world y in [-300, 300].
        const camera_2d camera{make_camera(vector2{400.0, 300.0}, vector2::zero(), 0.5)};

        const grid_lines lines{visible_grid_lines(camera, 200.0)};

        require_lines(lines.vertical, {-400.0, -200.0, 0.0, 200.0, 400.0});
        require_lines(lines.horizontal, {-200.0, 0.0, 200.0});
    }

    SECTION("A fractional spacing is honoured") {
        // 4x2 pixels at zoom 4: world x in [-0.5, 0.5], world y in [-0.25, 0.25].
        const camera_2d camera{make_camera(vector2{4.0, 2.0}, vector2::zero(), 4.0)};

        const grid_lines lines{visible_grid_lines(camera, 0.5)};

        require_lines(lines.vertical, {-0.5, 0.0, 0.5});
        require_lines(lines.horizontal, {0.0});
    }
}

TEST_CASE("visible_grid_lines() is anchored to the world, not to the viewport",
          "[tca][engine][rendering][scene][scene_grid]") {
    SECTION("A panned camera shows the lines at the same world positions") {
        // Centred on (50, 25): world x in [-150, 250], world y in [-125, 175].
        const camera_2d camera{make_camera(vector2{400.0, 300.0}, vector2{50.0, 25.0})};

        const grid_lines lines{visible_grid_lines(camera, 100.0)};

        require_lines(lines.vertical, {-100.0, 0.0, 100.0, 200.0});
        require_lines(lines.horizontal, {-100.0, 0.0, 100.0});
    }

    SECTION("A camera far from the origin still lands its lines on whole multiples") {
        const camera_2d camera{make_camera(vector2{400.0, 300.0}, vector2{-12'345.0, 6'789.0})};

        const grid_lines lines{visible_grid_lines(camera, 100.0)};

        require_lines(lines.vertical, {-12'500.0, -12'400.0, -12'300.0, -12'200.0});
        require_lines(lines.horizontal, {6'700.0, 6'800.0, 6'900.0});
    }

    SECTION("Panning by exactly one spacing shifts the line set by one spacing") {
        camera_2d camera{make_camera(vector2{400.0, 300.0}, vector2{50.0, 25.0})};
        const grid_lines before{visible_grid_lines(camera, 100.0)};

        camera.pan(vector2{100.0, 100.0});
        const grid_lines after{visible_grid_lines(camera, 100.0)};

        REQUIRE(after.vertical.size() == before.vertical.size());
        REQUIRE(after.horizontal.size() == before.horizontal.size());
        for (std::size_t i{0}; i < before.vertical.size(); ++i) {
            REQUIRE_THAT(after.vertical[i], WithinAbs(before.vertical[i] + 100.0, EPSILON));
        }
        for (std::size_t i{0}; i < before.horizontal.size(); ++i) {
            REQUIRE_THAT(after.horizontal[i], WithinAbs(before.horizontal[i] + 100.0, EPSILON));
        }
    }
}

TEST_CASE("visible_grid_lines() edge cases", "[tca][engine][rendering][scene][scene_grid]") {
    SECTION("A camera with an empty viewport shows no lines: there is nothing to draw them on") {
        const camera_2d camera{};

        const grid_lines lines{visible_grid_lines(camera, 100.0)};

        REQUIRE(lines.vertical.empty());
        REQUIRE(lines.horizontal.empty());
    }

    SECTION("A viewport empty on one axis only shows no lines either") {
        const camera_2d zeroWidth{make_camera(vector2{0.0, 300.0})};
        const camera_2d zeroHeight{make_camera(vector2{400.0, 0.0})};

        const grid_lines fromZeroWidth{visible_grid_lines(zeroWidth, 100.0)};
        const grid_lines fromZeroHeight{visible_grid_lines(zeroHeight, 100.0)};

        REQUIRE(fromZeroWidth.vertical.empty());
        REQUIRE(fromZeroWidth.horizontal.empty());
        REQUIRE(fromZeroHeight.vertical.empty());
        REQUIRE(fromZeroHeight.horizontal.empty());
    }

    SECTION("A spacing wider than the visible world still shows the one line inside it") {
        // World x in [-200, 200], world y in [-150, 150]: only 0 is a multiple of 1000 on both.
        const camera_2d camera{make_camera(vector2{400.0, 300.0})};

        const grid_lines lines{visible_grid_lines(camera, 1'000.0)};

        require_lines(lines.vertical, {0.0});
        require_lines(lines.horizontal, {0.0});
    }

    SECTION("A spacing wider than the visible world may show no line at all") {
        // World x in [300, 700], world y in [350, 650]: no multiple of 1000 on either axis.
        const camera_2d camera{make_camera(vector2{400.0, 300.0}, vector2{500.0, 500.0})};

        const grid_lines lines{visible_grid_lines(camera, 1'000.0)};

        REQUIRE(lines.vertical.empty());
        REQUIRE(lines.horizontal.empty());
    }

    SECTION("A zero, negative or non-finite spacing is rejected") {
        const camera_2d camera{make_camera(vector2{400.0, 300.0})};
        const double badSpacing = GENERATE(
            0.0, -0.0, -1.0, -100.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity());
        CAPTURE(badSpacing);

        REQUIRE_THROWS_AS(visible_grid_lines(camera, badSpacing), std::invalid_argument);
    }
}

TEST_CASE("visible_grid_lines() matches the camera's visible world rectangle",
          "[tca][engine][rendering][scene][scene_grid][property]") {
    const std::uint32_t seed = GENERATE(1u, 2u, 3u, 42u, 1337u);
    CAPTURE(seed);
    std::mt19937 generator{seed};

    std::uniform_real_distribution<double> viewportExtent{16.0, 4096.0};
    std::uniform_real_distribution<double> worldCoordinate{-10'000.0, 10'000.0};
    std::uniform_real_distribution<double> logZoom{std::log(camera_2d::MIN_ZOOM),
                                                   std::log(camera_2d::MAX_ZOOM)};

    for (std::size_t sample{0}; sample < 100; ++sample) {
        const camera_2d camera{
            make_camera(vector2{viewportExtent(generator), viewportExtent(generator)},
                        vector2{worldCoordinate(generator), worldCoordinate(generator)},
                        camera_2d::clamp_zoom(std::exp(logZoom(generator))).get())};

        // A spacing that always gives between 1 and ~64 cells across the visible world, so each
        // axis has at least one line and the test stays cheap at MIN_ZOOM.
        const auto visible{camera.visible_world_rect()};
        const double visibleExtent{std::max(visible.sizes().x(), visible.sizes().y())};
        std::uniform_real_distribution<double> cellsAcross{1.0, 64.0};
        const double spacing{visibleExtent / cellsAcross(generator)};

        CAPTURE(sample, spacing, camera.viewport_size().x(), camera.viewport_size().y(),
                camera.position().x(), camera.position().y(), camera.zoom().get());

        const grid_lines lines{visible_grid_lines(camera, spacing)};

        // A whole-axis spacing below the visible extent guarantees at least one line per axis
        // only on the axis that decided the spacing; the other one may legitimately be empty.
        if (!lines.vertical.empty()) {
            require_complete_axis(lines.vertical, visible.min().x(), visible.max().x(), spacing);
        }
        if (!lines.horizontal.empty()) {
            require_complete_axis(lines.horizontal, visible.min().y(), visible.max().y(), spacing);
        }
        REQUIRE_FALSE((lines.vertical.empty() && lines.horizontal.empty()));

        // Every vertical line maps into the viewport horizontally, every horizontal one
        // vertically: the grid never draws a line the viewport cannot show.
        for (const double x : lines.vertical) {
            const double screenX{camera.world_to_screen(vector2{x, camera.position().y()}).x()};
            REQUIRE(screenX >= -1e-6);
            REQUIRE(screenX <= camera.viewport_size().x() + 1e-6);
        }
        for (const double y : lines.horizontal) {
            const double screenY{camera.world_to_screen(vector2{camera.position().x(), y}).y()};
            REQUIRE(screenY >= -1e-6);
            REQUIRE(screenY <= camera.viewport_size().y() + 1e-6);
        }
    }
}
