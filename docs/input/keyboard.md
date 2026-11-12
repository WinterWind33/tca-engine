# Input

The `input` module gives a game the keyboard: which keys are down, which were pressed or released,
the text typed, and game-defined actions bound to keys the player can rebind. It gives the mouse the
same way, covered in [Mouse](mouse.md); views, consumption and fixed steps below apply to both. It
holds no Win32 and no ImGui code, so all of it is unit-tested on Linux with synthetic events, and a
game reads input without including either.

Input reaches game code as part of the call that uses it, never through a global: every
`updatable::update()` gets it from its `update_context`, every `fixed_step_updatable::step()` from
its `step_context`.

## The short version

```cpp
#include "input/actions/standard-actions.hpp"
#include "rendering/custom-tickables/fixed-step-tickables.hpp"

class player final : public rendering::fixed_step_updatable {
    void step(rendering::step_context& context) override {
        const input::input_view in{context.input()};

        if (m_actions.is_active(in, input::standard_action::move_left)) { /* steer */ }
        if (m_actions.was_triggered(in, input::standard_action::primary)) { /* jump */ }

        // Raw keys stay available for input that must not be rebindable.
        if (in.keyboard().was_pressed(input::key::f3)) { /* toggle debug view */ }
    }

    input::standard_action_map m_actions{};
};
```

`examples/demo-app/main.cpp` flies a ship this way, and has a pause menu that shows consumption.

## Keys

`input::key` (`input/key.hpp`) names a **physical position** on the keyboard, following W3C
`KeyboardEvent.code`: `key::w` is the key left of `E` on a US layout, whatever the active layout
prints on it, so WASD stays WASD on AZERTY. Typed characters are a separate thing (see
[Typed text](#typed-text)). `key_name()` gives each key a stable lowercase name (`"space"`,
`"arrow_up"`, `"delete"`), and `key_from_name()` reads it back.

## Levels and edges

A **level** is a key's state right now (`is_down()`). An **edge** is the moment it changed:
`was_pressed()` (up → down) and `was_released()` (down → up). Edges are counted, not flagged:
`press_count()` and `release_count()` say how many there were, so a key tapped twice in one frame
reports two presses, and a press and release inside one frame reports one of each. OS key repeats
are not presses.

Steering reads levels; one-shot actions (jump, fire, open a menu) read edges. To make a tap shorter
than a fixed step still count, read `is_down() || was_pressed()`.

## Views, and who owns what

The application owns one persistent `input_frame`, written to by the platform as messages arrive,
and ends its tick (`end_tick()`: edges and text are dropped, held keys stay) after every `tick()`.
Game code never gets the frame itself: it gets an `input_view` (and from it a `keyboard_view`),
small copyable handles that read the frame and can consume from it, but can't write to it. So no
updatable can fake a key press for the ones after it, or end the tick early.

`input_view::snapshot()` copies the frame as the view sees it, for code that has to keep input past
the tick it arrived in (`fixed_step_updatable` does, see below).

Tests and custom backends write through the state directly: `keyboard_state::press()` /
`release()`, `input_frame::append_text()`, or an `input_collector`, which turns platform-shaped
events (`on_key_down(k, isRepeat)`, `on_key_up()`, `on_char()`, `on_focus_lost()`) into those writes.

## Consuming input

Updatables run in registration order, which is also their input priority. `consume(key)` hides a
key from everything after the consumer: it reads as up with no edges. `consume_all()` hides every
key and the typed text. A pause menu registered before gameplay that consumes everything while it
is open keeps gameplay from seeing any of it. A paused updatable receives nothing.

A key consumed while held stays hidden until it is released and pressed again, even across ticks,
so the key that closed a menu doesn't also act in the game behind it. Its release isn't reported
either. Consuming everything is also how to *clear* input across a state change.

Before the update pass, when ImGui wants the keyboard (a text field or other active widget has it),
the engine consumes all input, so typing a name doesn't move the player. Only an active widget or
modal captures it: a merely focused ImGui window doesn't.

## Fixed steps

A `fixed_step_updatable` runs zero, one or several steps per frame. Its input is kept so that every
edge is seen exactly once:

- a frame that runs no step carries its edges and text over to the next one that does;
- of several steps in one frame, only the first sees the edges and the text; all of them see the
  held keys.

The game never latches or clears anything itself. A key consumed by a step stays hidden from later
steps until it's pressed again; the cost is that a key already held when the updatable starts or
resumes reads as up until it's pressed again.

## Typed text

`input_view::text()` is the UTF-8 text typed during the tick, from the platform's character
messages, so the keyboard layout, dead keys and IMEs are honoured. Control characters (Backspace,
Enter, Tab...) are not text: read them as keys.

## Focus

Losing window focus releases every held key (with release edges), so a key held while alt-tabbing
out doesn't stay stuck down.

## Actions

A game declares an action enum and an `action_schema` for it, in the style of a `settings_schema`:

```cpp
enum class player_action { jump, pause };

template <>
struct tca::engine::input::action_schema<player_action> {
    static constexpr auto actions{std::make_tuple(
        action{.name = "jump",
               .id = player_action::jump,
               .default_bindings = std::array{binding{key::space}, binding{key::w}}},
        action{.name = "pause",
               .id = player_action::pause,
               .default_bindings = std::array{binding{key::escape}}})};
};
```

`action_map<player_action>` starts on the defaults and answers `is_active()`, `was_triggered()` and
`was_released()` against an `input_view`. An action reads as the OR of its bindings. Queries only
read: `consume(input, action)` consumes every binding of the action, explicitly. `rebind()`,
`bindings_of()` and `reset_to_defaults()` change and inspect bindings at run time. An
`action_map<T>` can't be named without a schema, and can't be queried with another enum.

`name` is the action's key in a settings file: never change it once players have saved bindings. A
`binding` is a `std::variant` of a `key` or a `mouse_button` (see [Mouse](mouse.md#actions)), so an
action can be bound to either; further devices can be added without changing game code.

The engine ships `standard_action` / `standard_action_map` (`input/actions/standard-actions.hpp`):
movement (WASD + arrows), primary (E, Space), secondary (R), confirm (Enter, numpad Enter), cancel (Escape,
Backspace) and pause (Escape, Pause). A game that needs other actions declares its own enum; the
two kinds of map can live side by side.

## Saving bindings

With the settings module on, `input/settings/action-map-settings.hpp` (target
`tca-engine-input-settings`, part of the aggregate `tca-engine`) lets an `action_map` be a field of
the game's settings struct, declared with `group_field`:

```cpp
#include "input/settings/action-map-settings.hpp"

struct controls_settings {
    input::action_map<player_action> bindings{};
};

template <>
struct tca::engine::settings::settings_schema<controls_settings> {
    static constexpr auto fields{std::make_tuple(
        group_field{.name = "bindings", .binding = &controls_settings::bindings})};
};
```

```json
"bindings": {
    "jump": ["keyboard:space", "keyboard:w", "mouse:left"],
    "pause": ["keyboard:escape"]
}
```

Every action of the schema is saved; an empty list is an action the player unbound. An action the
file doesn't have, or whose list holds anything that isn't a binding, keeps its default; actions the
file has and the schema doesn't are ignored. The input module itself never depends on settings. See
`docs/settings/user-settings.md` for groups in general.

## Known limits

- Win32 merges all keyboards into one, so two keyboards holding the same key release it together.
- Media and browser keys arrive as `WM_APPCOMMAND`, not as keys, and aren't bindable.
- Game code sees a frame (levels, edge counts, text), not an event stream: the order of two keys
  pressed within one frame is lost.
- Keys and mouse buttons only: gamepads are a future binding source.
