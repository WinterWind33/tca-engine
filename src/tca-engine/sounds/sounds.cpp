// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "sounds/sounds.hpp"

// C++ STL
#include <chrono>
#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

// Third-party
#include <miniaudio.h>
#include <spdlog/spdlog.h>
#include <gsl/gsl>

// Engine
#include "core/logging/logging.hpp"

namespace tca::engine::sounds {

namespace {

std::shared_ptr<spdlog::logger> get_audio_logger() {
    static std::shared_ptr<spdlog::logger> logger{core::logging::get_logger("Audio Engine")};
    return logger;
}

/**
 * @brief RAII class which will initialize the audio engine on construction and uninitialize it on
 * destruction.
 */
class audio_engine_wrapper final {
public:
    audio_engine_wrapper() {
        if (const ma_result result{ma_engine_init(nullptr, &m_engine)}; result != MA_SUCCESS) {
            get_audio_logger()->critical("Failed to initialize the audio engine: error code {}",
                                         static_cast<std::int32_t>(result));
            throw std::runtime_error{"Failed to initialize the audio engine"};
        }

        get_audio_logger()->info("Audio engine initialized successfully");
    }
    ~audio_engine_wrapper() noexcept {
        ma_engine_uninit(&m_engine);
        get_audio_logger()->info("Audio engine uninitialized successfully");
    }
    audio_engine_wrapper(const audio_engine_wrapper&) = delete;
    audio_engine_wrapper(audio_engine_wrapper&&) = delete;
    audio_engine_wrapper& operator=(const audio_engine_wrapper&) = delete;
    audio_engine_wrapper& operator=(audio_engine_wrapper&&) = delete;

    ma_engine& get_engine() noexcept {
        return m_engine;
    }

private:
    ma_engine m_engine{};
};

/**
 * @brief Retrieves the main audio engine instance, which is initialized on the first call to this
 * function and then returned by reference on subsequent calls.
 *
 * @note This may throw an exception if the audio engine fails to initialize.
 *
 * @return ma_engine& A reference to the main audio engine instance.
 */
ma_engine& get_audio_engine() {
    static audio_engine_wrapper engineWrapper{};
    return engineWrapper.get_engine();
}

/**
 * @brief The sounds that are currently alive, so that stop_all() can reach every one of them.
 * Guarded by a mutex since sounds may be created, destroyed and stopped from different threads.
 */
class sound_registry final {
public:
    void add(ma_sound& sound) {
        const std::scoped_lock lock{m_mutex};
        m_sounds.push_back(&sound);
    }

    void remove(const ma_sound& sound) noexcept {
        const std::scoped_lock lock{m_mutex};
        std::erase(m_sounds, &sound);
    }

    void stop_all() noexcept {
        const std::scoped_lock lock{m_mutex};
        for (ma_sound* const sound : m_sounds) {
            ma_sound_stop(sound);
        }
    }

private:
    std::mutex m_mutex{};
    std::vector<ma_sound*> m_sounds{};
};

/**
 * @brief Retrieves the registry of live sounds. Every sound registers itself while it is being
 * constructed, so the registry finishes construction first and outlives every sound, static ones
 * included.
 */
sound_registry& get_sound_registry() {
    static sound_registry registry{};
    return registry;
}
} // namespace

class sound_impl final {
public:
    sound_impl() = default;
    sound_impl(const sound_impl&) = delete;
    sound_impl(sound_impl&&) = delete;
    sound_impl& operator=(const sound_impl&) = delete;
    sound_impl& operator=(sound_impl&&) = delete;

    // Releases whatever was initialized, so that both a sound whose loading failed half-way and a
    // sound replaced by a move assignment clean up after themselves.
    ~sound_impl() noexcept {
        if (registered) {
            get_sound_registry().remove(sound_handle);
        }

        if (sound_initialized) {
            ma_sound_uninit(&sound_handle);
        }

        if (decoder_handle.has_value()) {
            ma_decoder_uninit(&decoder_handle.value());
        }
    }

    ma_sound sound_handle{};
    bool sound_initialized{false};
    bool registered{false};
    std::optional<ma_decoder> decoder_handle{};

    // The encoded data a sound created from memory is decoded from. The decoder reads from it for
    // as long as the sound lives.
    sound_source_data data_buffer{};
};

sound::sound(std::filesystem::path filePath) : m_impl{std::make_unique<sound_impl>()} {
    initialize_sound_from_file(filePath);
}

sound::sound(sound_source_data&& data) : m_impl{std::make_unique<sound_impl>()} {
    m_impl->data_buffer = std::move(data);
    initialize_sound_from_data();
}

// Defined here, where sound_impl is a complete type.
sound::sound(sound&&) noexcept = default;
sound& sound::operator=(sound&&) noexcept = default;
sound::~sound() noexcept = default;

void sound::initialize_sound_from_file(const std::filesystem::path& filePath) {
    ma_engine& engine{get_audio_engine()};

    if (const ma_result result{ma_sound_init_from_file(&engine, filePath.string().c_str(),
                                                       0,       // flags
                                                       nullptr, // sound group
                                                       nullptr, // done fence,
                                                       &m_impl->sound_handle)};
        result != MA_SUCCESS) {
        get_audio_logger()->error("Failed to load sound from file {}: error code {}",
                                  filePath.string(), static_cast<std::int32_t>(result));
        throw std::runtime_error{
            std::format("Failed to load sound from file {}", filePath.string())};
    }
    m_impl->sound_initialized = true;

    get_sound_registry().add(m_impl->sound_handle);
    m_impl->registered = true;

    get_audio_logger()->info("Loaded sound: {}", filePath.string());
}

void sound::initialize_sound_from_data() {
    ma_engine& engine{get_audio_engine()};

    // First we need to decode the sound data from the provided memory buffer and create a sound
    // from it. If anything below throws, sound_impl's destructor releases what was initialized.
    m_impl->decoder_handle = ma_decoder{};
    if (const ma_result result{ma_decoder_init_memory(m_impl->data_buffer.data(),
                                                      m_impl->data_buffer.size(), nullptr,
                                                      &m_impl->decoder_handle.value())};
        result != MA_SUCCESS) {
        // The decoder failed to initialize, so there is nothing to uninitialize.
        m_impl->decoder_handle.reset();

        get_audio_logger()->error("Failed to decode sound from memory buffer: error code {}",
                                  static_cast<std::int32_t>(result));
        throw std::runtime_error{"Failed to decode sound from memory buffer"};
    }

    if (const ma_result result{
            ma_sound_init_from_data_source(&engine, &m_impl->decoder_handle.value(), 0, // flags
                                           nullptr, // sound group
                                           &m_impl->sound_handle)};
        result != MA_SUCCESS) {
        get_audio_logger()->error("Failed to load sound from memory buffer: error code {}",
                                  static_cast<std::int32_t>(result));
        throw std::runtime_error{"Failed to load sound from memory buffer"};
    }
    m_impl->sound_initialized = true;

    get_sound_registry().add(m_impl->sound_handle);
    m_impl->registered = true;

    get_audio_logger()->info("Loaded sound from memory buffer (size: {} bytes)",
                             m_impl->data_buffer.size());
}

void sound::play(const play_params params) const {
    ma_sound& soundHandle{m_impl->sound_handle};

    ma_sound_set_volume(&soundHandle, gsl::narrow_cast<float>(params.volume.get()));
    ma_sound_set_looping(&soundHandle, params.looping ? MA_TRUE : MA_FALSE);

    // Rewind first, so that play() always starts from the beginning: whether the sound is still
    // playing, was stopped half-way or reached its end. The mixing thread applies the seek before
    // it reads the next frames of this sound.
    if (const ma_result result{ma_sound_seek_to_pcm_frame(&soundHandle, 0)}; result != MA_SUCCESS) {
        get_audio_logger()->error("Failed to rewind sound: error code {}",
                                  static_cast<std::int32_t>(result));
        throw std::runtime_error{"Failed to rewind sound"};
    }

    // Then, play it. This does nothing if the sound is already playing, in which case the seek
    // above is what restarts it.
    if (const ma_result result{ma_sound_start(&soundHandle)}; result != MA_SUCCESS) {
        get_audio_logger()->error("Failed to play sound: error code {}",
                                  static_cast<std::int32_t>(result));
        throw std::runtime_error{"Failed to play sound"};
    }
}

void sound::play_synchronously(const volume_type volume) const {
    play({.volume = volume, .looping = false});

    // Wait until the sound finishes playing (or is stopped). We can do this by polling the sound
    // state until it indicates that it's no longer playing.
    while (is_playing()) {
        // Sleep for a short duration to avoid busy-waiting. The actual value here can
        // be tuned as needed.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void sound::stop() const noexcept {
    if (!m_impl) {
        // Can be the case of moved sound object.
        return;
    }

    ma_sound_stop(&m_impl->sound_handle);
}

bool sound::is_playing() const noexcept {
    if (!m_impl) {
        // Can be the case of moved sound object.
        return false;
    }

    return ma_sound_is_playing(&m_impl->sound_handle) == MA_TRUE;
}

void stop_all() noexcept {
    get_sound_registry().stop_all();
}

} // namespace tca::engine::sounds
