// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/scene/camera-2d.hpp"

// camera_2d is pure data and maths: it must build with no renderer at all, exactly like the theme
// database. Checked right after the camera header (and before anything else can pull ImGui in), so
// any ImGui include reaching it - directly or transitively - fails this translation unit.
#if defined(IMGUI_VERSION)
#error "rendering/scene/camera-2d.hpp must not include an ImGui header"
#endif

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

// Third-party
#include <Eigen/Geometry>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Engine
#include "core/math/vector.hpp"
#include "core/utilities/ranged-value.hpp"

namespace {

using tca::engine::math::vector2;
using tca::engine::scene::camera_2d;

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// Absolute tolerance for values near zero, relative tolerance for large ones: a round trip through
// a zoom of MAX_ZOOM on coordinates in the thousands legitimately loses a few ulps.
constexpr double ABS_EPSILON{1e-9};
constexpr double REL_EPSILON{1e-12};

void require_near(const double actual, const double expected) {
    REQUIRE_THAT(actual, WithinAbs(expected, ABS_EPSILON) || WithinRel(expected, REL_EPSILON));
}

void require_near(const vector2& actual, const vector2& expected) {
    require_near(actual.x(), expected.x());
    require_near(actual.y(), expected.y());
}

void require_same_camera(const camera_2d& actual, const camera_2d& expected) {
    require_near(actual.position(), expected.position());
    require_near(actual.zoom(), expected.zoom());
    require_near(actual.viewport_size(), expected.viewport_size());
}

// Samples a zoom log-uniformly across the whole allowed range, so zoomed-out and zoomed-in cameras
// are exercised as often as ones near 1.0. Clamped because exp(log(x)) can land an ulp past x,
// which zoom_type would reject.
camera_2d::zoom_type random_zoom(std::mt19937& generator) {
    std::uniform_real_distribution<double> logZoom{std::log(camera_2d::MIN_ZOOM),
                                                   std::log(camera_2d::MAX_ZOOM)};
    return camera_2d::clamp_zoom(std::exp(logZoom(generator)));
}

// A camera configuration drawn at random, for the property-style tests.
struct camera_sample {
    vector2 viewportSize;
    vector2 position;
    camera_2d::zoom_type zoom{1.0};
};

camera_sample random_camera_sample(std::mt19937& generator) {
    std::uniform_real_distribution<double> viewportExtent{1.0, 4096.0};
    std::uniform_real_distribution<double> worldCoordinate{-10'000.0, 10'000.0};

    return camera_sample{
        .viewportSize = vector2{viewportExtent(generator),  viewportExtent(generator) },
        .position = vector2{worldCoordinate(generator), worldCoordinate(generator)},
        .zoom = random_zoom(generator),
    };
}

camera_2d make_camera(const camera_sample& sample) {
    camera_2d camera{sample.viewportSize};
    camera.set_position(sample.position);
    camera.set_zoom(sample.zoom);
    return camera;
}

std::string describe(const camera_sample& sample) {
    return "viewport (" + std::to_string(sample.viewportSize.x()) + ", " +
           std::to_string(sample.viewportSize.y()) + "), position (" +
           std::to_string(sample.position.x()) + ", " + std::to_string(sample.position.y()) +
           "), zoom " + std::to_string(sample.zoom.get());
}

constexpr std::size_t SAMPLES_PER_SEED{250};

} // namespace

// Lets Catch print a zoom_type in assertion messages instead of "{?}".
template <>
struct Catch::StringMaker<camera_2d::zoom_type> {
    static std::string convert(const camera_2d::zoom_type& zoom) {
        return StringMaker<double>::convert(zoom.get());
    }
};

// The zoom range goes through core's ranged_value rather than a bespoke check.
static_assert(
    std::same_as<camera_2d::zoom_type,
                 tca::engine::core::ranged_value<double, camera_2d::MIN_ZOOM, camera_2d::MAX_ZOOM,
                                                 tca::engine::core::rv_bound_policy::closed>>);
static_assert(std::same_as<decltype(std::declval<const camera_2d&>().zoom()), camera_2d::zoom_type>,
              "the zoom is handed out as a zoom_type, so its range travels with it");
static_assert(0.0 < camera_2d::MIN_ZOOM, "a zero or negative zoom has no inverse");
static_assert(camera_2d::MIN_ZOOM == 1.0 / camera_2d::MAX_ZOOM,
              "zooming out goes as far as zooming in, so no coordinate overflows at MIN_ZOOM");
static_assert(camera_2d::MIN_ZOOM < 1.0 && 1.0 < camera_2d::MAX_ZOOM,
              "an unzoomed camera must be representable");

static_assert(std::is_default_constructible_v<camera_2d>,
              "a camera can be created before the surface knows its size");
static_assert(std::copyable<camera_2d>, "a camera is a plain value");

// The visible rectangle reuses Eigen's box type rather than a new one.
static_assert(std::same_as<decltype(std::declval<const camera_2d&>().visible_world_rect()),
                           tca::engine::scene::world_rect>);
static_assert(std::same_as<tca::engine::scene::world_rect, Eigen::AlignedBox2d>);

TEST_CASE("camera_2d construction", "[tca][engine][rendering][scene][camera_2d]") {
    SECTION("A default-constructed camera sits at the world origin, unzoomed, with no viewport") {
        const camera_2d camera{};

        REQUIRE(camera.position() == vector2::zero());
        REQUIRE(camera.zoom() == 1.0);
        REQUIRE(camera.viewport_size() == vector2::zero());
    }

    SECTION("A camera built from a viewport size keeps it, at the origin and unzoomed") {
        const camera_2d camera{
            vector2{800.0, 600.0}
        };

        REQUIRE(camera.viewport_size() == vector2{800.0, 600.0});
        REQUIRE(camera.position() == vector2::zero());
        REQUIRE(camera.zoom() == 1.0);
    }

    SECTION("A negative viewport size is rejected") {
        // Named up front: a braced vector2{x, y} inside the macro would split its arguments at the
        // comma.
        const vector2 negativeWidth{-1.0, 600.0};
        const vector2 negativeHeight{800.0, -1.0};

        REQUIRE_THROWS_AS(camera_2d{negativeWidth}, std::invalid_argument);
        REQUIRE_THROWS_AS(camera_2d{negativeHeight}, std::invalid_argument);
    }

    SECTION("A NaN or infinite viewport size is rejected") {
        const double bad = GENERATE(std::numeric_limits<double>::quiet_NaN(),
                                    std::numeric_limits<double>::infinity(),
                                    -std::numeric_limits<double>::infinity());
        CAPTURE(bad);
        const vector2 badWidth{bad, 600.0};
        const vector2 badHeight{800.0, bad};

        REQUIRE_THROWS_AS(camera_2d{badWidth}, std::invalid_argument);
        REQUIRE_THROWS_AS(camera_2d{badHeight}, std::invalid_argument);
    }
}

TEST_CASE("camera_2d state", "[tca][engine][rendering][scene][camera_2d]") {
    camera_2d camera{
        vector2{800.0, 600.0}
    };

    SECTION("set_position() moves the camera to the given world position") {
        camera.set_position(vector2{12.0, -7.0});
        REQUIRE(camera.position() == vector2{12.0, -7.0});
    }

    SECTION("pan() moves the camera by a world-space delta") {
        camera.set_position(vector2{10.0, 10.0});
        camera.pan(vector2{2.5, -4.0});
        REQUIRE(camera.position() == vector2{12.5, 6.0});
    }

    SECTION("pan() is independent of the zoom: its delta is in world units") {
        camera.set_zoom(4.0);
        camera.pan(vector2{3.0, 0.0});
        REQUIRE(camera.position() == vector2{3.0, 0.0});
    }

    SECTION("set_viewport_size() replaces the viewport size") {
        camera.set_viewport_size(vector2{1920.0, 1080.0});
        REQUIRE(camera.viewport_size() == vector2{1920.0, 1080.0});
    }

    SECTION("set_viewport_size() rejects a negative size and leaves the camera untouched") {
        REQUIRE_THROWS_AS(camera.set_viewport_size(vector2{-10.0, 10.0}), std::invalid_argument);
        REQUIRE(camera.viewport_size() == vector2{800.0, 600.0});
    }

    SECTION("set_viewport_size() rejects a NaN size and leaves the camera untouched") {
        const vector2 nanSize{800.0, std::numeric_limits<double>::quiet_NaN()};
        REQUIRE_THROWS_AS(camera.set_viewport_size(nanSize), std::invalid_argument);
        REQUIRE(camera.viewport_size() == vector2{800.0, 600.0});
    }

    SECTION("A zero-sized viewport is allowed: a surface may not have been laid out yet") {
        REQUIRE_NOTHROW(camera.set_viewport_size(vector2::zero()));
        REQUIRE(camera.viewport_size() == vector2::zero());
    }
}

TEST_CASE("camera_2d zoom range", "[tca][engine][rendering][scene][camera_2d][zoom]") {
    camera_2d camera{
        vector2{800.0, 600.0}
    };

    SECTION("A zoom inside the range is kept as given") {
        const camera_2d::zoom_type zoom =
            GENERATE(camera_2d::MIN_ZOOM, 0.5, 1.0, 2.0, camera_2d::MAX_ZOOM);
        camera.set_zoom(zoom);
        REQUIRE(camera.zoom() == zoom);
    }

    SECTION("A zoom below MIN_ZOOM is rejected and leaves the camera untouched") {
        camera.set_zoom(2.0);
        const double zoom =
            GENERATE(std::nextafter(camera_2d::MIN_ZOOM, 0.0), camera_2d::MIN_ZOOM / 10.0,
                     std::numeric_limits<double>::denorm_min());
        CAPTURE(zoom);
        REQUIRE_THROWS_AS(camera.set_zoom(zoom), tca::engine::core::ranged_value_exception);
        REQUIRE(camera.zoom() == 2.0);
    }

    SECTION("A zoom above MAX_ZOOM is rejected and leaves the camera untouched") {
        camera.set_zoom(2.0);
        const double zoom =
            GENERATE(std::nextafter(camera_2d::MAX_ZOOM, std::numeric_limits<double>::infinity()),
                     camera_2d::MAX_ZOOM * 10.0, std::numeric_limits<double>::infinity());
        CAPTURE(zoom);
        REQUIRE_THROWS_AS(camera.set_zoom(zoom), tca::engine::core::ranged_value_exception);
        REQUIRE(camera.zoom() == 2.0);
    }

    SECTION("A zero or negative zoom is rejected, never inverting the transform") {
        camera.set_zoom(2.0);
        const double zoom = GENERATE(0.0, -0.0, -1.0, -std::numeric_limits<double>::infinity());
        CAPTURE(zoom);
        REQUIRE_THROWS_AS(camera.set_zoom(zoom), tca::engine::core::ranged_value_exception);
        REQUIRE(camera.zoom() == 2.0);
    }

    SECTION("A NaN zoom is rejected and leaves the camera untouched") {
        camera.set_zoom(2.0);
        REQUIRE_THROWS_AS(camera.set_zoom(std::numeric_limits<double>::quiet_NaN()),
                          tca::engine::core::ranged_value_exception);
        REQUIRE(camera.zoom() == 2.0);
    }
}

TEST_CASE("camera_2d::clamp_zoom()", "[tca][engine][rendering][scene][camera_2d][zoom]") {
    SECTION("A zoom inside the range is kept as given") {
        const double zoom = GENERATE(camera_2d::MIN_ZOOM, 0.5, 1.0, 3.0, camera_2d::MAX_ZOOM);
        REQUIRE(camera_2d::clamp_zoom(zoom) == zoom);
    }

    SECTION("A zoom above the range becomes MAX_ZOOM") {
        const double zoom = GENERATE(camera_2d::MAX_ZOOM * 1.1, camera_2d::MAX_ZOOM * 100.0,
                                     std::numeric_limits<double>::infinity());
        CAPTURE(zoom);
        REQUIRE(camera_2d::clamp_zoom(zoom) == camera_2d::MAX_ZOOM);
    }

    SECTION("A zoom below the range, zero or negative becomes MIN_ZOOM") {
        const double zoom = GENERATE(camera_2d::MIN_ZOOM * 0.9, 0.0, -1.0,
                                     -std::numeric_limits<double>::infinity());
        CAPTURE(zoom);
        REQUIRE(camera_2d::clamp_zoom(zoom) == camera_2d::MIN_ZOOM);
    }

    SECTION("NaN has no nearest bound and is rejected") {
        REQUIRE_THROWS_AS(camera_2d::clamp_zoom(std::numeric_limits<double>::quiet_NaN()),
                          tca::engine::core::ranged_value_exception);
    }

    SECTION("Scroll-style zooming past the range stops at the bound instead of throwing") {
        camera_2d camera{
            vector2{800.0, 600.0}
        };
        const vector2 cursor{123.0, 456.0};

        for (int step{}; step < 100; ++step) {
            camera.zoom_about(cursor, camera_2d::clamp_zoom(camera.zoom() * 1.25));
        }
        REQUIRE(camera.zoom() == camera_2d::MAX_ZOOM);

        for (int step{}; step < 100; ++step) {
            camera.zoom_about(cursor, camera_2d::clamp_zoom(camera.zoom() / 1.25));
        }
        REQUIRE(camera.zoom() == camera_2d::MIN_ZOOM);
    }
}

TEST_CASE("camera_2d world/screen transform",
          "[tca][engine][rendering][scene][camera_2d][transform]") {
    GIVEN("An unzoomed 800x600 camera at the world origin") {
        const camera_2d camera{
            vector2{800.0, 600.0}
        };

        THEN("The camera position lands at the viewport centre") {
            require_near(camera.world_to_screen(vector2::zero()), vector2{400.0, 300.0});
        }

        THEN("The viewport's top-left corner is half a viewport up and left of the position") {
            require_near(camera.screen_to_world(vector2::zero()), vector2{-400.0, -300.0});
        }

        THEN("Screen y grows downwards, like world y: one world unit down is one pixel down") {
            require_near(camera.world_to_screen(vector2{0.0, 1.0}), vector2{400.0, 301.0});
            require_near(camera.world_to_screen(vector2{1.0, 0.0}), vector2{401.0, 300.0});
        }
    }

    GIVEN("A panned camera") {
        camera_2d camera{
            vector2{800.0, 600.0}
        };
        camera.set_position(vector2{12.0, 7.0});

        THEN("Its position is what lands at the viewport centre") {
            require_near(camera.world_to_screen(vector2{12.0, 7.0}), vector2{400.0, 300.0});
            require_near(camera.screen_to_world(vector2{400.0, 300.0}), vector2{12.0, 7.0});
        }

        THEN("Everything else is offset by the pan, one pixel per world unit") {
            require_near(camera.world_to_screen(vector2{0.0, 0.0}), vector2{388.0, 293.0});
        }
    }

    GIVEN("A zoomed camera") {
        camera_2d camera{
            vector2{800.0, 600.0}
        };
        camera.set_position(vector2{10.0, 20.0});
        camera.set_zoom(4.0);

        THEN("The position still lands at the viewport centre") {
            require_near(camera.world_to_screen(vector2{10.0, 20.0}), vector2{400.0, 300.0});
        }

        THEN("One world unit spans zoom pixels") {
            require_near(camera.world_to_screen(vector2{11.0, 20.0}), vector2{404.0, 300.0});
            require_near(camera.world_to_screen(vector2{10.0, 18.0}), vector2{400.0, 292.0});
        }

        THEN("One pixel spans 1/zoom world units") {
            require_near(camera.screen_to_world(vector2{401.0, 300.0}), vector2{10.25, 20.0});
        }
    }

    GIVEN("A zoomed camera panned by a world-space delta") {
        camera_2d camera{
            vector2{800.0, 600.0}
        };
        camera.set_position(vector2{10.0, 20.0});
        camera.set_zoom(4.0);
        camera.pan(vector2{5.0, -2.0});

        THEN("The panned-to world point lands at the viewport centre") {
            require_near(camera.world_to_screen(vector2{15.0, 18.0}), vector2{400.0, 300.0});
            require_near(camera.screen_to_world(vector2{400.0, 300.0}), vector2{15.0, 18.0});
        }

        THEN("The old centre moved by the delta scaled by the zoom, the opposite way") {
            require_near(camera.world_to_screen(vector2{10.0, 20.0}), vector2{380.0, 308.0});
        }
    }

    GIVEN("A zoomed-out camera") {
        camera_2d camera{
            vector2{800.0, 600.0}
        };
        camera.set_zoom(0.5);

        THEN("World distances shrink on screen") {
            require_near(camera.world_to_screen(vector2{100.0, 100.0}), vector2{450.0, 350.0});
        }
    }

    GIVEN("A non-square viewport with odd dimensions") {
        camera_2d camera{
            vector2{1023.0, 17.0}
        };

        THEN("The centre is at half the viewport, not rounded to a pixel") {
            require_near(camera.world_to_screen(vector2::zero()), vector2{511.5, 8.5});
        }
    }

    GIVEN("A zero-sized viewport") {
        camera_2d camera{vector2::zero()};
        camera.set_position(vector2{5.0, 5.0});
        camera.set_zoom(2.0);

        THEN("The transform still round-trips rather than dividing by zero") {
            const vector2 world{3.0, -8.0};
            const vector2 screen{camera.world_to_screen(world)};
            REQUIRE(std::isfinite(screen.x()));
            REQUIRE(std::isfinite(screen.y()));
            require_near(camera.screen_to_world(screen), world);
        }
    }
}

TEST_CASE("camera_2d transform properties",
          "[tca][engine][rendering][scene][camera_2d][transform][property]") {
    const std::uint32_t seed = GENERATE(1u, 42u, 1337u, 0xC0FFEEu);
    std::mt19937 generator{seed};
    std::uniform_real_distribution<double> worldCoordinate{-50'000.0, 50'000.0};
    std::uniform_real_distribution<double> unit{0.0, 1.0};

    SECTION("world_to_screen and screen_to_world are exact inverses") {
        for (std::size_t i{}; i < SAMPLES_PER_SEED; ++i) {
            const camera_sample sample{random_camera_sample(generator)};
            const camera_2d camera{make_camera(sample)};
            const vector2 world{worldCoordinate(generator), worldCoordinate(generator)};
            const vector2 screen{unit(generator) * sample.viewportSize.x(),
                                 unit(generator) * sample.viewportSize.y()};

            INFO("seed " << seed << ", " << describe(sample));
            require_near(camera.screen_to_world(camera.world_to_screen(world)), world);
            require_near(camera.world_to_screen(camera.screen_to_world(screen)), screen);
        }
    }

    SECTION("The camera position always lands at the viewport centre") {
        for (std::size_t i{}; i < SAMPLES_PER_SEED; ++i) {
            const camera_sample sample{random_camera_sample(generator)};
            const camera_2d camera{make_camera(sample)};

            INFO("seed " << seed << ", " << describe(sample));
            require_near(camera.world_to_screen(sample.position),
                         vector2{sample.viewportSize / 2.0});
        }
    }

    SECTION("Screen distances are world distances scaled by the zoom, on both axes") {
        for (std::size_t i{}; i < SAMPLES_PER_SEED; ++i) {
            const camera_sample sample{random_camera_sample(generator)};
            const camera_2d camera{make_camera(sample)};
            const vector2 a{worldCoordinate(generator), worldCoordinate(generator)};
            const vector2 b{worldCoordinate(generator), worldCoordinate(generator)};

            INFO("seed " << seed << ", " << describe(sample));
            const vector2 screenDelta{camera.world_to_screen(b) - camera.world_to_screen(a)};
            require_near(screenDelta, vector2{(b - a) * sample.zoom.get()});
        }
    }

    SECTION("The Eigen transforms agree with the point-wise functions and invert each other") {
        for (std::size_t i{}; i < SAMPLES_PER_SEED; ++i) {
            const camera_sample sample{random_camera_sample(generator)};
            const camera_2d camera{make_camera(sample)};
            const vector2 world{worldCoordinate(generator), worldCoordinate(generator)};

            const Eigen::Affine2d worldToScreen{camera.world_to_screen_transform()};
            const Eigen::Affine2d screenToWorld{camera.screen_to_world_transform()};

            INFO("seed " << seed << ", " << describe(sample));
            require_near(vector2{worldToScreen * world}, camera.world_to_screen(world));
            require_near(vector2{screenToWorld * camera.world_to_screen(world)}, world);

            // No rotation or shear: only an axis-aligned uniform scale and a translation.
            const Eigen::Matrix2d linear{worldToScreen.linear()};
            require_near(linear(0, 0), sample.zoom);
            require_near(linear(1, 1), sample.zoom);
            require_near(linear(0, 1), 0.0);
            require_near(linear(1, 0), 0.0);
        }
    }
}

TEST_CASE("camera_2d zoom about a point",
          "[tca][engine][rendering][scene][camera_2d][zoom][anchor]") {
    camera_2d camera{
        vector2{800.0, 600.0}
    };
    camera.set_position(vector2{120.0, -45.0});
    camera.set_zoom(1.5);

    // Where the anchor sits on screen: the viewport centre, each corner, and well outside the
    // viewport in every direction.
    const vector2 anchor =
        GENERATE(vector2{400.0, 300.0}, // centre
                 vector2{0.0, 0.0},     // top-left corner
                 vector2{800.0, 0.0},   // top-right corner
                 vector2{0.0, 600.0},   // bottom-left corner
                 vector2{800.0, 600.0}, // bottom-right corner
                 vector2{-350.0, 125.0}, vector2{1900.0, -40.0}, vector2{410.0, 5000.0});
    const camera_2d::zoom_type newZoom =
        GENERATE(camera_2d::MIN_ZOOM, 0.25, 1.0, 3.0, camera_2d::MAX_ZOOM);

    CAPTURE(anchor.x(), anchor.y(), newZoom);
    const vector2 anchoredWorld{camera.screen_to_world(anchor)};

    camera.zoom_about(anchor, newZoom);

    THEN("The zoom changes to the requested value") {
        REQUIRE(camera.zoom() == newZoom);
    }

    THEN("The world position under the anchor stays under the anchor") {
        require_near(camera.screen_to_world(anchor), anchoredWorld);
        require_near(camera.world_to_screen(anchoredWorld), anchor);
    }

    THEN("The viewport is untouched") {
        REQUIRE(camera.viewport_size() == vector2{800.0, 600.0});
    }
}

TEST_CASE("camera_2d zoom about a point, special cases",
          "[tca][engine][rendering][scene][camera_2d][zoom][anchor]") {
    camera_2d camera{
        vector2{800.0, 600.0}
    };
    camera.set_position(vector2{120.0, -45.0});
    camera.set_zoom(2.0);

    SECTION("Zooming about the viewport centre never moves the camera") {
        camera.zoom_about(vector2{400.0, 300.0}, 8.0);
        require_near(camera.position(), vector2{120.0, -45.0});
    }

    SECTION("Zooming about any other point moves the camera towards it when zooming in") {
        const vector2 anchor{700.0, 300.0};
        const vector2 anchoredWorld{camera.screen_to_world(anchor)};
        camera.zoom_about(anchor, 4.0);

        REQUIRE(camera.position().x() > 120.0);
        REQUIRE(camera.position().x() < anchoredWorld.x());
        require_near(camera.position().y(), -45.0);
    }

    SECTION("An out-of-range zoom is rejected and leaves the camera untouched") {
        const camera_2d before{camera};

        const double requested = GENERATE(camera_2d::MAX_ZOOM * 100.0, 0.0, -3.0);
        CAPTURE(requested);
        REQUIRE_THROWS_AS(camera.zoom_about(vector2{50.0, 580.0}, requested),
                          tca::engine::core::ranged_value_exception);
        require_same_camera(camera, before);
    }

    SECTION("Zooming to the current zoom is a no-op") {
        const camera_2d before{camera};
        camera.zoom_about(vector2{17.0, 590.0}, 2.0);
        require_same_camera(camera, before);
    }

    SECTION("A NaN zoom is rejected and leaves the camera untouched") {
        const camera_2d before{camera};
        REQUIRE_THROWS_AS(
            camera.zoom_about(vector2{17.0, 590.0}, std::numeric_limits<double>::quiet_NaN()),
            tca::engine::core::ranged_value_exception);
        require_same_camera(camera, before);
    }

    SECTION("Zooming in then back out about the same anchor restores the camera") {
        const camera_2d before{camera};
        const vector2 anchor{-120.0, 810.0};

        camera.zoom_about(anchor, 16.0);
        camera.zoom_about(anchor, 0.1);
        camera.zoom_about(anchor, 2.0);

        require_same_camera(camera, before);
    }
}

TEST_CASE("camera_2d zoom about a point, properties",
          "[tca][engine][rendering][scene][camera_2d][zoom][anchor][property]") {
    const std::uint32_t seed = GENERATE(1u, 42u, 1337u, 0xC0FFEEu);
    std::mt19937 generator{seed};
    // Anchors range over three viewports' worth in each direction, so most fall outside it.
    std::uniform_real_distribution<double> anchorFraction{-3.0, 4.0};

    for (std::size_t i{}; i < SAMPLES_PER_SEED; ++i) {
        const camera_sample sample{random_camera_sample(generator)};
        camera_2d camera{make_camera(sample)};
        const vector2 anchor{anchorFraction(generator) * sample.viewportSize.x(),
                             anchorFraction(generator) * sample.viewportSize.y()};
        const camera_2d::zoom_type newZoom{random_zoom(generator)};

        INFO("seed " << seed << ", " << describe(sample) << ", anchor (" << anchor.x() << ", "
                     << anchor.y() << "), new zoom " << newZoom.get());

        const vector2 anchoredWorld{camera.screen_to_world(anchor)};
        camera.zoom_about(anchor, newZoom);

        require_near(camera.zoom(), newZoom);
        require_near(camera.screen_to_world(anchor), anchoredWorld);
    }
}

TEST_CASE("camera_2d viewport resizing", "[tca][engine][rendering][scene][camera_2d][viewport]") {
    camera_2d camera{
        vector2{800.0, 600.0}
    };
    camera.set_position(vector2{33.0, -12.0});
    camera.set_zoom(2.5);

    camera.set_viewport_size(vector2{1920.0, 1080.0});

    THEN("The camera position stays at the (new) viewport centre") {
        require_near(camera.world_to_screen(vector2{33.0, -12.0}), vector2{960.0, 540.0});
    }

    THEN("Neither the position nor the zoom change") {
        REQUIRE(camera.position() == vector2{33.0, -12.0});
        REQUIRE(camera.zoom() == 2.5);
    }

    THEN("The scale does not change: a bigger viewport shows more world, not a bigger one") {
        const vector2 screenDelta{camera.world_to_screen(vector2{34.0, -12.0}) -
                                  camera.world_to_screen(vector2{33.0, -12.0})};
        require_near(screenDelta, vector2{2.5, 0.0});
    }
}

TEST_CASE("camera_2d visible world rectangle",
          "[tca][engine][rendering][scene][camera_2d][culling]") {
    GIVEN("An unzoomed 800x600 camera at the origin") {
        const camera_2d camera{
            vector2{800.0, 600.0}
        };
        const auto visible = camera.visible_world_rect();

        THEN("It spans the viewport around the position") {
            require_near(vector2{visible.min()}, vector2{-400.0, -300.0});
            require_near(vector2{visible.max()}, vector2{400.0, 300.0});
        }
    }

    GIVEN("A panned and zoomed camera") {
        camera_2d camera{
            vector2{800.0, 600.0}
        };
        camera.set_position(vector2{100.0, 50.0});
        camera.set_zoom(4.0);
        const auto visible = camera.visible_world_rect();

        THEN("Its size is the viewport size divided by the zoom") {
            require_near(vector2{visible.sizes()}, vector2{200.0, 150.0});
        }

        THEN("It is centred on the camera position") {
            require_near(vector2{visible.center()}, vector2{100.0, 50.0});
        }
    }

    GIVEN("A zoomed-out camera") {
        camera_2d camera{
            vector2{800.0, 600.0}
        };
        camera.set_zoom(0.5);

        THEN("It shows more world than the viewport has pixels") {
            require_near(vector2{camera.visible_world_rect().sizes()}, vector2{1600.0, 1200.0});
        }
    }

    GIVEN("A zero-sized viewport") {
        camera_2d camera{vector2::zero()};
        camera.set_position(vector2{5.0, -5.0});

        THEN("It collapses to the camera position rather than being empty or inverted") {
            const auto visible = camera.visible_world_rect();
            require_near(vector2{visible.min()}, vector2{5.0, -5.0});
            require_near(vector2{visible.max()}, vector2{5.0, -5.0});
        }
    }
}

TEST_CASE("camera_2d visible world rectangle matches the transform",
          "[tca][engine][rendering][scene][camera_2d][culling][property]") {
    const std::uint32_t seed = GENERATE(1u, 42u, 1337u, 0xC0FFEEu);
    std::mt19937 generator{seed};
    std::uniform_real_distribution<double> unit{0.0, 1.0};
    // Strictly outside [0, 1], on either side.
    std::uniform_real_distribution<double> outsideFraction{1.001, 3.0};
    std::bernoulli_distribution coin{0.5};

    for (std::size_t i{}; i < SAMPLES_PER_SEED; ++i) {
        const camera_sample sample{random_camera_sample(generator)};
        const camera_2d camera{make_camera(sample)};
        const auto visible = camera.visible_world_rect();

        INFO("seed " << seed << ", " << describe(sample));

        // Its corners are exactly where the viewport's corners map to.
        require_near(vector2{visible.min()}, camera.screen_to_world(vector2::zero()));
        require_near(vector2{visible.max()}, camera.screen_to_world(sample.viewportSize));
        require_near(camera.world_to_screen(vector2{visible.min()}), vector2::zero());
        require_near(camera.world_to_screen(vector2{visible.max()}), sample.viewportSize);

        // Every world point inside it lands inside the viewport.
        const vector2 inside{
            visible.min().x() + unit(generator) * visible.sizes().x(),
            visible.min().y() + unit(generator) * visible.sizes().y(),
        };
        REQUIRE(visible.contains(inside));
        const vector2 insideOnScreen{camera.world_to_screen(inside)};
        CHECK(insideOnScreen.x() >= -ABS_EPSILON);
        CHECK(insideOnScreen.y() >= -ABS_EPSILON);
        CHECK(insideOnScreen.x() <= sample.viewportSize.x() + ABS_EPSILON);
        CHECK(insideOnScreen.y() <= sample.viewportSize.y() + ABS_EPSILON);

        // And every world point outside it lands outside the viewport, so culling against it
        // never drops something that would have been visible.
        // Pushed out along one axis at a time, on either side.
        for (const Eigen::Index axis : {Eigen::Index{0}, Eigen::Index{1}}) {
            vector2 fraction{unit(generator), unit(generator)};
            fraction[axis] =
                coin(generator) ? outsideFraction(generator) : 1.0 - outsideFraction(generator);
            const vector2 outside{visible.min() + fraction.cwiseProduct(visible.sizes())};

            CAPTURE(axis);
            REQUIRE_FALSE(visible.contains(outside));
            const vector2 outsideOnScreen{camera.world_to_screen(outside)};
            CHECK(
                (outsideOnScreen[axis] < 0.0 || outsideOnScreen[axis] > sample.viewportSize[axis]));
        }
    }
}
