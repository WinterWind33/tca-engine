# Mouse

The mouse reaches game code the same way as the keyboard: through the `input_view` of every
`update()` and `step()`, as `in.mouse()`. It gives the five buttons (levels and counted edges, as
for keys), where the pointer is, whether it is over the window, and how far the wheel turned. Views,
consumption and fixed steps follow the [keyboard's rules](keyboard.md); this page covers what is
specific to the mouse.

## The short version

```cpp
void step(rendering::step_context& context) override {
    const input::mouse_view mouse{context.input().mouse()};

    if (mouse.was_pressed(input::mouse_button::left)) {
        if (const auto at{mouse.pointer()}; at && hits_the_bear(*at)) {
            pet_the_bear();
            mouse.consume(input::mouse_button::left); // nothing behind the bear gets the click
        }
    }

    if (mouse.is_down(input::mouse_button::right)) { /* drag the food bowl */ }

    m_zoom += mouse.wheel().vertical; // notches, positive scrolling up
}
```

## Buttons

`input::mouse_button` (`input/mouse-button.hpp`) is `left`, `right`, `middle`, `x1` and `x2` (the
side buttons, usually "back" and "forward"). `mouse_button_name()` gives each a stable name
(`"left"`, `"x1"`) and `mouse_button_from_name()` reads it back.

Buttons have levels and edges exactly like keys: `is_down()`, `was_pressed()`, `was_released()`,
`press_count()` and `release_count()`. A click that starts and ends within one frame reads as one
press and one release; a double-click is simply a second press.

## Pointer

`pointer()` is where the pointer last was, in the window's client area, in the unit the window size
is reported in (physical pixels on Win32), origin top-left, y down. It is empty until the pointer
first moves over the window. Converting it to world coordinates is the scene's job, not the input
module's.

```
(0,0) ───────────────► x
  │  ┌──────────────┐
  │  │ client area  │   is_pointer_inside() == true
  │  │       •(x,y) │
  ▼  └──────────────┘
  y                  •(-30, 410)  dragging out: still reported, inside == false
```

`is_pointer_inside()` says whether it is over the client area. While a button is held, the window
keeps receiving the pointer after it leaves, so a drag can be followed outside the window, with
negative or past-the-size positions, and its release is still reported. The pointer is a level: it
survives the end of the tick and can't be consumed.

## Wheel

`wheel()` is how far the wheel turned since the last tick, in **notches**: one notch of a standard
wheel is `1`, whatever the platform reports. `vertical` is positive scrolling up (away from the
player), `horizontal` positive to the right. Smooth wheels and touchpads report fractions of a
notch; turns within a tick add up, and opposite turns cancel out. The wheel resets every tick.

## Consuming

`mouse.consume(button)` hides a button from every later reader, like `keyboard.consume(key)`: held,
it stays hidden until it is released and pressed again, and that release isn't reported.
`consume_wheel()` zeroes the wheel for later readers, and `mouse.consume_all()` does both for every
button. The keyboard and the mouse are separate: `in.consume_all()` takes the keyboard and the
typed text, not the mouse.

## Fixed steps

A `fixed_step_updatable`'s steps see button edges exactly once, as with keys: a click in a frame
that ran no step reaches the next step that runs, and of several steps in one frame only the first
sees it. Wheel turns are carried over and summed the same way, and also reach only the first of
several steps. The pointer is a level, so every step sees its latest position.

## Focus

Losing window focus releases every held button, with release edges, like held keys. The pointer and
the wheel are left alone.

## Actions

A mouse button is a `binding` like a key, so an action can be bound to either, or both:

```cpp
action{.name = "feed",
       .id = player_action::feed,
       .default_bindings = std::array{binding{key::f}, binding{mouse_button::right}}}
```

The action map reads a key from the keyboard and a button from the mouse, and `consume(input,
action)` consumes both. Saved bindings name the device: `"mouse:right"` next to `"keyboard:f"`. See
[Actions](keyboard.md#actions).

## Known limits

- Clicking an ImGui window also reaches the game: unlike the keyboard, the engine doesn't consume
  the mouse when ImGui wants it.
- Buttons past the fifth, and touch and pen input as such, are not reported.
- Win32 merges every mouse into one, as it does keyboards.
