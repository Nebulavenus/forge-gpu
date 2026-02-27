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

/* ── Types ──────────────────────────────────────────────────────────────── */

/* A simple rectangle for widget bounds. */
typedef struct ForgeUiRect {
    float x;  /* left edge */
    float y;  /* top edge */
    float w;  /* width */
    float h;  /* height */
} ForgeUiRect;

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

    /* Used internally to track hot candidates within a single frame */
    Uint32 next_hot;

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
    if (!ctx || !text || id == FORGE_UI_ID_NONE) return false;

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
     * from falsely activating it. */
    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    if (ctx->active == FORGE_UI_ID_NONE && mouse_pressed && ctx->hot == id) {
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

#endif /* FORGE_UI_CTX_H */
