# forge-gpu CPU Rasterizer

A header-only CPU triangle rasterizer using the edge function method.

## Quick Start

```c
#include "raster/forge_raster.h"

/* Create a framebuffer */
ForgeRasterBuffer buf = forge_raster_buffer_create(512, 512);
forge_raster_clear(&buf, 0.1f, 0.1f, 0.1f, 1.0f);

/* Define a triangle */
ForgeRasterVertex v0 = {256, 50,  0, 0, 1, 0, 0, 1};  /* red */
ForgeRasterVertex v1 = {100, 400, 0, 0, 0, 1, 0, 1};  /* green */
ForgeRasterVertex v2 = {412, 400, 0, 0, 0, 0, 1, 1};  /* blue */
forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

/* Save to disk */
forge_raster_write_bmp(&buf, "triangle.bmp");
forge_raster_buffer_destroy(&buf);
```

## What's Included

### Types

- **`ForgeRasterVertex`** -- Screen position (`x, y`), texture coordinates
  (`u, v`), and color (`r, g, b, a`). 32 bytes, matches `ForgeUiVertex` layout
- **`ForgeRasterBuffer`** -- RGBA8888 pixel framebuffer (row-major, top-left
  origin)
- **`ForgeRasterTexture`** -- Single-channel grayscale texture for sampling
  (font atlases, masks)

### Functions

- **`forge_raster_buffer_create(width, height)`** -- Allocate an RGBA8888
  framebuffer. Returns a buffer with `pixels = NULL` on failure
- **`forge_raster_buffer_destroy(buf)`** -- Free framebuffer memory
- **`forge_raster_clear(buf, r, g, b, a)`** -- Fill the entire framebuffer
  with a solid color (components in `[0, 1]`)
- **`forge_raster_triangle(buf, v0, v1, v2, texture)`** -- Rasterize a single
  triangle using edge functions. Interpolates colors and UVs via barycentric
  coordinates. If `texture` is non-NULL, samples it and multiplies with vertex
  color. Alpha-blends onto the framebuffer (source-over compositing)
- **`forge_raster_triangles_indexed(buf, vertices, vertex_count, indices,
  index_count, texture)`** -- Draw triangles from vertex and index arrays.
  Every three consecutive indices form one triangle
- **`forge_raster_write_bmp(buf, path)`** -- Write the framebuffer to a 32-bit
  BMP file (handles RGBA-to-BGRA conversion and row flipping)

### Vertex Layout

The vertex format matches `ForgeUiVertex` exactly (32 bytes):

| Field | Type | Description |
|-------|------|-------------|
| `x, y` | `float` | Screen position in pixels (framebuffer coordinates) |
| `u, v` | `float` | Texture coordinates `[0, 1]` |
| `r, g, b, a` | `float` | Vertex color (straight alpha) |

This means UI vertex/index buffers can be rasterized directly for
visualization and testing.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `FORGE_RASTER_BPP` | 4 | Bytes per pixel (RGBA8888) |
| `FORGE_RASTER_MAX_DIM` | 16384 | Maximum width or height (4x 4K) |

## Supported Features

- RGBA8888 framebuffer with creation, clearing, and BMP output
- Edge-function triangle rasterization with bounding box optimization
- Barycentric interpolation of vertex colors and UV coordinates
- Optional grayscale texture sampling (nearest-neighbor)
- Source-over alpha blending
- Indexed triangle drawing (vertex + index buffer batches)
- Both CCW and CW winding orders
- Pixel center sampling at `(x + 0.5, y + 0.5)` matching GPU convention

## Limitations

These are intentional simplifications for a learning library:

- **No subpixel precision** -- basic edge function test only
- **No SIMD** -- clarity over speed
- **Nearest-neighbor only** -- no bilinear texture filtering
- **No depth buffer** -- triangles composite in submission order
- **No clipping** -- triangles are clamped to framebuffer bounds

## Dependencies

- **SDL3** -- basic types (`Uint8`, `Uint32`), memory allocation, and logging
- No GPU API or windowing dependencies

## Where It's Used

- [`lessons/engine/10-cpu-rasterization/`](../../lessons/engine/10-cpu-rasterization/) --
  Full example demonstrating edge-function rasterization
- [`tests/raster/`](../../tests/raster/) -- 24 comprehensive tests covering
  all features and edge cases

## Design Philosophy

1. **Readability over performance** -- code is meant to be learned from
2. **Header-only** -- just include `forge_raster.h`, no build config needed
3. **Defensive validation** -- rejects degenerate triangles, NaN/Infinity
   coordinates, out-of-bounds indices, and oversized dimensions
4. **GPU-compatible conventions** -- pixel center sampling, vertex layout,
   and winding order match GPU rasterization

## License

[zlib](../../LICENSE) -- same as SDL and the rest of forge-gpu.
