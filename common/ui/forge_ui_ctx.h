/*
 * forge_ui_ctx.h -- Header-only immediate-mode UI context for forge-gpu
 *
 * Implements a minimal immediate-mode UI system based on the two-ID state
 * machine from Casey Muratori's IMGUI talk.  The application declares widgets
 * each frame (labels, buttons, etc.), and this module generates vertex/index
 * draw data ready for GPU upload or software rasterization.
 *
 * Key concepts:
 *   - ForgeUiContext holds per-frame input state (mouse position, button)
 *     and the two persistence IDs: hot (mouse is hovering) and active
 *     (mouse is pressing).
 *   - Widget IDs are FNV-1a hashes of string labels combined with a
 *     hierarchical scope seed.  The "##" separator lets callers
 *     distinguish widgets with identical display text.
 *   - Labels emit textured quads for each character using forge_ui_text_layout.
 *   - Buttons emit a solid-colored background rectangle (using the atlas
 *     white_uv region) plus centered text, and return true on click.
 *   - Hit testing checks the mouse position against widget bounding rects.
 *   - Draw data uses ForgeUiVertex / uint32 indices, matching lesson 04.
 *
 * Usage:
 *   #include "ui/forge_ui.h"
 *   #include "ui/forge_ui_ctx.h"
 *
 *   ForgeUiContext ctx;
 *   forge_ui_ctx_init(&ctx, &atlas);
 *
 *   // Each frame:
 *   forge_ui_ctx_begin(&ctx, mouse_x, mouse_y, mouse_down);
 *   if (forge_ui_ctx_button(&ctx, "Click me", rect)) { ... }
 *   forge_ui_ctx_label(&ctx, "Hello", x, y);
 *   forge_ui_ctx_end(&ctx);
 *
 *   // Use ctx.vertices, ctx.vertex_count, ctx.indices, ctx.index_count
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_UI_CTX_H
#define FORGE_UI_CTX_H

#include <SDL3/SDL.h>
#include "forge_ui_theme.h"
#include "forge_ui.h"

/* Portable isfinite — SDL_stdinc.h does not yet provide this */
#ifndef forge_isfinite
#define forge_isfinite(x) (!SDL_isinf(x) && !SDL_isnan(x))
#endif

/* Largest float below 360.0 — replaces FORGE_UI_HUE_MAX so we
 * don't need <math.h> for a single call.  360.0f in IEEE 754 is
 * 0x43B40000; subtracting one ULP gives 0x43B3FFFF = 359.99997f. */
#define FORGE_UI_HUE_MAX 359.99997f

/* ── Constants ──────────────────────────────────────────────────────────── */

/* Initial capacity for the vertex and index buffers.  The buffers grow
 * dynamically as widgets emit draw data, doubling each time they fill up.
 * 256 vertices (64 quads) is enough for a simple UI without reallocation. */
#define FORGE_UI_CTX_INITIAL_VERTEX_CAPACITY  256
#define FORGE_UI_CTX_INITIAL_INDEX_CAPACITY   384

/* No widget is hot or active.  Zero is reserved as the null ID -- callers
 * must use non-zero IDs for their widgets. */
#define FORGE_UI_ID_NONE  0

/* Layout sentinel: pass as padding to forge_ui_ctx_layout_push to request
 * explicit zero padding (no themed default substitution).  Positive values
 * override the themed default; zero uses the themed default; this negative
 * sentinel is clamped to 0 internally but skips the "use theme" path. */
#define FORGE_UI_LAYOUT_EXPLICIT_ZERO  -1.0f

/* Maximum nesting depth for the ID seed stack.  32 levels supports deep
 * tree node hierarchies (e.g. window > panel > row > tree > tree > widget).
 * The stack is bounds-checked at runtime with SDL_Log on overflow/underflow. */
#define FORGE_UI_ID_STACK_MAX_DEPTH  32

/* ── Checkbox style ────────────────────────────────────────────────────── */

/* Checkbox box dimensions.  The box is a square drawn at the left edge
 * of the widget rect, vertically centered.  The label text is drawn to
 * the right of the box with a small gap. */
#define FORGE_UI_CB_BOX_SIZE     18.0f  /* checkbox square side length in pixels */
#define FORGE_UI_CB_INNER_PAD     3.0f  /* padding between box edge and check fill */
#define FORGE_UI_CB_LABEL_GAP     8.0f  /* horizontal gap between box and label */

/* General widget padding.  Used as the default inset inside widget
 * backgrounds (button text from edges, etc.) and as the base value for
 * ForgeUiSpacing.widget_padding. */
#define FORGE_UI_WIDGET_PADDING  10.0f

/* ── Slider style ──────────────────────────────────────────────────────── */

/* Slider track and thumb dimensions.  The track is a thin horizontal bar
 * centered vertically in the widget rect.  The thumb slides along the
 * track to indicate the current value.  The "effective track" (the range
 * the thumb center can travel) is inset by half the thumb width on each
 * side, so the thumb never overhangs the rect edges. */
#define FORGE_UI_SL_TRACK_HEIGHT   4.0f   /* thin track bar height */
#define FORGE_UI_SL_THUMB_WIDTH   12.0f   /* thumb rectangle width */
#define FORGE_UI_SL_THUMB_HEIGHT  22.0f   /* thumb rectangle height */

/* ── Text input style ─────────────────────────────────────────────────── */

/* Text input layout dimensions.  Padding keeps text away from the field
 * edge so characters are readable near the border.  The cursor bar is
 * thin (2 px) to mimic a standard text insertion caret.  The border is
 * 1 px to provide a focused-state indicator without obscuring content. */
#define FORGE_UI_TI_PADDING       8.0f   /* left padding in pixels before text starts */
#define FORGE_UI_TI_CURSOR_WIDTH  2.0f   /* cursor bar width in pixels */
#define FORGE_UI_TI_BORDER_WIDTH  1.0f   /* focused border edge width in pixels */

/* ── Layout constants ───────────────────────────────────────────────────── */

/* Maximum nesting depth for layout regions.  8 levels is enough for most
 * UI hierarchies (e.g. panel > row > column > widget).  The layout stack
 * lives inside ForgeUiContext and is bounds-checked with runtime guards
 * that log via SDL_Log and return safely on overflow/underflow. */
#define FORGE_UI_LAYOUT_MAX_DEPTH  8

/* ── Panel style ───────────────────────────────────────────────────────── */

/* Title bar height and content padding (pixels) */
#define FORGE_UI_PANEL_TITLE_HEIGHT    36.0f
#define FORGE_UI_PANEL_PADDING         12.0f

/* ── Scrollbar style ───────────────────────────────────────────────────── */

#define FORGE_UI_SCROLLBAR_WIDTH        10.0f  /* scrollbar track width (pixels) */
#define FORGE_UI_SCROLLBAR_MIN_THUMB    20.0f  /* minimum thumb height (pixels) */

/* ── Panel content layout ──────────────────────────────────────────────── */

/* Vertical spacing between child widgets inside a panel (pixels) */
#define FORGE_UI_PANEL_CONTENT_SPACING 10.0f

/* ── Scroll speed ──────────────────────────────────────────────────────── */

/* Pixels scrolled per unit of mouse wheel delta */
#define FORGE_UI_SCROLL_SPEED  30.0f

/* ── Separator / tree / sparkline widget constants ─────────────────────── */

#define FORGE_UI_SEPARATOR_THICKNESS   1.0f   /* separator line height (px) */
#define FORGE_UI_TREE_INDICATOR_PAD    4.0f   /* left padding for +/- indicator (unscaled px) */
#define FORGE_UI_TREE_LABEL_OFFSET    18.0f   /* label start offset from left edge (unscaled px) */
#define FORGE_UI_SPARKLINE_LINE_WIDTH  2.0f   /* sparkline line thickness (unscaled px) */
#define FORGE_UI_SPARKLINE_COL_CAP   200      /* max column quads per segment (safety cap) */

/* ── VU meter style ───────────────────────────────────────────────────── */

#define FORGE_UI_VU_GAP              2.0f   /* gap between L/R bars (unscaled px) */
#define FORGE_UI_VU_PEAK_LINE_H      2.0f   /* peak hold indicator height (unscaled px) */
#define FORGE_UI_VU_ZONE_GREEN       0.5f   /* level threshold: green → yellow */
#define FORGE_UI_VU_ZONE_YELLOW      0.8f   /* level threshold: yellow → red */

/* Zone colors (RGB) */
#define FORGE_UI_VU_GREEN_R   0.2f
#define FORGE_UI_VU_GREEN_G   0.8f
#define FORGE_UI_VU_GREEN_B   0.2f
#define FORGE_UI_VU_YELLOW_R  0.9f
#define FORGE_UI_VU_YELLOW_G  0.8f
#define FORGE_UI_VU_YELLOW_B  0.1f
#define FORGE_UI_VU_RED_R     0.9f
#define FORGE_UI_VU_RED_G     0.15f
#define FORGE_UI_VU_RED_B     0.15f
#define FORGE_UI_VU_BG_DIM    0.5f   /* background darkening factor for border color */
#define FORGE_UI_VU_PEAK_MIN  0.0f   /* minimum peak_hold to draw the indicator (0 = hidden) */
#define FORGE_UI_VU_PEAK_ALPHA 0.9f  /* peak hold indicator opacity */

/* ── Drag float/int style ──────────────────────────────────────────────── */

#define FORGE_UI_DRAG_LABEL_GAP        6.0f   /* gap between component label and value field (unscaled px) */

/* Component labels and per-axis colors for drag_float_n / drag_int_n */
#define FORGE_UI_DRAG_COMP_COLOR_R_R   0.85f  /* X-axis red component */
#define FORGE_UI_DRAG_COMP_COLOR_R_G   0.25f
#define FORGE_UI_DRAG_COMP_COLOR_R_B   0.25f
#define FORGE_UI_DRAG_COMP_COLOR_G_R   0.25f  /* Y-axis green component */
#define FORGE_UI_DRAG_COMP_COLOR_G_G   0.75f
#define FORGE_UI_DRAG_COMP_COLOR_G_B   0.25f
#define FORGE_UI_DRAG_COMP_COLOR_B_R   0.30f  /* Z-axis blue component */
#define FORGE_UI_DRAG_COMP_COLOR_B_G   0.45f
#define FORGE_UI_DRAG_COMP_COLOR_B_B   0.90f
#define FORGE_UI_DRAG_COMP_COLOR_W_R   0.85f  /* W-axis yellow component */
#define FORGE_UI_DRAG_COMP_COLOR_W_G   0.80f
#define FORGE_UI_DRAG_COMP_COLOR_W_B   0.20f

/* ── Listbox style ─────────────────────────────────────────────────────── */

#define FORGE_UI_LB_ITEM_HEIGHT       24.0f   /* height per list item (unscaled px) */

/* ── Dropdown style ────────────────────────────────────────────────────── */

#define FORGE_UI_DD_HEADER_HEIGHT     28.0f   /* dropdown header button height (unscaled px) */
#define FORGE_UI_DD_ARROW_PAD          8.0f   /* right-side padding for the arrow indicator (unscaled px) */

/* ── Radio button style ────────────────────────────────────────────────── */

#define FORGE_UI_RADIO_SIZE           16.0f   /* outer box side length (unscaled px) */
#define FORGE_UI_RADIO_INNER_PAD       4.0f   /* padding between outer box and inner fill (unscaled px) */
#define FORGE_UI_RADIO_LABEL_GAP       8.0f   /* gap between indicator box and label text (unscaled px) */

/* ── Color picker style ────────────────────────────────────────────────── */

#define FORGE_UI_CP_HUE_BAR_H        20.0f   /* hue bar height (unscaled px) */
#define FORGE_UI_CP_GAP                6.0f   /* gap between SV area, hue bar, preview (unscaled px) */
#define FORGE_UI_CP_PREVIEW_H         24.0f   /* color preview swatch height (unscaled px) */
#define FORGE_UI_CP_SV_GRID           16      /* grid resolution for SV area (NxN quads) */
#define FORGE_UI_CP_HUE_SEGMENTS       6      /* number of hue gradient segments */
#define FORGE_UI_CP_CURSOR_SIZE        8.0f   /* SV cursor crosshair size (unscaled px) */
#define FORGE_UI_CP_BAR_THICK          2.0f   /* SV crosshair bar thickness (unscaled px) */
#define FORGE_UI_CP_HUE_CURSOR_W       3.0f   /* hue indicator line width (unscaled px) */

/* ── Scaling and spacing ───────────────────────────────────────────────── */

/* Multiply a base spacing value by the context's global scale factor.
 * All dimension-related spacing values are stored unscaled in
 * ForgeUiSpacing; this macro applies the global scale at use time.
 *
 * Example:
 *   float box = FORGE_UI_SCALED(ctx, ctx->spacing.checkbox_box_size);
 *   float pad = FORGE_UI_SCALED(ctx, FORGE_UI_CB_INNER_PAD); */
#define FORGE_UI_SCALED(ctx, value) ((value) * (ctx)->scale)

/* ── Types ──────────────────────────────────────────────────────────────── */

/* A rectangle used for both hit testing and draw emission.  The layout
 * system produces ForgeUiRects, and widget functions consume them --
 * this is the common currency for positioning in the UI. */
typedef struct ForgeUiRect {
    float x;  /* left edge in screen pixels (0 = left of window) */
    float y;  /* top edge in screen pixels (0 = top of window, y increases downward) */
    float w;  /* width in screen pixels (>= 0 for valid rendering) */
    float h;  /* height in screen pixels (>= 0 for valid rendering) */
} ForgeUiRect;

/* Layout direction — determines which axis the cursor advances along
 * and which axis fills the available space. */
typedef enum ForgeUiLayoutDirection {
    FORGE_UI_LAYOUT_VERTICAL,    /* cursor moves downward; widgets get full width */
    FORGE_UI_LAYOUT_HORIZONTAL   /* cursor moves rightward; widgets get full height */
} ForgeUiLayoutDirection;

/* A layout region that positions widgets automatically.
 *
 * A layout defines a rectangular area, a direction (vertical or horizontal),
 * padding (inset from all four edges), spacing (gap between consecutive
 * widgets), and a cursor that advances after each widget is placed.
 *
 * In a vertical layout:
 *   - Each widget gets the full available width (rect.w - 2 * padding)
 *   - The caller specifies the widget height via forge_ui_ctx_layout_next()
 *   - cursor_y advances by (height + spacing) after each widget
 *
 * In a horizontal layout:
 *   - Each widget gets the full available height (rect.h - 2 * padding)
 *   - The caller specifies the widget width via forge_ui_ctx_layout_next()
 *   - cursor_x advances by (width + spacing) after each widget */
typedef struct ForgeUiLayout {
    ForgeUiRect              rect;         /* total layout region */
    ForgeUiLayoutDirection   direction;    /* vertical or horizontal */
    float                    padding;      /* inset from all four edges, in screen pixels (pre-scaled) */
    float                    spacing;      /* gap between consecutive widgets, in screen pixels (pre-scaled) */
    float                    cursor_x;     /* current placement x position in absolute screen pixels */
    float                    cursor_y;     /* current placement y position in absolute screen pixels */
    float                    remaining_w;  /* width left for more widgets, in screen pixels */
    float                    remaining_h;  /* height left for more widgets, in screen pixels */
    int                      item_count;   /* widgets placed so far; spacing is added when item_count > 0 */
} ForgeUiLayout;

/* Spacing constants for consistent widget layout.
 *
 * All values are base (unscaled) floats.  Use FORGE_UI_SCALED(ctx, value)
 * to apply the context's global scale factor at draw time.
 *
 * These defaults are initialized by forge_ui_ctx_init and can be
 * overridden by the application before the first frame.  The original
 * #define constants remain as the default base values; widgets read
 * from this struct (scaled) at draw time rather than the defines. */
typedef struct ForgeUiSpacing {
    float widget_padding;      /* px (unscaled, >0): inset inside widget backgrounds (default 10.0) */
    float item_spacing;        /* px (unscaled, >=0): vertical or horizontal gap between consecutive widgets (default 10.0) */
    float panel_padding;       /* px (unscaled, >=0): inset inside panel content areas (default 12.0) */
    float title_bar_height;    /* px (unscaled, >0): panel/window title bar height (default 36.0) */
    float checkbox_box_size;   /* px (unscaled, >0): checkbox square side length (default 18.0) */
    float slider_thumb_width;  /* px (unscaled, >0): slider thumb rectangle width (default 12.0) */
    float slider_thumb_height; /* px (unscaled, >0): slider thumb rectangle height (default 22.0) */
    float slider_track_height; /* px (unscaled, >0): slider thin track bar height (default 4.0) */
    float text_input_padding;  /* px (unscaled, >=0): left padding before text in text input (default 8.0) */
    float scrollbar_width;     /* px (unscaled, >0): scrollbar track width (default 10.0) */
} ForgeUiSpacing;

/* Panel state used internally by panel_begin/panel_end.
 *
 * A panel is a titled, clipped container that holds child widgets.
 * The caller provides the outer rect and a pointer to their scroll_y;
 * panel_begin computes the content_rect and sets the clip rect;
 * panel_end computes the content_height and draws the scrollbar. */
typedef struct ForgeUiPanel {
    ForgeUiRect  rect;           /* outer bounds (background fill) */
    ForgeUiRect  content_rect;   /* inner bounds after title bar and padding */
    float       *scroll_y;       /* pointer to caller's scroll offset */
    float        content_height; /* total height of child widgets in pixels (set by panel_end; compared against content_rect.h to determine scrollbar need) */
    Uint32       id;             /* widget ID hash for this panel; used to match scroll pre-clamp state across frames */
} ForgeUiPanel;

/* Application-owned text input state.
 *
 * Each text input field needs its own ForgeUiTextInputState that persists
 * across frames.  The application allocates the buffer and sets capacity;
 * the text input widget modifies buffer, length, and cursor each frame
 * based on keyboard input.
 *
 * buffer:   character array (owned by the application, not freed by the library)
 * capacity: total size of buffer in bytes (including space for '\0'); must be > 0
 * length:   current text length in bytes (not counting '\0'); 0 <= length < capacity
 * cursor:   byte index into buffer where the next character will be inserted;
 *           0 <= cursor <= length */
typedef struct ForgeUiTextInputState {
    char *buffer;    /* text buffer (owned by application, null-terminated) */
    int   capacity;  /* total buffer size in bytes (including '\0'); must be > 0 */
    int   length;    /* current text length in bytes; 0 <= length < capacity */
    int   cursor;    /* byte index for insertion point; 0 <= cursor <= length */
} ForgeUiTextInputState;

/* Immediate-mode UI context.
 *
 * Holds per-frame mouse input, the hot/active widget IDs, a pointer to
 * the font atlas (for text and the white pixel), and dynamically growing
 * vertex/index buffers that accumulate draw data during the frame.
 *
 * The hot/active state machine:
 *   - hot:    the widget under the mouse cursor (eligible for click)
 *   - active: the widget currently being pressed (mouse button held)
 *
 * State transitions:
 *   1. At frame start, hot is cleared to FORGE_UI_ID_NONE.
 *   2. Each widget that passes the hit test sets itself as hot (last writer
 *      wins, so draw order determines priority).
 *   3. On mouse press edge (up→down transition): if the mouse is over a
 *      widget (hot), that widget becomes active.  Edge detection prevents
 *      a held mouse dragged onto a button from falsely activating it.
 *   4. On mouse release: if the mouse is still over the active widget,
 *      that's a click.  Active is cleared regardless.
 *   5. Safety valve: if active is set but the mouse is up, active is cleared
 *      in forge_ui_ctx_end — this prevents permanent lockup when an active
 *      widget disappears (is not declared on a subsequent frame). */
typedef struct ForgeUiContext {
    /* Font atlas (not owned -- must outlive the context) */
    const ForgeUiFontAtlas *atlas;

    /* Per-frame input state (set by forge_ui_ctx_begin) */
    float mouse_x;        /* cursor x in screen pixels */
    float mouse_y;        /* cursor y in screen pixels */
    float mouse_x_prev;   /* mouse_x from the previous frame (for drag delta) */
    float mouse_y_prev;   /* mouse_y from the previous frame (for drag delta) */
    bool  mouse_down;     /* true while the primary button is held */
    bool  mouse_down_prev; /* mouse_down from the previous frame (for edge detection) */

    /* Persistent widget state (survives across frames) */
    Uint32 hot;           /* widget under the cursor (or FORGE_UI_ID_NONE) */
    Uint32 active;        /* widget being pressed (or FORGE_UI_ID_NONE) */

    /* Drag-int accumulator — fractional drag distance that has not yet
     * produced a full integer change.  Reset to 0 when the active widget
     * changes; accumulates dx * speed each frame while dragging. */
    float  drag_int_accum;

    /* Hot candidate for this frame.  Widgets write to next_hot during
     * processing (last writer wins = topmost in draw order).  In ctx_end,
     * next_hot is copied to hot -- this two-phase approach prevents hot
     * from flickering mid-frame as widgets are evaluated. */
    Uint32 next_hot;

    /* Focused widget (receives keyboard input).  Only one widget can be
     * focused at a time.  Focus is acquired when a text input is clicked
     * (same press-release-over pattern as button click), and lost by
     * clicking outside any text input or pressing Escape. */
    Uint32 focused;       /* widget receiving keyboard input (or FORGE_UI_ID_NONE) */

    /* Keyboard input state (set each frame via forge_ui_ctx_set_keyboard).
     * These fields are reset to NULL/false at the start of each frame by
     * forge_ui_ctx_begin, then set by the caller before widget calls. */
    const char *text_input;   /* UTF-8 characters typed this frame (NULL if none) */
    bool key_backspace;       /* Backspace pressed this frame */
    bool key_delete;          /* Delete pressed this frame */
    bool key_left;            /* Left arrow pressed this frame */
    bool key_right;           /* Right arrow pressed this frame */
    bool key_home;            /* Home pressed this frame */
    bool key_end;             /* End pressed this frame */
    bool key_escape;          /* Escape pressed this frame */

    /* Internal: tracks whether any text input widget was under the mouse
     * during a press edge this frame.  Used by ctx_end to detect "click
     * outside" for focus loss. */
    bool _ti_press_claimed;

    /* Internal: set by the window system when a window cannot receive
     * input (i.e. another window is on top).  While true, text input
     * widgets skip keyboard processing — the buffer is not modified and
     * the cursor does not move.  Visual state (focused background,
     * cursor caret) is intentionally preserved so the window still
     * *looks* focused even when input is suppressed.  This lets games
     * disable window input for game controls (e.g. FPS camera) without
     * the window appearing unfocused. */
    bool _keyboard_input_suppressed;

    /* Mouse wheel scroll delta for the current frame.  Positive values
     * scroll downward.  Set by the caller after forge_ui_ctx_begin(). */
    float scroll_delta;

    /* Clip rect for panel content area.  When has_clip is true, all
     * vertex-emitting functions clip quads against this rect: fully
     * outside quads are discarded, partially outside quads are trimmed
     * with UV remapping, and hit tests also respect the clip rect. */
    ForgeUiRect clip_rect;
    bool        has_clip;

    /* Internal panel state.  _panel_active is true between panel_begin
     * and panel_end; _panel holds the current panel's geometry and
     * scroll pointer; _panel_content_start_y records the layout cursor
     * at panel_begin so panel_end can compute content_height. */
    bool         _panel_active;
    ForgeUiPanel _panel;
    float        _panel_content_start_y;

    /* Internal tree node state.  _tree_call_depth tracks how many
     * tree_push calls are currently outstanding (awaiting tree_pop).
     * _tree_scope_pushed[i] records whether push_id succeeded for
     * the i-th tree_push call, so tree_pop only calls pop_id when
     * a scope was actually pushed.  This decouples tree nesting from
     * ID stack depth, allowing tree_push/tree_pop to pair correctly
     * even when push_id fails (stack full). */
    int  _tree_call_depth;
    bool _tree_scope_pushed[FORGE_UI_ID_STACK_MAX_DEPTH];

    /* Layout stack — automatic widget positioning.
     *
     * forge_ui_ctx_layout_push() adds a layout region to the stack;
     * forge_ui_ctx_layout_pop() removes it.  While a layout is active,
     * forge_ui_ctx_layout_next() returns the next widget rect from the
     * top-of-stack layout, advancing its cursor.  Nested layouts enable
     * complex arrangements (e.g. a vertical panel with a horizontal row
     * of buttons inside). */
    ForgeUiLayout layout_stack[FORGE_UI_LAYOUT_MAX_DEPTH];
    int           layout_depth;  /* number of active layouts on the stack */

    /* ID seed stack — hierarchical scoping for widget IDs.
     *
     * forge_ui_push_id() hashes a scope name with the current seed and
     * pushes the result.  All subsequent forge_ui_hash_id() calls use
     * the top-of-stack seed, so identically-labeled widgets in different
     * scopes produce different IDs.  Panels and windows push/pop
     * automatically. */
    Uint32 id_seed_stack[FORGE_UI_ID_STACK_MAX_DEPTH];
    int    id_stack_depth;  /* number of active seeds on the stack */

    /* Draw data -- accumulated across all widget calls during a frame.
     * Each widget appends its quads to these buffers.  At frame end the
     * caller uploads the entire batch to the GPU (or software rasterizer)
     * in a single draw call.  Buffers are reset (not freed) each frame
     * by forge_ui_ctx_begin to reuse allocated memory. */
    ForgeUiVertex *vertices;        /* dynamically growing vertex buffer */
    int            vertex_count;    /* number of vertices emitted this frame */
    int            vertex_capacity; /* allocated size of vertex buffer */

    Uint32        *indices;         /* dynamically growing index buffer */
    int            index_count;     /* number of indices emitted this frame */
    int            index_capacity;  /* allocated size of index buffer */

    /* ── Scale and spacing ──────────────────────────────────────────────── */

    /* Global UI scale factor (default 1.0, must be >0 and finite).
     * Multiplies all widget dimensions, font pixel height, padding, and
     * spacing.  Set once before building the atlas; the atlas pixel_height
     * should be base_pixel_height * scale.  The atlas must be rebuilt when
     * scale changes — this is an explicit application responsibility.
     * forge_ui_ctx_begin resets invalid values (<=0, NaN, Inf) to 1.0. */
    float scale;

    /* Unscaled design font size in pixels (e.g. 16.0, must be >0).
     * Stored so the application can rebuild the atlas at a different scale
     * without losing the original design size.  Set by forge_ui_ctx_init
     * from atlas->pixel_height. */
    float base_pixel_height;

    /* base_pixel_height * scale in pixels — precomputed during init.
     * Read-only after init; update by rebuilding the context at the
     * new scale. */
    float scaled_pixel_height;

    /* Base (unscaled) spacing constants.  Initialized with sensible
     * defaults by forge_ui_ctx_init; can be overridden by the application
     * before the first frame for custom themes. */
    ForgeUiSpacing spacing;

    /* Centralized color theme.  All widget colors are derived from this
     * struct at draw time.  Set by forge_ui_ctx_init to the default theme;
     * can be overridden with forge_ui_ctx_set_theme(), which returns false
     * if the context is NULL or any color component is outside [0, 1]. */
    ForgeUiTheme theme;
} ForgeUiContext;

/* ── Public API ─────────────────────────────────────────────────────────── */

/* Initialize a UI context with a font atlas.
 * Allocates initial vertex/index buffers.  Returns true on success. */
static inline bool forge_ui_ctx_init(ForgeUiContext *ctx,
                                     const ForgeUiFontAtlas *atlas);

/* Free vertex/index buffers allocated by forge_ui_ctx_init. */
static inline void forge_ui_ctx_free(ForgeUiContext *ctx);

/* Begin a new frame.  Resets draw buffers and updates input state.
 * Call this once at the start of each frame before any widget calls. */
static inline void forge_ui_ctx_begin(ForgeUiContext *ctx,
                                      float mouse_x, float mouse_y,
                                      bool mouse_down);

/* End the frame.  Finalizes hot/active state transitions.
 * Call this once after all widget calls. */
static inline void forge_ui_ctx_end(ForgeUiContext *ctx);

/* Draw a text label at (x, y) with an explicit color.
 * The y coordinate is the baseline.  Does not participate in hit testing. */
static inline void forge_ui_ctx_label_colored(ForgeUiContext *ctx,
                                               const char *text,
                                               float x, float y,
                                               float r, float g, float b, float a);

/* Draw a text label at (x, y) using the theme's primary text color.
 * The y coordinate is the baseline.  Does not participate in hit testing. */
static inline void forge_ui_ctx_label(ForgeUiContext *ctx,
                                      const char *text,
                                      float x, float y);

/* Draw a button with a background rectangle and centered text label.
 * Returns true on the frame the button is clicked (mouse released over it).
 *
 * text: button label (also used as widget ID via FNV-1a hash).
 *       Use "##suffix" to share display text: "Save##file" displays
 *       "Save" but hashes "##file" for a unique ID.
 * rect: bounding rectangle in screen pixels */
static inline bool forge_ui_ctx_button(ForgeUiContext *ctx,
                                       const char *text,
                                       ForgeUiRect rect);

/* Draw a checkbox with a toggle box and text label.
 * Toggles *value on click (mouse released over the widget).
 * Returns true on the frame the value changes.
 *
 * The checkbox uses the same hot/active state machine as buttons:
 * it becomes hot when the cursor is over the widget rect, active on
 * mouse press, and toggles *value when the mouse is released while
 * still over the widget.
 *
 * Draw elements: an outer box rect (white_uv, color varies by state),
 * a filled inner square when *value is true (accent color), and the
 * label text positioned to the right of the box.
 *
 * label: text drawn to the right of the checkbox box (also used as
 *        widget ID via FNV-1a hash; supports "##" separator)
 * value: pointer to the boolean state (toggled on click)
 * rect:  bounding rectangle for the entire widget (box + label area) */
static inline bool forge_ui_ctx_checkbox(ForgeUiContext *ctx,
                                          const char *label,
                                          bool *value,
                                          ForgeUiRect rect);

/* Draw a horizontal slider with a track and draggable thumb.
 * Updates *value while the slider is being dragged.
 * Returns true on frames where the value changes.
 *
 * The slider introduces drag interaction: when the mouse is pressed on
 * the slider (anywhere on the track or thumb), the slider becomes active
 * and the value snaps to the click position.  While active, the value
 * tracks the mouse x position even if the cursor moves outside the
 * widget bounds.  The value is always clamped to [min_val, max_val].
 *
 * Value mapping (pixel position to user value):
 *   t = clamp((mouse_x - track_x) / track_w, 0, 1)
 *   *value = min_val + t * (max_val - min_val)
 *
 * Inverse mapping (user value to thumb position):
 *   t = (*value - min_val) / (max_val - min_val)
 *   thumb_x = track_x + t * track_w
 *
 * Draw elements: a thin horizontal track rect (white_uv), a thumb rect
 * that slides along the track (white_uv, color varies by state).
 *
 * label:   widget label used as ID via FNV-1a hash (not displayed;
 *          supports "##" separator)
 * value:   pointer to the float value (updated during drag)
 * min_val: minimum value (left edge of track)
 * max_val: maximum value (right edge of track), must be > min_val
 * rect:    bounding rectangle for the slider track/thumb area */
static inline bool forge_ui_ctx_slider(ForgeUiContext *ctx,
                                        const char *label,
                                        float *value,
                                        float min_val, float max_val,
                                        ForgeUiRect rect);

/* Set keyboard input state for this frame.
 * Call after forge_ui_ctx_begin and before any widget calls.
 *
 * text_input:    UTF-8 string of characters typed this frame (NULL if none)
 * key_backspace: true if Backspace was pressed
 * key_delete:    true if Delete was pressed
 * key_left:      true if Left arrow was pressed
 * key_right:     true if Right arrow was pressed
 * key_home:      true if Home was pressed
 * key_end:       true if End was pressed
 * key_escape:    true if Escape was pressed */
static inline void forge_ui_ctx_set_keyboard(ForgeUiContext *ctx,
                                              const char *text_input,
                                              bool key_backspace,
                                              bool key_delete,
                                              bool key_left,
                                              bool key_right,
                                              bool key_home,
                                              bool key_end,
                                              bool key_escape);

/* Draw a single-line text input field with keyboard focus and cursor.
 * Processes keyboard input when this widget has focus (ctx->focused == id).
 * Returns true on frames where the buffer content changes.
 *
 * Focus is acquired by clicking on the text input (press then release
 * while the cursor is still over the widget).  Focus is lost when the
 * user clicks outside any text input or presses Escape.
 *
 * When focused, the widget processes keyboard input from the context:
 *   - text_input: characters are inserted at the cursor position
 *   - Backspace:  deletes the byte before the cursor
 *   - Delete:     deletes the byte at the cursor
 *   - Left/Right: moves the cursor one byte
 *   - Home/End:   jumps to the start/end of the buffer
 *
 * Draw elements: a background rectangle (color varies by state), text
 * quads positioned from the left edge with padding, and a cursor bar
 * (thin 2px-wide rect) whose x position is computed by measuring the
 * substring buffer[0..cursor].
 *
 * label:          widget label used as ID via FNV-1a hash (not displayed;
 *                 supports "##" separator, e.g. "##username")
 * state:          pointer to application-owned ForgeUiTextInputState
 * rect:           bounding rectangle in screen pixels
 * cursor_visible: false to hide the cursor bar (for blink animation) */
static inline bool forge_ui_ctx_text_input(ForgeUiContext *ctx,
                                            const char *label,
                                            ForgeUiTextInputState *state,
                                            ForgeUiRect rect,
                                            bool cursor_visible);

/* ── Layout API ────────────────────────────────────────────────────────── */

/* Push a new layout region onto the stack.
 *
 * rect:      the rectangular area this layout occupies
 * direction: FORGE_UI_LAYOUT_VERTICAL or FORGE_UI_LAYOUT_HORIZONTAL
 * padding:   inset from all four edges of rect (pixels).
 *            0 = use themed default (ctx->spacing.widget_padding * scale).
 *            negative = explicit zero (no padding).
 *            positive = use as-is.
 * spacing:   gap between consecutive widgets (pixels).
 *            0 = use themed default (ctx->spacing.item_spacing * scale).
 *            negative = explicit zero (no spacing).
 *            positive = use as-is.
 *
 * The cursor starts at (rect.x + padding, rect.y + padding).  Available
 * space is (rect.w - 2*padding) wide and (rect.h - 2*padding) tall.
 *
 * Returns true on success, false if ctx is NULL, the stack is full
 * (depth >= FORGE_UI_LAYOUT_MAX_DEPTH), or parameters are invalid. */
static inline bool forge_ui_ctx_layout_push(ForgeUiContext *ctx,
                                             ForgeUiRect rect,
                                             ForgeUiLayoutDirection direction,
                                             float padding,
                                             float spacing);

/* Pop the current layout region and return to the parent layout.
 * Returns true on success, false if the stack is empty or ctx is NULL. */
static inline bool forge_ui_ctx_layout_pop(ForgeUiContext *ctx);

/* Return the next widget rect from the current layout.
 *
 * size: the widget's height (in vertical layout) or width (in horizontal
 *       layout).  The other dimension is filled automatically from the
 *       layout's available space.  Negative sizes are clamped to 0.
 *
 * Returns a ForgeUiRect positioned at the cursor, then advances the
 * cursor by (size + spacing).  Returns a zero rect if no layout is
 * active or ctx is NULL. */
static inline ForgeUiRect forge_ui_ctx_layout_next(ForgeUiContext *ctx,
                                                    float size);

/* Draw a horizontal progress bar showing a filled portion of a track.
 * The bar displays value/max_val as a proportion of the track filled
 * from left to right.  Unlike a slider, the bar is non-interactive —
 * it does not respond to mouse input.
 *
 * Draw elements: a background track rect (border color) and a filled
 * rect overlaid from the left edge (fill_color).  The filled width is
 * proportional to value/max_val.
 *
 * value:      current value (clamped to [0, max_val])
 * max_val:    maximum value (must be > 0)
 * fill_color: RGBA color for the filled portion
 * rect:       bounding rectangle in screen pixels */
static inline void forge_ui_ctx_progress_bar(ForgeUiContext *ctx,
                                              float value,
                                              float max_val,
                                              ForgeUiColor fill_color,
                                              ForgeUiRect rect);

/* ── Layout-aware widget variants ──────────────────────────────────────── */
/* These call forge_ui_ctx_layout_next() internally to obtain their rect,
 * so the caller only specifies content (label, ID, state) and a size.
 * The size parameter means height in vertical layouts or width in
 * horizontal layouts. */

/* Label placed by the current layout with an explicit RGBA text color.
 * size is the widget height (vertical) or width (horizontal). */
static inline void forge_ui_ctx_label_colored_layout(ForgeUiContext *ctx,
                                                      const char *text,
                                                      float size,
                                                      float r, float g, float b, float a);

/* Label placed by the current layout using the theme's primary text color.
 * size is the widget height (vertical) or width (horizontal). */
static inline void forge_ui_ctx_label_layout(ForgeUiContext *ctx,
                                              const char *text,
                                              float size);

/* Button placed by the current layout.  Returns true on click. */
static inline bool forge_ui_ctx_button_layout(ForgeUiContext *ctx,
                                               const char *text,
                                               float size);

/* Checkbox placed by the current layout.  Returns true on toggle. */
static inline bool forge_ui_ctx_checkbox_layout(ForgeUiContext *ctx,
                                                 const char *label,
                                                 bool *value,
                                                 float size);

/* Slider placed by the current layout.  Returns true on value change. */
static inline bool forge_ui_ctx_slider_layout(ForgeUiContext *ctx,
                                               const char *label,
                                               float *value,
                                               float min_val, float max_val,
                                               float size);

/* Progress bar placed by the current layout.  size is the widget height
 * (vertical layout) or width (horizontal layout). */
static inline void forge_ui_ctx_progress_bar_layout(ForgeUiContext *ctx,
                                                     float value,
                                                     float max_val,
                                                     ForgeUiColor fill_color,
                                                     float size);

/* Solid-colored rectangle placed by the current layout.  size is the
 * widget height (vertical layout) or width (horizontal layout). */
static inline void forge_ui_ctx_rect_layout(ForgeUiContext *ctx,
                                             ForgeUiColor color,
                                             float size);

/* ── Separator ─────────────────────────────────────────────────────────── */

/* Draw a thin horizontal divider line spanning the widget rect.
 * Uses the theme's border color.  The line is 1px tall, centered
 * vertically within the rect.  Non-interactive.
 *
 * rect: bounding rectangle — the line spans the full width */
static inline void forge_ui_ctx_separator(ForgeUiContext *ctx,
                                           ForgeUiRect rect);

/* Separator placed by the current layout. */
static inline void forge_ui_ctx_separator_layout(ForgeUiContext *ctx,
                                                  float size);

/* ── Tree node ─────────────────────────────────────────────────────────── */

/* Begin a collapsible tree node with a clickable header.
 *
 * Draws a header row with an expand/collapse indicator (+ or -) and a
 * text label.  Clicking the header toggles *open.  When *open is true,
 * the caller should emit child widgets between tree_push and tree_pop;
 * when false, skip them (tree_pop must still be called).
 *
 * tree_push pushes an ID scope so child widgets get unique IDs.
 *
 * label: header text (also used as widget ID via FNV-1a hash)
 * open:  pointer to caller-owned bool (toggled on click)
 * rect:  bounding rectangle for the header row
 *
 * Returns the current value of *open (true = expanded). */
static inline bool forge_ui_ctx_tree_push(ForgeUiContext *ctx,
                                           const char *label,
                                           bool *open,
                                           ForgeUiRect rect);

/* Layout-aware tree_push.  Returns the current value of *open. */
static inline bool forge_ui_ctx_tree_push_layout(ForgeUiContext *ctx,
                                                  const char *label,
                                                  bool *open,
                                                  float size);

/* End a tree node scope.  Must be called after every tree_push,
 * regardless of the open state.  Pops the ID scope pushed by
 * tree_push. */
static inline void forge_ui_ctx_tree_pop(ForgeUiContext *ctx);

/* ── Sparkline ─────────────────────────────────────────────────────────── */

/* Draw a mini line graph of float values within a rectangle.
 *
 * The sparkline maps each value to a vertical position in [rect.y,
 * rect.y + rect.h] and draws line segments connecting consecutive
 * samples.  Each "line" segment is actually a thin quad (2 triangles)
 * so it renders through the standard vertex/index pipeline.
 *
 * values:     array of float samples (oldest first)
 * count:      number of samples (must be >= 2)
 * min_val:    value mapped to the bottom of the rect
 * max_val:    value mapped to the top of the rect
 * line_color: RGBA color for the line segments
 * rect:       bounding rectangle in screen pixels */
static inline void forge_ui_ctx_sparkline(ForgeUiContext *ctx,
                                           const float *values, int count,
                                           float min_val, float max_val,
                                           ForgeUiColor line_color,
                                           ForgeUiRect rect);

/* Sparkline placed by the current layout. */
static inline void forge_ui_ctx_sparkline_layout(ForgeUiContext *ctx,
                                                  const float *values,
                                                  int count,
                                                  float min_val, float max_val,
                                                  ForgeUiColor line_color,
                                                  float size);

/* ── VU meter ──────────────────────────────────────────────────────────── */

/* Draw a stereo VU meter: two vertical bars (left/right) side by side
 * with a 2px gap.  Each bar fills from the bottom up according to its
 * level value [0..1].  Color zones: green [0..0.5], yellow [0.5..0.8],
 * red [0.8..1.0].  An optional peak hold indicator is drawn as a thin
 * white horizontal line.
 *
 * Non-interactive — no ID, no hit test.
 *
 * level_l:   left channel level [0..1] (clamped)
 * level_r:   right channel level [0..1] (clamped)
 * peak_l:    left peak hold level [0..1] (thin white line, 0 = hidden)
 * peak_r:    right peak hold level [0..1] (thin white line, 0 = hidden)
 * rect:      bounding rectangle in screen pixels */
static inline void forge_ui_ctx_vu_meter(ForgeUiContext *ctx,
                                           float level_l, float level_r,
                                           float peak_l, float peak_r,
                                           ForgeUiRect rect);

/* VU meter placed by the current layout. */
static inline void forge_ui_ctx_vu_meter_layout(ForgeUiContext *ctx,
                                                  float level_l, float level_r,
                                                  float peak_l, float peak_r,
                                                  float size);

/* ── Drag float ────────────────────────────────────────────────────────── */

/* Draw a numeric field that changes value when click-dragged horizontally.
 * The widget shows the current value as text on a colored background.
 * Dragging left decreases the value; dragging right increases it.
 *
 * label:   widget label used as ID (supports "##" separator)
 * value:   pointer to the float value (updated during drag)
 * speed:   value change per pixel of mouse movement (must be > 0)
 * min_val: minimum allowed value
 * max_val: maximum allowed value (must be > min_val)
 * rect:    bounding rectangle in screen pixels
 *
 * Returns true on frames where the value changes. */
static inline bool forge_ui_ctx_drag_float(ForgeUiContext *ctx,
                                            const char *label,
                                            float *value, float speed,
                                            float min_val, float max_val,
                                            ForgeUiRect rect);

/* Drag float placed by the current layout. */
static inline bool forge_ui_ctx_drag_float_layout(ForgeUiContext *ctx,
                                                    const char *label,
                                                    float *value, float speed,
                                                    float min_val, float max_val,
                                                    float size);

/* Draw N drag-float fields side by side in a horizontal row.
 * Each component gets a colored label (X=red, Y=green, Z=blue, W=yellow)
 * followed by a drag field.  Returns true if any component changed.
 *
 * count: number of components (1..4) */
static inline bool forge_ui_ctx_drag_float_n(ForgeUiContext *ctx,
                                              const char *label,
                                              float *values, int count,
                                              float speed,
                                              float min_val, float max_val,
                                              ForgeUiRect rect);

/* Multi-component drag float placed by the current layout. */
static inline bool forge_ui_ctx_drag_float_n_layout(ForgeUiContext *ctx,
                                                      const char *label,
                                                      float *values, int count,
                                                      float speed,
                                                      float min_val, float max_val,
                                                      float size);

/* ── Drag int ──────────────────────────────────────────────────────────── */

/* Same as drag_float but for integer values.  The speed parameter is
 * value-change per pixel of drag (e.g. speed = 0.5 changes the value
 * by 0.5 for every pixel of horizontal mouse movement). */
static inline bool forge_ui_ctx_drag_int(ForgeUiContext *ctx,
                                          const char *label,
                                          int *value, float speed,
                                          int min_val, int max_val,
                                          ForgeUiRect rect);

static inline bool forge_ui_ctx_drag_int_layout(ForgeUiContext *ctx,
                                                  const char *label,
                                                  int *value, float speed,
                                                  int min_val, int max_val,
                                                  float size);

/* Multi-component drag int (1..4 components in a row). */
static inline bool forge_ui_ctx_drag_int_n(ForgeUiContext *ctx,
                                            const char *label,
                                            int *values, int count,
                                            float speed,
                                            int min_val, int max_val,
                                            ForgeUiRect rect);

static inline bool forge_ui_ctx_drag_int_n_layout(ForgeUiContext *ctx,
                                                    const char *label,
                                                    int *values, int count,
                                                    float speed,
                                                    int min_val, int max_val,
                                                    float size);

/* ── Listbox ───────────────────────────────────────────────────────────── */

/* Draw a clipped list of selectable items.  Clicking an item sets
 * *selected to that item's index.  Returns true when selection changes.
 *
 * Items that overflow rect.h are clipped and unreachable.  Callers
 * should size rect.h to fit item_count * FORGE_UI_LB_ITEM_HEIGHT, or
 * limit item_count to the visible range.
 *
 * selected:   pointer to the selected index (-1 for no selection)
 * items:      array of null-terminated strings (item labels)
 * item_count: number of items in the array */
static inline bool forge_ui_ctx_listbox(ForgeUiContext *ctx,
                                         const char *label,
                                         int *selected,
                                         const char *const *items,
                                         int item_count,
                                         ForgeUiRect rect);

static inline bool forge_ui_ctx_listbox_layout(ForgeUiContext *ctx,
                                                const char *label,
                                                int *selected,
                                                const char *const *items,
                                                int item_count,
                                                float size);

/* ── Dropdown ──────────────────────────────────────────────────────────── */

/* Draw a combo box: a header button showing the current selection, and
 * when *open is true, a list of items below it.  Clicking the header
 * toggles *open.  Clicking an item selects it and closes the dropdown.
 *
 * selected:   pointer to the selected index (0-based)
 * open:       pointer to the open/closed state (toggled by header click)
 * items:      array of null-terminated strings (item labels)
 * item_count: number of items
 *
 * Returns true when selection changes.
 *
 * NOTE: When open, the dropdown items extend BELOW the allocated rect.
 * The caller must ensure sufficient space below the widget. */
static inline bool forge_ui_ctx_dropdown(ForgeUiContext *ctx,
                                          const char *label,
                                          int *selected, bool *open,
                                          const char *const *items,
                                          int item_count,
                                          ForgeUiRect rect);

static inline bool forge_ui_ctx_dropdown_layout(ForgeUiContext *ctx,
                                                  const char *label,
                                                  int *selected, bool *open,
                                                  const char *const *items,
                                                  int item_count,
                                                  float size);

/* ── Radio button ──────────────────────────────────────────────────────── */

/* Draw a radio button: an outer box with an inner filled square when
 * selected, plus a label.  Multiple radio buttons sharing the same
 * *selected pointer form a group — clicking one deselects the others.
 * Each radio button has a unique option_value; clicking it sets
 * *selected = option_value.
 *
 * Returns true when selection changes. */
static inline bool forge_ui_ctx_radio(ForgeUiContext *ctx,
                                       const char *label,
                                       int *selected, int option_value,
                                       ForgeUiRect rect);

static inline bool forge_ui_ctx_radio_layout(ForgeUiContext *ctx,
                                               const char *label,
                                               int *selected,
                                               int option_value,
                                               float size);

/* ── Color picker ──────────────────────────────────────────────────────── */

/* Draw an HSV color picker with a saturation-value area, a hue slider
 * bar, and a color preview swatch.  The widget uses HSV coordinates
 * internally: h in [0, 360], s in [0, 1], v in [0, 1].
 *
 * h: pointer to hue (degrees, 0..360)
 * s: pointer to saturation (0..1)
 * v: pointer to value/brightness (0..1)
 *
 * Returns true when the color changes. */
static inline bool forge_ui_ctx_color_picker(ForgeUiContext *ctx,
                                              const char *label,
                                              float *h, float *s, float *v,
                                              ForgeUiRect rect);

static inline bool forge_ui_ctx_color_picker_layout(ForgeUiContext *ctx,
                                                      const char *label,
                                                      float *h, float *s,
                                                      float *v,
                                                      float size);

/* ── HSV/RGB conversion helpers ────────────────────────────────────────── */

/* Convert HSV to RGB.  h is in degrees [0, 360), s and v in [0, 1].
 * Out-of-range h is wrapped; s and v are clamped. */
static inline void forge_ui_hsv_to_rgb(float h, float s, float v,
                                        float *r, float *g, float *b);

/* Convert RGB to HSV.  r, g, b in [0, 1].
 * h is in degrees [0, 360), s and v in [0, 1]. */
static inline void forge_ui_rgb_to_hsv(float r, float g, float b,
                                        float *h, float *s, float *v);

/* ── Panel API ─────────────────────────────────────────────────────────── */

/* Begin a panel: draw background and title bar, set the clip rect, and
 * push a vertical layout for child widgets.  The caller declares widgets
 * between panel_begin and panel_end.
 *
 * title:     text displayed in the title bar (centered); also used as
 *            widget ID via FNV-1a hash and pushes a scope for child IDs
 * rect:      outer bounds of the panel in screen pixels
 * scroll_y:  pointer to the caller's scroll offset (persists across frames)
 *
 * Returns false without drawing if validation fails: NULL ctx/atlas/scroll_y,
 * nested panel (one already active), non-finite rect origin, or
 * non-positive rect dimensions.
 *
 * On false the caller must NOT call panel_end.  In the nested-panel case
 * an outer panel is still active, and calling panel_end would close that
 * outer panel instead of the rejected one.  Only call panel_end after
 * panel_begin returns true. */
static inline bool forge_ui_ctx_panel_begin(ForgeUiContext *ctx,
                                             const char *title,
                                             ForgeUiRect rect,
                                             float *scroll_y);

/* End a panel: compute content_height from how far the layout cursor
 * advanced, pop the layout, clear the clip rect, clamp scroll_y to
 * [0, max_scroll], and draw the scrollbar track and interactive thumb
 * if content overflows the visible area. */
static inline void forge_ui_ctx_panel_end(ForgeUiContext *ctx);

/* ── Internal Helpers ───────────────────────────────────────────────────── */

/* Test whether a point is inside a rectangle. */
static inline bool forge_ui__rect_contains(ForgeUiRect rect,
                                           float px, float py)
{
    return px >= rect.x && px < rect.x + rect.w &&
           py >= rect.y && py < rect.y + rect.h;
}

/* Compute the font's pixel-space ascender for baseline positioning.
 * Returns 0.0f if atlas is NULL or has no em data. */
static inline float forge_ui__ascender_px(const ForgeUiFontAtlas *atlas)
{
    if (!atlas || atlas->units_per_em == 0) return 0.0f;
    if (!forge_isfinite(atlas->pixel_height)) return 0.0f;
    float scale = atlas->pixel_height / (float)atlas->units_per_em;
    return (float)atlas->ascender * scale;
}

/* Test whether the mouse is over a widget, respecting the clip rect.
 * When clipping is active, a widget that has been scrolled out of the
 * visible area must not respond to mouse interaction. */
static inline bool forge_ui__widget_mouse_over(const ForgeUiContext *ctx,
                                                ForgeUiRect rect)
{
    if (!forge_ui__rect_contains(rect, ctx->mouse_x, ctx->mouse_y))
        return false;
    if (ctx->has_clip &&
        !forge_ui__rect_contains(ctx->clip_rect, ctx->mouse_x, ctx->mouse_y))
        return false;
    return true;
}

/* ── ID hashing helpers ─────────────────────────────────────────────────── */

#define FORGE_UI_FNV_OFFSET_BASIS 0x811c9dc5u  /* FNV-1a 32-bit offset basis */
#define FORGE_UI_FNV_PRIME        0x01000193u  /* FNV-1a 32-bit prime        */

/* FNV-1a hash of a null-terminated string, seeded with `seed`.
 * FNV-1a is a fast, non-cryptographic hash with good avalanche properties.
 * The standard FNV offset basis is FORGE_UI_FNV_OFFSET_BASIS; when scoping
 * is active, the caller passes the parent scope's hash as the seed instead. */
static inline Uint32 forge_ui__fnv1a(const char *str, Uint32 seed)
{
    if (!str) return seed;
    Uint32 hash = seed;
    for (const char *p = str; *p != '\0'; p++) {
        hash ^= (Uint32)(unsigned char)*p;
        hash *= FORGE_UI_FNV_PRIME;
    }
    return hash;
}

/* Compute a widget ID by hashing the label with the current scope seed.
 *
 * The "##" separator convention:
 *   - "Save"       → hashes full string "Save"
 *   - "Save##file" → hashes "##file" only (display text is "Save")
 *
 * The result is combined with the current top-of-stack seed so that
 * identically-labeled widgets in different scopes produce different IDs.
 *
 * Zero guard: if the hash equals FORGE_UI_ID_NONE (0), return 1. */
static inline Uint32 forge_ui_hash_id(const ForgeUiContext *ctx,
                                       const char *label)
{
    if (!label || label[0] == '\0') return 1;

    /* Find ## separator — hash the ## portion if present */
    const char *sep = SDL_strstr(label, "##");
    const char *id_str = sep ? sep : label;

    /* Use top-of-stack seed, or FNV offset basis if stack is empty */
    Uint32 seed = FORGE_UI_FNV_OFFSET_BASIS;
    if (ctx && ctx->id_stack_depth > 0) {
        seed = ctx->id_seed_stack[ctx->id_stack_depth - 1];
    }

    Uint32 hash = forge_ui__fnv1a(id_str, seed);
    if (hash == FORGE_UI_ID_NONE) hash = 1;
    return hash;
}

/* Return a pointer to the end of display text in a label.
 * If the label contains "##", returns a pointer to the "##".
 * Otherwise returns a pointer to the null terminator.
 * The caller can compute display length as (result - label). */
static inline const char *forge_ui__display_end(const char *label)
{
    if (!label) return label;
    const char *sep = SDL_strstr(label, "##");
    return sep ? sep : (label + SDL_strlen(label));
}

/* Push a named scope onto the ID seed stack.  All subsequent hash_id
 * calls will incorporate this scope's seed, so identically-labeled
 * widgets in different scopes produce different IDs.
 *
 * Panels and windows call this automatically; callers use it to
 * disambiguate repeated widget groups (e.g. list items). */
static inline bool forge_ui_push_id(ForgeUiContext *ctx, const char *name)
{
    if (!ctx) return false;
    if (ctx->id_stack_depth >= FORGE_UI_ID_STACK_MAX_DEPTH) {
        SDL_Log("forge_ui_push_id: stack overflow (depth=%d, max=%d)",
                ctx->id_stack_depth, FORGE_UI_ID_STACK_MAX_DEPTH);
        return false;
    }

    /* Hash the scope name with the current seed to produce the new seed */
    Uint32 parent_seed = FORGE_UI_FNV_OFFSET_BASIS;
    if (ctx->id_stack_depth > 0) {
        parent_seed = ctx->id_seed_stack[ctx->id_stack_depth - 1];
    }

    const char *scope_name = (name && name[0] != '\0') ? name : "";
    /* Apply the same ## identity extraction as forge_ui_hash_id:
     * if "Label##id" is passed, hash only "##id" so scope seeds
     * remain stable when only display text changes. */
    const char *sep = SDL_strstr(scope_name, "##");
    if (sep) scope_name = sep;
    if (scope_name[0] == '\0') {
        SDL_Log("forge_ui_push_id: empty scope name has no effect on IDs");
    }
    ctx->id_seed_stack[ctx->id_stack_depth] =
        forge_ui__fnv1a(scope_name, parent_seed);
    ctx->id_stack_depth++;
    return true;
}

/* Pop the current scope from the ID seed stack. */
static inline void forge_ui_pop_id(ForgeUiContext *ctx)
{
    if (!ctx) return;
    if (ctx->id_stack_depth <= 0) {
        SDL_Log("forge_ui_pop_id: stack underflow (depth=%d)",
                ctx->id_stack_depth);
        return;
    }
    ctx->id_stack_depth--;
}

/* Pop the ID scope only if push_id succeeded.  Useful for
 * peek-ahead patterns where push_id may or may not succeed:
 *     bool pushed = forge_ui_push_id(ctx, label);
 *     Uint32 id = forge_ui_hash_id(ctx, "##sub");
 *     forge_ui_pop_id_if(ctx, pushed);
 *
 * For compound widgets, prefer the early-abort pattern instead:
 *     if (!forge_ui_push_id(ctx, label)) return false;
 *     ... child widgets ...
 *     forge_ui_pop_id(ctx);                                      */
static inline void forge_ui_pop_id_if(ForgeUiContext *ctx, bool pushed)
{
    if (pushed) forge_ui_pop_id(ctx);
}

/* Ensure vertex buffer has room for `count` more vertices. */
static inline bool forge_ui__grow_vertices(ForgeUiContext *ctx, int count)
{
    if (count <= 0) return count == 0;

    /* Guard against signed integer overflow in the addition */
    if (ctx->vertex_count > INT_MAX - count) {
        SDL_Log("forge_ui__grow_vertices: count overflow");
        return false;
    }
    int needed = ctx->vertex_count + count;
    if (needed <= ctx->vertex_capacity) return true;

    /* Start from at least the initial capacity (handles zero after free) */
    int new_cap = ctx->vertex_capacity;
    if (new_cap == 0) new_cap = FORGE_UI_CTX_INITIAL_VERTEX_CAPACITY;

    while (new_cap < needed) {
        if (new_cap > INT_MAX / 2) {
            SDL_Log("forge_ui__grow_vertices: capacity overflow");
            return false;
        }
        new_cap *= 2;
    }

    ForgeUiVertex *new_buf = (ForgeUiVertex *)SDL_realloc(
        ctx->vertices, (size_t)new_cap * sizeof(ForgeUiVertex));
    if (!new_buf) {
        SDL_Log("forge_ui__grow_vertices: realloc failed (%d vertices)",
                new_cap);
        return false;
    }
    ctx->vertices = new_buf;
    ctx->vertex_capacity = new_cap;
    return true;
}

/* Ensure index buffer has room for `count` more indices. */
static inline bool forge_ui__grow_indices(ForgeUiContext *ctx, int count)
{
    if (count <= 0) return count == 0;

    /* Guard against signed integer overflow in the addition */
    if (ctx->index_count > INT_MAX - count) {
        SDL_Log("forge_ui__grow_indices: count overflow");
        return false;
    }
    int needed = ctx->index_count + count;
    if (needed <= ctx->index_capacity) return true;

    /* Start from at least the initial capacity (handles zero after free) */
    int new_cap = ctx->index_capacity;
    if (new_cap == 0) new_cap = FORGE_UI_CTX_INITIAL_INDEX_CAPACITY;

    while (new_cap < needed) {
        if (new_cap > INT_MAX / 2) {
            SDL_Log("forge_ui__grow_indices: capacity overflow");
            return false;
        }
        new_cap *= 2;
    }

    Uint32 *new_buf = (Uint32 *)SDL_realloc(
        ctx->indices, (size_t)new_cap * sizeof(Uint32));
    if (!new_buf) {
        SDL_Log("forge_ui__grow_indices: realloc failed (%d indices)",
                new_cap);
        return false;
    }
    ctx->indices = new_buf;
    ctx->index_capacity = new_cap;
    return true;
}

/* Emit a solid-colored rectangle using 4 vertices and 6 indices.
 * Samples the atlas white_uv region so the texture multiplier is 1.0,
 * giving a flat color determined entirely by the vertex color. */
static inline void forge_ui__emit_rect(ForgeUiContext *ctx,
                                       ForgeUiRect rect,
                                       float r, float g, float b, float a)
{
    if (!ctx->atlas) return;

    /* ── Clip against clip_rect if active ────────────────────────────── */
    if (ctx->has_clip) {
        float cx0 = ctx->clip_rect.x;
        float cy0 = ctx->clip_rect.y;
        float cx1 = cx0 + ctx->clip_rect.w;
        float cy1 = cy0 + ctx->clip_rect.h;

        float rx0 = rect.x;
        float ry0 = rect.y;
        float rx1 = rect.x + rect.w;
        float ry1 = rect.y + rect.h;

        /* Fully outside -- discard */
        if (rx1 <= cx0 || rx0 >= cx1 || ry1 <= cy0 || ry0 >= cy1) return;

        /* Trim to intersection (solid rects use a single UV point so
         * only positions change -- no UV remapping needed) */
        if (rx0 < cx0) rx0 = cx0;
        if (ry0 < cy0) ry0 = cy0;
        if (rx1 > cx1) rx1 = cx1;
        if (ry1 > cy1) ry1 = cy1;
        rect.x = rx0;
        rect.y = ry0;
        rect.w = rx1 - rx0;
        rect.h = ry1 - ry0;

        /* Discard degenerate (zero-area) intersections that can arise
         * from edge-touching rects or zero-size clip rects. */
        if (rect.w <= 0.0f || rect.h <= 0.0f) return;
    }

    if (!forge_ui__grow_vertices(ctx, 4)) return;
    if (!forge_ui__grow_indices(ctx, 6)) return;

    /* UV coordinates: center of the white pixel region to ensure we sample
     * pure white (coverage = 255).  Using the midpoint avoids edge texels. */
    const ForgeUiUVRect *wuv = &ctx->atlas->white_uv;
    float u = (wuv->u0 + wuv->u1) * 0.5f;
    float v = (wuv->v0 + wuv->v1) * 0.5f;

    Uint32 base = (Uint32)ctx->vertex_count;

    /* Quad corners: top-left, top-right, bottom-right, bottom-left */
    ForgeUiVertex *verts = &ctx->vertices[ctx->vertex_count];
    verts[0] = (ForgeUiVertex){ rect.x,          rect.y,          u, v, r, g, b, a };
    verts[1] = (ForgeUiVertex){ rect.x + rect.w, rect.y,          u, v, r, g, b, a };
    verts[2] = (ForgeUiVertex){ rect.x + rect.w, rect.y + rect.h, u, v, r, g, b, a };
    verts[3] = (ForgeUiVertex){ rect.x,          rect.y + rect.h, u, v, r, g, b, a };
    ctx->vertex_count += 4;

    /* Two CCW triangles: (0,1,2) and (0,2,3) */
    Uint32 *idx = &ctx->indices[ctx->index_count];
    idx[0] = base + 0;  idx[1] = base + 1;  idx[2] = base + 2;
    idx[3] = base + 0;  idx[4] = base + 2;  idx[5] = base + 3;
    ctx->index_count += 6;
}

/* Emit a single clipped quad (4 vertices, 6 indices) with UV remapping.
 * The quad is defined by the first 4 vertices at src[0..3] in the order:
 * top-left, top-right, bottom-right, bottom-left.  Positions and UVs are
 * clipped proportionally so the visible portion samples the correct region
 * of the font atlas texture. */
static inline void forge_ui__emit_quad_clipped(ForgeUiContext *ctx,
                                                const ForgeUiVertex *src,
                                                const ForgeUiRect *clip)
{
    /* Original quad bounds */
    float x0 = src[0].pos_x, x1 = src[1].pos_x;
    float y0 = src[0].pos_y, y1 = src[2].pos_y;

    float cx0 = clip->x, cy0 = clip->y;
    float cx1 = cx0 + clip->w, cy1 = cy0 + clip->h;

    /* Fully outside or degenerate (zero area) -- discard */
    if (x1 <= cx0 || x0 >= cx1 || y1 <= cy0 || y0 >= cy1) return;
    if (x0 >= x1 || y0 >= y1) return;

    /* Compute clipped bounds */
    float nx0 = (x0 < cx0) ? cx0 : x0;
    float ny0 = (y0 < cy0) ? cy0 : y0;
    float nx1 = (x1 > cx1) ? cx1 : x1;
    float ny1 = (y1 > cy1) ? cy1 : y1;

    /* Discard degenerate (zero-area) intersections that can arise
     * from edge-touching quads or zero-size clip rects. */
    if (nx1 <= nx0 || ny1 <= ny0) return;

    /* Proportional UV remapping */
    float u0 = src[0].uv_u, u1 = src[1].uv_u;
    float v0 = src[0].uv_v, v1 = src[2].uv_v;

    float inv_w = (x1 != x0) ? 1.0f / (x1 - x0) : 0.0f;
    float inv_h = (y1 != y0) ? 1.0f / (y1 - y0) : 0.0f;

    float nu0 = u0 + (u1 - u0) * (nx0 - x0) * inv_w;
    float nu1 = u0 + (u1 - u0) * (nx1 - x0) * inv_w;
    float nv0 = v0 + (v1 - v0) * (ny0 - y0) * inv_h;
    float nv1 = v0 + (v1 - v0) * (ny1 - y0) * inv_h;

    if (!forge_ui__grow_vertices(ctx, 4)) return;
    if (!forge_ui__grow_indices(ctx, 6)) return;

    Uint32 base = (Uint32)ctx->vertex_count;
    float r = src[0].r, g = src[0].g, b = src[0].b, a = src[0].a;

    ForgeUiVertex *v = &ctx->vertices[ctx->vertex_count];
    v[0] = (ForgeUiVertex){ nx0, ny0, nu0, nv0, r, g, b, a };
    v[1] = (ForgeUiVertex){ nx1, ny0, nu1, nv0, r, g, b, a };
    v[2] = (ForgeUiVertex){ nx1, ny1, nu1, nv1, r, g, b, a };
    v[3] = (ForgeUiVertex){ nx0, ny1, nu0, nv1, r, g, b, a };
    ctx->vertex_count += 4;

    Uint32 *idx = &ctx->indices[ctx->index_count];
    idx[0] = base + 0;  idx[1] = base + 1;  idx[2] = base + 2;
    idx[3] = base + 0;  idx[4] = base + 2;  idx[5] = base + 3;
    ctx->index_count += 6;
}

/* Append vertices and indices from a text layout into the context's
 * draw buffers.  When clipping is active, processes each glyph quad
 * individually with UV remapping; otherwise bulk-copies all data. */
static inline void forge_ui__emit_text_layout(ForgeUiContext *ctx,
                                              const ForgeUiTextLayout *layout)
{
    if (!layout || layout->vertex_count == 0 || !layout->vertices || !layout->indices) return;

    /* ── Per-quad clipping path (when has_clip is true) ──────────────── */
    if (ctx->has_clip) {
        /* Each glyph is a quad: 4 vertices, 6 indices.  Iterate per-quad
         * and clip individually with UV remapping. */
        int quad_count = layout->vertex_count / 4;
        for (int q = 0; q < quad_count; q++) {
            forge_ui__emit_quad_clipped(ctx,
                                         &layout->vertices[q * 4],
                                         &ctx->clip_rect);
        }
        return;
    }

    /* ── Bulk copy path (no clipping) ────────────────────────────────── */
    if (!forge_ui__grow_vertices(ctx, layout->vertex_count)) return;
    if (!forge_ui__grow_indices(ctx, layout->index_count)) return;

    Uint32 base = (Uint32)ctx->vertex_count;

    /* Copy vertices into the shared buffer without modification --
     * vertex positions are absolute screen coordinates. */
    SDL_memcpy(&ctx->vertices[ctx->vertex_count],
               layout->vertices,
               (size_t)layout->vertex_count * sizeof(ForgeUiVertex));
    ctx->vertex_count += layout->vertex_count;

    /* Rebase indices by the current vertex count so they reference the
     * correct positions in the shared vertex buffer (text layouts produce
     * indices starting from zero). */
    for (int i = 0; i < layout->index_count; i++) {
        ctx->indices[ctx->index_count + i] = layout->indices[i] + base;
    }
    ctx->index_count += layout->index_count;
}

/* Emit a rectangular border as four thin edge rects drawn INSIDE the
 * given rectangle.  Used for the focused text input outline. */
static inline void forge_ui__emit_border(ForgeUiContext *ctx,
                                          ForgeUiRect rect,
                                          float border_w,
                                          float r, float g, float b, float a)
{
    if (!ctx) return;
    /* Reject degenerate borders: width must be positive, finite, and must
     * fit within half the rect dimension to avoid inverted geometry.
     * NaN fails the > 0 check (NaN > 0 is false). */
    if (!(border_w > 0.0f)) return;
    if (border_w > rect.w * 0.5f || border_w > rect.h * 0.5f) return;

    /* Top edge — full width so corners are covered by the horizontal edges */
    forge_ui__emit_rect(ctx,
        (ForgeUiRect){ rect.x, rect.y, rect.w, border_w },
        r, g, b, a);
    /* Bottom edge — full width, same reason */
    forge_ui__emit_rect(ctx,
        (ForgeUiRect){ rect.x, rect.y + rect.h - border_w, rect.w, border_w },
        r, g, b, a);
    /* Left edge — shortened to avoid double-drawing corner pixels where
     * it would overlap the top and bottom edges */
    forge_ui__emit_rect(ctx,
        (ForgeUiRect){ rect.x, rect.y + border_w,
                       border_w, rect.h - 2.0f * border_w },
        r, g, b, a);
    /* Right edge — shortened for the same corner overlap reason */
    forge_ui__emit_rect(ctx,
        (ForgeUiRect){ rect.x + rect.w - border_w, rect.y + border_w,
                       border_w, rect.h - 2.0f * border_w },
        r, g, b, a);
}

/* ── Implementation ─────────────────────────────────────────────────────── */

static inline bool forge_ui_ctx_init(ForgeUiContext *ctx,
                                     const ForgeUiFontAtlas *atlas)
{
    if (!ctx || !atlas) {
        SDL_Log("forge_ui_ctx_init: NULL argument");
        return false;
    }

    SDL_memset(ctx, 0, sizeof(*ctx));
    ctx->atlas = atlas;
    ctx->hot = FORGE_UI_ID_NONE;
    ctx->active = FORGE_UI_ID_NONE;
    ctx->next_hot = FORGE_UI_ID_NONE;
    ctx->focused = FORGE_UI_ID_NONE;

    ctx->vertex_capacity = FORGE_UI_CTX_INITIAL_VERTEX_CAPACITY;
    ctx->vertices = (ForgeUiVertex *)SDL_calloc(
        (size_t)ctx->vertex_capacity, sizeof(ForgeUiVertex));
    if (!ctx->vertices) {
        SDL_Log("forge_ui_ctx_init: vertex allocation failed");
        return false;
    }

    ctx->index_capacity = FORGE_UI_CTX_INITIAL_INDEX_CAPACITY;
    ctx->indices = (Uint32 *)SDL_calloc(
        (size_t)ctx->index_capacity, sizeof(Uint32));
    if (!ctx->indices) {
        SDL_Log("forge_ui_ctx_init: index allocation failed");
        SDL_free(ctx->vertices);
        ctx->vertices = NULL;
        return false;
    }

    /* Validate atlas pixel_height: zero, negative, NaN, or Inf would
     * propagate through all text positioning via forge_ui__ascender_px. */
    if (!forge_isfinite(atlas->pixel_height) || atlas->pixel_height <= 0.0f) {
        SDL_Log("forge_ui_ctx_init: atlas pixel_height must be positive "
                "and finite (got %.2f)", (double)atlas->pixel_height);
        SDL_free(ctx->indices);
        ctx->indices = NULL;
        SDL_free(ctx->vertices);
        ctx->vertices = NULL;
        return false;
    }

    /* Scale defaults to 1.0 (no scaling).  The application can set
     * ctx->scale before the first frame to apply global DPI scaling.
     * The atlas must be built at base_pixel_height * scale. */
    ctx->scale = 1.0f;
    ctx->base_pixel_height = atlas->pixel_height;
    ctx->scaled_pixel_height = atlas->pixel_height;

    /* Default spacing values (unscaled).  These match the original
     * hardcoded defines so existing applications look identical at
     * scale 1.0. */
    ctx->spacing.widget_padding      = FORGE_UI_WIDGET_PADDING;         /* 10.0 */
    ctx->spacing.item_spacing        = FORGE_UI_PANEL_CONTENT_SPACING; /* 10.0 */
    ctx->spacing.panel_padding       = FORGE_UI_PANEL_PADDING;         /* 12.0 */
    ctx->spacing.title_bar_height    = FORGE_UI_PANEL_TITLE_HEIGHT;    /* 36.0 */
    ctx->spacing.checkbox_box_size   = FORGE_UI_CB_BOX_SIZE;           /* 18.0 */
    ctx->spacing.slider_thumb_width  = FORGE_UI_SL_THUMB_WIDTH;        /* 12.0 */
    ctx->spacing.slider_thumb_height = FORGE_UI_SL_THUMB_HEIGHT;       /* 22.0 */
    ctx->spacing.slider_track_height = FORGE_UI_SL_TRACK_HEIGHT;       /* 4.0 */
    ctx->spacing.text_input_padding  = FORGE_UI_TI_PADDING;            /* 8.0 */
    ctx->spacing.scrollbar_width     = FORGE_UI_SCROLLBAR_WIDTH;       /* 10.0 */

    /* Default theme — canonical dark palette from the diagram STYLE dict */
    ctx->theme = forge_ui_theme_default();

    return true;
}

static inline void forge_ui_ctx_free(ForgeUiContext *ctx)
{
    if (!ctx) return;
    SDL_free(ctx->vertices);
    SDL_free(ctx->indices);
    ctx->vertices = NULL;
    ctx->indices = NULL;
    ctx->atlas = NULL;
    ctx->vertex_count = 0;
    ctx->index_count = 0;
    ctx->vertex_capacity = 0;
    ctx->index_capacity = 0;
    ctx->hot = FORGE_UI_ID_NONE;
    ctx->active = FORGE_UI_ID_NONE;
    ctx->next_hot = FORGE_UI_ID_NONE;
    ctx->focused = FORGE_UI_ID_NONE;
    ctx->layout_depth = 0;
    ctx->id_stack_depth = 0;
    ctx->scroll_delta = 0.0f;
    ctx->has_clip = false;
    ctx->_panel_active = false;
    ctx->_tree_call_depth = 0;
    SDL_memset(&ctx->_panel, 0, sizeof(ctx->_panel));
    ctx->_panel_content_start_y = 0.0f;
}

static inline void forge_ui_ctx_begin(ForgeUiContext *ctx,
                                      float mouse_x, float mouse_y,
                                      bool mouse_down)
{
    if (!ctx) return;

    /* Track the previous frame's mouse state for edge detection */
    ctx->mouse_down_prev = ctx->mouse_down;

    /* Save previous mouse position for drag delta computation.
     * Drag widgets (drag_float, color_picker) use the difference
     * (mouse_x - mouse_x_prev) to determine value changes. */
    ctx->mouse_x_prev = ctx->mouse_x;
    ctx->mouse_y_prev = ctx->mouse_y;

    /* Snapshot the mouse state for this frame.  All widget calls see the
     * same position and button state, ensuring consistent hit-testing even
     * if the OS delivers new input events between widget calls.
     * Clamp NaN/Inf to 0 so downstream slider and scrollbar drag
     * calculations never produce NaN values written to caller data. */
    ctx->mouse_x = forge_isfinite(mouse_x) ? mouse_x : 0.0f;
    ctx->mouse_y = forge_isfinite(mouse_y) ? mouse_y : 0.0f;
    ctx->mouse_down = mouse_down;

    /* Reset hot for this frame -- widgets will claim it during processing */
    ctx->next_hot = FORGE_UI_ID_NONE;

    /* Reset keyboard input state for this frame.  The caller sets these
     * via forge_ui_ctx_set_keyboard after calling begin. */
    ctx->text_input = NULL;
    ctx->key_backspace = false;
    ctx->key_delete = false;
    ctx->key_left = false;
    ctx->key_right = false;
    ctx->key_home = false;
    ctx->key_end = false;
    ctx->key_escape = false;
    ctx->_ti_press_claimed = false;
    /* Reset suppression so widgets outside windows (or before the first
     * window_begin call) can receive keyboard input normally. */
    ctx->_keyboard_input_suppressed = false;

    /* Validate scale: NaN, Inf, zero, or negative would corrupt all
     * widget dimensions through FORGE_UI_SCALED.  Clamp to 1.0. */
    if (!forge_isfinite(ctx->scale) || ctx->scale <= 0.0f) {
        SDL_Log("forge_ui_ctx_begin: invalid scale %.4f, resetting to 1.0",
                (double)ctx->scale);
        ctx->scale = 1.0f;
    }

    /* Validate spacing fields: NaN, Inf, or negative values would produce
     * corrupt geometry (inverted rects, infinite positions).  Clamp each
     * field to its compiled default.  This catches accidental corruption
     * from uninitialised memory or bad arithmetic. */
    {
        struct { float *field; float fallback; const char *name; } checks[] = {
            { &ctx->spacing.widget_padding,    FORGE_UI_WIDGET_PADDING,        "widget_padding" },
            { &ctx->spacing.item_spacing,      FORGE_UI_PANEL_CONTENT_SPACING, "item_spacing" },
            { &ctx->spacing.panel_padding,     FORGE_UI_PANEL_PADDING,         "panel_padding" },
            { &ctx->spacing.title_bar_height,  FORGE_UI_PANEL_TITLE_HEIGHT,    "title_bar_height" },
            { &ctx->spacing.checkbox_box_size, FORGE_UI_CB_BOX_SIZE,           "checkbox_box_size" },
            { &ctx->spacing.slider_thumb_width,  FORGE_UI_SL_THUMB_WIDTH,      "slider_thumb_width" },
            { &ctx->spacing.slider_thumb_height, FORGE_UI_SL_THUMB_HEIGHT,     "slider_thumb_height" },
            { &ctx->spacing.slider_track_height, FORGE_UI_SL_TRACK_HEIGHT,     "slider_track_height" },
            { &ctx->spacing.text_input_padding,  FORGE_UI_TI_PADDING,          "text_input_padding" },
            { &ctx->spacing.scrollbar_width,     FORGE_UI_SCROLLBAR_WIDTH,     "scrollbar_width" },
        };
        for (int i = 0; i < (int)(sizeof(checks) / sizeof(checks[0])); i++) {
            if (!forge_isfinite(*checks[i].field) || *checks[i].field < 0.0f) {
                SDL_Log("forge_ui_ctx_begin: invalid spacing.%s = %.4f, "
                        "resetting to %.1f",
                        checks[i].name, (double)*checks[i].field,
                        (double)checks[i].fallback);
                *checks[i].field = checks[i].fallback;
            }
        }
    }

    /* Reset scroll and panel state for this frame.  The caller sets
     * scroll_delta after begin if mouse wheel input is available. */
    ctx->scroll_delta = 0.0f;
    ctx->has_clip = false;
    ctx->_panel_active = false;
    ctx->_tree_call_depth = 0;
    ctx->_panel.scroll_y = NULL;
    SDL_memset(&ctx->_panel.rect, 0, sizeof(ctx->_panel.rect));
    SDL_memset(&ctx->_panel.content_rect, 0, sizeof(ctx->_panel.content_rect));

    /* Reset layout stack for this frame.  Warn if the previous frame had
     * unmatched push/pop calls -- this is a programming error. */
    if (ctx->layout_depth != 0) {
        SDL_Log("forge_ui_ctx_begin: layout_depth=%d at frame start "
                "(unmatched push/pop last frame)", ctx->layout_depth);
    }
    ctx->layout_depth = 0;

    /* Reset ID seed stack for this frame. */
    if (ctx->id_stack_depth != 0) {
        SDL_Log("forge_ui_ctx_begin: id_stack_depth=%d at frame start "
                "(unmatched push_id/pop_id last frame)", ctx->id_stack_depth);
    }
    ctx->id_stack_depth = 0;

    /* Reset draw buffers (keep allocated memory) */
    ctx->vertex_count = 0;
    ctx->index_count = 0;
}

static inline void forge_ui_ctx_end(ForgeUiContext *ctx)
{
    if (!ctx) return;

    /* Safety valve: if a widget was active but the mouse is no longer held,
     * clear active.  This handles the case where an active widget disappears
     * (is not declared) on a subsequent frame -- without this, active would
     * remain stuck forever, blocking all other widgets. */
    if (ctx->active != FORGE_UI_ID_NONE && !ctx->mouse_down) {
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* Focus management: clear focused widget on click-outside or Escape.
     *
     * Click-outside: if the mouse was just pressed (edge) this frame and
     * no text input widget was under the cursor (_ti_press_claimed is
     * false), the user clicked outside all text inputs.  This unfocuses
     * the currently focused widget.
     *
     * Escape: always clears focus regardless of mouse state. */
    {
        bool pressed = ctx->mouse_down && !ctx->mouse_down_prev;
        if (pressed && !ctx->_ti_press_claimed) {
            ctx->focused = FORGE_UI_ID_NONE;
        }
        if (ctx->key_escape) {
            ctx->focused = FORGE_UI_ID_NONE;
            /* Clear active to prevent a pending click release from
             * re-acquiring focus on the next frame. */
            ctx->active = FORGE_UI_ID_NONE;
        }
    }

    /* Finalize hot state: adopt whatever widget claimed hot this frame.
     * If no widget claimed hot and nothing is active, hot stays NONE.
     * If a widget is active (being pressed), we don't change hot until
     * the mouse is released -- this prevents "losing" the active widget
     * if the cursor slides off during a press. */
    if (ctx->active == FORGE_UI_ID_NONE) {
        ctx->hot = ctx->next_hot;
    }

    /* Safety net: if a panel was opened but never closed, clean up the
     * panel state and pop its layout so the damage is confined to this
     * frame rather than leaking into the next one.  We also clear the
     * panel identity fields so the pre-clamp check on the next frame
     * cannot match against a panel that was never properly closed. */
    if (ctx->_panel_active) {
        SDL_Log("forge_ui_ctx_end: panel still active (missing panel_end call)");
        ctx->has_clip = false;
        ctx->_panel_active = false;
        ctx->_panel.id = FORGE_UI_ID_NONE;
        ctx->_panel.scroll_y = NULL;
        ctx->_panel.content_height = 0.0f;
        if (ctx->layout_depth > 0) {
            forge_ui_ctx_layout_pop(ctx);
        }
        if (ctx->id_stack_depth > 0) {
            forge_ui_pop_id(ctx);
        }
    }

    /* Check for unmatched layout push/pop.  A non-zero depth here means
     * the caller forgot one or more layout_pop calls this frame. */
    if (ctx->layout_depth != 0) {
        SDL_Log("forge_ui_ctx_end: layout_depth=%d (missing %d pop call%s)",
                ctx->layout_depth, ctx->layout_depth,
                ctx->layout_depth == 1 ? "" : "s");
    }
}

static inline void forge_ui_ctx_label_colored(ForgeUiContext *ctx,
                                               const char *text,
                                               float x, float y,
                                               float r, float g, float b, float a)
{
    if (!ctx || !text || !ctx->atlas) return;
    if (!forge_isfinite(x) || !forge_isfinite(y)) return;
    if (!forge_isfinite(r) || !forge_isfinite(g) || !forge_isfinite(b) || !forge_isfinite(a)) return;

    /* Clamp to valid [0, 1] range to prevent invalid vertex color data */
    if (r < 0.0f) r = 0.0f; else if (r > 1.0f) r = 1.0f;
    if (g < 0.0f) g = 0.0f; else if (g > 1.0f) g = 1.0f;
    if (b < 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f;
    if (a < 0.0f) a = 0.0f; else if (a > 1.0f) a = 1.0f;

    ForgeUiTextOpts opts = { 0.0f, FORGE_UI_TEXT_ALIGN_LEFT, r, g, b, a };
    ForgeUiTextLayout layout;
    if (forge_ui_text_layout(ctx->atlas, text, x, y, &opts, &layout)) {
        forge_ui__emit_text_layout(ctx, &layout);
        forge_ui_text_layout_free(&layout);
    }
}

static inline void forge_ui_ctx_label(ForgeUiContext *ctx,
                                      const char *text,
                                      float x, float y)
{
    if (!ctx) return;
    forge_ui_ctx_label_colored(ctx, text, x, y,
                               ctx->theme.text.r, ctx->theme.text.g,
                               ctx->theme.text.b, ctx->theme.text.a);
}

/* ── Internal: interaction-state color selection ─────────────────────── */

/* Return the surface color for a widget's current interaction state.
 * Centralizes the active/hot/normal branching repeated in button,
 * checkbox, and text input widgets. */
static inline ForgeUiColor forge_ui__surface_color(const ForgeUiContext *ctx,
                                                    Uint32 id)
{
    if (ctx->active == id) return ctx->theme.surface_active;
    if (ctx->hot    == id) return ctx->theme.surface_hot;
    return ctx->theme.surface;
}

/* Return the accent color for a widget's current interaction state.
 * Used by slider thumbs and scrollbar thumbs where the color series
 * is accent (active) → accent_hot (hovered) → surface_hot (idle). */
static inline ForgeUiColor forge_ui__accent_color(const ForgeUiContext *ctx,
                                                   Uint32 id)
{
    if (ctx->active == id) return ctx->theme.accent;
    if (ctx->hot    == id) return ctx->theme.accent_hot;
    return ctx->theme.surface_hot;
}

static inline bool forge_ui_ctx_button(ForgeUiContext *ctx,
                                       const char *text,
                                       ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !text || text[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;
    Uint32 id = forge_ui_hash_id(ctx, text);

    bool clicked = false;

    /* ── Hit testing ──────────────────────────────────────────────────── */
    /* Check if the mouse cursor is within this button's bounding rect.
     * If so, this widget becomes a candidate for hot. */
    bool mouse_over = forge_ui__widget_mouse_over(ctx, rect);

    if (mouse_over) {
        ctx->next_hot = id;
    }

    /* ── State transitions ────────────────────────────────────────────── */
    /* Active transition: on the frame the mouse button transitions from up
     * to down (press edge), if this widget is hot, it becomes active.
     * Using edge detection prevents a held mouse dragged onto a button
     * from falsely activating it.  When overlapping widgets share a click
     * point, each hovered widget overwrites ctx->active in draw order so
     * the last-drawn (topmost) widget wins. */
    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    if (mouse_pressed && ctx->next_hot == id) {
        ctx->active = id;
    }

    /* Click detection: a click occurs when the mouse button is released
     * while this widget is active AND the cursor is still over it. */
    if (ctx->active == id && !ctx->mouse_down) {
        if (mouse_over) {
            clicked = true;
        }
        /* Release: clear active regardless of cursor position */
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* ── Choose background color based on state ───────────────────────── */
    ForgeUiColor bg = forge_ui__surface_color(ctx, id);

    /* ── Emit background rectangle ────────────────────────────────────── */
    forge_ui__emit_rect(ctx, rect, bg.r, bg.g, bg.b, bg.a);

    /* ── Extract display text (strip ## suffix if present) ────────────── */
    const char *disp_end = forge_ui__display_end(text);
    int disp_len = (int)(disp_end - text);
    char disp_buf[256];
    if (disp_len >= (int)sizeof(disp_buf))
        disp_len = (int)sizeof(disp_buf) - 1;
    SDL_memcpy(disp_buf, text, (size_t)disp_len);
    disp_buf[disp_len] = '\0';

    /* ── Emit centered text label ─────────────────────────────────────── */
    /* Measure text to compute centering offsets */
    ForgeUiTextMetrics metrics = forge_ui_text_measure(ctx->atlas, disp_buf, NULL);

    /* Center the text within the button rectangle.
     * Horizontal: offset by half the difference between rect width and text width.
     * Vertical: place the baseline so that the text is vertically centered.
     * The baseline y = rect_center_y - text_half_height + ascender. */
    float ascender_px = forge_ui__ascender_px(ctx->atlas);
    float text_x = rect.x + (rect.w - metrics.width) * 0.5f;
    float text_y = rect.y + (rect.h - metrics.height) * 0.5f + ascender_px;

    forge_ui_ctx_label(ctx, disp_buf, text_x, text_y);

    return clicked;
}

static inline bool forge_ui_ctx_checkbox(ForgeUiContext *ctx,
                                          const char *label,
                                          bool *value,
                                          ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !label || !value || label[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;
    Uint32 id = forge_ui_hash_id(ctx, label);

    bool toggled = false;

    /* ── Hit testing ──────────────────────────────────────────────────── */
    /* The hit area covers the entire widget rect (box + label region).
     * This gives users a generous click target -- they can click on the
     * label text, not just the small box. */
    bool mouse_over = forge_ui__widget_mouse_over(ctx, rect);
    if (mouse_over) {
        ctx->next_hot = id;
    }

    /* ── State transitions ────────────────────────────────────────────── */
    /* Edge-triggered: activation fires once on the press edge (up→down).
     * This prevents a held mouse dragged onto the checkbox from toggling
     * it, and lets the user cancel by dragging off before releasing.
     * Overlapping widgets overwrite ctx->active in draw order so the
     * last-drawn (topmost) widget wins. */
    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    if (mouse_pressed && ctx->next_hot == id) {
        ctx->active = id;
    }

    /* Toggle on release: flip *value when the mouse is released while
     * still over the widget and it was the active widget. */
    if (ctx->active == id && !ctx->mouse_down) {
        if (mouse_over) {
            *value = !(*value);
            toggled = true;
        }
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* ── Box color reflects interaction state ─────────────────────────── */
    ForgeUiColor box_c = forge_ui__surface_color(ctx, id);

    /* ── Compute scaled checkbox dimensions ──────────────────────────── */
    float cb_size = FORGE_UI_SCALED(ctx, ctx->spacing.checkbox_box_size);

    /* ── Compute box position (vertically centered in widget rect) ────── */
    float box_x = rect.x;
    float box_y = rect.y + (rect.h - cb_size) * 0.5f;
    ForgeUiRect box_rect = { box_x, box_y, cb_size, cb_size };

    /* ── Outer box — border with hover feedback via box color ────────── */
    forge_ui__emit_rect(ctx, box_rect, box_c.r, box_c.g, box_c.b, box_c.a);

    /* ── Inner fill — solid rect rather than a glyph keeps the renderer
     *    purely quad-based with no dedicated checkmark in the atlas ──── */
    if (*value) {
        float cb_inner = FORGE_UI_SCALED(ctx, FORGE_UI_CB_INNER_PAD);
        ForgeUiRect inner = {
            box_x + cb_inner,
            box_y + cb_inner,
            cb_size - 2.0f * cb_inner,
            cb_size - 2.0f * cb_inner
        };
        forge_ui__emit_rect(ctx, inner,
                            ctx->theme.accent.r, ctx->theme.accent.g,
                            ctx->theme.accent.b, ctx->theme.accent.a);
    }

    /* ── Extract display text (strip ## suffix if present) ────────────── */
    const char *disp_end = forge_ui__display_end(label);
    int disp_len = (int)(disp_end - label);
    char disp_buf[256];
    if (disp_len >= (int)sizeof(disp_buf))
        disp_len = (int)sizeof(disp_buf) - 1;
    SDL_memcpy(disp_buf, label, (size_t)disp_len);
    disp_buf[disp_len] = '\0';

    /* ── Label baseline alignment ────────────────────────────────────── */
    /* The font origin is at the baseline, not the top of the em square.
     * Offset by the ascender so text sits visually centered in the rect. */
    float ascender_px = forge_ui__ascender_px(ctx->atlas);

    float label_x = box_x + cb_size + FORGE_UI_SCALED(ctx, ctx->spacing.widget_padding);
    float label_y = rect.y + (rect.h - ctx->atlas->pixel_height) * 0.5f
                    + ascender_px;

    forge_ui_ctx_label(ctx, disp_buf, label_x, label_y);

    return toggled;
}

static inline bool forge_ui_ctx_slider(ForgeUiContext *ctx,
                                        const char *label,
                                        float *value,
                                        float min_val, float max_val,
                                        ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !value || !label || label[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;
    if (!forge_isfinite(min_val) || !forge_isfinite(max_val)) return false;
    if (!(max_val > min_val)) return false;  /* also rejects equal */
    /* Sanitize *value: NaN/Inf would poison the thumb position and
     * propagate into vertex data.  Clamp to min_val as a safe default. */
    if (!forge_isfinite(*value)) *value = min_val;
    Uint32 id = forge_ui_hash_id(ctx, label);

    bool changed = false;

    /* ── Hit testing ──────────────────────────────────────────────────── */
    /* The hit area covers the entire widget rect.  Clicking anywhere on
     * the track (not just the thumb) activates the slider and snaps the
     * value to the click position. */
    bool mouse_over = forge_ui__widget_mouse_over(ctx, rect);
    if (mouse_over) {
        ctx->next_hot = id;
    }

    /* ── State transitions ────────────────────────────────────────────── */
    /* Edge-triggered: activation fires once on the press edge (up→down).
     * Subsequent frames update the value via the drag path below, without
     * re-entering the activation branch.  Overlapping widgets overwrite
     * ctx->active in draw order so the last-drawn (topmost) widget wins. */
    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    if (mouse_pressed && ctx->next_hot == id) {
        ctx->active = id;
    }

    /* ── Compute scaled slider dimensions ─────────────────────────────── */
    float sl_thumb_w = FORGE_UI_SCALED(ctx, ctx->spacing.slider_thumb_width);
    float sl_thumb_h = FORGE_UI_SCALED(ctx, ctx->spacing.slider_thumb_height);
    float sl_track_h = FORGE_UI_SCALED(ctx, ctx->spacing.slider_track_height);

    /* ── Effective track geometry ─────────────────────────────────────── */
    /* The thumb center can travel from half a thumb width inside the left
     * edge to half a thumb width inside the right edge.  This keeps the
     * thumb fully within the widget rect at both extremes.  Clamp to
     * zero so a rect narrower than the thumb does not produce a negative
     * range. */
    float track_x = rect.x + sl_thumb_w * 0.5f;
    float track_w = rect.w - sl_thumb_w;
    if (track_w < 0.0f) track_w = 0.0f;

    /* ── Value update while active (drag interaction) ─────────────────── */
    /* While the mouse button is held and this slider is active, map the
     * mouse x position to a normalized t in [0, 1], then to the user
     * value.  This update happens regardless of whether the cursor is
     * inside the widget bounds -- that is the key property of drag
     * interaction.  The value is always clamped to [min_val, max_val]. */
    if (ctx->active == id && ctx->mouse_down) {
        float t = 0.0f;
        if (track_w > 0.0f) {
            t = (ctx->mouse_x - track_x) / track_w;
        }
        if (!(t >= 0.0f)) t = 0.0f;  /* NaN-safe: NaN fails >= */
        if (!(t <= 1.0f)) t = 1.0f;
        float new_val = min_val + t * (max_val - min_val);
        if (new_val != *value) {
            *value = new_val;
            changed = true;
        }
    }

    /* ── Release: clear active ────────────────────────────────────────── */
    /* Unlike button, there is no click event to detect -- the slider's
     * purpose is the continuous value update during drag.  On release we
     * simply clear active so other widgets can become active again. */
    if (ctx->active == id && !ctx->mouse_down) {
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* ── Re-derive t from *value for thumb positioning ──────────────── */
    /* Use the canonical *value (not the drag t) so the thumb reflects any
     * clamping or quantization the caller may apply between frames. */
    float t = (*value - min_val) / (max_val - min_val);
    if (!(t >= 0.0f)) t = 0.0f;  /* NaN-safe: NaN fails >= */
    if (!(t <= 1.0f)) t = 1.0f;

    /* ── Track — thin bar so the thumb visually protrudes above/below ── */
    float track_draw_y = rect.y + (rect.h - sl_track_h) * 0.5f;
    ForgeUiRect track_rect = { rect.x, track_draw_y,
                               rect.w, sl_track_h };
    forge_ui__emit_rect(ctx, track_rect,
                        ctx->theme.border.r, ctx->theme.border.g,
                        ctx->theme.border.b, ctx->theme.border.a);

    /* ── Choose thumb color based on state ────────────────────────────── */
    ForgeUiColor th_c = forge_ui__accent_color(ctx, id);

    /* ── Emit thumb rectangle ─────────────────────────────────────────── */
    /* The thumb center is at track_x + t * track_w.  Subtract half the
     * thumb width to get the left edge. */
    float thumb_cx = track_x + t * track_w;
    float thumb_x = thumb_cx - sl_thumb_w * 0.5f;
    float thumb_y = rect.y + (rect.h - sl_thumb_h) * 0.5f;
    ForgeUiRect thumb_rect = { thumb_x, thumb_y,
                               sl_thumb_w, sl_thumb_h };
    forge_ui__emit_rect(ctx, thumb_rect, th_c.r, th_c.g, th_c.b, th_c.a);

    return changed;
}

/* ── Solid rect ────────────────────────────────────────────────────────── */

/* Draw a single solid-colored rectangle.  Use this for overlays, dividers,
 * backgrounds, or any UI element that is just a flat colored quad.  Unlike
 * forge_ui_ctx_progress_bar(), this emits one quad — no background track. */
static inline void forge_ui_ctx_rect(ForgeUiContext *ctx,
                                      ForgeUiRect rect,
                                      ForgeUiColor color)
{
    if (!ctx || !ctx->atlas) return;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return;
    if (!forge_isfinite(color.r) || !forge_isfinite(color.g) ||
        !forge_isfinite(color.b) || !forge_isfinite(color.a)) return;

    /* Clamp color components to [0,1] */
    float r = color.r < 0.0f ? 0.0f : (color.r > 1.0f ? 1.0f : color.r);
    float g = color.g < 0.0f ? 0.0f : (color.g > 1.0f ? 1.0f : color.g);
    float b = color.b < 0.0f ? 0.0f : (color.b > 1.0f ? 1.0f : color.b);
    float a = color.a < 0.0f ? 0.0f : (color.a > 1.0f ? 1.0f : color.a);

    forge_ui__emit_rect(ctx, rect, r, g, b, a);
}

/* Layout-aware variant — consumes a slot from the active layout. */
static inline void forge_ui_ctx_rect_layout(ForgeUiContext *ctx,
                                             ForgeUiColor color,
                                             float size)
{
    if (!ctx || !ctx->atlas) return;
    if (ctx->layout_depth <= 0) return;
    if (!forge_isfinite(color.r) || !forge_isfinite(color.g) ||
        !forge_isfinite(color.b) || !forge_isfinite(color.a)) return;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    forge_ui_ctx_rect(ctx, rect, color);
}

/* ── Progress bar implementation ───────────────────────────────────────── */

static inline void forge_ui_ctx_progress_bar(ForgeUiContext *ctx,
                                              float value,
                                              float max_val,
                                              ForgeUiColor fill_color,
                                              ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas) return;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return;
    if (!forge_isfinite(max_val) || !(max_val > 0.0f)) return;  /* rejects NaN and +Inf */
    if (!forge_isfinite(value)) value = 0.0f;

    /* Sanitize fill_color — reject non-finite components, clamp to [0,1] */
    if (!forge_isfinite(fill_color.r) || !forge_isfinite(fill_color.g) ||
        !forge_isfinite(fill_color.b) || !forge_isfinite(fill_color.a)) return;
    if (fill_color.r < 0.0f) fill_color.r = 0.0f;
    if (fill_color.r > 1.0f) fill_color.r = 1.0f;
    if (fill_color.g < 0.0f) fill_color.g = 0.0f;
    if (fill_color.g > 1.0f) fill_color.g = 1.0f;
    if (fill_color.b < 0.0f) fill_color.b = 0.0f;
    if (fill_color.b > 1.0f) fill_color.b = 1.0f;
    if (fill_color.a < 0.0f) fill_color.a = 0.0f;
    if (fill_color.a > 1.0f) fill_color.a = 1.0f;

    /* Clamp value to [0, max_val] */
    if (value < 0.0f) value = 0.0f;
    if (value > max_val) value = max_val;

    float t = value / max_val;

    /* Track background — uses border color for visual consistency with
     * slider tracks.  Emitted first so the fill draws on top. */
    forge_ui__emit_rect(ctx, rect,
                        ctx->theme.border.r, ctx->theme.border.g,
                        ctx->theme.border.b, ctx->theme.border.a);

    /* Filled portion — proportional to value/max_val */
    if (t > 0.0f) {
        ForgeUiRect fill = { rect.x, rect.y, rect.w * t, rect.h };
        forge_ui__emit_rect(ctx, fill,
                            fill_color.r, fill_color.g,
                            fill_color.b, fill_color.a);
    }
}

static inline void forge_ui_ctx_set_keyboard(ForgeUiContext *ctx,
                                              const char *text_input,
                                              bool key_backspace,
                                              bool key_delete,
                                              bool key_left,
                                              bool key_right,
                                              bool key_home,
                                              bool key_end,
                                              bool key_escape)
{
    if (!ctx) return;
    ctx->text_input = text_input;
    ctx->key_backspace = key_backspace;
    ctx->key_delete = key_delete;
    ctx->key_left = key_left;
    ctx->key_right = key_right;
    ctx->key_home = key_home;
    ctx->key_end = key_end;
    ctx->key_escape = key_escape;
}

static inline bool forge_ui_ctx_text_input(ForgeUiContext *ctx,
                                            const char *label,
                                            ForgeUiTextInputState *state,
                                            ForgeUiRect rect,
                                            bool cursor_visible)
{
    if (!ctx || !ctx->atlas || !state || !state->buffer
        || !label || label[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;
    Uint32 id = forge_ui_hash_id(ctx, label);

    /* Validate state invariants to prevent out-of-bounds access.
     * The application owns these fields; reject if they violate the
     * contract:  capacity > 0,  0 <= length < capacity,  0 <= cursor <= length. */
    if (state->capacity <= 0) return false;
    if (state->length < 0 || state->length >= state->capacity) return false;
    if (state->cursor < 0 || state->cursor > state->length) return false;
    if (state->buffer[state->length] != '\0') return false;

    bool content_changed = false;
    bool is_focused = (ctx->focused == id);

    /* ── Hit testing ──────────────────────────────────────────────────── */
    bool mouse_over = forge_ui__widget_mouse_over(ctx, rect);
    if (mouse_over) {
        ctx->next_hot = id;
    }

    /* ── State transitions (press-release-over, same as button) ──────── */
    /* Use ctx->next_hot == id (not mouse_over) so that overlapping widgets
     * resolve activation by draw order, matching the button/slider pattern. */
    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    if (mouse_pressed && ctx->next_hot == id) {
        ctx->active = id;
        /* Mark that a text input claimed this press -- prevents ctx_end
         * from clearing focused on this frame (click-outside detection). */
        ctx->_ti_press_claimed = true;
    }

    /* Click detection: release while active + cursor still over = focus */
    if (ctx->active == id && !ctx->mouse_down) {
        if (mouse_over) {
            ctx->focused = id;
            is_focused = true;
        }
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* ── Keyboard input processing (only when focused AND visible) ────── */
    /* When a panel clips this widget off-screen, the widget rect lies
     * entirely outside the clip rect.  Accepting keyboard input for an
     * invisible widget would silently mutate the buffer with no visual
     * feedback, so we suppress editing until the widget scrolls back
     * into view.  Focus is intentionally preserved so the cursor
     * reappears when the user scrolls back.
     *
     * Similarly, when the containing window is covered by another window
     * (_keyboard_input_suppressed), keyboard input is silenced.  The
     * visual focused state is kept so the window still looks focused --
     * important for games that suppress window input for game controls
     * (e.g. FPS camera) without wanting a visual change. */
    bool visible = true;
    if (ctx->has_clip) {
        float cx1 = ctx->clip_rect.x + ctx->clip_rect.w;
        float cy1 = ctx->clip_rect.y + ctx->clip_rect.h;
        float rx1 = rect.x + rect.w;
        float ry1 = rect.y + rect.h;
        visible = !(rx1 <= ctx->clip_rect.x || rect.x >= cx1 ||
                     ry1 <= ctx->clip_rect.y || rect.y >= cy1);
    }
    if (is_focused && visible && !ctx->_keyboard_input_suppressed) {
        /* Editing operations are mutually exclusive within a single frame.
         * When SDL delivers both a text input event and a key event in the
         * same frame, applying both would operate on inconsistent state
         * (e.g., backspace would delete the just-inserted character instead
         * of the pre-existing one).  Deletion keys take priority. */
        bool did_edit = false;

        /* Backspace: remove the byte before cursor, shift trailing left */
        if (!did_edit && ctx->key_backspace && state->cursor > 0) {
            SDL_memmove(state->buffer + state->cursor - 1,
                        state->buffer + state->cursor,
                        (size_t)(state->length - state->cursor));
            state->cursor--;
            state->length--;
            state->buffer[state->length] = '\0';
            content_changed = true;
            did_edit = true;
        }

        /* Delete: remove the byte at cursor, shift trailing left */
        if (!did_edit && ctx->key_delete && state->cursor < state->length) {
            SDL_memmove(state->buffer + state->cursor,
                        state->buffer + state->cursor + 1,
                        (size_t)(state->length - state->cursor - 1));
            state->length--;
            state->buffer[state->length] = '\0';
            content_changed = true;
            did_edit = true;
        }

        /* Character insertion: splice typed characters into the buffer
         * at the cursor position.  Trailing bytes shift right to make
         * room, then the new bytes are written at cursor. */
        if (!did_edit && ctx->text_input && ctx->text_input[0] != '\0') {
            size_t raw_len = SDL_strlen(ctx->text_input);
            /* Guard: reject input longer than the buffer can ever hold.
             * Cast is safe because capacity > 0 (validated above). */
            if (raw_len <= (size_t)(state->capacity - 1)) {
                int insert_len = (int)raw_len;
                /* Use subtraction form to avoid signed overflow:
                 * insert_len < capacity - length  (both sides positive). */
                if (insert_len < state->capacity - state->length) {
                    SDL_memmove(state->buffer + state->cursor + insert_len,
                                state->buffer + state->cursor,
                                (size_t)(state->length - state->cursor));
                    SDL_memcpy(state->buffer + state->cursor,
                               ctx->text_input, (size_t)insert_len);
                    state->cursor += insert_len;
                    state->length += insert_len;
                    state->buffer[state->length] = '\0';
                    content_changed = true;
                    did_edit = true;
                }
            }
        }

        /* Cursor movement -- also mutually exclusive with edits.
         * If an edit (backspace/delete/insert) already ran this frame,
         * skip cursor movement to avoid double-shifting the cursor. */
        if (!did_edit) {
            if (ctx->key_left && state->cursor > 0) {
                state->cursor--;
            }
            if (ctx->key_right && state->cursor < state->length) {
                state->cursor++;
            }
            if (ctx->key_home) {
                state->cursor = 0;
            }
            if (ctx->key_end) {
                state->cursor = state->length;
            }
        }
    }

    /* ── Choose background color based on state ──────────────────────── */
    float bg_r, bg_g, bg_b, bg_a;
    if (is_focused) {
        bg_r = ctx->theme.surface_active.r;  bg_g = ctx->theme.surface_active.g;
        bg_b = ctx->theme.surface_active.b;  bg_a = ctx->theme.surface_active.a;
    } else if (ctx->hot == id) {
        bg_r = ctx->theme.surface_hot.r;  bg_g = ctx->theme.surface_hot.g;
        bg_b = ctx->theme.surface_hot.b;  bg_a = ctx->theme.surface_hot.a;
    } else {
        bg_r = ctx->theme.surface.r;  bg_g = ctx->theme.surface.g;
        bg_b = ctx->theme.surface.b;  bg_a = ctx->theme.surface.a;
    }

    /* ── Emit background rectangle ───────────────────────────────────── */
    forge_ui__emit_rect(ctx, rect, bg_r, bg_g, bg_b, bg_a);

    /* ── Emit focused border (accent outline drawn on top of bg) ─────── */
    if (is_focused) {
        forge_ui__emit_border(ctx, rect, FORGE_UI_SCALED(ctx, FORGE_UI_TI_BORDER_WIDTH),
                              ctx->theme.border_focused.r, ctx->theme.border_focused.g,
                              ctx->theme.border_focused.b, ctx->theme.border_focused.a);
    }

    /* ── Compute font metrics for baseline positioning ───────────────── */
    float ascender_px = forge_ui__ascender_px(ctx->atlas);

    /* Vertically center the text within the rect: offset to the top of
     * the em square, then add the ascender to reach the baseline where
     * forge_ui_ctx_label expects the y coordinate. */
    float text_top_y = rect.y + (rect.h - ctx->atlas->pixel_height) * 0.5f;
    float baseline_y = text_top_y + ascender_px;

    /* ── Compute scaled text input padding ────────────────────────────── */
    float ti_pad = FORGE_UI_SCALED(ctx, ctx->spacing.text_input_padding);

    /* ── Emit text quads ─────────────────────────────────────────────── */
    if (state->length > 0) {
        forge_ui_ctx_label(ctx, state->buffer,
                           rect.x + ti_pad, baseline_y);
    }

    /* ── Emit cursor bar ─────────────────────────────────────────────── */
    /* The cursor x position is computed by measuring the substring
     * buffer[0..cursor] using forge_ui_text_measure.  This gives the
     * pen_x advance, which is the exact pixel offset where the cursor
     * should appear. */
    if (is_focused && cursor_visible) {
        float cursor_x = rect.x + ti_pad;
        if (state->cursor > 0 && state->length > 0) {
            /* Temporarily null-terminate at cursor so forge_ui_text_measure
             * measures only the substring before the insertion point */
            char saved = state->buffer[state->cursor];
            state->buffer[state->cursor] = '\0';
            ForgeUiTextMetrics m = forge_ui_text_measure(
                ctx->atlas, state->buffer, NULL);
            state->buffer[state->cursor] = saved;
            cursor_x += m.width;
        }

        ForgeUiRect cursor_rect = {
            cursor_x, text_top_y,
            FORGE_UI_SCALED(ctx, FORGE_UI_TI_CURSOR_WIDTH),
            ctx->atlas->pixel_height
        };
        forge_ui__emit_rect(ctx, cursor_rect,
                            ctx->theme.cursor.r, ctx->theme.cursor.g,
                            ctx->theme.cursor.b, ctx->theme.cursor.a);
    }

    return content_changed;
}

/* ── Layout implementation ──────────────────────────────────────────────── */

static inline bool forge_ui_ctx_layout_push(ForgeUiContext *ctx,
                                             ForgeUiRect rect,
                                             ForgeUiLayoutDirection direction,
                                             float padding,
                                             float spacing)
{
    if (!ctx) return false;

    /* Runtime bounds check — must not rely on assert() alone because
     * assert() is compiled out in NDEBUG builds, which would allow
     * an out-of-bounds write into layout_stack[]. */
    if (ctx->layout_depth >= FORGE_UI_LAYOUT_MAX_DEPTH) {
        SDL_Log("forge_ui_ctx_layout_push: stack overflow (depth=%d, max=%d)",
                ctx->layout_depth, FORGE_UI_LAYOUT_MAX_DEPTH);
        return false;
    }

    /* Validate direction — must be one of the defined enum values.
     * Reject unknown values rather than silently treating them as horizontal. */
    if (direction != FORGE_UI_LAYOUT_VERTICAL
        && direction != FORGE_UI_LAYOUT_HORIZONTAL) {
        SDL_Log("forge_ui_ctx_layout_push: invalid direction %d"
                " (expected FORGE_UI_LAYOUT_VERTICAL or FORGE_UI_LAYOUT_HORIZONTAL)",
                (int)direction);
        return false;
    }

    /* Reject NaN/Inf in rect fields */
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) {
        return false;
    }

    /* Substitute themed defaults when the caller passes 0, meaning
     * "use the standard spacing".  A value < 0 is treated as "explicit
     * zero" — clamped to 0 without default substitution.  This lets
     * internal code (panel_begin, window_begin) opt out of defaults
     * by passing a negative value. */
    if (SDL_isnan(padding) || SDL_isinf(padding)) padding = 0.0f;
    if (SDL_isnan(spacing) || SDL_isinf(spacing)) spacing = 0.0f;
    if (padding == 0.0f) {
        padding = FORGE_UI_SCALED(ctx, ctx->spacing.widget_padding);
    } else if (padding < 0.0f) {
        padding = 0.0f;
    }
    if (spacing == 0.0f) {
        spacing = FORGE_UI_SCALED(ctx, ctx->spacing.item_spacing);
    } else if (spacing < 0.0f) {
        spacing = 0.0f;
    }

    ForgeUiLayout *layout = &ctx->layout_stack[ctx->layout_depth];
    layout->rect       = rect;
    layout->direction   = direction;
    layout->padding     = padding;
    layout->spacing     = spacing;
    layout->cursor_x    = rect.x + padding;
    layout->cursor_y    = rect.y + padding;
    layout->item_count  = 0;

    /* Available space after subtracting padding from both sides.
     * Clamp to zero so a very small rect does not go negative. */
    float inner_w = rect.w - 2.0f * padding;
    float inner_h = rect.h - 2.0f * padding;
    layout->remaining_w = (inner_w > 0.0f) ? inner_w : 0.0f;
    layout->remaining_h = (inner_h > 0.0f) ? inner_h : 0.0f;

    ctx->layout_depth++;
    return true;
}

static inline bool forge_ui_ctx_layout_pop(ForgeUiContext *ctx)
{
    if (!ctx) return false;

    /* Runtime bounds check — must not rely on assert() alone because
     * assert() is compiled out in NDEBUG builds, which would allow
     * layout_depth to go negative and corrupt subsequent accesses. */
    if (ctx->layout_depth <= 0) {
        SDL_Log("forge_ui_ctx_layout_pop: stack underflow (depth=%d)",
                ctx->layout_depth);
        return false;
    }

    ctx->layout_depth--;
    return true;
}

static inline ForgeUiRect forge_ui_ctx_layout_next(ForgeUiContext *ctx,
                                                    float size)
{
    ForgeUiRect empty = {0.0f, 0.0f, 0.0f, 0.0f};

    /* Runtime guards — must not rely on assert() alone because assert()
     * is compiled out in NDEBUG builds, which would allow a NULL deref
     * or out-of-bounds read from layout_stack[-1]. */
    if (!ctx || ctx->layout_depth <= 0) {
        if (ctx) {
            SDL_Log("forge_ui_ctx_layout_next: no active layout (depth=%d)",
                    ctx->layout_depth);
        }
        return empty;
    }

    /* Clamp negative or invalid sizes to zero */
    if (size < 0.0f || SDL_isnan(size) || SDL_isinf(size)) size = 0.0f;

    ForgeUiLayout *layout = &ctx->layout_stack[ctx->layout_depth - 1];
    ForgeUiRect result;

    if (layout->direction == FORGE_UI_LAYOUT_VERTICAL) {
        /* Add spacing gap before this widget (but not before the first) */
        if (layout->item_count > 0) {
            layout->cursor_y += layout->spacing;
            layout->remaining_h -= layout->spacing;
            if (layout->remaining_h < 0.0f) layout->remaining_h = 0.0f;
        }

        /* Vertical: widget gets full available width, caller-specified height */
        result.x = layout->cursor_x;
        result.y = layout->cursor_y;
        result.w = layout->remaining_w;
        result.h = size;

        /* Advance cursor downward */
        layout->cursor_y += size;
        layout->remaining_h -= size;
        if (layout->remaining_h < 0.0f) layout->remaining_h = 0.0f;
    } else {
        /* Add spacing gap before this widget (but not before the first) */
        if (layout->item_count > 0) {
            layout->cursor_x += layout->spacing;
            layout->remaining_w -= layout->spacing;
            if (layout->remaining_w < 0.0f) layout->remaining_w = 0.0f;
        }

        /* Horizontal: widget gets caller-specified width, full available height */
        result.x = layout->cursor_x;
        result.y = layout->cursor_y;
        result.w = size;
        result.h = layout->remaining_h;

        /* Advance cursor rightward */
        layout->cursor_x += size;
        layout->remaining_w -= size;
        if (layout->remaining_w < 0.0f) layout->remaining_w = 0.0f;
    }

    layout->item_count++;

    /* Apply scroll offset when inside an active panel.  The widget's
     * logical position stays unchanged in the layout cursor, but the
     * returned rect is shifted upward by scroll_y so that content
     * scrolls visually.  The clip rect (set by panel_begin) discards
     * any quads that fall outside the visible area. */
    if (ctx->_panel_active && ctx->_panel.scroll_y) {
        result.y -= *ctx->_panel.scroll_y;
    }

    return result;
}

/* ── Layout-aware widget implementations ───────────────────────────────── */

static inline void forge_ui_ctx_label_colored_layout(ForgeUiContext *ctx,
                                                      const char *text,
                                                      float size,
                                                      float r, float g, float b, float a)
{
    if (!ctx || !text || !ctx->atlas) return;
    if (ctx->layout_depth <= 0) return;  /* no active layout — no-op */

    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);

    /* Compute baseline position within the rect: vertically center the
     * em square, then offset by the ascender to reach the baseline. */
    float ascender_px = forge_ui__ascender_px(ctx->atlas);
    float text_y = rect.y + (rect.h - ctx->atlas->pixel_height) * 0.5f
                   + ascender_px;

    forge_ui_ctx_label_colored(ctx, text, rect.x, text_y, r, g, b, a);
}

static inline void forge_ui_ctx_label_layout(ForgeUiContext *ctx,
                                              const char *text,
                                              float size)
{
    if (!ctx) return;
    forge_ui_ctx_label_colored_layout(ctx, text, size,
                                       ctx->theme.text.r, ctx->theme.text.g,
                                       ctx->theme.text.b, ctx->theme.text.a);
}

static inline bool forge_ui_ctx_button_layout(ForgeUiContext *ctx,
                                               const char *text,
                                               float size)
{
    /* Validate all params before calling layout_next so we don't
     * advance the cursor for a widget that will fail to draw. */
    if (!ctx || !ctx->atlas || !text || text[0] == '\0') return false;
    if (ctx->layout_depth <= 0) return false;  /* no active layout — no-op */
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_button(ctx, text, rect);
}

static inline bool forge_ui_ctx_checkbox_layout(ForgeUiContext *ctx,
                                                 const char *label,
                                                 bool *value,
                                                 float size)
{
    if (!ctx || !ctx->atlas || !label || !value || label[0] == '\0') return false;
    if (ctx->layout_depth <= 0) return false;  /* no active layout — no-op */
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_checkbox(ctx, label, value, rect);
}

static inline bool forge_ui_ctx_slider_layout(ForgeUiContext *ctx,
                                               const char *label,
                                               float *value,
                                               float min_val, float max_val,
                                               float size)
{
    if (!ctx || !ctx->atlas || !value || !label || label[0] == '\0') return false;
    if (ctx->layout_depth <= 0) return false;  /* no active layout — no-op */
    if (!forge_isfinite(min_val) || !forge_isfinite(max_val)) return false;
    if (!(max_val > min_val)) return false;  /* also rejects equal */
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_slider(ctx, label, value, min_val, max_val, rect);
}

static inline void forge_ui_ctx_progress_bar_layout(ForgeUiContext *ctx,
                                                     float value,
                                                     float max_val,
                                                     ForgeUiColor fill_color,
                                                     float size)
{
    if (!ctx || !ctx->atlas) return;
    if (ctx->layout_depth <= 0) return;  /* no active layout — no-op */
    if (!forge_isfinite(max_val) || !(max_val > 0.0f)) return;  /* rejects NaN and +Inf */
    if (!forge_isfinite(fill_color.r) || !forge_isfinite(fill_color.g) ||
        !forge_isfinite(fill_color.b) || !forge_isfinite(fill_color.a)) return;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    forge_ui_ctx_progress_bar(ctx, value, max_val, fill_color, rect);
}

/* ── Panel implementation ───────────────────────────────────────────────── */

static inline bool forge_ui_ctx_panel_begin(ForgeUiContext *ctx,
                                             const char *title,
                                             ForgeUiRect rect,
                                             float *scroll_y)
{
    if (!ctx || !ctx->atlas || !title || title[0] == '\0' || !scroll_y)
        return false;

    /* Reject nested panels -- only one panel may be active at a time.
     * Opening a second panel would overwrite the first's state, corrupt
     * the clip rect, and misalign the layout stack. */
    if (ctx->_panel_active) {
        SDL_Log("forge_ui_ctx_panel_begin: nested panels not supported "
                "(id=%u already active)", (unsigned)ctx->_panel.id);
        return false;
    }

    /* Compute the panel's own ID from the title string */
    Uint32 id = forge_ui_hash_id(ctx, title);

    /* Reject non-finite origin.  NaN or ±Inf in rect.x/rect.y would
     * propagate into every vertex position, clip rect, and content area
     * computation, corrupting the draw data. */
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y)) {
        SDL_Log("forge_ui_ctx_panel_begin: rect origin must be finite");
        return false;
    }

    /* Reject non-positive or non-finite rect dimensions.  !(x > 0) catches
     * NaN and ≤ 0; !isfinite catches ±Inf.  +Inf would produce infinite
     * vertex positions, corrupting the draw data. */
    if (!(rect.w > 0.0f) || !forge_isfinite(rect.w) ||
        !(rect.h > 0.0f) || !forge_isfinite(rect.h)) {
        SDL_Log("forge_ui_ctx_panel_begin: rect dimensions must be "
                "positive and finite");
        return false;
    }

    /* Sanitize *scroll_y: a negative offset would shift content downward
     * (exposing blank space above the first widget), NaN would poison
     * every layout_next position, and +Inf would push all widgets to
     * -Inf.  The !(x >= 0) form catches NaN and negatives; !isfinite
     * catches ±Inf. */
    if (!(*scroll_y >= 0.0f) || !forge_isfinite(*scroll_y)) *scroll_y = 0.0f;

    /* ── Draw panel background ────────────────────────────────────────── */
    forge_ui__emit_rect(ctx, rect,
                        ctx->theme.bg.r, ctx->theme.bg.g,
                        ctx->theme.bg.b, ctx->theme.bg.a);

    /* ── Compute scaled panel dimensions ─────────────────────────────── */
    float title_bar_h = FORGE_UI_SCALED(ctx, ctx->spacing.title_bar_height);
    float panel_pad   = FORGE_UI_SCALED(ctx, ctx->spacing.panel_padding);
    float sb_w        = FORGE_UI_SCALED(ctx, ctx->spacing.scrollbar_width);

    /* ── Draw title bar ───────────────────────────────────────────────── */
    ForgeUiRect title_rect = {
        rect.x, rect.y,
        rect.w, title_bar_h
    };
    forge_ui__emit_rect(ctx, title_rect,
                        ctx->theme.title_bar.r, ctx->theme.title_bar.g,
                        ctx->theme.title_bar.b, ctx->theme.title_bar.a);

    /* Center the title text in the title bar (strip ## suffix).
     * title is guaranteed non-NULL and non-empty by the guard at entry. */
    {
        const char *disp_end = forge_ui__display_end(title);
        int disp_len = (int)(disp_end - title);
        char disp_buf[256];
        if (disp_len >= (int)sizeof(disp_buf))
            disp_len = (int)sizeof(disp_buf) - 1;
        SDL_memcpy(disp_buf, title, (size_t)disp_len);
        disp_buf[disp_len] = '\0';

        ForgeUiTextMetrics m = forge_ui_text_measure(ctx->atlas, disp_buf, NULL);
        float ascender_px = forge_ui__ascender_px(ctx->atlas);
        float tx = rect.x + (rect.w - m.width) * 0.5f;
        float ty = rect.y + (title_bar_h - m.height) * 0.5f
                   + ascender_px;
        forge_ui_ctx_label_colored(ctx, disp_buf, tx, ty,
                                    ctx->theme.title_bar_text.r, ctx->theme.title_bar_text.g,
                                    ctx->theme.title_bar_text.b, ctx->theme.title_bar_text.a);
    }

    /* ── Compute content area ─────────────────────────────────────────── */
    ForgeUiRect content = {
        rect.x + panel_pad,
        rect.y + title_bar_h + panel_pad,
        rect.w - 2.0f * panel_pad - sb_w,
        rect.h - title_bar_h - 2.0f * panel_pad
    };
    if (content.w < 0.0f) content.w = 0.0f;
    if (content.h < 0.0f) content.h = 0.0f;

    /* ── Pre-clamp scroll_y using last frame's content height ────────── */
    /* The previous panel_end stored content_height in _panel.  Use it
     * to clamp the *incoming* scroll_y before applying the scroll offset
     * to widget positions.  This helps when the visible area grows
     * (e.g. panel resize): prev_max shrinks and the stale scroll_y
     * that was valid last frame now exceeds the new max.
     *
     * Only apply the pre-clamp when _panel.id matches this panel.
     * With multiple panels per frame, _panel retains the state of
     * whichever panel called panel_end last; using that stale height
     * for a different panel would silently corrupt its scroll_y.
     *
     * Limitation: when content *shrinks* between frames (e.g. items
     * removed from a list), prev_max is based on the old (large)
     * content height, so it won't clamp aggressively enough.  The
     * panel may show blank space for one frame until panel_end
     * recomputes the true max.  This one-frame lag is inherent to
     * immediate-mode UI — the current frame's content height is not
     * known until all widgets have been placed.
     *
     * On the very first frame content_height is 0, so prev_max is 0
     * and any stale scroll_y is clamped to 0 — correct behavior.
     * When the id doesn't match (first frame or different panel),
     * the pre-clamp is skipped and panel_end handles clamping. */
    if (ctx->_panel.id == id) {
        float prev_max = ctx->_panel.content_height - content.h;
        if (prev_max < 0.0f) prev_max = 0.0f;
        if (*scroll_y > prev_max) *scroll_y = prev_max;
    }

    /* ── Store panel state ────────────────────────────────────────────── */
    ctx->_panel.rect = rect;
    ctx->_panel.content_rect = content;
    ctx->_panel.scroll_y = scroll_y;
    ctx->_panel.id = id;
    ctx->_panel_active = true;
    /* content_height is intentionally NOT zeroed here.  The previous
     * frame's value is needed for the pre-clamp above; panel_end will
     * overwrite it with this frame's measured height. */

    /* ── Push ID scope so child widgets are scoped under this panel ──── */
    if (!forge_ui_push_id(ctx, title)) {
        SDL_Log("forge_ui_ctx_panel_begin: id scope push failed");
        ctx->has_clip = false;
        ctx->_panel_active = false;
        ctx->_panel.id = FORGE_UI_ID_NONE;
        ctx->_panel.scroll_y = NULL;
        SDL_memset(&ctx->_panel.rect, 0, sizeof(ctx->_panel.rect));
        SDL_memset(&ctx->_panel.content_rect, 0, sizeof(ctx->_panel.content_rect));
        return false;
    }

    /* ── Apply mouse wheel scrolling ──────────────────────────────────── */
    /* Validate scroll_delta: a NaN or ±Inf delta (from a corrupt input
     * event or driver bug) would make *scroll_y non-finite, poisoning
     * every subsequent layout position.  Treat non-finite as zero. */
    if (ctx->scroll_delta != 0.0f && forge_isfinite(ctx->scroll_delta) &&
        forge_ui__rect_contains(content, ctx->mouse_x, ctx->mouse_y)) {
        *scroll_y += ctx->scroll_delta * FORGE_UI_SCROLL_SPEED;
        if (*scroll_y < 0.0f) *scroll_y = 0.0f;
        /* Upper clamp is deferred to panel_end because content_height
         * is not known until all child widgets have been laid out. */
    }

    /* ── Set clip rect so child widgets outside the content area are
     *    discarded (fully outside) or trimmed (partially outside).
     *    This is what makes scrolling work: widgets placed above or
     *    below the visible region are clipped rather than drawn. ────── */
    ctx->clip_rect = content;
    ctx->has_clip = true;

    /* ── Push a vertical layout (no extra padding, 8px spacing between
     *    child widgets) so children fill the content width and stack
     *    downward with consistent gaps. ───────────────────────────────── */
    /* Pass FORGE_UI_LAYOUT_EXPLICIT_ZERO for padding so layout_push
     * treats it as explicit zero (no default substitution) — the
     * content rect already accounts for panel padding.  Spacing uses
     * the themed item_spacing. */
    if (!forge_ui_ctx_layout_push(ctx, content,
                                   FORGE_UI_LAYOUT_VERTICAL,
                                   FORGE_UI_LAYOUT_EXPLICIT_ZERO,
                                   FORGE_UI_SCALED(ctx, ctx->spacing.item_spacing))) {
        SDL_Log("forge_ui_ctx_panel_begin: layout_push failed (stack full?)");
        ctx->has_clip = false;
        ctx->_panel_active = false;
        /* Clear identity/state set above so the pre-clamp check on the
         * next frame does not match against a panel that never completed. */
        ctx->_panel.id = FORGE_UI_ID_NONE;
        ctx->_panel.scroll_y = NULL;
        SDL_memset(&ctx->_panel.rect, 0, sizeof(ctx->_panel.rect));
        SDL_memset(&ctx->_panel.content_rect, 0, sizeof(ctx->_panel.content_rect));
        forge_ui_pop_id(ctx);  /* undo the push_id from above */
        return false;
    }

    /* Record the layout cursor start position so panel_end can compute
     * how far child widgets advanced (= content_height) */
    if (ctx->layout_depth > 0) {
        ctx->_panel_content_start_y =
            ctx->layout_stack[ctx->layout_depth - 1].cursor_y;
    }

    return true;
}

static inline void forge_ui_ctx_panel_end(ForgeUiContext *ctx)
{
    if (!ctx || !ctx->_panel_active) return;

    /* ── Compute content height from layout cursor advancement ────────── */
    float content_h = 0.0f;
    if (ctx->layout_depth > 0) {
        float cursor_now = ctx->layout_stack[ctx->layout_depth - 1].cursor_y;
        content_h = cursor_now - ctx->_panel_content_start_y;
        if (!forge_isfinite(content_h) || content_h < 0.0f) content_h = 0.0f;
    }
    ctx->_panel.content_height = content_h;

    /* ── Pop the internal layout ──────────────────────────────────────── */
    forge_ui_ctx_layout_pop(ctx);

    /* ── Clear clip rect and panel state ──────────────────────────────── */
    ctx->has_clip = false;
    ctx->_panel_active = false;

    /* ── Clamp scroll_y now that this frame's content_height is known.
     *    panel_begin could only pre-clamp against the *previous* frame's
     *    height; this is the authoritative clamp using measured data.
     *    The !(x <= y) / !(x >= y) pattern forces NaN to the boundary
     *    values, preventing it from propagating into next frame. ──────── */
    float visible_h = ctx->_panel.content_rect.h;
    float max_scroll = content_h - visible_h;
    if (max_scroll < 0.0f) max_scroll = 0.0f;

    float *scroll_y = ctx->_panel.scroll_y;
    ctx->_panel.scroll_y = NULL;  /* clear before early returns to prevent stale ref */
    if (!scroll_y) return;  /* defensive: should not happen given panel_begin checks */
    if (!(*scroll_y <= max_scroll)) *scroll_y = max_scroll;
    if (!(*scroll_y >= 0.0f)) *scroll_y = 0.0f;

    /* ── Draw scrollbar (only if content overflows AND track is usable) ── */
    if (content_h <= visible_h) {
        forge_ui_pop_id(ctx);  /* pop the scope pushed by panel_begin */
        return;
    }
    if (visible_h < 1.0f) {
        forge_ui_pop_id(ctx);  /* pop the scope pushed by panel_begin */
        return;
    }

    ForgeUiRect cr = ctx->_panel.content_rect;
    float sb_w = FORGE_UI_SCALED(ctx, ctx->spacing.scrollbar_width);
    float p_pad = FORGE_UI_SCALED(ctx, ctx->spacing.panel_padding);
    float track_x = ctx->_panel.rect.x + ctx->_panel.rect.w
                     - p_pad - sb_w;
    float track_y = cr.y;
    float track_h = cr.h;
    float track_w = sb_w;

    /* Track background */
    ForgeUiRect track_rect = { track_x, track_y, track_w, track_h };
    forge_ui__emit_rect(ctx, track_rect,
                        ctx->theme.scrollbar_track.r, ctx->theme.scrollbar_track.g,
                        ctx->theme.scrollbar_track.b, ctx->theme.scrollbar_track.a);

    /* Thumb geometry — proportional height, clamped to [MIN_THUMB, track_h]
     * so the thumb never overflows the track even on very short panels. */
    float thumb_h = track_h * visible_h / content_h;
    float min_thumb = FORGE_UI_SCALED(ctx, FORGE_UI_SCROLLBAR_MIN_THUMB);
    if (thumb_h < min_thumb)
        thumb_h = min_thumb;
    if (thumb_h > track_h)
        thumb_h = track_h;
    float thumb_range = track_h - thumb_h;
    float t = (max_scroll > 0.0f) ? *scroll_y / max_scroll : 0.0f;
    float thumb_y = track_y + t * thumb_range;

    ForgeUiRect thumb_rect = { track_x, thumb_y, track_w, thumb_h };
    /* Compute scrollbar ID within the panel's scope.  The \xff prefix
     * is a non-printable byte that cannot appear in user label strings,
     * preventing collisions with user-chosen widget names. */
    Uint32 sb_id = forge_ui_hash_id(ctx, "\xff__scrollbar");

    /* ── Scrollbar thumb interaction (same drag pattern as slider) ────── */
    bool thumb_over = forge_ui__rect_contains(thumb_rect,
                                               ctx->mouse_x, ctx->mouse_y);
    if (thumb_over) {
        ctx->next_hot = sb_id;
    }

    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    if (mouse_pressed && ctx->next_hot == sb_id) {
        ctx->active = sb_id;
    }

    /* Drag: map mouse y to scroll_y while active */
    if (ctx->active == sb_id && ctx->mouse_down) {
        float drag_t = 0.0f;
        if (thumb_range > 0.0f) {
            drag_t = (ctx->mouse_y - track_y - thumb_h * 0.5f) / thumb_range;
        }
        if (drag_t < 0.0f) drag_t = 0.0f;
        if (drag_t > 1.0f) drag_t = 1.0f;
        *scroll_y = drag_t * max_scroll;
    }

    /* Release: clear active */
    if (ctx->active == sb_id && !ctx->mouse_down) {
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* ── Choose thumb color by state ──────────────────────────────────── */
    ForgeUiColor th_c = forge_ui__accent_color(ctx, sb_id);

    /* Recompute thumb_y after potential drag update */
    t = (max_scroll > 0.0f) ? *scroll_y / max_scroll : 0.0f;
    thumb_y = track_y + t * thumb_range;
    thumb_rect = (ForgeUiRect){ track_x, thumb_y, track_w, thumb_h };
    forge_ui__emit_rect(ctx, thumb_rect, th_c.r, th_c.g, th_c.b, th_c.a);

    /* ── Pop the ID scope pushed by panel_begin ────────────────────────── */
    forge_ui_pop_id(ctx);
}

/* ── Separator implementation ───────────────────────────────────────────── */

static inline void forge_ui_ctx_separator(ForgeUiContext *ctx,
                                           ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas) return;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return;
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    /* 1px line centered vertically within the rect — clamp thickness
     * so we never draw taller than the available rect height. */
    float thickness = rect.h < FORGE_UI_SEPARATOR_THICKNESS
        ? rect.h : FORGE_UI_SEPARATOR_THICKNESS;
    float line_y = rect.y + (rect.h - thickness) * 0.5f;
    ForgeUiRect line = { rect.x, line_y, rect.w, thickness };
    forge_ui__emit_rect(ctx, line,
                        ctx->theme.border.r, ctx->theme.border.g,
                        ctx->theme.border.b, ctx->theme.border.a);
}

static inline void forge_ui_ctx_separator_layout(ForgeUiContext *ctx,
                                                  float size)
{
    if (!ctx || !ctx->atlas) return;
    if (ctx->layout_depth <= 0) return;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    forge_ui_ctx_separator(ctx, rect);
}

/* ── Tree node implementation ──────────────────────────────────────────── */

static inline bool forge_ui_ctx_tree_push(ForgeUiContext *ctx,
                                           const char *label,
                                           bool *open,
                                           ForgeUiRect rect)
{
    if (!ctx) return false;

    /* Record this tree_push call so tree_pop can match it.  Track
     * whether push_id actually succeeded — tree_pop only calls
     * pop_id for levels that were actually pushed. */
    int call_idx = ctx->_tree_call_depth;
    bool pushed_scope = false;

    if (call_idx >= FORGE_UI_ID_STACK_MAX_DEPTH) {
        SDL_Log("forge_ui_ctx_tree_push: tree nesting %d exceeds max %d",
                call_idx, FORGE_UI_ID_STACK_MAX_DEPTH);
        ctx->_tree_call_depth++;
        return false;
    }

    if (!ctx->atlas || !label || !open || label[0] == '\0') {
        pushed_scope = forge_ui_push_id(ctx, label ? label : "__tree_err");
        ctx->_tree_scope_pushed[call_idx] = pushed_scope;
        ctx->_tree_call_depth++;
        return false;
    }
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) {
        pushed_scope = forge_ui_push_id(ctx, label);
        ctx->_tree_scope_pushed[call_idx] = pushed_scope;
        ctx->_tree_call_depth++;
        return false;
    }

    Uint32 id = forge_ui_hash_id(ctx, label);

    /* ── Hit testing (same pattern as button) ──────────────────────────── */
    bool mouse_over = forge_ui__widget_mouse_over(ctx, rect);
    if (mouse_over) {
        ctx->next_hot = id;
    }

    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    if (mouse_pressed && ctx->next_hot == id) {
        ctx->active = id;
    }

    /* Toggle on click */
    if (ctx->active == id && !ctx->mouse_down) {
        if (mouse_over) {
            *open = !(*open);
        }
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* ── Draw background ───────────────────────────────────────────────── */
    ForgeUiColor bg = forge_ui__surface_color(ctx, id);
    forge_ui__emit_rect(ctx, rect, bg.r, bg.g, bg.b, bg.a);

    /* ── Draw expand/collapse indicator ─────────────────────────────────── */
    /* Small square at the left edge showing "+" (collapsed) or "-" (expanded).
     * The indicator is a text character, vertically centered in the rect.
     * Centering uses the same (ascender + metrics.height) approach as buttons. */
    float ascender_px = forge_ui__ascender_px(ctx->atlas);
    ForgeUiTextMetrics ind_m = forge_ui_text_measure(ctx->atlas, "-", NULL);
    float ind_x = rect.x + FORGE_UI_SCALED(ctx, FORGE_UI_TREE_INDICATOR_PAD);
    float text_y = rect.y + (rect.h - ind_m.height) * 0.5f + ascender_px;
    forge_ui_ctx_label(ctx, *open ? "-" : "+", ind_x, text_y);

    /* ── Draw label text ───────────────────────────────────────────────── */
    float label_x = rect.x + FORGE_UI_SCALED(ctx, FORGE_UI_TREE_LABEL_OFFSET);

    /* Extract display text (strip ## suffix if present) */
    const char *disp_end = forge_ui__display_end(label);
    int disp_len = (int)(disp_end - label);
    char disp_buf[256];
    if (disp_len >= (int)sizeof(disp_buf))
        disp_len = (int)sizeof(disp_buf) - 1;
    SDL_memcpy(disp_buf, label, (size_t)disp_len);
    disp_buf[disp_len] = '\0';

    forge_ui_ctx_label(ctx, disp_buf, label_x, text_y);

    /* ── Push ID scope for children ────────────────────────────────────── */
    pushed_scope = forge_ui_push_id(ctx, label);
    ctx->_tree_scope_pushed[call_idx] = pushed_scope;
    ctx->_tree_call_depth++;

    return *open;
}

static inline bool forge_ui_ctx_tree_push_layout(ForgeUiContext *ctx,
                                                  const char *label,
                                                  bool *open,
                                                  float size)
{
    if (!ctx || !ctx->atlas) {
        /* Delegate to tree_push for scope tracking even on error */
        ForgeUiRect dummy = { 0, 0, 0, 0 };
        return forge_ui_ctx_tree_push(ctx, label, open, dummy);
    }
    if (ctx->layout_depth <= 0 || !label || !open || label[0] == '\0') {
        /* Delegate to tree_push for scope tracking even on error,
         * but always return false — we cannot render without a layout. */
        ForgeUiRect dummy = { 0, 0, 0, 0 };
        forge_ui_ctx_tree_push(ctx, label, open, dummy);
        return false;
    }
    /* Validate widget inputs before layout_next() to avoid advancing
     * the layout cursor for widgets that cannot render. */
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_tree_push(ctx, label, open, rect);
}

static inline void forge_ui_ctx_tree_pop(ForgeUiContext *ctx)
{
    if (!ctx) return;
    if (ctx->_tree_call_depth <= 0) return;

    ctx->_tree_call_depth--;
    int call_idx = ctx->_tree_call_depth;

    /* Only pop if the matching tree_push actually succeeded in pushing
     * an ID scope.  _tree_scope_pushed[call_idx] records whether
     * push_id succeeded for this tree_push call.  Without this check,
     * a failed push_id (stack full) would cause pop_id to remove a
     * parent scope, corrupting all subsequent widget IDs. */
    if (call_idx < FORGE_UI_ID_STACK_MAX_DEPTH &&
        ctx->_tree_scope_pushed[call_idx]) {
        ctx->_tree_scope_pushed[call_idx] = false;
        forge_ui_pop_id(ctx);
    }
}

/* ── Sparkline implementation ──────────────────────────────────────────── */

static inline void forge_ui_ctx_sparkline(ForgeUiContext *ctx,
                                           const float *values, int count,
                                           float min_val, float max_val,
                                           ForgeUiColor line_color,
                                           ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !values || count < 2) return;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return;
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;
    if (!forge_isfinite(min_val) || !forge_isfinite(max_val)) return;
    if (max_val <= min_val) return;
    if (!forge_isfinite(line_color.r) || !forge_isfinite(line_color.g) ||
        !forge_isfinite(line_color.b) || !forge_isfinite(line_color.a)) return;

    /* Clamp color components to [0,1] for consistency with other widgets */
    if (line_color.r < 0.0f) line_color.r = 0.0f;
    if (line_color.r > 1.0f) line_color.r = 1.0f;
    if (line_color.g < 0.0f) line_color.g = 0.0f;
    if (line_color.g > 1.0f) line_color.g = 1.0f;
    if (line_color.b < 0.0f) line_color.b = 0.0f;
    if (line_color.b > 1.0f) line_color.b = 1.0f;
    if (line_color.a < 0.0f) line_color.a = 0.0f;
    if (line_color.a > 1.0f) line_color.a = 1.0f;

    /* Draw background using border color */
    forge_ui__emit_rect(ctx, rect,
                        ctx->theme.border.r, ctx->theme.border.g,
                        ctx->theme.border.b, ctx->theme.border.a);

    float range = max_val - min_val;
    float line_w = FORGE_UI_SCALED(ctx, FORGE_UI_SPARKLINE_LINE_WIDTH);

    /* Map a value to a y-coordinate (bottom = min, top = max).
     * The y-axis is inverted in screen space: rect.y is the top. */
    #define SPARK_Y(v) \
        (rect.y + rect.h - ((((v) - min_val) / range) * rect.h))

    float step_x = rect.w / (float)(count - 1);

    for (int i = 0; i < count - 1; i++) {
        float v0 = values[i];
        float v1 = values[i + 1];
        if (!forge_isfinite(v0)) v0 = min_val;
        if (!forge_isfinite(v1)) v1 = min_val;
        if (v0 < min_val) v0 = min_val;
        if (v0 > max_val) v0 = max_val;
        if (v1 < min_val) v1 = min_val;
        if (v1 > max_val) v1 = max_val;

        float x0 = rect.x + step_x * (float)i;
        float y0 = SPARK_Y(v0);
        float x1 = rect.x + step_x * (float)(i + 1);
        float y1 = SPARK_Y(v1);

        /* Draw the segment as a series of 1px-wide column quads,
         * interpolating the y position linearly.  This produces a
         * clean diagonal line at any slope. */
        float half_w = line_w * 0.5f;
        float span = x1 - x0;
        if (span < 1.0f) span = 1.0f;
        int n_cols = (int)SDL_ceilf(span);
        if (n_cols < 1) n_cols = 1;
        if (n_cols > FORGE_UI_SPARKLINE_COL_CAP) n_cols = FORGE_UI_SPARKLINE_COL_CAP;

        for (int c = 0; c < n_cols; c++) {
            float t0 = (float)c / (float)n_cols;
            float t1 = (float)(c + 1) / (float)n_cols;
            float cy0 = y0 + (y1 - y0) * t0;
            float cy1 = y0 + (y1 - y0) * t1;
            float top = (cy0 < cy1 ? cy0 : cy1) - half_w;
            float bot = (cy0 < cy1 ? cy1 : cy0) + half_w;
            /* Clamp to sparkline rect to prevent bleed */
            if (top < rect.y) top = rect.y;
            if (bot > rect.y + rect.h) bot = rect.y + rect.h;
            if (bot <= top) continue;
            ForgeUiRect col_rect = {
                x0 + span * t0,
                top,
                span * (t1 - t0),
                bot - top
            };
            forge_ui__emit_rect(ctx, col_rect,
                                line_color.r, line_color.g,
                                line_color.b, line_color.a);
        }
    }

    #undef SPARK_Y
}

static inline void forge_ui_ctx_sparkline_layout(ForgeUiContext *ctx,
                                                  const float *values,
                                                  int count,
                                                  float min_val, float max_val,
                                                  ForgeUiColor line_color,
                                                  float size)
{
    if (!ctx || !ctx->atlas) return;
    if (ctx->layout_depth <= 0) return;
    /* Validate widget inputs before layout_next() to avoid advancing
     * the layout cursor for widgets that cannot render. */
    if (!values || count < 2) return;
    if (!forge_isfinite(min_val) || !forge_isfinite(max_val) || max_val <= min_val) return;
    if (!forge_isfinite(line_color.r) || !forge_isfinite(line_color.g) ||
        !forge_isfinite(line_color.b) || !forge_isfinite(line_color.a)) return;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    forge_ui_ctx_sparkline(ctx, values, count, min_val, max_val, line_color, rect);
}

/* ── VU meter implementation ───────────────────────────────────────────── */

/* Draw a single VU bar (one channel) filling from bottom to top.
 * Color transitions: green [0..0.5], yellow [0.5..0.8], red [0.8..1.0].
 * The bar is drawn as up to 3 colored segments to show the zone split. */
static inline void forge_ui__vu_bar(ForgeUiContext *ctx,
                                      float level, float peak_hold,
                                      ForgeUiRect bar)
{
    /* Background (darkened border color for consistency) */
    forge_ui__emit_rect(ctx, bar,
                        ctx->theme.border.r * FORGE_UI_VU_BG_DIM,
                        ctx->theme.border.g * FORGE_UI_VU_BG_DIM,
                        ctx->theme.border.b * FORGE_UI_VU_BG_DIM,
                        ctx->theme.border.a);

    if (level <= 0.0f && peak_hold <= 0.0f) return;

    /* Clamp level to [0, 1] */
    if (level > 1.0f) level = 1.0f;

    /* Zone thresholds and colors from named constants */
    float green_end  = FORGE_UI_VU_ZONE_GREEN;
    float yellow_end = FORGE_UI_VU_ZONE_YELLOW;

    float gr = FORGE_UI_VU_GREEN_R,  gg = FORGE_UI_VU_GREEN_G,  gb = FORGE_UI_VU_GREEN_B;
    float yr = FORGE_UI_VU_YELLOW_R, yg = FORGE_UI_VU_YELLOW_G, yb = FORGE_UI_VU_YELLOW_B;
    float rr = FORGE_UI_VU_RED_R,    rg = FORGE_UI_VU_RED_G,    rb = FORGE_UI_VU_RED_B;

    /* Draw filled segments from bottom up.  Bar fills from bottom (y+h)
     * toward top (y).  Each zone occupies a fraction of the total height. */
    float total_h = bar.h;

    /* Green zone: [0, green_end] → bottom portion */
    if (level > 0.0f) {
        float zone_frac = level < green_end ? level : green_end;
        float h = total_h * zone_frac;
        ForgeUiRect seg = {
            bar.x, bar.y + total_h * (1.0f - zone_frac), bar.w, h
        };
        forge_ui__emit_rect(ctx, seg, gr, gg, gb, 1.0f);
    }

    /* Yellow zone: [green_end, yellow_end] */
    if (level > green_end) {
        float zone_top = level < yellow_end ? level : yellow_end;
        float h = total_h * (zone_top - green_end);
        ForgeUiRect seg = {
            bar.x,
            bar.y + total_h * (1.0f - zone_top),
            bar.w,
            h
        };
        forge_ui__emit_rect(ctx, seg, yr, yg, yb, 1.0f);
    }

    /* Red zone: [yellow_end, 1.0] */
    if (level > yellow_end) {
        float zone_top = level; /* already clamped to 1.0 */
        float h = total_h * (zone_top - yellow_end);
        ForgeUiRect seg = {
            bar.x,
            bar.y + total_h * (1.0f - zone_top),
            bar.w,
            h
        };
        forge_ui__emit_rect(ctx, seg, rr, rg, rb, 1.0f);
    }

    /* Peak hold indicator: thin white line at peak_hold position */
    if (peak_hold > FORGE_UI_VU_PEAK_MIN) {
        if (peak_hold > 1.0f) peak_hold = 1.0f;
        float line_h = FORGE_UI_SCALED(ctx, FORGE_UI_VU_PEAK_LINE_H);
        float line_y = bar.y + total_h * (1.0f - peak_hold) - line_h * 0.5f;
        if (line_y < bar.y) line_y = bar.y;
        if (line_y + line_h > bar.y + bar.h) line_h = bar.y + bar.h - line_y;
        if (line_h > 0.0f) {
            ForgeUiRect line_rect = { bar.x, line_y, bar.w, line_h };
            forge_ui__emit_rect(ctx, line_rect, 1.0f, 1.0f, 1.0f,
                                FORGE_UI_VU_PEAK_ALPHA);
        }
    }
}

static inline void forge_ui_ctx_vu_meter(ForgeUiContext *ctx,
                                           float level_l, float level_r,
                                           float peak_l, float peak_r,
                                           ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas) return;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return;
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    /* Sanitize levels */
    if (!forge_isfinite(level_l)) level_l = 0.0f;
    if (!forge_isfinite(level_r)) level_r = 0.0f;
    if (!forge_isfinite(peak_l)) peak_l = 0.0f;
    if (!forge_isfinite(peak_r)) peak_r = 0.0f;

    /* Split rect into two bars with a scaled gap. */
    float gap = FORGE_UI_SCALED(ctx, FORGE_UI_VU_GAP);
    if (gap < 0.0f) gap = 0.0f;
    if (gap > rect.w) gap = rect.w;
    float bar_w = (rect.w - gap) * 0.5f;
    if (bar_w <= 0.0f) return;

    ForgeUiRect left_bar  = { rect.x, rect.y, bar_w, rect.h };
    ForgeUiRect right_bar = { rect.x + bar_w + gap, rect.y, bar_w, rect.h };

    forge_ui__vu_bar(ctx, level_l, peak_l, left_bar);
    forge_ui__vu_bar(ctx, level_r, peak_r, right_bar);
}

static inline void forge_ui_ctx_vu_meter_layout(ForgeUiContext *ctx,
                                                  float level_l, float level_r,
                                                  float peak_l, float peak_r,
                                                  float size)
{
    if (!ctx || !ctx->atlas) return;
    if (ctx->layout_depth <= 0) return;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    forge_ui_ctx_vu_meter(ctx, level_l, level_r, peak_l, peak_r, rect);
}

/* ── HSV/RGB conversion ────────────────────────────────────────────────── */

static inline void forge_ui_hsv_to_rgb(float h, float s, float v,
                                        float *r, float *g, float *b)
{
    if (!r || !g || !b) return;
    /* Replace non-finite inputs with zero before clamping */
    if (!forge_isfinite(h)) h = 0.0f;
    if (!forge_isfinite(s)) s = 0.0f;
    if (!forge_isfinite(v)) v = 0.0f;
    /* Clamp s and v to [0, 1] */
    if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
    if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
    /* Wrap hue to [0, 360) */
    h = SDL_fmodf(h, 360.0f);
    if (h < 0.0f) h += 360.0f;

    float c = v * s;
    float x = c * (1.0f - SDL_fabsf(SDL_fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r1 = 0.0f, g1 = 0.0f, b1 = 0.0f;

    if      (h < 60.0f)  { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120.0f) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 180.0f) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 240.0f) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 300.0f) { r1 = x; g1 = 0; b1 = c; }
    else                 { r1 = c; g1 = 0; b1 = x; }

    *r = r1 + m;
    *g = g1 + m;
    *b = b1 + m;
}

static inline void forge_ui_rgb_to_hsv(float r, float g, float b,
                                        float *h, float *s, float *v)
{
    if (!h || !s || !v) return;
    if (!forge_isfinite(r)) r = 0.0f;
    if (!forge_isfinite(g)) g = 0.0f;
    if (!forge_isfinite(b)) b = 0.0f;
    if (r < 0.0f) r = 0.0f; else if (r > 1.0f) r = 1.0f;
    if (g < 0.0f) g = 0.0f; else if (g > 1.0f) g = 1.0f;
    if (b < 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f;

    float cmax = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float cmin = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float delta = cmax - cmin;

    *v = cmax;
    *s = (cmax > 0.0f) ? delta / cmax : 0.0f;

    if (delta < 1e-6f) {
        *h = 0.0f;
    } else if (cmax == r) {
        *h = 60.0f * SDL_fmodf((g - b) / delta, 6.0f);
    } else if (cmax == g) {
        *h = 60.0f * ((b - r) / delta + 2.0f);
    } else {
        *h = 60.0f * ((r - g) / delta + 4.0f);
    }
    if (*h < 0.0f) *h += 360.0f;
}

/* ── Drag float implementation ─────────────────────────────────────────── */

static inline bool forge_ui_ctx_drag_float(ForgeUiContext *ctx,
                                            const char *label,
                                            float *value, float speed,
                                            float min_val, float max_val,
                                            ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !value || !label || label[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;
    if (!forge_isfinite(min_val) || !forge_isfinite(max_val)) return false;
    if (!(max_val > min_val)) return false;
    if (!forge_isfinite(speed) || !(speed > 0.0f)) return false;
    if (!forge_isfinite(*value)) *value = min_val;
    Uint32 id = forge_ui_hash_id(ctx, label);

    bool changed = false;

    /* ── Hit testing ─────────────────────────────────────────────────── */
    bool mouse_over = forge_ui__widget_mouse_over(ctx, rect);
    if (mouse_over) ctx->next_hot = id;

    /* ── State transitions ───────────────────────────────────────────── */
    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    bool just_activated = false;
    if (mouse_pressed && ctx->next_hot == id) {
        ctx->active = id;
        just_activated = true;
    }

    /* ── Value update while dragging ─────────────────────────────────── */
    /* Skip delta on the activation frame — the cursor may have traveled
     * a large distance to reach the widget, producing a visible jump. */
    if (ctx->active == id && ctx->mouse_down && !just_activated) {
        float dx = ctx->mouse_x - ctx->mouse_x_prev;
        float new_val = *value + dx * speed;
        if (new_val < min_val) new_val = min_val;
        if (new_val > max_val) new_val = max_val;
        if (new_val != *value) {
            *value = new_val;
            changed = true;
        }
    }

    /* ── Release ─────────────────────────────────────────────────────── */
    if (ctx->active == id && !ctx->mouse_down) ctx->active = FORGE_UI_ID_NONE;

    /* ── Draw background ─────────────────────────────────────────────── */
    ForgeUiColor bg = forge_ui__surface_color(ctx, id);
    forge_ui__emit_rect(ctx, rect, bg.r, bg.g, bg.b, bg.a);

    /* ── Draw value text (centered) ──────────────────────────────────── */
    char val_buf[32];
    SDL_snprintf(val_buf, sizeof(val_buf), "%.2f", (double)*value);
    ForgeUiTextMetrics m = forge_ui_text_measure(ctx->atlas, val_buf, NULL);
    float ascender_px = forge_ui__ascender_px(ctx->atlas);
    float tx = rect.x + (rect.w - m.width) * 0.5f;
    float ty = rect.y + (rect.h - m.height) * 0.5f + ascender_px;
    forge_ui_ctx_label(ctx, val_buf, tx, ty);

    return changed;
}

static inline bool forge_ui_ctx_drag_float_layout(ForgeUiContext *ctx,
                                                    const char *label,
                                                    float *value, float speed,
                                                    float min_val, float max_val,
                                                    float size)
{
    if (!ctx || !ctx->atlas || !value || !label || label[0] == '\0') return false;
    if (!forge_isfinite(min_val) || !forge_isfinite(max_val)) return false;
    if (!(max_val > min_val)) return false;
    if (!forge_isfinite(speed) || !(speed > 0.0f)) return false;
    if (ctx->layout_depth <= 0) return false;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_drag_float(ctx, label, value, speed, min_val, max_val, rect);
}

/* ── Drag float N (multi-component) ────────────────────────────────────── */

static inline bool forge_ui_ctx_drag_float_n(ForgeUiContext *ctx,
                                              const char *label,
                                              float *values, int count,
                                              float speed,
                                              float min_val, float max_val,
                                              ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !values || !label || label[0] == '\0') return false;
    if (count < 1 || count > 4) return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;
    if (!forge_isfinite(min_val) || !forge_isfinite(max_val)) return false;
    if (!(max_val > min_val)) return false;
    if (!forge_isfinite(speed) || !(speed > 0.0f)) return false;

    /* Component labels and colors (X=red, Y=green, Z=blue, W=yellow) */
    static const char *comp_labels[] = { "X", "Y", "Z", "W" };
    static const float comp_colors[][3] = {
        { FORGE_UI_DRAG_COMP_COLOR_R_R, FORGE_UI_DRAG_COMP_COLOR_R_G, FORGE_UI_DRAG_COMP_COLOR_R_B },
        { FORGE_UI_DRAG_COMP_COLOR_G_R, FORGE_UI_DRAG_COMP_COLOR_G_G, FORGE_UI_DRAG_COMP_COLOR_G_B },
        { FORGE_UI_DRAG_COMP_COLOR_B_R, FORGE_UI_DRAG_COMP_COLOR_B_G, FORGE_UI_DRAG_COMP_COLOR_B_B },
        { FORGE_UI_DRAG_COMP_COLOR_W_R, FORGE_UI_DRAG_COMP_COLOR_W_G, FORGE_UI_DRAG_COMP_COLOR_W_B },
    };

    float gap = FORGE_UI_SCALED(ctx, FORGE_UI_DRAG_LABEL_GAP);
    float ascender_px = forge_ui__ascender_px(ctx->atlas);

    /* Measure the widest component label so proportional fonts
     * (where "W" is wider than "X") don't overlap the drag field. */
    float max_label_w = 0.0f;
    float max_label_h = 0.0f;
    for (int i = 0; i < count; i++) {
        ForgeUiTextMetrics m =
            forge_ui_text_measure(ctx->atlas, comp_labels[i], NULL);
        if (m.width  > max_label_w) max_label_w = m.width;
        if (m.height > max_label_h) max_label_h = m.height;
    }
    float label_w = max_label_w + gap;

    /* Each component gets an equal share of the total width */
    float comp_w = rect.w / (float)count;
    bool any_changed = false;

    if (!forge_ui_push_id(ctx, label)) return false;

    for (int i = 0; i < count; i++) {
        float cx = rect.x + comp_w * (float)i;

        /* Skip if component slot is too narrow for label + field */
        float field_w = comp_w - label_w - gap * 0.5f;
        if (field_w < 1e-3f) continue;

        /* Draw colored component label (X/Y/Z/W) */
        ForgeUiRect label_rect = { cx, rect.y, label_w, rect.h };
        forge_ui__emit_rect(ctx, label_rect,
                            comp_colors[i][0] * 0.6f,
                            comp_colors[i][1] * 0.6f,
                            comp_colors[i][2] * 0.6f, 1.0f);
        float ly = rect.y + (rect.h - max_label_h) * 0.5f + ascender_px;
        forge_ui_ctx_label_colored(ctx, comp_labels[i],
                                    cx + gap * 0.5f, ly,
                                    1.0f, 1.0f, 1.0f, 1.0f);

        /* Drag field for this component */
        ForgeUiRect field_rect = {
            cx + label_w, rect.y, field_w, rect.h
        };
        char comp_id[16];
        SDL_snprintf(comp_id, sizeof(comp_id), "##c%d", i);
        if (forge_ui_ctx_drag_float(ctx, comp_id, &values[i],
                                     speed, min_val, max_val, field_rect)) {
            any_changed = true;
        }
    }

    forge_ui_pop_id(ctx);
    return any_changed;
}

static inline bool forge_ui_ctx_drag_float_n_layout(ForgeUiContext *ctx,
                                                      const char *label,
                                                      float *values, int count,
                                                      float speed,
                                                      float min_val, float max_val,
                                                      float size)
{
    if (!ctx || !ctx->atlas || !values || !label || label[0] == '\0') return false;
    if (count < 1 || count > 4) return false;
    if (!forge_isfinite(min_val) || !forge_isfinite(max_val)) return false;
    if (!(max_val > min_val)) return false;
    if (!forge_isfinite(speed) || !(speed > 0.0f)) return false;
    if (ctx->layout_depth <= 0) return false;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_drag_float_n(ctx, label, values, count,
                                      speed, min_val, max_val, rect);
}

/* ── Drag int implementation ───────────────────────────────────────────── */

static inline bool forge_ui_ctx_drag_int(ForgeUiContext *ctx,
                                          const char *label,
                                          int *value, float speed,
                                          int min_val, int max_val,
                                          ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !value || !label || label[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;
    if (max_val <= min_val) return false;
    if (!forge_isfinite(speed) || !(speed > 0.0f)) return false;
    Uint32 id = forge_ui_hash_id(ctx, label);

    bool changed = false;

    /* ── Hit testing ─────────────────────────────────────────────────── */
    bool mouse_over = forge_ui__widget_mouse_over(ctx, rect);
    if (mouse_over) ctx->next_hot = id;

    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    bool just_activated = false;
    if (mouse_pressed && ctx->next_hot == id) {
        ctx->active = id;
        ctx->drag_int_accum = 0.0f;
        just_activated = true;
    }

    /* ── Value update while dragging ─────────────────────────────────── */
    /* Accumulate fractional drag distance so sub-unit movement is not
     * lost between frames.  Only quantize to int when the accumulated
     * value crosses a threshold. */
    if (ctx->active == id && ctx->mouse_down && !just_activated) {
        float dx = ctx->mouse_x - ctx->mouse_x_prev;
        ctx->drag_int_accum += dx * speed;
        float fval = (float)*value + ctx->drag_int_accum;
        /* Clamp to int-safe range before cast to avoid UB */
        if (fval < (float)min_val) fval = (float)min_val;
        if (fval > (float)max_val) fval = (float)max_val;
        int new_val = (int)(fval + (fval >= 0.0f ? 0.5f : -0.5f));
        if (new_val < min_val) new_val = min_val;
        if (new_val > max_val) new_val = max_val;
        /* Keep only the fractional remainder so overshoot at min/max
         * boundaries does not accumulate and "stick" the control. */
        ctx->drag_int_accum = fval - (float)new_val;
        if (new_val != *value) {
            *value = new_val;
            changed = true;
        }
    }

    if (ctx->active == id && !ctx->mouse_down) ctx->active = FORGE_UI_ID_NONE;

    /* ── Draw background ─────────────────────────────────────────────── */
    ForgeUiColor bg = forge_ui__surface_color(ctx, id);
    forge_ui__emit_rect(ctx, rect, bg.r, bg.g, bg.b, bg.a);

    /* ── Draw value text (centered) ──────────────────────────────────── */
    char val_buf[32];
    SDL_snprintf(val_buf, sizeof(val_buf), "%d", *value);
    ForgeUiTextMetrics m = forge_ui_text_measure(ctx->atlas, val_buf, NULL);
    float ascender_px = forge_ui__ascender_px(ctx->atlas);
    float tx = rect.x + (rect.w - m.width) * 0.5f;
    float ty = rect.y + (rect.h - m.height) * 0.5f + ascender_px;
    forge_ui_ctx_label(ctx, val_buf, tx, ty);

    return changed;
}

static inline bool forge_ui_ctx_drag_int_layout(ForgeUiContext *ctx,
                                                  const char *label,
                                                  int *value, float speed,
                                                  int min_val, int max_val,
                                                  float size)
{
    if (!ctx || !ctx->atlas || !value || !label || label[0] == '\0') return false;
    if (max_val <= min_val) return false;
    if (!forge_isfinite(speed) || !(speed > 0.0f)) return false;
    if (ctx->layout_depth <= 0) return false;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_drag_int(ctx, label, value, speed, min_val, max_val, rect);
}

/* ── Drag int N (multi-component) ──────────────────────────────────────── */

static inline bool forge_ui_ctx_drag_int_n(ForgeUiContext *ctx,
                                            const char *label,
                                            int *values, int count,
                                            float speed,
                                            int min_val, int max_val,
                                            ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !values || !label || label[0] == '\0') return false;
    if (count < 1 || count > 4) return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;
    if (max_val <= min_val) return false;
    if (!forge_isfinite(speed) || !(speed > 0.0f)) return false;

    /* Component labels and colors (X=red, Y=green, Z=blue, W=yellow) */
    static const char *comp_labels[] = { "X", "Y", "Z", "W" };
    static const float comp_colors[][3] = {
        { FORGE_UI_DRAG_COMP_COLOR_R_R, FORGE_UI_DRAG_COMP_COLOR_R_G, FORGE_UI_DRAG_COMP_COLOR_R_B },
        { FORGE_UI_DRAG_COMP_COLOR_G_R, FORGE_UI_DRAG_COMP_COLOR_G_G, FORGE_UI_DRAG_COMP_COLOR_G_B },
        { FORGE_UI_DRAG_COMP_COLOR_B_R, FORGE_UI_DRAG_COMP_COLOR_B_G, FORGE_UI_DRAG_COMP_COLOR_B_B },
        { FORGE_UI_DRAG_COMP_COLOR_W_R, FORGE_UI_DRAG_COMP_COLOR_W_G, FORGE_UI_DRAG_COMP_COLOR_W_B },
    };

    float gap = FORGE_UI_SCALED(ctx, FORGE_UI_DRAG_LABEL_GAP);
    float ascender_px = forge_ui__ascender_px(ctx->atlas);

    /* Measure the widest component label so proportional fonts
     * (where "W" is wider than "X") don't overlap the drag field. */
    float max_label_w = 0.0f;
    float max_label_h = 0.0f;
    for (int i = 0; i < count; i++) {
        ForgeUiTextMetrics m =
            forge_ui_text_measure(ctx->atlas, comp_labels[i], NULL);
        if (m.width  > max_label_w) max_label_w = m.width;
        if (m.height > max_label_h) max_label_h = m.height;
    }
    float label_w = max_label_w + gap;
    float comp_w = rect.w / (float)count;
    bool any_changed = false;

    if (!forge_ui_push_id(ctx, label)) return false;

    for (int i = 0; i < count; i++) {
        float cx = rect.x + comp_w * (float)i;

        /* Skip if component slot is too narrow for label + field */
        float field_w = comp_w - label_w - gap * 0.5f;
        if (field_w < 1e-3f) continue;

        ForgeUiRect lr = { cx, rect.y, label_w, rect.h };
        forge_ui__emit_rect(ctx, lr,
                            comp_colors[i][0] * 0.6f,
                            comp_colors[i][1] * 0.6f,
                            comp_colors[i][2] * 0.6f, 1.0f);
        float ly = rect.y + (rect.h - max_label_h) * 0.5f + ascender_px;
        forge_ui_ctx_label_colored(ctx, comp_labels[i],
                                    cx + gap * 0.5f, ly,
                                    1.0f, 1.0f, 1.0f, 1.0f);

        ForgeUiRect field_rect = {
            cx + label_w, rect.y, field_w, rect.h
        };
        char comp_id[16];
        SDL_snprintf(comp_id, sizeof(comp_id), "##c%d", i);
        if (forge_ui_ctx_drag_int(ctx, comp_id, &values[i],
                                   speed, min_val, max_val, field_rect)) {
            any_changed = true;
        }
    }

    forge_ui_pop_id(ctx);
    return any_changed;
}

static inline bool forge_ui_ctx_drag_int_n_layout(ForgeUiContext *ctx,
                                                    const char *label,
                                                    int *values, int count,
                                                    float speed,
                                                    int min_val, int max_val,
                                                    float size)
{
    if (!ctx || !ctx->atlas || !values || !label || label[0] == '\0') return false;
    if (count < 1 || count > 4) return false;
    if (max_val <= min_val) return false;
    if (!forge_isfinite(speed) || !(speed > 0.0f)) return false;
    if (ctx->layout_depth <= 0) return false;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_drag_int_n(ctx, label, values, count,
                                    speed, min_val, max_val, rect);
}

/* ── Listbox implementation ────────────────────────────────────────────── */

static inline bool forge_ui_ctx_listbox(ForgeUiContext *ctx,
                                         const char *label,
                                         int *selected,
                                         const char *const *items,
                                         int item_count,
                                         ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !label || !selected || !items) return false;
    if (item_count <= 0 || label[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;

    /* Allow -1 as "no selection"; clamp only upper bound */
    if (*selected >= item_count) *selected = item_count - 1;

    bool changed = false;
    float item_h = FORGE_UI_SCALED(ctx, FORGE_UI_LB_ITEM_HEIGHT);
    float ascender_px = forge_ui__ascender_px(ctx->atlas);
    float pad = FORGE_UI_SCALED(ctx, ctx->spacing.text_input_padding);

    /* Draw list background */
    forge_ui__emit_rect(ctx, rect,
                        ctx->theme.surface.r, ctx->theme.surface.g,
                        ctx->theme.surface.b, ctx->theme.surface.a);

    if (!forge_ui_push_id(ctx, label)) return false;

    /* Save and set clip rect for the list area */
    bool had_clip = ctx->has_clip;
    ForgeUiRect prev_clip = ctx->clip_rect;
    if (had_clip) {
        /* Intersect with existing clip rect */
        float cx0 = ctx->clip_rect.x;
        float cy0 = ctx->clip_rect.y;
        float cx1 = cx0 + ctx->clip_rect.w;
        float cy1 = cy0 + ctx->clip_rect.h;
        float rx0 = rect.x > cx0 ? rect.x : cx0;
        float ry0 = rect.y > cy0 ? rect.y : cy0;
        float rx1 = (rect.x + rect.w) < cx1 ? (rect.x + rect.w) : cx1;
        float ry1 = (rect.y + rect.h) < cy1 ? (rect.y + rect.h) : cy1;
        ctx->clip_rect = (ForgeUiRect){ rx0, ry0, rx1 - rx0, ry1 - ry0 };
    } else {
        ctx->clip_rect = rect;
    }
    ctx->has_clip = true;

    for (int i = 0; i < item_count; i++) {
        float iy = rect.y + item_h * (float)i;
        if (iy + item_h < rect.y || iy > rect.y + rect.h) continue;

        ForgeUiRect item_rect = { rect.x, iy, rect.w, item_h };
        char item_id[32];
        SDL_snprintf(item_id, sizeof(item_id), "##item%d", i);
        Uint32 id = forge_ui_hash_id(ctx, item_id);

        bool mouse_over = forge_ui__widget_mouse_over(ctx, item_rect);
        if (mouse_over) ctx->next_hot = id;

        bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
        if (mouse_pressed && ctx->next_hot == id) ctx->active = id;

        if (ctx->active == id && !ctx->mouse_down) {
            if (mouse_over && *selected != i) {
                *selected = i;
                changed = true;
            }
            ctx->active = FORGE_UI_ID_NONE;
        }

        /* Draw item background — selected items get accent color */
        if (*selected == i) {
            forge_ui__emit_rect(ctx, item_rect,
                                ctx->theme.accent.r * 0.4f,
                                ctx->theme.accent.g * 0.4f,
                                ctx->theme.accent.b * 0.4f, 1.0f);
        } else if (ctx->hot == id) {
            forge_ui__emit_rect(ctx, item_rect,
                                ctx->theme.surface_hot.r,
                                ctx->theme.surface_hot.g,
                                ctx->theme.surface_hot.b,
                                ctx->theme.surface_hot.a);
        }

        /* Draw item text */
        if (items[i]) {
            float ty = iy + (item_h - ctx->atlas->pixel_height) * 0.5f
                      + ascender_px;
            forge_ui_ctx_label(ctx, items[i], rect.x + pad, ty);
        }
    }

    /* Restore clip state */
    ctx->has_clip = had_clip;
    ctx->clip_rect = prev_clip;

    forge_ui_pop_id(ctx);
    return changed;
}

static inline bool forge_ui_ctx_listbox_layout(ForgeUiContext *ctx,
                                                const char *label,
                                                int *selected,
                                                const char *const *items,
                                                int item_count,
                                                float size)
{
    if (!ctx || !ctx->atlas || !label || !selected || !items) return false;
    if (item_count <= 0 || label[0] == '\0') return false;
    if (ctx->layout_depth <= 0) return false;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_listbox(ctx, label, selected, items, item_count, rect);
}

/* ── Dropdown implementation ───────────────────────────────────────────── */

static inline bool forge_ui_ctx_dropdown(ForgeUiContext *ctx,
                                          const char *label,
                                          int *selected, bool *open,
                                          const char *const *items,
                                          int item_count,
                                          ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !label || !selected || !open || !items) return false;
    if (item_count <= 0 || label[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;

    /* Clamp *selected into valid range */
    if (*selected < 0) *selected = 0;
    if (*selected >= item_count) *selected = item_count - 1;

    bool changed = false;
    float default_h = FORGE_UI_SCALED(ctx, FORGE_UI_DD_HEADER_HEIGHT);
    float header_h = (rect.h > 0.0f) ? rect.h : default_h;
    float arrow_pad = FORGE_UI_SCALED(ctx, FORGE_UI_DD_ARROW_PAD);
    float pad = FORGE_UI_SCALED(ctx, ctx->spacing.text_input_padding);
    float ascender_px = forge_ui__ascender_px(ctx->atlas);

    if (!forge_ui_push_id(ctx, label)) return false;

    /* ── Header button ───────────────────────────────────────────────── */
    ForgeUiRect header_rect = { rect.x, rect.y, rect.w, header_h };
    {
        Uint32 hdr_id = forge_ui_hash_id(ctx, "##hdr");
        bool mouse_over = forge_ui__widget_mouse_over(ctx, header_rect);
        if (mouse_over) ctx->next_hot = hdr_id;

        bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
        if (mouse_pressed && ctx->next_hot == hdr_id) ctx->active = hdr_id;

        if (ctx->active == hdr_id && !ctx->mouse_down) {
            if (mouse_over) *open = !(*open);
            ctx->active = FORGE_UI_ID_NONE;
        }

        ForgeUiColor bg = forge_ui__surface_color(ctx, hdr_id);
        forge_ui__emit_rect(ctx, header_rect, bg.r, bg.g, bg.b, bg.a);

        /* Draw current selection text */
        const char *sel_text = (*selected >= 0 && *selected < item_count)
                                ? items[*selected] : "---";
        float ty = rect.y + (header_h - ctx->atlas->pixel_height) * 0.5f
                  + ascender_px;
        forge_ui_ctx_label(ctx, sel_text, rect.x + pad, ty);

        /* Draw arrow indicator */
        const char *arrow = *open ? "-" : "+";
        ForgeUiTextMetrics am = forge_ui_text_measure(ctx->atlas, arrow, NULL);
        forge_ui_ctx_label(ctx, arrow,
                           rect.x + rect.w - am.width - arrow_pad, ty);
    }

    /* ── Expanded item list (drawn below header when open) ───────────── */
    if (*open) {
        float item_h = FORGE_UI_SCALED(ctx, FORGE_UI_LB_ITEM_HEIGHT);

        for (int i = 0; i < item_count; i++) {
            float iy = rect.y + header_h + item_h * (float)i;
            ForgeUiRect item_rect = { rect.x, iy, rect.w, item_h };

            char item_id[32];
            SDL_snprintf(item_id, sizeof(item_id), "##dd%d", i);
            Uint32 id = forge_ui_hash_id(ctx, item_id);

            bool mouse_over = forge_ui__widget_mouse_over(ctx, item_rect);
            if (mouse_over) ctx->next_hot = id;

            bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
            if (mouse_pressed && ctx->next_hot == id) ctx->active = id;

            if (ctx->active == id && !ctx->mouse_down) {
                if (mouse_over) {
                    if (*selected != i) {
                        *selected = i;
                        changed = true;
                    }
                    *open = false;
                }
                ctx->active = FORGE_UI_ID_NONE;
            }

            /* Item background */
            if (*selected == i) {
                forge_ui__emit_rect(ctx, item_rect,
                                    ctx->theme.accent.r * 0.4f,
                                    ctx->theme.accent.g * 0.4f,
                                    ctx->theme.accent.b * 0.4f, 1.0f);
            } else if (ctx->hot == id) {
                forge_ui__emit_rect(ctx, item_rect,
                                    ctx->theme.surface_hot.r,
                                    ctx->theme.surface_hot.g,
                                    ctx->theme.surface_hot.b,
                                    ctx->theme.surface_hot.a);
            } else {
                forge_ui__emit_rect(ctx, item_rect,
                                    ctx->theme.surface.r,
                                    ctx->theme.surface.g,
                                    ctx->theme.surface.b,
                                    ctx->theme.surface.a);
            }

            /* Item text */
            if (items[i]) {
                float ty = iy + (item_h - ctx->atlas->pixel_height) * 0.5f
                          + ascender_px;
                forge_ui_ctx_label(ctx, items[i], rect.x + pad, ty);
            }
        }
    }

    forge_ui_pop_id(ctx);
    return changed;
}

static inline bool forge_ui_ctx_dropdown_layout(ForgeUiContext *ctx,
                                                  const char *label,
                                                  int *selected, bool *open,
                                                  const char *const *items,
                                                  int item_count,
                                                  float size)
{
    if (!ctx || !ctx->atlas || !label || !selected || !open || !items) return false;
    if (ctx->layout_depth <= 0) return false;
    if (item_count <= 0) return false;
    /* In vertical layouts the caller's size controls the header height;
     * fall back to the default when size is zero or non-finite. */
    float default_h = FORGE_UI_SCALED(ctx, FORGE_UI_DD_HEADER_HEIGHT);
    ForgeUiLayoutDirection dir =
        ctx->layout_stack[ctx->layout_depth - 1].direction;
    float header_h = (dir == FORGE_UI_LAYOUT_VERTICAL && forge_isfinite(size) && size > 0.0f)
        ? size : default_h;
    /* Reserve extra height for the open menu so subsequent widgets
     * do not overlap the dropdown items.  We predict the post-toggle
     * state of *open so the reserved space matches what
     * forge_ui_ctx_dropdown() will actually draw this frame.
     *
     * In a vertical layout, size = height, so we reserve header + items.
     * In a horizontal layout, size = width, so we reserve the caller's
     * requested width (size parameter); the expanded items drop below
     * the header and do not affect horizontal advancement. */

    /* Compute effective_open: the state *open will have after the
     * header mouse-up toggle inside forge_ui_ctx_dropdown().  This
     * replicates the toggle logic (push_id → hash "##hdr" → check
     * active/mouse/hover) without advancing the layout cursor. */
    bool effective_open = *open;
    if (dir == FORGE_UI_LAYOUT_VERTICAL) {
        ForgeUiLayout *layout = &ctx->layout_stack[ctx->layout_depth - 1];
        float peek_y = layout->cursor_y;
        if (layout->item_count > 0) peek_y += layout->spacing;
        ForgeUiRect peek_hdr = { layout->cursor_x, peek_y,
                                 layout->remaining_w, header_h };
        if (ctx->_panel_active && ctx->_panel.scroll_y)
            peek_hdr.y -= *ctx->_panel.scroll_y;

        bool pushed_peek = forge_ui_push_id(ctx, label);
        Uint32 hdr_id = forge_ui_hash_id(ctx, "##hdr");
        forge_ui_pop_id_if(ctx, pushed_peek);

        if (ctx->active == hdr_id && !ctx->mouse_down) {
            if (forge_ui__widget_mouse_over(ctx, peek_hdr))
                effective_open = !effective_open;
        }
    }

    float reserve;
    if (dir == FORGE_UI_LAYOUT_VERTICAL) {
        reserve = header_h;
        if (effective_open) {
            reserve += FORGE_UI_SCALED(ctx, FORGE_UI_LB_ITEM_HEIGHT)
                       * (float)item_count;
        }
    } else {
        reserve = size;
    }
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, reserve);
    rect.h = header_h;  /* dropdown itself only uses the header height */
    return forge_ui_ctx_dropdown(ctx, label, selected, open,
                                  items, item_count, rect);
}

/* ── Radio button implementation ───────────────────────────────────────── */

static inline bool forge_ui_ctx_radio(ForgeUiContext *ctx,
                                       const char *label,
                                       int *selected, int option_value,
                                       ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !label || !selected || label[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;
    Uint32 id = forge_ui_hash_id(ctx, label);

    bool changed = false;

    /* ── Hit testing ─────────────────────────────────────────────────── */
    bool mouse_over = forge_ui__widget_mouse_over(ctx, rect);
    if (mouse_over) ctx->next_hot = id;

    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    if (mouse_pressed && ctx->next_hot == id) ctx->active = id;

    if (ctx->active == id && !ctx->mouse_down) {
        if (mouse_over && *selected != option_value) {
            *selected = option_value;
            changed = true;
        }
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* ── Draw radio circle (approximated as a box — the atlas has no
     *    circle glyph, so we use a small square like checkbox) ───────── */
    float radio_size = FORGE_UI_SCALED(ctx, FORGE_UI_RADIO_SIZE);
    float inner_pad = FORGE_UI_SCALED(ctx, FORGE_UI_RADIO_INNER_PAD);

    float box_x = rect.x;
    float box_y = rect.y + (rect.h - radio_size) * 0.5f;
    ForgeUiRect outer = { box_x, box_y, radio_size, radio_size };

    ForgeUiColor box_c = forge_ui__surface_color(ctx, id);
    forge_ui__emit_rect(ctx, outer, box_c.r, box_c.g, box_c.b, box_c.a);

    /* Inner fill when selected */
    if (*selected == option_value) {
        ForgeUiRect inner = {
            box_x + inner_pad, box_y + inner_pad,
            radio_size - 2.0f * inner_pad,
            radio_size - 2.0f * inner_pad
        };
        forge_ui__emit_rect(ctx, inner,
                            ctx->theme.accent.r, ctx->theme.accent.g,
                            ctx->theme.accent.b, ctx->theme.accent.a);
    }

    /* ── Label text ──────────────────────────────────────────────────── */
    const char *disp_end = forge_ui__display_end(label);
    int disp_len = (int)(disp_end - label);
    char disp_buf[256];
    if (disp_len >= (int)sizeof(disp_buf))
        disp_len = (int)sizeof(disp_buf) - 1;
    SDL_memcpy(disp_buf, label, (size_t)disp_len);
    disp_buf[disp_len] = '\0';

    float ascender_px = forge_ui__ascender_px(ctx->atlas);
    float label_x = box_x + radio_size
                  + FORGE_UI_SCALED(ctx, FORGE_UI_RADIO_LABEL_GAP);
    float label_y = rect.y + (rect.h - ctx->atlas->pixel_height) * 0.5f
                  + ascender_px;
    forge_ui_ctx_label(ctx, disp_buf, label_x, label_y);

    return changed;
}

static inline bool forge_ui_ctx_radio_layout(ForgeUiContext *ctx,
                                               const char *label,
                                               int *selected,
                                               int option_value,
                                               float size)
{
    if (!ctx || !ctx->atlas || !label || !selected || label[0] == '\0') return false;
    if (ctx->layout_depth <= 0) return false;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_radio(ctx, label, selected, option_value, rect);
}

/* ── Color picker implementation ───────────────────────────────────────── */

/* Internal: emit a quad with per-vertex colors (for gradients).
 * Vertices are ordered: top-left, top-right, bottom-right, bottom-left.
 * Each corner has its own RGB color. */
static inline void forge_ui__emit_gradient_rect(ForgeUiContext *ctx,
                                                  ForgeUiRect rect,
                                                  float tl_r, float tl_g, float tl_b,
                                                  float tr_r, float tr_g, float tr_b,
                                                  float br_r, float br_g, float br_b,
                                                  float bl_r, float bl_g, float bl_b)
{
    if (!ctx || !ctx->atlas) return;

    /* ── Clip against clip_rect if active, interpolating corner colors ── */
    if (ctx->has_clip) {
        float cx0 = ctx->clip_rect.x;
        float cy0 = ctx->clip_rect.y;
        float cx1 = cx0 + ctx->clip_rect.w;
        float cy1 = cy0 + ctx->clip_rect.h;

        float rx0 = rect.x;
        float ry0 = rect.y;
        float rx1 = rect.x + rect.w;
        float ry1 = rect.y + rect.h;

        /* Fully outside — discard */
        if (rx1 <= cx0 || rx0 >= cx1 || ry1 <= cy0 || ry0 >= cy1) return;

        /* Compute fractional clip bounds for color interpolation */
        float orig_w = rect.w > 0.0f ? rect.w : 1.0f;
        float orig_h = rect.h > 0.0f ? rect.h : 1.0f;

        float new_x0 = rx0 < cx0 ? cx0 : rx0;
        float new_y0 = ry0 < cy0 ? cy0 : ry0;
        float new_x1 = rx1 > cx1 ? cx1 : rx1;
        float new_y1 = ry1 > cy1 ? cy1 : ry1;

        if (new_x1 - new_x0 <= 0.0f || new_y1 - new_y0 <= 0.0f) return;

        /* Interpolation fractions within the original rect */
        float fl = (new_x0 - rx0) / orig_w;  /* left   fraction */
        float fr = (new_x1 - rx0) / orig_w;  /* right  fraction */
        float ft = (new_y0 - ry0) / orig_h;  /* top    fraction */
        float fb = (new_y1 - ry0) / orig_h;  /* bottom fraction */

        /* Bilinear interpolation of corner colors for clipped corners */
#define LERP2(a, b, t) ((a) + ((b) - (a)) * (t))
#define BILERP(tl, tr, bl, br, fx, fy) \
    LERP2(LERP2((tl), (tr), (fx)), LERP2((bl), (br), (fx)), (fy))

        float c_tl_r = BILERP(tl_r, tr_r, bl_r, br_r, fl, ft);
        float c_tl_g = BILERP(tl_g, tr_g, bl_g, br_g, fl, ft);
        float c_tl_b = BILERP(tl_b, tr_b, bl_b, br_b, fl, ft);

        float c_tr_r = BILERP(tl_r, tr_r, bl_r, br_r, fr, ft);
        float c_tr_g = BILERP(tl_g, tr_g, bl_g, br_g, fr, ft);
        float c_tr_b = BILERP(tl_b, tr_b, bl_b, br_b, fr, ft);

        float c_br_r = BILERP(tl_r, tr_r, bl_r, br_r, fr, fb);
        float c_br_g = BILERP(tl_g, tr_g, bl_g, br_g, fr, fb);
        float c_br_b = BILERP(tl_b, tr_b, bl_b, br_b, fr, fb);

        float c_bl_r = BILERP(tl_r, tr_r, bl_r, br_r, fl, fb);
        float c_bl_g = BILERP(tl_g, tr_g, bl_g, br_g, fl, fb);
        float c_bl_b = BILERP(tl_b, tr_b, bl_b, br_b, fl, fb);

#undef BILERP
#undef LERP2

        rect.x = new_x0;
        rect.y = new_y0;
        rect.w = new_x1 - new_x0;
        rect.h = new_y1 - new_y0;
        tl_r = c_tl_r; tl_g = c_tl_g; tl_b = c_tl_b;
        tr_r = c_tr_r; tr_g = c_tr_g; tr_b = c_tr_b;
        br_r = c_br_r; br_g = c_br_g; br_b = c_br_b;
        bl_r = c_bl_r; bl_g = c_bl_g; bl_b = c_bl_b;
    }

    if (!forge_ui__grow_vertices(ctx, 4)) return;
    if (!forge_ui__grow_indices(ctx, 6)) return;

    const ForgeUiUVRect *wuv = &ctx->atlas->white_uv;
    float u = (wuv->u0 + wuv->u1) * 0.5f;
    float v = (wuv->v0 + wuv->v1) * 0.5f;

    Uint32 base = (Uint32)ctx->vertex_count;
    ForgeUiVertex *verts = &ctx->vertices[ctx->vertex_count];
    verts[0] = (ForgeUiVertex){ rect.x,          rect.y,          u, v, tl_r, tl_g, tl_b, 1.0f };
    verts[1] = (ForgeUiVertex){ rect.x + rect.w, rect.y,          u, v, tr_r, tr_g, tr_b, 1.0f };
    verts[2] = (ForgeUiVertex){ rect.x + rect.w, rect.y + rect.h, u, v, br_r, br_g, br_b, 1.0f };
    verts[3] = (ForgeUiVertex){ rect.x,          rect.y + rect.h, u, v, bl_r, bl_g, bl_b, 1.0f };
    ctx->vertex_count += 4;

    Uint32 *idx = &ctx->indices[ctx->index_count];
    idx[0] = base + 0; idx[1] = base + 1; idx[2] = base + 2;
    idx[3] = base + 0; idx[4] = base + 2; idx[5] = base + 3;
    ctx->index_count += 6;
}

static inline bool forge_ui_ctx_color_picker(ForgeUiContext *ctx,
                                              const char *label,
                                              float *h, float *s, float *v,
                                              ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !label || !h || !s || !v) return false;
    if (label[0] == '\0') return false;
    if (!forge_isfinite(rect.x) || !forge_isfinite(rect.y) ||
        !forge_isfinite(rect.w) || !forge_isfinite(rect.h)) return false;

    /* Clamp inputs */
    if (!forge_isfinite(*h)) *h = 0.0f;
    if (!forge_isfinite(*s)) *s = 1.0f;
    if (!forge_isfinite(*v)) *v = 1.0f;
    *h = SDL_fmodf(*h, 360.0f);
    if (*h < 0.0f) *h += 360.0f;
    if (*h >= 360.0f) *h = FORGE_UI_HUE_MAX;
    if (*s < 0.0f) *s = 0.0f; else if (*s > 1.0f) *s = 1.0f;
    if (*v < 0.0f) *v = 0.0f; else if (*v > 1.0f) *v = 1.0f;

    bool changed = false;
    float gap = FORGE_UI_SCALED(ctx, FORGE_UI_CP_GAP);
    float hue_bar_h = FORGE_UI_SCALED(ctx, FORGE_UI_CP_HUE_BAR_H);
    float preview_h = FORGE_UI_SCALED(ctx, FORGE_UI_CP_PREVIEW_H);

    /* ── Compute sub-areas ───────────────────────────────────────────── */
    /* Fit all sub-rects inside the supplied rect.  When rect.h is too
     * small the areas shrink proportionally instead of overflowing. */
    float available_h = rect.h - hue_bar_h - preview_h - 2.0f * gap;
    if (available_h < 0.0f) available_h = 0.0f;
    float sv_size = available_h;
    if (sv_size > rect.w) sv_size = rect.w;

    /* Clamp hue bar and preview into the remaining space */
    float remaining = rect.h - sv_size - 2.0f * gap;
    if (remaining < 0.0f) remaining = 0.0f;
    if (hue_bar_h + preview_h > remaining) {
        float ratio = remaining / (hue_bar_h + preview_h);
        hue_bar_h *= ratio;
        preview_h *= ratio;
    }

    ForgeUiRect sv_rect = { rect.x, rect.y, sv_size, sv_size };
    ForgeUiRect hue_rect = { rect.x, rect.y + sv_size + gap,
                              rect.w, hue_bar_h };
    ForgeUiRect preview_rect = { rect.x, rect.y + sv_size + gap + hue_bar_h + gap,
                                  rect.w, preview_h };

    if (!forge_ui_push_id(ctx, label)) return false;

    /* ── SV area — grid of colored quads ─────────────────────────────── */
    {
        Uint32 sv_id = forge_ui_hash_id(ctx, "##sv");
        bool mouse_over = forge_ui__widget_mouse_over(ctx, sv_rect);
        if (mouse_over) ctx->next_hot = sv_id;

        bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
        if (mouse_pressed && ctx->next_hot == sv_id) ctx->active = sv_id;

        /* Update S and V while dragging */
        if (ctx->active == sv_id && ctx->mouse_down) {
            if (sv_rect.w > 0.0f && sv_rect.h > 0.0f) {
                float new_s = (ctx->mouse_x - sv_rect.x) / sv_rect.w;
                float new_v = 1.0f - (ctx->mouse_y - sv_rect.y) / sv_rect.h;
                if (new_s < 0.0f) new_s = 0.0f; else if (new_s > 1.0f) new_s = 1.0f;
                if (new_v < 0.0f) new_v = 0.0f; else if (new_v > 1.0f) new_v = 1.0f;
                if (new_s != *s || new_v != *v) {
                    *s = new_s;
                    *v = new_v;
                    changed = true;
                }
            }
        }
        if (ctx->active == sv_id && !ctx->mouse_down) ctx->active = FORGE_UI_ID_NONE;

        /* Draw SV grid — each cell corner gets exact HSV→RGB color */
        int grid = FORGE_UI_CP_SV_GRID;
        float cell_w = sv_rect.w / (float)grid;
        float cell_h = sv_rect.h / (float)grid;

        for (int gy = 0; gy < grid; gy++) {
            for (int gx = 0; gx < grid; gx++) {
                float s0 = (float)gx / (float)grid;
                float s1 = (float)(gx + 1) / (float)grid;
                float v0 = 1.0f - (float)gy / (float)grid;
                float v1 = 1.0f - (float)(gy + 1) / (float)grid;

                float tl_r, tl_g, tl_b, tr_r, tr_g, tr_b;
                float bl_r, bl_g, bl_b, br_r, br_g, br_b;
                forge_ui_hsv_to_rgb(*h, s0, v0, &tl_r, &tl_g, &tl_b);
                forge_ui_hsv_to_rgb(*h, s1, v0, &tr_r, &tr_g, &tr_b);
                forge_ui_hsv_to_rgb(*h, s0, v1, &bl_r, &bl_g, &bl_b);
                forge_ui_hsv_to_rgb(*h, s1, v1, &br_r, &br_g, &br_b);

                ForgeUiRect cell = {
                    sv_rect.x + cell_w * (float)gx,
                    sv_rect.y + cell_h * (float)gy,
                    cell_w, cell_h
                };
                forge_ui__emit_gradient_rect(ctx, cell,
                    tl_r, tl_g, tl_b, tr_r, tr_g, tr_b,
                    br_r, br_g, br_b, bl_r, bl_g, bl_b);
            }
        }

        /* Draw crosshair cursor at current S, V position */
        float cx = sv_rect.x + (*s) * sv_rect.w;
        float cy = sv_rect.y + (1.0f - *v) * sv_rect.h;
        float cs = FORGE_UI_SCALED(ctx, FORGE_UI_CP_CURSOR_SIZE) * 0.5f;
        float bt = FORGE_UI_SCALED(ctx, FORGE_UI_CP_BAR_THICK);
        float bh = bt * 0.5f; /* half-thickness for centering */
        /* Horizontal bar */
        ForgeUiRect ch = { cx - cs, cy - bh, cs * 2.0f, bt };
        forge_ui__emit_rect(ctx, ch, 1.0f, 1.0f, 1.0f, 1.0f);
        /* Vertical bar */
        ForgeUiRect cv = { cx - bh, cy - cs, bt, cs * 2.0f };
        forge_ui__emit_rect(ctx, cv, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    /* ── Hue bar — horizontal rainbow gradient ───────────────────────── */
    {
        Uint32 hue_id = forge_ui_hash_id(ctx, "##hue");
        bool mouse_over = forge_ui__widget_mouse_over(ctx, hue_rect);
        if (mouse_over) ctx->next_hot = hue_id;

        bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
        if (mouse_pressed && ctx->next_hot == hue_id) ctx->active = hue_id;

        if (ctx->active == hue_id && ctx->mouse_down && hue_rect.w > 0.0f) {
            float t = (ctx->mouse_x - hue_rect.x) / hue_rect.w;
            if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
            float new_h = t * 360.0f;
            if (new_h >= 360.0f) new_h = FORGE_UI_HUE_MAX;
            if (new_h != *h) { *h = new_h; changed = true; }
        }
        if (ctx->active == hue_id && !ctx->mouse_down) ctx->active = FORGE_UI_ID_NONE;

        /* Draw hue segments */
        int segs = FORGE_UI_CP_HUE_SEGMENTS;
        float seg_w = hue_rect.w / (float)segs;
        for (int i = 0; i < segs; i++) {
            float h0 = 360.0f * (float)i / (float)segs;
            float h1 = 360.0f * (float)(i + 1) / (float)segs;
            float r0, g0, b0, r1, g1, b1;
            forge_ui_hsv_to_rgb(h0, 1.0f, 1.0f, &r0, &g0, &b0);
            forge_ui_hsv_to_rgb(h1, 1.0f, 1.0f, &r1, &g1, &b1);
            ForgeUiRect seg = {
                hue_rect.x + seg_w * (float)i, hue_rect.y,
                seg_w, hue_rect.h
            };
            forge_ui__emit_gradient_rect(ctx, seg,
                r0, g0, b0, r1, g1, b1,
                r1, g1, b1, r0, g0, b0);
        }

        /* Hue cursor — vertical line at current hue position */
        float hcw = FORGE_UI_SCALED(ctx, FORGE_UI_CP_HUE_CURSOR_W);
        float hx = hue_rect.x + (*h / 360.0f) * hue_rect.w;
        ForgeUiRect hcur = { hx - hcw * 0.5f, hue_rect.y, hcw, hue_rect.h };
        forge_ui__emit_rect(ctx, hcur, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    /* ── Preview swatch + RGB text ───────────────────────────────────── */
    {
        float pr, pg, pb;
        forge_ui_hsv_to_rgb(*h, *s, *v, &pr, &pg, &pb);

        /* Color swatch — clamp to available width */
        float available_w = preview_rect.w;
        float swatch_w = preview_h * 2.0f;
        if (swatch_w > available_w) swatch_w = available_w;
        ForgeUiRect swatch = { preview_rect.x, preview_rect.y,
                                swatch_w, preview_h };
        forge_ui__emit_rect(ctx, swatch, pr, pg, pb, 1.0f);

        /* RGB text to the right of the swatch — only if it fits */
        float ascender_px = forge_ui__ascender_px(ctx->atlas);
        float ty = preview_rect.y + (preview_h - ctx->atlas->pixel_height) * 0.5f
                  + ascender_px;
        char rgb_buf[64];
        SDL_snprintf(rgb_buf, sizeof(rgb_buf), "R:%.0f G:%.0f B:%.0f",
                     (double)(pr * 255.0f), (double)(pg * 255.0f),
                     (double)(pb * 255.0f));
        float remaining_w = available_w - swatch_w - gap;
        if (remaining_w > 0.0f) {
            forge_ui_ctx_label(ctx, rgb_buf, preview_rect.x + swatch_w + gap, ty);
        }
    }

    forge_ui_pop_id(ctx);
    return changed;
}

static inline bool forge_ui_ctx_color_picker_layout(ForgeUiContext *ctx,
                                                      const char *label,
                                                      float *h, float *s,
                                                      float *v,
                                                      float size)
{
    if (!ctx || !ctx->atlas || !label || !h || !s || !v) return false;
    if (ctx->layout_depth <= 0) return false;
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_color_picker(ctx, label, h, s, v, rect);
}

/* ── Theme setter (declared in forge_ui_theme.h) ───────────────────────── */

/* Return true if a single color has finite components in [0, 1]. */
static inline bool forge_ui__color_valid(const ForgeUiColor *c)
{
    return c
        && c->r >= 0.0f && c->r <= 1.0f
        && c->g >= 0.0f && c->g <= 1.0f
        && c->b >= 0.0f && c->b <= 1.0f
        && c->a >= 0.0f && c->a <= 1.0f;
}

static inline bool forge_ui_ctx_set_theme(struct ForgeUiContext *ctx,
                                           ForgeUiTheme theme)
{
    if (!ctx) return false;

    /* Validate every color slot — reject NaN, Inf, and out-of-range. */
    const ForgeUiColor *slots[] = {
        &theme.bg,  &theme.surface,  &theme.surface_hot,  &theme.surface_active,
        &theme.title_bar,  &theme.title_bar_text,  &theme.text,  &theme.text_dim,
        &theme.accent,  &theme.accent_hot,  &theme.border,  &theme.border_focused,
        &theme.scrollbar_track,  &theme.cursor,
    };
    for (int i = 0; i < (int)(sizeof(slots) / sizeof(slots[0])); i++) {
        if (!forge_ui__color_valid(slots[i])) return false;
    }

    ctx->theme = theme;
    return true;
}

#endif /* FORGE_UI_CTX_H */
