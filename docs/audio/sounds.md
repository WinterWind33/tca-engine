# Sounds

`sounds::sound` (`sounds/sounds.hpp`) is a sound loaded from a file or from a memory buffer, played
on [miniaudio](https://miniaud.io). One `sound` object is **one voice**: it plays at most once at a
time.

```cpp
#include "platform/folders.hpp"
#include "sounds/sounds.hpp"

using namespace tca::engine;

const sounds::sound music{platform::get_asset_path("assets/audio/level-1.wav")};
const sounds::sound jump{platform::get_asset_path("assets/audio/jump.wav")};

music.play({.volume = 0.4, .looping = true}); // loops until stopped
jump.play();                                  // full volume, once

if (music.is_playing()) {
    music.stop();
}

sounds::stop_all(); // e.g. on pause or when the scene changes
```

## Rules

- `play()` always starts from the beginning. On a sound that is already playing it **restarts** it
  with the new `play_params`.
- `play_params::volume` is a `sounds::volume_type`, a `core::ranged_value` in `[0.0, 1.0]`. A value
  outside it throws `core::ranged_value_exception` (a `std::out_of_range`) when the params are built.
- A looping sound plays until `stop()` or `sounds::stop_all()`. A sound that isn't looping stops on
  its own at its end. `is_playing()` tells which state it is in.
- `play_synchronously(volume)` plays the sound once (never looping) and blocks until it ends or is
  stopped.
- `sounds::stop_all()` stops every `sound` that is alive, from any thread.

## Playing the same effect several times at once

Since a `sound` is one voice, an effect that overlaps itself (a machine gun, several enemies dying
in the same frame) is loaded into N `sound` objects, played in turn:

```cpp
class gunshot_player {
public:
    explicit gunshot_player(const std::filesystem::path& file) {
        for (std::size_t i{}; i < 8; ++i) {
            m_voices.emplace_back(file);
        }
    }

    void fire() {
        // When every voice is busy, this restarts the one that started first.
        m_voices[m_next].play();
        m_next = (m_next + 1) % m_voices.size();
    }

private:
    std::vector<sounds::sound> m_voices{};
    std::size_t m_next{};
};
```

This is cheap for sounds loaded from a file: miniaudio caches the file's data by path, so the N
objects share one copy and each only adds its own decoder. Sounds built from a memory buffer each
keep their own copy of the buffer. Pick N from the sample length divided by how often the effect
fires.
