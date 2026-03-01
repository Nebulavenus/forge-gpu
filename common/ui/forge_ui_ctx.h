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
 *   forge_ui_ctx_label(&ctx, "Hello", x, y, r, g, b, a);
 *   forge_ui_ctx_end(&ctx);
 *
 *   // Use ctx.vertices, ctx.vertex_count, ctx.indices, ctx.index_count
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_UI_CTX_H
#define FORGE_UI_CTX_H

#include <math.h>
#include <string.h>
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

/* Maximum nesting depth for the ID seed stack.  8 levels is enough for
 * most UI hierarchies (e.g. window > panel > row > widget).  The stack
 * is bounds-checked at runtime with SDL_Log on overflow/underflow. */
#define FORGE_UI_ID_STACK_MAX_DEPTH  8

/* ── Button style ───────────────────────────────────────────────────────── */

/* Button appearance varies with interaction state.  Colors follow the
 * project's dark blue-gray theme (#1a1a2e background, #4fc3f7 accent).
 * All values are RGBA floats in [0, 1]. */
#define FORGE_UI_BTN_NORMAL_R   0.145f
#define FORGE_UI_BTN_NORMAL_G   0.176f
#define FORGE_UI_BTN_NORMAL_B   0.290f
#define FORGE_UI_BTN_NORMAL_A   1.00f

#define FORGE_UI_BTN_HOT_R      0.180f
#define FORGE_UI_BTN_HOT_G      0.220f
#define FORGE_UI_BTN_HOT_B      0.376f
#define FORGE_UI_BTN_HOT_A      1.00f

#define FORGE_UI_BTN_ACTIVE_R   0.102f
#define FORGE_UI_BTN_ACTIVE_G   0.125f
#define FORGE_UI_BTN_ACTIVE_B   0.220f
#define FORGE_UI_BTN_ACTIVE_A   1.00f

/* Default button text color — matches theme text #e0e0f0 */
#define FORGE_UI_BTN_TEXT_R     0.878f
#define FORGE_UI_BTN_TEXT_G     0.878f
#define FORGE_UI_BTN_TEXT_B     0.941f
#define FORGE_UI_BTN_TEXT_A     1.00f

/* ── Checkbox style ────────────────────────────────────────────────────── */

/* Checkbox box dimensions.  The box is a square drawn at the left edge
 * of the widget rect, vertically centered.  The label text is drawn to
 * the right of the box with a small gap. */
#define FORGE_UI_CB_BOX_SIZE     18.0f  /* checkbox square side length in pixels */
#define FORGE_UI_CB_INNER_PAD     3.0f  /* padding between box edge and check fill */
#define FORGE_UI_CB_LABEL_GAP     8.0f  /* horizontal gap between box and label */

/* Box outline colors by state — themed dark navy (RGBA floats in [0, 1]) */
#define FORGE_UI_CB_NORMAL_R    0.165f
#define FORGE_UI_CB_NORMAL_G    0.200f
#define FORGE_UI_CB_NORMAL_B    0.314f
#define FORGE_UI_CB_NORMAL_A    1.00f

#define FORGE_UI_CB_HOT_R       0.227f
#define FORGE_UI_CB_HOT_G       0.290f
#define FORGE_UI_CB_HOT_B       0.416f
#define FORGE_UI_CB_HOT_A       1.00f

#define FORGE_UI_CB_ACTIVE_R    0.118f
#define FORGE_UI_CB_ACTIVE_G    0.157f
#define FORGE_UI_CB_ACTIVE_B    0.259f
#define FORGE_UI_CB_ACTIVE_A    1.00f

/* Inner fill color when checked (accent cyan #4fc3f7) */
#define FORGE_UI_CB_CHECK_R     0.310f
#define FORGE_UI_CB_CHECK_G     0.765f
#define FORGE_UI_CB_CHECK_B     0.969f
#define FORGE_UI_CB_CHECK_A     1.00f

/* Checkbox label text color — matches theme text #e0e0f0 */
#define FORGE_UI_CB_TEXT_R      0.878f
#define FORGE_UI_CB_TEXT_G      0.878f
#define FORGE_UI_CB_TEXT_B      0.941f
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

/* Track background color — themed dark navy */
#define FORGE_UI_SL_TRACK_R     0.145f
#define FORGE_UI_SL_TRACK_G     0.176f
#define FORGE_UI_SL_TRACK_B     0.290f
#define FORGE_UI_SL_TRACK_A     1.00f

/* Thumb color varies with interaction state to give visual feedback
 * during drag: normal (idle), hot (cursor hovering), active (dragging). */
#define FORGE_UI_SL_NORMAL_R    0.353f
#define FORGE_UI_SL_NORMAL_G    0.416f
#define FORGE_UI_SL_NORMAL_B    0.533f
#define FORGE_UI_SL_NORMAL_A    1.00f

#define FORGE_UI_SL_HOT_R       0.416f
#define FORGE_UI_SL_HOT_G       0.478f
#define FORGE_UI_SL_HOT_B       0.604f
#define FORGE_UI_SL_HOT_A       1.00f

#define FORGE_UI_SL_ACTIVE_R    0.310f
#define FORGE_UI_SL_ACTIVE_G    0.765f
#define FORGE_UI_SL_ACTIVE_B    0.969f
#define FORGE_UI_SL_ACTIVE_A    1.00f

/* ── Text input style ─────────────────────────────────────────────────── */

/* Text input layout dimensions.  Padding keeps text away from the field
 * edge so characters are readable near the border.  The cursor bar is
 * thin (2 px) to mimic a standard text insertion caret.  The border is
 * 1 px to provide a focused-state indicator without obscuring content. */
#define FORGE_UI_TI_PADDING       6.0f   /* left padding in pixels before text starts */
#define FORGE_UI_TI_CURSOR_WIDTH  2.0f   /* cursor bar width in pixels */
#define FORGE_UI_TI_BORDER_WIDTH  1.0f   /* focused border edge width in pixels */

/* Background colors by state — themed dark navy (RGBA floats in [0, 1]) */
#define FORGE_UI_TI_NORMAL_R   0.078f   /* unfocused: deep dark */
#define FORGE_UI_TI_NORMAL_G   0.094f
#define FORGE_UI_TI_NORMAL_B   0.157f
#define FORGE_UI_TI_NORMAL_A   1.00f

#define FORGE_UI_TI_HOT_R      0.102f   /* hovered (unfocused): subtle highlight */
#define FORGE_UI_TI_HOT_G      0.133f
#define FORGE_UI_TI_HOT_B      0.220f
#define FORGE_UI_TI_HOT_A      1.00f

#define FORGE_UI_TI_FOCUSED_R  0.094f   /* focused: blue tint */
#define FORGE_UI_TI_FOCUSED_G  0.125f
#define FORGE_UI_TI_FOCUSED_B  0.251f
#define FORGE_UI_TI_FOCUSED_A  1.00f

/* Border color when focused (accent cyan #4fc3f7) */
#define FORGE_UI_TI_BORDER_R   0.310f
#define FORGE_UI_TI_BORDER_G   0.765f
#define FORGE_UI_TI_BORDER_B   0.969f
#define FORGE_UI_TI_BORDER_A   1.00f

/* Cursor bar color (accent cyan #4fc3f7) */
#define FORGE_UI_TI_CURSOR_R   0.310f
#define FORGE_UI_TI_CURSOR_G   0.765f
#define FORGE_UI_TI_CURSOR_B   0.969f
#define FORGE_UI_TI_CURSOR_A   1.00f

/* Text color — matches theme text #e0e0f0 */
#define FORGE_UI_TI_TEXT_R     0.878f
#define FORGE_UI_TI_TEXT_G     0.878f
#define FORGE_UI_TI_TEXT_B     0.941f
#define FORGE_UI_TI_TEXT_A     1.00f

/* ── Layout constants ───────────────────────────────────────────────────── */

/* Maximum nesting depth for layout regions.  8 levels is enough for most
 * UI hierarchies (e.g. panel > row > column > widget).  The layout stack
 * lives inside ForgeUiContext and is bounds-checked with runtime guards
 * that log via SDL_Log and return safely on overflow/underflow. */
#define FORGE_UI_LAYOUT_MAX_DEPTH  8

/* ── Panel style ───────────────────────────────────────────────────────── */

/* Title bar height and content padding (pixels) */
#define FORGE_UI_PANEL_TITLE_HEIGHT    30.0f
#define FORGE_UI_PANEL_PADDING         10.0f

/* Panel background RGBA — dark navy, lighter than app bg for contrast */
#define FORGE_UI_PANEL_BG_R     0.118f
#define FORGE_UI_PANEL_BG_G     0.165f
#define FORGE_UI_PANEL_BG_B     0.282f
#define FORGE_UI_PANEL_BG_A     1.00f

/* Title bar background RGBA — brighter navy to distinguish from body */
#define FORGE_UI_PANEL_TITLE_BG_R   0.145f
#define FORGE_UI_PANEL_TITLE_BG_G   0.188f
#define FORGE_UI_PANEL_TITLE_BG_B   0.314f
#define FORGE_UI_PANEL_TITLE_BG_A   1.00f

/* Title bar text color — matches theme text #e0e0f0 */
#define FORGE_UI_PANEL_TITLE_TEXT_R  0.878f
#define FORGE_UI_PANEL_TITLE_TEXT_G  0.878f
#define FORGE_UI_PANEL_TITLE_TEXT_B  0.941f
#define FORGE_UI_PANEL_TITLE_TEXT_A  1.00f

/* ── Scrollbar style ───────────────────────────────────────────────────── */

#define FORGE_UI_SCROLLBAR_WIDTH        10.0f  /* scrollbar track width (pixels) */
#define FORGE_UI_SCROLLBAR_MIN_THUMB    20.0f  /* minimum thumb height (pixels) */

/* Scrollbar track RGBA — themed deep dark navy */
#define FORGE_UI_SB_TRACK_R    0.055f
#define FORGE_UI_SB_TRACK_G    0.063f
#define FORGE_UI_SB_TRACK_B    0.125f
#define FORGE_UI_SB_TRACK_A    1.00f

/* Scrollbar thumb RGBA by state — themed navy blue */
#define FORGE_UI_SB_NORMAL_R   0.227f
#define FORGE_UI_SB_NORMAL_G   0.271f
#define FORGE_UI_SB_NORMAL_B   0.408f
#define FORGE_UI_SB_NORMAL_A   1.00f

#define FORGE_UI_SB_HOT_R      0.290f
#define FORGE_UI_SB_HOT_G      0.345f
#define FORGE_UI_SB_HOT_B      0.471f
#define FORGE_UI_SB_HOT_A      1.00f

#define FORGE_UI_SB_ACTIVE_R   0.310f
#define FORGE_UI_SB_ACTIVE_G   0.765f
#define FORGE_UI_SB_ACTIVE_B   0.969f
#define FORGE_UI_SB_ACTIVE_A   1.00f

/* ── Panel content layout ──────────────────────────────────────────────── */

/* Vertical spacing between child widgets inside a panel (pixels) */
#define FORGE_UI_PANEL_CONTENT_SPACING  8.0f

/* ── Scroll speed ──────────────────────────────────────────────────────── */

/* Pixels scrolled per unit of mouse wheel delta */
#define FORGE_UI_SCROLL_SPEED  30.0f

/* ── Types ──────────────────────────────────────────────────────────────── */

/* A rectangle used for both hit testing and draw emission.  The layout
 * system produces ForgeUiRects, and widget functions consume them --
 * this is the common currency for positioning in the UI. */
typedef struct ForgeUiRect {
    float x;  /* left edge */
    float y;  /* top edge */
    float w;  /* width */
    float h;  /* height */
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
    float                    padding;      /* inset from all four edges */
    float                    spacing;      /* gap between consecutive widgets */
    float                    cursor_x;     /* current placement x position */
    float                    cursor_y;     /* current placement y position */
    float                    remaining_w;  /* width left for more widgets */
    float                    remaining_h;  /* height left for more widgets */
    int                      item_count;   /* widgets placed so far (for spacing) */
} ForgeUiLayout;

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
    float        content_height; /* total height of child widgets (set by panel_end) */
    Uint32       id;             /* widget ID hash (for pre-clamp matching across frames) */
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
    bool  mouse_down;     /* true while the primary button is held */
    bool  mouse_down_prev; /* mouse_down from the previous frame (for edge detection) */

    /* Persistent widget state (survives across frames) */
    Uint32 hot;           /* widget under the cursor (or FORGE_UI_ID_NONE) */
    Uint32 active;        /* widget being pressed (or FORGE_UI_ID_NONE) */

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
 * padding:   inset from all four edges of rect (pixels); clamped to >= 0
 * spacing:   gap between consecutive widgets (pixels); clamped to >= 0
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

/* ── Layout-aware widget variants ──────────────────────────────────────── */
/* These call forge_ui_ctx_layout_next() internally to obtain their rect,
 * so the caller only specifies content (label, ID, state) and a size.
 * The size parameter means height in vertical layouts or width in
 * horizontal layouts. */

/* Label placed by the current layout.  size is the widget height (vertical)
 * or width (horizontal).  Text color uses the same defaults as buttons. */
static inline void forge_ui_ctx_label_layout(ForgeUiContext *ctx,
                                              const char *text,
                                              float size,
                                              float r, float g, float b, float a);

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
    ctx->layout_depth = 0;
    ctx->id_stack_depth = 0;
    ctx->scroll_delta = 0.0f;
    ctx->has_clip = false;
    ctx->_panel_active = false;
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

    /* Snapshot the mouse state for this frame.  All widget calls see the
     * same position and button state, ensuring consistent hit-testing even
     * if the OS delivers new input events between widget calls.
     * Clamp NaN/Inf to 0 so downstream slider and scrollbar drag
     * calculations never produce NaN values written to caller data. */
    ctx->mouse_x = isfinite(mouse_x) ? mouse_x : 0.0f;
    ctx->mouse_y = isfinite(mouse_y) ? mouse_y : 0.0f;
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

    /* Reset scroll and panel state for this frame.  The caller sets
     * scroll_delta after begin if mouse wheel input is available. */
    ctx->scroll_delta = 0.0f;
    ctx->has_clip = false;
    ctx->_panel_active = false;
    ctx->_panel.scroll_y = NULL;

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

static inline void forge_ui_ctx_label(ForgeUiContext *ctx,
                                      const char *text,
                                      float x, float y,
                                      float r, float g, float b, float a)
{
    if (!ctx || !text || !ctx->atlas) return;
    if (!isfinite(x) || !isfinite(y)) return;

    ForgeUiTextOpts opts = { 0.0f, FORGE_UI_TEXT_ALIGN_LEFT, r, g, b, a };
    ForgeUiTextLayout layout;
    if (forge_ui_text_layout(ctx->atlas, text, x, y, &opts, &layout)) {
        forge_ui__emit_text_layout(ctx, &layout);
        forge_ui_text_layout_free(&layout);
    }
}

static inline bool forge_ui_ctx_button(ForgeUiContext *ctx,
                                       const char *text,
                                       ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !text || text[0] == '\0') return false;
    if (!isfinite(rect.x) || !isfinite(rect.y) ||
        !isfinite(rect.w) || !isfinite(rect.h)) return false;
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

    forge_ui_ctx_label(ctx, disp_buf, text_x, text_y,
                       FORGE_UI_BTN_TEXT_R, FORGE_UI_BTN_TEXT_G,
                       FORGE_UI_BTN_TEXT_B, FORGE_UI_BTN_TEXT_A);

    return clicked;
}

static inline bool forge_ui_ctx_checkbox(ForgeUiContext *ctx,
                                          const char *label,
                                          bool *value,
                                          ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !label || !value || label[0] == '\0') return false;
    if (!isfinite(rect.x) || !isfinite(rect.y) ||
        !isfinite(rect.w) || !isfinite(rect.h)) return false;
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

    float label_x = box_x + FORGE_UI_CB_BOX_SIZE + FORGE_UI_CB_LABEL_GAP;
    float label_y = rect.y + (rect.h - ctx->atlas->pixel_height) * 0.5f
                    + ascender_px;

    forge_ui_ctx_label(ctx, disp_buf, label_x, label_y,
                       FORGE_UI_CB_TEXT_R, FORGE_UI_CB_TEXT_G,
                       FORGE_UI_CB_TEXT_B, FORGE_UI_CB_TEXT_A);

    return toggled;
}

static inline bool forge_ui_ctx_slider(ForgeUiContext *ctx,
                                        const char *label,
                                        float *value,
                                        float min_val, float max_val,
                                        ForgeUiRect rect)
{
    if (!ctx || !ctx->atlas || !value || !label || label[0] == '\0') return false;
    if (!isfinite(rect.x) || !isfinite(rect.y) ||
        !isfinite(rect.w) || !isfinite(rect.h)) return false;
    /* Sanitize *value: NaN/Inf would poison the thumb position and
     * propagate into vertex data.  Clamp to min_val as a safe default. */
    if (!isfinite(*value)) *value = min_val;
    Uint32 id = forge_ui_hash_id(ctx, label);
    if (!(max_val > min_val)) return false;  /* also rejects NaN */

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
                                            const char *label,
                                            ForgeUiTextInputState *state,
                                            ForgeUiRect rect,
                                            bool cursor_visible)
{
    if (!ctx || !ctx->atlas || !state || !state->buffer
        || !label || label[0] == '\0') return false;
    if (!isfinite(rect.x) || !isfinite(rect.y) ||
        !isfinite(rect.w) || !isfinite(rect.h)) return false;
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
    float ascender_px = forge_ui__ascender_px(ctx->atlas);

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
    if (!isfinite(rect.x) || !isfinite(rect.y) ||
        !isfinite(rect.w) || !isfinite(rect.h)) {
        return false;
    }

    /* Reject NaN/Inf in padding and spacing; clamp negatives to 0 */
    if (isnan(padding) || isinf(padding)) padding = 0.0f;
    if (isnan(spacing) || isinf(spacing)) spacing = 0.0f;
    if (padding < 0.0f) padding = 0.0f;
    if (spacing < 0.0f) spacing = 0.0f;

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
    if (size < 0.0f || isnan(size) || isinf(size)) size = 0.0f;

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

static inline void forge_ui_ctx_label_layout(ForgeUiContext *ctx,
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

    forge_ui_ctx_label(ctx, text, rect.x, text_y, r, g, b, a);
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
    if (!(max_val > min_val)) return false;  /* also rejects NaN */
    ForgeUiRect rect = forge_ui_ctx_layout_next(ctx, size);
    return forge_ui_ctx_slider(ctx, label, value, min_val, max_val, rect);
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
    if (!isfinite(rect.x) || !isfinite(rect.y)) {
        SDL_Log("forge_ui_ctx_panel_begin: rect origin must be finite");
        return false;
    }

    /* Reject non-positive or non-finite rect dimensions.  !(x > 0) catches
     * NaN and ≤ 0; !isfinite catches ±Inf.  +Inf would produce infinite
     * vertex positions, corrupting the draw data. */
    if (!(rect.w > 0.0f) || !isfinite(rect.w) ||
        !(rect.h > 0.0f) || !isfinite(rect.h)) {
        SDL_Log("forge_ui_ctx_panel_begin: rect dimensions must be "
                "positive and finite");
        return false;
    }

    /* Sanitize *scroll_y: a negative offset would shift content downward
     * (exposing blank space above the first widget), NaN would poison
     * every layout_next position, and +Inf would push all widgets to
     * -Inf.  The !(x >= 0) form catches NaN and negatives; !isfinite
     * catches ±Inf. */
    if (!(*scroll_y >= 0.0f) || !isfinite(*scroll_y)) *scroll_y = 0.0f;

    /* ── Draw panel background ────────────────────────────────────────── */
    forge_ui__emit_rect(ctx, rect,
                        FORGE_UI_PANEL_BG_R, FORGE_UI_PANEL_BG_G,
                        FORGE_UI_PANEL_BG_B, FORGE_UI_PANEL_BG_A);

    /* ── Draw title bar ───────────────────────────────────────────────── */
    ForgeUiRect title_rect = {
        rect.x, rect.y,
        rect.w, FORGE_UI_PANEL_TITLE_HEIGHT
    };
    forge_ui__emit_rect(ctx, title_rect,
                        FORGE_UI_PANEL_TITLE_BG_R, FORGE_UI_PANEL_TITLE_BG_G,
                        FORGE_UI_PANEL_TITLE_BG_B, FORGE_UI_PANEL_TITLE_BG_A);

    /* Center the title text in the title bar (strip ## suffix) */
    if (title && title[0] != '\0') {
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
        float ty = rect.y + (FORGE_UI_PANEL_TITLE_HEIGHT - m.height) * 0.5f
                   + ascender_px;
        forge_ui_ctx_label(ctx, disp_buf, tx, ty,
                           FORGE_UI_PANEL_TITLE_TEXT_R, FORGE_UI_PANEL_TITLE_TEXT_G,
                           FORGE_UI_PANEL_TITLE_TEXT_B, FORGE_UI_PANEL_TITLE_TEXT_A);
    }

    /* ── Compute content area ─────────────────────────────────────────── */
    float pad = FORGE_UI_PANEL_PADDING;
    ForgeUiRect content = {
        rect.x + pad,
        rect.y + FORGE_UI_PANEL_TITLE_HEIGHT + pad,
        rect.w - 2.0f * pad - FORGE_UI_SCROLLBAR_WIDTH,
        rect.h - FORGE_UI_PANEL_TITLE_HEIGHT - 2.0f * pad
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
        return false;
    }

    /* ── Apply mouse wheel scrolling ──────────────────────────────────── */
    /* Validate scroll_delta: a NaN or ±Inf delta (from a corrupt input
     * event or driver bug) would make *scroll_y non-finite, poisoning
     * every subsequent layout position.  Treat non-finite as zero. */
    if (ctx->scroll_delta != 0.0f && isfinite(ctx->scroll_delta) &&
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
    if (!forge_ui_ctx_layout_push(ctx, content,
                                   FORGE_UI_LAYOUT_VERTICAL,
                                   0.0f, FORGE_UI_PANEL_CONTENT_SPACING)) {
        SDL_Log("forge_ui_ctx_panel_begin: layout_push failed (stack full?)");
        ctx->has_clip = false;
        ctx->_panel_active = false;
        /* Clear identity/state set above so the pre-clamp check on the
         * next frame does not match against a panel that never completed. */
        ctx->_panel.id = FORGE_UI_ID_NONE;
        ctx->_panel.scroll_y = NULL;
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
        if (content_h < 0.0f) content_h = 0.0f;
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
    float track_x = ctx->_panel.rect.x + ctx->_panel.rect.w
                     - FORGE_UI_PANEL_PADDING - FORGE_UI_SCROLLBAR_WIDTH;
    float track_y = cr.y;
    float track_h = cr.h;
    float track_w = FORGE_UI_SCROLLBAR_WIDTH;

    /* Track background */
    ForgeUiRect track_rect = { track_x, track_y, track_w, track_h };
    forge_ui__emit_rect(ctx, track_rect,
                        FORGE_UI_SB_TRACK_R, FORGE_UI_SB_TRACK_G,
                        FORGE_UI_SB_TRACK_B, FORGE_UI_SB_TRACK_A);

    /* Thumb geometry — proportional height, clamped to [MIN_THUMB, track_h]
     * so the thumb never overflows the track even on very short panels. */
    float thumb_h = track_h * visible_h / content_h;
    if (thumb_h < FORGE_UI_SCROLLBAR_MIN_THUMB)
        thumb_h = FORGE_UI_SCROLLBAR_MIN_THUMB;
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
    float th_r, th_g, th_b, th_a;
    if (ctx->active == sb_id) {
        th_r = FORGE_UI_SB_ACTIVE_R;  th_g = FORGE_UI_SB_ACTIVE_G;
        th_b = FORGE_UI_SB_ACTIVE_B;  th_a = FORGE_UI_SB_ACTIVE_A;
    } else if (ctx->hot == sb_id) {
        th_r = FORGE_UI_SB_HOT_R;  th_g = FORGE_UI_SB_HOT_G;
        th_b = FORGE_UI_SB_HOT_B;  th_a = FORGE_UI_SB_HOT_A;
    } else {
        th_r = FORGE_UI_SB_NORMAL_R;  th_g = FORGE_UI_SB_NORMAL_G;
        th_b = FORGE_UI_SB_NORMAL_B;  th_a = FORGE_UI_SB_NORMAL_A;
    }

    /* Recompute thumb_y after potential drag update */
    t = (max_scroll > 0.0f) ? *scroll_y / max_scroll : 0.0f;
    thumb_y = track_y + t * thumb_range;
    thumb_rect = (ForgeUiRect){ track_x, thumb_y, track_w, thumb_h };
    forge_ui__emit_rect(ctx, thumb_rect, th_r, th_g, th_b, th_a);

    /* ── Pop the ID scope pushed by panel_begin ────────────────────────── */
    forge_ui_pop_id(ctx);
}

#endif /* FORGE_UI_CTX_H */
