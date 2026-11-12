// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT

// C++ STL
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Engine
#include "application/application.hpp"
#include "application/game-application-logic.hpp"
#include "core/math/vector.hpp"
#include "input/actions/standard-actions.hpp"
#include "input/input-frame.hpp"
#include "input/key.hpp"
#include "platform/folders.hpp"
#include "platform/images/image-decoder.hpp"
#include "rendering/colors.hpp"
#include "rendering/custom-tickables/fixed-step-tickables.hpp"
#include "rendering/gui/fps-overlay.hpp"
#include "rendering/gui/label.hpp"
#include "rendering/gui/layout-widgets.hpp"
#include "rendering/image-factory.hpp"
#include "rendering/image.hpp"
#include "rendering/scene/camera-2d.hpp"
#include "rendering/scene/scene-renderable.hpp"
#include "rendering/scene/scene-surface.hpp"
#include "rendering/scene/sprite.hpp"
#include "rendering/scene/texture-atlas.hpp"
#include "rendering/scene/virtual-resolution.hpp"
#include "rendering/theming/active-theme.hpp"
#include "rendering/theming/theme-factories.hpp"
#include "rendering/theming/theme-roles.hpp"
#include "rendering/tickables.hpp"

// This demo is written the way a game consuming the engine would be: it names widgets, roles and
// application services, and never reaches for Dear ImGui (or any other backend) itself. If
// something here needed an include from a rendering backend, that would be a hole in the engine's
// public API rather than something to work around locally.

namespace {

namespace application = tca::engine::application;
namespace input = tca::engine::input;
namespace math = tca::engine::math;
namespace platform = tca::engine::platform;
namespace rendering = tca::engine::rendering;
namespace scene = tca::engine::scene;
namespace theming = tca::engine::theming;
namespace widgets = tca::engine::widgets;

// ---------------------------------------------------------------------------
// The sprite atlas, loaded from the demo's assets folder
// ---------------------------------------------------------------------------

// The atlas is two files side by side: the texture, and a plain-text list naming its regions, so
// the art can be redrawn or repacked without touching this file.
constexpr std::string_view ATLAS_TEXTURE_FILE{"sprites.png"};
constexpr std::string_view ATLAS_REGIONS_FILE{"sprites.atlas"};

/**
 * @brief The atlas's texture and the atlas naming its regions. The image owns the texture, so it
 * lives as long as the atlas drawing from it.
 */
struct demo_atlas final {
    rendering::image image;
    scene::texture_atlas atlas;
};

// Each line of the regions file is "name x y width height", in texels; blank lines and lines
// starting with '#' are skipped. Anything else is an error naming the file and line, rather than
// a sprite silently missing on screen.
void add_regions_from_file(scene::texture_atlas& atlas, const std::filesystem::path& path) {
    std::ifstream file{path};
    if (!file) {
        throw std::runtime_error{
            std::format("Cannot open the atlas regions file {}", path.string())};
    }

    std::string line{};
    for (std::size_t lineNumber{1}; std::getline(file, line); ++lineNumber) {
        std::istringstream fields{line};
        std::string name{};
        if (!(fields >> name) || name.starts_with('#')) {
            continue;
        }

        scene::pixel_rect pixels{};
        std::string extra{};
        if (!(fields >> pixels.x >> pixels.y >> pixels.width >> pixels.height) || fields >> extra) {
            throw std::runtime_error{std::format("{}:{}: expected \"name x y width height\"",
                                                 path.string(), lineNumber)};
        }

        atlas.add_region(std::move(name), pixels);
    }
}

demo_atlas load_demo_atlas(const rendering::image_factory& factory,
                           const std::filesystem::path& assetsFolder) {
    rendering::image image{factory.load_from_file(assetsFolder / ATLAS_TEXTURE_FILE)};
    scene::texture_atlas atlas{image.as_sprite_texture()};
    add_regions_from_file(atlas, assetsFolder / ATLAS_REGIONS_FILE);

    return demo_atlas{.image = std::move(image), .atlas = std::move(atlas)};
}

// ---------------------------------------------------------------------------
// The game: a ship the player flies, and coins to pick up
// ---------------------------------------------------------------------------

// The board the ship flies over, in world units, centred on the origin. The camera zooms so it
// always fits the surface.
constexpr double BOARD_HALF_WIDTH{600.0};
constexpr double BOARD_HALF_HEIGHT{300.0};
constexpr double SHIP_HALF_SIZE{24.0};
constexpr double COIN_HALF_SIZE{14.0};

/**
 * @brief The game's rules, run at a fixed rate whatever the frame rate: the ship moves from the
 * player's actions and picks up the coins it touches, each coming back somewhere else a moment
 * later.
 *
 * It reads the engine's standard actions rather than keys, so the controls are whatever the
 * action map binds (WASD and the arrows to move, E or Space to dash, by default). Steering reads
 * levels (is the action active?) and the dash an edge (was it triggered?), so a tap shorter than a
 * step still moves the ship and a dash is never missed.
 */
class flight_game final : public rendering::fixed_step_updatable {
public:
    static constexpr std::size_t STEP_RATE_HZ{60};
    static constexpr std::size_t COIN_COUNT{5};

    /**
     * @brief A coin on the board. A picked-up one is hidden until it respawns, already at the
     * spot it will come back to.
     */
    struct coin final {
        Eigen::Vector2d position{};
        std::size_t respawnStepsLeft{};

        bool is_shown() const noexcept {
            return respawnStepsLeft == 0;
        }
    };

    /**
     * @brief Where a coin was just picked up, for the burst drawn there.
     */
    struct pickup_flash final {
        Eigen::Vector2d position{};
        std::size_t stepsLeft{};
    };

    static constexpr std::size_t FLASH_STEPS{STEP_RATE_HZ * 3 / 10};

    flight_game()
        : fixed_step_updatable{
              step_duration{std::chrono::nanoseconds{std::chrono::seconds{1}} / STEP_RATE_HZ},
              max_steps_per_frame{5}} {
        // The first coins are lined up across the top of the board; later ones land anywhere.
        constexpr double COIN_SPACING{140.0};
        for (std::size_t i{0}; i < COIN_COUNT; ++i) {
            m_coins[i].position =
                Eigen::Vector2d{(static_cast<double>(i) - 2.0) * COIN_SPACING, -220.0};
        }
    }

    /**
     * @brief Where to draw the ship this frame: between the last two steps, so its motion stays
     * smooth on screens refreshing faster than the step rate.
     */
    math::vector2 drawn_ship_position() const noexcept {
        const double alpha{get_interpolation_alpha().get()};
        return math::vector2{Eigen::Vector2d{m_previousShipPosition +
                                             (m_shipPosition - m_previousShipPosition) * alpha}};
    }

    bool is_thrusting() const noexcept {
        return m_isThrusting;
    }

    bool is_dashing() const noexcept {
        return m_dashStepsLeft > 0;
    }

    std::span<const coin> coins() const noexcept {
        return m_coins;
    }

    std::span<const pickup_flash> pickup_flashes() const noexcept {
        return m_pickupFlashes;
    }

    std::size_t coins_collected() const noexcept {
        return m_coinsCollected;
    }

private:
    static constexpr double SPEED{320.0};
    static constexpr double DASH_SPEED_FACTOR{3.5};
    static constexpr std::size_t DASH_STEPS{STEP_RATE_HZ / 6};
    static constexpr std::size_t COIN_RESPAWN_STEPS{STEP_RATE_HZ * 3 / 2};
    // A little forgiving: the ship's sprite doesn't fill its whole square.
    static constexpr double PICKUP_DISTANCE{SHIP_HALF_SIZE + COIN_HALF_SIZE - 8.0};

    void step(rendering::step_context& context) override {
        count_down_timers();
        move_ship(context.input());
        pick_up_coins();
    }

    void count_down_timers() {
        for (coin& c : m_coins) {
            if (!c.is_shown()) {
                --c.respawnStepsLeft;
            }
        }

        for (pickup_flash& flash : m_pickupFlashes) {
            --flash.stepsLeft;
        }
        std::erase_if(m_pickupFlashes, [](const pickup_flash& flash) {
            return flash.stepsLeft == 0;
        });
    }

    void move_ship(const input::input_view inputView) {
        using input::standard_action;

        // A key tapped and released between two steps is up by the time the step reads it, but its
        // press is still there: counting it moves the ship for at least one step.
        const auto held{[this, inputView](const standard_action action) {
            return m_actions.is_active(inputView, action) ||
                   m_actions.was_triggered(inputView, action);
        }};

        Eigen::Vector2d direction{Eigen::Vector2d::Zero()};
        direction.x() += held(standard_action::move_right) ? 1.0 : 0.0;
        direction.x() -= held(standard_action::move_left) ? 1.0 : 0.0;
        // World y points down, like the screen.
        direction.y() += held(standard_action::move_down) ? 1.0 : 0.0;
        direction.y() -= held(standard_action::move_up) ? 1.0 : 0.0;

        m_isThrusting = !direction.isZero();
        if (m_isThrusting) {
            m_heading = direction.normalized();
        }

        if (m_actions.was_triggered(inputView, standard_action::primary) && !is_dashing()) {
            m_dashStepsLeft = DASH_STEPS;
        }

        const double stepSeconds{std::chrono::duration<double>{get_step_duration().get()}.count()};
        Eigen::Vector2d velocity{Eigen::Vector2d::Zero()};
        if (is_dashing()) {
            // A dash always goes somewhere: when no key is held, along the last heading.
            velocity = m_heading * SPEED * DASH_SPEED_FACTOR;
            --m_dashStepsLeft;
        } else if (m_isThrusting) {
            velocity = m_heading * SPEED;
        }

        m_previousShipPosition = m_shipPosition;
        m_shipPosition += velocity * stepSeconds;
        m_shipPosition.x() = std::clamp(m_shipPosition.x(), -BOARD_HALF_WIDTH + SHIP_HALF_SIZE,
                                        BOARD_HALF_WIDTH - SHIP_HALF_SIZE);
        m_shipPosition.y() = std::clamp(m_shipPosition.y(), -BOARD_HALF_HEIGHT + SHIP_HALF_SIZE,
                                        BOARD_HALF_HEIGHT - SHIP_HALF_SIZE);
    }

    // Only the ship's position after the step is checked: even a dash moves it less than a coin's
    // width per step, so it can't skip over one.
    void pick_up_coins() {
        for (coin& c : m_coins) {
            if (!c.is_shown() || (c.position - m_shipPosition).norm() > PICKUP_DISTANCE) {
                continue;
            }

            ++m_coinsCollected;
            m_pickupFlashes.push_back(
                pickup_flash{.position = c.position, .stepsLeft = FLASH_STEPS});
            c.position = free_spot();
            c.respawnStepsLeft = COIN_RESPAWN_STEPS;
        }
    }

    // A random spot on the board, away from the ship and from the other coins, so a coin never
    // respawns right under the player or on top of another. After a few misses any spot will do.
    Eigen::Vector2d free_spot() {
        constexpr double EDGE_MARGIN{48.0};
        constexpr double MIN_DISTANCE_FROM_SHIP{220.0};
        constexpr double MIN_DISTANCE_FROM_COIN{80.0};
        constexpr std::size_t MAX_TRIES{20};

        std::uniform_real_distribution<double> x{-BOARD_HALF_WIDTH + EDGE_MARGIN,
                                                 BOARD_HALF_WIDTH - EDGE_MARGIN};
        std::uniform_real_distribution<double> y{-BOARD_HALF_HEIGHT + EDGE_MARGIN,
                                                 BOARD_HALF_HEIGHT - EDGE_MARGIN};

        Eigen::Vector2d spot{};
        for (std::size_t attempt{0}; attempt < MAX_TRIES; ++attempt) {
            spot = Eigen::Vector2d{x(m_random), y(m_random)};
            const bool farFromCoins{std::ranges::all_of(m_coins, [&spot](const coin& other) {
                return (other.position - spot).norm() >= MIN_DISTANCE_FROM_COIN;
            })};
            if (farFromCoins && (spot - m_shipPosition).norm() >= MIN_DISTANCE_FROM_SHIP) {
                break;
            }
        }
        return spot;
    }

    input::standard_action_map m_actions{};

    Eigen::Vector2d m_shipPosition{0.0, 160.0};
    Eigen::Vector2d m_previousShipPosition{m_shipPosition};
    Eigen::Vector2d m_heading{1.0, 0.0};
    std::size_t m_dashStepsLeft{};
    bool m_isThrusting{};

    std::array<coin, COIN_COUNT> m_coins{};
    std::vector<pickup_flash> m_pickupFlashes{};
    std::size_t m_coinsCollected{};
    std::mt19937 m_random{std::random_device{}()};
};

// ---------------------------------------------------------------------------
// The scene, drawn by two independent scene renderables
// ---------------------------------------------------------------------------

using seconds = std::chrono::duration<double>;

scene::world_rect centred_rect(const math::vector2& centre, const double halfWidth,
                               const double halfHeight) {
    const Eigen::Vector2d halfExtent{halfWidth, halfHeight};
    return scene::world_rect{centre - halfExtent, centre + halfExtent};
}

/**
 * @brief The board and its orbiting planets, drawn with shapes. Knows nothing of the surface it is
 * drawn into: the surface hands it a painter every frame.
 */
class orrery final : public scene::scene_renderable {
public:
    explicit orrery(const seconds& sceneTime) noexcept : m_sceneTime{sceneTime} {}

    // Everything here is in world units: the painter maps it through the surface's camera and
    // clips it to the surface.
    void render(scene::render_context& context) override {
        scene::scene_painter& painter{context.painter()};

        constexpr std::array<theming::color_role, 3> PLANET_ROLES{
            theming::color_role::success, theming::color_role::warning, theming::color_role::error};

        // Translucent, so the grid underneath still shows through the board.
        rendering::color boardColor{theming::color_for(theming::color_role::surface_variant)};
        boardColor.a = 160;
        painter.fill_rect(
            scene::world_rect{
                Eigen::Vector2d{-BOARD_HALF_WIDTH, -BOARD_HALF_HEIGHT},
                Eigen::Vector2d{BOARD_HALF_WIDTH,  BOARD_HALF_HEIGHT }
        },
            boardColor);

        // Orbits are flattened into ellipses so they fit the wide board.
        constexpr double ORBIT_FLATTENING{0.6};
        const math::vector2 sun{0.0, 0.0};
        const double time{m_sceneTime.count()};
        for (std::size_t i{0}; i < PLANET_ROLES.size(); ++i) {
            const double index{static_cast<double>(i)};
            const double orbit{90.0 + 70.0 * index};
            const double angle{time * (0.9 - 0.25 * index) + 2.1 * index};
            const math::vector2 planet{orbit * std::cos(angle),
                                       ORBIT_FLATTENING * orbit * std::sin(angle)};
            const rendering::color planetColor{theming::color_for(PLANET_ROLES[i])};

            painter.draw_line(sun, planet,
                              theming::color_for(theming::color_role::outline_variant));
            // Radii are in world units: planets scale with the zoom.
            painter.fill_circle(planet, 12.0 - 2.0 * index, planetColor);
        }
        painter.fill_circle(sun, 20.0, theming::color_for(theming::color_role::accent));
    }

private:
    const seconds& m_sceneTime;
};

/**
 * @brief The sparkles, the coins and the ship, drawn as sprites from the demo's atlas over the
 * orrery. Reads the game's state, never changes it.
 */
class flight_sprites final : public scene::scene_renderable {
public:
    flight_sprites(const seconds& sceneTime, const flight_game& game,
                   const std::optional<demo_atlas>& sprites) noexcept
        : m_sceneTime{sceneTime},
          m_game{game},
          m_sprites{sprites} {}

    // The atlas is only uploaded on the first update, so there may be nothing to draw with yet.
    void render(scene::render_context& context) override {
        if (m_sprites) {
            draw_sprites(context.painter(), m_sprites->atlas);
        }
    }

private:
    // The sprites are all regions of the one atlas texture, sized in world units like the shapes,
    // so they scale with the camera. The surface samples them nearest by default, which keeps the
    // 16x16 pixel art crisp however far it is scaled up.
    void draw_sprites(scene::scene_painter& painter, const scene::texture_atlas& atlas) const {
        const double time{m_sceneTime.count()};

        // Sparkles on a negative layer draw beneath the orrery's shapes, so the translucent board
        // dims them, even though the orrery is drawn by the renderable before this one.
        constexpr std::array<std::array<double, 2>, 12> SPARKLE_SPOTS{
            {{-540.0, -240.0},
             {-420.0, 40.0},
             {-300.0, -130.0},
             {-230.0, 230.0},
             {-110.0, -250.0},
             {-500.0, 190.0},
             {130.0, 250.0},
             {250.0, -200.0},
             {330.0, 110.0},
             {460.0, -60.0},
             {560.0, 230.0},
             {520.0, -250.0}}
        };
        const scene::sprite_region starSparkle{atlas.region("sparkle")};
        for (std::size_t i{0}; i < SPARKLE_SPOTS.size(); ++i) {
            const double twinkle{0.5 + 0.5 * std::sin(time * 2.0 + 1.7 * static_cast<double>(i))};
            const double halfSize{6.0 + 8.0 * twinkle};
            const math::vector2 centre{SPARKLE_SPOTS[i][0], SPARKLE_SPOTS[i][1]};
            painter.draw_sprite(
                centred_rect(centre, halfSize, halfSize), starSparkle,
                scene::sprite_options{.layer = -1, .tint = theming::color_role::on_surface});
        }

        // Coins spin on the shapes' layer: squashing a sprite's width is all a spin takes. Nothing
        // is drawn at the instant a coin is edge-on, since its width is then zero.
        const scene::sprite_region coin{atlas.region("coin")};
        const std::span<const flight_game::coin> coins{m_game.coins()};
        for (std::size_t i{0}; i < coins.size(); ++i) {
            if (!coins[i].is_shown()) {
                continue;
            }

            const double phase{time * 3.0 + 0.6 * static_cast<double>(i)};
            const math::vector2 centre{coins[i].position.x(),
                                       coins[i].position.y() + 5.0 * std::sin(phase)};
            painter.draw_sprite(
                centred_rect(centre, COIN_HALF_SIZE * std::abs(std::cos(phase)), COIN_HALF_SIZE),
                coin);
        }

        // A burst where a coin was just picked up: a sparkle growing as it fades out of the game,
        // over the ship so it isn't hidden by it. Drawn additively, it glows: it brightens what is
        // under it, and two bursts overlapping add up.
        const scene::sprite_region sparkle{atlas.region("sparkle")};
        painter.set_blend_mode(scene::blend_mode::additive);
        for (const flight_game::pickup_flash& flash : m_game.pickup_flashes()) {
            const double progress{1.0 - static_cast<double>(flash.stepsLeft) /
                                            static_cast<double>(flight_game::FLASH_STEPS)};
            const double halfSize{COIN_HALF_SIZE + 26.0 * progress};
            painter.draw_sprite(
                centred_rect(math::vector2{flash.position}, halfSize, halfSize), sparkle,
                scene::sprite_options{.layer = 2, .tint = theming::color_role::warning});
        }
        painter.set_blend_mode(scene::blend_mode::normal);

        // The ship flies over everything else, on a positive layer. Its flame flickers while it
        // thrusts, faster while it dashes, and is out while it stands still.
        const double flameFrameSeconds{m_game.is_dashing() ? 0.04 : 0.1};
        const bool flameFlickerUp{static_cast<long long>(time / flameFrameSeconds) % 2 == 0};
        const bool flameOn{(m_game.is_thrusting() || m_game.is_dashing()) && flameFlickerUp};
        painter.draw_sprite(
            centred_rect(m_game.drawn_ship_position(), SHIP_HALF_SIZE, SHIP_HALF_SIZE),
            atlas.region(flameOn ? "ship-1" : "ship-0"), scene::sprite_options{.layer = 1});
    }

    const seconds& m_sceneTime;
    const flight_game& m_game;
    const std::optional<demo_atlas>& m_sprites;
};

// ---------------------------------------------------------------------------
// The pixel-art mode: a fixed virtual resolution, scaled by whole numbers
// ---------------------------------------------------------------------------

constexpr scene::virtual_size PIXEL_ART_RESOLUTION{.width = 320, .height = 180};

// The side of a checkerboard square, in virtual pixels.
constexpr std::uint32_t CHECKER_SQUARE{8};

/**
 * @brief A checkerboard of 8x8 squares the size of the virtual screen, built in memory with one
 * texel per virtual pixel: any blur shows as soft edges between the squares, and an uneven column
 * as squares of different widths. Low contrast, so it is easy on the eyes.
 */
rendering::image make_pixel_checkerboard(const rendering::image_factory& factory) {
    constexpr std::byte LIGHT{0x58};
    constexpr std::byte DARK{0x38};

    platform::images::decoded_image checkerboard{.width = PIXEL_ART_RESOLUTION.width,
                                                 .height = PIXEL_ART_RESOLUTION.height};
    checkerboard.pixels.reserve(std::size_t{checkerboard.width} * checkerboard.height * 4);
    for (std::uint32_t y{0}; y < checkerboard.height; ++y) {
        for (std::uint32_t x{0}; x < checkerboard.width; ++x) {
            const std::byte value{(x / CHECKER_SQUARE + y / CHECKER_SQUARE) % 2 == 0 ? LIGHT
                                                                                     : DARK};
            checkerboard.pixels.insert(checkerboard.pixels.end(),
                                       {value, value, value, std::byte{0xFF}});
        }
    }
    return factory.create_from_pixels(std::move(checkerboard));
}

/**
 * @brief What the pixel-art mode shows: the checkerboard covering the virtual screen, and a coin
 * at its native 16x16 drifting over it. The coin moves by fractions of a pixel, and the surface
 * snaps it to whole virtual pixels, so it never lands between the checkerboard's.
 */
class pixel_art_scene final : public scene::scene_renderable {
public:
    pixel_art_scene(const seconds& sceneTime, const std::optional<rendering::image>& checkerboard,
                    const std::optional<demo_atlas>& sprites) noexcept
        : m_sceneTime{sceneTime},
          m_checkerboard{checkerboard},
          m_sprites{sprites} {}

    // One world unit is one virtual pixel: the virtual screen runs from (0, 0) to (320, 180).
    void render(scene::render_context& context) override {
        scene::scene_painter& painter{context.painter()};
        if (m_checkerboard) {
            painter.draw_sprite(
                scene::world_rect{
                    Eigen::Vector2d::Zero(),
                    Eigen::Vector2d{PIXEL_ART_RESOLUTION.width, PIXEL_ART_RESOLUTION.height}
            },
                m_checkerboard->as_sprite_texture());
        }
        if (m_sprites) {
            const double time{m_sceneTime.count()};
            const math::vector2 centre{160.0 + 120.0 * std::sin(time * 0.7),
                                       90.0 + 50.0 * std::sin(time * 1.3)};
            painter.draw_sprite(centred_rect(centre, 8.0, 8.0), m_sprites->atlas.region("coin"));
        }
    }

private:
    const seconds& m_sceneTime;
    const std::optional<rendering::image>& m_checkerboard;
    const std::optional<demo_atlas>& m_sprites;
};

// ---------------------------------------------------------------------------
// The demo's application logic
// ---------------------------------------------------------------------------

/**
 * @brief A small playable scene: a ship the player flies over a board of orbiting planets and
 * twinkling sparkles, picking up spinning coins, drawn on a scene surface with sprites from a
 * texture atlas, plus a coin counter and the FPS overlay. Escape opens a pause menu that keeps
 * every input from the game until it is closed again.
 */
class demo_logic final : public application::game_application_logic, public rendering::updatable {
public:
    explicit demo_logic(std::filesystem::path assetsFolder)
        : m_assetsFolder{std::move(assetsFolder)} {
        build_widgets();
    }

private:
    static constexpr double GRID_SPACING{32.0};
    static constexpr double BOARD_MARGIN{24.0};
    static constexpr std::string_view CONTROLS{
        "Arrow keys or WASD to fly, Space or E to dash, Escape to pause, P for pixel-art mode, "
        "F11 for fullscreen."};
    static constexpr std::string_view PAUSED{"Paused - press Escape to resume."};

    // The demo's own per-frame logic: the pause menu, animation time, the counter, and keeping the
    // board in view.
    void update(rendering::update_context& context) override {
        load_sprites_once();
        update_pause_menu(context.input());
        update_display_keys(context.input());
        if (!m_isPaused) {
            m_sceneTime += std::chrono::duration_cast<seconds>(context.delta());
        }
        m_coinCounter.set_text(std::format("Coins: {}", m_game.coins_collected()));
        fit_board_in_view();
    }

    // The demo is registered before the game, so it sees input first: while the menu is open it
    // consumes everything, and the game, which keeps stepping, sees no input at all. The key that
    // closes the menu is consumed too, and keys held through the menu read as up until pressed
    // again, so nothing pressed in the menu leaks into play.
    void update_pause_menu(const input::input_view inputView) {
        if (m_actions.was_triggered(inputView, input::standard_action::pause)) {
            m_isPaused = !m_isPaused;
            m_controls.set_text(m_isPaused ? PAUSED : CONTROLS);
            m_actions.consume(inputView, input::standard_action::pause);
        }

        if (m_isPaused) {
            inputView.consume_all();
        }
    }

    // P swaps the board for the pixel-art mode and back; F11 goes fullscreen and back, so every
    // whole-number scale the screen allows can be checked.
    void update_display_keys(const input::input_view inputView) {
        if (inputView.keyboard().was_pressed(input::key::p)) {
            set_pixel_art_mode(!m_isPixelArtMode);
        }
        if (inputView.keyboard().was_pressed(input::key::f11)) {
            application::application_controller& controller{get_application_controller()};
            if (controller.is_fullscreen()) {
                controller.go_windowed();
            } else {
                controller.go_fullscreen();
            }
        }
    }

    // The board and the pixel-art scene take turns on the one surface: only the pixel-art mode
    // presents a virtual resolution, and it needs no grid.
    void set_pixel_art_mode(const bool enabled) {
        m_isPixelArtMode = enabled;
        if (enabled) {
            m_scene.remove_renderable(m_orrery);
            m_scene.remove_renderable(m_flightSprites);
            m_scene.add_renderable(m_pixelArt);
            m_scene.set_grid(std::nullopt);
            m_scene.set_virtual_resolution(PIXEL_ART_RESOLUTION);
        } else {
            m_scene.remove_renderable(m_pixelArt);
            m_scene.add_renderable(m_orrery);
            m_scene.add_renderable(m_flightSprites);
            m_scene.set_grid(scene::grid_options{.spacing = GRID_SPACING});
            m_scene.set_virtual_resolution(std::nullopt);
        }
    }

    void build_widgets() {
        m_root.add_renderable(m_title);
        m_statusLine.add_renderable(m_controls);
        m_statusLine.add_renderable(m_coinCounter);
        m_root.add_renderable(m_statusLine);

        // Sized zero both ways, the surface takes the rest of the window.
        m_scene.set_grid(scene::grid_options{.spacing = GRID_SPACING});
        // Drawn in the order they are added, both through the one painter of each frame, so the
        // sprites' layers also order them against the orrery's shapes.
        m_scene.add_renderable(m_orrery);
        m_scene.add_renderable(m_flightSprites);
        m_root.add_renderable(m_scene);

        // The engine's game loop drives everything registered here, updating all of it before
        // rendering any of it. The counter shows the coins collected up to the previous frame: the
        // game steps after the demo's update. The two top-level widgets are the viewport filling
        // the window and the FPS overlay, which owns a window of its own and therefore sits next to
        // the viewport rather than inside it.
        get_updatables().add_updatable(*this);
        get_updatables().add_updatable(m_game);
        get_updatables().add_updatable(m_fpsOverlay);
        get_renderables().add_renderable(m_root);
        get_renderables().add_renderable(m_fpsOverlay);
    }

    // The image factory is only handed over once the renderer is up, i.e. after the constructor
    // but before the first frame, so the atlas is uploaded on the first update.
    void load_sprites_once() {
        if (!m_sprites) {
            m_sprites.emplace(load_demo_atlas(get_image_factory(), m_assetsFolder));
        }
        if (!m_checkerboard) {
            m_checkerboard.emplace(make_pixel_checkerboard(get_image_factory()));
        }
    }

    // The surface's size is only known once it has been laid out, so the zoom follows it every
    // frame: the whole board stays on screen however the window is resized. The pixel-art mode
    // needs none of that: the surface picks the whole-number scale, and the camera only has to
    // show the virtual screen at zoom 1.
    void fit_board_in_view() {
        scene::camera_2d& camera{m_scene.camera()};
        if (m_isPixelArtMode) {
            camera.set_position(
                math::vector2{PIXEL_ART_RESOLUTION.width / 2.0, PIXEL_ART_RESOLUTION.height / 2.0});
            camera.set_zoom(1.0);
            return;
        }

        const math::vector2& viewport{camera.viewport_size()};
        if (viewport.x() <= 0.0 || viewport.y() <= 0.0) {
            return;
        }

        const double zoom{std::min(viewport.x() / (2.0 * (BOARD_HALF_WIDTH + BOARD_MARGIN)),
                                   viewport.y() / (2.0 * (BOARD_HALF_HEIGHT + BOARD_MARGIN)))};
        camera.set_position(math::vector2::zero());
        camera.set_zoom(scene::camera_2d::clamp_zoom(zoom));
    }

    widgets::viewport_layout_widget m_root{"tca-engine-demo",
                                           theming::color_role::window_background};

    widgets::label m_title{"TCA Engine demo", theming::typography_role::heading,
                           theming::color_role::on_surface};
    widgets::same_line_layout m_statusLine{};
    widgets::label m_controls{CONTROLS, theming::typography_role::body,
                              theming::color_role::on_surface_muted};
    widgets::label m_coinCounter{"", theming::typography_role::body, theming::color_role::warning};

    input::standard_action_map m_actions{};
    bool m_isPaused{};

    seconds m_sceneTime{seconds::zero()};
    scene::scene_surface m_scene{"##demo-scene"};
    std::filesystem::path m_assetsFolder{};
    std::optional<demo_atlas> m_sprites{};
    flight_game m_game{};

    orrery m_orrery{m_sceneTime};
    flight_sprites m_flightSprites{m_sceneTime, m_game, m_sprites};

    bool m_isPixelArtMode{};
    std::optional<rendering::image> m_checkerboard{};
    pixel_art_scene m_pixelArt{m_sceneTime, m_checkerboard, m_sprites};

    widgets::fps_overlay m_fpsOverlay{
        widgets::anchor_position::bottom_right, math::vector2{12.0, 12.0}
    };
};

} // namespace

int main() {
    try {
        auto app{application::application_factory::create_platform_preferred_application()};

        app->set_app_name("TCA Engine demo")
            .set_window_display_settings(application::window_display_settings{
                .content_size = {1280, 800}
        })
            .set_theme(theming::default_dark_theme())
            // The build copies the assets folder next to the executable, so it is found from there
            // whatever the working directory is (the VS Code launch configuration runs the demo
            // from the repository root, for instance).
            .set_logic(std::make_unique<demo_logic>(platform::get_asset_path("assets")));

#ifdef _WIN32
        app->run();
#else
        // Only Windows has a GUI application backend today: everywhere else the factory falls back
        // to a console application, which has no surface for these widgets to draw on. The demo is
        // still built there - it is the cheapest check that the engine's public API stays usable
        // from outside the engine - it just has nothing to show.
        std::cout << "This platform has no GUI application backend yet, so the demo has nothing "
                     "to display. Build and run it on Windows to see it.\n";
#endif // _WIN32
    } catch (const std::exception& exception) {
        std::cerr << "The demo application failed: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
