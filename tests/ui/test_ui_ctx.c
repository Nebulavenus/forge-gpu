/*
 * UI Context Tests
 *
 * Automated tests for common/ui/forge_ui_ctx.h — the immediate-mode UI
 * context, including init/free lifecycle, the hot/active state machine,
 * hit testing, labels, buttons, draw data generation, edge-triggered
 * activation, buffer growth, and overflow guards.
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
#include "ui/forge_ui_ctx.h"

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
        if (SDL_isnan(_a) || SDL_isnan(_b)) {                             \
            SDL_Log("    FAIL: %s == %f, expected %f (NaN, "      \
                    "line %d)", #a, (double)_a, (double)_b,       \
                    __LINE__);                                    \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        if (SDL_fabsf(_a - _b) > (eps)) {                             \
            SDL_Log("    FAIL: %s == %f, expected %f (eps=%f, "   \
                    "line %d)", #a, _a, _b, (float)(eps),         \
                    __LINE__);                                    \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

/* ── Shared font/atlas ──────────────────────────────────────────────────── */

#define DEFAULT_FONT_PATH "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"
#define PIXEL_HEIGHT      28.0f
#define ATLAS_PADDING     1
#define ASCII_START       32
#define ASCII_END         126
#define ASCII_COUNT       (ASCII_END - ASCII_START + 1)

/* ── Audit fix test constants ────────────────────────────────────────── */
#define TEST_FNV1A_OFFSET_BASIS  0x811c9dc5u
#define TEST_FNV1A_ALT_SEED     0x12345678u
#define TEST_FNV1A_MIN_SEED     0x00000001u

/* Separator / audit fix test rects */
#define TEST_SEP_PANEL_X     10.0f
#define TEST_SEP_PANEL_Y     10.0f
#define TEST_SEP_PANEL_W     300.0f
#define TEST_SEP_PANEL_H     200.0f
#define TEST_SEP_BTN_X       10.0f
#define TEST_SEP_BTN_Y       10.0f
#define TEST_SEP_BTN_W       100.0f
#define TEST_SEP_BTN_H       30.0f

/* ── Common widget test geometry ──────────────────────────────────────── */
#define TEST_BTN_X         10.0f
#define TEST_BTN_Y         10.0f
#define TEST_BTN_W        100.0f
#define TEST_BTN_H         40.0f
#define TEST_BTN_CENTER_X  (TEST_BTN_X + TEST_BTN_W * 0.5f)
#define TEST_BTN_CENTER_Y  (TEST_BTN_Y + TEST_BTN_H * 0.5f)

#define TEST_CB_X          10.0f
#define TEST_CB_Y          10.0f
#define TEST_CB_W         200.0f
#define TEST_CB_H          30.0f
#define TEST_CB_CENTER_X   (TEST_CB_X + TEST_CB_W * 0.5f)
#define TEST_CB_CENTER_Y   (TEST_CB_Y + TEST_CB_H * 0.5f)

#define TEST_SL_X         100.0f
#define TEST_SL_Y          10.0f
#define TEST_SL_W         200.0f
#define TEST_SL_H          30.0f
#define TEST_SL_CENTER_X  (TEST_SL_X + TEST_SL_W * 0.5f)
#define TEST_SL_CENTER_Y  (TEST_SL_Y + TEST_SL_H * 0.5f)
#define TEST_MOUSE_FAR    300.0f

/* ── Emit rect test geometry ──────────────────────────────────────────── */
#define TEST_EMIT_X        20.0f
#define TEST_EMIT_Y        30.0f
#define TEST_EMIT_W        80.0f
#define TEST_EMIT_H        50.0f

/* ── Rect contains test geometry ──────────────────────────────────────── */
#define TEST_RECT_X        10.0f
#define TEST_RECT_Y        20.0f
#define TEST_RECT_W       100.0f
#define TEST_RECT_H        50.0f

/* ── Fake atlas test values ───────────────────────────────────────────── */
#define TEST_FAKE_PX_HEIGHT     32.0f
#define TEST_FAKE_UPM          1000
#define TEST_FAKE_ASCENDER      800
#define TEST_FAKE_UPM_REAL     2048

/* ── Drag float test constants ──────────────────────────────────────────── */
#define TEST_DF_X          10.0f
#define TEST_DF_Y          10.0f
#define TEST_DF_W         120.0f
#define TEST_DF_H          30.0f
#define TEST_DF_CENTER_X   (TEST_DF_X + TEST_DF_W * 0.5f)
#define TEST_DF_CENTER_Y   (TEST_DF_Y + TEST_DF_H * 0.5f)
#define TEST_DF_SPEED      0.1f
#define TEST_DF_MIN       -100.0f
#define TEST_DF_MAX        100.0f
#define TEST_DF_INIT       5.0f
#define TEST_DF_DRAG_DX   20.0f
#define TEST_DFN_W        360.0f   /* wider rect for multi-component drag */

/* ── Drag int test constants ───────────────────────────────────────────── */
#define TEST_DI_X          10.0f
#define TEST_DI_Y          10.0f
#define TEST_DI_W         120.0f
#define TEST_DI_H          30.0f
#define TEST_DI_CENTER_X   (TEST_DI_X + TEST_DI_W * 0.5f)
#define TEST_DI_CENTER_Y   (TEST_DI_Y + TEST_DI_H * 0.5f)
#define TEST_DI_SPEED      1.0f
#define TEST_DI_MIN       -50
#define TEST_DI_MAX        50
#define TEST_DI_INIT       10

/* ── Listbox test constants ────────────────────────────────────────────── */
#define TEST_LB_X          10.0f
#define TEST_LB_Y          10.0f
#define TEST_LB_W         150.0f
#define TEST_LB_H         120.0f
#define TEST_LB_ITEM_COUNT 4

/* ── Dropdown test constants ───────────────────────────────────────────── */
#define TEST_DD_X          10.0f
#define TEST_DD_Y          10.0f
#define TEST_DD_W         150.0f
#define TEST_DD_H         200.0f
#define TEST_DD_ITEM_COUNT 3

/* ── Radio button test constants ───────────────────────────────────────── */
#define TEST_RAD_X         10.0f
#define TEST_RAD_Y         10.0f
#define TEST_RAD_W        150.0f
#define TEST_RAD_H         30.0f
#define TEST_RAD_CENTER_X  (TEST_RAD_X + TEST_RAD_W * 0.5f)
#define TEST_RAD_CENTER_Y  (TEST_RAD_Y + TEST_RAD_H * 0.5f)
#define TEST_RAD_B_Y       60.0f   /* second radio group Y position */

/* ── Color picker test constants ───────────────────────────────────────── */
#define TEST_CP_X          10.0f
#define TEST_CP_Y          10.0f
#define TEST_CP_W         200.0f
#define TEST_CP_H         250.0f
#define TEST_CP_SHORT_H    30.0f   /* short rect for overflow test */
#define TEST_CP_TEXT_SLOP  16.0f   /* tolerance for text glyph descenders past container */

/* ── HSV/RGB conversion test constants ─────────────────────────────────── */
#define TEST_HSV_RED_H     0.0f
#define TEST_HSV_RED_S     1.0f
#define TEST_HSV_RED_V     1.0f
#define TEST_HSV_GREEN_H   120.0f
#define TEST_HSV_GREEN_S   1.0f
#define TEST_HSV_GREEN_V   1.0f
#define TEST_HSV_BLUE_H    240.0f
#define TEST_HSV_BLUE_S    1.0f
#define TEST_HSV_BLUE_V    1.0f
#define TEST_HSV_EPS       0.01f

static ForgeUiFont     test_font;
static ForgeUiFontAtlas test_atlas;
static bool font_loaded  = false;
static bool atlas_built  = false;
static bool setup_failed = false;  /* cache failure so we only attempt once */

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

/* ── forge_ui__rect_contains tests ──────────────────────────────────────── */

static void test_rect_contains_inside(void)
{
    TEST("rect_contains: point inside");
    ForgeUiRect r = { TEST_RECT_X, TEST_RECT_Y, TEST_RECT_W, TEST_RECT_H };
    float cx = TEST_RECT_X + TEST_RECT_W * 0.5f;
    float cy = TEST_RECT_Y + TEST_RECT_H * 0.5f;
    ASSERT_TRUE(forge_ui__rect_contains(r, cx, cy));
}

static void test_rect_contains_outside(void)
{
    TEST("rect_contains: point outside");
    ForgeUiRect r = { TEST_RECT_X, TEST_RECT_Y, TEST_RECT_W, TEST_RECT_H };
    float cx = TEST_RECT_X + TEST_RECT_W * 0.5f;
    float cy = TEST_RECT_Y + TEST_RECT_H * 0.5f;
    ASSERT_TRUE(!forge_ui__rect_contains(r, TEST_RECT_X - 5.0f, cy));
    ASSERT_TRUE(!forge_ui__rect_contains(r, TEST_RECT_X + TEST_RECT_W + 5.0f, cy));
    ASSERT_TRUE(!forge_ui__rect_contains(r, cx, TEST_RECT_Y - 5.0f));
    ASSERT_TRUE(!forge_ui__rect_contains(r, cx, TEST_RECT_Y + TEST_RECT_H + 10.0f));
}

static void test_rect_contains_left_edge(void)
{
    TEST("rect_contains: point on left edge (inclusive)");
    ForgeUiRect r = { TEST_RECT_X, TEST_RECT_Y, TEST_RECT_W, TEST_RECT_H };
    float cy = TEST_RECT_Y + TEST_RECT_H * 0.5f;
    ASSERT_TRUE(forge_ui__rect_contains(r, TEST_RECT_X, cy));
}

static void test_rect_contains_right_edge(void)
{
    TEST("rect_contains: point on right edge (exclusive)");
    ForgeUiRect r = { TEST_RECT_X, TEST_RECT_Y, TEST_RECT_W, TEST_RECT_H };
    float cy = TEST_RECT_Y + TEST_RECT_H * 0.5f;
    ASSERT_TRUE(!forge_ui__rect_contains(r, TEST_RECT_X + TEST_RECT_W, cy));
}

static void test_rect_contains_top_edge(void)
{
    TEST("rect_contains: point on top edge (inclusive)");
    ForgeUiRect r = { TEST_RECT_X, TEST_RECT_Y, TEST_RECT_W, TEST_RECT_H };
    float cx = TEST_RECT_X + TEST_RECT_W * 0.5f;
    ASSERT_TRUE(forge_ui__rect_contains(r, cx, TEST_RECT_Y));
}

static void test_rect_contains_bottom_edge(void)
{
    TEST("rect_contains: point on bottom edge (exclusive)");
    ForgeUiRect r = { TEST_RECT_X, TEST_RECT_Y, TEST_RECT_W, TEST_RECT_H };
    float cx = TEST_RECT_X + TEST_RECT_W * 0.5f;
    ASSERT_TRUE(!forge_ui__rect_contains(r, cx, TEST_RECT_Y + TEST_RECT_H));
}

static void test_rect_contains_zero_size(void)
{
    TEST("rect_contains: zero-size rect never contains");
    ForgeUiRect r = { 10.0f, 20.0f, 0.0f, 0.0f };
    ASSERT_TRUE(!forge_ui__rect_contains(r, 10.0f, 20.0f));
}

/* ── forge_ui__ascender_px tests ────────────────────────────────────────── */

static void test_ascender_px_known_values(void)
{
    TEST("ascender_px: correct result for known inputs");
    /* Use a fake atlas with hand-picked values so we can verify the
     * result against a pre-computed constant, not the same formula. */
    ForgeUiFontAtlas fake;
    SDL_memset(&fake, 0, sizeof(fake));
    fake.pixel_height = TEST_FAKE_PX_HEIGHT;
    fake.units_per_em = TEST_FAKE_UPM;
    fake.ascender     = TEST_FAKE_ASCENDER;

    /* Expected: scale = 32/1000 = 0.032, ascender_px = 800*0.032 = 25.6 */
    float result = forge_ui__ascender_px(&fake);
    ASSERT_NEAR(result, 25.6f, 0.001f);
}

static void test_ascender_px_real_atlas(void)
{
    TEST("ascender_px: positive result from real font atlas");
    if (!setup_atlas()) return;

    float result = forge_ui__ascender_px(&test_atlas);

    /* A real font always has a positive ascender */
    ASSERT_TRUE(result > 0.0f);
    /* Result must be less than the full pixel height */
    ASSERT_TRUE(result < test_atlas.pixel_height);
}

static void test_ascender_px_null_atlas(void)
{
    TEST("ascender_px: returns 0 for NULL atlas");
    float result = forge_ui__ascender_px(NULL);
    ASSERT_NEAR(result, 0.0f, 0.001f);
}

static void test_ascender_px_zero_upm(void)
{
    TEST("ascender_px: returns 0 when units_per_em is 0");
    ForgeUiFontAtlas fake;
    SDL_memset(&fake, 0, sizeof(fake));
    fake.pixel_height = PIXEL_HEIGHT;
    fake.units_per_em = 0;     /* invalid / zero */
    fake.ascender     = TEST_FAKE_ASCENDER;

    float result = forge_ui__ascender_px(&fake);
    ASSERT_NEAR(result, 0.0f, 0.001f);
}

static void test_ascender_px_zero_ascender(void)
{
    TEST("ascender_px: returns 0 when ascender is 0");
    ForgeUiFontAtlas fake;
    SDL_memset(&fake, 0, sizeof(fake));
    fake.pixel_height = PIXEL_HEIGHT;
    fake.units_per_em = TEST_FAKE_UPM_REAL;
    fake.ascender     = 0;     /* ascender is zero */

    float result = forge_ui__ascender_px(&fake);
    ASSERT_NEAR(result, 0.0f, 0.001f);
}

/* ── forge_ui_ctx_init tests ────────────────────────────────────────────── */

static void test_init_success(void)
{
    TEST("ctx_init: successful initialization");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ASSERT_TRUE(ctx.vertices != NULL);
    ASSERT_TRUE(ctx.indices != NULL);
    ASSERT_EQ_INT(ctx.vertex_capacity, FORGE_UI_CTX_INITIAL_VERTEX_CAPACITY);
    ASSERT_EQ_INT(ctx.index_capacity, FORGE_UI_CTX_INITIAL_INDEX_CAPACITY);
    ASSERT_EQ_INT(ctx.vertex_count, 0);
    ASSERT_EQ_INT(ctx.index_count, 0);
    ASSERT_EQ_U32(ctx.hot, FORGE_UI_ID_NONE);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);
    ASSERT_TRUE(ctx.atlas == &test_atlas);
    forge_ui_ctx_free(&ctx);
}

static void test_init_null_ctx(void)
{
    TEST("ctx_init: NULL ctx returns false");
    if (!setup_atlas()) return;
    ASSERT_TRUE(!forge_ui_ctx_init(NULL, &test_atlas));
}

static void test_init_null_atlas(void)
{
    TEST("ctx_init: NULL atlas returns false");
    ForgeUiContext ctx;
    ASSERT_TRUE(!forge_ui_ctx_init(&ctx, NULL));
}

/* ── forge_ui_ctx_free tests ────────────────────────────────────────────── */

static void test_free_zeroes_state(void)
{
    TEST("ctx_free: zeroes all state");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_free(&ctx);

    ASSERT_TRUE(ctx.vertices == NULL);
    ASSERT_TRUE(ctx.indices == NULL);
    ASSERT_TRUE(ctx.atlas == NULL);
    ASSERT_EQ_INT(ctx.vertex_count, 0);
    ASSERT_EQ_INT(ctx.index_count, 0);
    ASSERT_EQ_INT(ctx.vertex_capacity, 0);
    ASSERT_EQ_INT(ctx.index_capacity, 0);
    ASSERT_EQ_U32(ctx.hot, FORGE_UI_ID_NONE);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);
}

static void test_free_null_ctx(void)
{
    TEST("ctx_free: NULL ctx does not crash");
    forge_ui_ctx_free(NULL);
    /* no crash = pass */
}

static void test_free_double_free(void)
{
    TEST("ctx_free: double free does not crash");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_free(&ctx);
    forge_ui_ctx_free(&ctx);
    /* no crash = pass */
}

/* ── forge_ui_ctx_begin tests ───────────────────────────────────────────── */

static void test_begin_updates_input(void)
{
    TEST("ctx_begin: updates mouse state");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 100.0f, 200.0f, true);
    ASSERT_TRUE(ctx.mouse_x == 100.0f);
    ASSERT_TRUE(ctx.mouse_y == 200.0f);
    ASSERT_TRUE(ctx.mouse_down == true);
    ASSERT_EQ_U32(ctx.next_hot, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_begin_resets_draw_data(void)
{
    TEST("ctx_begin: resets vertex/index counts");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Emit some data */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_label(&ctx, "Hello", 10.0f, 10.0f);
    ASSERT_TRUE(ctx.vertex_count > 0);

    /* Begin again should reset */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ASSERT_EQ_INT(ctx.vertex_count, 0);
    ASSERT_EQ_INT(ctx.index_count, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_begin_tracks_mouse_prev(void)
{
    TEST("ctx_begin: tracks previous mouse state");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* First frame: mouse up */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_end(&ctx);

    /* Second frame: mouse down. Previous should be false */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, true);
    ASSERT_TRUE(ctx.mouse_down_prev == false);
    ASSERT_TRUE(ctx.mouse_down == true);
    forge_ui_ctx_end(&ctx);

    /* Third frame: mouse still down. Previous should be true */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, true);
    ASSERT_TRUE(ctx.mouse_down_prev == true);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_free(&ctx);
}

static void test_begin_null_ctx(void)
{
    TEST("ctx_begin: NULL ctx does not crash");
    forge_ui_ctx_begin(NULL, 0.0f, 0.0f, false);
    /* no crash = pass */
}

/* ── forge_ui_ctx_end tests ─────────────────────────────────────────────── */

static void test_end_promotes_hot(void)
{
    TEST("ctx_end: promotes next_hot to hot when no active");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ctx.next_hot = 42;
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, 42);

    forge_ui_ctx_free(&ctx);
}

static void test_end_freezes_hot_when_active(void)
{
    TEST("ctx_end: freezes hot when a widget is active");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Set up active widget */
    ctx.active = 5;
    ctx.hot = 5;
    ctx.mouse_down = true;

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, true);
    ctx.next_hot = 10;  /* A different widget claims hot */
    forge_ui_ctx_end(&ctx);

    /* hot should NOT be updated to next_hot because active is set */
    ASSERT_EQ_U32(ctx.hot, 5);

    forge_ui_ctx_free(&ctx);
}

static void test_end_clears_stuck_active(void)
{
    TEST("ctx_end: clears active when mouse is up (safety valve)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Simulate: widget 7 was active, but mouse is released and widget
     * is no longer declared (disappeared). Without the safety valve,
     * active would stay stuck at 7 forever. */
    ctx.active = 7;
    ctx.mouse_down = false;

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    /* Do NOT declare any widget with id=7 */
    forge_ui_ctx_end(&ctx);

    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_end_null_ctx(void)
{
    TEST("ctx_end: NULL ctx does not crash");
    forge_ui_ctx_end(NULL);
    /* no crash = pass */
}

/* ── forge_ui_ctx_label tests ───────────────────────────────────────────── */

#define LABEL_TEST_X      10.0f   /* label x position for all label tests */
#define LABEL_TEST_Y      30.0f   /* label y position for all label tests */
#define LABEL_TEST_R       1.0f   /* label color red   (white) */
#define LABEL_TEST_G       1.0f   /* label color green (white) */
#define LABEL_TEST_B       1.0f   /* label color blue  (white) */
#define LABEL_TEST_A       1.0f   /* label color alpha (opaque) */

static void test_label_emits_vertices(void)
{
    TEST("ctx_label: emits vertices for text");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_label_colored(&ctx, "AB", LABEL_TEST_X, LABEL_TEST_Y,
                               LABEL_TEST_R, LABEL_TEST_G, LABEL_TEST_B,
                               LABEL_TEST_A);

    /* 2 visible glyphs -> 2*4 = 8 vertices, 2*6 = 12 indices */
    ASSERT_EQ_INT(ctx.vertex_count, 8);
    ASSERT_EQ_INT(ctx.index_count, 12);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_empty_string(void)
{
    TEST("ctx_label: empty string emits nothing");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_label_colored(&ctx, "", LABEL_TEST_X, LABEL_TEST_Y,
                               LABEL_TEST_R, LABEL_TEST_G, LABEL_TEST_B,
                               LABEL_TEST_A);

    ASSERT_EQ_INT(ctx.vertex_count, 0);
    ASSERT_EQ_INT(ctx.index_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_null_text(void)
{
    TEST("ctx_label: NULL text does not crash");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_label_colored(&ctx, NULL, LABEL_TEST_X, LABEL_TEST_Y,
                               LABEL_TEST_R, LABEL_TEST_G, LABEL_TEST_B,
                               LABEL_TEST_A);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_null_ctx(void)
{
    TEST("ctx_label: NULL ctx does not crash");
    forge_ui_ctx_label_colored(NULL, "Hello", LABEL_TEST_X, LABEL_TEST_Y,
                               LABEL_TEST_R, LABEL_TEST_G, LABEL_TEST_B,
                               LABEL_TEST_A);
    /* no crash = pass */
}

/* ── forge_ui_ctx_button tests ──────────────────────────────────────────── */

static void test_button_emits_draw_data(void)
{
    TEST("ctx_button: emits background rect + text vertices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    forge_ui_ctx_button(&ctx, "OK", rect);

    /* Background rect: 4 verts + 6 idx.  "OK" = 2 glyphs: 8 verts + 12 idx.
     * Total: 12 verts, 18 idx */
    ASSERT_EQ_INT(ctx.vertex_count, 12);
    ASSERT_EQ_INT(ctx.index_count, 18);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_button_returns_false_no_click(void)
{
    TEST("ctx_button: returns false when not clicked");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };

    /* Mouse away from button */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    bool clicked = forge_ui_ctx_button(&ctx, "Test", rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!clicked);
    forge_ui_ctx_free(&ctx);
}

static void test_button_click_sequence(void)
{
    TEST("ctx_button: full click sequence (hover -> press -> release)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    float cx = 50.0f, cy = 30.0f;  /* center of button */
    bool clicked;

    Uint32 btn_id = forge_ui_hash_id(&ctx, "Btn");

    /* Frame 0: mouse away, no interaction */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    clicked = forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_U32(ctx.hot, FORGE_UI_ID_NONE);

    /* Frame 1: mouse over button (becomes hot) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    clicked = forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_U32(ctx.hot, btn_id);

    /* Frame 2: mouse pressed (becomes active, edge-triggered) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    clicked = forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_U32(ctx.active, btn_id);

    /* Frame 3: mouse released (click detected) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    clicked = forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(clicked);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_button_click_release_outside(void)
{
    TEST("ctx_button: no click when released outside button");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    float cx = 50.0f, cy = 30.0f;
    bool clicked;

    Uint32 btn_id = forge_ui_hash_id(&ctx, "Btn");

    /* Frame 0: hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, btn_id);

    /* Frame 2: release OUTSIDE the button */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    clicked = forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_button_hot_state(void)
{
    TEST("ctx_button: hot state set when mouse over");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };

    /* Frame: mouse over button */
    forge_ui_ctx_begin(&ctx, TEST_BTN_CENTER_X, TEST_BTN_CENTER_Y, false);
    forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_EQ_U32(ctx.hot, forge_ui_hash_id(&ctx, "Btn"));
    forge_ui_ctx_free(&ctx);
}

static void test_button_empty_label_rejected(void)
{
    TEST("ctx_button: empty label returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    forge_ui_ctx_begin(&ctx, TEST_BTN_CENTER_X, TEST_BTN_CENTER_Y, false);
    bool clicked = forge_ui_ctx_button(&ctx, "", rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!clicked);
    /* No draw data should be emitted */
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_button_null_ctx(void)
{
    TEST("ctx_button: NULL ctx returns false");
    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    bool clicked = forge_ui_ctx_button(NULL, "Btn", rect);
    ASSERT_TRUE(!clicked);
}

static void test_button_null_text(void)
{
    TEST("ctx_button: NULL text returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    forge_ui_ctx_begin(&ctx, TEST_BTN_CENTER_X, TEST_BTN_CENTER_Y, false);
    bool clicked = forge_ui_ctx_button(&ctx, NULL, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!clicked);
    forge_ui_ctx_free(&ctx);
}

/* ── Edge-triggered activation tests ────────────────────────────────────── */

static void test_button_edge_trigger_no_false_activate(void)
{
    TEST("ctx_button: held mouse dragged onto button does NOT activate");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    float cx = 50.0f, cy = 30.0f;

    /* Frame 0: mouse held down AWAY from button */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, true);
    forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    /* Frame 1: mouse still held, dragged ONTO button
     * With edge detection, this should NOT activate because mouse_down
     * was already true in the previous frame (no press edge). */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_button_edge_trigger_activates_on_press(void)
{
    TEST("ctx_button: activates on press edge (up->down transition)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    float cx = 50.0f, cy = 30.0f;

    Uint32 btn_id = forge_ui_hash_id(&ctx, "Btn");

    /* Frame 0: mouse over, not pressed */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, btn_id);

    /* Frame 1: mouse pressed (up->down edge) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_button(&ctx, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, btn_id);

    forge_ui_ctx_free(&ctx);
}

/* ── Multiple button tests ──────────────────────────────────────────────── */

static void test_multiple_buttons_last_hot_wins(void)
{
    TEST("multiple buttons: last drawn button wins hot (draw order priority)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Two overlapping buttons */
    ForgeUiRect rect1 = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    ForgeUiRect rect2 = { 50.0f, 10.0f, 100.0f, 40.0f };
    float cx = 80.0f, cy = 30.0f;  /* in overlap region */

    Uint32 id_b = forge_ui_hash_id(&ctx, "B");

    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, "A", rect1);
    forge_ui_ctx_button(&ctx, "B", rect2);
    forge_ui_ctx_end(&ctx);

    /* Button B was drawn last, so it should be hot */
    ASSERT_EQ_U32(ctx.hot, id_b);

    forge_ui_ctx_free(&ctx);
}

static void test_multiple_buttons_independent(void)
{
    TEST("multiple buttons: non-overlapping buttons have independent hot");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect1 = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    ForgeUiRect rect2 = { 10.0f, 60.0f, 100.0f, 40.0f };

    Uint32 id_a = forge_ui_hash_id(&ctx, "A");
    Uint32 id_b = forge_ui_hash_id(&ctx, "B");

    /* Mouse over button 1 */
    forge_ui_ctx_begin(&ctx, TEST_BTN_CENTER_X, TEST_BTN_CENTER_Y, false);
    forge_ui_ctx_button(&ctx, "A", rect1);
    forge_ui_ctx_button(&ctx, "B", rect2);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, id_a);

    /* Mouse over button 2 */
    forge_ui_ctx_begin(&ctx, 50.0f, 80.0f, false);
    forge_ui_ctx_button(&ctx, "A", rect1);
    forge_ui_ctx_button(&ctx, "B", rect2);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, id_b);

    forge_ui_ctx_free(&ctx);
}

static void test_overlap_press_last_drawn_wins(void)
{
    TEST("multiple buttons: overlapping press activates last-drawn (topmost)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Two overlapping buttons -- B is drawn after A so B is on top */
    ForgeUiRect rect_a = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    ForgeUiRect rect_b = { 50.0f, 10.0f, 100.0f, 40.0f };
    float cx = 80.0f, cy = 30.0f;  /* in overlap region */

    Uint32 id_b = forge_ui_hash_id(&ctx, "B");

    /* Frame 0: hover -- establish hot (last-drawn wins) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, "A", rect_a);
    forge_ui_ctx_button(&ctx, "B", rect_b);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, id_b);

    /* Frame 1: press -- last-drawn button must become active */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    bool clicked_a = forge_ui_ctx_button(&ctx, "A", rect_a);
    bool clicked_b = forge_ui_ctx_button(&ctx, "B", rect_b);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, id_b);
    ASSERT_TRUE(!clicked_a);
    ASSERT_TRUE(!clicked_b);

    /* Frame 2: release -- only button B registers a click */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    clicked_a = forge_ui_ctx_button(&ctx, "A", rect_a);
    clicked_b = forge_ui_ctx_button(&ctx, "B", rect_b);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!clicked_a);
    ASSERT_TRUE(clicked_b);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

/* ── Draw data verification tests ───────────────────────────────────────── */

static void test_button_rect_uses_white_uv(void)
{
    TEST("ctx_button: background rect uses atlas white_uv");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_button(&ctx, "A", rect);
    forge_ui_ctx_end(&ctx);

    /* First 4 vertices are the background rect; they should use white UV */
    float expected_u = (test_atlas.white_uv.u0 + test_atlas.white_uv.u1) * 0.5f;
    float expected_v = (test_atlas.white_uv.v0 + test_atlas.white_uv.v1) * 0.5f;

    ASSERT_TRUE(ctx.vertex_count >= 4);
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(ctx.vertices[i].uv_u == expected_u);
        ASSERT_TRUE(ctx.vertices[i].uv_v == expected_v);
    }

    forge_ui_ctx_free(&ctx);
}

static void test_button_normal_color(void)
{
    TEST("ctx_button: normal state uses normal color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    /* Mouse far away -> normal state */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_button(&ctx, "A", rect);
    forge_ui_ctx_end(&ctx);

    /* First vertex should have normal button color (theme surface) */
    ASSERT_TRUE(ctx.vertices[0].r == ctx.theme.surface.r);
    ASSERT_TRUE(ctx.vertices[0].g == ctx.theme.surface.g);
    ASSERT_TRUE(ctx.vertices[0].b == ctx.theme.surface.b);

    forge_ui_ctx_free(&ctx);
}

static void test_button_hot_color(void)
{
    TEST("ctx_button: hot state uses hot color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    float cx = 50.0f, cy = 30.0f;

    /* Frame 0: make hot */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, "A", rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: now hot, should use hot color */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, "A", rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[0].r == ctx.theme.surface_hot.r);
    ASSERT_TRUE(ctx.vertices[0].g == ctx.theme.surface_hot.g);
    ASSERT_TRUE(ctx.vertices[0].b == ctx.theme.surface_hot.b);

    forge_ui_ctx_free(&ctx);
}

static void test_button_active_color(void)
{
    TEST("ctx_button: active state uses active color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    float cx = 50.0f, cy = 30.0f;

    /* Frame 0: hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, "A", rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: press (active) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_button(&ctx, "A", rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[0].r == ctx.theme.surface_active.r);
    ASSERT_TRUE(ctx.vertices[0].g == ctx.theme.surface_active.g);
    ASSERT_TRUE(ctx.vertices[0].b == ctx.theme.surface_active.b);

    forge_ui_ctx_free(&ctx);
}

static void test_rect_ccw_winding(void)
{
    TEST("emit_rect: generates CCW winding indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    forge_ui__emit_rect(&ctx, rect, 1, 1, 1, 1);

    /* 6 indices: two triangles (0,1,2) and (0,2,3) */
    ASSERT_EQ_INT(ctx.index_count, 6);
    ASSERT_EQ_U32(ctx.indices[0], 0);
    ASSERT_EQ_U32(ctx.indices[1], 1);
    ASSERT_EQ_U32(ctx.indices[2], 2);
    ASSERT_EQ_U32(ctx.indices[3], 0);
    ASSERT_EQ_U32(ctx.indices[4], 2);
    ASSERT_EQ_U32(ctx.indices[5], 3);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_rect_vertex_positions(void)
{
    TEST("emit_rect: vertex positions match rect bounds");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ForgeUiRect rect = { TEST_EMIT_X, TEST_EMIT_Y, TEST_EMIT_W, TEST_EMIT_H };
    forge_ui__emit_rect(&ctx, rect, 1, 0, 0, 1);

    ASSERT_EQ_INT(ctx.vertex_count, 4);

    /* TL */
    ASSERT_TRUE(ctx.vertices[0].pos_x == 20.0f);
    ASSERT_TRUE(ctx.vertices[0].pos_y == 30.0f);
    /* TR */
    ASSERT_TRUE(ctx.vertices[1].pos_x == 100.0f);
    ASSERT_TRUE(ctx.vertices[1].pos_y == 30.0f);
    /* BR */
    ASSERT_TRUE(ctx.vertices[2].pos_x == 100.0f);
    ASSERT_TRUE(ctx.vertices[2].pos_y == 80.0f);
    /* BL */
    ASSERT_TRUE(ctx.vertices[3].pos_x == 20.0f);
    ASSERT_TRUE(ctx.vertices[3].pos_y == 80.0f);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Buffer growth tests ────────────────────────────────────────────────── */

static void test_grow_vertices_from_zero(void)
{
    TEST("grow_vertices: recovers from zero capacity (post-free)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_free(&ctx);

    /* After free, capacity is 0.  Re-init the atlas pointer so we can test
     * the grow function in isolation. */
    ctx.atlas = &test_atlas;
    ASSERT_TRUE(forge_ui__grow_vertices(&ctx, 4));
    ASSERT_TRUE(ctx.vertices != NULL);
    ASSERT_TRUE(ctx.vertex_capacity >= 4);

    SDL_free(ctx.vertices);
    ctx.vertices = NULL;
}

static void test_grow_indices_from_zero(void)
{
    TEST("grow_indices: recovers from zero capacity (post-free)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_free(&ctx);

    ctx.atlas = &test_atlas;
    ASSERT_TRUE(forge_ui__grow_indices(&ctx, 6));
    ASSERT_TRUE(ctx.indices != NULL);
    ASSERT_TRUE(ctx.index_capacity >= 6);

    SDL_free(ctx.indices);
    ctx.indices = NULL;
}

static void test_grow_vertices_negative_count(void)
{
    TEST("grow_vertices: negative count returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ASSERT_TRUE(!forge_ui__grow_vertices(&ctx, -1));
    forge_ui_ctx_free(&ctx);
}

static void test_grow_indices_negative_count(void)
{
    TEST("grow_indices: negative count returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ASSERT_TRUE(!forge_ui__grow_indices(&ctx, -1));
    forge_ui_ctx_free(&ctx);
}

static void test_grow_vertices_zero_count(void)
{
    TEST("grow_vertices: zero count returns true (no-op)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ASSERT_TRUE(forge_ui__grow_vertices(&ctx, 0));
    forge_ui_ctx_free(&ctx);
}

static void test_grow_many_widgets(void)
{
    TEST("grow: buffer grows with many widgets");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    /* Emit 100 rectangles to force buffer growth */
    for (int i = 0; i < 100; i++) {
        ForgeUiRect rect = { (float)i * 5.0f, 0.0f, 4.0f, 4.0f };
        forge_ui__emit_rect(&ctx, rect, 1, 1, 1, 1);
    }

    /* 100 rects * 4 verts = 400 verts, > initial capacity of 256 */
    ASSERT_EQ_INT(ctx.vertex_count, 400);
    ASSERT_TRUE(ctx.vertex_capacity >= 400);
    ASSERT_EQ_INT(ctx.index_count, 600);
    ASSERT_TRUE(ctx.index_capacity >= 600);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Index offset tests ─────────────────────────────────────────────────── */

static void test_multiple_rects_index_offsets(void)
{
    TEST("emit_rect: second rect indices offset by first rect's vertex count");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    ForgeUiRect r1 = { 0, 0, 10, 10 };
    ForgeUiRect r2 = { 20, 0, 10, 10 };
    forge_ui__emit_rect(&ctx, r1, 1, 1, 1, 1);
    forge_ui__emit_rect(&ctx, r2, 1, 1, 1, 1);

    /* Second rect's indices should start at base=4 */
    ASSERT_EQ_U32(ctx.indices[6], 4);   /* second tri 0: base+0 */
    ASSERT_EQ_U32(ctx.indices[7], 5);   /* second tri 0: base+1 */
    ASSERT_EQ_U32(ctx.indices[8], 6);   /* second tri 0: base+2 */
    ASSERT_EQ_U32(ctx.indices[9], 4);   /* second tri 1: base+0 */
    ASSERT_EQ_U32(ctx.indices[10], 6);  /* second tri 1: base+2 */
    ASSERT_EQ_U32(ctx.indices[11], 7);  /* second tri 1: base+3 */

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── emit_rect NULL atlas test ──────────────────────────────────────────── */

static void test_emit_rect_null_atlas(void)
{
    TEST("emit_rect: NULL atlas does not crash");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    /* Temporarily clear atlas to test the guard */
    const ForgeUiFontAtlas *saved = ctx.atlas;
    ctx.atlas = NULL;

    ForgeUiRect rect = { 0, 0, 10, 10 };
    forge_ui__emit_rect(&ctx, rect, 1, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    ctx.atlas = saved;
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── emit_text_layout tests ─────────────────────────────────────────────── */

static void test_emit_text_layout_null(void)
{
    TEST("emit_text_layout: NULL layout does not crash");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    forge_ui__emit_text_layout(&ctx, NULL);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_emit_text_layout_empty(void)
{
    TEST("emit_text_layout: layout with zero vertices is a no-op");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    ForgeUiTextLayout layout;
    SDL_memset(&layout, 0, sizeof(layout));
    forge_ui__emit_text_layout(&ctx, &layout);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_checkbox tests ────────────────────────────────────────── */

static void test_checkbox_emits_draw_data(void)
{
    TEST("ctx_checkbox: emits box rect + label vertices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_checkbox(&ctx, "AB", &val, rect);

    /* Outer box: 4 verts + 6 idx.  "AB" = 2 glyphs: 8 verts + 12 idx.
     * No inner fill because val is false.  Total: 12 verts, 18 idx */
    ASSERT_EQ_INT(ctx.vertex_count, 12);
    ASSERT_EQ_INT(ctx.index_count, 18);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_checked_emits_inner_fill(void)
{
    TEST("ctx_checkbox: checked state emits inner fill rect");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = true;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_checkbox(&ctx, "AB", &val, rect);

    /* Outer box: 4+6.  Inner fill: 4+6.  "AB": 8+12.  Total: 16 verts, 24 idx */
    ASSERT_EQ_INT(ctx.vertex_count, 16);
    ASSERT_EQ_INT(ctx.index_count, 24);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_toggle_sequence(void)
{
    TEST("ctx_checkbox: full toggle sequence (hover -> press -> release toggles value)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    float cx = 50.0f, cy = 25.0f;
    bool toggled;

    /* Frame 0: mouse away */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    toggled = forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_TRUE(!val);

    Uint32 opt_id = forge_ui_hash_id(&ctx, "Opt");

    /* Frame 1: hover (becomes hot) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    toggled = forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_EQ_U32(ctx.hot, opt_id);

    /* Frame 2: press (becomes active) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    toggled = forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_EQ_U32(ctx.active, opt_id);
    ASSERT_TRUE(!val);

    /* Frame 3: release (toggles val to true) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    toggled = forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(toggled);
    ASSERT_TRUE(val);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    /* Frame 4-5-6: click again to toggle back to false */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_begin(&ctx, cx, cy, false);
    toggled = forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(toggled);
    ASSERT_TRUE(!val);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_no_toggle_release_outside(void)
{
    TEST("ctx_checkbox: no toggle when released outside");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    float cx = 50.0f, cy = 25.0f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* Press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, forge_ui_hash_id(&ctx, "Opt"));

    /* Release outside */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    bool toggled = forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_TRUE(!val);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_null_ctx(void)
{
    TEST("ctx_checkbox: NULL ctx returns false");
    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    bool toggled = forge_ui_ctx_checkbox(NULL, "Opt", &val, rect);
    ASSERT_TRUE(!toggled);
}

static void test_checkbox_null_label(void)
{
    TEST("ctx_checkbox: NULL label returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    forge_ui_ctx_begin(&ctx, TEST_CB_CENTER_X, TEST_CB_CENTER_Y, false);
    bool toggled = forge_ui_ctx_checkbox(&ctx, NULL, &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_null_value(void)
{
    TEST("ctx_checkbox: NULL value returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    forge_ui_ctx_begin(&ctx, TEST_CB_CENTER_X, TEST_CB_CENTER_Y, false);
    bool toggled = forge_ui_ctx_checkbox(&ctx, "Opt", NULL, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_empty_label_rejected(void)
{
    TEST("ctx_checkbox: empty label returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    forge_ui_ctx_begin(&ctx, TEST_CB_CENTER_X, TEST_CB_CENTER_Y, false);
    bool toggled = forge_ui_ctx_checkbox(&ctx, "", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_null_atlas(void)
{
    TEST("ctx_checkbox: NULL atlas returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, TEST_CB_CENTER_X, TEST_CB_CENTER_Y, false);

    const ForgeUiFontAtlas *saved = ctx.atlas;
    ctx.atlas = NULL;

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    bool toggled = forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    ASSERT_TRUE(!toggled);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    ctx.atlas = saved;
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_normal_color(void)
{
    TEST("ctx_checkbox: normal state uses normal box color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };

    /* Mouse far away -> normal state */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* First 4 vertices are the outer box (theme surface) */
    ASSERT_TRUE(ctx.vertices[0].r == ctx.theme.surface.r);
    ASSERT_TRUE(ctx.vertices[0].g == ctx.theme.surface.g);
    ASSERT_TRUE(ctx.vertices[0].b == ctx.theme.surface.b);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_hot_color(void)
{
    TEST("ctx_checkbox: hot state uses hot box color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    float cx = 50.0f, cy = 25.0f;

    /* Frame 0: become hot */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: now hot=1, check color */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[0].r == ctx.theme.surface_hot.r);
    ASSERT_TRUE(ctx.vertices[0].g == ctx.theme.surface_hot.g);
    ASSERT_TRUE(ctx.vertices[0].b == ctx.theme.surface_hot.b);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_active_color(void)
{
    TEST("ctx_checkbox: active state uses active box color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    float cx = 50.0f, cy = 25.0f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* Press (active) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[0].r == ctx.theme.surface_active.r);
    ASSERT_TRUE(ctx.vertices[0].g == ctx.theme.surface_active.g);
    ASSERT_TRUE(ctx.vertices[0].b == ctx.theme.surface_active.b);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_edge_trigger(void)
{
    TEST("ctx_checkbox: held mouse dragged onto checkbox does NOT activate");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { TEST_CB_X, TEST_CB_Y, TEST_CB_W, TEST_CB_H };
    float cx = 50.0f, cy = 25.0f;

    /* Frame 0: mouse held down away */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, true);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: mouse still held, dragged onto checkbox */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_checkbox(&ctx, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_slider tests ─────────────────────────────────────────── */

static void test_slider_emits_draw_data(void)
{
    TEST("ctx_slider: emits track + thumb rectangles");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);

    /* Track: 4 verts + 6 idx.  Thumb: 4 verts + 6 idx.
     * Total: 8 verts, 12 idx */
    ASSERT_EQ_INT(ctx.vertex_count, 8);
    ASSERT_EQ_INT(ctx.index_count, 12);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_value_snap_on_click(void)
{
    TEST("ctx_slider: value snaps to click position on press");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.0f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    /* Effective track: x = 100 + 6 = 106, w = 200 - 12 = 188 */
    /* Click at midpoint: mouse_x = 106 + 94 = 200 -> t = 94/188 = 0.5 */
    float mid_x = 100.0f + FORGE_UI_SL_THUMB_WIDTH * 0.5f + (200.0f - FORGE_UI_SL_THUMB_WIDTH) * 0.5f;
    float cy = 25.0f;
    bool changed;

    Uint32 sl_id = forge_ui_hash_id(&ctx, "##slider");

    /* Frame 0: hover */
    forge_ui_ctx_begin(&ctx, mid_x, cy, false);
    changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!changed);
    ASSERT_EQ_U32(ctx.hot, sl_id);

    /* Frame 1: press (active + snap) */
    forge_ui_ctx_begin(&ctx, mid_x, cy, true);
    changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(changed);
    ASSERT_NEAR(val, 0.5f, 0.01f);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_drag_outside_bounds(void)
{
    TEST("ctx_slider: drag continues when cursor moves outside widget");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    float cx = 200.0f, cy = 25.0f;
    bool changed;

    /* Frame 0: hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, forge_ui_hash_id(&ctx, "##slider"));

    /* Frame 2: drag FAR to the right (outside widget) — still active, value clamped */
    forge_ui_ctx_begin(&ctx, 500.0f, 200.0f, true);
    changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(changed);
    ASSERT_NEAR(val, 1.0f, 0.001f);
    ASSERT_EQ_U32(ctx.active, forge_ui_hash_id(&ctx, "##slider"));

    /* Frame 3: drag FAR to the left — value clamped to min */
    forge_ui_ctx_begin(&ctx, -100.0f, 200.0f, true);
    changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(changed);
    ASSERT_NEAR(val, 0.0f, 0.001f);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_release_clears_active(void)
{
    TEST("ctx_slider: releasing mouse clears active");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    float cx = 200.0f, cy = 25.0f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, forge_ui_hash_id(&ctx, "##slider"));

    /* Release */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_value_mapping(void)
{
    TEST("ctx_slider: value maps correctly with custom range");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.0f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    /* track_x = 100 + 6 = 106, track_w = 200 - 12 = 188 */
    float track_x = 100.0f + FORGE_UI_SL_THUMB_WIDTH * 0.5f;
    float track_w = 200.0f - FORGE_UI_SL_THUMB_WIDTH;
    float cy = 25.0f;

    /* Click at 75% of the track with range [10, 50] */
    float click_x = track_x + track_w * 0.75f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, click_x, cy, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 10.0f, 50.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Press */
    forge_ui_ctx_begin(&ctx, click_x, cy, true);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 10.0f, 50.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Expected: 10 + 0.75 * (50 - 10) = 10 + 30 = 40 */
    ASSERT_NEAR(val, 40.0f, 0.5f);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_null_ctx(void)
{
    TEST("ctx_slider: NULL ctx returns false");
    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    bool changed = forge_ui_ctx_slider(NULL, "##slider", &val, 0.0f, 1.0f, rect);
    ASSERT_TRUE(!changed);
}

static void test_slider_null_value(void)
{
    TEST("ctx_slider: NULL value returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    forge_ui_ctx_begin(&ctx, TEST_SL_CENTER_X, TEST_SL_CENTER_Y, false);
    bool changed = forge_ui_ctx_slider(&ctx, "##slider", NULL, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_empty_label_rejected(void)
{
    TEST("ctx_slider: NULL label returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    forge_ui_ctx_begin(&ctx, TEST_SL_CENTER_X, TEST_SL_CENTER_Y, false);
    bool changed = forge_ui_ctx_slider(&ctx, NULL, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_null_atlas(void)
{
    TEST("ctx_slider: NULL atlas returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, TEST_SL_CENTER_X, TEST_SL_CENTER_Y, false);

    const ForgeUiFontAtlas *saved = ctx.atlas;
    ctx.atlas = NULL;

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    bool changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    ctx.atlas = saved;
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_invalid_range(void)
{
    TEST("ctx_slider: max <= min returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };

    forge_ui_ctx_begin(&ctx, TEST_SL_CENTER_X, TEST_SL_CENTER_Y, false);

    /* Equal range */
    bool changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 5.0f, 5.0f, rect);
    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    /* Inverted range */
    changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 10.0f, 5.0f, rect);
    ASSERT_TRUE(!changed);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_nan_range_rejected(void)
{
    TEST("ctx_slider: NaN in range returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    float nan_val = NAN;

    forge_ui_ctx_begin(&ctx, TEST_SL_CENTER_X, TEST_SL_CENTER_Y, false);

    /* NaN min */
    bool changed = forge_ui_ctx_slider(&ctx, "##slider", &val, nan_val, 1.0f, rect);
    ASSERT_TRUE(!changed);

    /* NaN max */
    changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, nan_val, rect);
    ASSERT_TRUE(!changed);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_narrow_rect(void)
{
    TEST("ctx_slider: rect narrower than thumb width still works");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    /* Width of 5 is less than FORGE_UI_SL_THUMB_WIDTH (12) */
    ForgeUiRect rect = { 10.0f, 10.0f, 5.0f, 30.0f };

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    bool changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Should emit draw data (track + thumb) without crashing */
    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(ctx.vertex_count, 8);
    ASSERT_EQ_INT(ctx.index_count, 12);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_normal_color(void)
{
    TEST("ctx_slider: normal state uses normal thumb color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };

    /* Mouse far away */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* First 4 vertices = track, next 4 = thumb */
    ASSERT_TRUE(ctx.vertex_count >= 8);
    ASSERT_TRUE(ctx.vertices[4].r == ctx.theme.surface_hot.r);
    ASSERT_TRUE(ctx.vertices[4].g == ctx.theme.surface_hot.g);
    ASSERT_TRUE(ctx.vertices[4].b == ctx.theme.surface_hot.b);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_hot_color(void)
{
    TEST("ctx_slider: hot state uses hot thumb color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    float cx = TEST_SL_X, cy = TEST_SL_CENTER_Y;

    /* Frame 0: become hot */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: now hot, check thumb color */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[4].r == ctx.theme.accent_hot.r);
    ASSERT_TRUE(ctx.vertices[4].g == ctx.theme.accent_hot.g);
    ASSERT_TRUE(ctx.vertices[4].b == ctx.theme.accent_hot.b);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_active_color(void)
{
    TEST("ctx_slider: active state uses active thumb color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    float cx = TEST_SL_X, cy = TEST_SL_CENTER_Y;

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Press (active) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[4].r == ctx.theme.accent.r);
    ASSERT_TRUE(ctx.vertices[4].g == ctx.theme.accent.g);
    ASSERT_TRUE(ctx.vertices[4].b == ctx.theme.accent.b);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_track_uses_white_uv(void)
{
    TEST("ctx_slider: track rect uses atlas white_uv");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* First 4 vertices are the track rect */
    float expected_u = (test_atlas.white_uv.u0 + test_atlas.white_uv.u1) * 0.5f;
    float expected_v = (test_atlas.white_uv.v0 + test_atlas.white_uv.v1) * 0.5f;
    ASSERT_TRUE(ctx.vertex_count >= 4);
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(ctx.vertices[i].uv_u == expected_u);
        ASSERT_TRUE(ctx.vertices[i].uv_v == expected_v);
    }

    forge_ui_ctx_free(&ctx);
}

static void test_slider_edge_trigger(void)
{
    TEST("ctx_slider: held mouse dragged onto slider does NOT activate");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    float cx = TEST_SL_X, cy = TEST_SL_CENTER_Y;

    /* Frame 0: mouse held away */
    forge_ui_ctx_begin(&ctx, 400.0f, 400.0f, true);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: still held, dragged onto slider */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);
    /* Value should not have changed */
    ASSERT_NEAR(val, 0.5f, 0.001f);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_returns_false_when_same_value(void)
{
    TEST("ctx_slider: returns false when drag produces same value");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.0f;
    ForgeUiRect rect = { TEST_SL_X, TEST_SL_Y, TEST_SL_W, TEST_SL_H };
    float track_x = 100.0f + FORGE_UI_SL_THUMB_WIDTH * 0.5f;
    float cy = 25.0f;

    /* Hover at far left */
    forge_ui_ctx_begin(&ctx, track_x, cy, false);
    forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Press at far left, val should become 0.0 = same as current */
    forge_ui_ctx_begin(&ctx, track_x, cy, true);
    bool changed = forge_ui_ctx_slider(&ctx, "##slider", &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!changed);
    ASSERT_NEAR(val, 0.0f, 0.001f);

    forge_ui_ctx_free(&ctx);
}

/* ── Button NULL atlas test ────────────────────────────────────────────── */

static void test_button_null_atlas(void)
{
    TEST("ctx_button: NULL atlas returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, TEST_BTN_CENTER_X, TEST_BTN_CENTER_Y, false);

    const ForgeUiFontAtlas *saved = ctx.atlas;
    ctx.atlas = NULL;

    ForgeUiRect rect = { TEST_BTN_X, TEST_BTN_Y, TEST_BTN_W, TEST_BTN_H };
    bool clicked = forge_ui_ctx_button(&ctx, "Btn", rect);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    ctx.atlas = saved;
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_set_keyboard tests ─────────────────────────────────────── */

static void test_set_keyboard_basic(void)
{
    TEST("ctx_set_keyboard: sets all keyboard fields");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_ctx_set_keyboard(&ctx, "AB", true, true, true, true, true, true, true);

    ASSERT_TRUE(ctx.text_input != NULL);
    ASSERT_TRUE(ctx.text_input[0] == 'A');
    ASSERT_TRUE(ctx.key_backspace);
    ASSERT_TRUE(ctx.key_delete);
    ASSERT_TRUE(ctx.key_left);
    ASSERT_TRUE(ctx.key_right);
    ASSERT_TRUE(ctx.key_home);
    ASSERT_TRUE(ctx.key_end);
    ASSERT_TRUE(ctx.key_escape);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_set_keyboard_null_ctx(void)
{
    TEST("ctx_set_keyboard: NULL ctx does not crash");
    forge_ui_ctx_set_keyboard(NULL, "X", false, false, false, false,
                              false, false, false);
    /* no crash = pass */
}

static void test_begin_resets_keyboard(void)
{
    TEST("ctx_begin: resets keyboard state from previous frame");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Set keyboard state */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_ctx_set_keyboard(&ctx, "Hi", true, true, true, true, true, true, true);
    forge_ui_ctx_end(&ctx);

    /* Begin a new frame -- keyboard should be reset */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(ctx.text_input == NULL);
    ASSERT_TRUE(!ctx.key_backspace);
    ASSERT_TRUE(!ctx.key_delete);
    ASSERT_TRUE(!ctx.key_left);
    ASSERT_TRUE(!ctx.key_right);
    ASSERT_TRUE(!ctx.key_home);
    ASSERT_TRUE(!ctx.key_end);
    ASSERT_TRUE(!ctx.key_escape);
    ASSERT_TRUE(!ctx._ti_press_claimed);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui__emit_border tests ───────────────────────────────────────── */

static void test_emit_border_basic(void)
{
    TEST("emit_border: emits 4 edge rects (16 verts, 24 indices)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 10.0f, 10.0f, 100.0f, 60.0f };
    forge_ui__emit_border(&ctx, r, 2.0f, 1, 0, 0, 1);

    /* 4 rects * 4 verts = 16, 4 rects * 6 indices = 24 */
    ASSERT_EQ_INT(ctx.vertex_count, 16);
    ASSERT_EQ_INT(ctx.index_count, 24);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_emit_border_null_ctx(void)
{
    TEST("emit_border: NULL ctx does not crash");
    ForgeUiRect r = { 0, 0, 100, 100 };
    forge_ui__emit_border(NULL, r, 1.0f, 1, 1, 1, 1);
    /* no crash = pass */
}

static void test_emit_border_zero_width(void)
{
    TEST("emit_border: zero border width emits nothing");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 0, 0, 100, 100 };
    forge_ui__emit_border(&ctx, r, 0.0f, 1, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_emit_border_negative_width(void)
{
    TEST("emit_border: negative border width emits nothing");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 0, 0, 100, 100 };
    forge_ui__emit_border(&ctx, r, -5.0f, 1, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_emit_border_too_wide(void)
{
    TEST("emit_border: border wider than half rect emits nothing");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 0, 0, 10.0f, 100.0f };
    /* border_w = 6 > 10/2 = 5, should be rejected */
    forge_ui__emit_border(&ctx, r, 6.0f, 1, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_text_input -- parameter validation tests ─────────────── */

static void test_text_input_null_ctx(void)
{
    TEST("ctx_text_input: NULL ctx returns false");
    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(NULL, "##input", &st, r, true));
}

static void test_text_input_null_state(void)
{
    TEST("ctx_text_input: NULL state returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##input", NULL, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_null_buffer(void)
{
    TEST("ctx_text_input: NULL buffer returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiTextInputState st = { NULL, 32, 0, 0 };
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##input", &st, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_empty_label_rejected(void)
{
    TEST("ctx_text_input: NULL label returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, NULL, &st, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_zero_capacity(void)
{
    TEST("ctx_text_input: capacity=0 returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    char buf[1] = "";
    ForgeUiTextInputState st = { buf, 0, 0, 0 };
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##input", &st, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_negative_capacity(void)
{
    TEST("ctx_text_input: negative capacity returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, -1, 0, 0 };
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##input", &st, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_negative_length(void)
{
    TEST("ctx_text_input: negative length returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, -1, 0 };
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##input", &st, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_length_exceeds_capacity(void)
{
    TEST("ctx_text_input: length >= capacity returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    char buf[8] = "1234567";
    ForgeUiTextInputState st = { buf, 8, 8, 0 };  /* length == capacity */
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##input", &st, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_negative_cursor(void)
{
    TEST("ctx_text_input: negative cursor returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, -1 };
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##input", &st, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_cursor_exceeds_length(void)
{
    TEST("ctx_text_input: cursor > length returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, 5 };
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##input", &st, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_text_input -- focus acquisition tests ────────────────── */

static void test_text_input_focus_click_sequence(void)
{
    TEST("ctx_text_input: click to focus (hover -> press -> release)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 100, 30 };
    float cx = 50.0f, cy = 25.0f;

    /* Frame 0: mouse away -- not focused */
    forge_ui_ctx_begin(&ctx, 300, 300, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);

    Uint32 ti_id = forge_ui_hash_id(&ctx, "##input");

    /* Frame 1: hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, ti_id);

    /* Frame 2: press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, ti_id);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);  /* not yet */

    /* Frame 3: release -- focus acquired */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, ti_id);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_focus_release_outside(void)
{
    TEST("ctx_text_input: no focus when released outside");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 100, 30 };
    float cx = 50.0f, cy = 25.0f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    /* Press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, forge_ui_hash_id(&ctx, "##input"));

    /* Release OUTSIDE */
    forge_ui_ctx_begin(&ctx, 300, 300, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_unfocus_click_outside(void)
{
    TEST("ctx_text_input: click outside unfocuses");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 100, 30 };
    float cx = 50.0f, cy = 25.0f;

    /* Focus the widget */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, forge_ui_hash_id(&ctx, "##input"));

    /* Press OUTSIDE -- should unfocus */
    forge_ui_ctx_begin(&ctx, 300, 300, true);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_unfocus_escape(void)
{
    TEST("ctx_text_input: Escape unfocuses");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 100, 30 };
    float cx = 50.0f, cy = 25.0f;

    /* Focus the widget */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, forge_ui_hash_id(&ctx, "##input"));

    /* Escape */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, false,
                              false, false, true);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_escape_clears_active(void)
{
    TEST("ctx_text_input: Escape during press clears active (no re-focus on release)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 100, 30 };
    float cx = 50.0f, cy = 25.0f;

    /* Focus the widget via click */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, forge_ui_hash_id(&ctx, "##input"));

    /* Press on the widget + Escape in same frame */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, false,
                              false, false, true);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    /* Release on widget -- must NOT re-focus because active was cleared */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_text_input -- character insertion tests ───────────────── */

/* Helper: focus a text input widget on a context (3-frame sequence) */
static bool focus_text_input(ForgeUiContext *ctx, const char *label,
                             ForgeUiTextInputState *st, ForgeUiRect r)
{
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;

    /* Hover frame */
    forge_ui_ctx_begin(ctx, cx, cy, false);
    forge_ui_ctx_text_input(ctx, label, st, r, true);
    forge_ui_ctx_end(ctx);
    if (ctx->id_stack_depth != 0) return false;

    /* Press frame */
    forge_ui_ctx_begin(ctx, cx, cy, true);
    forge_ui_ctx_text_input(ctx, label, st, r, true);
    forge_ui_ctx_end(ctx);
    if (ctx->id_stack_depth != 0) return false;

    /* Release frame */
    forge_ui_ctx_begin(ctx, cx, cy, false);
    forge_ui_ctx_text_input(ctx, label, st, r, true);
    forge_ui_ctx_end(ctx);
    if (ctx->id_stack_depth != 0) return false;
    Uint32 expected_id = forge_ui_hash_id(ctx, label);
    return (ctx->focused == expected_id);
}

static void test_text_input_insert_chars(void)
{
    TEST("ctx_text_input: insert characters at cursor");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));
    ASSERT_EQ_U32(ctx.focused, forge_ui_hash_id(&ctx, "##input"));

    /* Type "Hi" */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "Hi", false, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(changed);
    ASSERT_EQ_INT(st.length, 2);
    ASSERT_EQ_INT(st.cursor, 2);
    ASSERT_TRUE(SDL_strcmp(buf, "Hi") == 0);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_insert_mid_string(void)
{
    TEST("ctx_text_input: mid-string insertion shifts tail right");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "AC";
    ForgeUiTextInputState st = { buf, 32, 2, 1 };  /* cursor between A and C */
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "B", false, false, false, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(SDL_strcmp(buf, "ABC") == 0);
    ASSERT_EQ_INT(st.cursor, 2);
    ASSERT_EQ_INT(st.length, 3);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_insert_at_capacity(void)
{
    TEST("ctx_text_input: insertion rejected when buffer is full");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[4] = "ABC";  /* capacity=4, length=3, one byte for '\0' */
    ForgeUiTextInputState st = { buf, 4, 3, 3 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "D", false, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!changed);
    ASSERT_TRUE(SDL_strcmp(buf, "ABC") == 0);
    ASSERT_EQ_INT(st.length, 3);

    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_text_input -- backspace/delete tests ─────────────────── */

static void test_text_input_backspace(void)
{
    TEST("ctx_text_input: backspace deletes byte before cursor");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "ABC";
    ForgeUiTextInputState st = { buf, 32, 3, 3 };  /* cursor at end */
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, true, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(changed);
    ASSERT_TRUE(SDL_strcmp(buf, "AB") == 0);
    ASSERT_EQ_INT(st.length, 2);
    ASSERT_EQ_INT(st.cursor, 2);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_backspace_at_start(void)
{
    TEST("ctx_text_input: backspace at cursor=0 is a no-op");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, 0 };  /* cursor at start */
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, true, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!changed);
    ASSERT_TRUE(SDL_strcmp(buf, "AB") == 0);
    ASSERT_EQ_INT(st.cursor, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_backspace_empty(void)
{
    TEST("ctx_text_input: backspace on empty buffer is a no-op");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, true, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(st.length, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_delete_key(void)
{
    TEST("ctx_text_input: delete removes byte at cursor");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "ABC";
    ForgeUiTextInputState st = { buf, 32, 3, 1 };  /* cursor after A */
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, true, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(changed);
    ASSERT_TRUE(SDL_strcmp(buf, "AC") == 0);
    ASSERT_EQ_INT(st.cursor, 1);
    ASSERT_EQ_INT(st.length, 2);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_delete_at_end(void)
{
    TEST("ctx_text_input: delete at cursor=length is a no-op");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, 2 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, true, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!changed);
    ASSERT_TRUE(SDL_strcmp(buf, "AB") == 0);

    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_text_input -- cursor movement tests ──────────────────── */

static void test_text_input_cursor_left_right(void)
{
    TEST("ctx_text_input: Left/Right move cursor");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, 2 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;

    /* Left arrow */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, true, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(st.cursor, 1);

    /* Right arrow */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, true,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(st.cursor, 2);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_cursor_home_end(void)
{
    TEST("ctx_text_input: Home/End jump cursor");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "ABCDE";
    ForgeUiTextInputState st = { buf, 32, 5, 3 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;

    /* Home */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, false,
                              true, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(st.cursor, 0);

    /* End */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, false,
                              false, true, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(st.cursor, 5);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_left_at_start(void)
{
    TEST("ctx_text_input: Left at cursor=0 stays at 0");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, 0 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, true, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(st.cursor, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_right_at_end(void)
{
    TEST("ctx_text_input: Right at cursor=length stays at length");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, 2 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, true,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(st.cursor, 2);

    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_text_input -- mutual exclusion tests ─────────────────── */

static void test_text_input_backspace_beats_insert(void)
{
    TEST("ctx_text_input: backspace takes priority over insertion in same frame");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, 2 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    /* Both backspace and text input in same frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "C", true, false, false, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    /* Backspace should win: "AB" -> "A", not "AB" -> "ABC" -> "AB" */
    ASSERT_TRUE(SDL_strcmp(buf, "A") == 0);
    ASSERT_EQ_INT(st.length, 1);
    ASSERT_EQ_INT(st.cursor, 1);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_delete_beats_insert(void)
{
    TEST("ctx_text_input: delete takes priority over insertion in same frame");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, 1 };  /* cursor after A */
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    /* Both delete and text input in same frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "C", false, true, false, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    /* Delete should win: removes B at cursor 1, result "A" */
    ASSERT_TRUE(SDL_strcmp(buf, "A") == 0);
    ASSERT_EQ_INT(st.length, 1);
    ASSERT_EQ_INT(st.cursor, 1);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_backspace_blocks_cursor_move(void)
{
    TEST("ctx_text_input: backspace blocks cursor movement in same frame");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "ABC";
    ForgeUiTextInputState st = { buf, 32, 3, 2 };  /* cursor after B */
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    /* Backspace + Left in same frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, true, false, true, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    /* Backspace: "ABC" -> "AC", cursor 2->1. Left should NOT also fire. */
    ASSERT_TRUE(SDL_strcmp(buf, "AC") == 0);
    ASSERT_EQ_INT(st.cursor, 1);  /* not 0 */

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_insert_blocks_cursor_move(void)
{
    TEST("ctx_text_input: insertion blocks cursor movement in same frame");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "AB";
    ForgeUiTextInputState st = { buf, 32, 2, 2 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    /* Insert "C" + Left in same frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "C", false, false, true, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    /* Insert: "AB" -> "ABC", cursor 2->3. Left should NOT also fire. */
    ASSERT_TRUE(SDL_strcmp(buf, "ABC") == 0);
    ASSERT_EQ_INT(st.cursor, 3);  /* not 2 */

    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_text_input -- draw data tests ────────────────────────── */

static void test_text_input_emits_draw_data(void)
{
    TEST("ctx_text_input: unfocused emits background rect");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    forge_ui_ctx_begin(&ctx, 300, 300, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    /* At minimum: background rect = 4 verts, 6 indices */
    ASSERT_TRUE(ctx.vertex_count >= 4);
    ASSERT_TRUE(ctx.index_count >= 6);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_focused_emits_border(void)
{
    TEST("ctx_text_input: focused emits background + border");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    ASSERT_TRUE(focus_text_input(&ctx, "##input", &st, r));

    /* Render a focused frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    /* bg rect (4v) + border (4*4v=16v) + cursor bar (4v) = 24 verts */
    ASSERT_TRUE(ctx.vertex_count >= 24);

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_not_focused_no_keyboard(void)
{
    TEST("ctx_text_input: keyboard input ignored when not focused");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 10, 10, 200, 30 };

    /* NOT focused -- send keyboard input */
    forge_ui_ctx_begin(&ctx, 300, 300, false);
    forge_ui_ctx_set_keyboard(&ctx, "Hi", false, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, "##input", &st, r, true);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(st.length, 0);  /* nothing inserted */

    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_text_input -- overlap priority test ──────────────────── */

static void test_text_input_overlap_last_drawn_wins(void)
{
    TEST("ctx_text_input: overlapping inputs -- last drawn gets focus");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf1[32] = "";
    ForgeUiTextInputState st1 = { buf1, 32, 0, 0 };
    char buf2[32] = "";
    ForgeUiTextInputState st2 = { buf2, 32, 0, 0 };

    /* Overlapping rects */
    ForgeUiRect r1 = { 10, 10, 100, 30 };
    ForgeUiRect r2 = { 50, 10, 100, 30 };
    float cx = 80.0f, cy = 25.0f;  /* in overlap region */

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input1", &st1, r1, true);
    forge_ui_ctx_text_input(&ctx, "##input2", &st2, r2, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, forge_ui_hash_id(&ctx, "##input2"));  /* last drawn */

    /* Press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, "##input1", &st1, r1, true);
    forge_ui_ctx_text_input(&ctx, "##input2", &st2, r2, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, forge_ui_hash_id(&ctx, "##input2"));  /* last drawn wins */

    /* Release */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, "##input1", &st1, r1, true);
    forge_ui_ctx_text_input(&ctx, "##input2", &st2, r2, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, forge_ui_hash_id(&ctx, "##input2"));  /* last drawn gets focus */

    forge_ui_ctx_free(&ctx);
}

/* ── Null-termination validation test (audit fix) ───────────────────────── */

static void test_text_input_bad_null_termination(void)
{
    TEST("text input rejects buffer with missing null terminator");
    if (!atlas_built) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    char buf[16];
    SDL_memset(buf, 'A', sizeof(buf));  /* fill with non-zero, no '\0' at length */
    ForgeUiTextInputState state;
    state.buffer   = buf;
    state.capacity = (int)sizeof(buf);
    state.length   = 3;    /* claims 3 bytes, but buf[3] != '\0' */
    state.cursor   = 0;

    ForgeUiRect r = { 10, 10, 200, 30 };
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    bool result = forge_ui_ctx_text_input(&ctx, "##input", &state, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!result);  /* should reject: buf[length] != '\0' */

    forge_ui_ctx_free(&ctx);
}

/* ── Layout: push/pop basics ─────────────────────────────────────────────── */

static void test_layout_push_returns_true(void)
{
    TEST("layout_push returns true on success");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 10, 10, 200, 300 };
    bool ok = forge_ui_ctx_layout_push(&ctx, r, FORGE_UI_LAYOUT_VERTICAL,
                                       8.0f, 4.0f);
    ASSERT_TRUE(ok);
    ASSERT_EQ_INT(ctx.layout_depth, 1);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_pop_returns_true(void)
{
    TEST("layout_pop returns true on success");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 0, 0, 100, 100 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, r, FORGE_UI_LAYOUT_VERTICAL, -1, -1));
    bool ok = forge_ui_ctx_layout_pop(&ctx);
    ASSERT_TRUE(ok);
    ASSERT_EQ_INT(ctx.layout_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_push_null_ctx(void)
{
    TEST("layout_push with NULL ctx returns false");
    bool ok = forge_ui_ctx_layout_push(NULL, (ForgeUiRect){0,0,100,100},
                                       FORGE_UI_LAYOUT_VERTICAL, 0, 0);
    ASSERT_TRUE(!ok);
}

static void test_layout_pop_null_ctx(void)
{
    TEST("layout_pop with NULL ctx returns false");
    bool ok = forge_ui_ctx_layout_pop(NULL);
    ASSERT_TRUE(!ok);
}

static void test_layout_pop_empty_stack(void)
{
    TEST("layout_pop on empty stack returns false (no crash)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    /* Stack is empty — pop should fail gracefully */
    bool ok = forge_ui_ctx_layout_pop(&ctx);
    ASSERT_TRUE(!ok);
    ASSERT_EQ_INT(ctx.layout_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_push_overflow(void)
{
    TEST("layout_push at max depth returns false (no OOB write)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 0, 0, 100, 100 };
    /* Push up to max depth */
    for (int i = 0; i < FORGE_UI_LAYOUT_MAX_DEPTH; i++) {
        bool ok = forge_ui_ctx_layout_push(&ctx, r, FORGE_UI_LAYOUT_VERTICAL, -1, -1);
        ASSERT_TRUE(ok);
    }
    ASSERT_EQ_INT(ctx.layout_depth, FORGE_UI_LAYOUT_MAX_DEPTH);

    /* One more should fail */
    bool overflow = forge_ui_ctx_layout_push(&ctx, r, FORGE_UI_LAYOUT_VERTICAL, -1, -1);
    ASSERT_TRUE(!overflow);
    ASSERT_EQ_INT(ctx.layout_depth, FORGE_UI_LAYOUT_MAX_DEPTH);

    /* Pop all */
    for (int i = 0; i < FORGE_UI_LAYOUT_MAX_DEPTH; i++) {
        forge_ui_ctx_layout_pop(&ctx);
    }
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_nested_push_pop(void)
{
    TEST("layout nested push/pop tracks depth correctly");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 0, 0, 200, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, r, FORGE_UI_LAYOUT_VERTICAL, -1, -1));
    ASSERT_EQ_INT(ctx.layout_depth, 1);

    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, r, FORGE_UI_LAYOUT_HORIZONTAL, -1, -1));
    ASSERT_EQ_INT(ctx.layout_depth, 2);

    forge_ui_ctx_layout_pop(&ctx);
    ASSERT_EQ_INT(ctx.layout_depth, 1);

    forge_ui_ctx_layout_pop(&ctx);
    ASSERT_EQ_INT(ctx.layout_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Layout: layout_next positioning ─────────────────────────────────────── */

static void test_layout_next_null_ctx(void)
{
    TEST("layout_next with NULL ctx returns zero rect");
    ForgeUiRect r = forge_ui_ctx_layout_next(NULL, 30.0f);
    ASSERT_NEAR(r.x, 0.0f, 0.001f);
    ASSERT_NEAR(r.y, 0.0f, 0.001f);
    ASSERT_NEAR(r.w, 0.0f, 0.001f);
    ASSERT_NEAR(r.h, 0.0f, 0.001f);
}

static void test_layout_next_no_active_layout(void)
{
    TEST("layout_next with no active layout returns zero rect");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    /* No push — layout_next should return zero rect */
    ForgeUiRect r = forge_ui_ctx_layout_next(&ctx, 30.0f);
    ASSERT_NEAR(r.x, 0.0f, 0.001f);
    ASSERT_NEAR(r.w, 0.0f, 0.001f);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_vertical_positions(void)
{
    TEST("layout vertical: widgets stack top-to-bottom");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 10.0f, 20.0f, 200.0f, 300.0f };
    float padding = 5.0f;
    float spacing = 8.0f;
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL,
                             padding, spacing));

    /* First widget: should be at cursor start (no spacing before first) */
    ForgeUiRect r1 = forge_ui_ctx_layout_next(&ctx, 30.0f);
    ASSERT_NEAR(r1.x, 10.0f + 5.0f, 0.001f);    /* rect.x + padding */
    ASSERT_NEAR(r1.y, 20.0f + 5.0f, 0.001f);     /* rect.y + padding */
    ASSERT_NEAR(r1.w, 200.0f - 10.0f, 0.001f);   /* inner width */
    ASSERT_NEAR(r1.h, 30.0f, 0.001f);             /* requested size */

    /* Second widget: should be below first + spacing */
    ForgeUiRect r2 = forge_ui_ctx_layout_next(&ctx, 40.0f);
    ASSERT_NEAR(r2.x, 15.0f, 0.001f);             /* same x */
    ASSERT_NEAR(r2.y, 25.0f + 30.0f + 8.0f, 0.001f);  /* first.y + first.h + spacing */
    ASSERT_NEAR(r2.h, 40.0f, 0.001f);

    /* Third widget */
    ForgeUiRect r3 = forge_ui_ctx_layout_next(&ctx, 20.0f);
    ASSERT_NEAR(r3.y, r2.y + 40.0f + 8.0f, 0.001f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_horizontal_positions(void)
{
    TEST("layout horizontal: widgets stack left-to-right");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 10.0f, 20.0f, 300.0f, 50.0f };
    float padding = 4.0f;
    float spacing = 10.0f;
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_HORIZONTAL,
                             padding, spacing));

    /* First widget */
    ForgeUiRect r1 = forge_ui_ctx_layout_next(&ctx, 80.0f);
    ASSERT_NEAR(r1.x, 14.0f, 0.001f);            /* rect.x + padding */
    ASSERT_NEAR(r1.y, 24.0f, 0.001f);             /* rect.y + padding */
    ASSERT_NEAR(r1.w, 80.0f, 0.001f);             /* requested size */
    ASSERT_NEAR(r1.h, 42.0f, 0.001f);             /* inner height */

    /* Second widget: to the right + spacing */
    ForgeUiRect r2 = forge_ui_ctx_layout_next(&ctx, 60.0f);
    ASSERT_NEAR(r2.x, 14.0f + 80.0f + 10.0f, 0.001f);
    ASSERT_NEAR(r2.y, 24.0f, 0.001f);             /* same y */
    ASSERT_NEAR(r2.w, 60.0f, 0.001f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_remaining_after_last_widget(void)
{
    TEST("layout remaining_h is accurate after last widget (no extra spacing)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    /* 100px tall area, explicit 0 padding, 10px spacing */
    ForgeUiRect area = { 0, 0, 100, 100 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL,
                             -1.0f, 10.0f));

    /* Place one 30px widget */
    forge_ui_ctx_layout_next(&ctx, 30.0f);

    /* remaining_h should be 100 - 30 = 70, NOT 100 - 30 - 10 = 60.
     * The spacing before the NEXT widget hasn't been consumed yet. */
    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_NEAR(layout->remaining_h, 70.0f, 0.001f);

    /* Place second 20px widget — spacing of 10 is consumed BEFORE it */
    forge_ui_ctx_layout_next(&ctx, 20.0f);
    /* remaining = 70 - 10(spacing) - 20(widget) = 40 */
    ASSERT_NEAR(layout->remaining_h, 40.0f, 0.001f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Layout: parameter validation ────────────────────────────────────────── */

static void test_layout_push_negative_padding_clamped(void)
{
    TEST("layout_push clamps negative padding to 0");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 10, 20, 200, 100 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL,
                             -5.0f, 0.0f));

    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_NEAR(layout->padding, 0.0f, 0.001f);
    /* Cursor should be at rect origin (padding = 0) */
    ASSERT_NEAR(layout->cursor_x, 10.0f, 0.001f);
    ASSERT_NEAR(layout->cursor_y, 20.0f, 0.001f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_push_negative_spacing_clamped(void)
{
    TEST("layout_push clamps negative spacing to 0");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 100, 100 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL,
                             0.0f, -10.0f));

    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_NEAR(layout->spacing, 0.0f, 0.001f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_next_negative_size_clamped(void)
{
    TEST("layout_next clamps negative size to 0");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 100, 100 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, -1));

    ForgeUiRect r = forge_ui_ctx_layout_next(&ctx, -50.0f);
    ASSERT_NEAR(r.h, 0.0f, 0.001f);  /* negative size clamped to 0 */
    ASSERT_NEAR(r.y, 0.0f, 0.001f);  /* cursor didn't move backward */

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_push_tiny_rect_no_negative_remaining(void)
{
    TEST("layout_push with tiny rect: remaining clamped to 0");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    /* Rect smaller than 2*padding → inner space is 0, not negative */
    ForgeUiRect area = { 0, 0, 10, 10 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, 20.0f, -1.0f));

    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_NEAR(layout->remaining_w, 0.0f, 0.001f);
    ASSERT_NEAR(layout->remaining_h, 0.0f, 0.001f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Layout: lifecycle / begin/end interaction ───────────────────────────── */

static void test_layout_begin_resets_depth(void)
{
    TEST("layout begin() resets depth to 0");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* First frame: push without pop (intentional mismatch) */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect r = { 0, 0, 100, 100 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, r, FORGE_UI_LAYOUT_VERTICAL, -1, -1));
    /* Note: no pop — this is the bug scenario being tested */
    forge_ui_ctx_end(&ctx);  /* end will log a warning */

    /* Second frame: begin should have reset depth */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_EQ_INT(ctx.layout_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_free_resets_depth(void)
{
    TEST("layout free() resets depth to 0");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 0, 0, 100, 100 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, r, FORGE_UI_LAYOUT_VERTICAL, -1, -1));
    /* Free without popping */
    forge_ui_ctx_end(&ctx);  /* logs warning */
    forge_ui_ctx_free(&ctx);
    ASSERT_EQ_INT(ctx.layout_depth, 0);
}

/* ── Layout: widget variants — parameter validation ──────────────────────── */

static void test_button_layout_null_text(void)
{
    TEST("button_layout with NULL text returns false, no cursor advance");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, -1));

    /* NULL text should fail without advancing cursor */
    bool clicked = forge_ui_ctx_button_layout(&ctx, NULL, 30.0f);
    ASSERT_TRUE(!clicked);

    /* Cursor should NOT have advanced */
    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_EQ_INT(layout->item_count, 0);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_button_layout_empty_text(void)
{
    TEST("button_layout with empty text returns false, no cursor advance");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, -1));

    bool clicked = forge_ui_ctx_button_layout(&ctx, "", 30.0f);
    ASSERT_TRUE(!clicked);

    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_EQ_INT(layout->item_count, 0);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_layout_null_value(void)
{
    TEST("checkbox_layout with NULL value returns false, no cursor advance");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, -1));

    bool toggled = forge_ui_ctx_checkbox_layout(&ctx, "Test", NULL, 30.0f);
    ASSERT_TRUE(!toggled);

    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_EQ_INT(layout->item_count, 0);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_layout_null_label(void)
{
    TEST("checkbox_layout with NULL label returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, -1));

    bool val = true;
    bool toggled = forge_ui_ctx_checkbox_layout(&ctx, NULL, &val, 30.0f);
    ASSERT_TRUE(!toggled);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_layout_null_value(void)
{
    TEST("slider_layout with NULL value returns false, no cursor advance");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, -1));

    bool changed = forge_ui_ctx_slider_layout(&ctx, "##slider", NULL,
                                               0.0f, 100.0f, 30.0f);
    ASSERT_TRUE(!changed);

    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_EQ_INT(layout->item_count, 0);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_layout_invalid_range(void)
{
    TEST("slider_layout with min >= max returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, -1));

    float val = 50.0f;
    /* min == max: invalid range */
    bool changed = forge_ui_ctx_slider_layout(&ctx, "##slider", &val,
                                               100.0f, 100.0f, 30.0f);
    ASSERT_TRUE(!changed);

    /* min > max: also invalid */
    changed = forge_ui_ctx_slider_layout(&ctx, "##slider", &val,
                                          100.0f, 50.0f, 30.0f);
    ASSERT_TRUE(!changed);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Layout: widget variants — correct positioning ───────────────────────── */

static void test_label_layout_emits_draw_data(void)
{
    TEST("label_layout emits vertices at correct layout position");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 10, 20, 200, 100 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, -1));

    forge_ui_ctx_label_layout(&ctx, "Hi", 30.0f);
    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_button_layout_correct_rect(void)
{
    TEST("button_layout places button at layout cursor");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 200, 200, false);  /* mouse far away */

    ForgeUiRect area = { 10, 20, 200, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, 5, 8));

    int verts_before = ctx.vertex_count;
    (void)forge_ui_ctx_button_layout(&ctx, "Test", 30.0f);
    ASSERT_TRUE(ctx.vertex_count > verts_before);

    /* Button rect's first vertex should be near (15, 25) — the cursor start */
    ASSERT_NEAR(ctx.vertices[verts_before].pos_x, 15.0f, 0.5f);
    ASSERT_NEAR(ctx.vertices[verts_before].pos_y, 25.0f, 0.5f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Layout: spacing model correctness ───────────────────────────────────── */

static void test_layout_no_spacing_before_first_widget(void)
{
    TEST("layout does not add spacing before the first widget");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    /* Large spacing to make a gap obvious if misapplied */
    ForgeUiRect area = { 0, 0, 100, 100 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, 50.0f));

    ForgeUiRect r1 = forge_ui_ctx_layout_next(&ctx, 10.0f);
    /* First widget should be at y=0, not y=50 */
    ASSERT_NEAR(r1.y, 0.0f, 0.001f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_spacing_between_widgets(void)
{
    TEST("layout adds spacing between widgets but not after last");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 100, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, 10.0f));

    ForgeUiRect r1 = forge_ui_ctx_layout_next(&ctx, 20.0f);
    ForgeUiRect r2 = forge_ui_ctx_layout_next(&ctx, 20.0f);
    ForgeUiRect r3 = forge_ui_ctx_layout_next(&ctx, 20.0f);

    /* Verify gaps: 10px between r1-r2 and r2-r3 */
    ASSERT_NEAR(r1.y, 0.0f, 0.001f);
    ASSERT_NEAR(r2.y, 30.0f, 0.001f);   /* 0 + 20 + 10 */
    ASSERT_NEAR(r3.y, 60.0f, 0.001f);   /* 30 + 20 + 10 */

    /* remaining_h should be: 200 - 20 - 10 - 20 - 10 - 20 = 120
     * (no trailing spacing after r3) */
    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_NEAR(layout->remaining_h, 120.0f, 0.001f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_horizontal_spacing(void)
{
    TEST("horizontal layout spacing: gap between items, not before first");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 300, 50 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_HORIZONTAL, -1, 10.0f));

    ForgeUiRect r1 = forge_ui_ctx_layout_next(&ctx, 40.0f);
    ForgeUiRect r2 = forge_ui_ctx_layout_next(&ctx, 60.0f);

    ASSERT_NEAR(r1.x, 0.0f, 0.001f);
    ASSERT_NEAR(r2.x, 50.0f, 0.001f);  /* 0 + 40 + 10 */

    /* remaining_w = 300 - 40 - 10 - 60 = 190 */
    ForgeUiLayout *layout = &ctx.layout_stack[0];
    ASSERT_NEAR(layout->remaining_w, 190.0f, 0.001f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Layout: item_count tracking ─────────────────────────────────────────── */

static void test_layout_item_count(void)
{
    TEST("layout item_count increments correctly");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area, FORGE_UI_LAYOUT_VERTICAL, -1, -1));

    ASSERT_EQ_INT(ctx.layout_stack[0].item_count, 0);
    forge_ui_ctx_layout_next(&ctx, 10.0f);
    ASSERT_EQ_INT(ctx.layout_stack[0].item_count, 1);
    forge_ui_ctx_layout_next(&ctx, 10.0f);
    ASSERT_EQ_INT(ctx.layout_stack[0].item_count, 2);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Direction validation ─────────────────────────────────────────────────── */

static void test_layout_push_invalid_direction_rejected(void)
{
    TEST("layout_push rejects invalid direction value");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };

    /* A direction value outside the enum range should be rejected */
    bool ok = forge_ui_ctx_layout_push(&ctx, area,
                                        (ForgeUiLayoutDirection)99,
                                        5.0f, 5.0f);
    ASSERT_TRUE(!ok);
    ASSERT_EQ_INT(ctx.layout_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_push_negative_direction_rejected(void)
{
    TEST("layout_push rejects negative direction value");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };

    /* Negative direction value should be rejected */
    bool ok = forge_ui_ctx_layout_push(&ctx, area,
                                        (ForgeUiLayoutDirection)(-1),
                                        0.0f, 0.0f);
    ASSERT_TRUE(!ok);
    ASSERT_EQ_INT(ctx.layout_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_push_valid_directions_accepted(void)
{
    TEST("layout_push accepts both valid direction values");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect area = { 0, 0, 200, 200 };

    /* FORGE_UI_LAYOUT_VERTICAL should be accepted */
    bool ok = forge_ui_ctx_layout_push(&ctx, area,
                                        FORGE_UI_LAYOUT_VERTICAL,
                                        0.0f, 0.0f);
    ASSERT_TRUE(ok);
    ASSERT_EQ_INT(ctx.layout_depth, 1);
    forge_ui_ctx_layout_pop(&ctx);

    /* FORGE_UI_LAYOUT_HORIZONTAL should be accepted */
    ok = forge_ui_ctx_layout_push(&ctx, area,
                                   FORGE_UI_LAYOUT_HORIZONTAL,
                                   0.0f, 0.0f);
    ASSERT_TRUE(ok);
    ASSERT_EQ_INT(ctx.layout_depth, 1);
    forge_ui_ctx_layout_pop(&ctx);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Layout wrappers: no-op without active layout ────────────────────────── */

static void test_label_layout_noop_without_layout(void)
{
    TEST("label_layout is a no-op when no layout is active");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    /* No layout pushed — label_layout should silently return */
    int v_before = ctx.vertex_count;
    forge_ui_ctx_label_layout(&ctx, "Hi", 30.0f);
    ASSERT_EQ_INT(ctx.vertex_count, v_before);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_button_layout_noop_without_layout(void)
{
    TEST("button_layout returns false when no layout is active");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    /* No layout pushed */
    bool clicked = forge_ui_ctx_button_layout(&ctx, "OK", 30.0f);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_layout_noop_without_layout(void)
{
    TEST("checkbox_layout returns false when no layout is active");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    bool val = true;
    bool toggled = forge_ui_ctx_checkbox_layout(&ctx, "CB", &val, 30.0f);
    ASSERT_TRUE(!toggled);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_layout_noop_without_layout(void)
{
    TEST("slider_layout returns false when no layout is active");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float val = 50.0f;
    bool changed = forge_ui_ctx_slider_layout(&ctx, "##slider", &val,
                                               0.0f, 100.0f, 30.0f);
    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Layout wrappers: no cursor advance on null atlas ────────────────────── */

static void test_button_layout_null_atlas_no_advance(void)
{
    TEST("button_layout with null atlas returns false, no cursor advance");

    /* Build a context without atlas */
    ForgeUiContext ctx;
    SDL_memset(&ctx, 0, sizeof(ctx));
    ctx.vertices = (ForgeUiVertex *)SDL_malloc(
        FORGE_UI_CTX_INITIAL_VERTEX_CAPACITY * sizeof(ForgeUiVertex));
    ctx.indices = (Uint32 *)SDL_malloc(
        FORGE_UI_CTX_INITIAL_INDEX_CAPACITY * sizeof(Uint32));
    ctx.vertex_capacity = FORGE_UI_CTX_INITIAL_VERTEX_CAPACITY;
    ctx.index_capacity = FORGE_UI_CTX_INITIAL_INDEX_CAPACITY;
    ctx.atlas = NULL;
    ctx.hot = FORGE_UI_ID_NONE;
    ctx.active = FORGE_UI_ID_NONE;
    ctx.next_hot = FORGE_UI_ID_NONE;
    ctx.focused = FORGE_UI_ID_NONE;

    ForgeUiRect area = { 0, 0, 200, 200 };
    /* Push layout manually — direction validation will pass */
    ctx.layout_stack[0].rect = area;
    ctx.layout_stack[0].direction = FORGE_UI_LAYOUT_VERTICAL;
    ctx.layout_stack[0].padding = 0;
    ctx.layout_stack[0].spacing = 0;
    ctx.layout_stack[0].cursor_x = 0;
    ctx.layout_stack[0].cursor_y = 0;
    ctx.layout_stack[0].remaining_w = 200;
    ctx.layout_stack[0].remaining_h = 200;
    ctx.layout_stack[0].item_count = 0;
    ctx.layout_depth = 1;

    bool clicked = forge_ui_ctx_button_layout(&ctx, "OK", 30.0f);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_INT(ctx.layout_stack[0].item_count, 0);

    SDL_free(ctx.vertices);
    SDL_free(ctx.indices);
}

/* ── Panel and scrolling tests (Lesson 09 audit) ───────────────────────── */

/* Helper: set up a ctx with atlas for panel tests.
 * Returns true on success; callers must check before using ctx. */
static bool panel_test_setup(ForgeUiContext *ctx)
{
    if (!setup_atlas()) {
        /* setup_atlas increments fail_count on its own */
        return false;
    }
    if (!forge_ui_ctx_init(ctx, &test_atlas)) {
        SDL_Log("    FAIL: panel_test_setup: forge_ui_ctx_init failed "
                "(line %d)", __LINE__);
        fail_count++;
        return false;
    }
    forge_ui_ctx_begin(ctx, 0.0f, 0.0f, false);
    return true;
}

static void panel_test_teardown(ForgeUiContext *ctx)
{
    forge_ui_ctx_end(ctx);
    forge_ui_ctx_free(ctx);
}

/* ── widget_mouse_over tests ──────────────────────────────────────────── */

static void test_widget_mouse_over_no_clip(void)
{
    TEST("widget_mouse_over: returns true when mouse inside rect, no clip");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 50.0f, 50.0f, false);

    ForgeUiRect r = { 10.0f, 10.0f, 100.0f, 100.0f };
    ASSERT_TRUE(forge_ui__widget_mouse_over(&ctx, r));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_widget_mouse_over_outside(void)
{
    TEST("widget_mouse_over: returns false when mouse outside rect");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 200.0f, 200.0f, false);

    ForgeUiRect r = { 10.0f, 10.0f, 100.0f, 100.0f };
    ASSERT_TRUE(!forge_ui__widget_mouse_over(&ctx, r));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_widget_mouse_over_clipped(void)
{
    TEST("widget_mouse_over: returns false when mouse outside clip rect");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    /* Mouse at (50, 50) -- inside the widget rect but outside the clip */
    forge_ui_ctx_begin(&ctx, 50.0f, 50.0f, false);
    ctx.has_clip = true;
    ctx.clip_rect = (ForgeUiRect){ 200.0f, 200.0f, 100.0f, 100.0f };

    ForgeUiRect r = { 10.0f, 10.0f, 100.0f, 100.0f };
    ASSERT_TRUE(!forge_ui__widget_mouse_over(&ctx, r));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_widget_mouse_over_inside_clip(void)
{
    TEST("widget_mouse_over: returns true when inside both rect and clip");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 50.0f, 50.0f, false);
    ctx.has_clip = true;
    ctx.clip_rect = (ForgeUiRect){ 0.0f, 0.0f, 200.0f, 200.0f };

    ForgeUiRect r = { 10.0f, 10.0f, 100.0f, 100.0f };
    ASSERT_TRUE(forge_ui__widget_mouse_over(&ctx, r));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── emit_rect clipping tests ─────────────────────────────────────────── */

static void test_emit_rect_clip_discard(void)
{
    TEST("emit_rect: fully outside clip rect emits nothing");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ctx.has_clip = true;
    ctx.clip_rect = (ForgeUiRect){ 0.0f, 0.0f, 50.0f, 50.0f };

    int before = ctx.vertex_count;
    forge_ui__emit_rect(&ctx, (ForgeUiRect){ 100.0f, 100.0f, 50.0f, 50.0f },
                        1.0f, 1.0f, 1.0f, 1.0f);
    ASSERT_EQ_INT(ctx.vertex_count, before);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_emit_rect_clip_trim(void)
{
    TEST("emit_rect: partially outside clip rect trims vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ctx.has_clip = true;
    ctx.clip_rect = (ForgeUiRect){ 10.0f, 10.0f, 40.0f, 40.0f };

    int before = ctx.vertex_count;
    /* Rect from (0,0)-(60,60) partially overlaps clip (10,10)-(50,50) */
    forge_ui__emit_rect(&ctx, (ForgeUiRect){ 0.0f, 0.0f, 60.0f, 60.0f },
                        1.0f, 1.0f, 1.0f, 1.0f);
    ASSERT_EQ_INT(ctx.vertex_count, before + 4);  /* quad emitted */
    /* Check that top-left vertex is clipped to clip origin */
    ASSERT_NEAR(ctx.vertices[before].pos_x, 10.0f, 0.01f);
    ASSERT_NEAR(ctx.vertices[before].pos_y, 10.0f, 0.01f);
    /* Check bottom-right vertex clipped to clip extent */
    ASSERT_NEAR(ctx.vertices[before + 2].pos_x, 50.0f, 0.01f);
    ASSERT_NEAR(ctx.vertices[before + 2].pos_y, 50.0f, 0.01f);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── emit_quad_clipped tests ──────────────────────────────────────────── */

static void test_emit_quad_clipped_fully_outside(void)
{
    TEST("emit_quad_clipped: fully outside clip emits nothing");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    ForgeUiVertex src[4] = {
        { 100, 100, 0.0f, 0.0f, 1,1,1,1 },
        { 150, 100, 1.0f, 0.0f, 1,1,1,1 },
        { 150, 130, 1.0f, 1.0f, 1,1,1,1 },
        { 100, 130, 0.0f, 1.0f, 1,1,1,1 }
    };
    ForgeUiRect clip = { 0.0f, 0.0f, 50.0f, 50.0f };
    int before = ctx.vertex_count;
    forge_ui__emit_quad_clipped(&ctx, src, &clip);
    ASSERT_EQ_INT(ctx.vertex_count, before);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_emit_quad_clipped_uv_remap(void)
{
    TEST("emit_quad_clipped: partial clip remaps UVs proportionally");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    /* Quad from (0,0)-(100,100) with UV (0,0)-(1,1) */
    ForgeUiVertex src[4] = {
        {   0,   0, 0.0f, 0.0f, 1,1,1,1 },
        { 100,   0, 1.0f, 0.0f, 1,1,1,1 },
        { 100, 100, 1.0f, 1.0f, 1,1,1,1 },
        {   0, 100, 0.0f, 1.0f, 1,1,1,1 }
    };
    /* Clip to (25,25)-(75,75) — should produce UV (0.25,0.25)-(0.75,0.75) */
    ForgeUiRect clip = { 25.0f, 25.0f, 50.0f, 50.0f };
    int before = ctx.vertex_count;
    forge_ui__emit_quad_clipped(&ctx, src, &clip);
    ASSERT_EQ_INT(ctx.vertex_count, before + 4);

    /* Top-left UV should be (0.25, 0.25) */
    ASSERT_NEAR(ctx.vertices[before].uv_u, 0.25f, 0.001f);
    ASSERT_NEAR(ctx.vertices[before].uv_v, 0.25f, 0.001f);
    /* Bottom-right UV should be (0.75, 0.75) */
    ASSERT_NEAR(ctx.vertices[before + 2].uv_u, 0.75f, 0.001f);
    ASSERT_NEAR(ctx.vertices[before + 2].uv_v, 0.75f, 0.001f);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_emit_quad_clipped_degenerate(void)
{
    TEST("emit_quad_clipped: zero-width quad emits nothing");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    /* Zero-width quad: x0 == x1 */
    ForgeUiVertex src[4] = {
        { 50, 10, 0.0f, 0.0f, 1,1,1,1 },
        { 50, 10, 1.0f, 0.0f, 1,1,1,1 },
        { 50, 30, 1.0f, 1.0f, 1,1,1,1 },
        { 50, 30, 0.0f, 1.0f, 1,1,1,1 }
    };
    ForgeUiRect clip = { 0.0f, 0.0f, 100.0f, 100.0f };
    int before = ctx.vertex_count;
    forge_ui__emit_quad_clipped(&ctx, src, &clip);
    ASSERT_EQ_INT(ctx.vertex_count, before);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Panel test constants ─────────────────────────────────────────────── */

#define PANEL_X         0.0f   /* default panel origin x */
#define PANEL_Y         0.0f   /* default panel origin y */
#define PANEL_W       300.0f   /* standard panel width */
#define PANEL_H       300.0f   /* standard panel height (content fits) */
#define SMALL_PANEL_H 200.0f   /* shorter panel height (content overflows) */
#define ITEM_H         30.0f   /* standard widget / item height */

/* ── panel_begin parameter validation ─────────────────────────────────── */

static void test_panel_begin_null_ctx(void)
{
    TEST("panel_begin: null ctx returns false");
    float scroll_y = 0.0f;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(NULL, "Test",
                (ForgeUiRect){ 0, 0, 200, 200 }, &scroll_y));
}

static void test_panel_begin_null_scroll_y(void)
{
    TEST("panel_begin: null scroll_y returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 0, 0, 200, 200 }, NULL));
    panel_test_teardown(&ctx);
}

static void test_panel_begin_empty_title_rejected(void)
{
    TEST("panel_begin: empty title returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "",
                (ForgeUiRect){ 0, 0, 200, 200 }, &scroll_y));
    panel_test_teardown(&ctx);
}

static void test_panel_begin_null_title_rejected(void)
{
    TEST("panel_begin: NULL title returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, NULL,
                (ForgeUiRect){ 0, 0, 200, 200 }, &scroll_y));
    panel_test_teardown(&ctx);
}

static void test_panel_begin_nested_rejected(void)
{
    TEST("panel_begin: nested panel rejected when one is already active");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y1 = 0.0f, scroll_y2 = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    /* Compute expected hash BEFORE panel_begin pushes an ID scope */
    Uint32 expected_id = forge_ui_hash_id(&ctx, "Panel A");
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Panel A",
                (ForgeUiRect){ 0, 0, 200, 200 }, &scroll_y1));
    /* Second panel_begin while first is active should fail */
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Panel B",
                (ForgeUiRect){ 0, 0, 200, 200 }, &scroll_y2));
    /* First panel should still be active */
    ASSERT_TRUE(ctx._panel_active);
    ASSERT_EQ_U32(ctx._panel.id, expected_id);

    forge_ui_ctx_panel_end(&ctx);
    panel_test_teardown(&ctx);
}

static void test_panel_begin_zero_width_rejected(void)
{
    TEST("panel_begin: zero width rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 0, 0, 0, 200 }, &scroll_y));
    panel_test_teardown(&ctx);
}

static void test_panel_begin_negative_height_rejected(void)
{
    TEST("panel_begin: negative height rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 0, 0, 200, -50 }, &scroll_y));
    panel_test_teardown(&ctx);
}

static void test_panel_begin_nan_scroll_sanitized(void)
{
    TEST("panel_begin: NaN scroll_y sanitized to 0");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = NAN;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    ASSERT_NEAR(scroll_y, 0.0f, 0.001f);
    forge_ui_ctx_panel_end(&ctx);
    panel_test_teardown(&ctx);
}

static void test_panel_begin_negative_scroll_sanitized(void)
{
    TEST("panel_begin: negative scroll_y sanitized to 0");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = -10.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    ASSERT_NEAR(scroll_y, 0.0f, 0.001f);
    forge_ui_ctx_panel_end(&ctx);
    panel_test_teardown(&ctx);
}

/* ── panel_begin / panel_end lifecycle ────────────────────────────────── */

static void test_panel_begin_sets_clip(void)
{
    TEST("panel_begin: sets has_clip=true and clip_rect to content area");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 10, 20, PANEL_W, 400 }, &scroll_y));
    ASSERT_TRUE(ctx.has_clip);
    ASSERT_TRUE(ctx._panel_active);
    /* Content rect should be inset by padding and title height */
    ASSERT_NEAR(ctx.clip_rect.x, 10.0f + FORGE_UI_PANEL_PADDING, 0.01f);
    ASSERT_NEAR(ctx.clip_rect.y, 20.0f + FORGE_UI_PANEL_TITLE_HEIGHT + FORGE_UI_PANEL_PADDING, 0.01f);
    /* Width = panel.w - 2*padding - scrollbar; height = panel.h - title - 2*padding */
    ASSERT_NEAR(ctx.clip_rect.w,
                PANEL_W - 2.0f * FORGE_UI_PANEL_PADDING - FORGE_UI_SCROLLBAR_WIDTH, 0.01f);
    ASSERT_NEAR(ctx.clip_rect.h,
                400.0f - FORGE_UI_PANEL_TITLE_HEIGHT - 2.0f * FORGE_UI_PANEL_PADDING, 0.01f);

    forge_ui_ctx_panel_end(&ctx);
    panel_test_teardown(&ctx);
}

static void test_panel_end_clears_clip(void)
{
    TEST("panel_end: clears has_clip and _panel_active");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    forge_ui_ctx_panel_end(&ctx);

    ASSERT_TRUE(!ctx.has_clip);
    ASSERT_TRUE(!ctx._panel_active);

    panel_test_teardown(&ctx);
}

static void test_panel_end_without_begin(void)
{
    TEST("panel_end: no-op when no panel is active");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    if (!panel_test_setup(&ctx)) return;

    int depth_before = ctx.layout_depth;
    forge_ui_ctx_panel_end(&ctx);  /* should be no-op */
    ASSERT_EQ_INT(ctx.layout_depth, depth_before);
    ASSERT_TRUE(!ctx._panel_active);

    panel_test_teardown(&ctx);
}

static void test_panel_end_clamps_scroll(void)
{
    TEST("panel_end: clamps scroll_y to [0, max_scroll]");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 9999.0f;  /* way too large */
    if (!panel_test_setup(&ctx)) return;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    /* No child widgets → content_height = 0 → max_scroll = 0 */
    forge_ui_ctx_panel_end(&ctx);

    ASSERT_NEAR(scroll_y, 0.0f, 0.001f);
    panel_test_teardown(&ctx);
}

static void test_panel_layout_push_pop_balanced(void)
{
    TEST("panel_begin/end: layout depth returns to starting value");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    int depth_before = ctx.layout_depth;
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    ASSERT_EQ_INT(ctx.layout_depth, depth_before + 1);
    forge_ui_ctx_panel_end(&ctx);
    ASSERT_EQ_INT(ctx.layout_depth, depth_before);

    panel_test_teardown(&ctx);
}

/* ── ctx_end safety net for missing panel_end ─────────────────────────── */

static void test_ctx_end_cleans_up_active_panel(void)
{
    TEST("ctx_end: detects active panel and cleans up");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    /* Deliberately skip panel_end */
    forge_ui_ctx_end(&ctx);

    /* After ctx_end, panel state should be cleaned up */
    ASSERT_TRUE(!ctx._panel_active);
    ASSERT_TRUE(!ctx.has_clip);
    /* Identity fields must be cleared so the pre-clamp check on the next
     * frame cannot match against a panel that was never properly closed. */
    ASSERT_EQ_U32(ctx._panel.id, FORGE_UI_ID_NONE);
    ASSERT_TRUE(ctx._panel.scroll_y == NULL);
    ASSERT_NEAR(ctx._panel.content_height, 0.0f, 0.001f);

    forge_ui_ctx_free(&ctx);
}

/* ── scroll offset in layout_next ─────────────────────────────────────── */

static void test_panel_scroll_offset_applied(void)
{
    TEST("layout_next: scroll offset shifts widget y position");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 50.0f;
    if (!panel_test_setup(&ctx)) return;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, 400 }, &scroll_y));

    /* Get the first widget rect — should be offset by -scroll_y */
    ForgeUiRect r = forge_ui_ctx_layout_next(&ctx, ITEM_H);
    float expected_y = ctx._panel.content_rect.y - scroll_y;
    ASSERT_NEAR(r.y, expected_y, 0.01f);

    forge_ui_ctx_panel_end(&ctx);
    panel_test_teardown(&ctx);
}

static void test_panel_scroll_zero_no_offset(void)
{
    TEST("layout_next: scroll_y=0 means no offset");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, 400 }, &scroll_y));
    ForgeUiRect r = forge_ui_ctx_layout_next(&ctx, ITEM_H);
    float expected_y = ctx._panel.content_rect.y;
    ASSERT_NEAR(r.y, expected_y, 0.01f);

    forge_ui_ctx_panel_end(&ctx);
    panel_test_teardown(&ctx);
}

/* ── panel_begin: layout_push failure rollback ────────────────────────── */

static void test_panel_begin_layout_stack_full(void)
{
    TEST("panel_begin: returns false when layout stack is full");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    /* Fill the layout stack to max */
    ForgeUiRect area = { 0, 0, 500, 500 };
    for (int i = 0; i < FORGE_UI_LAYOUT_MAX_DEPTH; i++) {
        ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, area,
                    FORGE_UI_LAYOUT_VERTICAL, 0, 0));
    }
    ASSERT_EQ_INT(ctx.layout_depth, FORGE_UI_LAYOUT_MAX_DEPTH);

    /* panel_begin should fail because it cannot push another layout */
    bool ok = forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y);
    ASSERT_TRUE(!ok);
    /* State should be rolled back: no clip, no active panel */
    ASSERT_TRUE(!ctx.has_clip);
    ASSERT_TRUE(!ctx._panel_active);
    /* Identity fields must be cleared so the pre-clamp check on the next
     * frame does not match against a panel that never completed. */
    ASSERT_EQ_U32(ctx._panel.id, FORGE_UI_ID_NONE);
    ASSERT_TRUE(ctx._panel.scroll_y == NULL);

    /* Clean up the stacked layouts */
    for (int i = 0; i < FORGE_UI_LAYOUT_MAX_DEPTH; i++) {
        forge_ui_ctx_layout_pop(&ctx);
    }
    panel_test_teardown(&ctx);
}

/* ── panel_end: scrollbar thumb clamp ─────────────────────────────────── */

static void test_panel_end_thumb_clamped_to_track(void)
{
    TEST("panel_end: thumb_h clamped to track_h on very short panels");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    /* Panel is just barely tall enough to have a content area
     * but the content area will be shorter than MIN_THUMB (20px) */
    float panel_h = FORGE_UI_PANEL_TITLE_HEIGHT + 2.0f * FORGE_UI_PANEL_PADDING + 15.0f;
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, 100, panel_h }, &scroll_y));
    /* Put a widget taller than visible area to force scrollbar */
    forge_ui_ctx_layout_next(&ctx, 200.0f);
    forge_ui_ctx_panel_end(&ctx);

    /* If the fix works, we shouldn't crash and scroll_y should be properly
     * clamped.  With a very short panel, content (200) far exceeds visible
     * area (~15 px), so max_scroll > 0 and scroll_y remains at 0. */
    ASSERT_NEAR(scroll_y, 0.0f, 0.01f);
    /* Panel should have been cleanly closed */
    ASSERT_TRUE(!ctx._panel_active);
    ASSERT_TRUE(!ctx.has_clip);

    panel_test_teardown(&ctx);
}

/* ── ctx_free clears panel fields ─────────────────────────────────────── */

static void test_free_clears_panel_fields(void)
{
    TEST("ctx_free: zeroes panel-related fields");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    float scroll_y = 0.0f;
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_free(&ctx);

    ASSERT_TRUE(!ctx.has_clip);
    ASSERT_TRUE(!ctx._panel_active);
    ASSERT_NEAR(ctx.scroll_delta, 0.0f, 0.001f);
    ASSERT_TRUE(ctx._panel.scroll_y == NULL);
    ASSERT_NEAR(ctx._panel_content_start_y, 0.0f, 0.001f);
}

/* ── panel_begin: null title is rejected (title is the ID) ────────────── */

static void test_panel_begin_null_title_ok(void)
{
    TEST("panel_begin: null title returns false (title is required as ID)");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, NULL,
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    panel_test_teardown(&ctx);
}

/* ── panel_begin: emits draw data ─────────────────────────────────────── */

static void test_panel_begin_emits_draw_data(void)
{
    TEST("panel_begin: emits background and title bar quads");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    int v_before = ctx.vertex_count;
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Title",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    /* Should emit at least bg (4v) + title bar (4v) + title text */
    ASSERT_TRUE(ctx.vertex_count > v_before + 8);

    forge_ui_ctx_panel_end(&ctx);
    panel_test_teardown(&ctx);
}

/* ── panel_end: scrollbar drawn when content overflows ────────────────── */

static void test_panel_end_scrollbar_on_overflow(void)
{
    TEST("panel_end: draws scrollbar when content overflows");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, SMALL_PANEL_H }, &scroll_y));
    /* Emit many widgets to overflow the panel */
    for (int i = 0; i < 20; i++) {
        forge_ui_ctx_layout_next(&ctx, ITEM_H);
    }
    int v_before_end = ctx.vertex_count;
    forge_ui_ctx_panel_end(&ctx);
    /* panel_end should emit scrollbar track + thumb quads */
    ASSERT_TRUE(ctx.vertex_count > v_before_end);

    panel_test_teardown(&ctx);
}

static void test_panel_end_no_scrollbar_zero_track(void)
{
    TEST("panel_end: no scrollbar when track height is too small");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    /* Panel barely has room for the title bar — content_rect.h ≈ 0.
     * title=30 + 2*pad=20 = 50, so a 51px panel gives ~1px content.
     * A 50px panel gives 0px content. */
    float panel_h = FORGE_UI_PANEL_TITLE_HEIGHT + 2.0f * FORGE_UI_PANEL_PADDING;
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "T",
                (ForgeUiRect){ PANEL_X, PANEL_Y, 100, panel_h }, &scroll_y));
    /* Place a widget to force overflow */
    forge_ui_ctx_layout_next(&ctx, 100.0f);
    int v_before_end = ctx.vertex_count;
    forge_ui_ctx_panel_end(&ctx);
    /* Track is 0px tall — scrollbar should be skipped entirely */
    ASSERT_EQ_INT(ctx.vertex_count, v_before_end);

    panel_test_teardown(&ctx);
}

static void test_panel_end_no_scrollbar_when_fits(void)
{
    TEST("panel_end: no scrollbar when content fits");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, 500 }, &scroll_y));
    /* Single small widget that fits */
    forge_ui_ctx_layout_next(&ctx, 20.0f);
    int v_before_end = ctx.vertex_count;
    forge_ui_ctx_panel_end(&ctx);
    /* No overflow → no scrollbar quads */
    ASSERT_EQ_INT(ctx.vertex_count, v_before_end);

    panel_test_teardown(&ctx);
}

/* ── panel mouse wheel scrolling ──────────────────────────────────────── */

static void test_panel_mouse_wheel_scroll(void)
{
    TEST("panel_begin: mouse wheel delta applies scroll when mouse in content");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Position mouse inside where the content area will be */
    float content_x = 10.0f + FORGE_UI_PANEL_PADDING + 5.0f;
    float content_y = 20.0f + FORGE_UI_PANEL_TITLE_HEIGHT + FORGE_UI_PANEL_PADDING + 5.0f;
    forge_ui_ctx_begin(&ctx, content_x, content_y, false);
    ctx.scroll_delta = 2.0f;  /* scroll down */

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 10, 20, PANEL_W, 400 }, &scroll_y));
    /* scroll_y should be updated by delta * speed */
    ASSERT_NEAR(scroll_y, 2.0f * FORGE_UI_SCROLL_SPEED, 0.01f);

    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_panel_mouse_wheel_nan_ignored(void)
{
    TEST("panel_begin: NaN scroll_delta does not modify scroll_y");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 50.0f;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Position mouse inside the content area */
    float content_x = 10.0f + FORGE_UI_PANEL_PADDING + 5.0f;
    float content_y = 20.0f + FORGE_UI_PANEL_TITLE_HEIGHT + FORGE_UI_PANEL_PADDING + 5.0f;
    forge_ui_ctx_begin(&ctx, content_x, content_y, false);
    ctx.scroll_delta = NAN;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 10, 20, PANEL_W, 400 }, &scroll_y));
    /* NaN delta should be rejected; scroll_y stays at 50 */
    ASSERT_NEAR(scroll_y, 50.0f, 0.01f);

    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_panel_mouse_wheel_inf_ignored(void)
{
    TEST("panel_begin: +Inf scroll_delta does not modify scroll_y");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 50.0f;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Position mouse inside the content area */
    float content_x = 10.0f + FORGE_UI_PANEL_PADDING + 5.0f;
    float content_y = 20.0f + FORGE_UI_PANEL_TITLE_HEIGHT + FORGE_UI_PANEL_PADDING + 5.0f;
    forge_ui_ctx_begin(&ctx, content_x, content_y, false);
    ctx.scroll_delta = INFINITY;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 10, 20, PANEL_W, 400 }, &scroll_y));
    /* +Inf delta should be rejected; scroll_y stays at 50 */
    ASSERT_NEAR(scroll_y, 50.0f, 0.01f);

    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_panel_mouse_wheel_neg_inf_ignored(void)
{
    TEST("panel_begin: -Inf scroll_delta does not modify scroll_y");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 50.0f;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float content_x = 10.0f + FORGE_UI_PANEL_PADDING + 5.0f;
    float content_y = 20.0f + FORGE_UI_PANEL_TITLE_HEIGHT + FORGE_UI_PANEL_PADDING + 5.0f;
    forge_ui_ctx_begin(&ctx, content_x, content_y, false);
    ctx.scroll_delta = -INFINITY;

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 10, 20, PANEL_W, 400 }, &scroll_y));
    /* -Inf delta should be rejected; scroll_y stays at 50 */
    ASSERT_NEAR(scroll_y, 50.0f, 0.01f);

    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Text input: clipped visibility suppresses keyboard input ─────────────── */

static void test_text_input_clipped_ignores_keyboard(void)
{
    TEST("text_input: keyboard input suppressed when clipped out of view");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Frame 1: click to focus the text input */
    ForgeUiRect ti_rect = { 20, 80, 200, 30 };
    char buf[64] = "hello";
    ForgeUiTextInputState state = { buf, 64, 5, 5 };

    forge_ui_ctx_begin(&ctx, 120.0f, 95.0f, true);  /* mouse inside ti_rect */
    forge_ui_ctx_text_input(&ctx, "##field", &state, ti_rect, true);
    forge_ui_ctx_end(&ctx);

    /* Frame 2: release inside → acquires focus */
    forge_ui_ctx_begin(&ctx, 120.0f, 95.0f, false);
    forge_ui_ctx_text_input(&ctx, "##field", &state, ti_rect, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, forge_ui_hash_id(&ctx, "##field"));

    /* Frame 3: set clip rect that excludes the text input, type a character */
    forge_ui_ctx_begin(&ctx, 120.0f, 95.0f, false);
    ctx.has_clip = true;
    ctx.clip_rect = (ForgeUiRect){ 0, 0, 50, 50 };  /* ti_rect is outside */
    ctx.text_input = "X";
    forge_ui_ctx_text_input(&ctx, "##field", &state, ti_rect, true);
    forge_ui_ctx_end(&ctx);

    /* Buffer should be unchanged -- keyboard input was suppressed */
    ASSERT_EQ_INT(state.length, 5);
    ASSERT_TRUE(SDL_strcmp(buf, "hello") == 0);
    /* Focus should be preserved so it works again when scrolled back */
    ASSERT_EQ_U32(ctx.focused, forge_ui_hash_id(&ctx, "##field"));

    forge_ui_ctx_free(&ctx);
}

static void test_text_input_partially_visible_accepts_keyboard(void)
{
    TEST("text_input: keyboard input accepted when partially visible in clip");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Frame 1: focus the text input */
    ForgeUiRect ti_rect = { 20, 40, 200, 30 };
    char buf[64] = "hi";
    ForgeUiTextInputState state = { buf, 64, 2, 2 };

    forge_ui_ctx_begin(&ctx, 120.0f, 55.0f, true);
    forge_ui_ctx_text_input(&ctx, "##field", &state, ti_rect, true);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_begin(&ctx, 120.0f, 55.0f, false);
    forge_ui_ctx_text_input(&ctx, "##field", &state, ti_rect, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, forge_ui_hash_id(&ctx, "##field"));

    /* Frame 3: clip rect overlaps ti_rect partially (clip top half) */
    forge_ui_ctx_begin(&ctx, 120.0f, 55.0f, false);
    ctx.has_clip = true;
    ctx.clip_rect = (ForgeUiRect){ 0, 0, 300, 55 };  /* covers y 0-55, ti is 40-70 */
    ctx.text_input = "!";
    forge_ui_ctx_text_input(&ctx, "##field", &state, ti_rect, true);
    forge_ui_ctx_end(&ctx);

    /* Partially visible → input accepted */
    ASSERT_EQ_INT(state.length, 3);
    ASSERT_TRUE(SDL_strcmp(buf, "hi!") == 0);

    forge_ui_ctx_free(&ctx);
}

/* ── Scroll pre-clamp: content shrinkage ──────────────────────────────────── */

static void test_panel_scroll_preclamp_on_panel_resize(void)
{
    TEST("panel_begin: pre-clamps scroll_y when visible area grows");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Frame 1: small panel (200px) with enough content to scroll */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "List",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, SMALL_PANEL_H }, &scroll_y));
    /* 10 items × (30+8 spacing) ≈ 370px content in ~150px visible area.
     * max_scroll ≈ 220. */
    for (int i = 0; i < 10; i++) {
        forge_ui_ctx_layout_next(&ctx, ITEM_H);
    }
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    /* Scroll to 200px (valid for this panel size) */
    scroll_y = 200.0f;

    /* Frame 2: panel grows to 600px — visible area is now ~560px.
     * Same 10 items (~370px) now FIT entirely, so max_scroll = 0.
     * The pre-clamp uses prev content_height (~370) and new visible_h
     * (~560): prev_max = 370-560 = negative → 0.  scroll_y (200) > 0,
     * so it clamps to 0 immediately — no blank-space flash. */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "List",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, 600 }, &scroll_y));

    /* scroll_y should already be 0 thanks to pre-clamp */
    ASSERT_NEAR(scroll_y, 0.0f, 0.01f);

    /* First widget should be at content area top, not offset */
    ForgeUiRect r = forge_ui_ctx_layout_next(&ctx, ITEM_H);
    ASSERT_NEAR(r.y, ctx._panel.content_rect.y, 0.01f);

    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_panel_preclamp_skips_different_panel(void)
{
    TEST("panel_begin: pre-clamp skips when cached state belongs to another panel");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_a = 0.0f;
    float scroll_b = 0.0f;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Frame 1: panel A (tall content → scrollable), then panel B (short). */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "A",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, SMALL_PANEL_H }, &scroll_a));
    for (int i = 0; i < 15; i++)          /* ~570px content in ~150px view */
        forge_ui_ctx_layout_next(&ctx, ITEM_H);
    forge_ui_ctx_panel_end(&ctx);

    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "B",
                (ForgeUiRect){ 310, PANEL_Y, PANEL_W, SMALL_PANEL_H }, &scroll_b));
    forge_ui_ctx_layout_next(&ctx, ITEM_H); /* little content — max_scroll ≈ 0 */
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    /* Between frames: set A's scroll to a value that is valid for A's
     * content (~570px) but would be clamped to 0 if the pre-clamp
     * mistakenly used B's content_height (~30px). */
    scroll_a = 100.0f;

    /* Frame 2: open panel A first.  _panel still holds B's state from
     * the last panel_end.  The pre-clamp must detect the id mismatch
     * (20 != 10) and skip, preserving scroll_a = 100. */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "A",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, SMALL_PANEL_H }, &scroll_a));

    /* scroll_a should remain 100 — NOT clamped by B's tiny content */
    ASSERT_NEAR(scroll_a, 100.0f, 0.01f);

    for (int i = 0; i < 15; i++)
        forge_ui_ctx_layout_next(&ctx, ITEM_H);
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_panel_scroll_content_shrink_one_frame_lag(void)
{
    TEST("panel_end: clamps scroll_y after content shrinks (one-frame lag)");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Frame 1: lots of content, scroll to 400px */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "List",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, SMALL_PANEL_H }, &scroll_y));
    for (int i = 0; i < 30; i++) {
        forge_ui_ctx_layout_next(&ctx, ITEM_H);
    }
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    scroll_y = 400.0f;

    /* Frame 2: content shrinks to 3 items.  The pre-clamp cannot help
     * here because it uses the previous frame's (large) content_height.
     * But panel_end WILL clamp scroll_y to the new max_scroll. */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "List",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, SMALL_PANEL_H }, &scroll_y));
    for (int i = 0; i < 3; i++) {
        forge_ui_ctx_layout_next(&ctx, ITEM_H);
    }
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    /* After panel_end's final clamp, scroll_y should be 0
     * (3 items fit within the visible area) */
    ASSERT_NEAR(scroll_y, 0.0f, 0.01f);

    /* Frame 3: with scroll_y now 0, content is properly visible */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "List",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, SMALL_PANEL_H }, &scroll_y));
    ForgeUiRect r = forge_ui_ctx_layout_next(&ctx, ITEM_H);
    ASSERT_NEAR(r.y, ctx._panel.content_rect.y, 0.01f);  /* visible */
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_free(&ctx);
}

/* ── Additional validation tests (review pass) ───────────────────────────── */

static void test_panel_begin_nan_width_rejected(void)
{
    TEST("panel_begin: NaN width rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 0, 0, NAN, 200 }, &scroll_y));
    ASSERT_TRUE(!ctx._panel_active);
    panel_test_teardown(&ctx);
}

static void test_panel_begin_nan_height_rejected(void)
{
    TEST("panel_begin: NaN height rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 0, 0, 200, NAN }, &scroll_y));
    ASSERT_TRUE(!ctx._panel_active);
    panel_test_teardown(&ctx);
}

static void test_panel_begin_nan_x_rejected(void)
{
    TEST("panel_begin: NaN x rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ NAN, 0, 200, 200 }, &scroll_y));
    ASSERT_TRUE(!ctx._panel_active);
    panel_test_teardown(&ctx);
}

static void test_panel_begin_inf_x_rejected(void)
{
    TEST("panel_begin: +Inf x rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ INFINITY, 0, 200, 200 }, &scroll_y));
    ASSERT_TRUE(!ctx._panel_active);
    panel_test_teardown(&ctx);
}

static void test_panel_begin_nan_y_rejected(void)
{
    TEST("panel_begin: NaN y rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 0, NAN, 200, 200 }, &scroll_y));
    ASSERT_TRUE(!ctx._panel_active);
    panel_test_teardown(&ctx);
}

static void test_panel_begin_inf_y_rejected(void)
{
    TEST("panel_begin: +Inf y rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ 0, INFINITY, 200, 200 }, &scroll_y));
    ASSERT_TRUE(!ctx._panel_active);
    panel_test_teardown(&ctx);
}

static void test_emit_quad_clipped_zero_height(void)
{
    TEST("emit_quad_clipped: zero-height quad emits nothing");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);

    /* Zero-height quad: y0 == y1 */
    ForgeUiVertex src[4] = {
        { 10, 50, 0.0f, 0.0f, 1,1,1,1 },
        { 30, 50, 1.0f, 0.0f, 1,1,1,1 },
        { 30, 50, 1.0f, 1.0f, 1,1,1,1 },
        { 10, 50, 0.0f, 1.0f, 1,1,1,1 }
    };
    ForgeUiRect clip = { 0.0f, 0.0f, 100.0f, 100.0f };
    int before = ctx.vertex_count;
    forge_ui__emit_quad_clipped(&ctx, src, &clip);
    ASSERT_EQ_INT(ctx.vertex_count, before);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_panel_begin_long_title_ok(void)
{
    TEST("panel_begin: long title accepted and hashed correctly");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    if (!panel_test_setup(&ctx)) return;

    /* Any non-empty title string is valid; the hash serves as the ID.
     * Compute expected hash BEFORE panel_begin, because panel_begin
     * pushes a scope that changes the seed stack. */
    Uint32 expected_id = forge_ui_hash_id(&ctx, "LongTitlePanel");
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "LongTitlePanel",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    ASSERT_TRUE(ctx._panel_active);
    ASSERT_EQ_U32(ctx._panel.id, expected_id);

    forge_ui_ctx_panel_end(&ctx);
    panel_test_teardown(&ctx);
}

static void test_ctx_begin_resets_panel_state(void)
{
    TEST("ctx_begin: resets panel-related state from previous frame");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    float scroll_y = 0.0f;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Frame 1: open and close a panel normally */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Test",
                (ForgeUiRect){ PANEL_X, PANEL_Y, PANEL_W, PANEL_H }, &scroll_y));
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    /* Manually corrupt panel state to simulate stale data */
    ctx._panel_active = true;
    ctx.has_clip = true;
    ctx._panel.scroll_y = &scroll_y;

    /* Frame 2: ctx_begin should reset all panel state */
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ASSERT_TRUE(!ctx._panel_active);
    ASSERT_TRUE(!ctx.has_clip);
    ASSERT_TRUE(ctx._panel.scroll_y == NULL);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Hash / ID helper tests ──────────────────────────────────────────────── */

static void test_hash_id_deterministic(void)
{
    TEST("hash_id: same label and same stack produce identical hashes");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    Uint32 h1 = forge_ui_hash_id(&ctx, "Save");
    Uint32 h2 = forge_ui_hash_id(&ctx, "Save");
    ASSERT_EQ_U32(h1, h2);
    ASSERT_TRUE(h1 != FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

static void test_hash_id_different_labels(void)
{
    TEST("hash_id: different labels produce different hashes");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    Uint32 h_save = forge_ui_hash_id(&ctx, "Save");
    Uint32 h_load = forge_ui_hash_id(&ctx, "Load");
    ASSERT_TRUE(h_save != h_load);

    forge_ui_ctx_free(&ctx);
}

static void test_hash_id_separator(void)
{
    TEST("hash_id: ## separator yields different hash from display text");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* "Save##file" and "Save##dialog" both display "Save" but must
     * hash differently because the ## portion differs. */
    Uint32 h_file   = forge_ui_hash_id(&ctx, "Save##file");
    Uint32 h_dialog = forge_ui_hash_id(&ctx, "Save##dialog");
    ASSERT_TRUE(h_file != h_dialog);

    /* Both must differ from plain "Save" (which hashes the whole label) */
    Uint32 h_plain = forge_ui_hash_id(&ctx, "Save");
    ASSERT_TRUE(h_file != h_plain);
    ASSERT_TRUE(h_dialog != h_plain);

    forge_ui_ctx_free(&ctx);
}

static void test_push_pop_id_scoping(void)
{
    TEST("push_id/pop_id: changes hash result, pop restores original");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Hash "OK" at root scope */
    Uint32 h_root = forge_ui_hash_id(&ctx, "OK");

    /* Push a scope — same label should now produce a different hash */
    forge_ui_push_id(&ctx, "panel_A");
    Uint32 h_scoped = forge_ui_hash_id(&ctx, "OK");
    ASSERT_TRUE(h_root != h_scoped);

    /* A different scope name should produce yet another hash */
    forge_ui_pop_id(&ctx);
    forge_ui_push_id(&ctx, "panel_B");
    Uint32 h_other_scope = forge_ui_hash_id(&ctx, "OK");
    ASSERT_TRUE(h_scoped != h_other_scope);

    /* Pop back to root — should match original */
    forge_ui_pop_id(&ctx);
    Uint32 h_restored = forge_ui_hash_id(&ctx, "OK");
    ASSERT_EQ_U32(h_root, h_restored);

    forge_ui_ctx_free(&ctx);
}

static void test_display_text_separator(void)
{
    TEST("display_end: returns pointer to ## separator or end of string");

    /* Label with ## separator */
    const char *label1 = "Save##file";
    const char *end1 = forge_ui__display_end(label1);
    /* end1 should point to the '#' in "##file" */
    ASSERT_TRUE(end1 == label1 + 4);
    /* Display length = end1 - label1 = 4 ("Save") */
    ASSERT_EQ_INT((int)(end1 - label1), 4);

    /* Label without separator — returns pointer to null terminator */
    const char *label2 = "OK";
    const char *end2 = forge_ui__display_end(label2);
    ASSERT_TRUE(end2 == label2 + 2);
    ASSERT_EQ_INT((int)(end2 - label2), 2);

    /* Empty display portion: "##hidden" → display length = 0 */
    const char *label3 = "##hidden";
    const char *end3 = forge_ui__display_end(label3);
    ASSERT_TRUE(end3 == label3);
    ASSERT_EQ_INT((int)(end3 - label3), 0);
}

/* ── Audit fix tests ─────────────────────────────────────────────────────── */

static void test_fnv1a_null_returns_seed(void)
{
    TEST("fnv1a: NULL str returns seed unchanged");
    Uint32 seed = TEST_FNV1A_OFFSET_BASIS;
    Uint32 result = forge_ui__fnv1a(NULL, seed);
    ASSERT_EQ_U32(result, seed);
}

static void test_fnv1a_empty_string_returns_seed(void)
{
    TEST("fnv1a: empty string returns seed unchanged");
    Uint32 seed = TEST_FNV1A_OFFSET_BASIS;
    Uint32 result = forge_ui__fnv1a("", seed);
    ASSERT_EQ_U32(result, seed);
}

static void test_fnv1a_different_strings_differ(void)
{
    TEST("fnv1a: different strings produce different hashes");
    Uint32 seed = TEST_FNV1A_OFFSET_BASIS;
    Uint32 h1 = forge_ui__fnv1a("hello", seed);
    Uint32 h2 = forge_ui__fnv1a("world", seed);
    ASSERT_TRUE(h1 != h2);
}

static void test_fnv1a_same_string_same_hash(void)
{
    TEST("fnv1a: same string produces same hash");
    Uint32 seed = TEST_FNV1A_ALT_SEED;
    Uint32 h1 = forge_ui__fnv1a("test", seed);
    Uint32 h2 = forge_ui__fnv1a("test", seed);
    ASSERT_EQ_U32(h1, h2);
}

static void test_fnv1a_different_seeds_differ(void)
{
    TEST("fnv1a: same string with different seeds produces different hashes");
    Uint32 h1 = forge_ui__fnv1a("test", TEST_FNV1A_OFFSET_BASIS);
    Uint32 h2 = forge_ui__fnv1a("test", TEST_FNV1A_MIN_SEED);
    ASSERT_TRUE(h1 != h2);
}

static void test_display_end_null_returns_null(void)
{
    TEST("display_end: NULL returns NULL");
    const char *result = forge_ui__display_end(NULL);
    ASSERT_TRUE(result == NULL);
}

static void test_display_end_no_separator(void)
{
    TEST("display_end: string without ## returns end");
    const char *label = "Hello";
    const char *result = forge_ui__display_end(label);
    ASSERT_TRUE(result == label + 5);
}

static void test_display_end_with_separator(void)
{
    TEST("display_end: string with ## returns pointer to ##");
    const char *label = "Save##file";
    const char *result = forge_ui__display_end(label);
    ASSERT_TRUE(result == label + 4);
    ASSERT_EQ_INT((int)(result - label), 4);
}

static void test_display_end_double_separator(void)
{
    TEST("display_end: string with two ## returns first");
    const char *label = "A##B##C";
    const char *result = forge_ui__display_end(label);
    ASSERT_TRUE(result == label + 1);
    ASSERT_EQ_INT((int)(result - label), 1);
}

static void test_display_end_only_separator(void)
{
    TEST("display_end: '##' only returns pointer to start");
    const char *label = "##";
    const char *result = forge_ui__display_end(label);
    ASSERT_TRUE(result == label);
    ASSERT_EQ_INT((int)(result - label), 0);
}

static void test_display_end_empty_string(void)
{
    TEST("display_end: empty string returns pointer to end");
    const char *label = "";
    const char *result = forge_ui__display_end(label);
    ASSERT_TRUE(result == label);
}

static void test_hash_id_null_label_returns_one(void)
{
    TEST("hash_id: NULL label returns 1");
    Uint32 result = forge_ui_hash_id(NULL, NULL);
    ASSERT_EQ_U32(result, 1u);
}

static void test_hash_id_empty_label_returns_one(void)
{
    TEST("hash_id: empty label returns 1");
    Uint32 result = forge_ui_hash_id(NULL, "");
    ASSERT_EQ_U32(result, 1u);
}

static void test_hash_id_null_ctx_uses_default_seed(void)
{
    TEST("hash_id: NULL ctx uses default FNV offset basis");
    Uint32 h1 = forge_ui_hash_id(NULL, "Save");
    ASSERT_TRUE(h1 != 0);
    ASSERT_TRUE(h1 != 1);
    /* Should be deterministic */
    Uint32 h2 = forge_ui_hash_id(NULL, "Save");
    ASSERT_EQ_U32(h1, h2);
}

static void test_hash_id_separator_changes_hash(void)
{
    TEST("hash_id: ## separator hashes only suffix portion");
    Uint32 h_plain = forge_ui_hash_id(NULL, "Save");
    Uint32 h_sep   = forge_ui_hash_id(NULL, "Save##file");
    Uint32 h_sep2  = forge_ui_hash_id(NULL, "Load##file");
    /* "Save" and "Save##file" should differ (different hash input) */
    ASSERT_TRUE(h_plain != h_sep);
    /* "Save##file" and "Load##file" should be the same (both hash "##file") */
    ASSERT_EQ_U32(h_sep, h_sep2);
}

static void test_push_id_null_ctx_no_crash(void)
{
    TEST("push_id: NULL ctx does not crash");
    forge_ui_push_id(NULL, "scope");
    /* Should not crash — just returns early */
    /* no crash = pass */
}

static void test_pop_id_null_ctx_no_crash(void)
{
    TEST("pop_id: NULL ctx does not crash");
    forge_ui_pop_id(NULL);
    /* no crash = pass */
}

static void test_pop_id_underflow_no_crash(void)
{
    TEST("pop_id: underflow does not crash");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);
    /* Pop on empty stack — should log warning and return safely */
    forge_ui_pop_id(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);
    forge_ui_ctx_free(&ctx);
}

static void test_push_id_overflow_no_crash(void)
{
    TEST("push_id: overflow at max depth logs and returns safely");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    /* Push to max depth */
    for (int i = 0; i < FORGE_UI_ID_STACK_MAX_DEPTH; i++) {
        forge_ui_push_id(&ctx, "scope");
    }
    ASSERT_EQ_INT(ctx.id_stack_depth, FORGE_UI_ID_STACK_MAX_DEPTH);
    /* One more should fail gracefully */
    forge_ui_push_id(&ctx, "overflow");
    ASSERT_EQ_INT(ctx.id_stack_depth, FORGE_UI_ID_STACK_MAX_DEPTH);
    /* Pop all to clean up */
    for (int i = 0; i < FORGE_UI_ID_STACK_MAX_DEPTH; i++) {
        forge_ui_pop_id(&ctx);
    }
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);
    forge_ui_ctx_free(&ctx);
}

static void test_push_id_changes_hash(void)
{
    TEST("push_id: changes hash_id result for same label");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    Uint32 h_root = forge_ui_hash_id(&ctx, "Button");
    forge_ui_push_id(&ctx, "Window");
    Uint32 h_scoped = forge_ui_hash_id(&ctx, "Button");
    ASSERT_TRUE(h_root != h_scoped);
    forge_ui_pop_id(&ctx);
    Uint32 h_restored = forge_ui_hash_id(&ctx, "Button");
    ASSERT_EQ_U32(h_root, h_restored);
    forge_ui_ctx_free(&ctx);
}

static void test_ctx_begin_resets_id_stack(void)
{
    TEST("ctx_begin: resets id_stack_depth to 0 if leaked");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    /* Simulate a leaked push (no matching pop) */
    forge_ui_push_id(&ctx, "leaked");
    ASSERT_EQ_INT(ctx.id_stack_depth, 1);
    /* Begin a new frame — should reset */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_panel_title_strips_separator(void)
{
    TEST("panel_begin: title with ## only displays text before ##");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    float scroll_y = 0.0f;
    ForgeUiRect rect = { TEST_SEP_PANEL_X, TEST_SEP_PANEL_Y,
                         TEST_SEP_PANEL_W, TEST_SEP_PANEL_H };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    bool ok = forge_ui_ctx_panel_begin(&ctx, "Controls##p1", rect, &scroll_y);
    ASSERT_TRUE(ok);
    /* The panel should have a valid hashed ID */
    ASSERT_TRUE(ctx._panel.id != FORGE_UI_ID_NONE);
    /* The ID should use the ## convention — hash("##p1") not hash("Controls##p1") */
    Uint32 expected_id = forge_ui_hash_id(NULL, "Controls##p1");
    /* Note: panel ID is computed BEFORE push_id, so it uses the root seed */
    ASSERT_EQ_U32(ctx._panel.id, expected_id);
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_panel_separator_different_ids(void)
{
    TEST("panel_begin: panels with same display text but different ## suffixes get different IDs");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    float scroll_y = 0.0f;
    ForgeUiRect rect = { TEST_SEP_PANEL_X, TEST_SEP_PANEL_Y,
                         TEST_SEP_PANEL_W, TEST_SEP_PANEL_H };

    /* Panel with ##p1 */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Settings##p1", rect, &scroll_y));
    Uint32 id1 = ctx._panel.id;
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    /* Panel with ##p2 — same display text, different ID */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "Settings##p2", rect, &scroll_y));
    Uint32 id2 = ctx._panel.id;
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(id1 != id2);
    forge_ui_ctx_free(&ctx);
}

static void test_button_separator_display_text(void)
{
    TEST("button: ## separator does not appear in rendered text");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiRect rect = { TEST_SEP_BTN_X, TEST_SEP_BTN_Y,
                         TEST_SEP_BTN_W, TEST_SEP_BTN_H };

    /* Render a button with separator — "OK##dialog1" */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_ctx_button(&ctx, "OK##dialog1", rect);
    int verts_sep = ctx.vertex_count;
    forge_ui_ctx_end(&ctx);

    /* Render a button with just "OK" (no separator) */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_ctx_button(&ctx, "OK", rect);
    int verts_plain = ctx.vertex_count;
    forge_ui_ctx_end(&ctx);

    /* Both should emit the same number of vertices (same display text "OK") */
    ASSERT_EQ_INT(verts_sep, verts_plain);
    forge_ui_ctx_free(&ctx);
}

static void test_button_separator_different_ids(void)
{
    TEST("button: same display text with different ## suffix gets different IDs");
    Uint32 h1 = forge_ui_hash_id(NULL, "Delete##audio");
    Uint32 h2 = forge_ui_hash_id(NULL, "Delete##video");
    ASSERT_TRUE(h1 != h2);
}

/* ── NaN/Inf validation ──────────────────────────────────────────────── */

static void test_label_nan_x_rejected(void)
{
    TEST("label: NaN x coordinate produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_ctx_label_colored(&ctx, "Test", NAN, 10.0f, 1, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_inf_y_rejected(void)
{
    TEST("label: Inf y coordinate produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_ctx_label_colored(&ctx, "Test", 10.0f, INFINITY, 1, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_button_nan_rect_x_rejected(void)
{
    TEST("button: NaN rect.x returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiRect rect = { NAN, 10.0f, 100.0f, 30.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_button(&ctx, "OK", rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_button_inf_rect_w_rejected(void)
{
    TEST("button: Inf rect.w returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiRect rect = { 10.0f, 10.0f, INFINITY, 30.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_button(&ctx, "OK", rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_button_neg_inf_rect_h_rejected(void)
{
    TEST("button: -Inf rect.h returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, -INFINITY };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_button(&ctx, "OK", rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_nan_rect_rejected(void)
{
    TEST("checkbox: NaN rect.y returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    bool val = false;
    ForgeUiRect rect = { 10.0f, NAN, 200.0f, 28.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_checkbox(&ctx, "Enable", &val, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_inf_rect_rejected(void)
{
    TEST("checkbox: Inf rect.w returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    bool val = false;
    ForgeUiRect rect = { 10.0f, 10.0f, INFINITY, 28.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_checkbox(&ctx, "Enable", &val, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_nan_rect_rejected(void)
{
    TEST("slider: NaN rect.x returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    float val = 0.5f;
    ForgeUiRect rect = { NAN, 10.0f, 200.0f, 28.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_slider(&ctx, "Volume", &val, 0.0f, 1.0f, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_inf_rect_rejected(void)
{
    TEST("slider: Inf rect.h returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    float val = 0.5f;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, INFINITY };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_slider(&ctx, "Volume", &val, 0.0f, 1.0f, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_nan_value_sanitized(void)
{
    TEST("slider: NaN *value is sanitized to 0");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    float val = NAN;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 28.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_ctx_slider(&ctx, "Volume", &val, 0.0f, 1.0f, rect);
    ASSERT_TRUE(isfinite(val));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_inf_value_sanitized(void)
{
    TEST("slider: Inf *value is sanitized to finite");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    float val = INFINITY;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 28.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_ctx_slider(&ctx, "Volume", &val, 0.0f, 1.0f, rect);
    ASSERT_TRUE(isfinite(val));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_nan_rect_rejected(void)
{
    TEST("text_input: NaN rect.x returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    char buf[32] = "";
    ForgeUiTextInputState tis = { buf, 32, 0, 0 };
    ForgeUiRect rect = { NAN, 10.0f, 200.0f, 28.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##user", &tis, rect, true));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_inf_rect_rejected(void)
{
    TEST("text_input: Inf rect.w returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    char buf[32] = "";
    ForgeUiTextInputState tis = { buf, 32, 0, 0 };
    ForgeUiRect rect = { 10.0f, 10.0f, INFINITY, 28.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, "##user", &tis, rect, true));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_push_nan_rect_rejected(void)
{
    TEST("layout_push: NaN rect.x returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiRect rect = { NAN, 10.0f, 400.0f, 300.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_layout_push(&ctx, rect, FORGE_UI_LAYOUT_VERTICAL, 8.0f, 4.0f));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_layout_push_inf_rect_rejected(void)
{
    TEST("layout_push: Inf rect.w returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ForgeUiRect rect = { 10.0f, 10.0f, INFINITY, 300.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_layout_push(&ctx, rect, FORGE_UI_LAYOUT_VERTICAL, 8.0f, 4.0f));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Scale and spacing validation (Phase 2 audit) ────────────────────────── */

#define TEST_SCALE_PANEL_X       10.0f
#define TEST_SCALE_PANEL_Y       10.0f
#define TEST_SCALE_PANEL_W      300.0f
#define TEST_SCALE_PANEL_H      200.0f

static void test_begin_resets_zero_scale(void)
{
    TEST("begin: zero scale is reset to 1.0");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ctx.scale = 0.0f;
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_NEAR(ctx.scale, 1.0f, 0.001f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_begin_resets_negative_scale(void)
{
    TEST("begin: negative scale is reset to 1.0");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ctx.scale = -2.0f;
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_NEAR(ctx.scale, 1.0f, 0.001f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_begin_resets_nan_scale(void)
{
    TEST("begin: NaN scale is reset to 1.0");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ctx.scale = NAN;
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_NEAR(ctx.scale, 1.0f, 0.001f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_begin_resets_inf_scale(void)
{
    TEST("begin: Inf scale is reset to 1.0");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ctx.scale = INFINITY;
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_NEAR(ctx.scale, 1.0f, 0.001f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_begin_preserves_valid_scale(void)
{
    TEST("begin: valid scale (1.5) is preserved");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    ctx.scale = 1.5f;
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_NEAR(ctx.scale, 1.5f, 0.001f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_init_zero_pixel_height_rejected(void)
{
    TEST("init: atlas with pixel_height=0 is rejected");
    ForgeUiFontAtlas fake;
    SDL_memset(&fake, 0, sizeof(fake));
    fake.pixel_height = 0.0f;
    fake.units_per_em = 1000;
    ForgeUiContext ctx;
    ASSERT_TRUE(!forge_ui_ctx_init(&ctx, &fake));
}

static void test_init_nan_pixel_height_rejected(void)
{
    TEST("init: atlas with pixel_height=NaN is rejected");
    ForgeUiFontAtlas fake;
    SDL_memset(&fake, 0, sizeof(fake));
    fake.pixel_height = NAN;
    fake.units_per_em = 1000;
    ForgeUiContext ctx;
    ASSERT_TRUE(!forge_ui_ctx_init(&ctx, &fake));
}

static void test_init_inf_pixel_height_rejected(void)
{
    TEST("init: atlas with pixel_height=Inf is rejected");
    ForgeUiFontAtlas fake;
    SDL_memset(&fake, 0, sizeof(fake));
    fake.pixel_height = INFINITY;
    fake.units_per_em = 1000;
    ForgeUiContext ctx;
    ASSERT_TRUE(!forge_ui_ctx_init(&ctx, &fake));
}

static void test_init_negative_pixel_height_rejected(void)
{
    TEST("init: atlas with pixel_height=-16 is rejected");
    ForgeUiFontAtlas fake;
    SDL_memset(&fake, 0, sizeof(fake));
    fake.pixel_height = -16.0f;
    fake.units_per_em = 1000;
    ForgeUiContext ctx;
    ASSERT_TRUE(!forge_ui_ctx_init(&ctx, &fake));
}

/* ── State cleanup on early return (Phase 2 audit) ───────────────────────── */

static void test_panel_begin_failure_clears_panel_rect(void)
{
    TEST("panel_begin: push_id failure zeroes _panel.rect and content_rect");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    /* Fill the ID stack to force push_id failure */
    for (int i = 0; i < FORGE_UI_ID_STACK_MAX_DEPTH; i++) {
        forge_ui_push_id(&ctx, "x");
    }

    float scroll = 0.0f;
    ForgeUiRect rect = { TEST_SCALE_PANEL_X, TEST_SCALE_PANEL_Y,
                         TEST_SCALE_PANEL_W, TEST_SCALE_PANEL_H };
    /* panel_begin should fail because push_id will overflow */
    ASSERT_TRUE(!forge_ui_ctx_panel_begin(&ctx, "Panel", rect, &scroll));

    /* _panel.rect should be zeroed */
    ASSERT_NEAR(ctx._panel.rect.x, 0.0f, 0.001f);
    ASSERT_NEAR(ctx._panel.rect.y, 0.0f, 0.001f);
    ASSERT_NEAR(ctx._panel.rect.w, 0.0f, 0.001f);
    ASSERT_NEAR(ctx._panel.rect.h, 0.0f, 0.001f);
    ASSERT_TRUE(!ctx._panel_active);
    ASSERT_TRUE(ctx._panel.scroll_y == NULL);

    /* Pop the ID stack entries we pushed */
    for (int i = 0; i < FORGE_UI_ID_STACK_MAX_DEPTH; i++) {
        forge_ui_pop_id(&ctx);
    }

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_ctx_begin_zeroes_panel_geometry(void)
{
    TEST("ctx_begin: zeroes _panel.rect and content_rect from previous frame");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Frame 1: open a panel so _panel.rect gets written */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    float scroll = 0.0f;
    ForgeUiRect rect = { TEST_SCALE_PANEL_X, TEST_SCALE_PANEL_Y,
                         TEST_SCALE_PANEL_W, TEST_SCALE_PANEL_H };
    ASSERT_TRUE(forge_ui_ctx_panel_begin(&ctx, "MyPanel", rect, &scroll));
    forge_ui_ctx_panel_end(&ctx);
    forge_ui_ctx_end(&ctx);

    /* Frame 2: after begin, panel geometry should be zeroed */
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_NEAR(ctx._panel.rect.x, 0.0f, 0.001f);
    ASSERT_NEAR(ctx._panel.rect.y, 0.0f, 0.001f);
    ASSERT_NEAR(ctx._panel.content_rect.x, 0.0f, 0.001f);
    ASSERT_NEAR(ctx._panel.content_rect.y, 0.0f, 0.001f);
    ASSERT_TRUE(!ctx._panel_active);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_emit_border_nan_width_rejected(void)
{
    TEST("emit_border: NaN border_w is rejected (no vertices)");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 50.0f };
    forge_ui__emit_border(&ctx, rect, NAN, 1, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Phase 2 Check 3: Inf/NaN guard tests ──────────────────────────────── */

static void test_slider_inf_min_rejected(void)
{
    TEST("slider: Inf min_val returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    float val = 0.5f;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 28.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_slider(&ctx, "##s", &val, INFINITY, 1.0f, rect));
    ASSERT_TRUE(!forge_ui_ctx_slider(&ctx, "##s", &val, -INFINITY, 1.0f, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_inf_max_rejected(void)
{
    TEST("slider: Inf max_val returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    float val = 0.5f;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 28.0f };
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_TRUE(!forge_ui_ctx_slider(&ctx, "##s", &val, 0.0f, INFINITY, rect));
    ASSERT_TRUE(!forge_ui_ctx_slider(&ctx, "##s", &val, 0.0f, -INFINITY, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_slider_layout_inf_range_rejected(void)
{
    TEST("slider_layout: Inf min/max returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    float val = 0.5f;
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect lr = { 0, 0, 300, 400 };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, lr,
                FORGE_UI_LAYOUT_VERTICAL, -1, -1));
    ASSERT_TRUE(!forge_ui_ctx_slider_layout(&ctx, "##s", &val, INFINITY, 1.0f, 28.0f));
    ASSERT_TRUE(!forge_ui_ctx_slider_layout(&ctx, "##s", &val, 0.0f, INFINITY, 28.0f));
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_ascender_px_inf_pixel_height(void)
{
    TEST("ascender_px: Inf pixel_height returns 0");
    ForgeUiFontAtlas atlas;
    SDL_memset(&atlas, 0, sizeof(atlas));
    atlas.units_per_em = 1000;
    atlas.ascender = 800;
    atlas.pixel_height = INFINITY;
    float asc = forge_ui__ascender_px(&atlas);
    ASSERT_NEAR(asc, 0.0f, 0.001f);
}

static void test_ascender_px_nan_pixel_height(void)
{
    TEST("ascender_px: NaN pixel_height returns 0");
    ForgeUiFontAtlas atlas;
    SDL_memset(&atlas, 0, sizeof(atlas));
    atlas.units_per_em = 1000;
    atlas.ascender = 800;
    atlas.pixel_height = NAN;
    float asc = forge_ui__ascender_px(&atlas);
    ASSERT_NEAR(asc, 0.0f, 0.001f);
}

/* ── label_colored RGBA NaN/Inf validation ─────────────────────────────── */

static void test_label_colored_nan_r_rejected(void)
{
    TEST("label_colored: NaN red channel produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    forge_ui_ctx_label_colored(&ctx, "Test", LABEL_TEST_X,
                               LABEL_TEST_Y, NAN, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_colored_nan_g_rejected(void)
{
    TEST("label_colored: NaN green channel produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    forge_ui_ctx_label_colored(&ctx, "Test", LABEL_TEST_X,
                               LABEL_TEST_Y, 1, NAN, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_colored_nan_b_rejected(void)
{
    TEST("label_colored: NaN blue channel produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    forge_ui_ctx_label_colored(&ctx, "Test", LABEL_TEST_X,
                               LABEL_TEST_Y, 1, 1, NAN, 1);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_colored_nan_a_rejected(void)
{
    TEST("label_colored: NaN alpha channel produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    forge_ui_ctx_label_colored(&ctx, "Test", LABEL_TEST_X,
                               LABEL_TEST_Y, 1, 1, 1, NAN);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_colored_inf_r_rejected(void)
{
    TEST("label_colored: Inf red channel produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    forge_ui_ctx_label_colored(&ctx, "Test", LABEL_TEST_X,
                               LABEL_TEST_Y, INFINITY, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_colored_inf_neg_g_rejected(void)
{
    TEST("label_colored: -Inf green channel produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    forge_ui_ctx_label_colored(&ctx, "Test", LABEL_TEST_X,
                               LABEL_TEST_Y, 1, -INFINITY, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_colored_out_of_range_clamped(void)
{
    TEST("label_colored: out-of-range color (r=2.0) clamped, produces vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    forge_ui_ctx_label_colored(&ctx, "AB", LABEL_TEST_X,
                               LABEL_TEST_Y, 2.0f, -0.5f, 1.5f, 1.0f);
    /* Out-of-range but finite values are clamped — vertices should be emitted.
     * "AB" = 2 visible glyphs -> 2*4 = 8 vertices */
    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_EQ_INT(ctx.vertex_count, 8);
    /* Verify channels are clamped to [0, 1] in the emitted vertex data.
     * Input was (2.0, -0.5, 1.5, 1.0) → expect (1.0, 0.0, 1.0, 1.0). */
    ASSERT_NEAR(ctx.vertices[0].r, 1.0f, 0.001f);
    ASSERT_NEAR(ctx.vertices[0].g, 0.0f, 0.001f);
    ASSERT_NEAR(ctx.vertices[0].b, 1.0f, 0.001f);
    ASSERT_NEAR(ctx.vertices[0].a, 1.0f, 0.001f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Solid rect tests ──────────────────────────────────────────────────── */

#define SR_TEST_X       10.0f   /* solid rect test x */
#define SR_TEST_Y       20.0f   /* solid rect test y */
#define SR_TEST_W      150.0f   /* solid rect test width */
#define SR_TEST_H       30.0f   /* solid rect test height */
#define SR_LAYOUT_SIZE  20.0f   /* layout size for rect_layout tests */
#define SR_LAYOUT_X     10.0f   /* layout rect x */
#define SR_LAYOUT_Y     10.0f   /* layout rect y */
#define SR_LAYOUT_W    300.0f   /* layout rect width */
#define SR_LAYOUT_H    200.0f   /* layout rect height */
#define SR_LAYOUT_PAD    4.0f   /* layout padding */
#define SR_LAYOUT_GAP    2.0f   /* layout spacing */

static void test_rect_basic(void)
{
    TEST("rect: emits exactly one quad");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor color = { 0.0f, 0.0f, 0.0f, 0.5f };
    ForgeUiRect rect = { SR_TEST_X, SR_TEST_Y, SR_TEST_W, SR_TEST_H };
    int idx_before = ctx.index_count;
    forge_ui_ctx_rect(&ctx, rect, color);
    /* One quad = 4 vertices, 6 indices */
    ASSERT_EQ_INT(ctx.vertex_count - before, 4);
    ASSERT_EQ_INT(ctx.index_count - idx_before, 6);
    /* Verify color is stored correctly */
    ASSERT_NEAR(ctx.vertices[before].r, 0.0f, 0.001f);
    ASSERT_NEAR(ctx.vertices[before].a, 0.5f, 0.001f);
    /* Verify corner positions: [0]=top-left, [2]=bottom-right */
    ASSERT_NEAR(ctx.vertices[before].pos_x, SR_TEST_X, 0.1f);
    ASSERT_NEAR(ctx.vertices[before].pos_y, SR_TEST_Y, 0.1f);
    ASSERT_NEAR(ctx.vertices[before + 2].pos_x, SR_TEST_X + SR_TEST_W, 0.1f);
    ASSERT_NEAR(ctx.vertices[before + 2].pos_y, SR_TEST_Y + SR_TEST_H, 0.1f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_rect_null_ctx(void)
{
    TEST("rect: NULL ctx does not crash");
    ForgeUiColor color = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { SR_TEST_X, SR_TEST_Y, SR_TEST_W, SR_TEST_H };
    forge_ui_ctx_rect(NULL, rect, color);
    /* no crash = pass */
}

static void test_rect_nan_rect_rejected(void)
{
    TEST("rect: NaN in rect produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor color = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { NAN, SR_TEST_Y, SR_TEST_W, SR_TEST_H };
    forge_ui_ctx_rect(&ctx, rect, color);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_rect_inf_rect_rejected(void)
{
    TEST("rect: Inf in rect produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor color = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { SR_TEST_X, SR_TEST_Y, INFINITY, SR_TEST_H };
    forge_ui_ctx_rect(&ctx, rect, color);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_rect_nan_color_rejected(void)
{
    TEST("rect: NaN in color produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor color = { NAN, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { SR_TEST_X, SR_TEST_Y, SR_TEST_W, SR_TEST_H };
    forge_ui_ctx_rect(&ctx, rect, color);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_rect_inf_color_rejected(void)
{
    TEST("rect: Inf in color produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor color = { 0.5f, INFINITY, 0.0f, 1.0f };
    ForgeUiRect rect = { SR_TEST_X, SR_TEST_Y, SR_TEST_W, SR_TEST_H };
    forge_ui_ctx_rect(&ctx, rect, color);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_rect_color_clamped(void)
{
    TEST("rect: out-of-range color components clamped to [0,1]");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor color = { 1.5f, -0.3f, 0.5f, 2.0f };
    ForgeUiRect rect = { SR_TEST_X, SR_TEST_Y, SR_TEST_W, SR_TEST_H };
    forge_ui_ctx_rect(&ctx, rect, color);
    ASSERT_EQ_INT(ctx.vertex_count - before, 4);
    ASSERT_NEAR(ctx.vertices[before].r, 1.0f, 0.001f);
    ASSERT_NEAR(ctx.vertices[before].g, 0.0f, 0.001f);
    ASSERT_NEAR(ctx.vertices[before].b, 0.5f, 0.001f);
    ASSERT_NEAR(ctx.vertices[before].a, 1.0f, 0.001f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_rect_layout_basic(void)
{
    TEST("rect_layout: emits one quad inside layout");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect layout_rect = { SR_LAYOUT_X, SR_LAYOUT_Y,
                                 SR_LAYOUT_W, SR_LAYOUT_H };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, layout_rect,
                              FORGE_UI_LAYOUT_VERTICAL,
                              SR_LAYOUT_PAD, SR_LAYOUT_GAP));
    int before = ctx.vertex_count;
    int idx_before = ctx.index_count;
    ForgeUiColor color = { 0.5f, 0.5f, 0.5f, 1.0f };
    forge_ui_ctx_rect_layout(&ctx, color, SR_LAYOUT_SIZE);
    ASSERT_EQ_INT(ctx.vertex_count - before, 4);
    ASSERT_EQ_INT(ctx.index_count - idx_before, 6);
    /* Verify the rect is positioned inside the layout area */
    ASSERT_NEAR(ctx.vertices[before].pos_x,
                SR_LAYOUT_X + SR_LAYOUT_PAD, 0.1f);
    ASSERT_NEAR(ctx.vertices[before].pos_y,
                SR_LAYOUT_Y + SR_LAYOUT_PAD, 0.1f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_rect_layout_no_layout(void)
{
    TEST("rect_layout: no active layout produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor color = { 0.5f, 0.5f, 0.5f, 1.0f };
    forge_ui_ctx_rect_layout(&ctx, color, SR_LAYOUT_SIZE);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_rect_layout_nan_color_no_advance(void)
{
    TEST("rect_layout: NaN color does not advance cursor");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect layout_rect = { SR_LAYOUT_X, SR_LAYOUT_Y,
                                 SR_LAYOUT_W, SR_LAYOUT_H };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, layout_rect,
                              FORGE_UI_LAYOUT_VERTICAL,
                              SR_LAYOUT_PAD, SR_LAYOUT_GAP));
    ForgeUiLayout *layout = &ctx.layout_stack[ctx.layout_depth - 1];
    int items_before = layout->item_count;
    float cursor_before = layout->cursor_y;
    int before = ctx.vertex_count;
    ForgeUiColor color = { NAN, 0.5f, 0.5f, 1.0f };
    forge_ui_ctx_rect_layout(&ctx, color, SR_LAYOUT_SIZE);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    ASSERT_EQ_INT(layout->item_count, items_before);
    ASSERT_NEAR(layout->cursor_y, cursor_before, 0.001f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Progress bar tests ─────────────────────────────────────────────────── */

#define PB_TEST_X       10.0f   /* progress bar test rect x */
#define PB_TEST_Y       20.0f   /* progress bar test rect y */
#define PB_TEST_W      200.0f   /* progress bar test rect width */
#define PB_TEST_H       20.0f   /* progress bar test rect height */
#define PB_TEST_VAL     75.0f   /* test value for progress bar */
#define PB_TEST_MAX    100.0f   /* test max for progress bar */
#define PB_LAYOUT_SIZE  24.0f   /* layout size for progress bar layout tests */
#define PB_LAYOUT_X     10.0f   /* layout rect x */
#define PB_LAYOUT_Y     10.0f   /* layout rect y */
#define PB_LAYOUT_W    300.0f   /* layout rect width */
#define PB_LAYOUT_H    200.0f   /* layout rect height */
#define PB_LAYOUT_PAD    4.0f   /* layout padding */
#define PB_LAYOUT_GAP    2.0f   /* layout spacing */

static void test_progress_bar_basic(void)
{
    TEST("progress_bar: basic rendering emits vertices with correct fill ratio");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 0.8f, 0.2f, 0.2f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, PB_TEST_MAX, fill, rect);
    /* Background + fill = 2 quads = 8 vertices */
    ASSERT_EQ_INT(ctx.vertex_count - before, 8);
    ASSERT_TRUE(ctx.index_count > 0);

    /* Verify fill quad x-extent: fill width = PB_TEST_W * (75/100) = 150 */
    float expected_fill_w = PB_TEST_W * (PB_TEST_VAL / PB_TEST_MAX);
    /* Fill quad is the second quad emitted (vertices 4..7 from 'before').
     * Vertex 4 = top-left, vertex 5 = top-right of fill. */
    float fill_left  = ctx.vertices[before + 4].pos_x;
    float fill_right = ctx.vertices[before + 5].pos_x;
    ASSERT_NEAR(fill_left, PB_TEST_X, 0.1f);
    ASSERT_NEAR(fill_right, PB_TEST_X + expected_fill_w, 0.1f);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_zero_value(void)
{
    TEST("progress_bar: value=0 emits only background");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 0.2f, 0.8f, 0.2f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, 0.0f, PB_TEST_MAX, fill, rect);
    /* Only background quad = 4 vertices */
    ASSERT_EQ_INT(ctx.vertex_count - before, 4);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_full_value(void)
{
    TEST("progress_bar: value=max emits background + fill");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 0.2f, 0.2f, 0.8f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_MAX, PB_TEST_MAX, fill, rect);
    /* Background + fill = 8 vertices */
    ASSERT_EQ_INT(ctx.vertex_count - before, 8);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_over_max_clamped(void)
{
    TEST("progress_bar: value > max clamped to max");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 0.8f, 0.8f, 0.2f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    /* Value 200 exceeds max 100 — should clamp, producing same as full */
    forge_ui_ctx_progress_bar(&ctx, 200.0f, PB_TEST_MAX, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count - before, 8);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_negative_value(void)
{
    TEST("progress_bar: negative value clamped to 0");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 0.8f, 0.2f, 0.8f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, -50.0f, PB_TEST_MAX, fill, rect);
    /* Only background, no fill */
    ASSERT_EQ_INT(ctx.vertex_count - before, 4);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_zero_max_rejected(void)
{
    TEST("progress_bar: max_val=0 produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, 0.0f, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_negative_max_rejected(void)
{
    TEST("progress_bar: negative max_val produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, -100.0f, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_nan_value(void)
{
    TEST("progress_bar: NaN value treated as 0");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, NAN, PB_TEST_MAX, fill, rect);
    /* NaN value → clamped to 0 → only background */
    ASSERT_EQ_INT(ctx.vertex_count - before, 4);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_nan_max_rejected(void)
{
    TEST("progress_bar: NaN max_val produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, NAN, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_inf_rect_rejected(void)
{
    TEST("progress_bar: INFINITY in rect produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, INFINITY, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, PB_TEST_MAX, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_nan_rect_rejected(void)
{
    TEST("progress_bar: NaN in rect produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { NAN, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, PB_TEST_MAX, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_null_ctx(void)
{
    TEST("progress_bar: NULL ctx does not crash");
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(NULL, PB_TEST_VAL, PB_TEST_MAX, fill, rect);
    /* no crash = pass */
}

static void test_progress_bar_layout_basic(void)
{
    TEST("progress_bar_layout: emits vertices inside layout");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect layout_rect = { PB_LAYOUT_X, PB_LAYOUT_Y,
                                 PB_LAYOUT_W, PB_LAYOUT_H };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, layout_rect,
                                          FORGE_UI_LAYOUT_VERTICAL,
                                          PB_LAYOUT_PAD, PB_LAYOUT_GAP));
    int before = ctx.vertex_count;
    int idx_before = ctx.index_count;
    ForgeUiColor fill = { 0.2f, 0.8f, 0.2f, 1.0f };
    forge_ui_ctx_progress_bar_layout(&ctx, PB_TEST_VAL, PB_TEST_MAX,
                                      fill, PB_LAYOUT_SIZE);
    /* Exact two-quad topology: background (4v, 6i) + fill (4v, 6i) */
    ASSERT_EQ_INT(ctx.vertex_count - before, 8);
    ASSERT_EQ_INT(ctx.index_count - idx_before, 12);
    /* Verify the layout wrapper forwarded value/max correctly by checking
     * the fill quad's right edge.  Background = vertices [before..before+3],
     * fill = vertices [before+4..before+7]. */
    float bg_left = ctx.vertices[before].pos_x;
    float bar_w = ctx.vertices[before + 1].pos_x - bg_left;
    float fill_right = ctx.vertices[before + 4 + 1].pos_x;
    float expected_fill_w = bar_w * (PB_TEST_VAL / PB_TEST_MAX);
    ASSERT_NEAR(fill_right - bg_left, expected_fill_w, 0.5f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_layout_no_layout(void)
{
    TEST("progress_bar_layout: no active layout produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 0.2f, 0.8f, 0.2f, 1.0f };
    forge_ui_ctx_progress_bar_layout(&ctx, PB_TEST_VAL, PB_TEST_MAX,
                                      fill, PB_LAYOUT_SIZE);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_layout_zero_max_rejected(void)
{
    TEST("progress_bar_layout: max_val=0 produces no vertices and no cursor advance");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect layout_rect = { PB_LAYOUT_X, PB_LAYOUT_Y,
                                 PB_LAYOUT_W, PB_LAYOUT_H };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, layout_rect,
                              FORGE_UI_LAYOUT_VERTICAL,
                              PB_LAYOUT_PAD, PB_LAYOUT_GAP));
    int before = ctx.vertex_count;
    ForgeUiLayout *layout = &ctx.layout_stack[ctx.layout_depth - 1];
    int items_before = layout->item_count;
    float cursor_before = layout->cursor_y;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    forge_ui_ctx_progress_bar_layout(&ctx, PB_TEST_VAL, 0.0f,
                                      fill, PB_LAYOUT_SIZE);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    ASSERT_EQ_INT(layout->item_count, items_before);
    ASSERT_NEAR(layout->cursor_y, cursor_before, 0.001f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_inf_value(void)
{
    TEST("progress_bar: +Inf value treated as 0 (clamped)");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 0.8f, 0.2f, 0.2f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    /* +Inf value → isfinite fails → clamped to 0 → only background */
    forge_ui_ctx_progress_bar(&ctx, INFINITY, PB_TEST_MAX, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count - before, 4);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_neg_inf_value(void)
{
    TEST("progress_bar: -Inf value treated as 0 (clamped)");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 0.8f, 0.2f, 0.2f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    /* -Inf value → isfinite fails → clamped to 0 → only background */
    forge_ui_ctx_progress_bar(&ctx, -INFINITY, PB_TEST_MAX, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count - before, 4);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_inf_max_rejected(void)
{
    TEST("progress_bar: +Inf max_val produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, INFINITY, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_neg_inf_max_rejected(void)
{
    TEST("progress_bar: -Inf max_val produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, -INFINITY, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_nan_fill_color_rejected(void)
{
    TEST("progress_bar: NaN in fill_color produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { NAN, 0.2f, 0.2f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, PB_TEST_MAX, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_inf_fill_color_rejected(void)
{
    TEST("progress_bar: Inf in fill_color produces no vertices");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    ForgeUiColor fill = { 0.8f, INFINITY, 0.2f, 1.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_VAL, PB_TEST_MAX, fill, rect);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_fill_color_clamped(void)
{
    TEST("progress_bar: out-of-range fill_color components clamped to [0,1]");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    int before = ctx.vertex_count;
    /* Components outside [0,1] — should be clamped, not rejected */
    ForgeUiColor fill = { 1.5f, -0.3f, 0.5f, 2.0f };
    ForgeUiRect rect = { PB_TEST_X, PB_TEST_Y, PB_TEST_W, PB_TEST_H };
    forge_ui_ctx_progress_bar(&ctx, PB_TEST_MAX, PB_TEST_MAX, fill, rect);
    /* Should still emit background + fill = 8 vertices */
    ASSERT_EQ_INT(ctx.vertex_count - before, 8);

    /* Verify the fill quad colors are clamped: vertex indices 4..7 */
    ASSERT_NEAR(ctx.vertices[before + 4].r, 1.0f, 0.001f);   /* 1.5 → 1.0 */
    ASSERT_NEAR(ctx.vertices[before + 4].g, 0.0f, 0.001f);   /* -0.3 → 0.0 */
    ASSERT_NEAR(ctx.vertices[before + 4].b, 0.5f, 0.001f);   /* 0.5 unchanged */
    ASSERT_NEAR(ctx.vertices[before + 4].a, 1.0f, 0.001f);   /* 2.0 → 1.0 */

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_layout_cursor_advance(void)
{
    TEST("progress_bar_layout: advances layout cursor by size");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect layout_rect = { PB_LAYOUT_X, PB_LAYOUT_Y,
                                 PB_LAYOUT_W, PB_LAYOUT_H };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, layout_rect,
                                          FORGE_UI_LAYOUT_VERTICAL,
                                          PB_LAYOUT_PAD, PB_LAYOUT_GAP));

    /* Draw two progress bars and verify the second starts below the first.
     * Avoids probing with layout_next(0) which itself advances the cursor. */
    int before1 = ctx.vertex_count;
    int idx_before1 = ctx.index_count;
    ForgeUiColor fill = { 0.2f, 0.8f, 0.2f, 1.0f };
    forge_ui_ctx_progress_bar_layout(&ctx, PB_TEST_VAL, PB_TEST_MAX,
                                      fill, PB_LAYOUT_SIZE);
    /* Exact two-quad topology per bar */
    ASSERT_EQ_INT(ctx.vertex_count - before1, 8);
    ASSERT_EQ_INT(ctx.index_count - idx_before1, 12);
    /* Verify fill ratio on first bar */
    float bg1_left = ctx.vertices[before1].pos_x;
    float bg1_w = ctx.vertices[before1 + 1].pos_x - bg1_left;
    float fill1_right = ctx.vertices[before1 + 4 + 1].pos_x;
    float exp_fill1_w = bg1_w * (PB_TEST_VAL / PB_TEST_MAX);
    ASSERT_NEAR(fill1_right - bg1_left, exp_fill1_w, 0.5f);

    /* Second bar — its background top-left Y should be advanced */
    int before2 = ctx.vertex_count;
    int idx_before2 = ctx.index_count;
    forge_ui_ctx_progress_bar_layout(&ctx, PB_TEST_VAL, PB_TEST_MAX,
                                      fill, PB_LAYOUT_SIZE);
    ASSERT_EQ_INT(ctx.vertex_count - before2, 8);
    ASSERT_EQ_INT(ctx.index_count - idx_before2, 12);

    /* The second bar's background Y should equal first bar's Y + size + gap.
     * First bar background vertex 0 = top-left of first bar. */
    float bar1_y = ctx.vertices[before1].pos_y;
    float bar2_y = ctx.vertices[before2].pos_y;
    float expected_bar2_y = bar1_y + PB_LAYOUT_SIZE + PB_LAYOUT_GAP;
    ASSERT_NEAR(bar2_y, expected_bar2_y, 0.1f);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_layout_inf_max_rejected(void)
{
    TEST("progress_bar_layout: +Inf max_val produces no vertices and no cursor advance");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect layout_rect = { PB_LAYOUT_X, PB_LAYOUT_Y,
                                 PB_LAYOUT_W, PB_LAYOUT_H };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, layout_rect,
                              FORGE_UI_LAYOUT_VERTICAL,
                              PB_LAYOUT_PAD, PB_LAYOUT_GAP));
    int before = ctx.vertex_count;
    ForgeUiLayout *layout = &ctx.layout_stack[ctx.layout_depth - 1];
    int items_before = layout->item_count;
    float cursor_before = layout->cursor_y;
    ForgeUiColor fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    forge_ui_ctx_progress_bar_layout(&ctx, PB_TEST_VAL, INFINITY,
                                      fill, PB_LAYOUT_SIZE);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    ASSERT_EQ_INT(layout->item_count, items_before);
    ASSERT_NEAR(layout->cursor_y, cursor_before, 0.001f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_progress_bar_layout_nan_fill_no_advance(void)
{
    TEST("progress_bar_layout: NaN fill_color does not advance cursor");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect layout_rect = { PB_LAYOUT_X, PB_LAYOUT_Y,
                                 PB_LAYOUT_W, PB_LAYOUT_H };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, layout_rect,
                              FORGE_UI_LAYOUT_VERTICAL,
                              PB_LAYOUT_PAD, PB_LAYOUT_GAP));
    int before = ctx.vertex_count;
    ForgeUiLayout *layout = &ctx.layout_stack[ctx.layout_depth - 1];
    int items_before = layout->item_count;
    float cursor_before = layout->cursor_y;
    ForgeUiColor fill = { NAN, 0.5f, 0.5f, 1.0f };
    forge_ui_ctx_progress_bar_layout(&ctx, PB_TEST_VAL, PB_TEST_MAX,
                                      fill, PB_LAYOUT_SIZE);
    ASSERT_EQ_INT(ctx.vertex_count, before);
    ASSERT_EQ_INT(layout->item_count, items_before);
    ASSERT_NEAR(layout->cursor_y, cursor_before, 0.001f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Separator tests ───────────────────────────────────────────────────── */

#define TEST_SEP_X       20.0f
#define TEST_SEP_Y       40.0f
#define TEST_SEP_W      200.0f
#define TEST_SEP_H       10.0f

static void test_separator_emits_rect(void)
{
    TEST("separator: emits a 1px line as a filled rect");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect rect = { TEST_SEP_X, TEST_SEP_Y, TEST_SEP_W, TEST_SEP_H };
    forge_ui_ctx_separator(&ctx, rect);
    /* Should emit one rect: 4 verts, 6 indices */
    ASSERT_EQ_INT(ctx.vertex_count, 4);
    ASSERT_EQ_INT(ctx.index_count, 6);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_separator_null_ctx(void)
{
    TEST("separator: NULL ctx does not crash");
    ForgeUiRect rect = { TEST_SEP_X, TEST_SEP_Y, TEST_SEP_W, TEST_SEP_H };
    forge_ui_ctx_separator(NULL, rect);
    pass_count++;
}

static void test_separator_nan_rect_rejected(void)
{
    TEST("separator: NaN rect is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect rect = { NAN, TEST_SEP_Y, TEST_SEP_W, TEST_SEP_H };
    forge_ui_ctx_separator(&ctx, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_separator_uses_thickness_constant(void)
{
    TEST("separator: line height and vertical center match constants");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect rect = { TEST_SEP_X, TEST_SEP_Y, TEST_SEP_W, TEST_SEP_H };
    forge_ui_ctx_separator(&ctx, rect);
    ASSERT_TRUE(ctx.vertex_count >= 4);
    /* Height should be FORGE_UI_SEPARATOR_THICKNESS (1px) */
    float emitted_h = ctx.vertices[2].pos_y - ctx.vertices[0].pos_y;
    ASSERT_NEAR(emitted_h, FORGE_UI_SEPARATOR_THICKNESS, 0.01f);
    /* Top edge should be vertically centered within the input rect */
    float expected_y = TEST_SEP_Y + (TEST_SEP_H - FORGE_UI_SEPARATOR_THICKNESS) * 0.5f;
    ASSERT_NEAR(ctx.vertices[0].pos_y, expected_y, 0.01f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_separator_zero_height_rejected(void)
{
    TEST("separator: zero-height rect is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect rect = { TEST_SEP_X, TEST_SEP_Y, TEST_SEP_W, 0.0f };
    forge_ui_ctx_separator(&ctx, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_separator_zero_width_rejected(void)
{
    TEST("separator: zero-width rect is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect rect = { TEST_SEP_X, TEST_SEP_Y, 0.0f, TEST_SEP_H };
    forge_ui_ctx_separator(&ctx, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

#define TEST_SEP_THIN_H  0.5f  /* rect shorter than separator thickness */

static void test_separator_clamps_thickness_to_rect_height(void)
{
    TEST("separator: thickness clamped to rect height when rect is shorter");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect rect = { TEST_SEP_X, TEST_SEP_Y, TEST_SEP_W, TEST_SEP_THIN_H };
    forge_ui_ctx_separator(&ctx, rect);
    ASSERT_TRUE(ctx.vertex_count >= 4);
    /* Emitted height should equal the rect height, not the thickness constant */
    float emitted_h = ctx.vertices[2].pos_y - ctx.vertices[0].pos_y;
    ASSERT_NEAR(emitted_h, TEST_SEP_THIN_H, 0.01f);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_separator_negative_dims_rejected(void)
{
    TEST("separator: negative dimensions are rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect rect = { TEST_SEP_X, TEST_SEP_Y, -10.0f, TEST_SEP_H };
    forge_ui_ctx_separator(&ctx, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    rect = (ForgeUiRect){ TEST_SEP_X, TEST_SEP_Y, TEST_SEP_W, -10.0f };
    forge_ui_ctx_separator(&ctx, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Tree node tests ───────────────────────────────────────────────────── */

#define TEST_TREE_X       10.0f
#define TEST_TREE_Y       10.0f
#define TEST_TREE_W      200.0f
#define TEST_TREE_H       30.0f

static void test_tree_push_pop_basic(void)
{
    TEST("tree: push/pop emits draw data and manages id scope");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    bool open = true;
    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    int depth_before = ctx.id_stack_depth;
    bool expanded = forge_ui_ctx_tree_push(&ctx, "Root##tree", &open, rect);
    ASSERT_TRUE(expanded);
    ASSERT_EQ_INT(ctx.id_stack_depth, depth_before + 1);
    ASSERT_TRUE(ctx.vertex_count > 0);

    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, depth_before);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_push_closed_returns_false(void)
{
    TEST("tree: push with *open=false returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    bool open = false;
    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    bool expanded = forge_ui_ctx_tree_push(&ctx, "Closed##tree", &open, rect);
    ASSERT_TRUE(!expanded);
    /* Still pushes scope even when closed */
    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_push_null_label(void)
{
    TEST("tree: NULL label returns false and manages scope");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    bool open = true;
    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    bool expanded = forge_ui_ctx_tree_push(&ctx, NULL, &open, rect);
    ASSERT_TRUE(!expanded);

    /* tree_pop should still pop the error scope */
    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_push_null_open(void)
{
    TEST("tree: NULL open pointer returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    bool expanded = forge_ui_ctx_tree_push(&ctx, "Node", NULL, rect);
    ASSERT_TRUE(!expanded);

    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_push_empty_label(void)
{
    TEST("tree: empty label returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    bool open = true;
    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    bool expanded = forge_ui_ctx_tree_push(&ctx, "", &open, rect);
    ASSERT_TRUE(!expanded);

    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_nested_push_pop(void)
{
    TEST("tree: nested parent/child push/pop with call-depth tracking");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    bool open_parent = true;
    bool open_child  = true;
    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };

    /* Parent push */
    int depth0 = ctx.id_stack_depth;
    ASSERT_TRUE(forge_ui_ctx_tree_push(&ctx, "Parent", &open_parent, rect));
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0 + 1);

    /* Child push */
    ASSERT_TRUE(forge_ui_ctx_tree_push(&ctx, "Child", &open_child, rect));
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0 + 2);

    /* Pop child — must only pop child scope, not parent */
    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0 + 1);

    /* Pop parent */
    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_nested_closed_child_does_not_pop_parent(void)
{
    TEST("tree: closed child pop does not consume parent scope");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    bool open_parent = true;
    bool open_child  = false;
    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    int depth0 = ctx.id_stack_depth;

    /* Parent push succeeds */
    ASSERT_TRUE(forge_ui_ctx_tree_push(&ctx, "Parent", &open_parent, rect));
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0 + 1);

    /* Child push returns false (open=false) but still pushes a scope */
    ASSERT_TRUE(!forge_ui_ctx_tree_push(&ctx, "Child", &open_child, rect));
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0 + 2);

    /* Child pop must only pop the child scope, leaving parent intact */
    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0 + 1);

    /* Parent pop */
    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_pop_without_push(void)
{
    TEST("tree: pop without push does not crash or underflow");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    int depth = ctx.id_stack_depth;
    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, depth);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_pop_null_ctx(void)
{
    TEST("tree: pop with NULL ctx does not crash");
    forge_ui_ctx_tree_pop(NULL);
    pass_count++;
}

static void test_tree_push_nan_rect_rejected(void)
{
    TEST("tree: NaN rect returns false but manages scope");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    bool open = true;
    ForgeUiRect rect = { NAN, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    bool expanded = forge_ui_ctx_tree_push(&ctx, "NaN", &open, rect);
    ASSERT_TRUE(!expanded);
    /* Emits no draw data on NaN rect */
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_toggle_on_click(void)
{
    TEST("tree: click toggles open state");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool open = true;
    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    float cx = TEST_TREE_X + TEST_TREE_W * 0.5f;
    float cy = TEST_TREE_Y + TEST_TREE_H * 0.5f;

    /* Frame 0: hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_tree_push(&ctx, "Toggle", &open, rect);
    forge_ui_ctx_tree_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(open); /* still open */

    /* Frame 1: press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_tree_push(&ctx, "Toggle", &open, rect);
    forge_ui_ctx_tree_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(open); /* active but not toggled yet */

    /* Frame 2: release while hovering — toggle */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_tree_push(&ctx, "Toggle", &open, rect);
    forge_ui_ctx_tree_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!open); /* toggled to closed */

    forge_ui_ctx_free(&ctx);
}

static void test_tree_push_layout_no_layout(void)
{
    TEST("tree_push_layout: no layout returns false");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    /* No layout_push — layout_depth = 0 */

    bool open = true;
    bool expanded = forge_ui_ctx_tree_push_layout(&ctx, "Node", &open, 30.0f);
    ASSERT_TRUE(!expanded);

    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_push_layout_null_label_no_cursor_advance(void)
{
    TEST("tree_push_layout: NULL label does not advance cursor");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect lr = { 10.0f, 10.0f, 200.0f, 300.0f };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, lr,
                              FORGE_UI_LAYOUT_VERTICAL, 0.0f, 0.0f));
    ForgeUiLayout *layout = &ctx.layout_stack[ctx.layout_depth - 1];
    float cursor_before = layout->cursor_y;

    bool open = true;
    forge_ui_ctx_tree_push_layout(&ctx, NULL, &open, 30.0f);
    /* Cursor should NOT have advanced */
    ASSERT_NEAR(layout->cursor_y, cursor_before, 0.001f);

    forge_ui_ctx_tree_pop(&ctx);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_depth_exceeds_max(void)
{
    TEST("tree: push beyond max depth returns false, pop still pairs");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    int depth0 = ctx.id_stack_depth;
    bool opens[FORGE_UI_ID_STACK_MAX_DEPTH + 1];

    /* Push up to max depth — all succeed */
    int i;
    for (i = 0; i < FORGE_UI_ID_STACK_MAX_DEPTH; i++) {
        opens[i] = true;
        char label[32];
        SDL_snprintf(label, sizeof(label), "N%d", i);
        ASSERT_TRUE(forge_ui_ctx_tree_push(&ctx, label, &opens[i], rect));
    }

    /* One more push — should fail (exceeds max) */
    opens[i] = true;
    ASSERT_TRUE(!forge_ui_ctx_tree_push(&ctx, "overflow", &opens[i], rect));

    /* Pop the overflow push — must not consume a real node */
    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0 + FORGE_UI_ID_STACK_MAX_DEPTH);

    /* Pop all valid pushes in reverse */
    for (int j = FORGE_UI_ID_STACK_MAX_DEPTH - 1; j >= 0; j--) {
        forge_ui_ctx_tree_pop(&ctx);
    }

    /* ID stack should be back to original depth */
    ASSERT_EQ_INT(ctx.id_stack_depth, depth0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_tree_call_depth_tracks_correctly(void)
{
    TEST("tree: _tree_call_depth increments and decrements correctly");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ASSERT_EQ_INT(ctx._tree_call_depth, 0);

    ForgeUiRect rect = { TEST_TREE_X, TEST_TREE_Y, TEST_TREE_W, TEST_TREE_H };
    bool open = true;
    forge_ui_ctx_tree_push(&ctx, "A", &open, rect);
    ASSERT_EQ_INT(ctx._tree_call_depth, 1);

    forge_ui_ctx_tree_push(&ctx, "B", &open, rect);
    ASSERT_EQ_INT(ctx._tree_call_depth, 2);

    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx._tree_call_depth, 1);

    forge_ui_ctx_tree_pop(&ctx);
    ASSERT_EQ_INT(ctx._tree_call_depth, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ── Sparkline tests ───────────────────────────────────────────────────── */

#define TEST_SPARK_X       10.0f
#define TEST_SPARK_Y       10.0f
#define TEST_SPARK_W      100.0f
#define TEST_SPARK_H       40.0f

static void test_sparkline_emits_draw_data(void)
{
    TEST("sparkline: emits background rect and line segments");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 0.0f, 0.5f, 1.0f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, vals, 3, 0.0f, 1.0f, color, rect);

    /* Background (4 verts) + at least some line column rects */
    ASSERT_TRUE(ctx.vertex_count >= 8);
    ASSERT_TRUE(ctx.index_count >= 12);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_null_values_rejected(void)
{
    TEST("sparkline: NULL values is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, NULL, 3, 0.0f, 1.0f, color, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_count_below_two_rejected(void)
{
    TEST("sparkline: count < 2 is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 1.0f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, vals, 1, 0.0f, 1.0f, color, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_equal_range_rejected(void)
{
    TEST("sparkline: min == max is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 0.5f, 0.5f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, vals, 2, 5.0f, 5.0f, color, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_inverted_range_rejected(void)
{
    TEST("sparkline: max < min is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 0.5f, 0.5f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, vals, 2, 10.0f, 5.0f, color, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_nan_range_rejected(void)
{
    TEST("sparkline: NaN in range is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 0.0f, 1.0f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, vals, 2, NAN, 1.0f, color, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_nan_rect_rejected(void)
{
    TEST("sparkline: NaN rect is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 0.0f, 1.0f };
    ForgeUiRect rect = { TEST_SPARK_X, NAN, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, vals, 2, 0.0f, 1.0f, color, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_zero_width_rejected(void)
{
    TEST("sparkline: zero width rect is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 0.0f, 1.0f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, 0.0f, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, vals, 2, 0.0f, 1.0f, color, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_nan_color_rejected(void)
{
    TEST("sparkline: NaN line_color is rejected");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 0.0f, 1.0f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { NAN, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, vals, 2, 0.0f, 1.0f, color, rect);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_nan_value_clamped(void)
{
    TEST("sparkline: NaN value is clamped to min_val");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { NAN, 1.0f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(&ctx, vals, 2, 0.0f, 1.0f, color, rect);
    /* Should still render (NaN clamped internally) */
    ASSERT_TRUE(ctx.vertex_count > 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_null_ctx(void)
{
    TEST("sparkline: NULL ctx does not crash");
    float vals[] = { 0.0f, 1.0f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline(NULL, vals, 2, 0.0f, 1.0f, color, rect);
    pass_count++;
}

static void test_sparkline_out_of_range_color_clamped(void)
{
    TEST("sparkline: out-of-range color is clamped to [0,1]");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 0.0f, 1.0f };
    ForgeUiRect rect = { TEST_SPARK_X, TEST_SPARK_Y, TEST_SPARK_W, TEST_SPARK_H };
    /* Color with components outside [0,1] */
    ForgeUiColor color = { -0.5f, 2.0f, 0.5f, 1.5f };
    forge_ui_ctx_sparkline(&ctx, vals, 2, 0.0f, 1.0f, color, rect);
    ASSERT_TRUE(ctx.vertex_count > 4);
    /* Line column vertices start after background (first 4 verts) */
    ASSERT_NEAR(ctx.vertices[4].r, 0.0f, 0.01f);
    ASSERT_NEAR(ctx.vertices[4].g, 1.0f, 0.01f);
    ASSERT_NEAR(ctx.vertices[4].a, 1.0f, 0.01f);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_layout_no_layout(void)
{
    TEST("sparkline_layout: no layout stack returns early");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    float vals[] = { 0.0f, 1.0f };
    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline_layout(&ctx, vals, 2, 0.0f, 1.0f, color, 40.0f);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_sparkline_layout_invalid_no_cursor_advance(void)
{
    TEST("sparkline_layout: invalid values do not advance cursor");
    if (!setup_atlas()) return;
    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);
    ForgeUiRect lr = { 10.0f, 10.0f, 200.0f, 300.0f };
    ASSERT_TRUE(forge_ui_ctx_layout_push(&ctx, lr,
                              FORGE_UI_LAYOUT_VERTICAL, 0.0f, 0.0f));
    ForgeUiLayout *layout = &ctx.layout_stack[ctx.layout_depth - 1];
    float cursor_before = layout->cursor_y;

    ForgeUiColor color = { 0.0f, 1.0f, 0.0f, 1.0f };
    /* NULL values — should not advance */
    forge_ui_ctx_sparkline_layout(&ctx, NULL, 3, 0.0f, 1.0f, color, 40.0f);
    ASSERT_NEAR(layout->cursor_y, cursor_before, 0.001f);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    /* count < 2 — should not advance */
    float one_val[] = { 1.0f };
    forge_ui_ctx_sparkline_layout(&ctx, one_val, 1, 0.0f, 1.0f, color, 40.0f);
    ASSERT_NEAR(layout->cursor_y, cursor_before, 0.001f);

    /* inverted range — should not advance */
    float vals[] = { 0.0f, 1.0f };
    forge_ui_ctx_sparkline_layout(&ctx, vals, 2, 10.0f, 5.0f, color, 40.0f);
    ASSERT_NEAR(layout->cursor_y, cursor_before, 0.001f);

    /* invalid line_color — should not advance */
    color = (ForgeUiColor){ NAN, 1.0f, 0.0f, 1.0f };
    forge_ui_ctx_sparkline_layout(&ctx, vals, 2, 0.0f, 1.0f, color, 40.0f);
    ASSERT_NEAR(layout->cursor_y, cursor_before, 0.001f);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════
 * HSV/RGB conversion tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_hsv_to_rgb_red(void)
{
    TEST("hsv_to_rgb: pure red (0, 1, 1) -> (1, 0, 0)");
    float r, g, b;
    forge_ui_hsv_to_rgb(TEST_HSV_RED_H, TEST_HSV_RED_S, TEST_HSV_RED_V,
                         &r, &g, &b);
    ASSERT_NEAR(r, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 0.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_green(void)
{
    TEST("hsv_to_rgb: pure green (120, 1, 1) -> (0, 1, 0)");
    float r, g, b;
    forge_ui_hsv_to_rgb(TEST_HSV_GREEN_H, TEST_HSV_GREEN_S, TEST_HSV_GREEN_V,
                         &r, &g, &b);
    ASSERT_NEAR(r, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 0.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_blue(void)
{
    TEST("hsv_to_rgb: pure blue (240, 1, 1) -> (0, 0, 1)");
    float r, g, b;
    forge_ui_hsv_to_rgb(TEST_HSV_BLUE_H, TEST_HSV_BLUE_S, TEST_HSV_BLUE_V,
                         &r, &g, &b);
    ASSERT_NEAR(r, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 1.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_null_output(void)
{
    TEST("hsv_to_rgb: NULL output pointers do not crash");
    forge_ui_hsv_to_rgb(0.0f, 1.0f, 1.0f, NULL, NULL, NULL);
    /* survived without crash */
}

static void test_hsv_to_rgb_nan_hue(void)
{
    TEST("hsv_to_rgb: NaN hue treated as 0");
    float r, g, b;
    forge_ui_hsv_to_rgb(NAN, TEST_HSV_RED_S, TEST_HSV_RED_V, &r, &g, &b);
    /* NaN hue -> 0 -> red */
    ASSERT_NEAR(r, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 0.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_negative_hue(void)
{
    TEST("hsv_to_rgb: negative hue wraps to [0, 360)");
    float r, g, b;
    forge_ui_hsv_to_rgb(-120.0f, 1.0f, 1.0f, &r, &g, &b);
    /* -120 + 360 = 240 -> blue */
    ASSERT_NEAR(r, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 1.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_nan_saturation(void)
{
    TEST("hsv_to_rgb: NaN saturation treated as 0 (grayscale)");
    float r, g, b;
    forge_ui_hsv_to_rgb(TEST_HSV_RED_H, NAN, TEST_HSV_RED_V, &r, &g, &b);
    /* NaN s -> 0 -> grayscale: r=g=b=v=1.0 */
    ASSERT_NEAR(r, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 1.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_inf_saturation(void)
{
    TEST("hsv_to_rgb: +Inf saturation treated as 0 (grayscale)");
    float r, g, b;
    forge_ui_hsv_to_rgb(TEST_HSV_RED_H, INFINITY, TEST_HSV_RED_V, &r, &g, &b);
    /* +Inf s -> isfinite false -> 0 -> grayscale: r=g=b=v=1.0 */
    ASSERT_NEAR(r, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 1.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_nan_value(void)
{
    TEST("hsv_to_rgb: NaN value treated as 0 (black)");
    float r, g, b;
    forge_ui_hsv_to_rgb(TEST_HSV_RED_H, TEST_HSV_RED_S, NAN, &r, &g, &b);
    /* NaN v -> 0 -> black */
    ASSERT_NEAR(r, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 0.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_inf_value(void)
{
    TEST("hsv_to_rgb: +Inf value treated as 0 (black)");
    float r, g, b;
    forge_ui_hsv_to_rgb(TEST_HSV_RED_H, TEST_HSV_RED_S, INFINITY, &r, &g, &b);
    /* +Inf v -> isfinite false -> 0 -> black */
    ASSERT_NEAR(r, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 0.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_neg_inf_saturation(void)
{
    TEST("hsv_to_rgb: -Inf saturation treated as 0 (grayscale)");
    float r, g, b;
    forge_ui_hsv_to_rgb(TEST_HSV_RED_H, -INFINITY, TEST_HSV_RED_V, &r, &g, &b);
    /* -Inf s -> 0 -> grayscale: r=g=b=v=1.0 */
    ASSERT_NEAR(r, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 1.0f, TEST_HSV_EPS);
}

static void test_hsv_to_rgb_neg_inf_value(void)
{
    TEST("hsv_to_rgb: -Inf value treated as 0 (black)");
    float r, g, b;
    forge_ui_hsv_to_rgb(TEST_HSV_RED_H, TEST_HSV_RED_S, -INFINITY, &r, &g, &b);
    /* -Inf v -> 0 -> black */
    ASSERT_NEAR(r, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(g, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(b, 0.0f, TEST_HSV_EPS);
}

static void test_rgb_to_hsv_red(void)
{
    TEST("rgb_to_hsv: pure red (1, 0, 0) -> (0, 1, 1)");
    float h, s, v;
    forge_ui_rgb_to_hsv(1.0f, 0.0f, 0.0f, &h, &s, &v);
    ASSERT_NEAR(h, TEST_HSV_RED_H, TEST_HSV_EPS);
    ASSERT_NEAR(s, TEST_HSV_RED_S, TEST_HSV_EPS);
    ASSERT_NEAR(v, TEST_HSV_RED_V, TEST_HSV_EPS);
}

static void test_rgb_to_hsv_green(void)
{
    TEST("rgb_to_hsv: pure green (0, 1, 0) -> (120, 1, 1)");
    float h, s, v;
    forge_ui_rgb_to_hsv(0.0f, 1.0f, 0.0f, &h, &s, &v);
    ASSERT_NEAR(h, TEST_HSV_GREEN_H, TEST_HSV_EPS);
    ASSERT_NEAR(s, TEST_HSV_GREEN_S, TEST_HSV_EPS);
    ASSERT_NEAR(v, TEST_HSV_GREEN_V, TEST_HSV_EPS);
}

static void test_rgb_to_hsv_blue(void)
{
    TEST("rgb_to_hsv: pure blue (0, 0, 1) -> (240, 1, 1)");
    float h, s, v;
    forge_ui_rgb_to_hsv(0.0f, 0.0f, 1.0f, &h, &s, &v);
    ASSERT_NEAR(h, TEST_HSV_BLUE_H, TEST_HSV_EPS);
    ASSERT_NEAR(s, TEST_HSV_BLUE_S, TEST_HSV_EPS);
    ASSERT_NEAR(v, TEST_HSV_BLUE_V, TEST_HSV_EPS);
}

static void test_rgb_to_hsv_null_output(void)
{
    TEST("rgb_to_hsv: NULL output pointers do not crash");
    forge_ui_rgb_to_hsv(1.0f, 0.0f, 0.0f, NULL, NULL, NULL);
    /* no crash = pass */
}

static void test_rgb_to_hsv_nan_input(void)
{
    TEST("rgb_to_hsv: NaN input treated as 0");
    float h, s, v;
    forge_ui_rgb_to_hsv(NAN, 0.0f, 0.0f, &h, &s, &v);
    /* NaN->0, so (0,0,0) = black: h=0, s=0, v=0 */
    ASSERT_NEAR(h, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(s, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(v, 0.0f, TEST_HSV_EPS);
}

static void test_hsv_rgb_roundtrip(void)
{
    TEST("hsv->rgb->hsv roundtrip preserves values");
    float h = 195.0f, s = 0.75f, v = 0.85f;
    float r, g, b;
    forge_ui_hsv_to_rgb(h, s, v, &r, &g, &b);
    float h2, s2, v2;
    forge_ui_rgb_to_hsv(r, g, b, &h2, &s2, &v2);
    ASSERT_NEAR(h2, h, TEST_HSV_EPS);
    ASSERT_NEAR(s2, s, TEST_HSV_EPS);
    ASSERT_NEAR(v2, v, TEST_HSV_EPS);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Drag float tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_drag_float_emits_draw_data(void)
{
    TEST("drag_float: emits vertices and indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float value = TEST_DF_INIT;
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    /* Return value is 'changed' (false when not dragging), not success */
    forge_ui_ctx_drag_float(&ctx, "test##df", &value,
                             TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);

    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_null_ctx(void)
{
    TEST("drag_float: NULL ctx returns false");
    float value = TEST_DF_INIT;
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W, TEST_DF_H };
    ASSERT_TRUE(!forge_ui_ctx_drag_float(NULL, "test##df", &value,
                TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect));
}

static void test_drag_float_null_value(void)
{
    TEST("drag_float: NULL value returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_float(&ctx, "test##df", NULL,
                TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_nan_rect_rejected(void)
{
    TEST("drag_float: NaN rect rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float value = TEST_DF_INIT;
    ForgeUiRect rect = { NAN, TEST_DF_Y, TEST_DF_W, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_float(&ctx, "test##df", &value,
                TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_inverted_range_rejected(void)
{
    TEST("drag_float: inverted range (min > max) rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float value = TEST_DF_INIT;
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    /* min=100, max=-100 -> inverted */
    ASSERT_TRUE(!forge_ui_ctx_drag_float(&ctx, "test##df", &value,
                TEST_DF_SPEED, TEST_DF_MAX, TEST_DF_MIN, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_zero_speed_rejected(void)
{
    TEST("drag_float: zero speed rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float value = TEST_DF_INIT;
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_float(&ctx, "test##df", &value,
                0.0f, TEST_DF_MIN, TEST_DF_MAX, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_nan_value_sanitized(void)
{
    TEST("drag_float: NaN *value sanitized to min");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float value = NAN;
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    /* Return value is 'changed', not success — NaN was sanitized on entry */
    forge_ui_ctx_drag_float(&ctx, "test##df", &value,
                             TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_NEAR(value, TEST_DF_MIN, TEST_HSV_EPS);

    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_drag_changes_value(void)
{
    TEST("drag_float: drag motion changes value");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float value = 0.0f;
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W, TEST_DF_H };

    /* Frame 1: mouse down on widget — activation frame should NOT change value */
    forge_ui_ctx_begin(&ctx, TEST_DF_CENTER_X, TEST_DF_CENTER_Y, true);
    forge_ui_ctx_drag_float(&ctx, "test##df", &value,
                             TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_NEAR(value, 0.0f, TEST_HSV_EPS);

    /* Frame 2: drag right by TEST_DF_DRAG_DX pixels */
    forge_ui_ctx_begin(&ctx, TEST_DF_CENTER_X + TEST_DF_DRAG_DX,
                        TEST_DF_CENTER_Y, true);
    bool changed = forge_ui_ctx_drag_float(&ctx, "test##df", &value,
                                            TEST_DF_SPEED, TEST_DF_MIN,
                                            TEST_DF_MAX, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(changed);
    ASSERT_TRUE(value > 0.0f);

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Drag float N tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_drag_float_n_emits_draw_data(void)
{
    TEST("drag_float_n: 3-component emits draw data");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float values[3] = { 1.0f, 2.0f, 3.0f };
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DFN_W, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    /* Return value is 'any_changed', not success */
    forge_ui_ctx_drag_float_n(&ctx, "vec3##dfn", values, 3,
                               TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);

    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_n_count_zero_rejected(void)
{
    TEST("drag_float_n: count 0 rejected — no mutation, no draw data");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float values[1] = { 42.0f };
    float backup[1] = { 42.0f };
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_float_n(&ctx, "test##dfn", values, 0,
                TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect));
    /* Values must be unchanged */
    ASSERT_NEAR(values[0], backup[0], 1e-6f);
    /* No draw data emitted */
    ASSERT_TRUE(ctx.vertex_count == 0);
    ASSERT_TRUE(ctx.index_count == 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_n_count_five_rejected(void)
{
    TEST("drag_float_n: count 5 rejected — no mutation, no draw data");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float values[5] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
    float backup[5] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_float_n(&ctx, "test##dfn", values, 5,
                TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect));
    /* Values must be unchanged */
    for (int i = 0; i < 5; i++) {
        ASSERT_NEAR(values[i], backup[i], 1e-6f);
    }
    /* No draw data emitted */
    ASSERT_TRUE(ctx.vertex_count == 0);
    ASSERT_TRUE(ctx.index_count == 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_n_invalid_speed_rejected(void)
{
    TEST("drag_float_n: invalid speed rejected — no draw data");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float values[2] = { 1.0f, 2.0f };
    float backup[2] = { 1.0f, 2.0f };
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, TEST_DF_W * 2.0f, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    /* Zero speed should be rejected before any drawing */
    ASSERT_TRUE(!forge_ui_ctx_drag_float_n(&ctx, "test##fn", values, 2,
                0.0f, TEST_DF_MIN, TEST_DF_MAX, rect));
    ASSERT_NEAR(values[0], backup[0], 1e-6f);
    ASSERT_NEAR(values[1], backup[1], 1e-6f);
    ASSERT_TRUE(ctx.vertex_count == 0);
    ASSERT_TRUE(ctx.index_count == 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_n_narrow_rect_no_crash(void)
{
    TEST("drag_float_n: very narrow rect skips components gracefully");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float values[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    /* Width of 10px split among 4 components — each gets 2.5px, too
     * narrow for label + field */
    ForgeUiRect rect = { TEST_DF_X, TEST_DF_Y, 10.0f, TEST_DF_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    /* Should not crash or produce inverted geometry */
    forge_ui_ctx_drag_float_n(&ctx, "test##narrow", values, 4,
                               TEST_DF_SPEED, TEST_DF_MIN, TEST_DF_MAX, rect);
    forge_ui_ctx_end(&ctx);

    /* All values should be finite */
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(isfinite(values[i]));
    }

    forge_ui_ctx_free(&ctx);
}

static void test_color_picker_hue_drag_no_360(void)
{
    TEST("color_picker: hue drag at far right stays < 360");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float h = 0.0f, s = 1.0f, v = 1.0f;
    ForgeUiRect rect = { TEST_CP_X, TEST_CP_Y, TEST_CP_W, TEST_CP_H };

    /* Compute hue bar position matching the widget's layout:
     * sv_size = min(rect.h - hue_bar_h - preview_h - 2*gap, rect.w)
     * hue_rect.y = rect.y + sv_size + gap */
    float gap = FORGE_UI_CP_GAP;
    float hue_bar_h = FORGE_UI_CP_HUE_BAR_H;
    float preview_h = FORGE_UI_CP_PREVIEW_H;
    float avail = TEST_CP_H - hue_bar_h - preview_h - 2.0f * gap;
    if (avail < 0.0f) avail = 0.0f;
    float sv_size = avail < TEST_CP_W ? avail : TEST_CP_W;
    float hue_y = TEST_CP_Y + sv_size + gap + hue_bar_h * 0.5f;
    float hue_mid_x = TEST_CP_X + TEST_CP_W * 0.5f;
    float hue_right_x = TEST_CP_X + TEST_CP_W;

    /* Frame 1: hover over the hue bar center to make it hot */
    forge_ui_ctx_begin(&ctx, hue_mid_x, hue_y, false);
    forge_ui_ctx_color_picker(&ctx, "cp##hue360", &h, &s, &v, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 2: press at center to activate the hue widget */
    forge_ui_ctx_begin(&ctx, hue_mid_x, hue_y, true);
    forge_ui_ctx_color_picker(&ctx, "cp##hue360", &h, &s, &v, rect);
    forge_ui_ctx_end(&ctx);

    /* Verify the hue actually changed from the initial 0 */
    ASSERT_TRUE(h > 0.0f);

    /* Frame 3: drag to the far right edge while still holding */
    forge_ui_ctx_begin(&ctx, hue_right_x, hue_y, true);
    forge_ui_ctx_color_picker(&ctx, "cp##hue360", &h, &s, &v, rect);
    forge_ui_ctx_end(&ctx);

    /* Hue should be near 360 but strictly < 360 */
    ASSERT_TRUE(h < 360.0f);
    ASSERT_TRUE(h >= 0.0f);
    /* Should be close to 360 (the nextafterf value) */
    ASSERT_TRUE(h > 350.0f);

    forge_ui_ctx_free(&ctx);
}

static void test_color_picker_narrow_no_label_overflow(void)
{
    TEST("color_picker: narrow rect does not overflow swatch/label");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float h = 120.0f, s = 1.0f, v = 1.0f;
    /* Very narrow rect — swatch_w should be clamped */
    ForgeUiRect rect = { TEST_CP_X, TEST_CP_Y, 20.0f, TEST_CP_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_color_picker(&ctx, "cp##narrow", &h, &s, &v, rect);
    forge_ui_ctx_end(&ctx);

    /* All vertices should be within rect bounds (with text slop) */
    float right = rect.x + rect.w + TEST_CP_TEXT_SLOP;
    for (Uint32 i = 0; i < ctx.vertex_count; i++) {
        ASSERT_TRUE(ctx.vertices[i].pos_x <= right);
    }

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Drag int tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_drag_int_emits_draw_data(void)
{
    TEST("drag_int: emits vertices and indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int value = TEST_DI_INIT;
    ForgeUiRect rect = { TEST_DI_X, TEST_DI_Y, TEST_DI_W, TEST_DI_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    /* Return value is 'changed', not success */
    forge_ui_ctx_drag_int(&ctx, "test##di", &value,
                           TEST_DI_SPEED, TEST_DI_MIN, TEST_DI_MAX, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);

    forge_ui_ctx_free(&ctx);
}

static void test_drag_int_null_value(void)
{
    TEST("drag_int: NULL value returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_DI_X, TEST_DI_Y, TEST_DI_W, TEST_DI_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_int(&ctx, "test##di", NULL,
                TEST_DI_SPEED, TEST_DI_MIN, TEST_DI_MAX, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_int_inverted_range_rejected(void)
{
    TEST("drag_int: inverted range rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int value = TEST_DI_INIT;
    ForgeUiRect rect = { TEST_DI_X, TEST_DI_Y, TEST_DI_W, TEST_DI_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_int(&ctx, "test##di", &value,
                TEST_DI_SPEED, TEST_DI_MAX, TEST_DI_MIN, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_int_nan_speed_rejected(void)
{
    TEST("drag_int: NaN speed rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int value = TEST_DI_INIT;
    ForgeUiRect rect = { TEST_DI_X, TEST_DI_Y, TEST_DI_W, TEST_DI_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_int(&ctx, "test##di", &value,
                NAN, TEST_DI_MIN, TEST_DI_MAX, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_int_no_jump_on_activation(void)
{
    TEST("drag_int: no value change on activation frame");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int value = TEST_DI_INIT;
    ForgeUiRect rect = { TEST_DI_X, TEST_DI_Y, TEST_DI_W, TEST_DI_H };

    /* Frame 1: mouse down — activation frame should not change value */
    float cx = TEST_DI_X + TEST_DI_W * 0.5f;
    float cy = TEST_DI_Y + TEST_DI_H * 0.5f;
    Uint32 expected_id = forge_ui_hash_id(&ctx, "test##di");
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_drag_int(&ctx, "test##di", &value,
                           TEST_DI_SPEED, TEST_DI_MIN, TEST_DI_MAX, rect);
    forge_ui_ctx_end(&ctx);
    /* Verify the widget became active and stays active after end() */
    ASSERT_EQ_U32(ctx.active, expected_id);
    ASSERT_EQ_INT(value, TEST_DI_INIT);

    forge_ui_ctx_free(&ctx);
}

static void test_drag_int_n_emits_draw_data(void)
{
    TEST("drag_int_n: emits vertices and indices for 2 components");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int values[2] = { TEST_DI_INIT, TEST_DI_INIT };
    ForgeUiRect rect = { TEST_DI_X, TEST_DI_Y, TEST_DI_W * 2.0f, TEST_DI_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_drag_int_n(&ctx, "test##din", values, 2,
                             TEST_DI_SPEED, TEST_DI_MIN, TEST_DI_MAX, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);

    forge_ui_ctx_free(&ctx);
}

static void test_drag_int_n_rejects_count_0(void)
{
    TEST("drag_int_n: count 0 rejected — no mutation, no draw data");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int values[1] = { 42 };
    int backup[1] = { 42 };
    ForgeUiRect rect = { TEST_DI_X, TEST_DI_Y, TEST_DI_W * 2.0f, TEST_DI_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_int_n(&ctx, "test##din", values, 0,
                TEST_DI_SPEED, TEST_DI_MIN, TEST_DI_MAX, rect));
    /* Values must be unchanged */
    ASSERT_TRUE(values[0] == backup[0]);
    /* No draw data emitted */
    ASSERT_TRUE(ctx.vertex_count == 0);
    ASSERT_TRUE(ctx.index_count == 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_drag_int_n_rejects_count_5(void)
{
    TEST("drag_int_n: count 5 rejected — no mutation, no draw data");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int values[5] = { 1, 2, 3, 4, 5 };
    int backup[5] = { 1, 2, 3, 4, 5 };
    ForgeUiRect rect = { TEST_DI_X, TEST_DI_Y, TEST_DI_W * 2.0f, TEST_DI_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_drag_int_n(&ctx, "test##din", values, 5,
                TEST_DI_SPEED, TEST_DI_MIN, TEST_DI_MAX, rect));
    /* Values must be unchanged */
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(values[i] == backup[i]);
    }
    /* No draw data emitted */
    ASSERT_TRUE(ctx.vertex_count == 0);
    ASSERT_TRUE(ctx.index_count == 0);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

#define TEST_DI_CLAMP_DRAG_PX 200.0f  /* large drag to overshoot bounds */

static void test_drag_int_clamp_unstick(void)
{
    TEST("drag_int: value unsticks when dragging back from clamp");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int value = TEST_DI_MAX - 1;  /* start near max */
    ForgeUiRect rect = { TEST_DI_X, TEST_DI_Y, TEST_DI_W, TEST_DI_H };
    float cx = TEST_DI_CENTER_X;
    float cy = TEST_DI_CENTER_Y;

    /* Frame 1: activate by pressing inside widget */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_drag_int(&ctx, "test##di", &value,
                           TEST_DI_SPEED, TEST_DI_MIN, TEST_DI_MAX, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(value, TEST_DI_MAX - 1);

    /* Frame 2: drag far right to hit max clamp */
    forge_ui_ctx_begin(&ctx, cx + TEST_DI_CLAMP_DRAG_PX, cy, true);
    forge_ui_ctx_drag_int(&ctx, "test##di", &value,
                           TEST_DI_SPEED, TEST_DI_MIN, TEST_DI_MAX, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(value, TEST_DI_MAX);

    /* Frame 3: drag back left — value must decrease (not stay stuck) */
    float drag_back = cx + TEST_DI_CLAMP_DRAG_PX - 5.0f;
    forge_ui_ctx_begin(&ctx, drag_back, cy, true);
    forge_ui_ctx_drag_int(&ctx, "test##di", &value,
                           TEST_DI_SPEED, TEST_DI_MIN, TEST_DI_MAX, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(value < TEST_DI_MAX);

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Listbox tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_listbox_emits_draw_data(void)
{
    TEST("listbox: emits vertices and indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Alpha", "Beta", "Gamma", "Delta" };
    int selected = 0;
    ForgeUiRect rect = { TEST_LB_X, TEST_LB_Y, TEST_LB_W, TEST_LB_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_listbox(&ctx, "test##lb", &selected, items,
                          TEST_LB_ITEM_COUNT, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);

    forge_ui_ctx_free(&ctx);
}

static void test_listbox_null_items(void)
{
    TEST("listbox: NULL items returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int selected = 0;
    ForgeUiRect rect = { TEST_LB_X, TEST_LB_Y, TEST_LB_W, TEST_LB_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_listbox(&ctx, "test##lb", &selected, NULL,
                                        TEST_LB_ITEM_COUNT, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_listbox_zero_count_rejected(void)
{
    TEST("listbox: zero item_count rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Alpha" };
    int selected = 0;
    ForgeUiRect rect = { TEST_LB_X, TEST_LB_Y, TEST_LB_W, TEST_LB_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_listbox(&ctx, "test##lb", &selected, items,
                                        0, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_listbox_clamps_selected(void)
{
    TEST("listbox: out-of-range *selected clamped to valid range");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Alpha", "Beta", "Gamma", "Delta" };
    int selected = 99;  /* out of range */
    ForgeUiRect rect = { TEST_LB_X, TEST_LB_Y, TEST_LB_W, TEST_LB_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_listbox(&ctx, "test##lb", &selected, items,
                          TEST_LB_ITEM_COUNT, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(selected >= 0 && selected < TEST_LB_ITEM_COUNT);

    forge_ui_ctx_free(&ctx);
}

static void test_listbox_negative_selected_preserved(void)
{
    TEST("listbox: negative *selected preserved (no selection)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Alpha", "Beta", "Gamma", "Delta" };
    int selected = -1;
    ForgeUiRect rect = { TEST_LB_X, TEST_LB_Y, TEST_LB_W, TEST_LB_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_listbox(&ctx, "test##lb", &selected, items,
                          TEST_LB_ITEM_COUNT, rect);
    forge_ui_ctx_end(&ctx);

    /* -1 means "no selection" and should not be clamped */
    ASSERT_EQ_INT(selected, -1);

    forge_ui_ctx_free(&ctx);
}

static void test_listbox_nan_rect_rejected(void)
{
    TEST("listbox: NaN rect rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Alpha", "Beta" };
    int selected = 0;
    ForgeUiRect rect = { NAN, TEST_LB_Y, TEST_LB_W, TEST_LB_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_listbox(&ctx, "test##lb", &selected, items,
                                        2, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_listbox_layout_zero_count_no_layout_gap(void)
{
    TEST("listbox_layout: zero count does not consume layout space");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Alpha" };
    int selected = 0;
    ForgeUiRect lr = { 10.0f, 10.0f, 200.0f, 400.0f };

    /* Baseline: record where the first layout_next lands */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_layout_push(&ctx, lr, FORGE_UI_LAYOUT_VERTICAL, 4.0f, 0.0f);
    ForgeUiRect baseline = forge_ui_ctx_layout_next(&ctx, 24.0f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);

    /* With a rejected listbox_layout (count=0) before the layout_next */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_layout_push(&ctx, lr, FORGE_UI_LAYOUT_VERTICAL, 4.0f, 0.0f);
    ASSERT_TRUE(!forge_ui_ctx_listbox_layout(&ctx, "test##lb0", &selected,
                                              items, 0, 80.0f));
    ForgeUiRect after = forge_ui_ctx_layout_next(&ctx, 24.0f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);

    /* Rejected call should not have shifted the cursor */
    ASSERT_NEAR(baseline.y, after.y, 1.0f);

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Dropdown tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_dropdown_emits_draw_data(void)
{
    TEST("dropdown: emits vertices and indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Low", "Medium", "High" };
    int selected = 0;
    bool open = false;
    ForgeUiRect rect = { TEST_DD_X, TEST_DD_Y, TEST_DD_W, TEST_DD_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_dropdown(&ctx, "test##dd", &selected, &open, items,
                            TEST_DD_ITEM_COUNT, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);

    forge_ui_ctx_free(&ctx);
}

static void test_dropdown_null_open(void)
{
    TEST("dropdown: NULL open returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Low", "Medium", "High" };
    int selected = 0;
    ForgeUiRect rect = { TEST_DD_X, TEST_DD_Y, TEST_DD_W, TEST_DD_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_dropdown(&ctx, "test##dd", &selected, NULL,
                                         items, TEST_DD_ITEM_COUNT, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_dropdown_clamps_selected(void)
{
    TEST("dropdown: out-of-range *selected clamped");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Low", "Medium", "High" };
    int selected = 50;
    bool open = false;
    ForgeUiRect rect = { TEST_DD_X, TEST_DD_Y, TEST_DD_W, TEST_DD_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_dropdown(&ctx, "test##dd", &selected, &open, items,
                            TEST_DD_ITEM_COUNT, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(selected >= 0 && selected < TEST_DD_ITEM_COUNT);

    forge_ui_ctx_free(&ctx);
}

static void test_dropdown_nan_rect_rejected(void)
{
    TEST("dropdown: NaN rect rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Low", "Medium", "High" };
    int selected = 0;
    bool open = false;
    ForgeUiRect rect = { TEST_DD_X, NAN, TEST_DD_W, TEST_DD_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_dropdown(&ctx, "test##dd", &selected, &open,
                                         items, TEST_DD_ITEM_COUNT, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_dropdown_open_emits_more_draw_data(void)
{
    TEST("dropdown: open state emits more vertices than closed");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "Low", "Medium", "High" };
    int selected = 0;
    bool open_closed = false;
    ForgeUiRect rect = { TEST_DD_X, TEST_DD_Y, TEST_DD_W, TEST_DD_H };

    /* Render closed */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_dropdown(&ctx, "test##dd", &selected, &open_closed, items,
                            TEST_DD_ITEM_COUNT, rect);
    forge_ui_ctx_end(&ctx);
    Uint32 closed_verts = ctx.vertex_count;

    /* Render open */
    bool open_true = true;
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_dropdown(&ctx, "test##dd", &selected, &open_true, items,
                            TEST_DD_ITEM_COUNT, rect);
    forge_ui_ctx_end(&ctx);
    Uint32 open_verts = ctx.vertex_count;

    /* Open state draws item rows — must have more vertices */
    ASSERT_TRUE(open_verts > closed_verts);

    forge_ui_ctx_free(&ctx);
}

static void test_dropdown_layout_reserves_for_effective_open(void)
{
    TEST("dropdown_layout: reserves space matching post-toggle open state");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "A", "B", "C" };
    int selected = 0;
    bool open = false;
    float layout_w = 200.0f, layout_h = 600.0f;
    float spacing = 0.0f, padding = 0.0f;

    /* Render with open=false: the layout should reserve only header height */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ForgeUiRect lr = { 10.0f, 10.0f, layout_w, layout_h };
    forge_ui_ctx_layout_push(&ctx, lr, FORGE_UI_LAYOUT_VERTICAL,
                             spacing, padding);
    forge_ui_ctx_dropdown_layout(&ctx, "test##dd", &selected, &open,
                                  items, 3, 30.0f);
    /* Place a second widget and record its y position */
    ForgeUiRect next_closed = forge_ui_ctx_layout_next(&ctx, 20.0f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    float closed_next_y = next_closed.y;

    /* Render with open=true: the layout should reserve header + items */
    open = true;
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_layout_push(&ctx, lr, FORGE_UI_LAYOUT_VERTICAL,
                             spacing, padding);
    forge_ui_ctx_dropdown_layout(&ctx, "test##dd", &selected, &open,
                                  items, 3, 30.0f);
    ForgeUiRect next_open = forge_ui_ctx_layout_next(&ctx, 20.0f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);
    float open_next_y = next_open.y;

    /* When open, the next widget should be placed further down */
    ASSERT_TRUE(open_next_y > closed_next_y);

    forge_ui_ctx_free(&ctx);
}

static void test_dropdown_honors_rect_height(void)
{
    TEST("dropdown: rect.h sets header height when non-zero");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    const char *items[] = { "A", "B", "C" };
    int selected = 0;
    bool open = false;
    float custom_h = 40.0f;
    ForgeUiRect rect = { TEST_DD_X, TEST_DD_Y, TEST_DD_W, custom_h };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_dropdown(&ctx, "test##ddh", &selected, &open,
                           items, TEST_DD_ITEM_COUNT, rect);
    forge_ui_ctx_end(&ctx);

    /* All header vertices should be within the custom height */
    float max_y = 0.0f;
    for (Uint32 i = 0; i < ctx.vertex_count; i++) {
        ASSERT_TRUE(ctx.vertices[i].pos_y <= TEST_DD_Y + custom_h + 1.0f);
        if (ctx.vertices[i].pos_y > max_y) max_y = ctx.vertices[i].pos_y;
    }
    /* The header should actually reach the requested height */
    ASSERT_TRUE(max_y >= TEST_DD_Y + custom_h - 1.0f);

    forge_ui_ctx_free(&ctx);
}

static void test_drag_float_n_layout_rejects_bad_count(void)
{
    TEST("drag_float_n_layout: bad count does not consume layout space");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float values[1] = { 1.0f };
    ForgeUiRect lr = { 10.0f, 10.0f, 200.0f, 400.0f };

    /* Baseline: measure where the first layout_next lands with no
     * preceding widgets, then compare to the same after a rejected call */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_layout_push(&ctx, lr, FORGE_UI_LAYOUT_VERTICAL, 4.0f, 0.0f);
    ForgeUiRect baseline = forge_ui_ctx_layout_next(&ctx, 24.0f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);

    /* Now try with a rejected drag_float_n_layout before the layout_next */
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_layout_push(&ctx, lr, FORGE_UI_LAYOUT_VERTICAL, 4.0f, 0.0f);
    ASSERT_TRUE(!forge_ui_ctx_drag_float_n_layout(&ctx, "test##fn0",
                values, 0, 1.0f, 0.0f, 100.0f, 24.0f));
    ForgeUiRect after = forge_ui_ctx_layout_next(&ctx, 24.0f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);

    /* The rejected call should not have shifted the cursor */
    ASSERT_NEAR(baseline.y, after.y, 1.0f);

    forge_ui_ctx_free(&ctx);
}

static void test_drag_int_n_layout_rejects_bad_count(void)
{
    TEST("drag_int_n_layout: bad count does not consume layout space");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int values[1] = { 1 };
    ForgeUiRect lr = { 10.0f, 10.0f, 200.0f, 400.0f };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_layout_push(&ctx, lr, FORGE_UI_LAYOUT_VERTICAL, 4.0f, 0.0f);
    ForgeUiRect baseline = forge_ui_ctx_layout_next(&ctx, 24.0f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_layout_push(&ctx, lr, FORGE_UI_LAYOUT_VERTICAL, 4.0f, 0.0f);
    ASSERT_TRUE(!forge_ui_ctx_drag_int_n_layout(&ctx, "test##in0",
                values, 0, 1.0f, 0, 100, 24.0f));
    ForgeUiRect after = forge_ui_ctx_layout_next(&ctx, 24.0f);
    forge_ui_ctx_layout_pop(&ctx);
    forge_ui_ctx_end(&ctx);

    ASSERT_NEAR(baseline.y, after.y, 1.0f);

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Radio button tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_radio_emits_draw_data(void)
{
    TEST("radio: emits vertices and indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int selected = 0;
    ForgeUiRect rect = { TEST_RAD_X, TEST_RAD_Y, TEST_RAD_W, TEST_RAD_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    /* Return value is 'changed', not success */
    forge_ui_ctx_radio(&ctx, "opt_a##rad", &selected, 0, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertex_count > 0);
    ASSERT_TRUE(ctx.index_count > 0);

    forge_ui_ctx_free(&ctx);
}

static void test_radio_null_selected(void)
{
    TEST("radio: NULL selected returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_RAD_X, TEST_RAD_Y, TEST_RAD_W, TEST_RAD_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_radio(&ctx, "opt_a##rad", NULL, 0, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_radio_click_changes_selection(void)
{
    TEST("radio: click changes *selected to option_value");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int selected = 0;
    ForgeUiRect rect = { TEST_RAD_X, TEST_RAD_Y, TEST_RAD_W, TEST_RAD_H };

    /* Frame 1: press */
    forge_ui_ctx_begin(&ctx, TEST_RAD_CENTER_X, TEST_RAD_CENTER_Y, true);
    forge_ui_ctx_radio(&ctx, "opt_b##rad", &selected, 1, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 2: release */
    forge_ui_ctx_begin(&ctx, TEST_RAD_CENTER_X, TEST_RAD_CENTER_Y, false);
    bool changed = forge_ui_ctx_radio(&ctx, "opt_b##rad", &selected, 1, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(changed);
    ASSERT_EQ_INT(selected, 1);

    forge_ui_ctx_free(&ctx);
}

static void test_radio_nan_rect_rejected(void)
{
    TEST("radio: NaN rect rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int selected = 0;
    ForgeUiRect rect = { TEST_RAD_X, TEST_RAD_Y, NAN, TEST_RAD_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_radio(&ctx, "opt_a##rad", &selected, 0, rect));
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_radio_independent_groups(void)
{
    TEST("radio: separate groups do not interfere");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    int group_a = 0;
    int group_b = 2;
    ForgeUiRect rect_a = { TEST_RAD_X, TEST_RAD_Y, TEST_RAD_W, TEST_RAD_H };
    ForgeUiRect rect_b = { TEST_RAD_X, TEST_RAD_B_Y, TEST_RAD_W, TEST_RAD_H };

    /* Click on group_a option 1 — IDs must be distinct across groups */
    forge_ui_ctx_begin(&ctx, TEST_RAD_CENTER_X, TEST_RAD_CENTER_Y, true);
    forge_ui_ctx_radio(&ctx, "a_opt1##rad_a1", &group_a, 1, rect_a);
    forge_ui_ctx_radio(&ctx, "b_opt0##rad_b0", &group_b, 0, rect_b);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_begin(&ctx, TEST_RAD_CENTER_X, TEST_RAD_CENTER_Y, false);
    forge_ui_ctx_radio(&ctx, "a_opt1##rad_a1", &group_a, 1, rect_a);
    forge_ui_ctx_radio(&ctx, "b_opt0##rad_b0", &group_b, 0, rect_b);
    forge_ui_ctx_end(&ctx);

    /* group_a changed to 1, group_b unchanged at 2 */
    ASSERT_EQ_INT(group_a, 1);
    ASSERT_EQ_INT(group_b, 2);

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Color picker tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_color_picker_emits_draw_data(void)
{
    TEST("color_picker: emits vertices and indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float h = 180.0f, s = 0.5f, v = 0.8f;
    ForgeUiRect rect = { TEST_CP_X, TEST_CP_Y, TEST_CP_W, TEST_CP_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    /* Return value is 'changed', not success */
    forge_ui_ctx_color_picker(&ctx, "cp##test", &h, &s, &v, rect);
    forge_ui_ctx_end(&ctx);

    /* Color picker emits SV grid + hue bar + preview: many vertices */
    ASSERT_TRUE(ctx.vertex_count > 100);
    ASSERT_TRUE(ctx.index_count > 100);

    forge_ui_ctx_free(&ctx);
}

static void test_color_picker_null_hsv(void)
{
    TEST("color_picker: NULL h/s/v returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float h = 0.0f, s = 1.0f;
    ForgeUiRect rect = { TEST_CP_X, TEST_CP_Y, TEST_CP_W, TEST_CP_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_color_picker(&ctx, "cp##test", &h, &s, NULL, rect));
    ASSERT_TRUE(!forge_ui_ctx_color_picker(&ctx, "cp##test", &h, NULL, &s, rect));
    ASSERT_TRUE(!forge_ui_ctx_color_picker(&ctx, "cp##test", NULL, &s, &h, rect));
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_free(&ctx);
}

static void test_color_picker_nan_rect_rejected(void)
{
    TEST("color_picker: NaN rect rejected");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float h = 0.0f, s = 1.0f, v = 1.0f;
    ForgeUiRect rect = { TEST_CP_X, TEST_CP_Y, TEST_CP_W, NAN };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    ASSERT_TRUE(!forge_ui_ctx_color_picker(&ctx, "cp##test", &h, &s, &v, rect));
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_free(&ctx);
}

#define TEST_CP_SV_CLICK_OFFSET 0.25f  /* fraction into SV area for click */

static void test_color_picker_sv_click_updates_sv(void)
{
    TEST("color_picker: clicking SV area updates s and v");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float h = 120.0f, s = 0.0f, v = 0.0f;
    ForgeUiRect rect = { TEST_CP_X, TEST_CP_Y, TEST_CP_W, TEST_CP_H };

    /* SV area occupies rect.y .. rect.y + sv_size.  With default scale=1
     * the constants come from forge_ui_ctx.h; sv_size = H - hue - preview - 2*gap.
     * Click at 25% into the SV area from top-left */
    float sv_size = TEST_CP_H - FORGE_UI_CP_HUE_BAR_H
                    - FORGE_UI_CP_PREVIEW_H - 2.0f * FORGE_UI_CP_GAP;
    float click_x = TEST_CP_X + sv_size * TEST_CP_SV_CLICK_OFFSET;
    float click_y = TEST_CP_Y + sv_size * TEST_CP_SV_CLICK_OFFSET;

    /* Frame 1: press inside SV area */
    forge_ui_ctx_begin(&ctx, click_x, click_y, true);
    bool changed = forge_ui_ctx_color_picker(&ctx, "cp##test",
                                               &h, &s, &v, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(changed);
    /* s = click fraction along SV width ≈ TEST_CP_SV_CLICK_OFFSET
     * v = 1 - click fraction along SV height ≈ 1 - TEST_CP_SV_CLICK_OFFSET */
    float expected_s = TEST_CP_SV_CLICK_OFFSET;
    float expected_v = 1.0f - TEST_CP_SV_CLICK_OFFSET;
    float tol = 0.05f;
    ASSERT_TRUE(SDL_fabsf(s - expected_s) < tol);
    ASSERT_TRUE(SDL_fabsf(v - expected_v) < tol);

    forge_ui_ctx_free(&ctx);
}

static void test_color_picker_hue_360_clamped(void)
{
    TEST("color_picker: h=360 is clamped to < 360 (no cursor snap)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* fmodf(360, 360) = 0 — hue 360 and hue 0 are identical.
     * The key invariant is that *h is always strictly < 360.0f after
     * normalization, preventing cursor-position discontinuities. */
    float h = 360.0f, s = 1.0f, v = 1.0f;
    ForgeUiRect rect = { TEST_CP_X, TEST_CP_Y, TEST_CP_W, TEST_CP_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_color_picker(&ctx, "cp##test", &h, &s, &v, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(h >= 0.0f);
    ASSERT_TRUE(h < 360.0f);

    /* Also test a value just below 360 that fmodf preserves — the
     * nextafterf guard should keep it strictly below 360. */
    h = nextafterf(360.0f, 0.0f);
    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_color_picker(&ctx, "cp##test", &h, &s, &v, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(h >= 0.0f);
    ASSERT_TRUE(h < 360.0f);

    forge_ui_ctx_free(&ctx);
}

static void test_color_picker_short_rect_no_overflow(void)
{
    TEST("color_picker: short rect does not overflow bounds");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float h = 0.0f, s = 1.0f, v = 1.0f;
    /* Very short rect — should not overflow */
    ForgeUiRect rect = { TEST_CP_X, TEST_CP_Y, TEST_CP_W, TEST_CP_SHORT_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    forge_ui_ctx_color_picker(&ctx, "cp##test", &h, &s, &v, rect);
    forge_ui_ctx_end(&ctx);

    /* The widget should still emit geometry */
    ASSERT_TRUE(ctx.vertex_count > 0);

    /* Verify no vertex is placed far below rect bottom.  Allow a small
     * tolerance for text glyph descenders which naturally extend past
     * the container rect.  The original bug (hard 20px sv_size minimum)
     * would overshoot by ~56px — this catches that while allowing text. */
    float bottom = rect.y + rect.h;
    for (Uint32 i = 0; i < ctx.vertex_count; i++) {
        ASSERT_TRUE(ctx.vertices[i].pos_y <= bottom + TEST_CP_TEXT_SLOP);
    }

    forge_ui_ctx_free(&ctx);
}

static void test_color_picker_zero_width_no_crash(void)
{
    TEST("color_picker: zero-width rect does not divide by zero");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float h = 180.0f, s = 0.5f, v = 0.5f;
    ForgeUiRect normal_rect = { TEST_CP_X, TEST_CP_Y, TEST_CP_W, TEST_CP_H };

    /* Frame 1: press inside the SV area to make the widget active */
    float sv_mx = TEST_CP_X + TEST_CP_W * 0.5f;
    float sv_my = TEST_CP_Y + 10.0f;
    forge_ui_ctx_begin(&ctx, sv_mx, sv_my, false);
    forge_ui_ctx_color_picker(&ctx, "cp##zero", &h, &s, &v, normal_rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 2: mouse down (press) to activate the SV widget */
    forge_ui_ctx_begin(&ctx, sv_mx, sv_my, true);
    forge_ui_ctx_color_picker(&ctx, "cp##zero", &h, &s, &v, normal_rect);
    forge_ui_ctx_end(&ctx);

    /* Verify the widget became active (if activation regresses, the
     * zero-width frame below would not exercise the guarded path) */
    ASSERT_TRUE(ctx.active != FORGE_UI_ID_NONE);

    /* Frame 3: keep mouse down but switch to zero-width rect —
     * this exercises the guarded divide-by-zero path while active */
    ForgeUiRect zero_rect = { TEST_CP_X, TEST_CP_Y, 0.0f, TEST_CP_H };
    forge_ui_ctx_begin(&ctx, sv_mx, sv_my, true);
    forge_ui_ctx_color_picker(&ctx, "cp##zero", &h, &s, &v, zero_rect);
    forge_ui_ctx_end(&ctx);

    /* HSV values should not become NaN or Inf */
    ASSERT_TRUE(isfinite(h));
    ASSERT_TRUE(isfinite(s));
    ASSERT_TRUE(isfinite(v));

    forge_ui_ctx_free(&ctx);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Gradient rect tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_gradient_rect_emits_4_verts(void)
{
    TEST("emit_gradient_rect: emits 4 vertices and 6 indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { TEST_EMIT_X, TEST_EMIT_Y, TEST_EMIT_W, TEST_EMIT_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);
    int v_before = ctx.vertex_count;
    int i_before = ctx.index_count;
    forge_ui__emit_gradient_rect(&ctx, rect,
                                  1.0f, 0.0f, 0.0f,  /* TL red */
                                  0.0f, 1.0f, 0.0f,  /* TR green */
                                  0.0f, 0.0f, 1.0f,  /* BR blue */
                                  1.0f, 1.0f, 0.0f);  /* BL yellow */
    forge_ui_ctx_end(&ctx);

    ASSERT_EQ_INT(ctx.vertex_count - v_before, 4);
    ASSERT_EQ_INT(ctx.index_count - i_before, 6);

    forge_ui_ctx_free(&ctx);
}

static void test_gradient_rect_null_ctx(void)
{
    TEST("emit_gradient_rect: NULL ctx does not crash");
    ForgeUiRect rect = { TEST_EMIT_X, TEST_EMIT_Y, TEST_EMIT_W, TEST_EMIT_H };
    forge_ui__emit_gradient_rect(NULL, rect,
                                  1.0f, 0.0f, 0.0f,
                                  0.0f, 1.0f, 0.0f,
                                  0.0f, 0.0f, 1.0f,
                                  1.0f, 1.0f, 0.0f);
    /* no crash = pass */
}

#define TEST_GRAD_CLIP_X    50.0f
#define TEST_GRAD_CLIP_Y    50.0f
#define TEST_GRAD_CLIP_W    100.0f
#define TEST_GRAD_CLIP_H    100.0f

static void test_gradient_rect_clipped(void)
{
    TEST("emit_gradient_rect: clipping trims rect and interpolates colors");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect full_rect = { TEST_GRAD_CLIP_X, TEST_GRAD_CLIP_Y,
                               TEST_GRAD_CLIP_W, TEST_GRAD_CLIP_H };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    /* Set clip rect after begin (begin resets has_clip) */
    ctx.has_clip = true;
    ctx.clip_rect = (ForgeUiRect){
        TEST_GRAD_CLIP_X, TEST_GRAD_CLIP_Y,
        TEST_GRAD_CLIP_W * 0.5f, TEST_GRAD_CLIP_H * 0.5f
    };
    forge_ui__emit_gradient_rect(&ctx, full_rect,
                                  1.0f, 0.0f, 0.0f,   /* TL red */
                                  0.0f, 1.0f, 0.0f,   /* TR green */
                                  0.0f, 0.0f, 1.0f,   /* BR blue */
                                  1.0f, 1.0f, 0.0f);  /* BL yellow */
    forge_ui_ctx_end(&ctx);

    /* Should still emit 4 vertices and 6 indices (clipped quad) */
    ASSERT_EQ_INT(ctx.vertex_count, 4);
    ASSERT_EQ_INT(ctx.index_count, 6);

    /* All vertex positions must lie within the clip rect */
    float cx1 = TEST_GRAD_CLIP_X + TEST_GRAD_CLIP_W * 0.5f;
    float cy1 = TEST_GRAD_CLIP_Y + TEST_GRAD_CLIP_H * 0.5f;
    for (int i = 0; i < ctx.vertex_count; i++) {
        ASSERT_TRUE(ctx.vertices[i].pos_x >= TEST_GRAD_CLIP_X - TEST_HSV_EPS);
        ASSERT_TRUE(ctx.vertices[i].pos_x <= cx1 + TEST_HSV_EPS);
        ASSERT_TRUE(ctx.vertices[i].pos_y >= TEST_GRAD_CLIP_Y - TEST_HSV_EPS);
        ASSERT_TRUE(ctx.vertices[i].pos_y <= cy1 + TEST_HSV_EPS);
    }

    /* Top-left corner should still be red (unchanged by clip) */
    ASSERT_NEAR(ctx.vertices[0].r, 1.0f, TEST_HSV_EPS);
    ASSERT_NEAR(ctx.vertices[0].g, 0.0f, TEST_HSV_EPS);
    ASSERT_NEAR(ctx.vertices[0].b, 0.0f, TEST_HSV_EPS);

    /* Top-right corner should be midpoint of red and green (0.5, 0.5, 0) */
    ASSERT_NEAR(ctx.vertices[1].r, 0.5f, TEST_HSV_EPS);
    ASSERT_NEAR(ctx.vertices[1].g, 0.5f, TEST_HSV_EPS);
    ASSERT_NEAR(ctx.vertices[1].b, 0.0f, TEST_HSV_EPS);

    forge_ui_ctx_free(&ctx);
}

static void test_gradient_rect_fully_clipped(void)
{
    TEST("emit_gradient_rect: fully outside clip emits nothing");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect full_rect = { 200.0f, 200.0f, 100.0f, 100.0f };

    forge_ui_ctx_begin(&ctx, TEST_MOUSE_FAR, TEST_MOUSE_FAR, false);

    /* Set clip rect after begin (begin resets has_clip) */
    ctx.has_clip = true;
    ctx.clip_rect = (ForgeUiRect){ 0.0f, 0.0f, 50.0f, 50.0f };
    forge_ui__emit_gradient_rect(&ctx, full_rect,
                                  1.0f, 0.0f, 0.0f,
                                  0.0f, 1.0f, 0.0f,
                                  0.0f, 0.0f, 1.0f,
                                  1.0f, 1.0f, 0.0f);
    forge_ui_ctx_end(&ctx);

    ASSERT_EQ_INT(ctx.vertex_count, 0);
    ASSERT_EQ_INT(ctx.index_count, 0);

    forge_ui_ctx_free(&ctx);
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== UI Context Tests (forge_ui_ctx.h) ===");

    /* Hit testing */
    test_rect_contains_inside();
    test_rect_contains_outside();
    test_rect_contains_left_edge();
    test_rect_contains_right_edge();
    test_rect_contains_top_edge();
    test_rect_contains_bottom_edge();
    test_rect_contains_zero_size();

    /* Ascender pixel helper */
    test_ascender_px_known_values();
    test_ascender_px_real_atlas();
    test_ascender_px_null_atlas();
    test_ascender_px_zero_upm();
    test_ascender_px_zero_ascender();

    /* Init */
    test_init_success();
    test_init_null_ctx();
    test_init_null_atlas();

    /* Free */
    test_free_zeroes_state();
    test_free_null_ctx();
    test_free_double_free();

    /* Begin */
    test_begin_updates_input();
    test_begin_resets_draw_data();
    test_begin_tracks_mouse_prev();
    test_begin_null_ctx();

    /* End */
    test_end_promotes_hot();
    test_end_freezes_hot_when_active();
    test_end_clears_stuck_active();
    test_end_null_ctx();

    /* Labels */
    test_label_emits_vertices();
    test_label_empty_string();
    test_label_null_text();
    test_label_null_ctx();

    /* Buttons — basic */
    test_button_emits_draw_data();
    test_button_returns_false_no_click();
    test_button_click_sequence();
    test_button_click_release_outside();
    test_button_hot_state();
    test_button_empty_label_rejected();
    test_button_null_ctx();
    test_button_null_text();

    /* Edge-triggered activation */
    test_button_edge_trigger_no_false_activate();
    test_button_edge_trigger_activates_on_press();

    /* Multiple buttons */
    test_multiple_buttons_last_hot_wins();
    test_multiple_buttons_independent();
    test_overlap_press_last_drawn_wins();

    /* Draw data verification */
    test_button_rect_uses_white_uv();
    test_button_normal_color();
    test_button_hot_color();
    test_button_active_color();
    test_rect_ccw_winding();
    test_rect_vertex_positions();

    /* Buffer growth */
    test_grow_vertices_from_zero();
    test_grow_indices_from_zero();
    test_grow_vertices_negative_count();
    test_grow_indices_negative_count();
    test_grow_vertices_zero_count();
    test_grow_many_widgets();

    /* Index offsets */
    test_multiple_rects_index_offsets();

    /* NULL/edge case guards */
    test_emit_rect_null_atlas();
    test_emit_text_layout_null();
    test_emit_text_layout_empty();

    /* Checkboxes */
    test_checkbox_emits_draw_data();
    test_checkbox_checked_emits_inner_fill();
    test_checkbox_toggle_sequence();
    test_checkbox_no_toggle_release_outside();
    test_checkbox_null_ctx();
    test_checkbox_null_label();
    test_checkbox_null_value();
    test_checkbox_empty_label_rejected();
    test_checkbox_null_atlas();
    test_checkbox_normal_color();
    test_checkbox_hot_color();
    test_checkbox_active_color();
    test_checkbox_edge_trigger();

    /* Sliders */
    test_slider_emits_draw_data();
    test_slider_value_snap_on_click();
    test_slider_drag_outside_bounds();
    test_slider_release_clears_active();
    test_slider_value_mapping();
    test_slider_null_ctx();
    test_slider_null_value();
    test_slider_empty_label_rejected();
    test_slider_null_atlas();
    test_slider_invalid_range();
    test_slider_nan_range_rejected();
    test_slider_narrow_rect();
    test_slider_normal_color();
    test_slider_hot_color();
    test_slider_active_color();
    test_slider_track_uses_white_uv();
    test_slider_edge_trigger();
    test_slider_returns_false_when_same_value();

    /* Button NULL atlas (audit fix) */
    test_button_null_atlas();

    /* Keyboard input */
    test_set_keyboard_basic();
    test_set_keyboard_null_ctx();
    test_begin_resets_keyboard();

    /* Border */
    test_emit_border_basic();
    test_emit_border_null_ctx();
    test_emit_border_zero_width();
    test_emit_border_negative_width();
    test_emit_border_too_wide();

    /* Text input -- parameter validation */
    test_text_input_null_ctx();
    test_text_input_null_state();
    test_text_input_null_buffer();
    test_text_input_empty_label_rejected();
    test_text_input_zero_capacity();
    test_text_input_negative_capacity();
    test_text_input_negative_length();
    test_text_input_length_exceeds_capacity();
    test_text_input_negative_cursor();
    test_text_input_cursor_exceeds_length();

    /* Text input -- focus */
    test_text_input_focus_click_sequence();
    test_text_input_focus_release_outside();
    test_text_input_unfocus_click_outside();
    test_text_input_unfocus_escape();
    test_text_input_escape_clears_active();

    /* Text input -- character insertion */
    test_text_input_insert_chars();
    test_text_input_insert_mid_string();
    test_text_input_insert_at_capacity();

    /* Text input -- backspace/delete */
    test_text_input_backspace();
    test_text_input_backspace_at_start();
    test_text_input_backspace_empty();
    test_text_input_delete_key();
    test_text_input_delete_at_end();

    /* Text input -- cursor movement */
    test_text_input_cursor_left_right();
    test_text_input_cursor_home_end();
    test_text_input_left_at_start();
    test_text_input_right_at_end();

    /* Text input -- mutual exclusion */
    test_text_input_backspace_beats_insert();
    test_text_input_delete_beats_insert();
    test_text_input_backspace_blocks_cursor_move();
    test_text_input_insert_blocks_cursor_move();

    /* Text input -- draw data */
    test_text_input_emits_draw_data();
    test_text_input_focused_emits_border();
    test_text_input_not_focused_no_keyboard();

    /* Text input -- overlap priority */
    test_text_input_overlap_last_drawn_wins();

    /* Text input -- null-termination validation (audit fix) */
    test_text_input_bad_null_termination();

    /* Text input -- clipped visibility */
    test_text_input_clipped_ignores_keyboard();
    test_text_input_partially_visible_accepts_keyboard();

    /* Layout -- push/pop basics */
    test_layout_push_returns_true();
    test_layout_pop_returns_true();
    test_layout_push_null_ctx();
    test_layout_pop_null_ctx();
    test_layout_pop_empty_stack();
    test_layout_push_overflow();
    test_layout_nested_push_pop();

    /* Layout -- layout_next positioning */
    test_layout_next_null_ctx();
    test_layout_next_no_active_layout();
    test_layout_vertical_positions();
    test_layout_horizontal_positions();
    test_layout_remaining_after_last_widget();

    /* Layout -- parameter validation */
    test_layout_push_negative_padding_clamped();
    test_layout_push_negative_spacing_clamped();
    test_layout_next_negative_size_clamped();
    test_layout_push_tiny_rect_no_negative_remaining();

    /* Layout -- lifecycle */
    test_layout_begin_resets_depth();
    test_layout_free_resets_depth();

    /* Layout -- widget parameter validation */
    test_button_layout_null_text();
    test_button_layout_empty_text();
    test_checkbox_layout_null_value();
    test_checkbox_layout_null_label();
    test_slider_layout_null_value();
    test_slider_layout_invalid_range();

    /* Layout -- widget positioning */
    test_label_layout_emits_draw_data();
    test_button_layout_correct_rect();

    /* Layout -- spacing model */
    test_layout_no_spacing_before_first_widget();
    test_layout_spacing_between_widgets();
    test_layout_horizontal_spacing();
    test_layout_item_count();

    /* Layout -- direction validation */
    test_layout_push_invalid_direction_rejected();
    test_layout_push_negative_direction_rejected();
    test_layout_push_valid_directions_accepted();

    /* Layout -- wrappers no-op without active layout */
    test_label_layout_noop_without_layout();
    test_button_layout_noop_without_layout();
    test_checkbox_layout_noop_without_layout();
    test_slider_layout_noop_without_layout();

    /* Layout -- wrappers no cursor advance on null atlas */
    test_button_layout_null_atlas_no_advance();

    /* Panels -- widget_mouse_over (clip-aware hit test) */
    test_widget_mouse_over_no_clip();
    test_widget_mouse_over_outside();
    test_widget_mouse_over_clipped();
    test_widget_mouse_over_inside_clip();

    /* Panels -- emit_rect clipping */
    test_emit_rect_clip_discard();
    test_emit_rect_clip_trim();

    /* Panels -- emit_quad_clipped */
    test_emit_quad_clipped_fully_outside();
    test_emit_quad_clipped_uv_remap();
    test_emit_quad_clipped_degenerate();
    test_emit_quad_clipped_zero_height();

    /* Panels -- panel_begin parameter validation */
    test_panel_begin_null_ctx();
    test_panel_begin_null_scroll_y();
    test_panel_begin_empty_title_rejected();
    test_panel_begin_null_title_rejected();
    test_panel_begin_long_title_ok();
    test_panel_begin_nested_rejected();
    test_panel_begin_zero_width_rejected();
    test_panel_begin_negative_height_rejected();
    test_panel_begin_nan_width_rejected();
    test_panel_begin_nan_height_rejected();
    test_panel_begin_nan_x_rejected();
    test_panel_begin_inf_x_rejected();
    test_panel_begin_nan_y_rejected();
    test_panel_begin_inf_y_rejected();
    test_panel_begin_nan_scroll_sanitized();
    test_panel_begin_negative_scroll_sanitized();

    /* Panels -- lifecycle */
    test_panel_begin_sets_clip();
    test_panel_end_clears_clip();
    test_panel_end_without_begin();
    test_panel_end_clamps_scroll();
    test_panel_layout_push_pop_balanced();

    /* Panels -- safety nets */
    test_ctx_end_cleans_up_active_panel();
    test_panel_begin_layout_stack_full();

    /* Panels -- scroll offset */
    test_panel_scroll_offset_applied();
    test_panel_scroll_zero_no_offset();

    /* Panels -- scrollbar */
    test_panel_end_thumb_clamped_to_track();
    test_panel_end_scrollbar_on_overflow();
    test_panel_end_no_scrollbar_when_fits();
    test_panel_end_no_scrollbar_zero_track();

    /* Panels -- draw data and features */
    test_panel_begin_emits_draw_data();
    test_panel_begin_null_title_ok();
    test_panel_mouse_wheel_scroll();
    test_panel_mouse_wheel_nan_ignored();
    test_panel_mouse_wheel_inf_ignored();
    test_panel_mouse_wheel_neg_inf_ignored();

    /* Panels -- scroll pre-clamp */
    test_panel_scroll_preclamp_on_panel_resize();
    test_panel_preclamp_skips_different_panel();
    test_panel_scroll_content_shrink_one_frame_lag();

    /* Panels -- ctx_begin reset */
    test_ctx_begin_resets_panel_state();

    /* Panels -- cleanup */
    test_free_clears_panel_fields();

    /* Hash / ID helpers */
    test_hash_id_deterministic();
    test_hash_id_different_labels();
    test_hash_id_separator();
    test_push_pop_id_scoping();
    test_display_text_separator();

    /* Audit fix: fnv1a */
    test_fnv1a_null_returns_seed();
    test_fnv1a_empty_string_returns_seed();
    test_fnv1a_different_strings_differ();
    test_fnv1a_same_string_same_hash();
    test_fnv1a_different_seeds_differ();

    /* Audit fix: display_end */
    test_display_end_null_returns_null();
    test_display_end_no_separator();
    test_display_end_with_separator();
    test_display_end_double_separator();
    test_display_end_only_separator();
    test_display_end_empty_string();

    /* Audit fix: hash_id edge cases */
    test_hash_id_null_label_returns_one();
    test_hash_id_empty_label_returns_one();
    test_hash_id_null_ctx_uses_default_seed();
    test_hash_id_separator_changes_hash();

    /* Audit fix: push_id / pop_id */
    test_push_id_null_ctx_no_crash();
    test_pop_id_null_ctx_no_crash();
    test_pop_id_underflow_no_crash();
    test_push_id_overflow_no_crash();
    test_push_id_changes_hash();
    test_ctx_begin_resets_id_stack();

    /* Audit fix: panel ## separator */
    test_panel_title_strips_separator();
    test_panel_separator_different_ids();

    /* Audit fix: button ## separator */
    test_button_separator_display_text();
    test_button_separator_different_ids();

    /* Scale and spacing validation (Phase 2 audit) */
    test_begin_resets_zero_scale();
    test_begin_resets_negative_scale();
    test_begin_resets_nan_scale();
    test_begin_resets_inf_scale();
    test_begin_preserves_valid_scale();
    test_init_zero_pixel_height_rejected();
    test_init_nan_pixel_height_rejected();
    test_init_inf_pixel_height_rejected();
    test_init_negative_pixel_height_rejected();

    /* State cleanup on early return (Phase 2 audit) */
    test_panel_begin_failure_clears_panel_rect();
    test_ctx_begin_zeroes_panel_geometry();
    test_emit_border_nan_width_rejected();

    /* NaN/Inf validation */
    test_label_nan_x_rejected();
    test_label_inf_y_rejected();
    test_label_colored_nan_r_rejected();
    test_label_colored_nan_g_rejected();
    test_label_colored_nan_b_rejected();
    test_label_colored_nan_a_rejected();
    test_label_colored_inf_r_rejected();
    test_label_colored_inf_neg_g_rejected();
    test_label_colored_out_of_range_clamped();
    test_button_nan_rect_x_rejected();
    test_button_inf_rect_w_rejected();
    test_button_neg_inf_rect_h_rejected();
    test_checkbox_nan_rect_rejected();
    test_checkbox_inf_rect_rejected();
    test_slider_nan_rect_rejected();
    test_slider_inf_rect_rejected();
    test_slider_nan_value_sanitized();
    test_slider_inf_value_sanitized();
    test_text_input_nan_rect_rejected();
    test_text_input_inf_rect_rejected();
    test_layout_push_nan_rect_rejected();
    test_layout_push_inf_rect_rejected();
    test_slider_inf_min_rejected();
    test_slider_inf_max_rejected();
    test_slider_layout_inf_range_rejected();
    test_ascender_px_inf_pixel_height();
    test_ascender_px_nan_pixel_height();

    /* Solid rect */
    test_rect_basic();
    test_rect_null_ctx();
    test_rect_nan_rect_rejected();
    test_rect_inf_rect_rejected();
    test_rect_nan_color_rejected();
    test_rect_inf_color_rejected();
    test_rect_color_clamped();
    test_rect_layout_basic();
    test_rect_layout_no_layout();
    test_rect_layout_nan_color_no_advance();

    /* Progress bar */
    test_progress_bar_basic();
    test_progress_bar_zero_value();
    test_progress_bar_full_value();
    test_progress_bar_over_max_clamped();
    test_progress_bar_negative_value();
    test_progress_bar_zero_max_rejected();
    test_progress_bar_negative_max_rejected();
    test_progress_bar_nan_value();
    test_progress_bar_nan_max_rejected();
    test_progress_bar_inf_rect_rejected();
    test_progress_bar_nan_rect_rejected();
    test_progress_bar_null_ctx();
    test_progress_bar_inf_value();
    test_progress_bar_neg_inf_value();
    test_progress_bar_inf_max_rejected();
    test_progress_bar_neg_inf_max_rejected();
    test_progress_bar_nan_fill_color_rejected();
    test_progress_bar_inf_fill_color_rejected();
    test_progress_bar_fill_color_clamped();
    test_progress_bar_layout_basic();
    test_progress_bar_layout_no_layout();
    test_progress_bar_layout_zero_max_rejected();
    test_progress_bar_layout_cursor_advance();
    test_progress_bar_layout_inf_max_rejected();
    test_progress_bar_layout_nan_fill_no_advance();

    /* Separator */
    test_separator_emits_rect();
    test_separator_null_ctx();
    test_separator_nan_rect_rejected();
    test_separator_uses_thickness_constant();
    test_separator_zero_height_rejected();
    test_separator_zero_width_rejected();
    test_separator_clamps_thickness_to_rect_height();
    test_separator_negative_dims_rejected();

    /* Tree node */
    test_tree_push_pop_basic();
    test_tree_push_closed_returns_false();
    test_tree_push_null_label();
    test_tree_push_null_open();
    test_tree_push_empty_label();
    test_tree_nested_push_pop();
    test_tree_nested_closed_child_does_not_pop_parent();
    test_tree_pop_without_push();
    test_tree_pop_null_ctx();
    test_tree_push_nan_rect_rejected();
    test_tree_toggle_on_click();
    test_tree_push_layout_no_layout();
    test_tree_push_layout_null_label_no_cursor_advance();
    test_tree_depth_exceeds_max();
    test_tree_call_depth_tracks_correctly();

    /* Sparkline */
    test_sparkline_emits_draw_data();
    test_sparkline_null_values_rejected();
    test_sparkline_count_below_two_rejected();
    test_sparkline_equal_range_rejected();
    test_sparkline_inverted_range_rejected();
    test_sparkline_nan_range_rejected();
    test_sparkline_nan_rect_rejected();
    test_sparkline_zero_width_rejected();
    test_sparkline_nan_color_rejected();
    test_sparkline_nan_value_clamped();
    test_sparkline_null_ctx();
    test_sparkline_out_of_range_color_clamped();
    test_sparkline_layout_no_layout();
    test_sparkline_layout_invalid_no_cursor_advance();

    /* HSV/RGB conversion */
    test_hsv_to_rgb_red();
    test_hsv_to_rgb_green();
    test_hsv_to_rgb_blue();
    test_hsv_to_rgb_null_output();
    test_hsv_to_rgb_nan_hue();
    test_hsv_to_rgb_negative_hue();
    test_hsv_to_rgb_nan_saturation();
    test_hsv_to_rgb_inf_saturation();
    test_hsv_to_rgb_nan_value();
    test_hsv_to_rgb_inf_value();
    test_hsv_to_rgb_neg_inf_saturation();
    test_hsv_to_rgb_neg_inf_value();
    test_rgb_to_hsv_red();
    test_rgb_to_hsv_green();
    test_rgb_to_hsv_blue();
    test_rgb_to_hsv_null_output();
    test_rgb_to_hsv_nan_input();
    test_hsv_rgb_roundtrip();

    /* Drag float */
    test_drag_float_emits_draw_data();
    test_drag_float_null_ctx();
    test_drag_float_null_value();
    test_drag_float_nan_rect_rejected();
    test_drag_float_inverted_range_rejected();
    test_drag_float_zero_speed_rejected();
    test_drag_float_nan_value_sanitized();
    test_drag_float_drag_changes_value();

    /* Drag float N */
    test_drag_float_n_emits_draw_data();
    test_drag_float_n_count_zero_rejected();
    test_drag_float_n_count_five_rejected();
    test_drag_float_n_invalid_speed_rejected();
    test_drag_float_n_narrow_rect_no_crash();

    /* Drag int */
    test_drag_int_emits_draw_data();
    test_drag_int_null_value();
    test_drag_int_inverted_range_rejected();
    test_drag_int_nan_speed_rejected();
    test_drag_int_no_jump_on_activation();
    test_drag_int_n_emits_draw_data();
    test_drag_int_n_rejects_count_0();
    test_drag_int_n_rejects_count_5();
    test_drag_int_clamp_unstick();

    /* Listbox */
    test_listbox_emits_draw_data();
    test_listbox_null_items();
    test_listbox_zero_count_rejected();
    test_listbox_layout_zero_count_no_layout_gap();
    test_listbox_clamps_selected();
    test_listbox_negative_selected_preserved();
    test_listbox_nan_rect_rejected();

    /* Dropdown */
    test_dropdown_emits_draw_data();
    test_dropdown_null_open();
    test_dropdown_clamps_selected();
    test_dropdown_nan_rect_rejected();
    test_dropdown_open_emits_more_draw_data();
    test_dropdown_layout_reserves_for_effective_open();
    test_dropdown_honors_rect_height();
    test_drag_float_n_layout_rejects_bad_count();
    test_drag_int_n_layout_rejects_bad_count();

    /* Radio */
    test_radio_emits_draw_data();
    test_radio_null_selected();
    test_radio_click_changes_selection();
    test_radio_nan_rect_rejected();
    test_radio_independent_groups();

    /* Color picker */
    test_color_picker_emits_draw_data();
    test_color_picker_null_hsv();
    test_color_picker_nan_rect_rejected();
    test_color_picker_sv_click_updates_sv();
    test_color_picker_hue_360_clamped();
    test_color_picker_short_rect_no_overflow();
    test_color_picker_zero_width_no_crash();
    test_color_picker_hue_drag_no_360();
    test_color_picker_narrow_no_label_overflow();

    /* Gradient rect */
    test_gradient_rect_emits_4_verts();
    test_gradient_rect_null_ctx();
    test_gradient_rect_clipped();
    test_gradient_rect_fully_clipped();

    SDL_Log("=== Results: %d tests, %d passed, %d failed ===",
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
