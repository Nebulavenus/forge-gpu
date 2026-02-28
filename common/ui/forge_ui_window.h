/*
 * forge_ui_window.h -- Header-only draggable window system for forge-gpu
 *
 * Extends the immediate-mode UI context (forge_ui_ctx.h) with draggable
 * windows that support z-ordering (bring-to-front on click), title bar
 * collapse/expand, and deferred draw ordering.
 *
 * A window is a panel (lesson 09) that can be dragged by its title bar,
 * reordered in depth by clicking, and collapsed to show only the title bar.
 *
 * Key concepts:
 *   - ForgeUiWindowState is application-owned persistent state: rect
 *     (position and size, updated by dragging), scroll_y (content scroll),
 *     collapsed (toggled by clicking a collapse button), and z_order
 *     (draw/input priority, higher is on top).
 *   - ForgeUiWindowEntry holds per-frame window registration data including
 *     a separate draw list (vertex/index buffers) for deferred rendering.
 *   - Windows draw back-to-front: each window's vertices/indices are emitted
 *     into a per-window draw list during declaration, then
 *     forge_ui_wctx_end() sorts by z_order and appends to the main buffers.
 *   - Input routing respects z-order: only the topmost window under the
 *     mouse cursor receives mouse interaction.
 *   - The collapse toggle is a small triangle indicator in the title bar:
 *     right-pointing when collapsed, down-pointing when expanded.
 *
 * Usage:
 *   #include "ui/forge_ui.h"
 *   #include "ui/forge_ui_window.h"
 *
 *   // Application-owned window state (persists across frames)
 *   ForgeUiWindowState win = {
 *       .rect = { 50, 50, 280, 300 },
 *       .scroll_y = 0.0f,
 *       .collapsed = false,
 *       .z_order = 0
 *   };
 *
 *   ForgeUiWindowContext wctx;
 *   forge_ui_wctx_init(&wctx, &ctx);
 *
 *   // Each frame:
 *   forge_ui_ctx_begin(&ctx, mouse_x, mouse_y, mouse_down);
 *   forge_ui_wctx_begin(&wctx);
 *   if (forge_ui_wctx_window_begin(&wctx, 100, "My Window", &win)) {
 *       forge_ui_ctx_label_layout(wctx.ctx, "Hello", 26.0f, 0.9f, 0.9f, 0.9f, 1.0f);
 *       forge_ui_wctx_window_end(&wctx);
 *   }
 *   forge_ui_wctx_end(&wctx);
 *   forge_ui_ctx_end(&ctx);
 *
 *   // Use ctx.vertices, ctx.indices for rendering
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_UI_WINDOW_H
#define FORGE_UI_WINDOW_H

#include "forge_ui_ctx.h"

/* ── Constants ──────────────────────────────────────────────────────────── */

/* Maximum number of windows that can be registered per frame.  16 is
 * generous for most UI layouts. */
#define FORGE_UI_WINDOW_MAX  16

/* Initial capacity for per-window draw list buffers.  Smaller than the
 * main context buffers because each window typically contains fewer
 * widgets.  Buffers grow dynamically if needed. */
#define FORGE_UI_WINDOW_INITIAL_VERTEX_CAPACITY  128
#define FORGE_UI_WINDOW_INITIAL_INDEX_CAPACITY   192

/* ── Window style ──────────────────────────────────────────────────────── */

/* Title bar height matches panel title for visual consistency */
#define FORGE_UI_WIN_TITLE_HEIGHT    30.0f

/* Content padding and widget spacing match panel for consistency */
#define FORGE_UI_WIN_PADDING         10.0f
#define FORGE_UI_WIN_CONTENT_SPACING  8.0f

/* Collapse toggle triangle dimensions */
#define FORGE_UI_WIN_TOGGLE_SIZE     10.0f  /* triangle side length */
#define FORGE_UI_WIN_TOGGLE_PAD       8.0f  /* padding from left edge */

/* Window background RGBA (matches panel for visual consistency) */
#define FORGE_UI_WIN_BG_R     0.14f
#define FORGE_UI_WIN_BG_G     0.14f
#define FORGE_UI_WIN_BG_B     0.18f
#define FORGE_UI_WIN_BG_A     1.00f

/* Title bar background RGBA (matches panel title bar) */
#define FORGE_UI_WIN_TITLE_BG_R   0.18f
#define FORGE_UI_WIN_TITLE_BG_G   0.18f
#define FORGE_UI_WIN_TITLE_BG_B   0.24f
#define FORGE_UI_WIN_TITLE_BG_A   1.00f

/* Title bar text color (near-white) */
#define FORGE_UI_WIN_TITLE_TEXT_R  0.90f
#define FORGE_UI_WIN_TITLE_TEXT_G  0.90f
#define FORGE_UI_WIN_TITLE_TEXT_B  0.95f
#define FORGE_UI_WIN_TITLE_TEXT_A  1.00f

/* Collapse toggle color (dim until hovered) */
#define FORGE_UI_WIN_TOGGLE_R      0.60f
#define FORGE_UI_WIN_TOGGLE_G      0.60f
#define FORGE_UI_WIN_TOGGLE_B      0.68f
#define FORGE_UI_WIN_TOGGLE_A      1.00f

/* Collapse toggle hover color (brighter when the title bar is hot) */
#define FORGE_UI_WIN_TOGGLE_HOT_R  0.80f
#define FORGE_UI_WIN_TOGGLE_HOT_G  0.80f
#define FORGE_UI_WIN_TOGGLE_HOT_B  0.90f
#define FORGE_UI_WIN_TOGGLE_HOT_A  1.00f

/* Extra padding around the collapse toggle triangle to make the click
 * target more forgiving.  The hit rect extends this many pixels beyond
 * the triangle bounding box on each side. */
#define FORGE_UI_WIN_TOGGLE_HIT_PAD  2.0f

/* ── Types ──────────────────────────────────────────────────────────────── */

/* Application-owned window state that persists across frames.
 *
 * The application allocates one ForgeUiWindowState per window and passes
 * a pointer to window_begin each frame.  The window system updates rect
 * (dragging), scroll_y (scrollbar), collapsed (toggle), and z_order
 * (click-to-front) as the user interacts.
 *
 * rect:      Current position and size (mutable -- updated by dragging).
 *            The application sets the initial position and size.
 * scroll_y:  Content scroll offset (same semantics as panel scroll_y).
 * collapsed: When true, only the title bar is drawn; content is hidden.
 * z_order:   Draw and input priority.  Higher values are drawn on top.
 *            Updated automatically when the user clicks on a window. */
typedef struct ForgeUiWindowState {
    ForgeUiRect rect;       /* position and size (updated by drag) */
    float       scroll_y;   /* content scroll offset (same as panel) */
    bool        collapsed;  /* true = only title bar visible */
    int         z_order;    /* draw priority (higher = on top) */
} ForgeUiWindowState;

/* Per-window draw list entry.  Each window gets its own vertex/index
 * buffers during the declaration phase.  forge_ui_wctx_end() sorts
 * these by z_order and appends to the main context buffers in
 * back-to-front order for correct overlap rendering. */
typedef struct ForgeUiWindowEntry {
    Uint32               id;        /* widget ID for this window */
    ForgeUiWindowState  *state;     /* pointer to application-owned state */

    /* Per-window draw list (temporary, filled during declaration, reset each frame) */
    ForgeUiVertex       *vertices;         /* heap-allocated vertex array for this window */
    int                  vertex_count;     /* number of vertices emitted so far */
    int                  vertex_capacity;  /* current allocation size (grows dynamically) */
    Uint32              *indices;          /* heap-allocated index array for this window */
    int                  index_count;      /* number of indices emitted so far */
    int                  index_capacity;   /* current allocation size (grows dynamically) */
} ForgeUiWindowEntry;

/* Window context that wraps a ForgeUiContext with window management.
 *
 * The window context adds z-ordering, deferred drawing, and input
 * routing on top of the existing immediate-mode UI context.  The
 * underlying ForgeUiContext is used for all widget operations; the
 * window context intercepts vertex/index emission to redirect it
 * into per-window draw lists. */
typedef struct ForgeUiWindowContext {
    ForgeUiContext      *ctx;       /* underlying UI context (not owned) */

    /* Window registration array (per-frame) */
    ForgeUiWindowEntry   window_entries[FORGE_UI_WINDOW_MAX];
    int                  window_count;

    /* Index into window_entries for the currently open window, or -1
     * if not inside a window begin/end pair.  When >= 0, all emit
     * functions write to this window's draw list. */
    int                  active_window_idx;

    /* Hovered window ID -- determined during wctx_begin by checking
     * which window (by z-order) contains the mouse position.  Used
     * to route input only to the topmost window at the cursor. */
    Uint32               hovered_window_id;

    /* Grab offset for title bar dragging.  Set on the frame a title
     * bar becomes active (mouse pressed on title bar): records the
     * offset from the mouse position to the window rect origin.
     * This keeps the window anchored at the click point during drag.
     * Persists across frames (not reset by wctx_begin) because
     * dragging spans multiple frames. */
    float                grab_offset_x;
    float                grab_offset_y;

    /* Previous frame's window data -- stored so wctx_begin can determine
     * hovered_window_id.  Uses all three arrays: ids (to record the
     * winner), rects (for mouse hit testing), and z_orders (to pick
     * the topmost window under the cursor). */
    Uint32               prev_window_ids[FORGE_UI_WINDOW_MAX];
    ForgeUiRect          prev_window_rects[FORGE_UI_WINDOW_MAX];
    int                  prev_window_z_orders[FORGE_UI_WINDOW_MAX];
    int                  prev_window_count;

    /* Saved main context buffer pointers.  During a window begin/end
     * pair, the main context's vertex/index pointers are temporarily
     * replaced with the per-window draw list buffers.  These fields
     * save the originals so they can be restored. */
    ForgeUiVertex       *saved_vertices;
    int                  saved_vertex_count;
    int                  saved_vertex_capacity;
    Uint32              *saved_indices;
    int                  saved_index_count;
    int                  saved_index_capacity;
} ForgeUiWindowContext;

/* ── Public API ─────────────────────────────────────────────────────────── */

/* Initialize a window context wrapping an existing UI context.
 * The UI context must be initialized before calling this.
 * Returns true on success. */
static inline bool forge_ui_wctx_init(ForgeUiWindowContext *wctx,
                                       ForgeUiContext *ctx);

/* Free per-window draw list buffers allocated during frames. */
static inline void forge_ui_wctx_free(ForgeUiWindowContext *wctx);

/* Begin a new frame.  Must be called after forge_ui_ctx_begin().
 * Determines which window is under the mouse (hovered_window_id)
 * using the previous frame's window data, then resets per-frame
 * window state. */
static inline void forge_ui_wctx_begin(ForgeUiWindowContext *wctx);

/* End the frame.  Sorts window draw lists by z_order and appends
 * them to the main context's vertex/index buffers in back-to-front
 * order.  Must be called before forge_ui_ctx_end(). */
static inline void forge_ui_wctx_end(ForgeUiWindowContext *wctx);

/* Begin a window: draw title bar with collapse toggle, process
 * dragging and z-ordering, and if not collapsed set up clipping
 * and layout for child widgets.
 *
 * id:    unique non-zero widget ID less than UINT32_MAX-1 (scrollbar
 *        uses id+1, collapse toggle uses id+2)
 * title: text displayed in the title bar
 * state: pointer to application-owned ForgeUiWindowState
 *
 * Returns true if the window is expanded (caller should declare
 * child widgets and call window_end).  Returns false if collapsed
 * or validation fails (caller must NOT call window_end). */
static inline bool forge_ui_wctx_window_begin(ForgeUiWindowContext *wctx,
                                                Uint32 id,
                                                const char *title,
                                                ForgeUiWindowState *state);

/* End a window: compute content height, draw scrollbar if needed,
 * pop layout, clear clip rect, and restore the main context buffers.
 * If the window was collapsed, window_begin returned false and
 * window_end should NOT be called. */
static inline void forge_ui_wctx_window_end(ForgeUiWindowContext *wctx);

/* ── Internal helpers ──────────────────────────────────────────────────── */

/* Ensure a per-window vertex buffer has room for `count` more vertices. */
static inline bool forge_ui_win__grow_vertices(ForgeUiWindowEntry *entry,
                                                int count)
{
    if (count <= 0) return count == 0;
    if (entry->vertex_count > INT_MAX - count) return false;
    int needed = entry->vertex_count + count;
    if (needed <= entry->vertex_capacity) return true;

    int new_cap = entry->vertex_capacity;
    if (new_cap == 0) new_cap = FORGE_UI_WINDOW_INITIAL_VERTEX_CAPACITY;
    while (new_cap < needed) {
        if (new_cap > INT_MAX / 2) return false;
        new_cap *= 2;
    }

    ForgeUiVertex *buf = (ForgeUiVertex *)SDL_realloc(
        entry->vertices, (size_t)new_cap * sizeof(ForgeUiVertex));
    if (!buf) return false;
    entry->vertices = buf;
    entry->vertex_capacity = new_cap;
    return true;
}

/* Ensure a per-window index buffer has room for `count` more indices. */
static inline bool forge_ui_win__grow_indices(ForgeUiWindowEntry *entry,
                                               int count)
{
    if (count <= 0) return count == 0;
    if (entry->index_count > INT_MAX - count) return false;
    int needed = entry->index_count + count;
    if (needed <= entry->index_capacity) return true;

    int new_cap = entry->index_capacity;
    if (new_cap == 0) new_cap = FORGE_UI_WINDOW_INITIAL_INDEX_CAPACITY;
    while (new_cap < needed) {
        if (new_cap > INT_MAX / 2) return false;
        new_cap *= 2;
    }

    Uint32 *buf = (Uint32 *)SDL_realloc(
        entry->indices, (size_t)new_cap * sizeof(Uint32));
    if (!buf) return false;
    entry->indices = buf;
    entry->index_capacity = new_cap;
    return true;
}

/* Emit a triangle (3 vertices forming the collapse toggle arrow)
 * into the given context.  Uses the atlas white_uv for solid color. */
static inline void forge_ui_win__emit_triangle(ForgeUiContext *ctx,
                                                float x0, float y0,
                                                float x1, float y1,
                                                float x2, float y2,
                                                float r, float g,
                                                float b, float a)
{
    if (!ctx || !ctx->atlas) return;
    if (!forge_ui__grow_vertices(ctx, 3)) return;
    if (!forge_ui__grow_indices(ctx, 3)) return;

    const ForgeUiUVRect *wuv = &ctx->atlas->white_uv;
    float u = (wuv->u0 + wuv->u1) * 0.5f;
    float v = (wuv->v0 + wuv->v1) * 0.5f;

    Uint32 base = (Uint32)ctx->vertex_count;

    ForgeUiVertex *verts = &ctx->vertices[ctx->vertex_count];
    verts[0] = (ForgeUiVertex){ x0, y0, u, v, r, g, b, a };
    verts[1] = (ForgeUiVertex){ x1, y1, u, v, r, g, b, a };
    verts[2] = (ForgeUiVertex){ x2, y2, u, v, r, g, b, a };
    ctx->vertex_count += 3;

    Uint32 *idx = &ctx->indices[ctx->index_count];
    idx[0] = base + 0;
    idx[1] = base + 1;
    idx[2] = base + 2;
    ctx->index_count += 3;
}

/* ── Implementation ─────────────────────────────────────────────────────── */

static inline bool forge_ui_wctx_init(ForgeUiWindowContext *wctx,
                                       ForgeUiContext *ctx)
{
    if (!wctx || !ctx) {
        SDL_Log("forge_ui_wctx_init: NULL argument");
        return false;
    }

    SDL_memset(wctx, 0, sizeof(*wctx));
    wctx->ctx = ctx;
    wctx->active_window_idx = -1;
    wctx->hovered_window_id = FORGE_UI_ID_NONE;
    return true;
}

/* Restore the context's vertex/index buffers from saved state */
static inline void forge_ui_win__restore_from_window(
    ForgeUiWindowContext *wctx)
{
    ForgeUiContext *ctx = wctx->ctx;
    int idx = wctx->active_window_idx;
    if (idx < 0 || idx >= wctx->window_count) return;

    ForgeUiWindowEntry *entry = &wctx->window_entries[idx];

    /* Save the window's final buffer state back to its entry.  Widget
     * emit calls may have reallocated the buffers (growing capacity),
     * so the entry's pointers and counts must reflect the current ctx
     * state — not the values from redirect_to_window. */
    entry->vertices = ctx->vertices;
    entry->vertex_count = ctx->vertex_count;
    entry->vertex_capacity = ctx->vertex_capacity;
    entry->indices = ctx->indices;
    entry->index_count = ctx->index_count;
    entry->index_capacity = ctx->index_capacity;

    /* Restore main context's buffers */
    ctx->vertices = wctx->saved_vertices;
    ctx->vertex_count = wctx->saved_vertex_count;
    ctx->vertex_capacity = wctx->saved_vertex_capacity;
    ctx->indices = wctx->saved_indices;
    ctx->index_count = wctx->saved_index_count;
    ctx->index_capacity = wctx->saved_index_capacity;

    wctx->active_window_idx = -1;
}

static inline void forge_ui_wctx_free(ForgeUiWindowContext *wctx)
{
    if (!wctx) return;

    /* If the context is currently redirected to a per-window draw list,
     * restore the main context's buffer pointers before freeing.
     * Otherwise ctx->vertices/indices would become dangling pointers
     * (use-after-free) and a subsequent forge_ui_ctx_free would
     * double-free the same allocation. */
    if (wctx->active_window_idx >= 0 && wctx->ctx) {
        forge_ui_win__restore_from_window(wctx);
    }

    /* Free any allocated per-window draw list buffers */
    for (int i = 0; i < FORGE_UI_WINDOW_MAX; i++) {
        SDL_free(wctx->window_entries[i].vertices);
        SDL_free(wctx->window_entries[i].indices);
        wctx->window_entries[i].vertices = NULL;
        wctx->window_entries[i].indices = NULL;
        wctx->window_entries[i].vertex_count = 0;
        wctx->window_entries[i].vertex_capacity = 0;
        wctx->window_entries[i].index_count = 0;
        wctx->window_entries[i].index_capacity = 0;
    }

    wctx->window_count = 0;
    wctx->active_window_idx = -1;
    wctx->hovered_window_id = FORGE_UI_ID_NONE;
    wctx->prev_window_count = 0;
    wctx->saved_vertices = NULL;
    wctx->saved_indices = NULL;
    wctx->saved_vertex_count = 0;
    wctx->saved_vertex_capacity = 0;
    wctx->saved_index_count = 0;
    wctx->saved_index_capacity = 0;
    wctx->ctx = NULL;
}

static inline void forge_ui_wctx_begin(ForgeUiWindowContext *wctx)
{
    if (!wctx || !wctx->ctx) {
        SDL_Log("forge_ui_wctx_begin: NULL argument");
        return;
    }

    /* If a previous window was never closed (missing window_end call),
     * restore the context's main buffers before proceeding.  Without
     * this, ctx->vertices/indices would still point at the per-window
     * draw list, and the reset below would orphan those pointers. */
    if (wctx->active_window_idx >= 0) {
        SDL_Log("forge_ui_wctx_begin: previous window not closed "
                "(missing window_end call), restoring buffers");
        forge_ui_win__restore_from_window(wctx);
    }

    /* ── Determine hovered window from previous frame's data ──────────── */
    /* Scan all previous frame's windows (in declaration order) to find
     * which window (if any) the mouse cursor is over.  The highest-z
     * window among those containing the cursor wins.  This pre-pass
     * ensures that during the current frame's
     * widget processing, hit tests inside a window only succeed if that
     * window is the hovered window.  This prevents clicking through a
     * foreground window to activate a widget in a background window. */
    wctx->hovered_window_id = FORGE_UI_ID_NONE;
    {
        float mx = wctx->ctx->mouse_x;
        float my = wctx->ctx->mouse_y;
        int best_z = -1;

        for (int i = 0; i < wctx->prev_window_count; i++) {
            ForgeUiRect r = wctx->prev_window_rects[i];
            if (mx >= r.x && mx < r.x + r.w &&
                my >= r.y && my < r.y + r.h) {
                if (wctx->prev_window_z_orders[i] > best_z) {
                    best_z = wctx->prev_window_z_orders[i];
                    wctx->hovered_window_id = wctx->prev_window_ids[i];
                }
            }
        }
    }

    /* ── Save previous frame data, then reset per-frame window state ──── */
    wctx->prev_window_count = wctx->window_count;
    for (int i = 0; i < wctx->window_count; i++) {
        wctx->prev_window_ids[i] = wctx->window_entries[i].id;
        if (wctx->window_entries[i].state) {
            ForgeUiRect r = wctx->window_entries[i].state->rect;
            /* Collapsed windows only show their title bar, so use the
             * title-bar rect for hover detection.  Without this,
             * collapsed windows create an invisible dead zone over
             * their full (hidden) content area that blocks input to
             * windows behind them. */
            if (wctx->window_entries[i].state->collapsed) {
                r.h = FORGE_UI_WIN_TITLE_HEIGHT;
            }
            wctx->prev_window_rects[i] = r;
            wctx->prev_window_z_orders[i] = wctx->window_entries[i].state->z_order;
        }
    }

    /* Reset per-window draw lists (keep allocated memory) */
    for (int i = 0; i < wctx->window_count; i++) {
        wctx->window_entries[i].vertex_count = 0;
        wctx->window_entries[i].index_count = 0;
        wctx->window_entries[i].id = FORGE_UI_ID_NONE;
        wctx->window_entries[i].state = NULL;
    }
    wctx->window_count = 0;
    wctx->active_window_idx = -1;
}

/* Sort comparison for window entries by z_order (ascending = back to front) */
static inline void forge_ui_win__sort_entries(ForgeUiWindowEntry *entries,
                                               int count)
{
    /* Simple insertion sort -- at most 16 elements, so O(n^2) is fine */
    for (int i = 1; i < count; i++) {
        ForgeUiWindowEntry tmp = entries[i];
        int j = i - 1;
        while (j >= 0 && entries[j].state &&
               tmp.state && entries[j].state->z_order > tmp.state->z_order) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = tmp;
    }
}

static inline void forge_ui_wctx_end(ForgeUiWindowContext *wctx)
{
    if (!wctx || !wctx->ctx) {
        SDL_Log("forge_ui_wctx_end: NULL argument");
        return;
    }

    ForgeUiContext *ctx = wctx->ctx;

    /* ── Sort window entries by z_order (ascending = back to front) ────── */
    forge_ui_win__sort_entries(wctx->window_entries, wctx->window_count);

    /* ── Append per-window draw lists to main context in z-order ────────── */
    /* Non-window widgets (labels, buttons drawn outside any window) are
     * already in the main context's buffers and are drawn behind all
     * windows because we append window data after them. */
    for (int w = 0; w < wctx->window_count; w++) {
        ForgeUiWindowEntry *entry = &wctx->window_entries[w];
        if (entry->vertex_count == 0 || entry->index_count == 0) continue;

        if (!forge_ui__grow_vertices(ctx, entry->vertex_count)) {
            SDL_Log("forge_ui_wctx_end: vertex buffer grow failed for "
                    "window %u", (unsigned)entry->id);
            continue;
        }
        if (!forge_ui__grow_indices(ctx, entry->index_count)) {
            SDL_Log("forge_ui_wctx_end: index buffer grow failed for "
                    "window %u", (unsigned)entry->id);
            continue;
        }

        Uint32 base = (Uint32)ctx->vertex_count;

        /* Copy vertices */
        SDL_memcpy(&ctx->vertices[ctx->vertex_count],
                   entry->vertices,
                   (size_t)entry->vertex_count * sizeof(ForgeUiVertex));
        ctx->vertex_count += entry->vertex_count;

        /* Copy indices with rebase */
        for (int i = 0; i < entry->index_count; i++) {
            ctx->indices[ctx->index_count + i] = entry->indices[i] + base;
        }
        ctx->index_count += entry->index_count;
    }
}

/* Switch the context's vertex/index buffers to a per-window draw list */
static inline void forge_ui_win__redirect_to_window(
    ForgeUiWindowContext *wctx, int window_idx)
{
    ForgeUiContext *ctx = wctx->ctx;
    ForgeUiWindowEntry *entry = &wctx->window_entries[window_idx];

    /* Save main context's current buffer state */
    wctx->saved_vertices = ctx->vertices;
    wctx->saved_vertex_count = ctx->vertex_count;
    wctx->saved_vertex_capacity = ctx->vertex_capacity;
    wctx->saved_indices = ctx->indices;
    wctx->saved_index_count = ctx->index_count;
    wctx->saved_index_capacity = ctx->index_capacity;

    /* Point context at this window's draw list */
    ctx->vertices = entry->vertices;
    ctx->vertex_count = entry->vertex_count;
    ctx->vertex_capacity = entry->vertex_capacity;
    ctx->indices = entry->indices;
    ctx->index_count = entry->index_count;
    ctx->index_capacity = entry->index_capacity;

    wctx->active_window_idx = window_idx;
}

static inline bool forge_ui_wctx_window_begin(ForgeUiWindowContext *wctx,
                                                Uint32 id,
                                                const char *title,
                                                ForgeUiWindowState *state)
{
    if (!wctx || !wctx->ctx || !wctx->ctx->atlas ||
        !state || id == FORGE_UI_ID_NONE) {
        return false;
    }

    ForgeUiContext *ctx = wctx->ctx;

    /* ── Reject if too many windows ────────────────────────────────────── */
    if (wctx->window_count >= FORGE_UI_WINDOW_MAX) {
        SDL_Log("forge_ui_wctx_window_begin: too many windows (max=%d)",
                FORGE_UI_WINDOW_MAX);
        return false;
    }

    /* ── Reject if already inside a window ─────────────────────────────── */
    if (wctx->active_window_idx >= 0) {
        SDL_Log("forge_ui_wctx_window_begin: nested windows not supported");
        return false;
    }

    /* ── Reject if a standalone panel is currently active ───────────────── */
    if (ctx->_panel_active) {
        SDL_Log("forge_ui_wctx_window_begin: a panel is already active "
                "(close it with panel_end before opening a window)");
        return false;
    }

    /* ── The scrollbar uses id+1, collapse toggle uses id+2 ────────────── */
    if (id >= UINT32_MAX - 1) {
        SDL_Log("forge_ui_wctx_window_begin: id must be < UINT32_MAX-1");
        return false;
    }

    /* ── Validate rect origin and dimensions ──────────────────────────── */
    if (!isfinite(state->rect.x) || !isfinite(state->rect.y)) {
        SDL_Log("forge_ui_wctx_window_begin: rect origin must be finite");
        return false;
    }
    if (!(state->rect.w > 0.0f) || !isfinite(state->rect.w) ||
        !(state->rect.h > 0.0f) || !isfinite(state->rect.h)) {
        SDL_Log("forge_ui_wctx_window_begin: rect dimensions must be "
                "positive and finite");
        return false;
    }

    /* ── Register this window ──────────────────────────────────────────── */
    int widx = wctx->window_count;
    ForgeUiWindowEntry *entry = &wctx->window_entries[widx];
    entry->id = id;
    entry->state = state;
    entry->vertex_count = 0;
    entry->index_count = 0;
    wctx->window_count++;

    /* ── Redirect context output to this window's draw list ────────────── */
    forge_ui_win__redirect_to_window(wctx, widx);

    /* ── Determine if this window can receive input ────────────────────── */
    /* A window receives input only if it is the hovered window (topmost
     * under the cursor) or if no window contains the mouse position.
     * This prevents clicking through a foreground window. */
    bool can_receive_input = (wctx->hovered_window_id == FORGE_UI_ID_NONE ||
                              wctx->hovered_window_id == id);

    /* Suppress keyboard input for widgets in windows that are covered
     * by another window.  This prevents a focused text input in a
     * background window from silently accepting keystrokes.  The flag
     * is cleared by window_end so widgets declared after the window
     * (or in a subsequent window) are unaffected.  Visual focus state
     * is intentionally preserved — the window still looks focused. */
    ctx->_keyboard_input_suppressed = !can_receive_input;

    /* ── Find the maximum z_order across all registered windows ────────── */
    int max_z = state->z_order;
    for (int i = 0; i < wctx->prev_window_count; i++) {
        if (wctx->prev_window_z_orders[i] > max_z) {
            max_z = wctx->prev_window_z_orders[i];
        }
    }

    /* ── Title bar geometry ────────────────────────────────────────────── */
    ForgeUiRect title_rect = {
        state->rect.x, state->rect.y,
        state->rect.w, FORGE_UI_WIN_TITLE_HEIGHT
    };

    /* ── Hit test: title bar ───────────────────────────────────────────── */
    bool title_over = can_receive_input &&
                      forge_ui__rect_contains(title_rect,
                                               ctx->mouse_x, ctx->mouse_y);

    /* ── Hit test: entire window (for bring-to-front) ─────────────────── */
    /* Use title-bar-only rect when collapsed so the invisible content
     * area does not intercept clicks meant for windows behind. */
    ForgeUiRect hit_rect = state->rect;
    if (state->collapsed) {
        hit_rect.h = FORGE_UI_WIN_TITLE_HEIGHT;
    }
    bool window_over = can_receive_input &&
                       forge_ui__rect_contains(hit_rect,
                                                ctx->mouse_x, ctx->mouse_y);

    /* ── Bring to front on click ───────────────────────────────────────── */
    bool mouse_pressed = ctx->mouse_down && !ctx->mouse_down_prev;
    if (mouse_pressed && window_over) {
        /* Guard against signed integer overflow: if max_z has reached
         * INT_MAX we cannot increment further.  In practice this requires
         * ~2 billion bring-to-front clicks. */
        if (max_z < INT_MAX && state->z_order < max_z + 1) {
            state->z_order = max_z + 1;
        }
    }

    /* ── Collapse toggle button (id+2) ─────────────────────────────────── */
    Uint32 toggle_id = id + 2;
    float toggle_cx = state->rect.x + FORGE_UI_WIN_TOGGLE_PAD
                      + FORGE_UI_WIN_TOGGLE_SIZE * 0.5f;
    float toggle_cy = state->rect.y + FORGE_UI_WIN_TITLE_HEIGHT * 0.5f;
    float half = FORGE_UI_WIN_TOGGLE_SIZE * 0.5f;

    /* Toggle hit rect (generous click target around the triangle) */
    ForgeUiRect toggle_hit_rect = {
        toggle_cx - half - FORGE_UI_WIN_TOGGLE_HIT_PAD,
        toggle_cy - half - FORGE_UI_WIN_TOGGLE_HIT_PAD,
        FORGE_UI_WIN_TOGGLE_SIZE + FORGE_UI_WIN_TOGGLE_HIT_PAD * 2.0f,
        FORGE_UI_WIN_TOGGLE_SIZE + FORGE_UI_WIN_TOGGLE_HIT_PAD * 2.0f
    };

    bool toggle_over = can_receive_input &&
                       forge_ui__rect_contains(toggle_hit_rect,
                                                ctx->mouse_x, ctx->mouse_y);
    if (toggle_over) {
        ctx->next_hot = toggle_id;
    }

    if (mouse_pressed && ctx->next_hot == toggle_id) {
        ctx->active = toggle_id;
    }

    /* Toggle on release over the collapse button */
    if (ctx->active == toggle_id && !ctx->mouse_down) {
        if (toggle_over) {
            state->collapsed = !state->collapsed;
        }
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* ── Title bar drag (using the title bar area minus the toggle) ────── */
    if (title_over && !toggle_over) {
        ctx->next_hot = id;
    }

    if (mouse_pressed && ctx->next_hot == id && title_over) {
        ctx->active = id;
        /* Store grab offset on the press frame */
        wctx->grab_offset_x = ctx->mouse_x - state->rect.x;
        wctx->grab_offset_y = ctx->mouse_y - state->rect.y;
    }

    /* Drag: update window position while title bar is active */
    if (ctx->active == id && ctx->mouse_down) {
        state->rect.x = ctx->mouse_x - wctx->grab_offset_x;
        state->rect.y = ctx->mouse_y - wctx->grab_offset_y;
    }

    /* Release: clear active on mouse up */
    if (ctx->active == id && !ctx->mouse_down) {
        ctx->active = FORGE_UI_ID_NONE;
    }

    /* ── Draw title bar background ─────────────────────────────────────── */
    /* Recalculate title_rect after potential drag */
    title_rect = (ForgeUiRect){
        state->rect.x, state->rect.y,
        state->rect.w, FORGE_UI_WIN_TITLE_HEIGHT
    };

    if (!state->collapsed) {
        /* Draw full window background first */
        forge_ui__emit_rect(ctx, state->rect,
                            FORGE_UI_WIN_BG_R, FORGE_UI_WIN_BG_G,
                            FORGE_UI_WIN_BG_B, FORGE_UI_WIN_BG_A);
    }

    /* Title bar on top of background */
    forge_ui__emit_rect(ctx, title_rect,
                        FORGE_UI_WIN_TITLE_BG_R, FORGE_UI_WIN_TITLE_BG_G,
                        FORGE_UI_WIN_TITLE_BG_B, FORGE_UI_WIN_TITLE_BG_A);

    /* ── Draw collapse toggle triangle ─────────────────────────────────── */
    {
        /* Recalculate toggle center after potential drag */
        toggle_cx = state->rect.x + FORGE_UI_WIN_TOGGLE_PAD
                    + FORGE_UI_WIN_TOGGLE_SIZE * 0.5f;
        toggle_cy = state->rect.y + FORGE_UI_WIN_TITLE_HEIGHT * 0.5f;

        float tr, tg, tb, ta;
        if (ctx->hot == toggle_id || ctx->active == toggle_id) {
            tr = FORGE_UI_WIN_TOGGLE_HOT_R;
            tg = FORGE_UI_WIN_TOGGLE_HOT_G;
            tb = FORGE_UI_WIN_TOGGLE_HOT_B;
            ta = FORGE_UI_WIN_TOGGLE_HOT_A;
        } else {
            tr = FORGE_UI_WIN_TOGGLE_R;
            tg = FORGE_UI_WIN_TOGGLE_G;
            tb = FORGE_UI_WIN_TOGGLE_B;
            ta = FORGE_UI_WIN_TOGGLE_A;
        }

        if (state->collapsed) {
            /* Right-pointing triangle: vertex order gives CCW winding */
            forge_ui_win__emit_triangle(ctx,
                toggle_cx - half * 0.5f, toggle_cy - half,   /* top-left */
                toggle_cx + half,        toggle_cy,           /* right */
                toggle_cx - half * 0.5f, toggle_cy + half,   /* bottom-left */
                tr, tg, tb, ta);
        } else {
            /* Down-pointing triangle: vertex order gives CCW winding */
            forge_ui_win__emit_triangle(ctx,
                toggle_cx - half, toggle_cy - half * 0.5f,   /* top-left */
                toggle_cx + half, toggle_cy - half * 0.5f,   /* top-right */
                toggle_cx,        toggle_cy + half,           /* bottom */
                tr, tg, tb, ta);
        }
    }

    /* ── Draw title text (offset to the right of the toggle) ───────────── */
    if (title && title[0] != '\0') {
        ForgeUiTextMetrics m = forge_ui_text_measure(ctx->atlas, title, NULL);
        float ascender_px = forge_ui__ascender_px(ctx->atlas);
        float title_text_x = state->rect.x + FORGE_UI_WIN_TOGGLE_PAD
                             + FORGE_UI_WIN_TOGGLE_SIZE + FORGE_UI_WIN_TOGGLE_PAD;
        float title_text_y = state->rect.y
                             + (FORGE_UI_WIN_TITLE_HEIGHT - m.height) * 0.5f
                             + ascender_px;
        forge_ui_ctx_label(ctx, title, title_text_x, title_text_y,
                           FORGE_UI_WIN_TITLE_TEXT_R, FORGE_UI_WIN_TITLE_TEXT_G,
                           FORGE_UI_WIN_TITLE_TEXT_B, FORGE_UI_WIN_TITLE_TEXT_A);
    }

    /* ── If collapsed, we're done: restore buffers and return false ─────── */
    if (state->collapsed) {
        ctx->_keyboard_input_suppressed = false;
        forge_ui_win__restore_from_window(wctx);
        return false;
    }

    /* ── Compute content area (same as panel) ──────────────────────────── */
    float pad = FORGE_UI_WIN_PADDING;
    ForgeUiRect content = {
        state->rect.x + pad,
        state->rect.y + FORGE_UI_WIN_TITLE_HEIGHT + pad,
        state->rect.w - 2.0f * pad - FORGE_UI_SCROLLBAR_WIDTH,
        state->rect.h - FORGE_UI_WIN_TITLE_HEIGHT - 2.0f * pad
    };
    if (content.w < 0.0f) content.w = 0.0f;
    if (content.h < 0.0f) content.h = 0.0f;

    /* ── Sanitize scroll_y ─────────────────────────────────────────────── */
    if (!(state->scroll_y >= 0.0f) || !isfinite(state->scroll_y)) {
        state->scroll_y = 0.0f;
    }

    /* ── Apply mouse wheel scrolling ───────────────────────────────────── */
    if (can_receive_input &&
        ctx->scroll_delta != 0.0f && isfinite(ctx->scroll_delta) &&
        forge_ui__rect_contains(content, ctx->mouse_x, ctx->mouse_y)) {
        state->scroll_y += ctx->scroll_delta * FORGE_UI_SCROLL_SPEED;
        if (state->scroll_y < 0.0f) state->scroll_y = 0.0f;
    }

    /* ── Set clip rect and push layout ─────────────────────────────────── */
    /* Store panel state so panel_end-style cleanup can compute
     * content_height and draw scrollbar. */
    ctx->clip_rect = content;
    ctx->has_clip = true;
    ctx->_panel.rect = state->rect;
    ctx->_panel.content_rect = content;
    ctx->_panel.scroll_y = &state->scroll_y;
    ctx->_panel.id = id;
    ctx->_panel_active = true;

    if (!forge_ui_ctx_layout_push(ctx, content,
                                   FORGE_UI_LAYOUT_VERTICAL,
                                   0.0f, FORGE_UI_WIN_CONTENT_SPACING)) {
        SDL_Log("forge_ui_wctx_window_begin: layout_push failed");
        ctx->has_clip = false;
        ctx->_panel_active = false;
        ctx->_panel.id = FORGE_UI_ID_NONE;
        ctx->_panel.scroll_y = NULL;
        ctx->_keyboard_input_suppressed = false;
        forge_ui_win__restore_from_window(wctx);
        /* Undo registration: discard partial draw data so wctx_end
         * does not render a half-constructed window. */
        entry->vertex_count = 0;
        entry->index_count = 0;
        entry->id = FORGE_UI_ID_NONE;
        entry->state = NULL;
        wctx->window_count--;
        return false;
    }

    /* Record content start y for height measurement */
    if (ctx->layout_depth > 0) {
        ctx->_panel_content_start_y =
            ctx->layout_stack[ctx->layout_depth - 1].cursor_y;
    }

    return true;
}

static inline void forge_ui_wctx_window_end(ForgeUiWindowContext *wctx)
{
    if (!wctx || !wctx->ctx) return;
    if (wctx->active_window_idx < 0) {
        SDL_Log("forge_ui_wctx_window_end: no active window "
                "(missing window_begin or window was collapsed?)");
        return;
    }

    ForgeUiContext *ctx = wctx->ctx;

    /* Guard: if the panel state was not set up (e.g. misuse with
     * multiple window contexts), log, clean up, and restore buffers
     * without running panel_end, which would operate on stale state. */
    if (!ctx->_panel_active) {
        SDL_Log("forge_ui_wctx_window_end: panel not active "
                "(missing window_begin?)");
        ctx->has_clip = false;
        ctx->_keyboard_input_suppressed = false;
        int widx = wctx->active_window_idx;
        wctx->window_entries[widx].vertex_count = 0;
        wctx->window_entries[widx].index_count = 0;
        wctx->window_entries[widx].id = FORGE_UI_ID_NONE;
        wctx->window_entries[widx].state = NULL;
        wctx->window_count--;
        /* Restore main context buffers directly instead of calling
         * restore_from_window, which would (a) fail its idx >= window_count
         * guard now that window_count has been decremented, and (b) overwrite
         * the zeroed entry counts with stale ctx draw-list sizes. */
        ctx->vertices = wctx->saved_vertices;
        ctx->vertex_count = wctx->saved_vertex_count;
        ctx->vertex_capacity = wctx->saved_vertex_capacity;
        ctx->indices = wctx->saved_indices;
        ctx->index_count = wctx->saved_index_count;
        ctx->index_capacity = wctx->saved_index_capacity;
        wctx->active_window_idx = -1;
        return;
    }

    /* Clear keyboard suppression so widgets after this window (or in
     * a subsequent window) are not affected by this window's state. */
    ctx->_keyboard_input_suppressed = false;

    /* Reuse panel_end logic to compute content height, draw scrollbar,
     * clear clip rect, and pop layout. */
    forge_ui_ctx_panel_end(ctx);

    /* Restore main context buffers */
    forge_ui_win__restore_from_window(wctx);
}

#endif /* FORGE_UI_WINDOW_H */
