// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "sounds/sounds.hpp"

// C++ STL
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <thread>
#include <utility>

// Third-party
#include <catch2/catch_test_macros.hpp>

#include "utilities/test-assets.hpp"

namespace {

// test-sound.wav lasts about 0.19 seconds: waiting this long is enough for it to end several
// times over, while a looping sound must still be playing afterwards.
constexpr std::chrono::milliseconds SEVERAL_TIMES_THE_TEST_SOUND_LENGTH{600};

// How long a sound that was told to end may take to do so before a test gives up on it.
constexpr std::chrono::seconds END_TIMEOUT{5};

// Polls the sound until it stops playing or END_TIMEOUT elapses. Returns whether it stopped.
bool wait_until_not_playing(const tca::engine::sounds::sound& sound) {
    const auto deadline{std::chrono::steady_clock::now() + END_TIMEOUT};
    while (sound.is_playing()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

tca::engine::sounds::sound_source_data read_file(const std::filesystem::path& filePath) {
    std::ifstream file{filePath, std::ios::binary};
    REQUIRE(file.is_open());

    const auto fileSize{std::filesystem::file_size(filePath)};
    tca::engine::sounds::sound_source_data data(fileSize);
    REQUIRE(
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fileSize)));
    return data;
}

} // namespace

TEST_CASE("sound class tests", "[tca][engine][sounds][sound]") {
    using namespace tca::engine;

    const std::filesystem::path testAssetsDir{tca::tests::utilities::TEST_ENGINE_ASSETS_REL_DIR};
    const std::filesystem::path validSoundFilePath{testAssetsDir / "test-sound.wav"};

    SECTION("Loading a sound from a valid file path should succeed") {
        REQUIRE_NOTHROW(sounds::sound{validSoundFilePath});
    }

    SECTION("Loading a sound from an invalid file path should throw an exception") {
        const std::filesystem::path invalidSoundFilePath{testAssetsDir / "nonexistent-sound.wav"};
        REQUIRE_THROWS_AS(sounds::sound{invalidSoundFilePath}, std::runtime_error);
    }

    SECTION("Loading a sound from a valid memory buffer should succeed") {
        REQUIRE_NOTHROW(sounds::sound{read_file(validSoundFilePath)});
    }

    SECTION(
        "Loading a sound from an invalid memory buffer should throw an exception, without "
        "leaking the underlying decoder") {
        // Not a valid audio format: decoding this buffer should fail immediately.
        sounds::sound_source_data invalidData(64, std::byte{0xFF});

        REQUIRE_THROWS_AS(sounds::sound{std::move(invalidData)}, std::runtime_error);
    }

    GIVEN("A valid sound") {
        const sounds::sound validSound{validSoundFilePath};

        SECTION("It should not be playing before it is played") {
            REQUIRE_FALSE(validSound.is_playing());
        }

        SECTION("Playing the sound with a valid volume should succeed") {
            REQUIRE_NOTHROW(validSound.play({.volume = 0.5}));
            REQUIRE(validSound.is_playing());
        }

        SECTION("Playing the sound with a valid volume synchronously should succeed") {
            REQUIRE_NOTHROW(validSound.play_synchronously(0.5));
            REQUIRE_FALSE(validSound.is_playing());
        }

        SECTION("Playing the sound with an invalid volume should throw an exception") {
            REQUIRE_THROWS_AS(validSound.play({.volume = -0.1}), std::out_of_range);
            REQUIRE_THROWS_AS(validSound.play({.volume = 1.1}), std::out_of_range);
            REQUIRE_THROWS_AS(validSound.play_synchronously(1.1), std::out_of_range);
        }

        SECTION("A sound that isn't looping should stop playing once it reaches its end") {
            validSound.play();

            REQUIRE(wait_until_not_playing(validSound));
        }

        SECTION("A looping sound should keep playing past its end, until it is stopped") {
            validSound.play({.looping = true});
            std::this_thread::sleep_for(SEVERAL_TIMES_THE_TEST_SOUND_LENGTH);

            REQUIRE(validSound.is_playing());

            validSound.stop();
            REQUIRE_FALSE(validSound.is_playing());
        }

        SECTION("Stopping a sound that isn't playing should do nothing") {
            REQUIRE_NOTHROW(validSound.stop());
            REQUIRE_FALSE(validSound.is_playing());
        }

        SECTION("A stopped sound should play again when played") {
            validSound.play({.looping = true});
            validSound.stop();

            validSound.play();
            REQUIRE(validSound.is_playing());
        }

        SECTION("A sound should play again once it has reached its end") {
            validSound.play();
            REQUIRE(wait_until_not_playing(validSound));

            validSound.play();
            REQUIRE(validSound.is_playing());
        }

        SECTION(
            "Playing a sound that is already playing should restart it with the new "
            "parameters") {
            validSound.play({.looping = true});
            REQUIRE(validSound.is_playing());

            // Restarted without looping, it must now reach its end and stop on its own.
            validSound.play({.looping = false});
            REQUIRE(validSound.is_playing());
            REQUIRE(wait_until_not_playing(validSound));
        }

        SECTION("Playing a looping sound synchronously should play it once and return") {
            validSound.play({.looping = true});

            REQUIRE_NOTHROW(validSound.play_synchronously());
            REQUIRE_FALSE(validSound.is_playing());
        }
    }
}

TEST_CASE("sound move tests", "[tca][engine][sounds][sound]") {
    using namespace tca::engine;

    const std::filesystem::path validSoundFilePath{
        std::filesystem::path{tca::tests::utilities::TEST_ENGINE_ASSETS_REL_DIR} /
        "test-sound.wav"};

    SECTION("A moved-to sound should keep playing and stopping as the original did") {
        sounds::sound original{validSoundFilePath};
        original.play({.looping = true});

        const sounds::sound movedTo{std::move(original)};
        REQUIRE(movedTo.is_playing());

        movedTo.stop();
        REQUIRE_FALSE(movedTo.is_playing());
    }

    SECTION("A moved-from sound should report it isn't playing and ignore stop()") {
        sounds::sound original{validSoundFilePath};
        original.play({.looping = true});
        const sounds::sound movedTo{std::move(original)};

        REQUIRE_FALSE(original.is_playing()); // NOLINT(*-use-after-move)
        REQUIRE_NOTHROW(original.stop());     // NOLINT(*-use-after-move)

        REQUIRE(movedTo.is_playing());
        movedTo.stop();
    }

    SECTION("Move-assigning over a playing sound should release it and play the new one") {
        sounds::sound target{validSoundFilePath};
        target.play({.looping = true});

        target = sounds::sound{read_file(validSoundFilePath)};
        REQUIRE_FALSE(target.is_playing());

        target.play();
        REQUIRE(target.is_playing());
        REQUIRE(wait_until_not_playing(target));
    }
}

TEST_CASE("sounds::stop_all() tests", "[tca][engine][sounds][stop_all]") {
    using namespace tca::engine;

    const std::filesystem::path validSoundFilePath{
        std::filesystem::path{tca::tests::utilities::TEST_ENGINE_ASSETS_REL_DIR} /
        "test-sound.wav"};

    SECTION("It should stop every playing sound, loaded from files and from memory alike") {
        const sounds::sound fromFile{validSoundFilePath};
        const sounds::sound fromMemory{read_file(validSoundFilePath)};
        const sounds::sound notPlayed{validSoundFilePath};

        fromFile.play({.looping = true});
        fromMemory.play({.looping = true});
        REQUIRE(fromFile.is_playing());
        REQUIRE(fromMemory.is_playing());

        sounds::stop_all();

        REQUIRE_FALSE(fromFile.is_playing());
        REQUIRE_FALSE(fromMemory.is_playing());
        REQUIRE_FALSE(notPlayed.is_playing());
    }

    SECTION("It should leave destroyed and moved-from sounds alone") {
        {
            const sounds::sound destroyed{validSoundFilePath};
            destroyed.play({.looping = true});
        }

        sounds::sound original{validSoundFilePath};
        original.play({.looping = true});
        const sounds::sound movedTo{std::move(original)};

        REQUIRE_NOTHROW(sounds::stop_all());
        REQUIRE_FALSE(movedTo.is_playing());
    }

    SECTION("Stopped sounds should play again when played") {
        const sounds::sound sound{validSoundFilePath};
        sound.play({.looping = true});
        sounds::stop_all();

        sound.play();
        REQUIRE(sound.is_playing());
        REQUIRE(wait_until_not_playing(sound));
    }

    SECTION("It should do nothing when no sound is playing") {
        REQUIRE_NOTHROW(sounds::stop_all());
    }
}
