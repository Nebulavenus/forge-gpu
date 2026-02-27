# forge-gpu UI Library

A header-only UI toolkit: TrueType font parsing, glyph rasterization, font
atlas building, text layout, and immediate-mode UI controls.

## Quick Start

```c
#include "ui/forge_ui.h"

ForgeUiFont font;
if (forge_ui_ttf_load("font.ttf", &font)) {
    Uint16 glyph_idx = forge_ui_ttf_glyph_index(&font, 'A');
    ForgeUiTtfGlyph glyph;
    if (forge_ui_ttf_load_glyph(&font, glyph_idx, &glyph)) {
        // glyph.points[], glyph.flags[], glyph.contour_ends[]
        forge_ui_ttf_glyph_free(&glyph);
    }
    forge_ui_ttf_free(&font);
}
```

## What's Included

### Types -- Font Parsing

- **`ForgeUiPoint`** -- 2D point in font units (`Sint16 x, y`)
- **`ForgeUiTtfTableEntry`** -- Table directory entry (tag, offset, length)
- **`ForgeUiTtfHead`** -- Global font metadata (unitsPerEm, bounding box)
- **`ForgeUiTtfHhea`** -- Horizontal metrics (ascender, descender, lineGap)
- **`ForgeUiTtfMaxp`** -- Maximum profile (numGlyphs)
- **`ForgeUiTtfGlyph`** -- Parsed glyph outline (contours, points, flags)
- **`ForgeUiFont`** -- Top-level font structure holding all parsed data
- **`ForgeUiRasterOpts`** -- Rasterization options (supersample level)
- **`ForgeUiGlyphBitmap`** -- Rasterized glyph bitmap (pixels, dimensions,
  bearing offsets)

### Types -- Font Atlas & Text Layout

- **`ForgeUiUVRect`** -- UV rectangle within the atlas (normalized coordinates)
- **`ForgeUiPackedGlyph`** -- Per-glyph metadata in the atlas (UVs, bearings,
  advance width)
- **`ForgeUiFontAtlas`** -- Font atlas: single-channel texture with all packed
  glyphs, a white pixel region, and cached font metrics
- **`ForgeUiVertex`** -- Universal UI vertex: position, UV, and RGBA color
  (32 bytes, matches `ForgeRasterVertex` layout)
- **`ForgeUiTextAlign`** -- Enum: `LEFT`, `CENTER`, `RIGHT`
- **`ForgeUiTextOpts`** -- Text layout options (max width, alignment, color)
- **`ForgeUiTextLayout`** -- Laid-out text: vertex/index arrays, bounding box,
  line count
- **`ForgeUiTextMetrics`** -- Text measurement: width, height, line count
  (no vertex generation)

### Types -- Immediate-Mode UI (forge_ui_ctx.h)

- **`ForgeUiRect`** -- Simple rectangle (x, y, w, h) for widget bounds
- **`ForgeUiTextInputState`** -- Application-owned text input buffer state
  (buffer, capacity, length, cursor)
- **`ForgeUiContext`** -- Immediate-mode UI context: holds mouse input, the
  hot/active widget IDs, font atlas reference, and dynamic vertex/index buffers

### Functions -- Font Parsing & Rasterization

- **`forge_ui_ttf_load(path, out_font)`** -- Load a TTF file and parse core
  tables. Returns `true` on success
- **`forge_ui_ttf_free(font)`** -- Free all memory from `forge_ui_ttf_load`
- **`forge_ui_ttf_glyph_index(font, codepoint)`** -- Look up glyph index for
  a Unicode codepoint via cmap (returns 0 for unmapped codepoints)
- **`forge_ui_ttf_load_glyph(font, glyph_index, out_glyph)`** -- Parse a
  simple glyph's outline on demand. Returns `false` for compound glyphs
- **`forge_ui_ttf_glyph_free(glyph)`** -- Free memory from
  `forge_ui_ttf_load_glyph`
- **`forge_ui_rasterize_glyph(font, glyph_index, pixel_height, opts, out)`**
  -- Rasterize a glyph into a single-channel alpha bitmap. Pass `NULL` for
  `opts` to use default 4x4 supersampling
- **`forge_ui_glyph_bitmap_free(bitmap)`** -- Free pixel data from
  `forge_ui_rasterize_glyph`
- **`forge_ui_ttf_advance_width(font, glyph_index)`** -- Look up the advance
  width (in font units) for a glyph via the hmtx table

### Functions -- Font Atlas

- **`forge_ui_atlas_build(font, pixel_height, codepoints, codepoint_count,
  padding, out_atlas)`** -- Rasterize all requested glyphs and pack into a
  single power-of-two texture with shelf packing. Returns `true` on success
- **`forge_ui_atlas_free(atlas)`** -- Free atlas memory
- **`forge_ui_atlas_lookup(atlas, codepoint)`** -- Look up a packed glyph by
  Unicode codepoint. Returns `NULL` if not found

### Functions -- Text Layout

- **`forge_ui_text_layout(atlas, text, x, y, opts, out_layout)`** -- Lay out
  a string into positioned, textured quads (4 vertices + 6 indices per
  character). Supports word wrapping and alignment. Returns `true` on success
- **`forge_ui_text_layout_free(layout)`** -- Free vertex/index arrays
- **`forge_ui_text_measure(atlas, text, opts)`** -- Measure text bounding box
  without generating vertices

### Functions -- Immediate-Mode UI (forge_ui_ctx.h)

- **`forge_ui_ctx_init(ctx, atlas)`** -- Initialize a UI context with a font
  atlas. Allocates vertex/index buffers. Returns `true` on success
- **`forge_ui_ctx_free(ctx)`** -- Free context buffers
- **`forge_ui_ctx_begin(ctx, mouse_x, mouse_y, mouse_down)`** -- Begin a
  frame: reset draw data, update input state
- **`forge_ui_ctx_end(ctx)`** -- End a frame: resolve hot/active widget state
- **`forge_ui_ctx_label(ctx, text, x, y, r, g, b, a)`** -- Draw a text label
- **`forge_ui_ctx_button(ctx, id, text, rect)`** -- Draw a button. Returns
  `true` on click
- **`forge_ui_ctx_checkbox(ctx, id, label, value, rect)`** -- Draw a checkbox.
  Returns `true` when toggled (flips `*value`)
- **`forge_ui_ctx_slider(ctx, id, value, min_val, max_val, rect)`** -- Draw a
  horizontal slider. Returns `true` when the value changes
- **`forge_ui_ctx_set_keyboard(ctx, text_input, ...)`** -- Pass keyboard input
  state for the current frame
- **`forge_ui_ctx_text_input(ctx, id, state, rect, cursor_visible)`** -- Draw
  a text input field with cursor and keyboard editing

## Supported Features

### Font Parsing

- TTF offset table and table directory parsing
- `head` table (unitsPerEm, global bounding box, indexToLocFormat)
- `hhea` table (ascender, descender, lineGap, numberOfHMetrics)
- `maxp` table (numGlyphs)
- `cmap` table with format 4 (BMP Unicode to glyph index)
- `loca` table (short and long format glyph offsets)
- `glyf` table (simple glyph outlines -- contours, flags, delta-encoded
  coordinates)
- `hmtx` table (per-glyph advance widths and left side bearings)

### Rasterization & Atlas

- Scanline rasterization with non-zero winding rule
- Configurable supersampled anti-aliasing (1x to 8x)
- Font atlas building with shelf (row-based) packing
- Power-of-two atlas textures with a white pixel region for solid shapes
- Grayscale BMP writing for atlas and glyph visualization

### Text Layout

- String-to-quad conversion (vertex/index arrays for GPU upload)
- Word wrapping with configurable max width
- Left, center, and right alignment
- Text measurement without vertex generation

### Immediate-Mode UI

- Two-ID state machine (hot/active) for interaction
- Buttons, checkboxes, sliders, and text input fields
- Keyboard input support (typing, cursor movement, backspace, delete)
- Focus management for text inputs
- Dynamic vertex/index buffer accumulation per frame

## Limitations

These are intentional simplifications for a learning library:

- **No compound glyphs** -- detected and skipped with a log message
- **No hinting** -- hinting instructions are skipped
- **No kerning** -- `kern` table not parsed
- **No advanced positioning** -- `GPOS` not parsed
- **No glyph substitution** -- `GSUB` not parsed
- **No sub-pixel rendering** -- no ClearType-style RGB anti-aliasing
- **TrueType only** -- CFF/OpenType outlines not supported

## Dependencies

- **SDL3** -- for file I/O (`SDL_LoadFile`), memory allocation (`SDL_malloc`,
  `SDL_free`), and logging (`SDL_Log`)
- No `forge_math.h` dependency -- TTF parsing is integer/byte work

## Where It's Used

- [`lessons/ui/01-ttf-parsing/`](../../lessons/ui/01-ttf-parsing/) -- Loading
  a TTF and printing its structure
- [`lessons/ui/02-glyph-rasterization/`](../../lessons/ui/02-glyph-rasterization/) --
  Rasterizing glyph outlines into alpha bitmaps with anti-aliasing
- [`lessons/ui/03-font-atlas/`](../../lessons/ui/03-font-atlas/) -- Building
  a font atlas with shelf packing
- [`lessons/ui/04-text-layout/`](../../lessons/ui/04-text-layout/) -- Laying
  out text into positioned quads with wrapping and alignment
- [`lessons/ui/05-immediate-mode-basics/`](../../lessons/ui/05-immediate-mode-basics/) --
  Buttons, labels, and the hot/active state machine
- [`lessons/ui/06-checkboxes-and-sliders/`](../../lessons/ui/06-checkboxes-and-sliders/) --
  Checkboxes and slider controls
- [`lessons/ui/07-text-input/`](../../lessons/ui/07-text-input/) -- Text input
  fields with cursor and keyboard editing
- [`tests/ui/`](../../tests/ui/) -- Unit tests for the UI library

## Design Philosophy

1. **Readability over performance** -- code is meant to be learned from
2. **Header-only** -- include `forge_ui.h` for fonts/atlas/layout, add
   `forge_ui_ctx.h` for immediate-mode controls
3. **On-demand glyph parsing** -- font data is kept in memory; glyphs are
   parsed only when requested, so loading a font is fast
4. **No dependencies beyond SDL** -- no external parsing libraries
5. **Data-oriented** -- the UI library produces vertex/index data; the caller
   chooses how to render it (GPU or software rasterizer)

## License

[zlib](../../LICENSE) -- same as SDL and the rest of forge-gpu.
