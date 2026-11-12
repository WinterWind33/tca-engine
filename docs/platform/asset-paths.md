# Asset paths

A shipped game can't count on its working directory. Steam usually starts it in the install
folder, but a desktop shortcut, a launch option, Proton on the Steam Deck or a player
double-clicking it from another folder can all start it somewhere else, and then every relative
path like `"res/ship.png"` points at nothing.

`platform/folders.hpp` resolves paths against the folder holding the executable instead:

```cpp
#include "platform/folders.hpp"

using namespace tca::engine;

// <executable folder>/res/ship.png, whatever the working directory is
rendering::image ship{get_image_factory().load_from_file(platform::get_asset_path("res/ship.png"))};
const sounds::sound jump{platform::get_asset_path("res/audio/jump.wav")};

const std::filesystem::path installFolder{platform::get_executable_directory()};
```

```
<install folder>/              <- get_executable_directory()
├── my-game(.exe)
└── res/
    └── ship.png               <- get_asset_path("res/ship.png")
```

## Rules

- Ship assets next to the executable, and copy them there in the build (see
  `examples/demo-app/CMakeLists.txt`), so a development build finds them the same way.
- `get_asset_path()` only builds the path; it doesn't check the file exists. Loading a missing
  file fails as usual.
- An empty path gives the executable's folder itself, and an absolute path is returned unchanged.
- If the executable's folder can't be determined, both functions return an empty path rather than
  falling back to the working directory.
- Engine APIs that take a path (`load_from_file()`, `sounds::sound`, ...) still resolve relative
  paths against the working directory. Choosing the executable's folder is up to the caller.
- Files the player changes (settings, saves, logs) don't belong here: the install folder may not be
  writable. Use `get_preferred_user_data_folder()` and `get_preferred_log_folder()` for those.
