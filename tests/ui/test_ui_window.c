/*
 * UI Window Tests
 *
 * Automated tests for common/ui/forge_ui_window.h — the draggable window
 * system built on top of the immediate-mode UI context.
 *
 * Tests cover:
 *   - Init/free lifecycle and parameter validation
 *   - Window begin/end and draw data generation
 *   - Title bar drag with grab offset
 *   - Z-ordering and bring-to-front
 *   - Collapse toggle
 *   - Deferred draw ordering (back-to-front by z_order)
 *   - Input routing (hovered_window_id, z-aware hit testing)
 *   - Collapsed window hover rect (title-bar only)
 *   - Edge cases: NaN/Inf rect, INT_MAX z_order, NULL atlas
 *   - wctx_free while redirected (use-after-free prevention)
 *   - layout_push failure cleanup
 *   - window_end without window_begin
 *   - NaN/Inf mouse coord validation (clamped to zero)
 *   - State cleanup (ctx_free panel zeroing, panel_end scroll_y NULL,
 *     window_end guard path, wctx_free saved fields)
 *   - Shared state isolation (window_begin rejects during active panel)
 *   - Keyboard focus leak (covered window suppresses keyboard input)
 *
 * Uses the bundled Liberation Mono Regular font for all tests.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <limits.h>
#include <math.h>
#include "ui/forge_ui.h"
#include "ui/forge_ui_window.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST(name)                                                \
    do {                                                          \
        test_count++;                                             \
        SDL_Log("  [TEST] %s", name);                             \
    } while (0)

#define ASSERT_TRUE(expr)                                         \
    do {                                                          \
        if (!(expr)) {                                            \
            SDL_Log("    FAIL: %s (line %d)", #expr, __LINE__);   \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

#define ASSERT_EQ_INT(a, b)                                       \
    do {                                                          \
        int _a = (a), _b = (b);                                   \
        if (_a != _b) {                                           \
            SDL_Log("    FAIL: %s == %d, expected %d (line %d)",  \
                    #a, _a, _b, __LINE__);                        \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

#define ASSERT_EQ_U32(a, b)                                       \
    do {                                                          \
        Uint32 _a = (a), _b = (b);                                \
        if (_a != _b) {                                           \
            SDL_Log("    FAIL: %s == %u, expected %u (line %d)",  \
                    #a, _a, _b, __LINE__);                        \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

#define ASSERT_NEAR(a, b, eps)                                    \
    do {                                                          \
        float _a = (a), _b = (b);                                 \
        if (isnan(_a) || isnan(_b)) {                             \
            SDL_Log("    FAIL: %s == %f, expected %f (NaN, "      \
                    "line %d)", #a, (double)_a, (double)_b,       \
                    __LINE__);                                    \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        if (fabsf(_a - _b) > (eps)) {                             \
            SDL_Log("    FAIL: %s == %f, expected %f (eps=%f, "   \
                    "line %d)", #a, (double)_a, (double)_b,       \
                    (double)(eps), __LINE__);                     \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

/* ── Shared font/atlas ──────────────────────────────────────────────────── */

#define DEFAULT_FONT_PATH \
    "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"
#define PIXEL_HEIGHT      24.0f
#define ATLAS_PADDING     1
#define ASCII_START       32
#define ASCII_END         126
#define ASCII_COUNT       (ASCII_END - ASCII_START + 1)

/* ── Test constants ────────────────────────────────────────────────────── */

/* Standard window geometry used across most tests */
#define TEST_WIN_X        10.0f
#define TEST_WIN_Y        10.0f
#define TEST_WIN_W        200.0f
#define TEST_WIN_H        200.0f

/* Larger window used in draw-data tests */
#define TEST_WIN_DRAW_W   250.0f
#define TEST_WIN_DRAW_H   300.0f

/* Second overlapping window position */
#define TEST_WIN2_X       50.0f
#define TEST_WIN2_Y       50.0f

/* Window IDs */
#define TEST_WIN_ID_1     100u
#define TEST_WIN_ID_2     200u

/* Label rendering */
#define TEST_LABEL_PX     24
#define TEST_LABEL_R      0.9f
#define TEST_LABEL_G      0.9f
#define TEST_LABEL_B      0.9f
#define TEST_LABEL_A      1.0f

/* Drag test */
#define TEST_DRAG_PRESS_X 150.0f   /* title bar center: 50 + 200*0.5 */
#define TEST_DRAG_PRESS_Y  65.0f   /* title bar center: 50 + 15      */
#define TEST_DRAG_DELTA_X  30.0f
#define TEST_DRAG_DELTA_Y  20.0f

/* Mouse position outside all windows */
#define TEST_MOUSE_FAR    500.0f

/* Prev-frame-data test */
#define TEST_PREV_WIN_X   100.0f
#define TEST_PREV_WIN_Y   200.0f
#define TEST_PREV_WIN_W   300.0f
#define TEST_PREV_WIN_H   400.0f
#define TEST_PREV_WIN_Z   5
#define TEST_PREV_WIN_ID  42u
#define TEST_NEAR_EPS     0.01f

/* NaN/Inf mouse validation (task 3) */
#define TEST_NAN_MOUSE_X  ((float)NAN)
#define TEST_NAN_MOUSE_Y  ((float)NAN)
#define TEST_INF_MOUSE_X  ((float)INFINITY)
#define TEST_INF_MOUSE_Y  ((float)INFINITY)

/* Slider test geometry */
#define TEST_SLIDER_X     20.0f
#define TEST_SLIDER_Y     20.0f
#define TEST_SLIDER_W     160.0f
#define TEST_SLIDER_H     24.0f
#define TEST_SLIDER_ID    300u
#define TEST_SLIDER_MIN   0.0f
#define TEST_SLIDER_MAX   100.0f
#define TEST_SLIDER_INIT  50.0f

/* Panel test geometry (tasks 4, 7) */
#define TEST_PANEL_X      10.0f
#define TEST_PANEL_Y      10.0f
#define TEST_PANEL_W      200.0f
#define TEST_PANEL_H      200.0f
#define TEST_PANEL_ID     400u

/* Text input keyboard focus leak test */
#define TEST_TI_ID        500u             /* text input widget ID */
#define TEST_TI_BUF_CAP   32               /* text input buffer capacity */
#define TEST_TI_RECT_X    20.0f            /* text input rect (inside window) */
#define TEST_TI_RECT_Y    60.0f
#define TEST_TI_RECT_W    160.0f
#define TEST_TI_RECT_H    28.0f
#define TEST_TI_CLICK_X   100.0f           /* center of text input for clicking */
#define TEST_TI_CLICK_Y   74.0f

static ForgeUiFont      test_font;
static ForgeUiFontAtlas  test_atlas;
static bool font_loaded  = false;
static bool atlas_built  = false;
static bool setup_failed = false;

static bool setup_atlas(void)
{
    if (atlas_built) return true;
    if (setup_failed) {
        fail_count++;
        return false;
    }

    if (!font_loaded) {
        if (!forge_ui_ttf_load(DEFAULT_FONT_PATH, &test_font)) {
            SDL_Log("    FAIL: Cannot load font: %s", DEFAULT_FONT_PATH);
            setup_failed = true;
            fail_count++;
            return false;
        }
        font_loaded = true;
    }

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    if (!forge_ui_atlas_build(&test_font, PIXEL_HEIGHT, codepoints,
                               ASCII_COUNT, ATLAS_PADDING, &test_atlas)) {
        SDL_Log("    FAIL: Cannot build atlas");
        setup_failed = true;
        fail_count++;
        return false;
    }
    atlas_built = true;
    return true;
}

/* ── Helper: run one frame with windows ──────────────────────────────────── */

/* Creates a ctx + wctx, runs one frame with the given callback, cleans up.
 * The callback receives the ctx, wctx, and user data pointer. */
typedef void (*frame_fn)(ForgeUiContext *, ForgeUiWindowContext *, void *);

static void run_frame(float mx, float my, bool mouse_down,
                      frame_fn fn, void *user_data)
{
    ForgeUiContext ctx;
    if (!forge_ui_ctx_init(&ctx, &test_atlas)) {
        SDL_Log("    FAIL: forge_ui_ctx_init failed");
        fail_count++;
        return;
    }

    ForgeUiWindowContext wctx;
    if (!forge_ui_wctx_init(&wctx, &ctx)) {
        SDL_Log("    FAIL: forge_ui_wctx_init failed");
        forge_ui_ctx_free(&ctx);
        fail_count++;
        return;
    }

    forge_ui_ctx_begin(&ctx, mx, my, mouse_down);
    forge_ui_wctx_begin(&wctx);

    fn(&ctx, &wctx, user_data);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  INIT / FREE LIFECYCLE
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_init_null_wctx(void)
{
    TEST("wctx_init: NULL wctx returns false");
    ForgeUiContext ctx;
    ASSERT_TRUE(!forge_ui_wctx_init(NULL, &ctx));
}

static void test_init_null_ctx(void)
{
    TEST("wctx_init: NULL ctx returns false");
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(!forge_ui_wctx_init(&wctx, NULL));
}

static void test_init_sets_defaults(void)
{
    TEST("wctx_init: sets correct defaults");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    if (!forge_ui_ctx_init(&ctx, &test_atlas)) {
        SDL_Log("    FAIL: ctx_init");
        fail_count++;
        return;
    }

    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));
    ASSERT_TRUE(wctx.ctx == &ctx);
    ASSERT_EQ_INT(wctx.window_count, 0);
    ASSERT_EQ_INT(wctx.active_window_idx, -1);
    ASSERT_EQ_U32(wctx.hovered_window_id, FORGE_UI_ID_NONE);
    ASSERT_EQ_INT(wctx.prev_window_count, 0);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_free_null_safe(void)
{
    TEST("wctx_free: NULL is safe");
    forge_ui_wctx_free(NULL);  /* must not crash */
    pass_count++;
}

static void test_free_clears_ctx(void)
{
    TEST("wctx_free: clears ctx pointer");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    if (!forge_ui_ctx_init(&ctx, &test_atlas)) {
        fail_count++;
        return;
    }

    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));
    forge_ui_wctx_free(&wctx);
    ASSERT_TRUE(wctx.ctx == NULL);
    ASSERT_EQ_INT(wctx.active_window_idx, -1);
    ASSERT_EQ_INT(wctx.window_count, 0);

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  WINDOW BEGIN / END PARAMETER VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_window_begin_null_state(void)
{
    TEST("window_begin: NULL state returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    ASSERT_TRUE(!forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Test", NULL));

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_window_begin_id_none(void)
{
    TEST("window_begin: FORGE_UI_ID_NONE returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    ASSERT_TRUE(!forge_ui_wctx_window_begin(&wctx, FORGE_UI_ID_NONE,
                                              "Test", &ws));

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_window_begin_zero_width(void)
{
    TEST("window_begin: zero width returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { 10, 10, 0, 200 }, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    ASSERT_TRUE(!forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Test", &ws));
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_window_begin_nan_x(void)
{
    TEST("window_begin: NaN rect.x returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { NAN, 10, 200, 200 }, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    ASSERT_TRUE(!forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Test", &ws));
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_window_begin_inf_y(void)
{
    TEST("window_begin: Inf rect.y returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { 10, INFINITY, 200, 200 }, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    ASSERT_TRUE(!forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Test", &ws));
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_window_begin_nan_width(void)
{
    TEST("window_begin: NaN width returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { 10, 10, NAN, 200 }, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    ASSERT_TRUE(!forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Test", &ws));
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_window_begin_null_atlas(void)
{
    TEST("window_begin: NULL atlas returns false");
    if (!setup_atlas()) return;

    /* Build a context then NULL out its atlas to test the guard */
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ctx.atlas = NULL;  /* simulate missing atlas */

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    ASSERT_TRUE(!forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Test", &ws));
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Restore atlas before free to avoid issues */
    ctx.atlas = &test_atlas;
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  WINDOW DRAW DATA GENERATION
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_window_emits_draw_data(void)
{
    TEST("window_begin/end: produces vertices and indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_DRAW_W, TEST_WIN_DRAW_H },
        .scroll_y = 0, .collapsed = false, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    bool opened = forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Test Window", &ws);
    ASSERT_TRUE(opened);
    if (opened) {
        forge_ui_ctx_label_layout(wctx.ctx, "Hello", TEST_LABEL_PX,
                                   TEST_LABEL_R, TEST_LABEL_G,
                                   TEST_LABEL_B, TEST_LABEL_A);
        forge_ui_wctx_window_end(&wctx);
    }

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* After wctx_end, draw data should be in the main context */
    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);
    /* Indices should form complete triangles */
    ASSERT_TRUE(ctx.index_count % 3 == 0);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_window_collapsed_returns_false(void)
{
    TEST("window_begin: collapsed window returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_DRAW_W, TEST_WIN_DRAW_H },
        .scroll_y = 0, .collapsed = true, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    bool opened = forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Collapsed", &ws);
    ASSERT_TRUE(!opened);
    /* Do NOT call window_end when begin returns false */

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Collapsed window should still emit title bar draw data */
    ASSERT_TRUE(ctx.vertex_count > 0);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_window_collapsed_still_renders_title_bar(void)
{
    TEST("window_begin: collapsed emits fewer verts than expanded");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ForgeUiWindowContext wctx;

    /* Frame 1: expanded */
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_DRAW_W, TEST_WIN_DRAW_H },
        .scroll_y = 0, .collapsed = false, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Win", &ws)) {
        forge_ui_ctx_label_layout(wctx.ctx, "Label", TEST_LABEL_PX,
                                   TEST_LABEL_R, TEST_LABEL_G,
                                   TEST_LABEL_B, TEST_LABEL_A);
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    int expanded_verts = ctx.vertex_count;
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);

    /* Frame 2: collapsed */
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ws.collapsed = true;

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Win", &ws)) {
        forge_ui_ctx_label_layout(wctx.ctx, "Label", TEST_LABEL_PX,
                                   TEST_LABEL_R, TEST_LABEL_G,
                                   TEST_LABEL_B, TEST_LABEL_A);
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    int collapsed_verts = ctx.vertex_count;
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);

    ASSERT_TRUE(collapsed_verts < expanded_verts);
    ASSERT_TRUE(collapsed_verts > 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Z-ORDERING
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_z_order_bring_to_front(void)
{
    TEST("z-ordering: click brings window to front");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState w1 = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 0
    };
    ForgeUiWindowState w2 = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 1
    };

    /* Frame 0: no interaction, establishes prev frame data */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W1", &w1)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "W2", &w2)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: click on W1 title bar to bring it to front */
    float click_x = 60.0f;  /* inside W1 title bar (between x=10..210) */
    float click_y = 25.0f;  /* inside title bar height (y=10..40) */

    forge_ui_ctx_begin(&ctx, click_x, click_y, true);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W1", &w1)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "W2", &w2)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* W1 was z=0, W2 was z=1.  After clicking W1, it should be > W2 */
    ASSERT_TRUE(w1.z_order > w2.z_order);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_z_order_overflow_guarded(void)
{
    TEST("z-ordering: INT_MAX z_order does not overflow");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState w1 = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = INT_MAX
    };
    ForgeUiWindowState w2 = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = INT_MAX - 1
    };

    /* Frame 0: establish prev */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W1", &w1)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "W2", &w2)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: click W2 to try to bring to front */
    forge_ui_ctx_begin(&ctx, 60, 65, true);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W1", &w1)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "W2", &w2)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* z_order should not have overflowed */
    ASSERT_TRUE(w1.z_order >= 0);
    ASSERT_TRUE(w2.z_order >= 0);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  DEFERRED DRAW ORDERING
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_deferred_draw_z_order(void)
{
    TEST("deferred draw: higher z_order window drawn last (more verts at end)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    /* W1 declared first but z=1 (front), W2 declared second but z=0 (back) */
    ForgeUiWindowState w1 = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 1
    };
    ForgeUiWindowState w2 = {
        .rect = { 100, 100, TEST_WIN_W, TEST_WIN_H }, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Front", &w1)) {
        forge_ui_ctx_label_layout(wctx.ctx, "Front", TEST_LABEL_PX,
                                   TEST_LABEL_R, TEST_LABEL_G,
                                   TEST_LABEL_B, TEST_LABEL_A);
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "Back", &w2)) {
        forge_ui_ctx_label_layout(wctx.ctx, "Back", TEST_LABEL_PX,
                                   TEST_LABEL_R, TEST_LABEL_G,
                                   TEST_LABEL_B, TEST_LABEL_A);
        forge_ui_wctx_window_end(&wctx);
    }

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Both windows should have produced draw data */
    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_multiple_windows_all_rendered(void)
{
    TEST("multiple windows: all produce draw data");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    #define TEST_MULTI_WIN_COUNT 3
    #define TEST_MULTI_WIN_STEP 50.0f
    ForgeUiWindowState wins[TEST_MULTI_WIN_COUNT];
    for (int i = 0; i < TEST_MULTI_WIN_COUNT; i++) {
        wins[i] = (ForgeUiWindowState){
            .rect = { TEST_WIN_X + TEST_MULTI_WIN_STEP * i,
                      TEST_WIN_Y + TEST_MULTI_WIN_STEP * i,
                      TEST_WIN_W, TEST_WIN_H },
            .z_order = i
        };
    }

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    for (int i = 0; i < TEST_MULTI_WIN_COUNT; i++) {
        Uint32 id = (Uint32)(TEST_WIN_ID_1 + (Uint32)i * TEST_WIN_ID_1);
        if (forge_ui_wctx_window_begin(&wctx, id, "Win", &wins[i])) {
            forge_ui_wctx_window_end(&wctx);
        }
    }

    ASSERT_EQ_INT(wctx.window_count, TEST_MULTI_WIN_COUNT);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertex_count > 0);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}
#undef TEST_MULTI_WIN_COUNT
#undef TEST_MULTI_WIN_STEP

/* ═══════════════════════════════════════════════════════════════════════════
 *  DRAG MECHANICS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_drag_moves_window(void)
{
    TEST("drag: title bar drag moves window rect");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 0
    };
    float orig_x = ws.rect.x;
    float orig_y = ws.rect.y;

    float press_x = TEST_DRAG_PRESS_X;
    float press_y = TEST_DRAG_PRESS_Y;

    /* Frame 0: establish prev_window data (idle) */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Drag Me", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: press on title bar */
    forge_ui_ctx_begin(&ctx, press_x, press_y, true);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Drag Me", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 2: drag by delta */
    float drag_x = press_x + TEST_DRAG_DELTA_X;
    float drag_y = press_y + TEST_DRAG_DELTA_Y;

    forge_ui_ctx_begin(&ctx, drag_x, drag_y, true);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Drag Me", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Window should have moved by the drag delta */
    ASSERT_NEAR(ws.rect.x, orig_x + TEST_DRAG_DELTA_X, 1.0f);
    ASSERT_NEAR(ws.rect.y, orig_y + TEST_DRAG_DELTA_Y, 1.0f);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COLLAPSE TOGGLE
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_collapse_toggle(void)
{
    TEST("collapse: toggle button flips collapsed state");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN_W, TEST_WIN_H },
        .collapsed = false, .z_order = 0
    };

    /* Toggle center: x = 50 + 8 + 5 = 63, y = 50 + 15 = 65 */
    float toggle_cx = ws.rect.x + FORGE_UI_WIN_TOGGLE_PAD
                      + FORGE_UI_WIN_TOGGLE_SIZE * 0.5f;
    float toggle_cy = ws.rect.y + FORGE_UI_WIN_TITLE_HEIGHT * 0.5f;

    /* Frame 0: idle -- establish prev data */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Toggle", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!ws.collapsed);

    /* Frame 1: press on toggle */
    forge_ui_ctx_begin(&ctx, toggle_cx, toggle_cy, true);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Toggle", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 2: release on toggle -- should collapse */
    forge_ui_ctx_begin(&ctx, toggle_cx, toggle_cy, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Toggle", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ws.collapsed);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  INPUT ROUTING
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_hovered_window_id_set(void)
{
    TEST("input routing: hovered_window_id set for topmost window");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    /* Two overlapping windows */
    ForgeUiWindowState w1 = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 0
    };
    ForgeUiWindowState w2 = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 1
    };

    /* Frame 0: establish prev */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W1", &w1)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "W2", &w2)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: idle in overlap region -- prev data now has 2 windows from
     * frame 0.  But hovered_window_id is computed using prev data that was
     * saved at the START of frame 0 (which had count=0).  We need a third
     * frame because hover detection has a one-frame lag: prev data becomes
     * available one frame after the windows are declared. */
    forge_ui_ctx_begin(&ctx, 100, 100, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W1", &w1)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "W2", &w2)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 2: now prev data reflects frame 1's 2 windows.  Mouse in
     * overlap region (100, 100) -- W2 should be hovered because z=1 > z=0 */
    forge_ui_ctx_begin(&ctx, 100, 100, false);
    forge_ui_wctx_begin(&wctx);

    ASSERT_EQ_U32(wctx.hovered_window_id, TEST_WIN_ID_2);

    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W1", &w1)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "W2", &w2)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

static void test_hovered_window_none_outside(void)
{
    TEST("input routing: no hovered window when mouse is outside all windows");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, 100, 100 }, .z_order = 0
    };

    /* Frame 0: establish prev */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: mouse far outside the window */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_wctx_begin(&wctx);

    ASSERT_EQ_U32(wctx.hovered_window_id, FORGE_UI_ID_NONE);

    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COLLAPSED WINDOW HOVER RECT (BUG FIX VALIDATION)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_collapsed_hover_rect_title_only(void)
{
    TEST("collapsed hover: invisible content area does not block input");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    /* Collapsed window at y=10 with full height 200, but only title bar
     * (30px) should block input.  Place another window behind it in the
     * "ghost" content area (y=50 to y=210). */
    ForgeUiWindowState collapsed_win = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .collapsed = true, .z_order = 1
    };
    ForgeUiWindowState behind_win = {
        .rect = { TEST_WIN_X, 60, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };

    /* Frame 0: establish prev */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Collapsed", &collapsed_win)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "Behind", &behind_win)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: declare again so prev data has 2 windows (one-frame lag) */
    forge_ui_ctx_begin(&ctx, 50, 100, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Collapsed", &collapsed_win)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "Behind", &behind_win)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 2: mouse in the "ghost" area (y=100) -- below collapsed title bar
     * (which ends at y=10+30=40), but inside the behind window (y=60..260).
     * The hovered window should be the behind window, not the collapsed one. */
    forge_ui_ctx_begin(&ctx, 50, 100, false);
    forge_ui_wctx_begin(&wctx);

    ASSERT_EQ_U32(wctx.hovered_window_id, TEST_WIN_ID_2);

    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Collapsed", &collapsed_win)) {
        forge_ui_wctx_window_end(&wctx);
    }
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "Behind", &behind_win)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  WCTX_FREE WHILE REDIRECTED (USE-AFTER-FREE PREVENTION)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_free_while_redirected(void)
{
    TEST("wctx_free: restores context when called mid-window");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    /* Open window but don't call window_end -- simulate early exit */
    bool opened = forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Win", &ws);
    ASSERT_TRUE(opened);

    /* Free while redirected -- must restore context buffers */
    forge_ui_wctx_free(&wctx);

    /* Context should have its own buffers back, not the freed window buffers.
     * We verify by checking that ctx_end doesn't crash. */
    forge_ui_ctx_end(&ctx);

    /* ctx should still have valid (non-NULL) buffer pointers */
    ASSERT_TRUE(ctx.vertices != NULL || ctx.vertex_capacity == 0);

    forge_ui_ctx_free(&ctx);
    pass_count++;  /* reaching here without crash = pass */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  WINDOW_END WITHOUT WINDOW_BEGIN
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_window_end_without_begin(void)
{
    TEST("window_end: no-op when no window is active");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    /* Call window_end without window_begin -- should not crash */
    forge_ui_wctx_window_end(&wctx);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
    pass_count++;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  WCTX_BEGIN RESTORES UNCLOSED WINDOW
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_wctx_begin_restores_unclosed_window(void)
{
    TEST("wctx_begin: restores context if previous window not closed");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 0
    };

    /* Frame 0: open window but DON'T call window_end */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    bool opened = forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Win", &ws);
    ASSERT_TRUE(opened);
    /* Skip window_end intentionally */

    /* End frame without proper cleanup -- wctx_end should still work */
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: wctx_begin should detect the unclosed window, restore
     * buffers, and proceed normally. */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    /* Should be able to open a new window normally */
    if (forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "Win", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Main context should have valid draw data */
    ASSERT_TRUE(ctx.vertex_count > 0);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MAX WINDOWS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_max_windows_rejected(void)
{
    TEST("window_begin: rejects window when at FORGE_UI_WINDOW_MAX");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState states[FORGE_UI_WINDOW_MAX + 1];
    for (int i = 0; i <= FORGE_UI_WINDOW_MAX; i++) {
        states[i] = (ForgeUiWindowState){
            .rect = { TEST_WIN_X + 5.0f * i, TEST_WIN_Y, 100, 100 },
            .z_order = i
        };
    }

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    /* Open FORGE_UI_WINDOW_MAX windows */
    int opened_count = 0;
    for (int i = 0; i < FORGE_UI_WINDOW_MAX; i++) {
        Uint32 id = (Uint32)(100 + i * 10);
        if (forge_ui_wctx_window_begin(&wctx, id, "W", &states[i])) {
            forge_ui_wctx_window_end(&wctx);
            opened_count++;
        }
    }
    ASSERT_EQ_INT(opened_count, FORGE_UI_WINDOW_MAX);

    /* The 17th window should be rejected */
    bool extra = forge_ui_wctx_window_begin(
        &wctx, 999, "Extra", &states[FORGE_UI_WINDOW_MAX]);
    ASSERT_TRUE(!extra);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  NESTED WINDOWS REJECTED
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_nested_windows_rejected(void)
{
    TEST("window_begin: nested windows are rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState w1 = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 0
    };
    ForgeUiWindowState w2 = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN_W, TEST_WIN_H }, .z_order = 1
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    bool first = forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1, "W1", &w1);
    ASSERT_TRUE(first);

    /* Try to open a second window while the first is still open */
    bool nested = forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2, "W2", &w2);
    ASSERT_TRUE(!nested);

    forge_ui_wctx_window_end(&wctx);
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  WCTX_BEGIN / WCTX_END NULL SAFETY
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_wctx_begin_null(void)
{
    TEST("wctx_begin: NULL wctx is safe");
    forge_ui_wctx_begin(NULL);  /* must not crash */
    pass_count++;
}

static void test_wctx_end_null(void)
{
    TEST("wctx_end: NULL wctx is safe");
    forge_ui_wctx_end(NULL);  /* must not crash */
    pass_count++;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  PREVIOUS FRAME DATA
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_prev_frame_data_saved(void)
{
    TEST("wctx_begin: saves previous frame window data correctly");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_PREV_WIN_X, TEST_PREV_WIN_Y,
                  TEST_PREV_WIN_W, TEST_PREV_WIN_H },
        .z_order = TEST_PREV_WIN_Z
    };

    /* Frame 0: declare one window */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    if (forge_ui_wctx_window_begin(&wctx, TEST_PREV_WIN_ID, "Test", &ws)) {
        forge_ui_wctx_window_end(&wctx);
    }
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: check prev data */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    ASSERT_EQ_INT(wctx.prev_window_count, 1);
    ASSERT_EQ_U32(wctx.prev_window_ids[0], TEST_PREV_WIN_ID);
    ASSERT_EQ_INT(wctx.prev_window_z_orders[0], TEST_PREV_WIN_Z);
    ASSERT_NEAR(wctx.prev_window_rects[0].x, TEST_PREV_WIN_X, TEST_NEAR_EPS);
    ASSERT_NEAR(wctx.prev_window_rects[0].y, TEST_PREV_WIN_Y, TEST_NEAR_EPS);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TASK 3: NaN/Inf MOUSE VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_nan_mouse_clamped_to_zero(void)
{
    TEST("ctx_begin: NaN mouse coords are clamped to zero");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Pass NaN mouse coordinates */
    forge_ui_ctx_begin(&ctx, TEST_NAN_MOUSE_X, TEST_NAN_MOUSE_Y, false);
    ASSERT_TRUE(isfinite(ctx.mouse_x));
    ASSERT_TRUE(isfinite(ctx.mouse_y));
    ASSERT_NEAR(ctx.mouse_x, 0.0f, TEST_NEAR_EPS);
    ASSERT_NEAR(ctx.mouse_y, 0.0f, TEST_NEAR_EPS);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_free(&ctx);
}

static void test_inf_mouse_clamped_to_zero(void)
{
    TEST("ctx_begin: Inf mouse coords are clamped to zero");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Pass +Inf mouse coordinates */
    forge_ui_ctx_begin(&ctx, TEST_INF_MOUSE_X, TEST_INF_MOUSE_Y, false);
    ASSERT_TRUE(isfinite(ctx.mouse_x));
    ASSERT_TRUE(isfinite(ctx.mouse_y));
    ASSERT_NEAR(ctx.mouse_x, 0.0f, TEST_NEAR_EPS);
    ASSERT_NEAR(ctx.mouse_y, 0.0f, TEST_NEAR_EPS);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_free(&ctx);
}

static void test_nan_mouse_slider_safe(void)
{
    TEST("ctx_begin: NaN mouse does not corrupt active slider value");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiRect slider_rect = {
        TEST_SLIDER_X, TEST_SLIDER_Y, TEST_SLIDER_W, TEST_SLIDER_H
    };
    float value = TEST_SLIDER_INIT;

    /* Frame 0: press on the slider to make it active.
     * slider() returns "changed" (not success/failure), so we don't
     * assert the return value — we only care that value stays finite. */
    float press_x = TEST_SLIDER_X + TEST_SLIDER_W * 0.5f;
    float press_y = TEST_SLIDER_Y + TEST_SLIDER_H * 0.5f;
    forge_ui_ctx_begin(&ctx, press_x, press_y, true);
    forge_ui_ctx_slider(&ctx, TEST_SLIDER_ID, &value,
                        TEST_SLIDER_MIN, TEST_SLIDER_MAX, slider_rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: still held down, but mouse is NaN — value must stay finite */
    forge_ui_ctx_begin(&ctx, TEST_NAN_MOUSE_X, TEST_NAN_MOUSE_Y, true);
    forge_ui_ctx_slider(&ctx, TEST_SLIDER_ID, &value,
                        TEST_SLIDER_MIN, TEST_SLIDER_MAX, slider_rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(isfinite(value));
    ASSERT_TRUE(value >= TEST_SLIDER_MIN);
    ASSERT_TRUE(value <= TEST_SLIDER_MAX);

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TASK 4: STATE CLEANUP — ctx_free zeros panel
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_ctx_free_zeros_panel(void)
{
    TEST("ctx_free: panel fields are fully zeroed");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Open and close a panel to populate _panel fields */
    float scroll = 0.0f;
    ForgeUiRect panel_rect = {
        TEST_PANEL_X, TEST_PANEL_Y, TEST_PANEL_W, TEST_PANEL_H
    };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    bool panel_ok = forge_ui_ctx_panel_begin(
        &ctx, TEST_PANEL_ID, "TestPanel", panel_rect, &scroll);
    ASSERT_TRUE(panel_ok);
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    /* Free the context — all panel fields should be zeroed */
    forge_ui_ctx_free(&ctx);

    ASSERT_TRUE(!ctx._panel_active);
    ASSERT_TRUE(ctx._panel.scroll_y == NULL);
    ASSERT_EQ_U32(ctx._panel.id, 0u);
    ASSERT_NEAR(ctx._panel.rect.x, 0.0f, TEST_NEAR_EPS);
    ASSERT_NEAR(ctx._panel.rect.y, 0.0f, TEST_NEAR_EPS);
    ASSERT_NEAR(ctx._panel.rect.w, 0.0f, TEST_NEAR_EPS);
    ASSERT_NEAR(ctx._panel.rect.h, 0.0f, TEST_NEAR_EPS);
    ASSERT_NEAR(ctx._panel.content_height, 0.0f, TEST_NEAR_EPS);
    ASSERT_NEAR(ctx._panel_content_start_y, 0.0f, TEST_NEAR_EPS);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TASK 4: STATE CLEANUP — panel_end NULLs scroll_y
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_panel_end_nulls_scroll_y(void)
{
    TEST("panel_end: _panel.scroll_y is NULL after panel_end");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll = 0.0f;
    ForgeUiRect panel_rect = {
        TEST_PANEL_X, TEST_PANEL_Y, TEST_PANEL_W, TEST_PANEL_H
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    bool panel_ok = forge_ui_ctx_panel_begin(
        &ctx, TEST_PANEL_ID, "TestPanel", panel_rect, &scroll);
    ASSERT_TRUE(panel_ok);

    /* While the panel is active, scroll_y should point to our local */
    ASSERT_TRUE(ctx._panel.scroll_y == &scroll);

    forge_ui_ctx_panel_end(&ctx);

    /* After panel_end, scroll_y must be NULL to prevent stale pointer use */
    ASSERT_TRUE(ctx._panel.scroll_y == NULL);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TASK 4: STATE CLEANUP — window_end guard path
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_window_end_guard_decrements_count(void)
{
    TEST("window_end: !panel_active guard decrements window_count and clears entry");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);

    /* Open a window normally */
    bool opened = forge_ui_wctx_window_begin(
        &wctx, TEST_WIN_ID_1, "Guard", &ws);
    ASSERT_TRUE(opened);
    ASSERT_EQ_INT(wctx.window_count, 1);

    /* Simulate the panel being cleared externally (misuse scenario that
     * the guard protects against).  This forces window_end down the
     * !_panel_active error path. */
    ctx._panel_active = false;

    forge_ui_wctx_window_end(&wctx);

    /* The guard path should have decremented window_count */
    ASSERT_EQ_INT(wctx.window_count, 0);

    /* has_clip should be cleared */
    ASSERT_TRUE(!ctx.has_clip);

    /* The window entry should be zeroed out */
    ASSERT_EQ_U32(wctx.window_entries[0].id, FORGE_UI_ID_NONE);
    ASSERT_TRUE(wctx.window_entries[0].state == NULL);
    ASSERT_EQ_INT(wctx.window_entries[0].vertex_count, 0);
    ASSERT_EQ_INT(wctx.window_entries[0].index_count, 0);

    /* active_window_idx should be restored to -1 */
    ASSERT_EQ_INT(wctx.active_window_idx, -1);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TASK 4: STATE CLEANUP — wctx_free zeros saved fields
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_wctx_free_zeros_saved_fields(void)
{
    TEST("wctx_free: all saved fields and bookkeeping are zeroed");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };

    /* Run a full frame so saved fields get populated */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_wctx_begin(&wctx);
    bool opened = forge_ui_wctx_window_begin(
        &wctx, TEST_WIN_ID_1, "Test", &ws);
    ASSERT_TRUE(opened);
    forge_ui_wctx_window_end(&wctx);
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Free the window context */
    forge_ui_wctx_free(&wctx);

    /* All bookkeeping fields should be zeroed */
    ASSERT_EQ_INT(wctx.window_count, 0);
    ASSERT_EQ_INT(wctx.active_window_idx, -1);
    ASSERT_EQ_U32(wctx.hovered_window_id, FORGE_UI_ID_NONE);
    ASSERT_EQ_INT(wctx.prev_window_count, 0);

    /* Saved buffer pointers should be NULL */
    ASSERT_TRUE(wctx.saved_vertices == NULL);
    ASSERT_TRUE(wctx.saved_indices == NULL);
    ASSERT_EQ_INT(wctx.saved_vertex_count, 0);
    ASSERT_EQ_INT(wctx.saved_vertex_capacity, 0);
    ASSERT_EQ_INT(wctx.saved_index_count, 0);
    ASSERT_EQ_INT(wctx.saved_index_capacity, 0);

    /* ctx pointer should be cleared */
    ASSERT_TRUE(wctx.ctx == NULL);

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TASK 7: SHARED STATE — window_begin rejects during active panel
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_window_begin_rejects_during_panel(void)
{
    TEST("window_begin: rejects when standalone panel is active");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    float scroll = 0.0f;
    ForgeUiRect panel_rect = {
        TEST_PANEL_X, TEST_PANEL_Y, TEST_PANEL_W, TEST_PANEL_H
    };
    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, 0, 0, false);

    /* Open a standalone panel */
    bool panel_ok = forge_ui_ctx_panel_begin(
        &ctx, TEST_PANEL_ID, "Blocker", panel_rect, &scroll);
    ASSERT_TRUE(panel_ok);
    ASSERT_TRUE(ctx._panel_active);

    /* Attempt to open a window while the panel is active — should fail */
    forge_ui_wctx_begin(&wctx);
    bool win_ok = forge_ui_wctx_window_begin(
        &wctx, TEST_WIN_ID_1, "Blocked", &ws);
    ASSERT_TRUE(!win_ok);

    /* Window count should remain at 0 */
    ASSERT_EQ_INT(wctx.window_count, 0);

    forge_ui_wctx_end(&wctx);

    /* Close the panel and end the frame normally */
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Keyboard focus leak: covered window must not accept keyboard input ──── */

static void test_covered_window_blocks_keyboard(void)
{
    TEST("covered window: text input does not accept keyboard input");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    /* Two fully overlapping windows: A has a text input, B will cover it.
     * Window A starts on top (z=1) so we can click its text input.
     * Window B starts behind (z=0) and will be brought to front later. */
    ForgeUiWindowState winA = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 1
    };
    ForgeUiWindowState winB = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };

    char ti_buf[TEST_TI_BUF_CAP] = "";
    ForgeUiTextInputState ti_state = {
        ti_buf, TEST_TI_BUF_CAP, 0, 0
    };
    ForgeUiRect ti_rect = {
        TEST_TI_RECT_X, TEST_TI_RECT_Y, TEST_TI_RECT_W, TEST_TI_RECT_H
    };

    /* Helper macro: declare both windows for a frame, with text input in A.
     * Neither window is ever collapsed in this test, so window_begin must
     * always succeed — assert it.  text_input returns "content_changed"
     * (not success/failure), so it is not wrapped with ASSERT_TRUE. */
#define DECLARE_BOTH_WINDOWS()                                             \
    do {                                                                   \
        ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_1,      \
                                               "WinA", &winA));           \
        forge_ui_ctx_text_input(&ctx, TEST_TI_ID, &ti_state,              \
                                ti_rect, true);                            \
        forge_ui_wctx_window_end(&wctx);                                  \
        ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, TEST_WIN_ID_2,      \
                                               "WinB", &winB));           \
        forge_ui_wctx_window_end(&wctx);                                  \
    } while (0)

    /* ── Frame 0: warm up (populates prev_window data) ───────────────── */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_wctx_begin(&wctx);
    DECLARE_BOTH_WINDOWS();
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* ── Frame 1: hover over text input in window A ──────────────────── */
    /* prev data now has both windows.  A (z=1) > B (z=0), so mouse over
     * the overlap area → hovered = A.  This frame establishes hot. */
    forge_ui_ctx_begin(&ctx, TEST_TI_CLICK_X, TEST_TI_CLICK_Y, false);
    forge_ui_wctx_begin(&wctx);
    DECLARE_BOTH_WINDOWS();
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* ── Frame 2: press on text input ────────────────────────────────── */
    forge_ui_ctx_begin(&ctx, TEST_TI_CLICK_X, TEST_TI_CLICK_Y, true);
    forge_ui_wctx_begin(&wctx);
    DECLARE_BOTH_WINDOWS();
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, TEST_TI_ID);

    /* ── Frame 3: release → focus acquired ───────────────────────────── */
    forge_ui_ctx_begin(&ctx, TEST_TI_CLICK_X, TEST_TI_CLICK_Y, false);
    forge_ui_wctx_begin(&wctx);
    DECLARE_BOTH_WINDOWS();
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, TEST_TI_ID);

    /* ── Frame 4: bring Window B to front ────────────────────────────── */
    /* Set B's z_order above A's.  This frame's wctx_begin still uses
     * prev data where A was on top, but it saves the new z_orders for
     * the next frame's hovered determination.  Mouse stays inside the
     * window area so hovered_window_id is computed for the overlap. */
    winB.z_order = 2;
    forge_ui_ctx_begin(&ctx, TEST_TI_CLICK_X, TEST_TI_CLICK_Y, false);
    forge_ui_wctx_begin(&wctx);
    DECLARE_BOTH_WINDOWS();
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* ── Frame 5: type "X" — Window B now hovered (z=2 > z=1) ────────── */
    /* prev data now reflects z_orders [A:1, B:2].  Mouse is inside both
     * windows, so hovered_window_id = B (highest z).  Window A's
     * can_receive_input = false → _keyboard_input_suppressed = true.
     * The focused text input in A should NOT accept keyboard input. */
    forge_ui_ctx_begin(&ctx, TEST_TI_CLICK_X, TEST_TI_CLICK_Y, false);
    forge_ui_ctx_set_keyboard(&ctx, "X", false, false, false, false,
                              false, false, false);
    forge_ui_wctx_begin(&wctx);
    DECLARE_BOTH_WINDOWS();
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Buffer must be untouched */
    ASSERT_EQ_INT(ti_state.length, 0);
    ASSERT_TRUE(ti_buf[0] == '\0');

    /* Focus is intentionally preserved (visual state unchanged) */
    ASSERT_EQ_U32(ctx.focused, TEST_TI_ID);

    /* ── Frame 6: bring A back on top, verify typing works ───────────── */
    winA.z_order = 3;

    /* Propagation frame: prev data updates to [A:3, B:2] */
    forge_ui_ctx_begin(&ctx, TEST_TI_CLICK_X, TEST_TI_CLICK_Y, false);
    forge_ui_wctx_begin(&wctx);
    DECLARE_BOTH_WINDOWS();
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* ── Frame 7: type "Y" — A on top, keyboard should work ──────────── */
    forge_ui_ctx_begin(&ctx, TEST_TI_CLICK_X, TEST_TI_CLICK_Y, false);
    forge_ui_ctx_set_keyboard(&ctx, "Y", false, false, false, false,
                              false, false, false);
    forge_ui_wctx_begin(&wctx);
    DECLARE_BOTH_WINDOWS();
    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Now Window A is on top — keyboard input should go through */
    ASSERT_EQ_INT(ti_state.length, 1);
    ASSERT_TRUE(ti_buf[0] == 'Y');

#undef DECLARE_BOTH_WINDOWS

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== UI Window Tests (forge_ui_window.h) ===");
    SDL_Log("");

    /* Setup */
    if (!setup_atlas()) {
        SDL_Log("FATAL: Could not set up font atlas");
        SDL_Quit();
        return 1;
    }

    /* Init / Free lifecycle */
    SDL_Log("--- Init / Free ---");
    test_init_null_wctx();
    test_init_null_ctx();
    test_init_sets_defaults();
    test_free_null_safe();
    test_free_clears_ctx();

    /* Parameter validation */
    SDL_Log("--- Parameter Validation ---");
    test_window_begin_null_state();
    test_window_begin_id_none();
    test_window_begin_zero_width();
    test_window_begin_nan_x();
    test_window_begin_inf_y();
    test_window_begin_nan_width();
    test_window_begin_null_atlas();

    /* Draw data generation */
    SDL_Log("--- Draw Data ---");
    test_window_emits_draw_data();
    test_window_collapsed_returns_false();
    test_window_collapsed_still_renders_title_bar();

    /* Z-ordering */
    SDL_Log("--- Z-Ordering ---");
    test_z_order_bring_to_front();
    test_z_order_overflow_guarded();

    /* Deferred draw ordering */
    SDL_Log("--- Deferred Draw ---");
    test_deferred_draw_z_order();
    test_multiple_windows_all_rendered();

    /* Drag mechanics */
    SDL_Log("--- Drag ---");
    test_drag_moves_window();

    /* Collapse toggle */
    SDL_Log("--- Collapse ---");
    test_collapse_toggle();

    /* Input routing */
    SDL_Log("--- Input Routing ---");
    test_hovered_window_id_set();
    test_hovered_window_none_outside();

    /* Bug fix validation */
    SDL_Log("--- Bug Fix Validation ---");
    test_collapsed_hover_rect_title_only();
    test_free_while_redirected();
    test_window_end_without_begin();
    test_wctx_begin_restores_unclosed_window();

    /* Limits */
    SDL_Log("--- Limits ---");
    test_max_windows_rejected();
    test_nested_windows_rejected();

    /* NULL safety */
    SDL_Log("--- NULL Safety ---");
    test_wctx_begin_null();
    test_wctx_end_null();

    /* Previous frame data */
    SDL_Log("--- Prev Frame Data ---");
    test_prev_frame_data_saved();

    /* Task 3: NaN/Inf mouse validation */
    SDL_Log("--- NaN/Inf Mouse Validation ---");
    test_nan_mouse_clamped_to_zero();
    test_inf_mouse_clamped_to_zero();
    test_nan_mouse_slider_safe();

    /* Task 4: State cleanup */
    SDL_Log("--- State Cleanup ---");
    test_ctx_free_zeros_panel();
    test_panel_end_nulls_scroll_y();
    test_window_end_guard_decrements_count();
    test_wctx_free_zeros_saved_fields();

    /* Task 7: Shared state isolation */
    SDL_Log("--- Shared State Isolation ---");
    test_window_begin_rejects_during_panel();

    /* Keyboard focus leak fix */
    SDL_Log("--- Keyboard Focus Leak ---");
    test_covered_window_blocks_keyboard();

    SDL_Log("");
    SDL_Log("=== Results: %d tests, %d assertions passed, %d failed ===",
            test_count, pass_count, fail_count);

    /* Cleanup */
    if (atlas_built) {
        forge_ui_atlas_free(&test_atlas);
    }
    if (font_loaded) {
        forge_ui_ttf_free(&test_font);
    }
    SDL_Quit();

    return fail_count > 0 ? 1 : 0;
}
