# forge-gpu UI Library

A header-only TrueType font parser for loading `.ttf` files and extracting
glyph outlines, font metrics, and character-to-glyph mappings.

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

### Types

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

### Functions

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

## Supported Features

- TTF offset table and table directory parsing
- `head` table (unitsPerEm, global bounding box, indexToLocFormat)
- `hhea` table (ascender, descender, lineGap, numberOfHMetrics)
- `maxp` table (numGlyphs)
- `cmap` table with format 4 (BMP Unicode to glyph index)
- `loca` table (short and long format glyph offsets)
- `glyf` table (simple glyph outlines -- contours, flags, delta-encoded
  coordinates)

## Limitations

These are intentional simplifications for a learning library:

- **No compound glyphs** -- detected and skipped with a log message
- **No hinting** -- hinting instructions are skipped
- **No kerning** -- `kern` table not parsed
- **No advanced positioning** -- `GPOS` not parsed
- **No glyph substitution** -- `GSUB` not parsed
- **No per-glyph metrics** -- `hmtx` table not parsed (future lesson)
- ~~No rasterization~~ — **Added in UI Lesson 02** (scanline rasterization
  with non-zero winding rule and supersampled anti-aliasing)
- **TrueType only** -- CFF/OpenType outlines not supported

## Dependencies

- **SDL3** -- for file I/O (`SDL_LoadFile`), memory allocation (`SDL_malloc`,
  `SDL_free`), and logging (`SDL_Log`)
- No `forge_math.h` dependency -- TTF parsing is integer/byte work

## Where It's Used

- [`lessons/ui/01-ttf-parsing/`](../../lessons/ui/01-ttf-parsing/) -- Full
  example loading a TTF and printing its structure
- [`lessons/ui/02-glyph-rasterization/`](../../lessons/ui/02-glyph-rasterization/) --
  Rasterizing glyph outlines into alpha bitmaps with anti-aliasing

## Design Philosophy

1. **Readability over performance** -- code is meant to be learned from
2. **Header-only** -- just include `forge_ui.h`, no build configuration needed
3. **On-demand glyph parsing** -- font data is kept in memory; glyphs are
   parsed only when requested, so loading a font is fast
4. **No dependencies beyond SDL** -- no external parsing libraries

## License

[zlib](../../LICENSE) -- same as SDL and the rest of forge-gpu.
