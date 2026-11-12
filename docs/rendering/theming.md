# Theming

The theme resource database is the one place a game says what it looks like. It maps named
**roles** - "the muted text on a surface", "the corner radius", "a page heading" - to colours,
dimensions and typography, and every widget and every rendering backend resolves against it.
Changing one value in one place changes the whole application, with no widget to chase.

It lives in the `rendering` module (`src/tca-engine/rendering/theming/`), so there is nothing to
opt into: the aggregate `tca-engine` target already carries it.

## The short version

```cpp
#include "rendering/gui/imgui/imgui-theme.hpp"
#include "rendering/theming/active-theme.hpp"
#include "rendering/theming/theme.hpp"

namespace theming = tca::engine::theming;
namespace rendering = tca::engine::rendering;

int main() {
    auto app{tca::engine::application::application_factory::create_platform_preferred_application()};

    app->set_app_name("The Clinical Aspect")
        .set_theme(theming::theme{}
                       .set(theming::color_role::accent, rendering::rgb(0, 150, 136))
                       .set(theming::color_role::window_background, rendering::rgb(12, 14, 18))
                       .set(theming::dimension_role::corner_radius, 2.0))
        .set_logic(std::make_unique<my_game_logic>());

    app->run();
}
```

That is the whole configuration. Every engine widget, every ImGui element and the colour the
window is cleared to now come from those three lines plus the engine's defaults.

## Roles, not colours

A widget names a role, never a value:

```cpp
// In a widget's render():
const rendering::color hint{theming::color_for(theming::color_role::on_surface_muted)};
const double gutter{theming::dimension_for(theming::dimension_role::item_spacing)};
const theming::typography title{theming::typography_for(theming::typography_role::heading)};
```

`color_for()`, `dimension_for()` and `typography_for()` resolve against the *active* theme - the
ambient one the application is currently wearing - so a widget does not have to be handed a theme
through its constructor, and two widgets naming the same role cannot drift apart.

Resolve at render time rather than caching the resolved value in the widget. That is what lets a
theme swapped at runtime reach widgets that were built long before it.

Resolving a built-in role is **total**: it always returns a value, so there is no failure path and
no `std::optional` to unwrap at the call site.

### The built-in vocabulary

| Kind | Roles |
|------|-------|
| `color_role` | `window_background`, `surface`, `surface_variant`, `surface_hovered`, `surface_active`, `overlay_scrim`, `on_surface`, `on_surface_muted`, `on_surface_disabled`, `on_accent`, `accent`, `accent_hovered`, `accent_active`, `outline`, `outline_variant`, `success`, `warning`, `error` |
| `dimension_role` | `window_padding`, `frame_padding`, `item_spacing`, `item_inner_spacing`, `indent`, `scrollbar_size`, `grab_min_size`, `border_thickness`, `corner_radius`, `content_width_fraction`, `content_height_fraction` |
| `typography_role` | `body`, `caption`, `subheading`, `heading`, `display` |

Dimensions are unscaled pixels, except the two content fractions, which are fractions of the
available space in `[0.0, 1.0]`. A `typography` is a scale relative to the application's body font
size; the scale is a `core::ranged_value`, so a negative or absurd one is rejected where the theme
is written rather than at the point where text comes out wrong.

Each enum ends with a `count` enumerator. It is not a role - it is the size of the theme's table
for that kind, and passing it to `set()` or `resolve()` is a programming error.

### Roles the engine has never heard of

The built-in set is a starting point, not a ceiling. A game adds its own without forking the
engine:

```cpp
const theming::theme clinical{
    theming::theme{}.set_custom_color("ecg_trace", rendering::rgb(0, 220, 130))};

// At the call site, with a built-in role as the fallback, so this stays as total as any other
// resolve:
const rendering::color trace{
    theming::get_active_theme().resolve_custom_color_or("ecg_trace", theming::color_role::accent)};
```

`resolve_custom_color()` (and the dimension and typography equivalents) return a
`std::optional` when you do want to know whether the role was defined at all.

## Overriding one colour

ImGui has exactly one text colour (`ImGuiCol_Text`), so roles describing a *text variant* -
`on_surface_muted`, `on_accent` - cannot live in the style table the applier writes. They are
reached by overriding the style colour for the scope that needs it, which is what
`scoped_style_color` is for:

```cpp
#include "rendering/gui/imgui/imgui-color-helpers.hpp"

const imgui::scoped_style_color muted{ImGuiCol_Text, theming::color_role::on_surface_muted};
ImGui::Text("%s", hint.c_str());
```

It restores the previous colour when the scope ends, and is neither copyable nor movable, so the
push and its pop cannot be separated - ImGui's style stack is strictly last-in-first-out, and
nesting these correctly is what guarantees it.

It works for any `ImGuiCol_` entry, not only text: pushing `color_role::error` onto
`ImGuiCol_Button` paints one destructive button without the theme having to know about it.

There is a second constructor taking an explicit `rendering::color`, for colours that were
*computed* - a tint driven by data, a colour sampled at runtime. If you are about to pass a
constant to it, you want `set_custom_color()` and a named role instead: a literal written at a call
site is exactly the per-widget colour this database exists to abolish.

## Deriving a variant

A variant - high contrast, colour-blind friendly, a seasonal skin - restates only what differs.
`derive()` returns a copy to build on, and the `set()` overloads chain:

```cpp
const theming::theme highContrast{
    base.derive()
        .set(theming::color_role::on_surface, rendering::colors::WHITE)
        .set(theming::color_role::window_background, rendering::colors::BLACK)
        .set(theming::color_role::outline, rendering::colors::WHITE)};
```

Derivation copies rather than keeping a pointer to the base: a derived theme keeps working after
the theme it came from is gone, and resolving never walks a chain.

## Deriving a theme from an accent colour

`theme_from_accent_color()` turns a single colour - typically one sampled from the player's
wallpaper - into a whole theme:

```cpp
#include "rendering/theming/theme-factories.hpp"

app->set_theme(theming::theme_from_accent_color(sampledWallpaperColor));
```

Backgrounds and outlines keep the accent's hue and saturation at fixed brightness levels, so the
result reads consistently dark however dark the sampled colour was, while staying visibly tinted
by it. Interactive elements use the accent itself but never drop below a minimum brightness, so an
accent sampled from a night sky still yields legible buttons; hover and active brighten it further,
clamped so they cannot overshoot white. Roles the accent says nothing about - the feedback colours,
the dimensions, the typography - keep their defaults.

The result is an ordinary theme: derive from it, compare it, store it, apply it, like any other.

## Re-theming at runtime

`application::set_theme()` may be called after `run()`, so a game can offer a theme setting with no
restart. The new theme reaches the widgets, the ImGui style and the window clear colour from the
next frame.

Game logic does not hold the `application` - it holds the `application_controller` it was handed -
and re-themes through that:

```cpp
// Inside application_logic, e.g. when the player picks a theme in the settings screen:
get_application_controller().use_theme(chosenTheme);
```

That routes back into `set_theme()`, so it is the same path with the same effects; there is no
shorter one that only moves half of it. Engine-internal code below the application layer can still
reach `tca::engine::imgui::use_theme()` directly, which changes the active theme and the ImGui
style but knows nothing about the window clear colour.

`use_theme()` makes the theme active *and* mirrors it into the ImGui style. If there is no ImGui
context yet - which is the normal case while the application is still being configured - it only
makes it active, and the context picks it up when it is created.

For tests, and for rendering a subtree under a different theme, `theming::scoped_active_theme` is
an RAII override that puts the previous theme back on the way out.

## Applying a theme to a backend

The database is plain data and knows nothing about ImGui, or about any other renderer: it builds
and is tested with no graphics context at all. Mirroring it into a concrete backend is a separate
step, which for ImGui is `imgui::apply_theme()`.

That application is **exhaustive** by construction, which matters because a half-applied theme is
the failure mode nobody notices in review:

- the style is first reset to ImGui's own defaults, so no value can survive from whatever style was
  applied before, not even the style fields no role covers;
- the colour table is written through a mapping that a `static_assert` checks covers *every*
  `ImGuiCol_` entry, in enum order. A Dear ImGui update that adds or reorders a colour breaks the
  build rather than quietly leaving that element unthemed.

A second backend (the web module, a future Vulkan path) is themed by writing its own applier
against the same database - there is nothing to restate.

`apply_theme()` takes a size scale, which the Win32 ImGui bootstrap passes the window's DPI scale,
so paddings and roundings are DPI-scaled while the theme keeps its sizes unscaled and
authoritative. A DPI change re-applies the theme at the new scale rather than rescaling the style
in place, so repeated DPI changes cannot accumulate rounding.

## What the engine's own widgets do

`label` is described by a `typography_role` and a `color_role` and holds no size or colour of its
own; `fps_overlay` is caption-sized muted text because that is what a diagnostics overlay *is*;
`viewport_layout_widget` names the role its backdrop is painted with (or `std::nullopt` for no
backdrop at all). `scroll_view` carries no appearance of its own and gets everything from the style
the theme wrote.

They are the forcing function for the vocabulary: if an engine widget cannot be expressed in these
roles, the roles are wrong.
