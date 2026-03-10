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

### Types -- Theming (forge_ui_theme.h)

- **`ForgeUiColor`** -- RGBA color as four floats (`float r, g, b, a`)
- **`ForgeUiTheme`** -- Centralized color palette with 14 semantic slots:
  `bg`, `surface`, `surface_hot`, `surface_active`, `title_bar`,
  `title_bar_text`, `text`, `text_dim`, `accent`, `accent_hot`, `border`,
  `border_focused`, `scrollbar_track`, `cursor`

### Types -- Immediate-Mode UI (forge_ui_ctx.h)

- **`ForgeUiRect`** -- Simple rectangle (x, y, w, h) for widget bounds
- **`ForgeUiTextInputState`** -- Application-owned text input buffer state
  (buffer, capacity, length, cursor)
- **`ForgeUiLayoutDirection`** -- Enum: `VERTICAL` (cursor moves down) or
  `HORIZONTAL` (cursor moves right)
- **`ForgeUiLayout`** -- Layout region: rect, direction, padding, spacing,
  cursor position, remaining space
- **`ForgeUiPanel`** -- Panel container: outer rect, content rect, scroll
  offset pointer, computed content height, and widget ID
- **`ForgeUiSpacing`** -- Themed spacing defaults: widget padding, item spacing,
  panel padding, title bar height, checkbox box size, slider dimensions,
  text input padding, and scrollbar width (all in logical pixels, scaled via
  `FORGE_UI_SCALED`)
- **`FORGE_UI_SCALED(ctx, value)`** -- Macro that multiplies a logical pixel
  value by the context's current scale factor (`ctx->scale`). Use for all
  spacing and sizing constants to support DPI-independent layout
- **`ForgeUiContext`** -- Immediate-mode UI context: holds mouse input, the
  hot/active widget IDs, font atlas reference, layout stack, clip rect,
  panel state, dynamic vertex/index buffers, scaling state (`scale`,
  `base_pixel_height`, `scaled_pixel_height`, `spacing`), and theme
  (`ForgeUiTheme`)

### Types -- Windows (forge_ui_window.h)

- **`ForgeUiWindowState`** -- Application-owned persistent window state: rect
  (position and size, updated by dragging), scroll_y (content scroll offset),
  collapsed (bool toggle), z_order (int draw priority, higher = on top)
- **`ForgeUiWindowEntry`** -- Per-frame window registration: widget ID,
  pointer to window state, and a separate vertex/index draw list for
  deferred rendering
- **`ForgeUiWindowContext`** -- Window management wrapper around ForgeUiContext:
  window registration array, active window tracking, hovered window ID for
  input routing, grab offset for drag, previous frame data for z-aware hit
  testing, and saved buffer pointers for draw list redirection

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
- **`forge_ui_ctx_label(ctx, text, x, y)`** -- Draw a text label using the
  theme's text color
- **`forge_ui_ctx_label_colored(ctx, text, x, y, r, g, b, a)`** -- Draw a
  text label with an explicit RGBA color
- **`forge_ui_hash_id(ctx, label)`** -- Hash a string label with the current
  scope seed (FNV-1a). Returns a `Uint32` widget ID
- **`forge_ui_push_id(ctx, name)`** -- Push a named scope onto the ID stack.
  Returns `true` on success, `false` if the stack is full (max 8 levels).
  Subsequent `hash_id` calls use the new scope as their seed
- **`forge_ui_pop_id(ctx)`** -- Pop the current scope from the ID stack
- **`forge_ui_pop_id_if(ctx, pushed)`** -- Pop the ID scope only if
  `pushed` is `true`. Pairs with `forge_ui_push_id()` to prevent
  underflow when the stack is full
- **`forge_ui_ctx_button(ctx, text, rect)`** -- Draw a button. Returns
  `true` on click. The `text` label doubles as the widget ID (hashed
  automatically). Use `"Label##suffix"` to disambiguate
- **`forge_ui_ctx_checkbox(ctx, label, value, rect)`** -- Draw a checkbox.
  Returns `true` when toggled (flips `*value`)
- **`forge_ui_ctx_slider(ctx, label, value, min_val, max_val, rect)`** --
  Draw a horizontal slider. Returns `true` when the value changes
- **`forge_ui_ctx_set_keyboard(ctx, text_input, ...)`** -- Pass keyboard input
  state for the current frame
- **`forge_ui_ctx_text_input(ctx, label, state, rect, cursor_visible)`** --
  Draw a text input field with cursor and keyboard editing
- **`forge_ui_ctx_layout_push(ctx, rect, direction, padding, spacing)`** --
  Push a layout region onto the layout stack
- **`forge_ui_ctx_layout_pop(ctx)`** -- Pop the current layout and return to
  the parent
- **`forge_ui_ctx_layout_next(ctx, size)`** -- Return the next widget rect
  from the current layout, advancing the cursor
- **`forge_ui_ctx_label_layout(ctx, text, size)`** -- Label placed by the
  current layout using the theme's text color
- **`forge_ui_ctx_label_colored_layout(ctx, text, size, r, g, b, a)`** --
  Label placed by the current layout with an explicit RGBA color
- **`forge_ui_ctx_button_layout(ctx, text, size)`** -- Button placed
  by the current layout
- **`forge_ui_ctx_checkbox_layout(ctx, label, value, size)`** -- Checkbox
  placed by the current layout
- **`forge_ui_ctx_slider_layout(ctx, label, value, min, max, size)`** --
  Slider placed by the current layout
- **`forge_ui_ctx_rect(ctx, rect, color)`** -- Draw a single solid-colored
  rectangle (one quad, no background track). Use for overlays, dividers, and
  backgrounds
- **`forge_ui_ctx_rect_layout(ctx, color, size)`** -- Solid rect placed by
  the current layout
- **`forge_ui_ctx_progress_bar(ctx, value, max_val, fill_color, rect)`** --
  Draw a non-interactive horizontal progress bar (filled portion proportional
  to value/max_val)
- **`forge_ui_ctx_progress_bar_layout(ctx, value, max_val, fill_color, size)`** --
  Progress bar placed by the current layout
- **`forge_ui_ctx_separator(ctx, rect)`** -- Draw a 1px horizontal divider
  line centered vertically in the rect, using the theme's border color
- **`forge_ui_ctx_separator_layout(ctx, size)`** -- Separator placed by the
  current layout
- **`forge_ui_ctx_tree_push(ctx, label, open, rect)`** -- Begin a collapsible
  tree node with a +/- indicator and label. Toggles `*open` on click. Pushes
  an ID scope. Returns the current value of `*open`
- **`forge_ui_ctx_tree_push_layout(ctx, label, open, size)`** -- Tree node
  placed by the current layout
- **`forge_ui_ctx_tree_pop(ctx)`** -- End a tree node: pops the ID scope
  pushed by `tree_push`. Must be called after every `tree_push`
- **`forge_ui_ctx_sparkline(ctx, values, count, min, max, color, rect)`** --
  Draw a mini line graph of float values. Each sample maps to a vertical
  position; consecutive samples are connected by thin quads
- **`forge_ui_ctx_sparkline_layout(ctx, values, count, min, max, color,
  size)`** -- Sparkline placed by the current layout
- **`forge_ui_ctx_drag_float(ctx, label, value, speed, min, max, rect)`** --
  Draw a drag-float field. Click-drag horizontally to change `*value` by
  mouse delta times `speed`, clamped to `[min, max]`. Returns `true` when
  the value changes
- **`forge_ui_ctx_drag_float_layout(ctx, label, value, speed, min, max,
  size)`** -- Drag-float placed by the current layout
- **`forge_ui_ctx_drag_float_n(ctx, label, values, count, speed, min, max,
  rect)`** -- Draw 1..4 drag-float fields side by side, dividing the rect
  width evenly. Returns `false` for counts outside `1..4`
- **`forge_ui_ctx_drag_float_n_layout(ctx, label, values, count, speed, min,
  max, size)`** -- Multi-component drag-float placed by the current layout.
  `count` must be in `1..4`; returns `false` otherwise
- **`forge_ui_ctx_drag_int(ctx, label, value, speed, min, max, rect)`** --
  Draw a drag-int field. Same interaction as drag-float but for integer
  values. Returns `true` when the value changes
- **`forge_ui_ctx_drag_int_layout(ctx, label, value, speed, min, max,
  size)`** -- Drag-int placed by the current layout
- **`forge_ui_ctx_drag_int_n(ctx, label, values, count, speed, min, max,
  rect)`** -- Draw 1..4 drag-int fields side by side. Returns `false` for
  counts outside `1..4`; otherwise returns `true` when any value changes
- **`forge_ui_ctx_drag_int_n_layout(ctx, label, values, count, speed, min,
  max, size)`** -- Multi-component drag-int placed by the current layout.
  `count` must be in `1..4`; returns `false` otherwise
- **`forge_ui_ctx_listbox(ctx, label, selected, items, item_count, rect)`** --
  Draw a clipped list of items. Click to select; `*selected` is updated to
  the clicked index (-1 means no selection). Returns `true` when the
  selection changes. The widget does not manage scrolling or a per-widget
  scroll offset — overflow rows are clipped and not reachable unless the
  caller places the listbox inside a scrolling container (e.g., a panel).
  Selection changes do not auto-scroll the item into view; callers must
  handle scrolling externally
- **`forge_ui_ctx_listbox_layout(ctx, label, selected, items, item_count,
  size)`** -- Listbox placed by the current layout
- **`forge_ui_ctx_dropdown(ctx, label, selected, open, items, item_count,
  rect)`** -- Draw a dropdown combo box. Click the header to toggle `*open`;
  when expanded, click an item to select it. `rect` bounds the header only;
  the open menu renders below `rect` and is not clipped to it. Returns
  `true` when the selection changes
- **`forge_ui_ctx_dropdown_layout(ctx, label, selected, open, items,
  item_count, size)`** -- Dropdown placed by the current layout. The layout
  reserves space for both the header and the open menu (in vertical layouts)
- **`forge_ui_ctx_radio(ctx, label, selected, option_value, rect)`** -- Draw
  a radio button. If `*selected == option_value`, shows a filled rectangular
  indicator; clicking sets `*selected = option_value`. Returns `true` when
  the selection actually changes (i.e., `*selected` is set to
  `option_value`); clicking an already-selected option returns `false`
- **`forge_ui_ctx_radio_layout(ctx, label, selected, option_value, size)`** --
  Radio button placed by the current layout
- **`forge_ui_ctx_color_picker(ctx, label, h, s, v, rect)`** -- Draw an HSV
  color picker with saturation-value gradient, hue bar, and preview swatch.
  `*h` is hue in degrees (0-360), `*s` and `*v` are 0-1. Returns `true`
  when any value changes
- **`forge_ui_ctx_color_picker_layout(ctx, label, h, s, v, size)`** -- Color
  picker placed by the current layout
- **`forge_ui_hsv_to_rgb(h, s, v, out_r, out_g, out_b)`** -- Convert HSV
  to RGB (all outputs 0-1). Wraps hue into [0, 360) via `fmodf` and clamps
  saturation and value to [0, 1]; non-finite inputs are replaced with 0
- **`forge_ui_rgb_to_hsv(r, g, b, out_h, out_s, out_v)`** -- Convert RGB
  (0-1) to HSV. Replaces non-finite inputs with 0, then clamps each RGB
  channel to [0, 1] before conversion
- **`forge_ui_ctx_panel_begin(ctx, title, rect, scroll_y)`** -- Begin a
  panel: draw background and title bar, set clip rect, push layout for
  child widgets. Returns `true` on success
- **`forge_ui_ctx_panel_end(ctx)`** -- End a panel: compute content overflow,
  draw scrollbar if needed, clear clip rect

### Functions -- Theming (forge_ui_theme.h)

- **`forge_ui_theme_default()`** -- Return the default dark theme with all 14
  color slots populated (derived from the matplotlib STYLE dict)
- **`forge_ui_theme_relative_luminance(r, g, b)`** -- Compute WCAG 2.1
  relative luminance from sRGB color components (applies piecewise
  linearization and CIE 1931 weights)
- **`forge_ui_theme_contrast_ratio(r1, g1, b1, r2, g2, b2)`** -- Compute
  WCAG 2.1 contrast ratio between two sRGB colors (range 1.0 to 21.0)
- **`forge_ui_theme_validate(theme)`** -- Validate all 17 adjacent color pairs
  in a theme against WCAG AA contrast ratios: 4.5:1 (SC 1.4.3) for text pairs,
  3.0:1 (SC 1.4.11) for non-text UI components. Returns the number of failing
  pairs (0 means all pass), or -1 if `theme` is NULL
- **`forge_ui_ctx_set_theme(ctx, theme)`** -- Apply a new theme to a UI
  context. Validates that all color components are in [0, 1]. Returns `true` on
  success, `false` if `ctx` is NULL or any color is out of range

### Functions -- Windows (forge_ui_window.h)

- **`forge_ui_wctx_init(wctx, ctx)`** -- Initialize a window context wrapping
  an existing UI context. Returns `true` on success
- **`forge_ui_wctx_free(wctx)`** -- Free per-window draw list buffers
- **`forge_ui_wctx_begin(wctx)`** -- Begin a frame: determine hovered window
  from previous frame data, reset per-frame window state. Call after
  `forge_ui_ctx_begin()`
- **`forge_ui_wctx_end(wctx)`** -- End the frame: sort window draw lists by
  z_order and append to the main context buffers in back-to-front order. Call
  before `forge_ui_ctx_end()`
- **`forge_ui_wctx_window_begin(wctx, title, state)`** -- Begin a window:
  draw title bar with collapse toggle, process dragging and z-ordering. Returns
  `true` if expanded (caller declares child widgets and calls window_end).
  Returns `false` if collapsed (caller must NOT call window_end)
- **`forge_ui_wctx_window_end(wctx)`** -- End a window: compute content height,
  draw scrollbar, pop layout, clear clip rect, restore main context buffers

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
- Stack-based layout system (vertical/horizontal, padding, spacing, nesting)
- Layout-aware widget variants (`_layout()` overloads)
- Panels with title bar, clipping, and vertical scrolling
- Axis-aligned rect clipping with UV remapping for glyph quads
- Interactive scrollbar with proportional thumb and drag interaction
- Mouse wheel scroll input via `scroll_delta` field
- Draggable windows with title bar drag, z-ordering, and collapse toggle
- Deferred draw ordering: per-window draw lists assembled back-to-front
- Z-aware input routing: only the topmost window receives mouse interaction
- Drag-value fields for float and int editing (single and multi-component)
- Listbox, dropdown, and radio button selection controls
- HSV color picker with gradient rendering and color space conversion
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
- [`lessons/ui/08-layout/`](../../lessons/ui/08-layout/) -- Automatic widget
  layout with vertical/horizontal stacking and nested layouts
- [`lessons/ui/09-panels-and-scrolling/`](../../lessons/ui/09-panels-and-scrolling/) --
  Panels with clipping, scrolling, and interactive scrollbar
- [`lessons/ui/10-windows/`](../../lessons/ui/10-windows/) -- Draggable windows
  with z-ordering, collapse toggle, and deferred draw ordering
- [`lessons/ui/11-widget-id-system/`](../../lessons/ui/11-widget-id-system/) --
  FNV-1a hashed string IDs with hierarchical scope stacking
- [`lessons/ui/12-font-scaling-and-spacing/`](../../lessons/ui/12-font-scaling-and-spacing/) --
  Global scale factor, spacing struct, and DPI-independent layout
- [`lessons/ui/13-theming-and-color-system/`](../../lessons/ui/13-theming-and-color-system/) --
  Centralized color palette, WCAG contrast validation, theme slot mapping
- [`lessons/ui/14-game-ui/`](../../lessons/ui/14-game-ui/) -- Game UI patterns:
  progress bars, inventory grids, HUD anchoring, action bars, pause menus
- [`lessons/ui/15-dev-ui/`](../../lessons/ui/15-dev-ui/) -- Dev UI patterns:
  collapsible tree nodes, property inspectors, editable property editors with
  drag-float/drag-int fields, console logs, sparkline graphs, scene tree views,
  listbox/dropdown/radio selection controls, HSV color picker
- [`tests/ui/`](../../tests/ui/) -- Unit tests for the UI library

## Design Philosophy

1. **Readability over performance** -- code is meant to be learned from
2. **Header-only** -- include `forge_ui.h` for fonts/atlas/layout, add
   `forge_ui_ctx.h` for immediate-mode controls, add `forge_ui_window.h`
   for draggable windows with z-ordering
3. **On-demand glyph parsing** -- font data is kept in memory; glyphs are
   parsed only when requested, so loading a font is fast
4. **No dependencies beyond SDL** -- no external parsing libraries
5. **Data-oriented** -- the UI library produces vertex/index data; the caller
   chooses how to render it (GPU or software rasterizer)

## License

[zlib](../../LICENSE) -- same as SDL and the rest of forge-gpu.
