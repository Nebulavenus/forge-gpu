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
 *   - Widget IDs are simple integers chosen by the caller -- the context
 *     does not allocate or manage IDs.
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
 *   if (forge_ui_ctx_button(&ctx, 1, "Click me", rect)) { ... }
 *   forge_ui_ctx_label(&ctx, "Hello", x, y, r, g, b, a);
 *   forge_ui_ctx_end(&ctx);
 *
 *   // Use ctx.vertices, ctx.vertex_count, ctx.indices, ctx.index_count
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_UI_CTX_H
#define FORGE_UI_CTX_H

#include <SDL3/SDL.h>
#include "forge_ui.h"

/* ── Constants ──────────────────────────────────────────────────────────── */

/* Initial capacity for the vertex and index buffers.  The buffers grow
 * dynamically as widgets emit draw data, doubling each time they fill up.
 * 256 vertices (64 quads) is enough for a simple UI without reallocation. */
#define FORGE_UI_CTX_INITIAL_VERTEX_CAPACITY  256
#define FORGE_UI_CTX_INITIAL_INDEX_CAPACITY   384

/* No widget is hot or active.  Zero is reserved as the null ID -- callers
 * must use non-zero IDs for their widgets. */
#define FORGE_UI_ID_NONE  0

/* ── Button style ───────────────────────────────────────────────────────── */

/* Button appearance varies with interaction state.  These default colors
 * provide a clear visual distinction between normal, hovered, and pressed
 * states.  All values are RGBA floats in [0, 1]. */
#define FORGE_UI_BTN_NORMAL_R   0.25f
#define FORGE_UI_BTN_NORMAL_G   0.25f
#define FORGE_UI_BTN_NORMAL_B   0.30f
#define FORGE_UI_BTN_NORMAL_A   1.00f

#define FORGE_UI_BTN_HOT_R      0.35f
#define FORGE_UI_BTN_HOT_G      0.35f
#define FORGE_UI_BTN_HOT_B      0.42f
#define FORGE_UI_BTN_HOT_A      1.00f

#define FORGE_UI_BTN_ACTIVE_R   0.18f
#define FORGE_UI_BTN_ACTIVE_G   0.18f
#define FORGE_UI_BTN_ACTIVE_B   0.22f
#define FORGE_UI_BTN_ACTIVE_A   1.00f

/* Default button text color (near-white) */
#define FORGE_UI_BTN_TEXT_R     0.95f
#define FORGE_UI_BTN_TEXT_G     0.95f
#define FORGE_UI_BTN_TEXT_B     0.95f
#define FORGE_UI_BTN_TEXT_A     1.00f

/* ── Checkbox style ────────────────────────────────────────────────────── */

/* Checkbox box dimensions.  The box is a square drawn at the left edge
 * of the widget rect, vertically centered.  The label text is drawn to
 * the right of the box with a small gap. */
#define FORGE_UI_CB_BOX_SIZE     18.0f  /* checkbox square side length in pixels */
#define FORGE_UI_CB_INNER_PAD     3.0f  /* padding between box edge and check fill */
#define FORGE_UI_CB_LABEL_GAP     8.0f  /* horizontal gap between box and label */

/* Box outline colors by state (RGBA floats in [0, 1]) */
#define FORGE_UI_CB_NORMAL_R    0.30f
#define FORGE_UI_CB_NORMAL_G    0.30f
#define FORGE_UI_CB_NORMAL_B    0.35f
#define FORGE_UI_CB_NORMAL_A    1.00f

#define FORGE_UI_CB_HOT_R       0.40f
#define FORGE_UI_CB_HOT_G       0.40f
#define FORGE_UI_CB_HOT_B       0.48f
#define FORGE_UI_CB_HOT_A       1.00f

#define FORGE_UI_CB_ACTIVE_R    0.22f
#define FORGE_UI_CB_ACTIVE_G    0.22f
#define FORGE_UI_CB_ACTIVE_B    0.26f
#define FORGE_UI_CB_ACTIVE_A    1.00f

/* Inner fill color when checked (accent cyan) */
#define FORGE_UI_CB_CHECK_R     0.31f
#define FORGE_UI_CB_CHECK_G     0.76f
#define FORGE_UI_CB_CHECK_B     0.97f
#define FORGE_UI_CB_CHECK_A     1.00f

/* Checkbox label text color (near-white, matches button text) */
#define FORGE_UI_CB_TEXT_R      0.95f
#define FORGE_UI_CB_TEXT_G      0.95f
#define FORGE_UI_CB_TEXT_B      0.95f
#define FORGE_UI_CB_TEXT_A      1.00f

/* ── Slider style ──────────────────────────────────────────────────────── */

/* Slider track and thumb dimensions.  The track is a thin horizontal bar
 * centered vertically in the widget rect.  The thumb slides along the
 * track to indicate the current value.  The "effective track" (the range
 * the thumb center can travel) is inset by half the thumb width on each
 * side, so the thumb never overhangs the rect edges. */
#define FORGE_UI_SL_TRACK_HEIGHT   4.0f   /* thin track bar height */
#define FORGE_UI_SL_THUMB_WIDTH   12.0f   /* thumb rectangle width */
#define FORGE_UI_SL_THUMB_HEIGHT  22.0f   /* thumb rectangle height */

/* Track background color (dark gray) */
#define FORGE_UI_SL_TRACK_R     0.30f
#define FORGE_UI_SL_TRACK_G     0.30f
#define FORGE_UI_SL_TRACK_B     0.35f
#define FORGE_UI_SL_TRACK_A     1.00f

/* Thumb colors by state */
#define FORGE_UI_SL_NORMAL_R    0.50f
#define FORGE_UI_SL_NORMAL_G    0.50f
#define FORGE_UI_SL_NORMAL_B    0.58f
#define FORGE_UI_SL_NORMAL_A    1.00f

#define FORGE_UI_SL_HOT_R       0.60f
#define FORGE_UI_SL_HOT_G       0.60f
#define FORGE_UI_SL_HOT_B       0.72f
#define FORGE_UI_SL_HOT_A       1.00f

#define FORGE_UI_SL_ACTIVE_R    0.31f
#define FORGE_UI_SL_ACTIVE_G    0.76f
#define FORGE_UI_SL_ACTIVE_B    0.97f
#define FORGE_UI_SL_ACTIVE_A    1.00f

/* ── Text input style ─────────────────────────────────────────────────── */

/* Text input layout dimensions */
#define FORGE_UI_TI_PADDING       6.0f   /* left padding in pixels before text starts */
#define FORGE_UI_TI_CURSOR_WIDTH  2.0f   /* cursor bar width in pixels */
#define FORGE_UI_TI_BORDER_WIDTH  1.0f   /* focused border edge width in pixels */

/* Background colors by state (RGBA floats in [0, 1]) */
#define FORGE_UI_TI_NORMAL_R   0.15f   /* unfocused: dark */
#define FORGE_UI_TI_NORMAL_G   0.15f
#define FORGE_UI_TI_NORMAL_B   0.18f
#define FORGE_UI_TI_NORMAL_A   1.00f

#define FORGE_UI_TI_HOT_R      0.20f   /* hovered (unfocused): subtle highlight */
#define FORGE_UI_TI_HOT_G      0.20f
#define FORGE_UI_TI_HOT_B      0.24f
#define FORGE_UI_TI_HOT_A      1.00f

#define FORGE_UI_TI_FOCUSED_R  0.18f   /* focused: medium */
#define FORGE_UI_TI_FOCUSED_G  0.18f
#define FORGE_UI_TI_FOCUSED_B  0.22f
#define FORGE_UI_TI_FOCUSED_A  1.00f

/* Border color when focused (accent cyan, matches check/slider active) */
#define FORGE_UI_TI_BORDER_R   0.31f
#define FORGE_UI_TI_BORDER_G   0.76f
#define FORGE_UI_TI_BORDER_B   0.97f
#define FORGE_UI_TI_BORDER_A   1.00f

/* Cursor bar color (accent cyan) */
#define FORGE_UI_TI_CURSOR_R   0.31f
#define FORGE_UI_TI_CURSOR_G   0.76f
#define FORGE_UI_TI_CURSOR_B   0.97f
#define FORGE_UI_TI_CURSOR_A   1.00f

/* Text color (near-white, matches other widget text) */
#define FORGE_UI_TI_TEXT_R     0.95f
#define FORGE_UI_TI_TEXT_G     0.95f
#define FORGE_UI_TI_TEXT_B     0.95f
#define FORGE_UI_TI_TEXT_A     1.00f

/* ── Types ──────────────────────────────────────────────────────────────── */

/* A simple rectangle for widget bounds. */
typedef struct ForgeUiRect {
    float x;  /* left edge */
    float y;  /* top edge */
    float w;  /* width */
    float h;  /* height */
} ForgeUiRect;

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
    bool  mouse_down;     /* true while the primary button is held */
    bool  mouse_down_prev; /* mouse_down from the previous frame (for edge detection) */

    /* Persistent widget state (survives across frames) */
    Uint32 hot;           /* widget under the cursor (or FORGE_UI_ID_NONE) */
    Uint32 active;        /* widget being pressed (or FORGE_UI_ID_NONE) */

    Uint32 next_hot;      /* hot candidate for this frame (resolved in ctx_end) */

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

    /* Draw data (reset each frame by forge_ui_ctx_begin) */
    ForgeUiVertex *vertices;   /* dynamically growing vertex buffer */
    int            vertex_count;    /* number of vertices emitted this frame */
    int            vertex_capacity; /* allocated size of vertex buffer */

    Uint32        *indices;    /* dynamically growing index buffer */
    int            index_count;     /* number of indices emitted this frame */
    int            index_capacity;  /* allocated size of index buffer */
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

/* Draw a text label at (x, y) with the given color.
 * The y coordinate is the baseline.  Does not participate in hit testing. */
static inline void forge_ui_ctx_label(ForgeUiContext *ctx,
                                      const char *text,
                                      float x, float y,
                                      float r, float g, float b, float a);

/* Draw a button with a background rectangle and centered text label.
 * Returns true on the frame the button is clicked (mouse released over it).
 *
 * id:   unique non-zero identifier for this widget
 * text: button label
 * rect: bounding rectangle in screen pixels */
static inline bool forge_ui_ctx_button(ForgeUiContext *ctx,
                                       Uint32 id,
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
 * id:    unique non-zero identifier for this widget
 * label: text drawn to the right of the checkbox box
 * value: pointer to the boolean state (toggled on click)
 * rect:  bounding rectangle for the entire widget (box + label area) */
static inline bool forge_ui_ctx_checkbox(ForgeUiContext *ctx,
                                          Uint32 id,
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
 * id:      unique non-zero identifier for this widget
 * value:   pointer to the float value (updated during drag)
 * min_val: minimum value (left edge of track)
 * max_val: maximum value (right edge of track), must be > min_val
 * rect:    bounding rectangle for the slider track/thumb area */
static inline bool forge_ui_ctx_slider(ForgeUiContext *ctx,
                                        Uint32 id,
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
 * id:             unique non-zero identifier for this widget
 * state:          pointer to application-owned ForgeUiTextInputState
 * rect:           bounding rectangle in screen pixels
 * cursor_visible: false to hide the cursor bar (for blink animation) */
static inline bool forge_ui_ctx_text_input(ForgeUiContext *ctx,
                                            Uint32 id,
                                            ForgeUiTextInputState *state,
                                            ForgeUiRect rect,
                                            bool cursor_visible);

/* ── Internal Helpers ───────────────────────────────────────────────────── */

/* Test whether a point is inside a rectangle. */
static inline bool forge_ui__rect_contains(ForgeUiRect rect,
                                           float px, float py)
{
    return px >= rect.x && px < rect.x + rect.w &&
           py >= rect.y && py < rect.y + rect.h;
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

/* Append vertices and indices from a text layout into the context's
 * draw buffers.  Offsets indices by the current vertex count. */
static inline void forge_ui__emit_text_layout(ForgeUiContext *ctx,
                                              const ForgeUiTextLayout *layout)
{
    if (!layout || layout->vertex_count == 0 || !layout->vertices || !layout->indices) return;
    if (!forge_ui__grow_vertices(ctx, layout->vertex_count)) return;
    if (!forge_ui__grow_indices(ctx, layout->index_count)) return;

    Uint32 base = (Uint32)ctx->vertex_count;

    /* Copy vertices directly */
    SDL_memcpy(&ctx->vertices[ctx->vertex_count],
               layout->vertices,
               (size_t)layout->vertex_count * sizeof(ForgeUiVertex));
    ctx->vertex_count += layout->vertex_count;

    /* Copy indices with offset */
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
    /* Reject degenerate borders: width must be positive and must fit
     * within half the rect dimension to avoid inverted geometry. */
    if (border_w <= 0.0f) return;
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
}

static inline void forge_ui_ctx_begin(ForgeUiContext *ctx,
                                      float mouse_x, float mouse_y,
                                      bool mouse_down)
{
    if (!ctx) return;

    /* Track the previous frame's mouse state for edge detection */
    ctx->mouse_down_prev = ctx->mouse_down;

    /* Update input state */
    ctx->mouse_x = mouse_x;
    ctx->mouse_y = mouse_y;
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
}

static inline void forge_ui_ctx_label(ForgeUiContext *ctx,
                                      const char *text,
                                      float x, float y,
                                      float r, float g, float b, float a)
{
    if (!ctx || !text || !ctx->atlas) return;

    ForgeUiTextOpts opts = { 0.0f, FORGE_UI_TEXT_ALIGN_LEFT, r, g, b, a };
    ForgeUiTextLayout layout;
    if (forge_ui_text_layout(ctx->atlas, text, x, y, &opts, &layout)) {
        forge_ui__emit_text_layout(ctx, &layout);
        forge_ui_text_layout_free(&layout);
    }
}

static inline bool forge_ui_ctx_button(ForgeUiContext *ctx,
                                       Uint32 id,
                                       const char *text,
                                       ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !text || id == FORGE_UI_ID_NONE) return false;

    bool clicked = false;

    /* ── Hit testing ──────────────────────────────────────────────────── */
    /* Check if the mouse cursor is within this button's bounding rect.
     * If so, this widget becomes a candidate for hot. */
    bool mouse_over = forge_ui__rect_contains(rect, ctx->mouse_x, ctx->mouse_y);

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
    float bg_r, bg_g, bg_b, bg_a;
    if (ctx->active == id) {
        /* Pressed state -- darker to give visual feedback */
        bg_r = FORGE_UI_BTN_ACTIVE_R;
        bg_g = FORGE_UI_BTN_ACTIVE_G;
        bg_b = FORGE_UI_BTN_ACTIVE_B;
        bg_a = FORGE_UI_BTN_ACTIVE_A;
    } else if (ctx->hot == id) {
        /* Hovered state -- lighter to indicate interactivity */
        bg_r = FORGE_UI_BTN_HOT_R;
        bg_g = FORGE_UI_BTN_HOT_G;
        bg_b = FORGE_UI_BTN_HOT_B;
        bg_a = FORGE_UI_BTN_HOT_A;
    } else {
        /* Normal state */
        bg_r = FORGE_UI_BTN_NORMAL_R;
        bg_g = FORGE_UI_BTN_NORMAL_G;
        bg_b = FORGE_UI_BTN_NORMAL_B;
        bg_a = FORGE_UI_BTN_NORMAL_A;
    }

    /* ── Emit background rectangle ────────────────────────────────────── */
    forge_ui__emit_rect(ctx, rect, bg_r, bg_g, bg_b, bg_a);

    /* ── Emit centered text label ─────────────────────────────────────── */
    /* Measure text to compute centering offsets */
    ForgeUiTextMetrics metrics = forge_ui_text_measure(ctx->atlas, text, NULL);

    /* Compute the font's pixel-space ascender for baseline calculation.
     * The ascender tells us how far above the baseline the tallest glyph
     * extends -- we need this to convert from "top of text" to baseline. */
    float scale = 0.0f;
    float ascender_px = 0.0f;
    if (ctx->atlas->units_per_em > 0) {
        scale = ctx->atlas->pixel_height / (float)ctx->atlas->units_per_em;
        ascender_px = (float)ctx->atlas->ascender * scale;
    }

    /* Center the text within the button rectangle.
     * Horizontal: offset by half the difference between rect width and text width.
     * Vertical: place the baseline so that the text is vertically centered.
     * The baseline y = rect_center_y - text_half_height + ascender. */
    float text_x = rect.x + (rect.w - metrics.width) * 0.5f;
    float text_y = rect.y + (rect.h - metrics.height) * 0.5f + ascender_px;

    forge_ui_ctx_label(ctx, text, text_x, text_y,
                       FORGE_UI_BTN_TEXT_R, FORGE_UI_BTN_TEXT_G,
                       FORGE_UI_BTN_TEXT_B, FORGE_UI_BTN_TEXT_A);

    return clicked;
}

static inline bool forge_ui_ctx_checkbox(ForgeUiContext *ctx,
                                          Uint32 id,
                                          const char *label,
                                          bool *value,
                                          ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !label || !value || id == FORGE_UI_ID_NONE) return false;

    bool toggled = false;

    /* ── Hit testing ──────────────────────────────────────────────────── */
    /* The hit area covers the entire widget rect (box + label region).
     * This gives users a generous click target -- they can click on the
     * label text, not just the small box. */
    bool mouse_over = forge_ui__rect_contains(rect, ctx->mouse_x, ctx->mouse_y);
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
    float box_r, box_g, box_b, box_a;
    if (ctx->active == id) {
        box_r = FORGE_UI_CB_ACTIVE_R;  box_g = FORGE_UI_CB_ACTIVE_G;
        box_b = FORGE_UI_CB_ACTIVE_B;  box_a = FORGE_UI_CB_ACTIVE_A;
    } else if (ctx->hot == id) {
        box_r = FORGE_UI_CB_HOT_R;  box_g = FORGE_UI_CB_HOT_G;
        box_b = FORGE_UI_CB_HOT_B;  box_a = FORGE_UI_CB_HOT_A;
    } else {
        box_r = FORGE_UI_CB_NORMAL_R;  box_g = FORGE_UI_CB_NORMAL_G;
        box_b = FORGE_UI_CB_NORMAL_B;  box_a = FORGE_UI_CB_NORMAL_A;
    }

    /* ── Compute box position (vertically centered in widget rect) ────── */
    float box_x = rect.x;
    float box_y = rect.y + (rect.h - FORGE_UI_CB_BOX_SIZE) * 0.5f;
    ForgeUiRect box_rect = { box_x, box_y,
                             FORGE_UI_CB_BOX_SIZE, FORGE_UI_CB_BOX_SIZE };

    /* ── Outer box — border with hover feedback via box color ────────── */
    forge_ui__emit_rect(ctx, box_rect, box_r, box_g, box_b, box_a);

    /* ── Inner fill — solid rect rather than a glyph keeps the renderer
     *    purely quad-based with no dedicated checkmark in the atlas ──── */
    if (*value) {
        ForgeUiRect inner = {
            box_x + FORGE_UI_CB_INNER_PAD,
            box_y + FORGE_UI_CB_INNER_PAD,
            FORGE_UI_CB_BOX_SIZE - 2.0f * FORGE_UI_CB_INNER_PAD,
            FORGE_UI_CB_BOX_SIZE - 2.0f * FORGE_UI_CB_INNER_PAD
        };
        forge_ui__emit_rect(ctx, inner,
                            FORGE_UI_CB_CHECK_R, FORGE_UI_CB_CHECK_G,
                            FORGE_UI_CB_CHECK_B, FORGE_UI_CB_CHECK_A);
    }

    /* ── Label baseline alignment ────────────────────────────────────── */
    /* The font origin is at the baseline, not the top of the em square.
     * Offset by the ascender so text sits visually centered in the rect. */
    float scale = 0.0f;
    float ascender_px = 0.0f;
    if (ctx->atlas->units_per_em > 0) {
        scale = ctx->atlas->pixel_height / (float)ctx->atlas->units_per_em;
        ascender_px = (float)ctx->atlas->ascender * scale;
    }
    (void)scale;  /* only ascender_px is needed for baseline placement */

    float label_x = box_x + FORGE_UI_CB_BOX_SIZE + FORGE_UI_CB_LABEL_GAP;
    float label_y = rect.y + (rect.h - ctx->atlas->pixel_height) * 0.5f
                    + ascender_px;

    forge_ui_ctx_label(ctx, label, label_x, label_y,
                       FORGE_UI_CB_TEXT_R, FORGE_UI_CB_TEXT_G,
                       FORGE_UI_CB_TEXT_B, FORGE_UI_CB_TEXT_A);

    return toggled;
}

static inline bool forge_ui_ctx_slider(ForgeUiContext *ctx,
                                        Uint32 id,
                                        float *value,
                                        float min_val, float max_val,
                                        ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !value || id == FORGE_UI_ID_NONE) return false;
    if (!(max_val > min_val)) return false;  /* also rejects NaN */

    bool changed = false;

    /* ── Hit testing ──────────────────────────────────────────────────── */
    /* The hit area covers the entire widget rect.  Clicking anywhere on
     * the track (not just the thumb) activates the slider and snaps the
     * value to the click position. */
    bool mouse_over = forge_ui__rect_contains(rect, ctx->mouse_x, ctx->mouse_y);
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

    /* ── Effective track geometry ─────────────────────────────────────── */
    /* The thumb center can travel from half a thumb width inside the left
     * edge to half a thumb width inside the right edge.  This keeps the
     * thumb fully within the widget rect at both extremes.  Clamp to
     * zero so a rect narrower than the thumb does not produce a negative
     * range. */
    float track_x = rect.x + FORGE_UI_SL_THUMB_WIDTH * 0.5f;
    float track_w = rect.w - FORGE_UI_SL_THUMB_WIDTH;
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
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
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
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    /* ── Track — thin bar so the thumb visually protrudes above/below ── */
    float track_draw_y = rect.y + (rect.h - FORGE_UI_SL_TRACK_HEIGHT) * 0.5f;
    ForgeUiRect track_rect = { rect.x, track_draw_y,
                               rect.w, FORGE_UI_SL_TRACK_HEIGHT };
    forge_ui__emit_rect(ctx, track_rect,
                        FORGE_UI_SL_TRACK_R, FORGE_UI_SL_TRACK_G,
                        FORGE_UI_SL_TRACK_B, FORGE_UI_SL_TRACK_A);

    /* ── Choose thumb color based on state ────────────────────────────── */
    float th_r, th_g, th_b, th_a;
    if (ctx->active == id) {
        th_r = FORGE_UI_SL_ACTIVE_R;  th_g = FORGE_UI_SL_ACTIVE_G;
        th_b = FORGE_UI_SL_ACTIVE_B;  th_a = FORGE_UI_SL_ACTIVE_A;
    } else if (ctx->hot == id) {
        th_r = FORGE_UI_SL_HOT_R;  th_g = FORGE_UI_SL_HOT_G;
        th_b = FORGE_UI_SL_HOT_B;  th_a = FORGE_UI_SL_HOT_A;
    } else {
        th_r = FORGE_UI_SL_NORMAL_R;  th_g = FORGE_UI_SL_NORMAL_G;
        th_b = FORGE_UI_SL_NORMAL_B;  th_a = FORGE_UI_SL_NORMAL_A;
    }

    /* ── Emit thumb rectangle ─────────────────────────────────────────── */
    /* The thumb center is at track_x + t * track_w.  Subtract half the
     * thumb width to get the left edge. */
    float thumb_cx = track_x + t * track_w;
    float thumb_x = thumb_cx - FORGE_UI_SL_THUMB_WIDTH * 0.5f;
    float thumb_y = rect.y + (rect.h - FORGE_UI_SL_THUMB_HEIGHT) * 0.5f;
    ForgeUiRect thumb_rect = { thumb_x, thumb_y,
                               FORGE_UI_SL_THUMB_WIDTH,
                               FORGE_UI_SL_THUMB_HEIGHT };
    forge_ui__emit_rect(ctx, thumb_rect, th_r, th_g, th_b, th_a);

    return changed;
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
                                            Uint32 id,
                                            ForgeUiTextInputState *state,
                                            ForgeUiRect rect,
                                            bool cursor_visible)
{
    if (!ctx || !ctx->atlas || !state || !state->buffer
        || id == FORGE_UI_ID_NONE) return false;

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
    bool mouse_over = forge_ui__rect_contains(rect, ctx->mouse_x, ctx->mouse_y);
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

    /* ── Keyboard input processing (only when focused) ────────────────── */
    if (is_focused) {
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
        bg_r = FORGE_UI_TI_FOCUSED_R;  bg_g = FORGE_UI_TI_FOCUSED_G;
        bg_b = FORGE_UI_TI_FOCUSED_B;  bg_a = FORGE_UI_TI_FOCUSED_A;
    } else if (ctx->hot == id) {
        bg_r = FORGE_UI_TI_HOT_R;  bg_g = FORGE_UI_TI_HOT_G;
        bg_b = FORGE_UI_TI_HOT_B;  bg_a = FORGE_UI_TI_HOT_A;
    } else {
        bg_r = FORGE_UI_TI_NORMAL_R;  bg_g = FORGE_UI_TI_NORMAL_G;
        bg_b = FORGE_UI_TI_NORMAL_B;  bg_a = FORGE_UI_TI_NORMAL_A;
    }

    /* ── Emit background rectangle ───────────────────────────────────── */
    forge_ui__emit_rect(ctx, rect, bg_r, bg_g, bg_b, bg_a);

    /* ── Emit focused border (accent outline drawn on top of bg) ─────── */
    if (is_focused) {
        forge_ui__emit_border(ctx, rect, FORGE_UI_TI_BORDER_WIDTH,
                              FORGE_UI_TI_BORDER_R, FORGE_UI_TI_BORDER_G,
                              FORGE_UI_TI_BORDER_B, FORGE_UI_TI_BORDER_A);
    }

    /* ── Compute font metrics for baseline positioning ───────────────── */
    float scale = 0.0f;
    float ascender_px = 0.0f;
    if (ctx->atlas->units_per_em > 0) {
        scale = ctx->atlas->pixel_height / (float)ctx->atlas->units_per_em;
        ascender_px = (float)ctx->atlas->ascender * scale;
    }
    (void)scale;  /* only ascender_px needed for baseline placement */

    /* Vertically center the text within the rect: offset to the top of
     * the em square, then add the ascender to reach the baseline where
     * forge_ui_ctx_label expects the y coordinate. */
    float text_top_y = rect.y + (rect.h - ctx->atlas->pixel_height) * 0.5f;
    float baseline_y = text_top_y + ascender_px;

    /* ── Emit text quads ─────────────────────────────────────────────── */
    if (state->length > 0) {
        forge_ui_ctx_label(ctx, state->buffer,
                           rect.x + FORGE_UI_TI_PADDING, baseline_y,
                           FORGE_UI_TI_TEXT_R, FORGE_UI_TI_TEXT_G,
                           FORGE_UI_TI_TEXT_B, FORGE_UI_TI_TEXT_A);
    }

    /* ── Emit cursor bar ─────────────────────────────────────────────── */
    /* The cursor x position is computed by measuring the substring
     * buffer[0..cursor] using forge_ui_text_measure.  This gives the
     * pen_x advance, which is the exact pixel offset where the cursor
     * should appear. */
    if (is_focused && cursor_visible) {
        float cursor_x = rect.x + FORGE_UI_TI_PADDING;
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
            FORGE_UI_TI_CURSOR_WIDTH, ctx->atlas->pixel_height
        };
        forge_ui__emit_rect(ctx, cursor_rect,
                            FORGE_UI_TI_CURSOR_R, FORGE_UI_TI_CURSOR_G,
                            FORGE_UI_TI_CURSOR_B, FORGE_UI_TI_CURSOR_A);
    }

    return content_changed;
}

#endif /* FORGE_UI_CTX_H */
