# Settings

The settings module persists and restores a game's user settings, so a game describes *what* its
settings are and the engine takes care of storing them. It is built when
`TCA_ENGINE_BUILD_SETTINGS_MODULE` is `ON` (the default) and lives behind the CMake target
`tca-engine-settings`, which the aggregate `tca-engine` target links for you.

## The short version

```cpp
#include "settings/settings-io.hpp"
#include "settings/settings-json-formatter.hpp"
#include "settings/settings-location.hpp"

namespace settings = tca::engine::settings;

struct audio_settings {
    tca::engine::core::ranged_value<int, 0, 100> master_volume{};
    bool muted{};
};

struct game_settings {
    audio_settings audio{};
    std::string player_name{};
};

// The one thing a game has to write: which fields there are, what they are called in the file,
// and what they are worth before the player has ever touched them.
template <>
struct tca::engine::settings::settings_schema<audio_settings> {
    static constexpr auto fields{std::make_tuple(
        field{.name = "master_volume", .binding = &audio_settings::master_volume,
              .default_value = decltype(audio_settings::master_volume){80}},
        field{.name = "muted", .binding = &audio_settings::muted, .default_value = false})};
};

template <>
struct tca::engine::settings::settings_schema<game_settings> {
    static constexpr auto fields{std::make_tuple(
        field{.name = "audio", .binding = &game_settings::audio},
        field{.name = "player_name", .binding = &game_settings::player_name})};
};

using game_settings_store =
    settings::settings_store<game_settings, settings::formatters::settings_json_formatter>;

int main() {
    game_settings_store store{settings::get_settings_file_path("The Clinical Aspect")};

    store.settings().audio.master_volume = 55;
    // ...and that reaches the file when the store is saved, which the destructor does.
}
```

Nothing else is needed: no serialization code, no path, no failure handling around startup.

## Declaring a schema

A schema is a specialization of `settings_schema<T>` holding a `static constexpr auto fields`
tuple of `field` objects. Each field carries:

- `name` — the key it is stored under. This is the part of the file that has to stay stable
  between versions; the C++ member can be renamed freely.
- `binding` — a pointer to the member it fills.
- `default_value` — optional. A field without one is left value-initialized, which is what string
  fields have to do: a `std::string` cannot live in a `constexpr` schema.

A field whose type has a schema of its own is a nested group: it becomes a nested object in the
file, so `audio.volume` and `music.volume` do not collide. Nesting is unlimited.

## Where the file goes

`get_settings_file_path(appName)` (from `settings/settings-location.hpp`) answers with
`settings.json` inside the folder the platform keeps user data in — `%AppData%\<app>` on Windows,
`~/.config/<app>` on Linux, `~/Library/Application Support/<app>` on macOS. Pass a second argument
to name the file yourself, which is what a game with per-profile settings wants. Nothing needs to
exist beforehand: saving creates the folders leading to the file.

A game that already knows where it wants the file can hand `settings_store` any path instead.

## Value types the formatter does not know

`settings_json_formatter` stores `bool`, the integral and floating-point types, `std::string`, and
`std::vector<std::string>` (as a JSON array of strings) directly. Any other type reaches the file through a `settings_value_traits` specialization saying
which of those stores it:

```cpp
template <>
struct tca::engine::settings::settings_value_traits<my_type> {
    using storage_type = std::string;

    static storage_type to_storage(const my_type& value);
    static my_type from_storage(storage_type value);  // may throw: see below
};
```

`core::ranged_value` already has one, storing as its underlying arithmetic type. A
`from_storage()` that throws is how a type refuses a value: the field is then reported as
unreadable and keeps its schema default, which is what keeps an out-of-range value in the file
from ever reaching game code.

A `std::vector<std::string>` field reads back only from an array whose every element is a string:
one element of any other type makes the whole field unreadable, rather than a shorter list. An
empty vector is written as `[]` and reads back as an empty list, not as a missing field.

## Groups: entries named at run time

Some values are a set of named entries that no `settings_schema` describes — an input action map
is one, keyed by its own schema's action names. Such a type gets a `settings_group_traits`
specialization (see `settings/settings-schema.hpp` for the full contract) and is declared with
`group_field` instead of `field`:

```cpp
template <>
struct tca::engine::settings::settings_group_traits<my_group> {
    using entry_type = std::vector<std::string>;  // every entry is a leaf of this type

    template <typename EntryWriter>
    static bool write_entries(const my_group& group, EntryWriter&& write);  // write(name, entry)

    template <typename EntryReader>
    static bool read_entries(my_group& group, EntryReader&& read);  // read(name) -> optional
};

// ...in the schema:
group_field{.name = "bindings", .binding = &controls_settings::bindings}
```

A group is written as a nested object with one key per entry. It has no `default_value`: before
every load the store resets it to a value-initialized one, so an entry the file doesn't have, or
one its type refuses, keeps the group's own default and the load reads as `incomplete`. Keys
`read_entries()` never asks for are ignored. `field` refuses a group type, so the two can't be
mixed up.

`input::action_map` has one in `input/settings/action-map-settings.hpp` (built with this module),
which stores each action's bindings as `"keyboard:space"`-style strings.

## What the store guarantees

**Restoring cannot stop a game from starting.** Neither `load()` nor `save()` throws; both report
through their return value. Whatever the file turns out to be, the settings are usable afterwards:
what the file does not give is the schema default.

`load_file()` says how much of that came from the file, when a game wants to know:

| `settings_load_result` | What it means                                                            |
|------------------------|--------------------------------------------------------------------------|
| `complete`             | Every field of the schema was in the file.                               |
| `incomplete`           | Some fields were not, or held something their type refused. Expected of a file written by an older version. |
| `unreadable`           | There is a file, but nothing could be read from it. Everything defaulted. |
| `missing`              | No file yet — a first run.                                               |

`load()` is `load_file() == complete`.

**A damaged file is kept, not thrown away.** When the store is constructed on a file nothing can
be read from, that file is renamed to `<settings file>.corrupt` before the defaults are written
over its name, so it can still be looked at afterwards. A file that only read *in part* is not
damaged and is left exactly where it is.

**Saving is crash-safe.** A save is written to a temporary next to the settings file and renamed
over it, through `platform::write_file_atomically()`. A crash, a full disk or a power loss
part-way through leaves the previously saved settings whole rather than a truncated file. What it
does not do is force the new contents out of the operating system's write-back cache: a power loss
immediately after a save may still lose that save — never the one before it.

**The format can evolve.** Fields are looked up by key, not by position, so reordering them,
removing one, or adding one to a later version all leave older files loading: a key the schema no
longer has is ignored, and a field the file does not have keeps its default. What this does not
cover is a field changing meaning under the same key — renaming the key is the way to say that,
which leaves old files defaulting the new field.

## Using another format

`settings_store`'s second template parameter is any type satisfying `settings_formatter` for the
settings struct: `make_document`, `parse`, `serialize`, `root`, `subdocument`, plus `write`/`read`
templates. A game that wants `key = value` lines, or its own binary format, writes one of those
instead of a `settings_value_traits` specialization. `tests/settings/settings-io.tests.cpp` has a
worked example of one.
