// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef> // for std::byte
#include <filesystem>
#include <memory>
#include <vector>

// Engine
#include "core/utilities/ranged-value.hpp"

namespace tca::engine::sounds {

class sound_impl;

/**
 * @brief Simple data buffer type representing the data of a sound source, which can be used to
 * create a sound from a memory buffer instead of a file.
 */
using sound_source_data = std::vector<std::byte>;

/**
 * @brief Volume level for playing a sound, in the range [0.0, 1.0], where 0.0 means silence and
 * 1.0 means full volume. Constructing it from a value outside that range throws a
 * core::ranged_value_exception (a std::out_of_range).
 */
using volume_type = core::ranged_value<double, 0.0, 1.0>;

/**
 * @brief How a sound is played by sound::play().
 */
struct play_params {
    /**
     * @brief The volume to play the sound at (default is full volume).
     */
    volume_type volume{1.0};

    /**
     * @brief Whether the sound starts over every time it reaches its end, until it is stopped.
     */
    bool looping{false};
};

/**
 * @brief Class representing a sound that can be played in the game. This class owns the resources
 * associated with the sound, and ensures that they are properly released when the sound is
 * destroyed.
 *
 * @note A sound object is exactly one voice: it plays at most once at a time. To play the same
 * effect several times at once, load several sound objects from it and play them in turn.
 */
class sound {
public:
    using volume_type = sounds::volume_type;

    sound(const sound&) = delete;
    sound(sound&&) noexcept;
    sound& operator=(const sound&) = delete;

    /**
     * @brief Takes over the other sound. The sound this one held until now is stopped and
     * released.
     */
    sound& operator=(sound&&) noexcept;

    /**
     * @brief RAII: loads the sound from the specified file or throws an exception if the sound
     * fails to load.
     *
     * @param filePath The path to the sound file to load.
     */
    explicit sound(std::filesystem::path filePath);

    /**
     * @brief Construct a new sound object from a memory buffer containing the sound source data.
     * The sound source data should be in a format that can be loaded by the sound system (e.g., a
     * .wav file format). The function will throw an exception if the sound fails to load from the
     * provided data buffer.
     *
     * @param data The buffer containing the sound source data to load the sound from. The data
     * should be in a format that can be loaded by the sound system (e.g., a .wav file format).
     */
    explicit sound(sound_source_data&& data);

    /**
     * @brief Releases the sound resources.
     */
    ~sound() noexcept;

    /**
     * @brief Plays the sound from the beginning with the given volume and looping. If the sound is
     * already playing, it is restarted from the beginning with the new parameters.
     *
     * @note This will play a sound asynchronously, meaning that the function will return
     * immediately after starting the sound playback, and the sound will continue to play in the
     * background.
     *
     * @param params The volume and looping to play the sound with.
     */
    void play(play_params params = {}) const;

    /**
     * @brief Plays the sound once from the beginning with the specified volume (default is full
     * volume) and blocks until the sound finishes playing.
     *
     * @param volume The volume to play the sound at.
     */
    void play_synchronously(volume_type volume = volume_type{1.0}) const;

    /**
     * @brief Stops the sound immediately. Does nothing if the sound isn't playing. The next play()
     * starts it again from the beginning.
     */
    void stop() const noexcept;

    /**
     * @brief Whether the sound is currently playing: true from play() until it reaches its end
     * (never, for a looping sound) or is stopped.
     */
    [[nodiscard]] bool is_playing() const noexcept;

private:
    std::unique_ptr<sound_impl> m_impl{};

    void initialize_sound_from_file(const std::filesystem::path& filePath);
    void initialize_sound_from_data();
};

/**
 * @brief Stops every sound that is currently alive, e.g. when the game is paused or changes scene.
 */
void stop_all() noexcept;

} // namespace tca::engine::sounds
