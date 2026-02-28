# UI Lessons

Build an immediate-mode UI system from scratch — font parsing, text rendering,
layout, and interactive controls — all in pure C with no GPU dependency.

## Purpose

UI lessons teach how to build the **data layer** for a game or tool UI:

- Parse TrueType fonts and extract glyph outlines
- Rasterize glyphs into bitmap atlases
- Lay out text with proper metrics, kerning, and line breaking
- Build immediate-mode controls (buttons, checkboxes, sliders, text input, panels)
- Generate vertex, index, and texture data ready for any renderer

The UI library produces **textures, vertices, indices, and UVs** — it contains
no GPU code. A separate GPU lesson renders the output using SDL's GPU API.

## Philosophy

- **Data-first** — Define what the GPU needs (vertices, UVs, textures) before
  writing any code. The data contract is the bridge between CPU and GPU.
- **Immediate-mode** — UI state lives in the application, not the library.
  Each frame, the application describes what it wants; the library generates
  draw data.
- **One concept per lesson** — Font parsing, rasterization, atlas packing,
  text layout, and controls are each separate lessons that build on each other.
- **Visual results** — Every lesson outputs images or diagrams showing what
  it produced, even without a GPU renderer.

## How lessons work

Each UI lesson is a standalone C program that demonstrates one concept:

- **`main.c`** — Demo program producing data (images, vertex arrays, metrics)
- **`README.md`** — Explanation of the concept, data formats, and exercises
- **`CMakeLists.txt`** — Build configuration
- **`assets/`** — Diagrams and example output images

Lessons build progressively — later lessons use types and functions from
earlier ones. Reusable code moves into the shared UI library at
`common/ui/` as the track grows.

## Lessons

| # | Topic | What you'll learn |
|---|-------|-------------------|
| 01 | [TTF Parsing](01-ttf-parsing/) | TrueType binary format, table directory, font metrics, cmap character mapping, glyph outlines |
| 02 | [Glyph Rasterization](02-glyph-rasterization/) | Contour reconstruction, scanline rasterization, non-zero winding rule, quadratic Bézier crossings, supersampled anti-aliasing |
| 03 | [Font Atlas](03-font-atlas/) | Rectangle packing, shelf algorithm, atlas sizing, UV coordinates, per-glyph metadata, padding for texture bleed, white pixel technique |
| 04 | [Text Layout](04-text-layout/) | Pen/cursor model, advance widths, bearing offsets, baseline positioning, quad generation, index buffers, line breaking, text alignment, ForgeUiVertex format |
| 05 | [Immediate-Mode Basics](05-immediate-mode-basics/) | Retained vs immediate mode, declare-then-draw loop, ForgeUiContext, hot/active state machine, hit testing, labels, buttons, white pixel technique |
| 06 | [Checkboxes and Sliders](06-checkboxes-and-sliders/) | External mutable state, checkbox toggle, slider drag interaction, value mapping (pixel to normalized to user value), active persistence outside widget bounds |
| 07 | [Text Input](07-text-input/) | Focused ID (keyboard focus), click-to-focus / click-outside-to-unfocus, ForgeUiTextInputState, character insertion and deletion, cursor movement, cursor bar positioning via text measurement |
| 08 | [Layout](08-layout/) | ForgeUiLayout cursor model, vertical and horizontal directions, padding and spacing, layout stack with push/pop nesting, layout_next() for automatic widget positioning, layout-aware widget variants |
| 09 | [Panels and Scrolling](09-panels-and-scrolling/) | ForgeUiPanel containers, axis-aligned rect clipping with UV remapping, scroll offset in layout, interactive scrollbar with proportional thumb, mouse wheel input |
| 10 | [Windows](10-windows/) | ForgeUiWindowState (rect, scroll_y, collapsed, z_order), title bar dragging with grab offset, click-to-front z-ordering, collapse/expand toggle, deferred per-window draw lists, z-aware input routing |

## Building

```bash
cmake -B build
cmake --build build --config Debug
```

Individual lessons:

```bash
# Windows
build\lessons\ui\NN-topic-name\Debug\NN-topic-name.exe

# Linux / macOS
./build/lessons/ui/NN-topic-name/NN-topic-name
```

## Data contract

UI lessons produce data that a GPU renderer consumes. Every lesson documents
its output format in a **Data output** section:

- **Vertices** — position (vec2), UV (vec2), color (vec4)
- **Indices** — uint16 or uint32 triangle list, CCW winding
- **Textures** — single-channel alpha (fonts) or RGBA (colored UI elements)
- **Metadata** — glyph metrics, widget bounds, layout rects

This separation keeps the UI logic testable and renderer-agnostic.

## Related lessons

- **[Math lessons](../math/)** — Vectors (vec2 for positions and UVs),
  rectangles, interpolation
- **[Engine lessons](../engine/)** — Memory layout (structs, offsets), C
  fundamentals, header-only libraries
- **GPU lessons** — A future GPU lesson will render UI data using the
  techniques from the GPU track

## Adding new lessons

Use the `/ui-lesson` skill to scaffold a new UI lesson with all required files.
