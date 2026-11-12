# Testing support

The engine's test-only helpers live in their own module, `tca-engine-testing`, exposed as the
CMake target **`TCAEngine::testing`**. They used to be compiled straight into the engine's own
test executable, which made them unreachable from a game consuming the engine, so every game that
wanted to unit-test its ImGui widgets had to copy the engine's implementation into its own repo.

The module is deliberately **framework-agnostic**: nothing in it pulls in Catch2, or any other test
framework, so a consumer stays free to pick its own.

## Building it

The module is controlled by `TCA_ENGINE_BUILD_TESTING_SUPPORT`, which defaults to whatever
`TCA_ENGINE_BUILD_TESTS` is. The two are independent on purpose: a game consuming the engine as a
subdirectory turns the engine's own test suite off and still wants these helpers.

```cmake
set(TCA_ENGINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)          # not interested in the engine's suite
set(TCA_ENGINE_BUILD_TESTING_SUPPORT ON CACHE BOOL "" FORCE) # but do want the helpers
add_subdirectory(third-party/tca-engine)

add_executable(my_game_tests ...)
target_link_libraries(my_game_tests PRIVATE
    tca-engine
    TCAEngine::testing
    Catch2::Catch2WithMain # or whichever framework the game uses - the module forces none
)
```

`TCAEngine::testing` carries its own public include directory, so the headers below resolve with no
`target_include_directories` pointing into the engine's `tests/` folder. It is *not* part of the
aggregate `tca-engine` target: test scaffolding has no business in a game's shipping binary, so it
is linked explicitly, from test targets only.

Everything lives in the `tca::engine::testing` namespace.

## `headless_imgui_context`

`#include "testing/imgui/headless-imgui-context.hpp"`

Stands up an ImGui context with no rendering backend attached, so widget code calling
`ImGui::Begin()/End()`, `ImGui::GetMainViewport()` and friends can be driven frame by frame from a
unit test, with no window and no GPU. It builds the font atlas up front (which a real backend would
otherwise do, and without which `ImGui::NewFrame()` asserts) and disables ImGui's `imgui.ini`
persistence, so a test run leaves nothing behind and cannot inherit window positions from an
earlier one.

Frames are driven explicitly, mirroring how a real application drives ImGui:

```cpp
#include "testing/imgui/headless-imgui-context.hpp"

TEST_CASE("The main menu draws its buttons") {
    tca::engine::testing::headless_imgui_context imguiContext{};

    imguiContext.new_frame();

    main_menu_widget widget{};
    widget.render();

    imguiContext.end_frame();
}
```

`set_display_size()` can be called in between frames to simulate the application window being
resized or maximized. A frame left open when the context is destroyed - which is what a failing
test case leaves behind as it unwinds - is closed by the destructor rather than tripping an ImGui
assert.

## `scoped_temp_directory`

`#include "testing/scoped-temp-directory.hpp"`

A uniquely-named temporary directory for one test case, removed again when the test case finishes,
whatever its outcome. The constructor takes the name of a subfolder of the system temp directory to
group a suite's directories under, so anything a crashing test leaves behind is easy to find and
clean up by hand.

```cpp
#include "testing/scoped-temp-directory.hpp"

TEST_CASE("Settings survive a round trip") {
    const tca::engine::testing::scoped_temp_directory tempDirectory{"my-game-settings"};

    const std::filesystem::path settingsFile{tempDirectory.path() / "settings.json"};
    // ...
}
```

## `windowing-utilities` (Windows only)

`#include "testing/win32/windowing-utilities.hpp"`

Helpers for tests that create and destroy real Win32 windows:

- `application_tss<ApplicationType>` - a test-specific subclass exposing the window an engine Win32
  application created, which the application itself keeps protected, so a test can drive the
  application the way a user would without the application widening its production API.
- `simulate_user_closing_application()` - posts `WM_CLOSE` to a window.
- `remove_wm_quit_messages_from_queue()` - drains lingering `WM_QUIT` messages from the thread's
  message queue, which would otherwise make the next test's message loop exit immediately. It
  *returns* how many it drained rather than asserting, since the module depends on no test
  framework; assert the result against `WM_QUIT_DRAIN_LIMIT` with your own. The engine's suite
  wraps it in `tests/utilities/win32/wm-quit-drain.hpp` for exactly that.

## `windowing-utilities` (Linux only)

`#include "testing/linux/windowing-utilities.hpp"`

Helpers for tests that create real GLFW windows headlessly, i.e. with no display attached:

- `scoped_null_glfw_context` - RAII helper that initializes GLFW on its "null" platform
  (`GLFW_PLATFORM_NULL`, GLFW `>=3.4`), which hands out real `GLFWwindow` handles without talking
  to an actual windowing system, so GLFW-backed code can be exercised against real handles in
  headless CI the same way it would run on a developer machine with a display. Throws
  `std::runtime_error` if `glfwInit()` fails, since the module depends on no test framework to
  report the failure itself. It is not reference-counted: destroying one calls `glfwTerminate()`,
  which destroys every GLFW window still alive, so keep one per test and never nest them.
- `apply_headless_glfw_window_hints()` - sets the GLFW window hints (`GLFW_VISIBLE = false`,
  `GLFW_CLIENT_API = GLFW_NO_API`) that keep a window hidden and avoid the null platform's lack of
  a real OpenGL implementation (it would otherwise try to load OSMesa, which CI does not have
  installed). Window hints are global GLFW state consumed by the *next* `glfwCreateWindow()` call,
  so call this right before creating a window - whether directly, or indirectly by constructing an
  engine window type (e.g. `linux_application_window`) that creates its own.
- `create_headless_glfw_window()` - applies the hints above and creates a hidden, no-client-API
  `GLFWwindow`. Throws `std::runtime_error` if `glfwCreateWindow()` fails.

## Adding to the module

A helper belongs here when it is useful to a game and not specific to the engine's own suite. The
bar is the framework-agnostic rule above: if reporting a failure is part of what it does, the
Catch2-flavoured wrapper stays in `tests/utilities/` and only the mechanism moves here. Anything
tied to the engine's own fixtures - `tests/utilities/test-assets.hpp`, for instance, which points
at the engine's own `res/` folder - stays in `tests/` too.
