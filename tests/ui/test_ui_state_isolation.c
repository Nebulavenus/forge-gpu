/*
 * UI State Isolation Tests
 *
 * Automated tests verifying that separate windows, panels, and widget groups
 * do not unexpectedly share state.  Covers:
 *
 *   - Two sequential panels with same-label widgets get different IDs
 *   - Two sequential windows with same-label widgets get different IDs
 *   - After window_end, the ID stack depth returns to pre-window_begin
 *   - After panel_end, the layout stack depth returns to pre-panel_begin
 *   - Pre-clamp cross-contamination between same-title sequential panels
 *   - Layout stack isolation between sequential panels
 *   - Clip rect isolation between sequential panels
 *   - Panel scroll_y pointer isolation between sequential panels
 *   - Window draw list redirect restored between sequential windows
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

#define ASSERT_NEQ_U32(a, b)                                      \
    do {                                                          \
        Uint32 _a = (a), _b = (b);                                \
        if (_a == _b) {                                           \
            SDL_Log("    FAIL: %s == %u, should differ (line %d)",\
                    #a, _a, __LINE__);                            \
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

/* Panel geometry */
#define TEST_PANEL_X      10.0f
#define TEST_PANEL_Y      10.0f
#define TEST_PANEL_W      200.0f
#define TEST_PANEL_H      200.0f

/* Second panel (non-overlapping) */
#define TEST_PANEL2_X     220.0f
#define TEST_PANEL2_Y     10.0f
#define TEST_PANEL2_W     200.0f
#define TEST_PANEL2_H     200.0f

/* Window geometry */
#define TEST_WIN_X        10.0f
#define TEST_WIN_Y        10.0f
#define TEST_WIN_W        200.0f
#define TEST_WIN_H        200.0f

/* Second window (non-overlapping) */
#define TEST_WIN2_X       220.0f
#define TEST_WIN2_Y       10.0f
#define TEST_WIN2_W       200.0f
#define TEST_WIN2_H       200.0f

/* Mouse position outside all windows/panels */
#define TEST_MOUSE_FAR    500.0f

/* Widget layout height */
#define TEST_WIDGET_H     30.0f

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

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Sequential panels with same-label widgets
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_sequential_panels_different_widget_ids(void)
{
    TEST("sequential panels: same-label widgets get different IDs");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll_a = 0.0f;
    float scroll_b = 0.0f;
    ForgeUiRect rect_a = { TEST_PANEL_X, TEST_PANEL_Y,
                           TEST_PANEL_W, TEST_PANEL_H };
    ForgeUiRect rect_b = { TEST_PANEL2_X, TEST_PANEL2_Y,
                           TEST_PANEL2_W, TEST_PANEL2_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    /* Panel A: "Settings" */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Settings", rect_a, &scroll_a));
    Uint32 id_ok_in_a = forge_ui_hash_id(&ctx, "OK");
    Uint32 id_cancel_in_a = forge_ui_hash_id(&ctx, "Cancel");
    forge_ui_ctx_panel_end(&ctx);

    /* Panel B: "Preferences" */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Preferences", rect_b, &scroll_b));
    Uint32 id_ok_in_b = forge_ui_hash_id(&ctx, "OK");
    Uint32 id_cancel_in_b = forge_ui_hash_id(&ctx, "Cancel");
    forge_ui_ctx_panel_end(&ctx);

    forge_ui_ctx_end(&ctx);

    /* Same label in different panel scopes must produce different IDs */
    ASSERT_NEQ_U32(id_ok_in_a, id_ok_in_b);
    ASSERT_NEQ_U32(id_cancel_in_a, id_cancel_in_b);

    /* IDs within the same panel must also differ from each other */
    ASSERT_NEQ_U32(id_ok_in_a, id_cancel_in_a);
    ASSERT_NEQ_U32(id_ok_in_b, id_cancel_in_b);

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Sequential windows with same-label widgets
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_sequential_windows_different_widget_ids(void)
{
    TEST("sequential windows: same-label widgets get different IDs");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws_a = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };
    ForgeUiWindowState ws_b = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN2_W, TEST_WIN2_H },
        .z_order = 1
    };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_wctx_begin(&wctx);

    /* Window A: "Audio" */
    ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, "Audio", &ws_a));
    Uint32 id_enable_in_a = forge_ui_hash_id(&ctx, "Enable");
    Uint32 id_volume_in_a = forge_ui_hash_id(&ctx, "Volume");
    forge_ui_wctx_window_end(&wctx);

    /* Window B: "Video" */
    ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, "Video", &ws_b));
    Uint32 id_enable_in_b = forge_ui_hash_id(&ctx, "Enable");
    Uint32 id_volume_in_b = forge_ui_hash_id(&ctx, "Volume");
    forge_ui_wctx_window_end(&wctx);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* Same label in different window scopes must produce different IDs */
    ASSERT_NEQ_U32(id_enable_in_a, id_enable_in_b);
    ASSERT_NEQ_U32(id_volume_in_a, id_volume_in_b);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: ID stack depth restored after window_end
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_window_end_restores_id_stack_depth(void)
{
    TEST("window_end: ID stack depth returns to pre-window_begin state");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_wctx_begin(&wctx);

    int depth_before = ctx.id_stack_depth;
    ASSERT_EQ_INT(depth_before, 0);

    /* Open and close a window */
    ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, "TestWin", &ws));

    /* While inside, depth should be greater */
    ASSERT_TRUE(ctx.id_stack_depth > depth_before);

    forge_ui_wctx_window_end(&wctx);

    /* After window_end, depth must return to what it was before */
    ASSERT_EQ_INT(ctx.id_stack_depth, depth_before);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: ID stack depth restored after collapsed window
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_collapsed_window_restores_id_stack_depth(void)
{
    TEST("collapsed window: ID stack depth returns to pre-window_begin state");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0,
        .collapsed = true
    };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_wctx_begin(&wctx);

    int depth_before = ctx.id_stack_depth;
    ASSERT_EQ_INT(depth_before, 0);

    /* Open a collapsed window -- returns false, no window_end needed */
    bool expanded = forge_ui_wctx_window_begin(&wctx, "Collapsed", &ws);
    ASSERT_TRUE(!expanded);

    /* Depth must be restored even though the window was collapsed */
    ASSERT_EQ_INT(ctx.id_stack_depth, depth_before);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Layout stack depth restored after panel_end
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_panel_end_restores_layout_depth(void)
{
    TEST("panel_end: layout stack depth returns to pre-panel_begin state");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll = 0.0f;
    ForgeUiRect rect = { TEST_PANEL_X, TEST_PANEL_Y,
                         TEST_PANEL_W, TEST_PANEL_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    int depth_before = ctx.layout_depth;
    ASSERT_EQ_INT(depth_before, 0);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Panel", rect, &scroll));

    /* While inside, layout depth should be greater */
    ASSERT_TRUE(ctx.layout_depth > depth_before);

    forge_ui_ctx_panel_end(&ctx);

    /* After panel_end, layout depth must return to what it was before */
    ASSERT_EQ_INT(ctx.layout_depth, depth_before);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: ID stack depth restored after panel_end
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_panel_end_restores_id_stack_depth(void)
{
    TEST("panel_end: ID stack depth returns to pre-panel_begin state");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll = 0.0f;
    ForgeUiRect rect = { TEST_PANEL_X, TEST_PANEL_Y,
                         TEST_PANEL_W, TEST_PANEL_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    int id_depth_before = ctx.id_stack_depth;
    ASSERT_EQ_INT(id_depth_before, 0);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Panel", rect, &scroll));

    /* While inside, ID stack depth should be greater */
    ASSERT_TRUE(ctx.id_stack_depth > id_depth_before);

    forge_ui_ctx_panel_end(&ctx);

    /* After panel_end, ID stack depth must return to what it was before */
    ASSERT_EQ_INT(ctx.id_stack_depth, id_depth_before);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Window scope seed starts clean from root
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_second_window_scope_starts_from_root(void)
{
    TEST("second window scope seed is derived from root, not first window");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws_a = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };
    ForgeUiWindowState ws_b = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN2_W, TEST_WIN2_H },
        .z_order = 1
    };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_wctx_begin(&wctx);

    /* Compute expected ID seeds manually */
    Uint32 root_seed = FORGE_UI_FNV_OFFSET_BASIS;
    Uint32 expected_seed_a = forge_ui__fnv1a("WinA", root_seed);
    Uint32 expected_seed_b = forge_ui__fnv1a("WinB", root_seed);

    /* Window A */
    ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, "WinA", &ws_a));
    /* Verify the scope seed is derived from root + "WinA" */
    ASSERT_TRUE(ctx.id_stack_depth == 1);
    ASSERT_EQ_U32(ctx.id_seed_stack[0], expected_seed_a);
    forge_ui_wctx_window_end(&wctx);

    /* After window A, stack depth must be back to 0 */
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);

    /* Window B */
    ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, "WinB", &ws_b));
    /* Verify the scope seed is derived from root + "WinB", not from WinA */
    ASSERT_TRUE(ctx.id_stack_depth == 1);
    ASSERT_EQ_U32(ctx.id_seed_stack[0], expected_seed_b);
    forge_ui_wctx_window_end(&wctx);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    /* The two scope seeds must differ */
    ASSERT_NEQ_U32(expected_seed_a, expected_seed_b);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Clip rect cleared between sequential panels
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_clip_rect_cleared_between_panels(void)
{
    TEST("sequential panels: clip rect cleared between panels");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll_a = 0.0f;
    float scroll_b = 0.0f;
    ForgeUiRect rect_a = { TEST_PANEL_X, TEST_PANEL_Y,
                           TEST_PANEL_W, TEST_PANEL_H };
    ForgeUiRect rect_b = { TEST_PANEL2_X, TEST_PANEL2_Y,
                           TEST_PANEL2_W, TEST_PANEL2_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    /* Panel A */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "PanelA", rect_a, &scroll_a));
    ASSERT_TRUE(ctx.has_clip);
    forge_ui_ctx_panel_end(&ctx);

    /* After panel A, clip rect must be cleared */
    ASSERT_TRUE(!ctx.has_clip);

    /* Panel B */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "PanelB", rect_b, &scroll_b));
    ASSERT_TRUE(ctx.has_clip);
    /* Verify clip rect matches panel B's content area, not panel A's */
    float expected_clip_x = TEST_PANEL2_X + FORGE_UI_PANEL_PADDING;
    ASSERT_NEAR(ctx.clip_rect.x, expected_clip_x, 0.01f);
    forge_ui_ctx_panel_end(&ctx);

    ASSERT_TRUE(!ctx.has_clip);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Panel scroll_y pointer isolation
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_panel_scroll_y_pointer_isolation(void)
{
    TEST("sequential panels: scroll_y pointer scoped to each panel");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll_a = 42.0f;
    float scroll_b = 99.0f;
    ForgeUiRect rect_a = { TEST_PANEL_X, TEST_PANEL_Y,
                           TEST_PANEL_W, TEST_PANEL_H };
    ForgeUiRect rect_b = { TEST_PANEL2_X, TEST_PANEL2_Y,
                           TEST_PANEL2_W, TEST_PANEL2_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    /* Panel A -- scroll_a is stored in _panel */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "PanA", rect_a, &scroll_a));
    ASSERT_TRUE(ctx._panel.scroll_y == &scroll_a);
    forge_ui_ctx_panel_end(&ctx);

    /* After panel_end, _panel.scroll_y must be NULL */
    ASSERT_TRUE(ctx._panel.scroll_y == NULL);

    /* Panel B -- scroll_b is stored in _panel */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "PanB", rect_b, &scroll_b));
    ASSERT_TRUE(ctx._panel.scroll_y == &scroll_b);
    /* Verify it is NOT pointing to scroll_a */
    ASSERT_TRUE(ctx._panel.scroll_y != &scroll_a);
    forge_ui_ctx_panel_end(&ctx);

    ASSERT_TRUE(ctx._panel.scroll_y == NULL);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: _panel_active cleared between panels
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_panel_active_cleared_between_panels(void)
{
    TEST("sequential panels: _panel_active properly toggled");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll_a = 0.0f;
    float scroll_b = 0.0f;
    ForgeUiRect rect_a = { TEST_PANEL_X, TEST_PANEL_Y,
                           TEST_PANEL_W, TEST_PANEL_H };
    ForgeUiRect rect_b = { TEST_PANEL2_X, TEST_PANEL2_Y,
                           TEST_PANEL2_W, TEST_PANEL2_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    ASSERT_TRUE(!ctx._panel_active);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "First", rect_a, &scroll_a));
    ASSERT_TRUE(ctx._panel_active);
    forge_ui_ctx_panel_end(&ctx);
    ASSERT_TRUE(!ctx._panel_active);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Second", rect_b, &scroll_b));
    ASSERT_TRUE(ctx._panel_active);
    forge_ui_ctx_panel_end(&ctx);
    ASSERT_TRUE(!ctx._panel_active);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Window draw list redirect properly restored
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_window_draw_redirect_restored(void)
{
    TEST("sequential windows: draw list redirect properly restored");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws_a = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };
    ForgeUiWindowState ws_b = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN2_W, TEST_WIN2_H },
        .z_order = 1
    };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_wctx_begin(&wctx);

    /* Save main buffer pointer before any window */
    ForgeUiVertex *main_verts = ctx.vertices;
    Uint32 *main_indices = ctx.indices;

    /* Window A */
    ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, "WinA", &ws_a));
    /* While inside window A, ctx buffers point to per-window list */
    ASSERT_TRUE(ctx.vertices != main_verts);
    forge_ui_wctx_window_end(&wctx);

    /* After window A, main buffers must be restored */
    ASSERT_TRUE(ctx.vertices == main_verts);
    ASSERT_TRUE(ctx.indices == main_indices);
    ASSERT_EQ_INT(wctx.active_window_idx, -1);

    /* Window B */
    ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, "WinB", &ws_b));
    /* While inside window B, ctx buffers point to a different per-window list */
    ASSERT_TRUE(ctx.vertices != main_verts);
    forge_ui_wctx_window_end(&wctx);

    /* After window B, main buffers must be restored again */
    ASSERT_TRUE(ctx.vertices == main_verts);
    ASSERT_TRUE(ctx.indices == main_indices);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Layout stack at depth 0 between sequential panels
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_layout_depth_zero_between_panels(void)
{
    TEST("sequential panels: layout depth is 0 between panels");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll_a = 0.0f;
    float scroll_b = 0.0f;
    ForgeUiRect rect_a = { TEST_PANEL_X, TEST_PANEL_Y,
                           TEST_PANEL_W, TEST_PANEL_H };
    ForgeUiRect rect_b = { TEST_PANEL2_X, TEST_PANEL2_Y,
                           TEST_PANEL2_W, TEST_PANEL2_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    ASSERT_EQ_INT(ctx.layout_depth, 0);

    /* Panel A */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "PanelA", rect_a, &scroll_a));
    ASSERT_EQ_INT(ctx.layout_depth, 1);

    /* Place some widgets to advance the cursor */
    forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);

    forge_ui_ctx_panel_end(&ctx);
    ASSERT_EQ_INT(ctx.layout_depth, 0);

    /* Panel B -- must start at depth 0, not influenced by panel A */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "PanelB", rect_b, &scroll_b));
    ASSERT_EQ_INT(ctx.layout_depth, 1);

    /* Verify the layout cursor starts at panel B's content area, not
     * panel A's leftover cursor position */
    float expected_cursor_y = TEST_PANEL2_Y + FORGE_UI_PANEL_TITLE_HEIGHT
                              + FORGE_UI_PANEL_PADDING;
    ASSERT_NEAR(ctx.layout_stack[0].cursor_y, expected_cursor_y, 0.01f);

    forge_ui_ctx_panel_end(&ctx);
    ASSERT_EQ_INT(ctx.layout_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BUG TEST: Pre-clamp cross-contamination between same-title panels
 *
 *  When two sequential panels share the same title (and thus the same
 *  hashed ID), panel B's panel_begin reads panel A's content_height
 *  for pre-clamping, which is the wrong value.
 *
 *  This test demonstrates the issue: Panel A has tall content, Panel B
 *  has short content.  Panel B's scroll_y should remain at 0, but the
 *  pre-clamp using Panel A's content_height may allow a non-zero value.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_same_title_panels_preclamp_contamination(void)
{
    TEST("same-title panels: pre-clamp uses correct content_height");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Both panels use the same title "Info" */
    float scroll_a = 0.0f;
    float scroll_b = 0.0f;
    ForgeUiRect rect_a = { TEST_PANEL_X, TEST_PANEL_Y,
                           TEST_PANEL_W, TEST_PANEL_H };
    ForgeUiRect rect_b = { TEST_PANEL2_X, TEST_PANEL2_Y,
                           TEST_PANEL2_W, TEST_PANEL2_H };

    /* ── Frame 1: establish content_height for both panels ─────────────── */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    /* Panel A ("Info") with TALL content: 10 widgets * 30px = 300px */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Info", rect_a, &scroll_a));
    for (int i = 0; i < 10; i++) {
        forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    }
    forge_ui_ctx_panel_end(&ctx);
    /* Panel A stored content_height ~= 300 + spacing in _panel */

    /* Panel B ("Info") with SHORT content: 1 widget * 30px = 30px
     * Because it has the same title as panel A, panel_begin will read
     * panel A's content_height for the pre-clamp.  This is the bug
     * being documented. */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Info", rect_b, &scroll_b));
    forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    forge_ui_ctx_panel_end(&ctx);

    forge_ui_ctx_end(&ctx);

    /* ── Frame 2: with stale content_height from frame 1 ───────────────── */
    /* Set scroll_b to a value that would be valid for panel A's tall
     * content but too large for panel B's short content. */
    scroll_b = 100.0f;

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    /* Panel A again -- establishes stale content_height */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Info", rect_a, &scroll_a));
    for (int i = 0; i < 10; i++) {
        forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    }
    forge_ui_ctx_panel_end(&ctx);

    /* Panel B ("Info") -- BUG: pre-clamp will use Panel A's content_height
     * because _panel.id == id (both are "Info").  The pre-clamp may allow
     * scroll_b = 100.0 to remain, even though Panel B's content is short.
     *
     * Note: panel_end's post-clamp will correct this, so after panel_end
     * the scroll_b value will be correct.  The issue only affects widget
     * positions DURING panel B's frame. */
    float scroll_b_before_panel = scroll_b;
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Info", rect_b, &scroll_b));

    /* Validate pre-clamp immediately: because _panel.id matches (both
     * panels are "Info"), the pre-clamp used Panel A's content_height
     * (~300px).  Panel A's max_scroll = 300 - 150 = 150, so scroll_b
     * (100.0) was within range and was NOT clamped.  This proves the
     * contamination — Panel B's own max_scroll would be 0. */
    ASSERT_NEAR(scroll_b, scroll_b_before_panel, 0.01f);

    forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    forge_ui_ctx_panel_end(&ctx);

    /* After panel_end, scroll_b must be correctly clamped to Panel B's
     * actual max_scroll.  Panel B's content is ~30px, visible area is
     * ~(200 - 30 - 20) = 150px, so max_scroll = max(0, 30 - 150) = 0.
     * scroll_b must be clamped to 0. */
    ASSERT_NEAR(scroll_b, 0.0f, 0.01f);

    /* Document: scroll_b was originally 100.0, which is wrong for this
     * panel.  The pre-clamp may have left it at 100 (using panel A's
     * content_height), but the post-clamp in panel_end fixed it. */
    (void)scroll_b_before_panel;

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Different-title panels skip pre-clamp
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_different_title_panels_skip_preclamp(void)
{
    TEST("different-title panels: pre-clamp correctly skipped");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll_a = 0.0f;
    float scroll_b = 50.0f;  /* intentionally large */
    ForgeUiRect rect_a = { TEST_PANEL_X, TEST_PANEL_Y,
                           TEST_PANEL_W, TEST_PANEL_H };
    ForgeUiRect rect_b = { TEST_PANEL2_X, TEST_PANEL2_Y,
                           TEST_PANEL2_W, TEST_PANEL2_H };

    /* Frame 1: establish content_height */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Alpha", rect_a, &scroll_a));
    for (int i = 0; i < 10; i++) {
        forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    }
    forge_ui_ctx_panel_end(&ctx);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Beta", rect_b, &scroll_b));
    forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    forge_ui_ctx_panel_end(&ctx);

    forge_ui_ctx_end(&ctx);

    /* Frame 2: Beta's panel_begin should NOT use Alpha's content_height
     * because their IDs differ */
    scroll_b = 50.0f;

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Alpha", rect_a, &scroll_a));
    for (int i = 0; i < 10; i++) {
        forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    }
    forge_ui_ctx_panel_end(&ctx);

    /* _panel.id is now Alpha's ID.  Beta has a different ID, so pre-clamp
     * will be skipped.  scroll_b will remain at 50 until panel_end's
     * post-clamp. */
    Uint32 alpha_id = forge_ui_hash_id(&ctx, "Alpha");
    Uint32 beta_id = forge_ui_hash_id(&ctx, "Beta");
    ASSERT_NEQ_U32(alpha_id, beta_id);
    ASSERT_EQ_U32(ctx._panel.id, alpha_id);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Beta", rect_b, &scroll_b));
    forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    forge_ui_ctx_panel_end(&ctx);

    /* Post-clamp corrects scroll_b */
    ASSERT_NEAR(scroll_b, 0.0f, 0.01f);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: keyboard_input_suppressed cleared after window
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_keyboard_suppression_cleared_between_windows(void)
{
    TEST("sequential windows: keyboard suppression cleared between windows");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiWindowContext wctx;
    ASSERT_TRUE(forge_ui_wctx_init(&wctx, &ctx));

    ForgeUiWindowState ws_a = {
        .rect = { TEST_WIN_X, TEST_WIN_Y, TEST_WIN_W, TEST_WIN_H },
        .z_order = 0
    };
    ForgeUiWindowState ws_b = {
        .rect = { TEST_WIN2_X, TEST_WIN2_Y, TEST_WIN2_W, TEST_WIN2_H },
        .z_order = 1
    };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_wctx_begin(&wctx);

    /* Window A */
    ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, "WinA", &ws_a));
    forge_ui_wctx_window_end(&wctx);

    /* After window_end, keyboard suppression must be cleared */
    ASSERT_TRUE(!ctx._keyboard_input_suppressed);

    /* Window B */
    ASSERT_TRUE(forge_ui_wctx_window_begin(&wctx, "WinB", &ws_b));
    forge_ui_wctx_window_end(&wctx);

    ASSERT_TRUE(!ctx._keyboard_input_suppressed);

    forge_ui_wctx_end(&wctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  POSITIVE VERIFICATION: Layout cursor fresh for each panel
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_layout_cursor_fresh_for_each_panel(void)
{
    TEST("sequential panels: layout cursor starts fresh for each panel");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float scroll_a = 0.0f;
    float scroll_b = 0.0f;
    ForgeUiRect rect_a = { TEST_PANEL_X, TEST_PANEL_Y,
                           TEST_PANEL_W, TEST_PANEL_H };
    ForgeUiRect rect_b = { TEST_PANEL2_X, TEST_PANEL2_Y,
                           TEST_PANEL2_W, TEST_PANEL2_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    /* Panel A -- advance cursor significantly */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "PanA", rect_a, &scroll_a));
    float cursor_a_start = ctx.layout_stack[0].cursor_y;
    for (int i = 0; i < 5; i++) {
        forge_ui_ctx_layout_next(&ctx, TEST_WIDGET_H);
    }
    float cursor_a_end = ctx.layout_stack[0].cursor_y;
    /* Cursor should have advanced */
    ASSERT_TRUE(cursor_a_end > cursor_a_start);
    forge_ui_ctx_panel_end(&ctx);

    /* Panel B -- cursor must start fresh */
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "PanB", rect_b, &scroll_b));
    float cursor_b_start = ctx.layout_stack[0].cursor_y;
    /* Panel B's cursor must start at Panel B's content area, not at
     * Panel A's advanced cursor position */
    float expected_b_start = TEST_PANEL2_Y + FORGE_UI_PANEL_TITLE_HEIGHT
                             + FORGE_UI_PANEL_PADDING;
    ASSERT_NEAR(cursor_b_start, expected_b_start, 0.01f);
    /* Must NOT equal Panel A's end cursor */
    ASSERT_TRUE(fabsf(cursor_b_start - cursor_a_end) > 1.0f);
    forge_ui_ctx_panel_end(&ctx);

    forge_ui_ctx_end(&ctx);
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

    SDL_Log("=== UI State Isolation Tests ===");
    SDL_Log("");

    /* ID scope isolation */
    SDL_Log("--- ID Scope Isolation ---");
    test_sequential_panels_different_widget_ids();
    test_sequential_windows_different_widget_ids();
    test_second_window_scope_starts_from_root();

    /* Stack depth restoration */
    SDL_Log("--- Stack Depth Restoration ---");
    test_window_end_restores_id_stack_depth();
    test_collapsed_window_restores_id_stack_depth();
    test_panel_end_restores_layout_depth();
    test_panel_end_restores_id_stack_depth();

    /* Layout isolation */
    SDL_Log("--- Layout Isolation ---");
    test_layout_depth_zero_between_panels();
    test_layout_cursor_fresh_for_each_panel();

    /* Panel state isolation */
    SDL_Log("--- Panel State Isolation ---");
    test_clip_rect_cleared_between_panels();
    test_panel_scroll_y_pointer_isolation();
    test_panel_active_cleared_between_panels();

    /* Window state isolation */
    SDL_Log("--- Window State Isolation ---");
    test_window_draw_redirect_restored();
    test_keyboard_suppression_cleared_between_windows();

    /* Pre-clamp cross-contamination */
    SDL_Log("--- Pre-Clamp Cross-Contamination ---");
    test_same_title_panels_preclamp_contamination();
    test_different_title_panels_skip_preclamp();

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
