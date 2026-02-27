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
        if (isnan(_a) || isnan(_b)) {                             \
            SDL_Log("    FAIL: %s == %f, expected %f (NaN, "      \
                    "line %d)", #a, (double)_a, (double)_b,       \
                    __LINE__);                                    \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        if (fabsf(_a - _b) > (eps)) {                             \
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
    ForgeUiRect r = { 10.0f, 20.0f, 100.0f, 50.0f };
    ASSERT_TRUE(forge_ui__rect_contains(r, 50.0f, 40.0f));
}

static void test_rect_contains_outside(void)
{
    TEST("rect_contains: point outside");
    ForgeUiRect r = { 10.0f, 20.0f, 100.0f, 50.0f };
    ASSERT_TRUE(!forge_ui__rect_contains(r, 5.0f, 40.0f));
    ASSERT_TRUE(!forge_ui__rect_contains(r, 200.0f, 40.0f));
    ASSERT_TRUE(!forge_ui__rect_contains(r, 50.0f, 5.0f));
    ASSERT_TRUE(!forge_ui__rect_contains(r, 50.0f, 80.0f));
}

static void test_rect_contains_left_edge(void)
{
    TEST("rect_contains: point on left edge (inclusive)");
    ForgeUiRect r = { 10.0f, 20.0f, 100.0f, 50.0f };
    ASSERT_TRUE(forge_ui__rect_contains(r, 10.0f, 40.0f));
}

static void test_rect_contains_right_edge(void)
{
    TEST("rect_contains: point on right edge (exclusive)");
    ForgeUiRect r = { 10.0f, 20.0f, 100.0f, 50.0f };
    ASSERT_TRUE(!forge_ui__rect_contains(r, 110.0f, 40.0f));
}

static void test_rect_contains_top_edge(void)
{
    TEST("rect_contains: point on top edge (inclusive)");
    ForgeUiRect r = { 10.0f, 20.0f, 100.0f, 50.0f };
    ASSERT_TRUE(forge_ui__rect_contains(r, 50.0f, 20.0f));
}

static void test_rect_contains_bottom_edge(void)
{
    TEST("rect_contains: point on bottom edge (exclusive)");
    ForgeUiRect r = { 10.0f, 20.0f, 100.0f, 50.0f };
    ASSERT_TRUE(!forge_ui__rect_contains(r, 50.0f, 70.0f));
}

static void test_rect_contains_zero_size(void)
{
    TEST("rect_contains: zero-size rect never contains");
    ForgeUiRect r = { 10.0f, 20.0f, 0.0f, 0.0f };
    ASSERT_TRUE(!forge_ui__rect_contains(r, 10.0f, 20.0f));
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
    ASSERT_TRUE(true);
}

static void test_free_double_free(void)
{
    TEST("ctx_free: double free does not crash");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_free(&ctx);
    forge_ui_ctx_free(&ctx);
    ASSERT_TRUE(true);
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
    forge_ui_ctx_label(&ctx, "Hello", 10.0f, 10.0f, 1, 1, 1, 1);
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
    ASSERT_TRUE(true);
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
    ASSERT_TRUE(true);
}

/* ── forge_ui_ctx_label tests ───────────────────────────────────────────── */

static void test_label_emits_vertices(void)
{
    TEST("ctx_label: emits vertices for text");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_label(&ctx, "AB", 10.0f, 30.0f, 1, 1, 1, 1);

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
    forge_ui_ctx_label(&ctx, "", 10.0f, 30.0f, 1, 1, 1, 1);

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
    forge_ui_ctx_label(&ctx, NULL, 10.0f, 30.0f, 1, 1, 1, 1);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_label_null_ctx(void)
{
    TEST("ctx_label: NULL ctx does not crash");
    forge_ui_ctx_label(NULL, "Hello", 10.0f, 30.0f, 1, 1, 1, 1);
    ASSERT_TRUE(true);
}

/* ── forge_ui_ctx_button tests ──────────────────────────────────────────── */

static void test_button_emits_draw_data(void)
{
    TEST("ctx_button: emits background rect + text vertices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    forge_ui_ctx_button(&ctx, 1, "OK", rect);

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

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };

    /* Mouse away from button */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    bool clicked = forge_ui_ctx_button(&ctx, 1, "Test", rect);
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

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    float cx = 50.0f, cy = 30.0f;  /* center of button */
    bool clicked;

    /* Frame 0: mouse away, no interaction */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    clicked = forge_ui_ctx_button(&ctx, 1, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_U32(ctx.hot, FORGE_UI_ID_NONE);

    /* Frame 1: mouse over button (becomes hot) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    clicked = forge_ui_ctx_button(&ctx, 1, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_U32(ctx.hot, 1);

    /* Frame 2: mouse pressed (becomes active, edge-triggered) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    clicked = forge_ui_ctx_button(&ctx, 1, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!clicked);
    ASSERT_EQ_U32(ctx.active, 1);

    /* Frame 3: mouse released (click detected) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    clicked = forge_ui_ctx_button(&ctx, 1, "Btn", rect);
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

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    float cx = 50.0f, cy = 30.0f;
    bool clicked;

    /* Frame 0: hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, 1, "Btn", rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_button(&ctx, 1, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, 1);

    /* Frame 2: release OUTSIDE the button */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    clicked = forge_ui_ctx_button(&ctx, 1, "Btn", rect);
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

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };

    /* Frame: mouse over button */
    forge_ui_ctx_begin(&ctx, 50.0f, 30.0f, false);
    forge_ui_ctx_button(&ctx, 42, "Btn", rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_EQ_U32(ctx.hot, 42);
    forge_ui_ctx_free(&ctx);
}

static void test_button_id_zero_rejected(void)
{
    TEST("ctx_button: ID 0 (FORGE_UI_ID_NONE) returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    forge_ui_ctx_begin(&ctx, 50.0f, 30.0f, false);
    bool clicked = forge_ui_ctx_button(&ctx, FORGE_UI_ID_NONE, "Btn", rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(!clicked);
    /* No draw data should be emitted */
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_button_null_ctx(void)
{
    TEST("ctx_button: NULL ctx returns false");
    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    bool clicked = forge_ui_ctx_button(NULL, 1, "Btn", rect);
    ASSERT_TRUE(!clicked);
}

static void test_button_null_text(void)
{
    TEST("ctx_button: NULL text returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    forge_ui_ctx_begin(&ctx, 50.0f, 30.0f, false);
    bool clicked = forge_ui_ctx_button(&ctx, 1, NULL, rect);
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

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    float cx = 50.0f, cy = 30.0f;

    /* Frame 0: mouse held down AWAY from button */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, true);
    forge_ui_ctx_button(&ctx, 1, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    /* Frame 1: mouse still held, dragged ONTO button
     * With edge detection, this should NOT activate because mouse_down
     * was already true in the previous frame (no press edge). */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_button(&ctx, 1, "Btn", rect);
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

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    float cx = 50.0f, cy = 30.0f;

    /* Frame 0: mouse over, not pressed */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, 1, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, 1);

    /* Frame 1: mouse pressed (up->down edge) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_button(&ctx, 1, "Btn", rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, 1);

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
    ForgeUiRect rect1 = { 10.0f, 10.0f, 100.0f, 40.0f };
    ForgeUiRect rect2 = { 50.0f, 10.0f, 100.0f, 40.0f };
    float cx = 80.0f, cy = 30.0f;  /* in overlap region */

    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, 1, "A", rect1);
    forge_ui_ctx_button(&ctx, 2, "B", rect2);
    forge_ui_ctx_end(&ctx);

    /* Button 2 was drawn last, so it should be hot */
    ASSERT_EQ_U32(ctx.hot, 2);

    forge_ui_ctx_free(&ctx);
}

static void test_multiple_buttons_independent(void)
{
    TEST("multiple buttons: non-overlapping buttons have independent hot");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect1 = { 10.0f, 10.0f, 100.0f, 40.0f };
    ForgeUiRect rect2 = { 10.0f, 60.0f, 100.0f, 40.0f };

    /* Mouse over button 1 */
    forge_ui_ctx_begin(&ctx, 50.0f, 30.0f, false);
    forge_ui_ctx_button(&ctx, 1, "A", rect1);
    forge_ui_ctx_button(&ctx, 2, "B", rect2);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, 1);

    /* Mouse over button 2 */
    forge_ui_ctx_begin(&ctx, 50.0f, 80.0f, false);
    forge_ui_ctx_button(&ctx, 1, "A", rect1);
    forge_ui_ctx_button(&ctx, 2, "B", rect2);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, 2);

    forge_ui_ctx_free(&ctx);
}

static void test_overlap_press_last_drawn_wins(void)
{
    TEST("multiple buttons: overlapping press activates last-drawn (topmost)");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    /* Two overlapping buttons -- B is drawn after A so B is on top */
    ForgeUiRect rect_a = { 10.0f, 10.0f, 100.0f, 40.0f };
    ForgeUiRect rect_b = { 50.0f, 10.0f, 100.0f, 40.0f };
    float cx = 80.0f, cy = 30.0f;  /* in overlap region */

    /* Frame 0: hover -- establish hot (last-drawn wins) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, 1, "A", rect_a);
    forge_ui_ctx_button(&ctx, 2, "B", rect_b);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, 2);

    /* Frame 1: press -- last-drawn button must become active */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    bool clicked_a = forge_ui_ctx_button(&ctx, 1, "A", rect_a);
    bool clicked_b = forge_ui_ctx_button(&ctx, 2, "B", rect_b);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, 2);
    ASSERT_TRUE(!clicked_a);
    ASSERT_TRUE(!clicked_b);

    /* Frame 2: release -- only button B registers a click */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    clicked_a = forge_ui_ctx_button(&ctx, 1, "A", rect_a);
    clicked_b = forge_ui_ctx_button(&ctx, 2, "B", rect_b);
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

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    forge_ui_ctx_button(&ctx, 1, "A", rect);
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

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    /* Mouse far away -> normal state */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    forge_ui_ctx_button(&ctx, 1, "A", rect);
    forge_ui_ctx_end(&ctx);

    /* First vertex should have normal button color */
    ASSERT_TRUE(ctx.vertices[0].r == FORGE_UI_BTN_NORMAL_R);
    ASSERT_TRUE(ctx.vertices[0].g == FORGE_UI_BTN_NORMAL_G);
    ASSERT_TRUE(ctx.vertices[0].b == FORGE_UI_BTN_NORMAL_B);

    forge_ui_ctx_free(&ctx);
}

static void test_button_hot_color(void)
{
    TEST("ctx_button: hot state uses hot color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    float cx = 50.0f, cy = 30.0f;

    /* Frame 0: make hot */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, 1, "A", rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: now hot=1, should use hot color */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, 1, "A", rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[0].r == FORGE_UI_BTN_HOT_R);
    ASSERT_TRUE(ctx.vertices[0].g == FORGE_UI_BTN_HOT_G);
    ASSERT_TRUE(ctx.vertices[0].b == FORGE_UI_BTN_HOT_B);

    forge_ui_ctx_free(&ctx);
}

static void test_button_active_color(void)
{
    TEST("ctx_button: active state uses active color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    float cx = 50.0f, cy = 30.0f;

    /* Frame 0: hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_button(&ctx, 1, "A", rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: press (active) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_button(&ctx, 1, "A", rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[0].r == FORGE_UI_BTN_ACTIVE_R);
    ASSERT_TRUE(ctx.vertices[0].g == FORGE_UI_BTN_ACTIVE_G);
    ASSERT_TRUE(ctx.vertices[0].b == FORGE_UI_BTN_ACTIVE_B);

    forge_ui_ctx_free(&ctx);
}

static void test_rect_ccw_winding(void)
{
    TEST("emit_rect: generates CCW winding indices");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
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
    ForgeUiRect rect = { 20.0f, 30.0f, 80.0f, 50.0f };
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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_checkbox(&ctx, 1, "AB", &val, rect);

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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_checkbox(&ctx, 1, "AB", &val, rect);

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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    float cx = 50.0f, cy = 25.0f;
    bool toggled;

    /* Frame 0: mouse away */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    toggled = forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_TRUE(!val);

    /* Frame 1: hover (becomes hot) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    toggled = forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_EQ_U32(ctx.hot, 1);

    /* Frame 2: press (becomes active) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    toggled = forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_EQ_U32(ctx.active, 1);
    ASSERT_TRUE(!val);

    /* Frame 3: release (toggles val to true) */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    toggled = forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(toggled);
    ASSERT_TRUE(val);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    /* Frame 4-5-6: click again to toggle back to false */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    forge_ui_ctx_begin(&ctx, cx, cy, false);
    toggled = forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    float cx = 50.0f, cy = 25.0f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* Press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, 1);

    /* Release outside */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    bool toggled = forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_TRUE(!val);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_null_ctx(void)
{
    TEST("ctx_checkbox: NULL ctx returns false");
    bool val = false;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    bool toggled = forge_ui_ctx_checkbox(NULL, 1, "Opt", &val, rect);
    ASSERT_TRUE(!toggled);
}

static void test_checkbox_null_label(void)
{
    TEST("ctx_checkbox: NULL label returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    forge_ui_ctx_begin(&ctx, 50.0f, 25.0f, false);
    bool toggled = forge_ui_ctx_checkbox(&ctx, 1, NULL, &val, rect);
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

    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    forge_ui_ctx_begin(&ctx, 50.0f, 25.0f, false);
    bool toggled = forge_ui_ctx_checkbox(&ctx, 1, "Opt", NULL, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!toggled);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_id_zero_rejected(void)
{
    TEST("ctx_checkbox: ID 0 (FORGE_UI_ID_NONE) returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    forge_ui_ctx_begin(&ctx, 50.0f, 25.0f, false);
    bool toggled = forge_ui_ctx_checkbox(&ctx, FORGE_UI_ID_NONE, "Opt", &val, rect);
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
    forge_ui_ctx_begin(&ctx, 50.0f, 25.0f, false);

    const ForgeUiFontAtlas *saved = ctx.atlas;
    ctx.atlas = NULL;

    bool val = false;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    bool toggled = forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };

    /* Mouse far away -> normal state */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* First 4 vertices are the outer box */
    ASSERT_TRUE(ctx.vertices[0].r == FORGE_UI_CB_NORMAL_R);
    ASSERT_TRUE(ctx.vertices[0].g == FORGE_UI_CB_NORMAL_G);
    ASSERT_TRUE(ctx.vertices[0].b == FORGE_UI_CB_NORMAL_B);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_hot_color(void)
{
    TEST("ctx_checkbox: hot state uses hot box color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    float cx = 50.0f, cy = 25.0f;

    /* Frame 0: become hot */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: now hot=1, check color */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[0].r == FORGE_UI_CB_HOT_R);
    ASSERT_TRUE(ctx.vertices[0].g == FORGE_UI_CB_HOT_G);
    ASSERT_TRUE(ctx.vertices[0].b == FORGE_UI_CB_HOT_B);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_active_color(void)
{
    TEST("ctx_checkbox: active state uses active box color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    float cx = 50.0f, cy = 25.0f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* Press (active) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[0].r == FORGE_UI_CB_ACTIVE_R);
    ASSERT_TRUE(ctx.vertices[0].g == FORGE_UI_CB_ACTIVE_G);
    ASSERT_TRUE(ctx.vertices[0].b == FORGE_UI_CB_ACTIVE_B);

    forge_ui_ctx_free(&ctx);
}

static void test_checkbox_edge_trigger(void)
{
    TEST("ctx_checkbox: held mouse dragged onto checkbox does NOT activate");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    bool val = false;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    float cx = 50.0f, cy = 25.0f;

    /* Frame 0: mouse held down away */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, true);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: mouse still held, dragged onto checkbox */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_checkbox(&ctx, 1, "Opt", &val, rect);
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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };

    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);

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
    ForgeUiRect rect = { 100.0f, 10.0f, 200.0f, 30.0f };
    /* Effective track: x = 100 + 6 = 106, w = 200 - 12 = 188 */
    /* Click at midpoint: mouse_x = 106 + 94 = 200 -> t = 94/188 = 0.5 */
    float mid_x = 100.0f + FORGE_UI_SL_THUMB_WIDTH * 0.5f + (200.0f - FORGE_UI_SL_THUMB_WIDTH) * 0.5f;
    float cy = 25.0f;
    bool changed;

    /* Frame 0: hover */
    forge_ui_ctx_begin(&ctx, mid_x, cy, false);
    changed = forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!changed);
    ASSERT_EQ_U32(ctx.hot, 1);

    /* Frame 1: press (active + snap) */
    forge_ui_ctx_begin(&ctx, mid_x, cy, true);
    changed = forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
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
    ForgeUiRect rect = { 100.0f, 10.0f, 200.0f, 30.0f };
    float cx = 200.0f, cy = 25.0f;
    bool changed;

    /* Frame 0: hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, 1);

    /* Frame 2: drag FAR to the right (outside widget) — still active, value clamped */
    forge_ui_ctx_begin(&ctx, 500.0f, 200.0f, true);
    changed = forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(changed);
    ASSERT_NEAR(val, 1.0f, 0.001f);
    ASSERT_EQ_U32(ctx.active, 1);

    /* Frame 3: drag FAR to the left — value clamped to min */
    forge_ui_ctx_begin(&ctx, -100.0f, 200.0f, true);
    changed = forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
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
    ForgeUiRect rect = { 100.0f, 10.0f, 200.0f, 30.0f };
    float cx = 200.0f, cy = 25.0f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, 1);

    /* Release */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
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
    ForgeUiRect rect = { 100.0f, 10.0f, 200.0f, 30.0f };
    /* track_x = 100 + 6 = 106, track_w = 200 - 12 = 188 */
    float track_x = 100.0f + FORGE_UI_SL_THUMB_WIDTH * 0.5f;
    float track_w = 200.0f - FORGE_UI_SL_THUMB_WIDTH;
    float cy = 25.0f;

    /* Click at 75% of the track with range [10, 50] */
    float click_x = track_x + track_w * 0.75f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, click_x, cy, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 10.0f, 50.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Press */
    forge_ui_ctx_begin(&ctx, click_x, cy, true);
    forge_ui_ctx_slider(&ctx, 1, &val, 10.0f, 50.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Expected: 10 + 0.75 * (50 - 10) = 10 + 30 = 40 */
    ASSERT_NEAR(val, 40.0f, 0.5f);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_null_ctx(void)
{
    TEST("ctx_slider: NULL ctx returns false");
    float val = 0.5f;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    bool changed = forge_ui_ctx_slider(NULL, 1, &val, 0.0f, 1.0f, rect);
    ASSERT_TRUE(!changed);
}

static void test_slider_null_value(void)
{
    TEST("ctx_slider: NULL value returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    forge_ui_ctx_begin(&ctx, 50.0f, 25.0f, false);
    bool changed = forge_ui_ctx_slider(&ctx, 1, NULL, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_id_zero_rejected(void)
{
    TEST("ctx_slider: ID 0 (FORGE_UI_ID_NONE) returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    forge_ui_ctx_begin(&ctx, 50.0f, 25.0f, false);
    bool changed = forge_ui_ctx_slider(&ctx, FORGE_UI_ID_NONE, &val, 0.0f, 1.0f, rect);
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
    forge_ui_ctx_begin(&ctx, 50.0f, 25.0f, false);

    const ForgeUiFontAtlas *saved = ctx.atlas;
    ctx.atlas = NULL;

    float val = 0.5f;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    bool changed = forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };

    forge_ui_ctx_begin(&ctx, 50.0f, 25.0f, false);

    /* Equal range */
    bool changed = forge_ui_ctx_slider(&ctx, 1, &val, 5.0f, 5.0f, rect);
    ASSERT_TRUE(!changed);
    ASSERT_EQ_INT(ctx.vertex_count, 0);

    /* Inverted range */
    changed = forge_ui_ctx_slider(&ctx, 1, &val, 10.0f, 5.0f, rect);
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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    float nan_val = NAN;

    forge_ui_ctx_begin(&ctx, 50.0f, 25.0f, false);

    /* NaN min */
    bool changed = forge_ui_ctx_slider(&ctx, 1, &val, nan_val, 1.0f, rect);
    ASSERT_TRUE(!changed);

    /* NaN max */
    changed = forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, nan_val, rect);
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
    bool changed = forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };

    /* Mouse far away */
    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* First 4 vertices = track, next 4 = thumb */
    ASSERT_TRUE(ctx.vertex_count >= 8);
    ASSERT_TRUE(ctx.vertices[4].r == FORGE_UI_SL_NORMAL_R);
    ASSERT_TRUE(ctx.vertices[4].g == FORGE_UI_SL_NORMAL_G);
    ASSERT_TRUE(ctx.vertices[4].b == FORGE_UI_SL_NORMAL_B);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_hot_color(void)
{
    TEST("ctx_slider: hot state uses hot thumb color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    float cx = 100.0f, cy = 25.0f;

    /* Frame 0: become hot */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: now hot, check thumb color */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[4].r == FORGE_UI_SL_HOT_R);
    ASSERT_TRUE(ctx.vertices[4].g == FORGE_UI_SL_HOT_G);
    ASSERT_TRUE(ctx.vertices[4].b == FORGE_UI_SL_HOT_B);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_active_color(void)
{
    TEST("ctx_slider: active state uses active thumb color");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    float cx = 100.0f, cy = 25.0f;

    /* Hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Press (active) */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    ASSERT_TRUE(ctx.vertices[4].r == FORGE_UI_SL_ACTIVE_R);
    ASSERT_TRUE(ctx.vertices[4].g == FORGE_UI_SL_ACTIVE_G);
    ASSERT_TRUE(ctx.vertices[4].b == FORGE_UI_SL_ACTIVE_B);

    forge_ui_ctx_free(&ctx);
}

static void test_slider_track_uses_white_uv(void)
{
    TEST("ctx_slider: track rect uses atlas white_uv");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));

    float val = 0.5f;
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };

    forge_ui_ctx_begin(&ctx, 300.0f, 300.0f, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
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
    ForgeUiRect rect = { 10.0f, 10.0f, 200.0f, 30.0f };
    float cx = 100.0f, cy = 25.0f;

    /* Frame 0: mouse held away */
    forge_ui_ctx_begin(&ctx, 400.0f, 400.0f, true);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Frame 1: still held, dragged onto slider */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
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
    ForgeUiRect rect = { 100.0f, 10.0f, 200.0f, 30.0f };
    float track_x = 100.0f + FORGE_UI_SL_THUMB_WIDTH * 0.5f;
    float cy = 25.0f;

    /* Hover at far left */
    forge_ui_ctx_begin(&ctx, track_x, cy, false);
    forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
    forge_ui_ctx_end(&ctx);

    /* Press at far left, val should become 0.0 = same as current */
    forge_ui_ctx_begin(&ctx, track_x, cy, true);
    bool changed = forge_ui_ctx_slider(&ctx, 1, &val, 0.0f, 1.0f, rect);
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
    forge_ui_ctx_begin(&ctx, 50.0f, 30.0f, false);

    const ForgeUiFontAtlas *saved = ctx.atlas;
    ctx.atlas = NULL;

    ForgeUiRect rect = { 10.0f, 10.0f, 100.0f, 40.0f };
    bool clicked = forge_ui_ctx_button(&ctx, 1, "Btn", rect);
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
    ASSERT_TRUE(true);
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
    ASSERT_TRUE(true);
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
    ASSERT_TRUE(!forge_ui_ctx_text_input(NULL, 1, &st, r, true));
}

static void test_text_input_null_state(void)
{
    TEST("ctx_text_input: NULL state returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, 1, NULL, r, true));

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
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, 1, &st, r, true));

    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_free(&ctx);
}

static void test_text_input_id_zero(void)
{
    TEST("ctx_text_input: ID 0 returns false");
    if (!setup_atlas()) return;

    ForgeUiContext ctx;
    ASSERT_TRUE(forge_ui_ctx_init(&ctx, &test_atlas));
    forge_ui_ctx_begin(&ctx, 0, 0, false);

    char buf[32] = "";
    ForgeUiTextInputState st = { buf, 32, 0, 0 };
    ForgeUiRect r = { 0, 0, 100, 30 };
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, FORGE_UI_ID_NONE, &st, r, true));

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
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, 1, &st, r, true));

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
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, 1, &st, r, true));

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
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, 1, &st, r, true));

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
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, 1, &st, r, true));

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
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, 1, &st, r, true));

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
    ASSERT_TRUE(!forge_ui_ctx_text_input(&ctx, 1, &st, r, true));

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
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);

    /* Frame 1: hover */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, 1);

    /* Frame 2: press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, 1);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);  /* not yet */

    /* Frame 3: release -- focus acquired */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, 1);
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
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);

    /* Press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, 1);

    /* Release OUTSIDE */
    forge_ui_ctx_begin(&ctx, 300, 300, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, 1);

    /* Press OUTSIDE -- should unfocus */
    forge_ui_ctx_begin(&ctx, 300, 300, true);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, 1);

    /* Escape */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, false,
                              false, false, true);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, 1);

    /* Press on the widget + Escape in same frame */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, false,
                              false, false, true);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);
    ASSERT_EQ_U32(ctx.active, FORGE_UI_ID_NONE);

    /* Release on widget -- must NOT re-focus because active was cleared */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, FORGE_UI_ID_NONE);

    forge_ui_ctx_free(&ctx);
}

/* ── forge_ui_ctx_text_input -- character insertion tests ───────────────── */

/* Helper: focus a text input widget on a context (3-frame sequence) */
static void focus_text_input(ForgeUiContext *ctx, Uint32 id,
                             ForgeUiTextInputState *st, ForgeUiRect r)
{
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;

    forge_ui_ctx_begin(ctx, cx, cy, false);
    forge_ui_ctx_text_input(ctx, id, st, r, true);
    forge_ui_ctx_end(ctx);

    forge_ui_ctx_begin(ctx, cx, cy, true);
    forge_ui_ctx_text_input(ctx, id, st, r, true);
    forge_ui_ctx_end(ctx);

    forge_ui_ctx_begin(ctx, cx, cy, false);
    forge_ui_ctx_text_input(ctx, id, st, r, true);
    forge_ui_ctx_end(ctx);
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

    focus_text_input(&ctx, 1, &st, r);
    ASSERT_EQ_U32(ctx.focused, 1);

    /* Type "Hi" */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "Hi", false, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "B", false, false, false, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "D", false, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, true, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, true, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, true, false, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, true, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, true, false, false,
                              false, false, false);
    bool changed = forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;

    /* Left arrow */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, true, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(st.cursor, 1);

    /* Right arrow */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, true,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;

    /* Home */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, false,
                              true, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_INT(st.cursor, 0);

    /* End */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, false,
                              false, true, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, true, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, false, false, false, true,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    /* Both backspace and text input in same frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "C", true, false, false, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    /* Both delete and text input in same frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "C", false, true, false, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    /* Backspace + Left in same frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, NULL, true, false, true, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    /* Insert "C" + Left in same frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_set_keyboard(&ctx, "C", false, false, true, false,
                              false, false, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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

    focus_text_input(&ctx, 1, &st, r);

    /* Render a focused frame */
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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
    bool changed = forge_ui_ctx_text_input(&ctx, 1, &st, r, true);
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
    forge_ui_ctx_text_input(&ctx, 1, &st1, r1, true);
    forge_ui_ctx_text_input(&ctx, 2, &st2, r2, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.hot, 2);  /* last drawn */

    /* Press */
    forge_ui_ctx_begin(&ctx, cx, cy, true);
    forge_ui_ctx_text_input(&ctx, 1, &st1, r1, true);
    forge_ui_ctx_text_input(&ctx, 2, &st2, r2, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.active, 2);  /* last drawn wins */

    /* Release */
    forge_ui_ctx_begin(&ctx, cx, cy, false);
    forge_ui_ctx_text_input(&ctx, 1, &st1, r1, true);
    forge_ui_ctx_text_input(&ctx, 2, &st2, r2, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_EQ_U32(ctx.focused, 2);  /* last drawn gets focus */

    forge_ui_ctx_free(&ctx);
}

/* ── Null-termination validation test (audit fix) ───────────────────────── */

static void test_text_input_bad_null_termination(void)
{
    TEST("text input rejects buffer with missing null terminator");
    if (!atlas_built) return;

    ForgeUiContext ctx;
    forge_ui_ctx_init(&ctx, &test_atlas);

    char buf[16];
    SDL_memset(buf, 'A', sizeof(buf));  /* fill with non-zero, no '\0' at length */
    ForgeUiTextInputState state;
    state.buffer   = buf;
    state.capacity = (int)sizeof(buf);
    state.length   = 3;    /* claims 3 bytes, but buf[3] != '\0' */
    state.cursor   = 0;

    ForgeUiRect r = { 10, 10, 200, 30 };
    forge_ui_ctx_begin(&ctx, 0.0f, 0.0f, false);
    bool result = forge_ui_ctx_text_input(&ctx, 1, &state, r, true);
    forge_ui_ctx_end(&ctx);
    ASSERT_TRUE(!result);  /* should reject: buf[length] != '\0' */

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
    test_button_id_zero_rejected();
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
    test_checkbox_id_zero_rejected();
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
    test_slider_id_zero_rejected();
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
    test_text_input_id_zero();
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
