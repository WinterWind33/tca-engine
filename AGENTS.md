# TCAEngine — contributor guide

Guidance for anyone working on the engine, and for the coding agents they use: how the repo is
laid out, how to build and test it, and the conventions a change is expected to follow.

Pure C++23 project built with CMake. GSL, spdlog, Eigen3, nlohmann-json, Catch2 and (Windows-only) WIL
come from vcpkg via `vcpkg.json` + a toolchain file. A few libraries without a proper
`CONFIG` package (imgui, miniaudio, imgui-markdown-ext, stb, verso) are instead fetched
directly from their git repositories at configure time by `third-party/CMakeLists.txt`.

## Layout

| Path                       | Contents                                                      |
|-----------------------------|----------------------------------------------------------------|
| `src/tca-engine/`           | The engine itself, one CMake module (STATIC lib) per subfolder: `core`, `input`, `platform`, `rendering`, `application`, `sounds`, `settings`, `web`, `testing` — aggregated behind the `tca-engine` INTERFACE target (all but `testing`, which test targets link explicitly) |
| `tests/`                    | Catch2 v3 test suite, mirroring `src/tca-engine/`'s subfolders |
| `third-party/`               | `CMakeLists.txt`/`cmake/` fetch git-only dependencies at configure time (git-ignored past that) |
| `examples/`                 | Opt-in demo applications (`TCA_ENGINE_BUILD_EXAMPLES`, default `OFF`) built on the engine's public API only |
| `scripts/`                  | Developer helper scripts                                       |
| `docs/`                     | Guides, one folder per category (`rendering/`, `input/`, `settings/`, `testing/`, `project/`) |
| `bin/`, `lib/`               | Build outputs (git-ignored); multi-config generators add a `<Config>/` subfolder |

Most of `platform`, `rendering` and `application` fork on `WIN32` — the Win32 sources
(DirectX 12 / OpenGL3 rendering backends, Win32 windowing, WIC image decoding) only build
on Windows; Linux builds the `UNIX AND NOT APPLE` path (GLFW window, XDG-style folders), and macOS
only the platform-neutral code plus its own `platform/macos/` sources. Both decode images with
stb_image (`platform/stb/`).
CI builds and tests all three on every push (see Build and test below).
`TCA_ENGINE_BUILD_WEB_MODULE` and `TCA_ENGINE_BUILD_SETTINGS_MODULE` (both default `ON`) opt the
`web` and `settings` modules in/out. The `settings` module is the only thing pulling nlohmann-json
in, and keeps it PRIVATE so it appears in none of its headers — see
`docs/settings/user-settings.md` for what that module gives a game.

The `rendering` module carries the theme resource database (`rendering/theming/`): the one place a
game defines its colours, dimensions and typography, as named roles every widget and backend
resolves against. It holds no ImGui include of its own - the ImGui applier in
`rendering/gui/imgui/imgui-theme.cpp` is one consumer of it - so the data half builds and is tested
with no renderer. See `docs/rendering/theming.md`.

The `input` module (`input/`) is the keyboard and the mouse: levels, counted edges, typed text, the
pointer, the wheel and rebindable actions, delivered to every `update()` / `step()` through its
context as a view of the frame the application owns. It has no Win32, GLFW or ImGui code; the
platform feeds live in `platform`. Its bridge to settings (`input/settings/`) is a target of its
own, so input never depends on settings. See `docs/input/keyboard.md` and `docs/input/mouse.md`.

Games get drawable images only through `rendering::image_factory`, either from a file
(`load_from_file()`) or from RGBA8 pixels built in memory (`create_from_pixels()`), never by talking
to an `image_render_queue` directly. See `docs/rendering/images.md`.

`TCA_ENGINE_BUILD_EXAMPLES` (default `OFF`) builds `examples/`, whose `tca-engine-demo` target is a
game-shaped consumer of the engine: it links the aggregate `tca-engine` target and uses widgets,
theme roles and the application API only, never a rendering backend's headers. It is turned on in
`.vscode/settings.json` and in CI, where it is the cheapest check that the public API stays usable
from outside the engine's own sources.

`TCA_ENGINE_BUILD_TESTING_SUPPORT` (defaulting to `TCA_ENGINE_BUILD_TESTS`) opts in the `testing`
module, exposed as `TCAEngine::testing` — the reusable test helpers (headless ImGui context,
scoped temp directory, Win32 windowing utilities) that both this repo's suite and a downstream
game's suite link. It depends on no test framework on purpose, so anything in it that would need
to report a failure keeps its Catch2 wrapper in `tests/utilities/` instead. See
`docs/testing/testing-support.md`.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DTCA_ENGINE_BUILD_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
ctest --test-dir build --output-on-failure
```

Run a single test case (Catch2 tags/names work too, e.g. `ctest --test-dir build -R <name>`
or invoke the test binary directly with `./tca-engine_tests "<test name>"` from inside `bin/`).
The binary must run with `bin/` as its working directory: the build copies `tests/res` next to it
and tests load resources (images, sounds) by paths relative to it, so running it from the repo root
fails every resource-loading test with "file not found". `ctest` already sets that working
directory. When checking a change, rebuild first and run only the tags you touched (e.g.
`"[math]"`) before the full suite.

CI builds and tests Debug and Release, with `-DTCA_ENGINE_WARNINGS_AS_ERRORS=ON`, on every push
to every branch: `.github/workflows/linux.yml` (gcc, clang), `windows.yml` (MSVC, clang) and
`macos.yml` (Apple Clang), all through the shared `.github/actions/build-and-test` action.
Nothing is cached, so every run starts from a clean vcpkg checkout at the baseline `vcpkg.json`
pins. Match the warnings flag locally before pushing if you've touched anything that might warn.

## Conventions

- Every CMake option, cache variable and internal variable is prefixed `TCA_ENGINE_`. Keep it
  that way — it is what makes the project safe to `add_subdirectory()` into something larger.
- Targets link `TCAEngine::compile_options` for shared warning flags rather than calling
  `target_compile_options` directly.
- Every source file starts with the two-line header:

  ```cpp
  // Copyright (c) 2026 Andrea Ballestrazzi
  // SPDX-License-Identifier: MIT
  ```

  New `.cpp`/`.hpp` files get it too — the year is the year the file was created, not the
  current year, and existing headers are left alone.
- If a piece of code needs its reason explained, say it in the comment's own words rather than
  pointing at an external record; link a GitHub issue only when it adds context the comment can't.
- Formatting is fixed by `.clang-format`; run it before committing rather than reformatting by
  hand.
- `CHANGELOG.md` is hand-written and follows Keep a Changelog + SemVer 2.0.0. User-visible changes
  go under `## [Unreleased]`, added by the PR that makes them; internal churn does not. It is
  deliberately not generated: release-please is configured with `skip-changelog`, so it bumps the
  version, tags and publishes the release but never rewrites this file. Two things follow from
  that. Keep `## [Unreleased]` at the very top, directly under the header — release-please used to
  insert generated sections above it, which left hand-written entries stranded mid-file and
  duplicated. And `## [Unreleased]` is promoted to a numbered section at release time by
  `scripts/promote_changelog.py`, run from the release workflow — see `docs/project/releasing.md`.
- Keep each `CHANGELOG.md` entry to one sentence of at most two standard lines (~160 characters):
  name the API and say what it does or what changed, nothing more. Edge cases, defaults, error
  behaviour, rationale and migration detail belong in the docs, the header comments or the PR
  description, not in the changelog. Split genuinely separate changes into separate entries
  rather than chaining clauses.
- Every user-visible feature ships with its docs in the same PR: a section in the module's guide in
  `docs/`, or a new guide linked from the README's documentation list. Guides sit in one folder
  per category (`docs/rendering/`, `docs/input/`, ...) with lowercase kebab-case names (e.g.
  `docs/rendering/scenes.md`); a new category gets a folder and a README heading of its own. Keep
  them short: what it is, a minimal code example, the rules a game must know, and a diagram
  (Mermaid or ASCII) where a picture explains it faster than prose. Leave implementation detail to
  the header comments, and restructure an existing guide rather than bolting on a section that
  doesn't fit it.
- The single source of truth for the version is `project(VERSION ...)` in the root
  `CMakeLists.txt`, available to CMake as `TCA_ENGINE_VERSION`. It is not surfaced to C++ code:
  no header or compile definition carries it.
- PRs are squash-merged, so the PR title becomes the commit message on `main` — title it as
  a [Conventional Commit](https://www.conventionalcommits.org/) (`feat: ...`, `fix: ...`,
  `feat!: ...`/`BREAKING CHANGE:` footer, `chore:`/`docs:`/`refactor:`/etc.). While the
  project is pre-1.0 (`0.y.z`), `fix`/`chore`/other non-breaking types bump patch, and
  `feat` or a breaking change bump minor (major stays `0` until the first stable release).
  `.github/workflows/pr-title.yml` fails a PR whose title release-please would ignore.
