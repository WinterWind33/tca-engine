# Tickables

`rendering/tickables.hpp` is how the engine drives a game once per frame. Everything is either an
`updatable`, which changes state, or a `renderable`, which draws it. The application updates every
registered updatable, then renders every registered renderable.

Objects are composed through **non-owning views**: `updatable_view`, `renderable_view` and their
pausable variants. A view holds references in the order things were added, calls them in that
order, and never owns them. Layout widgets (`viewport_layout_widget`, `same_line_layout`,
`scroll_view`) are renderable views, so a page is a tree of renderables.

The model covers the per-frame loop and composition. It isn't an ECS, a scene graph or a render
pass, and it doesn't decide how `game_application_logic` ticks.

## The rule: context flows down as a parameter

Some drawing needs context that only exists for one call: a camera, bounds, or a clip rectangle
that the owner pushes while it draws. That context is **handed down as a parameter by whatever
owns it**, wrapped in a context object, the same way an `updatable` gets its `update_context&`:

```cpp
struct updatable        { virtual void update(update_context& context) = 0; };
struct renderable       { virtual void render() = 0; };        // ImGui's current window
struct scene_renderable { virtual void render(scene::render_context& context) = 0; };
```

The parameter is a context object, not the painter itself, so that what a renderable gets can grow
(frame time, the surface's input, debug flags) without every `render()` override changing.

- **Dependencies point one way.** The owner knows its children, and a child never keeps a
  reference back to its owner just to get at the context. That would make the object graph cyclic.
- **Scope is checked at compile time.** Only the owner can build the context (`render_context` and
  `scene_painter` have private constructors and can't be copied), so a child can only use it inside
  the call it was handed to. The context, and anything it hands out, mustn't be stored past that
  call.
- **Callbacks are only adapters.** `widgets::callback_renderable` and
  `scene::callback_scene_renderable` wrap a lambda for callers that don't want to write a class.
  They aren't an entry point: an owner takes the interface, never a `std::function`.

Plain `renderable`s draw into ImGui's current window, which needs no context. GUI widgets stay
that way.

## Scene surfaces

A `scene::scene_surface` renders its `scene_renderable`s in the order they were added, all through
the one `render_context` it builds each frame. How to write one is in [Scenes](scenes.md).

## When an ECS arrives

An ECS fits under the same rule. Components stay plain data, and a render system is a single
`scene_renderable` that walks its registry view and draws each entity with the painter of the
context it's handed.

## Why not the alternatives

- **A draw callback on the surface** (`set_draw_callback`, the original API): one hook per surface,
  with no add, remove or ordering.
- **A base class bound to its surface at construction, with a protected `painter()`**: every child
  references its owner and the owner references every child, which is a cycle. Misuse would also
  only be caught at run time.
- **Templating `renderable_view` over the context**: generic, but more machinery than one more kind
  of drawer justifies. It's worth revisiting if a third context-dependent drawer appears.

This is a middle ground until the rendering architecture gets a full rework.
