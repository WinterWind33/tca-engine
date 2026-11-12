# Images

A game gets drawable images from the `rendering::image_factory` injected into its logic
(`application_logic::get_image_factory()`). The factory uploads pixels through whichever rendering
backend is active and hands back a `rendering::image`: a move-only handle that owns the GPU
texture, draws itself with `render()`, and releases the texture when it is destroyed.

A game never talks to the `image_render_queue` behind the factory, and never needs to know which
backend (DirectX 12, OpenGL3, or the null queue of a headless application) is running.

## From a file

```cpp
rendering::image wallpaper{
    get_image_factory().load_from_file(platform::get_asset_path("res/wallpaper.png"))};
```

Resolve shipped files with `platform::get_asset_path()` rather than a bare relative path, which
follows the working directory: see [Asset paths](../platform/asset-paths.md).

`load_from_file()` decodes the file and uploads it. It throws `std::runtime_error` when the file
can't be decoded.

## From pixels built in memory

```cpp
#include "platform/images/image-decoder.hpp"
#include "rendering/image-factory.hpp"

platform::images::decoded_image atlas{
    .pixels = std::vector<std::byte>(width * height * 4),
    .width = width,
    .height = height,
};
// ... fill atlas.pixels ...
rendering::image atlasImage{get_image_factory().create_from_pixels(std::move(atlas))};
```

`create_from_pixels()` uploads a `decoded_image` the game built itself and returns the same kind of
`image` as `load_from_file()`, with the same ownership and the same average colour sampling. In
fact `load_from_file(path)` is exactly `create_from_pixels(decode_image_from_file(path))`, so an
image created from a PNG's pixels draws the same as the PNG loaded directly. Use it for:

- **An atlas packed at load time**: decode one PNG per sprite frame, pack them into one buffer, and
  upload that once. The frames then share one texture, with no offline packing step.
- **Images generated at run time**: a sprite cut along a line, a colour-keyed copy, a procedural
  texture.

Pass the `decoded_image` as an rvalue (`std::move`): the factory consumes the pixels, and passing
an lvalue copies them first.

### Pixel format

`decoded_image::pixels` is always 8-bit RGBA, 4 bytes per pixel in R, G, B, A order, row-major,
top row first, with no padding between rows. Pixel `(x, y)` starts at byte `(y * width + x) * 4`.

`create_from_pixels()` validates the buffer before anything reaches the GPU and throws
`std::invalid_argument` when:

- `width` or `height` is zero, or
- `pixels.size()` is not exactly `width * height * 4` (a size that would overflow is rejected too).

Errors from the backend's upload propagate unchanged, as with `load_from_file()`.

### Decode, edit, upload

`platform::images::decode_image_from_file()` is public API for games. It decodes PNG and the
other formats the platform decoder supports (WIC on Windows, stb_image on Linux) into the format
above, on every platform, so a game can decode source art, change it and upload the result:

```cpp
platform::images::decoded_image sprite{platform::images::decode_image_from_file("res/ship.png")};
// ... edit sprite.pixels ...
rendering::image edited{get_image_factory().create_from_pixels(std::move(sprite))};
```

It throws `std::runtime_error` when the file is missing or can't be decoded.

## Not covered

- Packing atlases. How to pack is the game's choice; the engine only provides the upload path.
- Updating an uploaded texture in place (streaming or dynamic textures). Create a new image
  instead.
