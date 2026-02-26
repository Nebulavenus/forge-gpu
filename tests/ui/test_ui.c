/*
 * UI Library Tests
 *
 * Automated tests for common/ui/forge_ui.h — TTF parser, rasterizer,
 * hmtx metrics, font atlas building, text layout, and BMP writing.
 *
 * Verifies correctness of font loading, table directory parsing,
 * metric extraction, cmap lookups, glyph outline parsing, advance
 * width lookups, atlas packing, UV coordinates, glyph lookup,
 * text layout (pen model, line breaking, alignment, vertex/index
 * generation), and text measurement.
 *
 * Uses the bundled Liberation Mono Regular font for all tests.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>   /* remove() for cleaning up test BMP files */
#include "ui/forge_ui.h"

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

#define ASSERT_EQ_U16(a, b)                                       \
    do {                                                          \
        Uint16 _a = (a), _b = (b);                                \
        if (_a != _b) {                                           \
            SDL_Log("    FAIL: %s == %u, expected %u (line %d)",  \
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

#define ASSERT_EQ_I16(a, b)                                       \
    do {                                                          \
        Sint16 _a = (a), _b = (b);                                \
        if (_a != _b) {                                           \
            SDL_Log("    FAIL: %s == %d, expected %d (line %d)",  \
                    #a, _a, _b, __LINE__);                        \
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

/* ── Atlas test parameters ───────────────────────────────────────────────── */
#define ATLAS_PIXEL_HEIGHT  32.0f  /* render glyphs at 32px for atlas tests */
#define ATLAS_PADDING       1      /* 1 pixel padding between glyphs */
#define ASCII_START         32     /* first printable ASCII codepoint */
#define ASCII_END           126    /* last printable ASCII codepoint */
#define ASCII_COUNT         (ASCII_END - ASCII_START + 1) /* 95 glyphs */

/* ── Test font path ──────────────────────────────────────────────────────── */

#define TEST_FONT_PATH "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"

/* Shared font instance loaded once for all tests */
static ForgeUiFont test_font;
static bool font_loaded = false;

/* ── Test: font loading ──────────────────────────────────────────────────── */

static void test_font_load(void)
{
    TEST("forge_ui_ttf_load succeeds with valid font");
    font_loaded = forge_ui_ttf_load(TEST_FONT_PATH, &test_font);
    ASSERT_TRUE(font_loaded);
}

static void test_font_load_nonexistent(void)
{
    TEST("forge_ui_ttf_load fails with nonexistent path");
    ForgeUiFont bad_font;
    bool result = forge_ui_ttf_load("nonexistent.ttf", &bad_font);
    ASSERT_TRUE(!result);
}

/* ── Test: table directory ───────────────────────────────────────────────── */

static void test_table_directory(void)
{
    TEST("table directory has expected count");
    if (!font_loaded) return;

    /* Liberation Mono Regular has 16 tables */
    ASSERT_EQ_U16(test_font.num_tables, 16);
}

static void test_table_lookup(void)
{
    TEST("required tables are present");
    if (!font_loaded) return;

    /* All required tables must be found in the public table directory */
    const char *required[] = {"head", "hhea", "maxp", "cmap", "loca", "glyf"};
    int num_required = (int)(sizeof(required) / sizeof(required[0]));
    for (int i = 0; i < num_required; i++) {
        bool found = false;
        for (Uint16 j = 0; j < test_font.num_tables; j++) {
            if (SDL_strcmp(test_font.tables[j].tag, required[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            SDL_Log("    FAIL: table '%s' not found (line %d)",
                    required[i], __LINE__);
            fail_count++;
            return;
        }
    }
    pass_count++;
}

static void test_table_entry_bounds(void)
{
    TEST("table entries have valid offset+length within file");
    if (!font_loaded) return;

    for (Uint16 i = 0; i < test_font.num_tables; i++) {
        Uint64 end = (Uint64)test_font.tables[i].offset +
                     (Uint64)test_font.tables[i].length;
        if (end > test_font.data_size) {
            SDL_Log("    FAIL: table '%.4s' offset+length (%u+%u) exceeds "
                    "file size (%zu) (line %d)",
                    test_font.tables[i].tag,
                    test_font.tables[i].offset,
                    test_font.tables[i].length,
                    test_font.data_size, __LINE__);
            fail_count++;
            return;
        }
    }
    pass_count++;
}

/* ── Test: head table ────────────────────────────────────────────────────── */

static void test_head_units_per_em(void)
{
    TEST("head: unitsPerEm is 2048");
    if (!font_loaded) return;
    ASSERT_EQ_U16(test_font.head.units_per_em, 2048);
}

static void test_head_index_to_loc_format(void)
{
    TEST("head: indexToLocFormat is 0 (short)");
    if (!font_loaded) return;
    ASSERT_EQ_I16(test_font.head.index_to_loc_fmt, 0);
}

static void test_head_bounding_box(void)
{
    TEST("head: global bounding box values");
    if (!font_loaded) return;
    ASSERT_EQ_I16(test_font.head.x_min, -50);
    ASSERT_EQ_I16(test_font.head.y_min, -615);
    ASSERT_EQ_I16(test_font.head.x_max, 1247);
    ASSERT_EQ_I16(test_font.head.y_max, 1705);
}

/* ── Test: hhea table ────────────────────────────────────────────────────── */

static void test_hhea_metrics(void)
{
    TEST("hhea: ascender, descender, lineGap");
    if (!font_loaded) return;
    ASSERT_EQ_I16(test_font.hhea.ascender, 1705);
    ASSERT_EQ_I16(test_font.hhea.descender, -615);
    ASSERT_EQ_I16(test_font.hhea.line_gap, 0);
}

static void test_hhea_num_hmetrics(void)
{
    TEST("hhea: numberOfHMetrics");
    if (!font_loaded) return;
    ASSERT_EQ_U16(test_font.hhea.number_of_h_metrics, 4);
}

/* ── Test: maxp table ────────────────────────────────────────────────────── */

static void test_maxp_num_glyphs(void)
{
    TEST("maxp: numGlyphs is 670");
    if (!font_loaded) return;
    ASSERT_EQ_U16(test_font.maxp.num_glyphs, 670);
}

/* ── Test: cmap lookups ──────────────────────────────────────────────────── */

static void test_cmap_ascii_a(void)
{
    TEST("cmap: 'A' (U+0041) maps to glyph 36");
    if (!font_loaded) return;
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, 'A'), 36);
}

static void test_cmap_ascii_g(void)
{
    TEST("cmap: 'g' (U+0067) maps to glyph 74");
    if (!font_loaded) return;
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, 'g'), 74);
}

static void test_cmap_space(void)
{
    TEST("cmap: space (U+0020) maps to glyph 3");
    if (!font_loaded) return;
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, ' '), 3);
}

static void test_cmap_unmapped(void)
{
    TEST("cmap: unmapped codepoint returns 0 (.notdef)");
    if (!font_loaded) return;
    /* U+FFFE is guaranteed to be a noncharacter */
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, 0xFFFE), 0);
}

static void test_cmap_beyond_bmp(void)
{
    TEST("cmap: codepoint > 0xFFFF returns 0 (format 4 BMP only)");
    if (!font_loaded) return;
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, 0x10000), 0);
}

/* ── Test: glyph index out of range ──────────────────────────────────────── */

static void test_glyph_out_of_range(void)
{
    TEST("load_glyph: index >= numGlyphs returns false");
    if (!font_loaded) return;
    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(
        &test_font, test_font.maxp.num_glyphs, &glyph);
    ASSERT_TRUE(!result);
}

/* ── Test: space glyph (zero-length) ─────────────────────────────────────── */

static void test_glyph_space(void)
{
    TEST("load_glyph: space glyph has no contours");
    if (!font_loaded) return;
    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, ' ');
    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(&test_font, idx, &glyph);
    ASSERT_TRUE(result);
    ASSERT_EQ_U16(glyph.contour_count, 0);
    ASSERT_EQ_U16(glyph.point_count, 0);
    forge_ui_ttf_glyph_free(&glyph);
}

/* ── Test: glyph 'A' outline ─────────────────────────────────────────────── */
/* Loads glyph 'A' once and verifies contour count, point count, bounding
 * box, contour endpoints, and first point coordinates.  A single load
 * avoids redundant work and ensures glyph_free is always called. */

static void test_glyph_a_outline(void)
{
    TEST("load_glyph: 'A' outline (contours, bbox, endpoints, first point)");
    if (!font_loaded) return;
    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(&test_font, idx, &glyph);
    if (!result) {
        SDL_Log("    FAIL: forge_ui_ttf_load_glyph returned false (line %d)",
                __LINE__);
        fail_count++;
        return;
    }
    pass_count++; /* load succeeded */

    bool ok = true;

    /* Contour and point counts */
    if (glyph.contour_count != 2) {
        SDL_Log("    FAIL: contour_count == %u, expected 2 (line %d)",
                glyph.contour_count, __LINE__);
        fail_count++; ok = false;
    } else { pass_count++; }

    if (glyph.point_count != 21) {
        SDL_Log("    FAIL: point_count == %u, expected 21 (line %d)",
                glyph.point_count, __LINE__);
        fail_count++; ok = false;
    } else { pass_count++; }

    /* Bounding box */
    if (glyph.x_min != 0 || glyph.y_min != 0 ||
        glyph.x_max != 1228 || glyph.y_max != 1349) {
        SDL_Log("    FAIL: bbox (%d,%d)-(%d,%d), expected (0,0)-(1228,1349) "
                "(line %d)", glyph.x_min, glyph.y_min,
                glyph.x_max, glyph.y_max, __LINE__);
        fail_count++; ok = false;
    } else { pass_count++; }

    /* Contour endpoints (only check if contours were correct) */
    if (ok && glyph.contour_count == 2) {
        if (glyph.contour_ends[0] != 7 || glyph.contour_ends[1] != 20) {
            SDL_Log("    FAIL: contour_ends [%u, %u], expected [7, 20] "
                    "(line %d)", glyph.contour_ends[0],
                    glyph.contour_ends[1], __LINE__);
            fail_count++;
        } else { pass_count++; }
    }

    /* First point: (1034, 0), on-curve */
    if (ok && glyph.point_count > 0) {
        if (glyph.points[0].x != 1034 || glyph.points[0].y != 0) {
            SDL_Log("    FAIL: points[0] == (%d,%d), expected (1034,0) "
                    "(line %d)", glyph.points[0].x, glyph.points[0].y,
                    __LINE__);
            fail_count++;
        } else { pass_count++; }

        if (!(glyph.flags[0] & FORGE_UI_FLAG_ON_CURVE)) {
            SDL_Log("    FAIL: points[0] not on-curve (line %d)", __LINE__);
            fail_count++;
        } else { pass_count++; }
    }

    forge_ui_ttf_glyph_free(&glyph);
}

/* ── Test: glyph free is safe on zeroed struct ───────────────────────────── */

static void test_glyph_free_zeroed(void)
{
    TEST("glyph_free: safe on zero-initialized struct");
    ForgeUiTtfGlyph glyph;
    SDL_memset(&glyph, 0, sizeof(glyph));
    forge_ui_ttf_glyph_free(&glyph); /* must not crash */
    pass_count++;
}

/* ── Test: loca offsets are monotonically non-decreasing ─────────────────── */

static void test_loca_monotonic(void)
{
    TEST("loca: offsets are monotonically non-decreasing");
    if (!font_loaded) return;
    Uint32 count = (Uint32)test_font.maxp.num_glyphs + 1;
    for (Uint32 i = 1; i < count; i++) {
        if (test_font.loca_offsets[i] < test_font.loca_offsets[i - 1]) {
            SDL_Log("    FAIL: loca[%u]=%u < loca[%u]=%u (line %d)",
                    i, test_font.loca_offsets[i],
                    i - 1, test_font.loca_offsets[i - 1], __LINE__);
            fail_count++;
            return;
        }
    }
    pass_count++;
}

/* ── Test: index_to_loc_fmt is valid ─────────────────────────────────────── */

static void test_head_index_to_loc_valid(void)
{
    TEST("head: index_to_loc_fmt is 0 or 1");
    if (!font_loaded) return;
    ASSERT_TRUE(test_font.head.index_to_loc_fmt == 0 ||
                test_font.head.index_to_loc_fmt == 1);
}

/* ── Test: load_glyph rejects reversed loca offsets ──────────────────────── */

static void test_glyph_reject_reversed_loca(void)
{
    TEST("load_glyph: rejects reversed loca offsets (next < current)");
    if (!font_loaded) return;

    /* Use glyph 'A' — it has outline data (non-zero length) */
    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');

    /* Save originals and swap so next_offset < glyph_offset */
    Uint32 saved_cur  = test_font.loca_offsets[idx];
    Uint32 saved_next = test_font.loca_offsets[idx + 1];

    test_font.loca_offsets[idx]     = saved_next;
    test_font.loca_offsets[idx + 1] = saved_cur;

    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(&test_font, idx, &glyph);

    /* Restore before asserting — ASSERT_TRUE may early-return */
    test_font.loca_offsets[idx]     = saved_cur;
    test_font.loca_offsets[idx + 1] = saved_next;

    ASSERT_TRUE(!result);
}

/* ── Test: load_glyph rejects glyph that extends past file ───────────────── */

static void test_glyph_reject_out_of_bounds_loca(void)
{
    TEST("load_glyph: rejects glyph extending past file bounds");
    if (!font_loaded) return;

    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');

    /* Save original and set next_offset far past file end */
    Uint32 saved_next = test_font.loca_offsets[idx + 1];
    test_font.loca_offsets[idx + 1] = 0xFFFFFFFF;

    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(&test_font, idx, &glyph);

    /* Restore before asserting — ASSERT_TRUE may early-return */
    test_font.loca_offsets[idx + 1] = saved_next;

    ASSERT_TRUE(!result);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Rasterizer Tests ─────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── Test: rasterize 'A' produces valid bitmap ───────────────────────────── */

static void test_raster_basic(void)
{
    TEST("rasterize_glyph: 'A' at 64px produces valid bitmap");
    if (!font_loaded) return;

    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ForgeUiRasterOpts opts;
    opts.supersample_level = 4;

    ForgeUiGlyphBitmap bmp;
    bool result = forge_ui_rasterize_glyph(&test_font, idx, 64.0f, &opts, &bmp);
    ASSERT_TRUE(result);
    ASSERT_TRUE(bmp.width > 0);
    ASSERT_TRUE(bmp.height > 0);
    ASSERT_TRUE(bmp.pixels != NULL);

    /* At least some pixels should be filled (non-zero) */
    int filled = 0;
    for (int i = 0; i < bmp.width * bmp.height; i++) {
        if (bmp.pixels[i] > 0) filled++;
    }
    ASSERT_TRUE(filled > 0);

    forge_ui_glyph_bitmap_free(&bmp);
}

/* ── Test: 'O' produces a hole (donut shape) ─────────────────────────────── */
/* The center of 'O' should have pixels with coverage 0 because the inner
 * contour winds counter-clockwise, cancelling the outer contour's winding. */

static void test_raster_donut(void)
{
    TEST("rasterize_glyph: 'O' has empty center (hole from winding rule)");
    if (!font_loaded) return;

    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'O');
    ForgeUiRasterOpts opts;
    opts.supersample_level = 1; /* binary — easier to verify hole */

    ForgeUiGlyphBitmap bmp;
    bool result = forge_ui_rasterize_glyph(&test_font, idx, 64.0f, &opts, &bmp);
    ASSERT_TRUE(result);
    ASSERT_TRUE(bmp.width > 0 && bmp.height > 0);

    /* Sample the center pixel — should be empty (inside the hole) */
    int cx = bmp.width / 2;
    int cy = bmp.height / 2;
    Uint8 center = bmp.pixels[cy * bmp.width + cx];
    ASSERT_TRUE(center == 0);

    forge_ui_glyph_bitmap_free(&bmp);
}

/* ── Test: space glyph returns zero-size bitmap ──────────────────────────── */

static void test_raster_whitespace(void)
{
    TEST("rasterize_glyph: space returns success with zero-size bitmap");
    if (!font_loaded) return;

    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, ' ');
    ForgeUiGlyphBitmap bmp;
    bool result = forge_ui_rasterize_glyph(&test_font, idx, 64.0f, NULL, &bmp);
    ASSERT_TRUE(result);
    ASSERT_TRUE(bmp.width == 0);
    ASSERT_TRUE(bmp.height == 0);

    forge_ui_glyph_bitmap_free(&bmp);
}

/* ── Test: supersampling produces intermediate coverage values ────────────── */

static void test_raster_antialiasing(void)
{
    TEST("rasterize_glyph: ss=4 produces intermediate coverage values");
    if (!font_loaded) return;

    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ForgeUiRasterOpts opts;
    opts.supersample_level = 4;

    ForgeUiGlyphBitmap bmp;
    bool result = forge_ui_rasterize_glyph(&test_font, idx, 64.0f, &opts, &bmp);
    ASSERT_TRUE(result);

    /* With 4x4 supersampling, edge pixels should have values between 1-254.
     * Count how many distinct values exist. */
    bool has_intermediate = false;
    for (int i = 0; i < bmp.width * bmp.height; i++) {
        if (bmp.pixels[i] > 0 && bmp.pixels[i] < 255) {
            has_intermediate = true;
            break;
        }
    }
    ASSERT_TRUE(has_intermediate);

    forge_ui_glyph_bitmap_free(&bmp);
}

/* ── Test: binary rasterization has no intermediate values ───────────────── */

static void test_raster_no_aa(void)
{
    TEST("rasterize_glyph: ss=1 produces only 0 and 255");
    if (!font_loaded) return;

    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ForgeUiRasterOpts opts;
    opts.supersample_level = 1;

    ForgeUiGlyphBitmap bmp;
    bool result = forge_ui_rasterize_glyph(&test_font, idx, 64.0f, &opts, &bmp);
    ASSERT_TRUE(result);

    bool all_binary = true;
    for (int i = 0; i < bmp.width * bmp.height; i++) {
        if (bmp.pixels[i] != 0 && bmp.pixels[i] != 255) {
            all_binary = false;
            break;
        }
    }
    ASSERT_TRUE(all_binary);

    forge_ui_glyph_bitmap_free(&bmp);
}

/* ── Test: bitmap_free is safe on zeroed struct ──────────────────────────── */

static void test_raster_bitmap_free_zeroed(void)
{
    TEST("glyph_bitmap_free: safe on zero-initialized struct");
    ForgeUiGlyphBitmap bmp;
    SDL_memset(&bmp, 0, sizeof(bmp));
    forge_ui_glyph_bitmap_free(&bmp); /* must not crash */
    pass_count++;
}

/* ── Test: default opts (NULL) uses 4x4 supersampling ────────────────────── */

static void test_raster_default_opts(void)
{
    TEST("rasterize_glyph: NULL opts uses default (produces AA)");
    if (!font_loaded) return;

    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ForgeUiGlyphBitmap bmp;
    bool result = forge_ui_rasterize_glyph(&test_font, idx, 64.0f, NULL, &bmp);
    ASSERT_TRUE(result);
    ASSERT_TRUE(bmp.width > 0);

    /* Default should be 4x4 SS — expect intermediate values */
    bool has_intermediate = false;
    for (int i = 0; i < bmp.width * bmp.height; i++) {
        if (bmp.pixels[i] > 0 && bmp.pixels[i] < 255) {
            has_intermediate = true;
            break;
        }
    }
    ASSERT_TRUE(has_intermediate);

    forge_ui_glyph_bitmap_free(&bmp);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── hmtx / Advance Width Tests ────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── Test: hmtx arrays are populated after load ──────────────────────────── */

static void test_hmtx_loaded(void)
{
    TEST("hmtx: arrays are populated after font load");
    if (!font_loaded) return;
    ASSERT_TRUE(test_font.hmtx_advance_widths != NULL);
    ASSERT_TRUE(test_font.hmtx_left_side_bearings != NULL);
}

/* ── Test: advance width for 'A' ─────────────────────────────────────────── */
/* Liberation Mono is monospaced — all printable glyphs share the same
 * advance width (1229 font units at 2048 unitsPerEm). */

static void test_hmtx_advance_width_a(void)
{
    TEST("hmtx: advance_width for 'A' is 1229");
    if (!font_loaded) return;
    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ASSERT_EQ_U16(forge_ui_ttf_advance_width(&test_font, idx), 1229);
}

/* ── Test: advance width for glyph beyond numberOfHMetrics ───────────────── */
/* Glyphs at or beyond numberOfHMetrics share the last advance width.
 * Liberation Mono has 4 hmetrics entries but 670 glyphs. */

static void test_hmtx_advance_width_trailing(void)
{
    TEST("hmtx: glyphs beyond numberOfHMetrics use last advance");
    if (!font_loaded) return;

    /* Pick a glyph index well beyond numberOfHMetrics (4) */
    Uint16 trailing_idx = 100;
    ASSERT_TRUE(trailing_idx >= test_font.hhea.number_of_h_metrics);
    ASSERT_EQ_U16(forge_ui_ttf_advance_width(&test_font, trailing_idx),
                  test_font.hmtx_last_advance);
}

/* ── Test: last_advance matches final hmtx entry ─────────────────────────── */

static void test_hmtx_last_advance(void)
{
    TEST("hmtx: hmtx_last_advance matches last entry in advance_widths");
    if (!font_loaded) return;
    Uint16 n = test_font.hhea.number_of_h_metrics;
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_U16(test_font.hmtx_last_advance,
                  test_font.hmtx_advance_widths[n - 1]);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Font Atlas Tests ──────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── Test: atlas build succeeds with printable ASCII ─────────────────────── */

static void test_atlas_build(void)
{
    TEST("atlas_build: succeeds with printable ASCII at 32px");
    if (!font_loaded) return;

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       codepoints, ASCII_COUNT,
                                       ATLAS_PADDING, &atlas);
    ASSERT_TRUE(result);
    ASSERT_TRUE(atlas.pixels != NULL);
    ASSERT_TRUE(atlas.glyphs != NULL);
    ASSERT_EQ_INT(atlas.glyph_count, ASCII_COUNT);

    forge_ui_atlas_free(&atlas);
}

/* ── Test: atlas dimensions are powers of two ────────────────────────────── */

static bool is_power_of_two(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

static void test_atlas_power_of_two(void)
{
    TEST("atlas_build: dimensions are powers of two");
    if (!font_loaded) return;

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       codepoints, ASCII_COUNT,
                                       ATLAS_PADDING, &atlas);
    ASSERT_TRUE(result);
    ASSERT_TRUE(is_power_of_two(atlas.width));
    ASSERT_TRUE(is_power_of_two(atlas.height));

    forge_ui_atlas_free(&atlas);
}

/* ── Test: atlas lookup finds 'A' ────────────────────────────────────────── */

static void test_atlas_lookup_found(void)
{
    TEST("atlas_lookup: finds 'A' with valid metadata");
    if (!font_loaded) return;

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       codepoints, ASCII_COUNT,
                                       ATLAS_PADDING, &atlas);
    ASSERT_TRUE(result);

    const ForgeUiPackedGlyph *g = forge_ui_atlas_lookup(&atlas, 'A');
    ASSERT_TRUE(g != NULL);
    ASSERT_EQ_U32(g->codepoint, 'A');
    ASSERT_TRUE(g->bitmap_w > 0);
    ASSERT_TRUE(g->bitmap_h > 0);
    ASSERT_TRUE(g->advance_width > 0);

    forge_ui_atlas_free(&atlas);
}

/* ── Test: atlas lookup returns NULL for missing codepoint ───────────────── */

static void test_atlas_lookup_missing(void)
{
    TEST("atlas_lookup: returns NULL for codepoint not in atlas");
    if (!font_loaded) return;

    /* Build a minimal atlas with just one codepoint */
    Uint32 codepoints[] = { 'A' };
    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       codepoints, 1, ATLAS_PADDING, &atlas);
    ASSERT_TRUE(result);

    const ForgeUiPackedGlyph *g = forge_ui_atlas_lookup(&atlas, 'Z');
    ASSERT_TRUE(g == NULL);

    forge_ui_atlas_free(&atlas);
}

/* ── Test: UV coordinates are in [0, 1] range ────────────────────────────── */

static void test_atlas_uv_range(void)
{
    TEST("atlas_build: all UV coordinates are in [0.0, 1.0]");
    if (!font_loaded) return;

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       codepoints, ASCII_COUNT,
                                       ATLAS_PADDING, &atlas);
    ASSERT_TRUE(result);

    for (int i = 0; i < atlas.glyph_count; i++) {
        const ForgeUiPackedGlyph *g = &atlas.glyphs[i];
        if (g->uv.u0 < 0.0f || g->uv.u0 > 1.0f ||
            g->uv.v0 < 0.0f || g->uv.v0 > 1.0f ||
            g->uv.u1 < 0.0f || g->uv.u1 > 1.0f ||
            g->uv.v1 < 0.0f || g->uv.v1 > 1.0f) {
            SDL_Log("    FAIL: glyph %d (U+%04X) UV out of [0,1]: "
                    "(%.4f,%.4f)-(%.4f,%.4f) (line %d)",
                    i, g->codepoint,
                    (double)g->uv.u0, (double)g->uv.v0,
                    (double)g->uv.u1, (double)g->uv.v1, __LINE__);
            fail_count++;
            forge_ui_atlas_free(&atlas);
            return;
        }
    }
    pass_count++;

    forge_ui_atlas_free(&atlas);
}

/* ── Test: UV ordering (u0 <= u1, v0 <= v1) ──────────────────────────────── */

static void test_atlas_uv_ordering(void)
{
    TEST("atlas_build: UVs satisfy u0 <= u1 and v0 <= v1");
    if (!font_loaded) return;

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       codepoints, ASCII_COUNT,
                                       ATLAS_PADDING, &atlas);
    ASSERT_TRUE(result);

    for (int i = 0; i < atlas.glyph_count; i++) {
        const ForgeUiPackedGlyph *g = &atlas.glyphs[i];
        if (g->uv.u0 > g->uv.u1 || g->uv.v0 > g->uv.v1) {
            SDL_Log("    FAIL: glyph %d (U+%04X) UV ordering violated: "
                    "(%.4f,%.4f)-(%.4f,%.4f) (line %d)",
                    i, g->codepoint,
                    (double)g->uv.u0, (double)g->uv.v0,
                    (double)g->uv.u1, (double)g->uv.v1, __LINE__);
            fail_count++;
            forge_ui_atlas_free(&atlas);
            return;
        }
    }
    pass_count++;

    forge_ui_atlas_free(&atlas);
}

/* ── Test: white pixel region has valid UVs ──────────────────────────────── */

static void test_atlas_white_pixel(void)
{
    TEST("atlas_build: white pixel UVs are in [0,1] and ordered");
    if (!font_loaded) return;

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       codepoints, ASCII_COUNT,
                                       ATLAS_PADDING, &atlas);
    ASSERT_TRUE(result);

    /* White pixel UVs must be in [0, 1] */
    ASSERT_TRUE(atlas.white_uv.u0 >= 0.0f && atlas.white_uv.u0 <= 1.0f);
    ASSERT_TRUE(atlas.white_uv.v0 >= 0.0f && atlas.white_uv.v0 <= 1.0f);
    ASSERT_TRUE(atlas.white_uv.u1 >= 0.0f && atlas.white_uv.u1 <= 1.0f);
    ASSERT_TRUE(atlas.white_uv.v1 >= 0.0f && atlas.white_uv.v1 <= 1.0f);

    /* Must be ordered */
    ASSERT_TRUE(atlas.white_uv.u0 < atlas.white_uv.u1);
    ASSERT_TRUE(atlas.white_uv.v0 < atlas.white_uv.v1);

    /* The white pixel region should actually contain white (255) pixels */
    int wx = (int)(atlas.white_uv.u0 * (float)atlas.width + 0.5f);
    int wy = (int)(atlas.white_uv.v0 * (float)atlas.height + 0.5f);
    ASSERT_TRUE(wx >= 0 && wx < atlas.width);
    ASSERT_TRUE(wy >= 0 && wy < atlas.height);
    ASSERT_TRUE(atlas.pixels[wy * atlas.width + wx] == 255);

    forge_ui_atlas_free(&atlas);
}

/* ── Test: UV round-trip recovers correct pixel positions ────────────────── */

static void test_atlas_uv_roundtrip(void)
{
    TEST("atlas_build: UV round-trip for 'A' recovers pixel position");
    if (!font_loaded) return;

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       codepoints, ASCII_COUNT,
                                       ATLAS_PADDING, &atlas);
    ASSERT_TRUE(result);

    const ForgeUiPackedGlyph *g = forge_ui_atlas_lookup(&atlas, 'A');
    ASSERT_TRUE(g != NULL);
    ASSERT_TRUE(g->bitmap_w > 0 && g->bitmap_h > 0);

    /* Convert UV back to pixel coordinates */
    int px = (int)(g->uv.u0 * (float)atlas.width + 0.5f);
    int py = (int)(g->uv.v0 * (float)atlas.height + 0.5f);
    int px1 = (int)(g->uv.u1 * (float)atlas.width + 0.5f);
    int py1 = (int)(g->uv.v1 * (float)atlas.height + 0.5f);

    /* Validate pixel coordinates are within atlas bounds before indexing
     * into atlas.pixels[] in the loop below. */
    ASSERT_TRUE(px >= 0 && px < atlas.width);
    ASSERT_TRUE(py >= 0 && py < atlas.height);
    ASSERT_TRUE(px1 > 0 && px1 <= atlas.width);
    ASSERT_TRUE(py1 > 0 && py1 <= atlas.height);

    /* The UV-derived width and height should match bitmap_w and bitmap_h */
    ASSERT_EQ_INT(px1 - px, g->bitmap_w);
    ASSERT_EQ_INT(py1 - py, g->bitmap_h);

    /* The pixel region should contain non-zero data (glyph A has ink) */
    int filled = 0;
    for (int row = 0; row < g->bitmap_h; row++) {
        for (int col = 0; col < g->bitmap_w; col++) {
            if (atlas.pixels[(py + row) * atlas.width + (px + col)] > 0) {
                filled++;
            }
        }
    }
    ASSERT_TRUE(filled > 0);

    forge_ui_atlas_free(&atlas);
}

/* ── Test: atlas_build rejects zero codepoints ───────────────────────────── */

static void test_atlas_build_empty(void)
{
    TEST("atlas_build: returns false with zero codepoints");
    if (!font_loaded) return;

    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       NULL, 0, ATLAS_PADDING, &atlas);
    ASSERT_TRUE(!result);
}

/* ── Test: atlas_free is safe on zeroed struct ───────────────────────────── */

static void test_atlas_free_zeroed(void)
{
    TEST("atlas_free: safe on zero-initialized struct");
    ForgeUiFontAtlas atlas;
    SDL_memset(&atlas, 0, sizeof(atlas));
    forge_ui_atlas_free(&atlas); /* must not crash */
    pass_count++;
}

/* ── Test: space glyph in atlas has zero-size bitmap ─────────────────────── */

static void test_atlas_space_glyph(void)
{
    TEST("atlas_build: space glyph has zero-size bitmap but valid advance");
    if (!font_loaded) return;

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    bool result = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                       codepoints, ASCII_COUNT,
                                       ATLAS_PADDING, &atlas);
    ASSERT_TRUE(result);

    const ForgeUiPackedGlyph *g = forge_ui_atlas_lookup(&atlas, ' ');
    ASSERT_TRUE(g != NULL);
    ASSERT_EQ_INT(g->bitmap_w, 0);
    ASSERT_EQ_INT(g->bitmap_h, 0);
    ASSERT_TRUE(g->advance_width > 0);

    forge_ui_atlas_free(&atlas);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── BMP Writer Tests ──────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* BMP file header constants for validation */
#define BMP_HEADER_SIZE   14  /* BITMAPFILEHEADER */
#define BMP_INFO_SIZE     40  /* BITMAPINFOHEADER */
#define BMP_PALETTE_SIZE  1024 /* 256 * 4 bytes */
#define BMP_TEST_PATH     "test_output.bmp"

/* ── Test: BMP writer produces a valid file ──────────────────────────────── */

static void test_bmp_write_basic(void)
{
    TEST("write_grayscale_bmp: writes a valid BMP file");

    /* Create a small 4x4 test image */
    Uint8 pixels[16];
    for (int i = 0; i < 16; i++) {
        pixels[i] = (Uint8)(i * 16); /* gradient 0-240 */
    }

    bool result = forge_ui__write_grayscale_bmp(BMP_TEST_PATH, pixels, 4, 4);
    ASSERT_TRUE(result);

    /* Read back and verify BMP header */
    size_t file_size = 0;
    Uint8 *data = (Uint8 *)SDL_LoadFile(BMP_TEST_PATH, &file_size);
    ASSERT_TRUE(data != NULL);

    /* Check BMP signature */
    ASSERT_TRUE(data[0] == 'B' && data[1] == 'M');

    /* Check minimum file size: header + info + palette + pixels */
    int row_stride = (4 + 3) & ~3; /* 4 bytes (already aligned) */
    Uint32 expected_size = (Uint32)(BMP_HEADER_SIZE + BMP_INFO_SIZE +
                                     BMP_PALETTE_SIZE + row_stride * 4);
    ASSERT_EQ_U32((Uint32)file_size, expected_size);

    /* Check bits per pixel is 8.  BMP is always little-endian, so the
     * biBitCount field at BITMAPINFOHEADER offset 14 is a single byte
     * for values <= 255.  This byte-index check is intentional and
     * works correctly regardless of host endianness. */
    Uint8 *info = data + BMP_HEADER_SIZE;
    ASSERT_TRUE(info[14] == 8);

    SDL_free(data);

    /* Clean up test file */
    remove(BMP_TEST_PATH);
}

/* ── Test: BMP row padding for odd widths ────────────────────────────────── */

static void test_bmp_write_odd_width(void)
{
    TEST("write_grayscale_bmp: handles odd width (row padding)");

    /* Width 3 requires padding to 4-byte boundary */
    Uint8 pixels[9]; /* 3x3 */
    SDL_memset(pixels, 128, sizeof(pixels));

    bool result = forge_ui__write_grayscale_bmp("test_odd.bmp", pixels, 3, 3);
    ASSERT_TRUE(result);

    size_t file_size = 0;
    Uint8 *data = (Uint8 *)SDL_LoadFile("test_odd.bmp", &file_size);
    ASSERT_TRUE(data != NULL);

    /* Row stride for width 3: (3 + 3) & ~3 = 4 */
    int row_stride = 4;
    Uint32 expected_size = (Uint32)(BMP_HEADER_SIZE + BMP_INFO_SIZE +
                                     BMP_PALETTE_SIZE + row_stride * 3);
    ASSERT_EQ_U32((Uint32)file_size, expected_size);

    SDL_free(data);
    remove("test_odd.bmp");
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Text Layout Tests ─────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* Shared atlas built once for text layout tests */
static ForgeUiFontAtlas test_atlas;
static bool atlas_built = false;

/* ── Test: atlas metrics are populated after build ───────────────────────── */

static void test_atlas_metrics_populated(void)
{
    TEST("atlas_build: font metrics fields are set correctly");
    if (!font_loaded) return;

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    atlas_built = forge_ui_atlas_build(&test_font, ATLAS_PIXEL_HEIGHT,
                                        codepoints, ASCII_COUNT,
                                        ATLAS_PADDING, &test_atlas);
    ASSERT_TRUE(atlas_built);

    /* Verify font metrics were copied from the font */
    ASSERT_TRUE(test_atlas.pixel_height == ATLAS_PIXEL_HEIGHT);
    ASSERT_EQ_U16(test_atlas.units_per_em, 2048);
    ASSERT_EQ_I16(test_atlas.ascender, 1705);
    ASSERT_EQ_I16(test_atlas.descender, -615);
    ASSERT_EQ_I16(test_atlas.line_gap, 0);
}

/* ── Test: layout single line of text ────────────────────────────────────── */

static void test_layout_single_line(void)
{
    TEST("text_layout: single line 'Hello' produces correct vertex/index counts");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "Hello", 0.0f, 0.0f,
                                        NULL, &layout);
    ASSERT_TRUE(result);

    /* "Hello" = 5 visible characters, each producing 4 vertices and 6 indices */
    ASSERT_EQ_INT(layout.vertex_count, 5 * 4);
    ASSERT_EQ_INT(layout.index_count, 5 * 6);
    ASSERT_EQ_INT(layout.line_count, 1);
    ASSERT_TRUE(layout.total_width > 0.0f);
    ASSERT_TRUE(layout.total_height > 0.0f);
    ASSERT_TRUE(layout.vertices != NULL);
    ASSERT_TRUE(layout.indices != NULL);

    forge_ui_text_layout_free(&layout);
}

/* ── Test: layout empty string ───────────────────────────────────────────── */

static void test_layout_empty_string(void)
{
    TEST("text_layout: empty string returns true with zero counts");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "", 0.0f, 0.0f,
                                        NULL, &layout);
    ASSERT_TRUE(result);
    ASSERT_EQ_INT(layout.vertex_count, 0);
    ASSERT_EQ_INT(layout.index_count, 0);
    ASSERT_EQ_INT(layout.line_count, 1);

    forge_ui_text_layout_free(&layout);
}

/* ── Test: layout NULL parameters ────────────────────────────────────────── */

static void test_layout_null_params(void)
{
    TEST("text_layout: NULL atlas/text/output returns false");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextLayout layout;
    ASSERT_TRUE(!forge_ui_text_layout(NULL, "test", 0.0f, 0.0f, NULL, &layout));
    ASSERT_TRUE(!forge_ui_text_layout(&test_atlas, NULL, 0.0f, 0.0f,
                                       NULL, &layout));
    ASSERT_TRUE(!forge_ui_text_layout(&test_atlas, "test", 0.0f, 0.0f,
                                       NULL, NULL));
}

/* ── Test: space character advances pen but emits no quad ────────────────── */

static void test_layout_space_no_quad(void)
{
    TEST("text_layout: space advances pen but emits no quad");
    ASSERT_TRUE(atlas_built);

    /* "A B" = 2 visible glyphs (A and B), 1 space (no quad) */
    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "A B", 0.0f, 0.0f,
                                        NULL, &layout);
    ASSERT_TRUE(result);

    /* Only 2 visible characters → 2 quads */
    ASSERT_EQ_INT(layout.vertex_count, 2 * 4);
    ASSERT_EQ_INT(layout.index_count, 2 * 6);

    /* But total width should include the space advance */
    ForgeUiTextLayout layout_ab;
    bool result_ab = forge_ui_text_layout(&test_atlas, "AB", 0.0f, 0.0f,
                                           NULL, &layout_ab);
    ASSERT_TRUE(result_ab);
    ASSERT_TRUE(layout.total_width > layout_ab.total_width);

    forge_ui_text_layout_free(&layout);
    forge_ui_text_layout_free(&layout_ab);
}

/* ── Test: newline creates multiple lines ─────────────────────────────────── */

static void test_layout_newline(void)
{
    TEST("text_layout: newline creates multiple lines");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "A\nB\nC", 0.0f, 0.0f,
                                        NULL, &layout);
    ASSERT_TRUE(result);
    ASSERT_EQ_INT(layout.line_count, 3);

    /* 3 visible characters → 3 quads */
    ASSERT_EQ_INT(layout.vertex_count, 3 * 4);
    ASSERT_EQ_INT(layout.index_count, 3 * 6);

    /* Total height should accommodate 3 lines */
    ForgeUiTextLayout single;
    bool result_s = forge_ui_text_layout(&test_atlas, "A", 0.0f, 0.0f,
                                          NULL, &single);
    ASSERT_TRUE(result_s);
    ASSERT_TRUE(layout.total_height > single.total_height);

    forge_ui_text_layout_free(&layout);
    forge_ui_text_layout_free(&single);
}

/* ── Test: line wrapping at max_width ────────────────────────────────────── */

static void test_layout_wrapping(void)
{
    TEST("text_layout: wraps lines at max_width");
    ASSERT_TRUE(atlas_built);

    /* Get advance width for one character to set a reasonable max_width */
    ForgeUiTextMetrics m_one = forge_ui_text_measure(&test_atlas, "A", NULL);
    ASSERT_TRUE(m_one.width > 0.0f);

    /* Set max_width to fit ~3 characters — "ABCDE" should wrap */
    ForgeUiTextOpts opts;
    SDL_memset(&opts, 0, sizeof(opts));
    opts.max_width = m_one.width * 3.5f;
    opts.alignment = FORGE_UI_TEXT_ALIGN_LEFT;
    opts.r = 1.0f; opts.g = 1.0f; opts.b = 1.0f; opts.a = 1.0f;

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "ABCDE", 0.0f, 0.0f,
                                        &opts, &layout);
    ASSERT_TRUE(result);

    /* Should wrap to at least 2 lines */
    ASSERT_TRUE(layout.line_count >= 2);

    /* All 5 characters should still be emitted */
    ASSERT_EQ_INT(layout.vertex_count, 5 * 4);
    ASSERT_EQ_INT(layout.index_count, 5 * 6);

    forge_ui_text_layout_free(&layout);
}

/* ── Test: vertex positions start at the specified origin ─────────────────── */

static void test_layout_origin(void)
{
    TEST("text_layout: vertex positions offset by origin (x, y)");
    ASSERT_TRUE(atlas_built);

    float ox = 100.0f;
    float oy = 200.0f;

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "A", ox, oy,
                                        NULL, &layout);
    ASSERT_TRUE(result);
    ASSERT_TRUE(layout.vertex_count >= 4);

    /* All vertex x positions should be near the origin x (pen + bearing) */
    bool x_near_origin = true;
    for (int i = 0; i < layout.vertex_count; i++) {
        /* pos_x should be >= origin (bearing can shift slightly right) */
        if (layout.vertices[i].pos_x < ox - 1.0f) {
            x_near_origin = false;
            break;
        }
    }
    ASSERT_TRUE(x_near_origin);

    forge_ui_text_layout_free(&layout);
}

/* ── Test: vertex UVs are within atlas range [0, 1] ──────────────────────── */

static void test_layout_uv_range(void)
{
    TEST("text_layout: vertex UVs are in [0.0, 1.0] range");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "Test!", 0.0f, 0.0f,
                                        NULL, &layout);
    ASSERT_TRUE(result);

    for (int i = 0; i < layout.vertex_count; i++) {
        if (layout.vertices[i].uv_u < 0.0f || layout.vertices[i].uv_u > 1.0f ||
            layout.vertices[i].uv_v < 0.0f || layout.vertices[i].uv_v > 1.0f) {
            SDL_Log("    FAIL: vertex %d UV (%.4f, %.4f) out of [0,1] (line %d)",
                    i, (double)layout.vertices[i].uv_u,
                    (double)layout.vertices[i].uv_v, __LINE__);
            fail_count++;
            forge_ui_text_layout_free(&layout);
            return;
        }
    }
    pass_count++;

    forge_ui_text_layout_free(&layout);
}

/* ── Test: vertex colors match opts ──────────────────────────────────────── */

static void test_layout_vertex_color(void)
{
    TEST("text_layout: vertex colors match opts color");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextOpts opts;
    SDL_memset(&opts, 0, sizeof(opts));
    opts.r = 0.5f; opts.g = 0.25f; opts.b = 0.75f; opts.a = 1.0f;

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "A", 0.0f, 0.0f,
                                        &opts, &layout);
    ASSERT_TRUE(result);

    for (int i = 0; i < layout.vertex_count; i++) {
        if (layout.vertices[i].r != 0.5f || layout.vertices[i].g != 0.25f ||
            layout.vertices[i].b != 0.75f || layout.vertices[i].a != 1.0f) {
            SDL_Log("    FAIL: vertex %d color mismatch (line %d)", i, __LINE__);
            fail_count++;
            forge_ui_text_layout_free(&layout);
            return;
        }
    }
    pass_count++;

    forge_ui_text_layout_free(&layout);
}

/* ── Test: index values reference valid vertices ─────────────────────────── */

static void test_layout_index_bounds(void)
{
    TEST("text_layout: all indices reference valid vertices");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "Hello!", 0.0f, 0.0f,
                                        NULL, &layout);
    ASSERT_TRUE(result);

    for (int i = 0; i < layout.index_count; i++) {
        if (layout.indices[i] >= (Uint32)layout.vertex_count) {
            SDL_Log("    FAIL: index[%d] = %u >= vertex_count %d (line %d)",
                    i, layout.indices[i], layout.vertex_count, __LINE__);
            fail_count++;
            forge_ui_text_layout_free(&layout);
            return;
        }
    }
    pass_count++;

    forge_ui_text_layout_free(&layout);
}

/* ── Test: CCW winding order for each quad ───────────────────────────────── */

static void test_layout_ccw_winding(void)
{
    TEST("text_layout: index pattern is (0,1,2, 2,3,0) per quad");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "AB", 0.0f, 0.0f,
                                        NULL, &layout);
    ASSERT_TRUE(result);

    /* 2 quads, each with 6 indices following pattern (base+0,1,2, 2,3,0) */
    ASSERT_EQ_INT(layout.index_count, 12);

    for (int q = 0; q < 2; q++) {
        Uint32 base = (Uint32)(q * 4);
        int idx_off = q * 6;
        ASSERT_EQ_U32(layout.indices[idx_off + 0], base + 0);
        ASSERT_EQ_U32(layout.indices[idx_off + 1], base + 1);
        ASSERT_EQ_U32(layout.indices[idx_off + 2], base + 2);
        ASSERT_EQ_U32(layout.indices[idx_off + 3], base + 2);
        ASSERT_EQ_U32(layout.indices[idx_off + 4], base + 3);
        ASSERT_EQ_U32(layout.indices[idx_off + 5], base + 0);
    }

    forge_ui_text_layout_free(&layout);
}

/* ── Test: default opts (NULL) uses opaque white ─────────────────────────── */

static void test_layout_default_opts(void)
{
    TEST("text_layout: NULL opts uses opaque white color");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&test_atlas, "X", 0.0f, 0.0f,
                                        NULL, &layout);
    ASSERT_TRUE(result);
    ASSERT_TRUE(layout.vertex_count >= 4);

    /* Default color is opaque white (1, 1, 1, 1) */
    for (int i = 0; i < layout.vertex_count; i++) {
        if (layout.vertices[i].r != 1.0f || layout.vertices[i].g != 1.0f ||
            layout.vertices[i].b != 1.0f || layout.vertices[i].a != 1.0f) {
            SDL_Log("    FAIL: vertex %d not opaque white with NULL opts "
                    "(line %d)", i, __LINE__);
            fail_count++;
            forge_ui_text_layout_free(&layout);
            return;
        }
    }
    pass_count++;

    forge_ui_text_layout_free(&layout);
}

/* ── Test: center alignment shifts vertices ──────────────────────────────── */

static void test_layout_center_alignment(void)
{
    TEST("text_layout: center alignment shifts vertices right");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextOpts opts_left;
    SDL_memset(&opts_left, 0, sizeof(opts_left));
    opts_left.max_width = 500.0f;
    opts_left.alignment = FORGE_UI_TEXT_ALIGN_LEFT;
    opts_left.r = 1.0f; opts_left.g = 1.0f; opts_left.b = 1.0f; opts_left.a = 1.0f;

    ForgeUiTextOpts opts_center = opts_left;
    opts_center.alignment = FORGE_UI_TEXT_ALIGN_CENTER;

    ForgeUiTextLayout layout_left, layout_center;
    ASSERT_TRUE(forge_ui_text_layout(&test_atlas, "Hi", 0.0f, 0.0f,
                                      &opts_left, &layout_left));
    ASSERT_TRUE(forge_ui_text_layout(&test_atlas, "Hi", 0.0f, 0.0f,
                                      &opts_center, &layout_center));

    /* Center-aligned vertices should have larger x positions than left */
    ASSERT_TRUE(layout_center.vertices[0].pos_x >
                layout_left.vertices[0].pos_x);

    forge_ui_text_layout_free(&layout_left);
    forge_ui_text_layout_free(&layout_center);
}

/* ── Test: right alignment shifts vertices ───────────────────────────────── */

static void test_layout_right_alignment(void)
{
    TEST("text_layout: right alignment shifts vertices further than center");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextOpts opts;
    SDL_memset(&opts, 0, sizeof(opts));
    opts.max_width = 500.0f;
    opts.r = 1.0f; opts.g = 1.0f; opts.b = 1.0f; opts.a = 1.0f;

    ForgeUiTextLayout layout_center, layout_right;

    opts.alignment = FORGE_UI_TEXT_ALIGN_CENTER;
    ASSERT_TRUE(forge_ui_text_layout(&test_atlas, "Hi", 0.0f, 0.0f,
                                      &opts, &layout_center));

    opts.alignment = FORGE_UI_TEXT_ALIGN_RIGHT;
    ASSERT_TRUE(forge_ui_text_layout(&test_atlas, "Hi", 0.0f, 0.0f,
                                      &opts, &layout_right));

    /* Right-aligned first vertex x should be greater than center-aligned */
    ASSERT_TRUE(layout_right.vertices[0].pos_x >
                layout_center.vertices[0].pos_x);

    forge_ui_text_layout_free(&layout_center);
    forge_ui_text_layout_free(&layout_right);
}

/* ── Test: layout_free is safe on zeroed struct ──────────────────────────── */

static void test_layout_free_zeroed(void)
{
    TEST("text_layout_free: safe on zero-initialized struct");
    ForgeUiTextLayout layout;
    SDL_memset(&layout, 0, sizeof(layout));
    forge_ui_text_layout_free(&layout); /* must not crash */
    pass_count++;
}

/* ── Test: layout_free is safe with NULL ──────────────────────────────────── */

static void test_layout_free_null(void)
{
    TEST("text_layout_free: safe with NULL pointer");
    forge_ui_text_layout_free(NULL); /* must not crash */
    pass_count++;
}

/* ── Test: layout rejects atlas with units_per_em == 0 ───────────────────── */

static void test_layout_invalid_atlas(void)
{
    TEST("text_layout: returns false for atlas with units_per_em == 0");

    /* Construct a zeroed atlas — simulates a corrupt or uninitialized atlas */
    ForgeUiFontAtlas bad_atlas;
    SDL_memset(&bad_atlas, 0, sizeof(bad_atlas));

    ForgeUiTextLayout layout;
    bool result = forge_ui_text_layout(&bad_atlas, "test", 0.0f, 0.0f,
                                        NULL, &layout);
    ASSERT_TRUE(!result);
}

/* ── Test: measure returns zero for atlas with units_per_em == 0 ─────────── */

static void test_measure_invalid_atlas(void)
{
    TEST("text_measure: returns zero metrics for atlas with units_per_em == 0");

    ForgeUiFontAtlas bad_atlas;
    SDL_memset(&bad_atlas, 0, sizeof(bad_atlas));

    ForgeUiTextMetrics m = forge_ui_text_measure(&bad_atlas, "test", NULL);
    ASSERT_EQ_INT(m.line_count, 0);
    ASSERT_TRUE(m.width == 0.0f);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Text Measure Tests ────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── Test: measure single line matches layout dimensions ─────────────────── */

static void test_measure_matches_layout(void)
{
    TEST("text_measure: matches layout dimensions for 'Hello'");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextLayout layout;
    ASSERT_TRUE(forge_ui_text_layout(&test_atlas, "Hello", 0.0f, 0.0f,
                                      NULL, &layout));

    ForgeUiTextMetrics metrics = forge_ui_text_measure(&test_atlas, "Hello",
                                                        NULL);

    /* Width and height should match (epsilon for float comparison) */
    const float eps = 1e-3f;
    ASSERT_TRUE(SDL_fabsf(metrics.width - layout.total_width) < eps);
    ASSERT_TRUE(SDL_fabsf(metrics.height - layout.total_height) < eps);
    ASSERT_EQ_INT(metrics.line_count, layout.line_count);

    forge_ui_text_layout_free(&layout);
}

/* ── Test: measure empty string ──────────────────────────────────────────── */

static void test_measure_empty_string(void)
{
    TEST("text_measure: empty string returns zero size, 1 line");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextMetrics m = forge_ui_text_measure(&test_atlas, "", NULL);
    ASSERT_TRUE(m.width == 0.0f);
    ASSERT_TRUE(m.height == 0.0f);
    ASSERT_EQ_INT(m.line_count, 1);
}

/* ── Test: measure NULL parameters ───────────────────────────────────────── */

static void test_measure_null_params(void)
{
    TEST("text_measure: NULL atlas or text returns zero metrics");
    ForgeUiTextMetrics m1 = forge_ui_text_measure(NULL, "test", NULL);
    ASSERT_EQ_INT(m1.line_count, 0);
    ASSERT_TRUE(m1.width == 0.0f);

    ForgeUiTextMetrics m2 = forge_ui_text_measure(&test_atlas, NULL, NULL);
    ASSERT_EQ_INT(m2.line_count, 0);
    ASSERT_TRUE(m2.width == 0.0f);
}

/* ── Test: measure multi-line ────────────────────────────────────────────── */

static void test_measure_multiline(void)
{
    TEST("text_measure: newlines produce correct line count");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextMetrics m = forge_ui_text_measure(&test_atlas, "A\nB\nC", NULL);
    ASSERT_EQ_INT(m.line_count, 3);
    ASSERT_TRUE(m.width > 0.0f);
    ASSERT_TRUE(m.height > 0.0f);
}

/* ── Test: measure with wrapping ─────────────────────────────────────────── */

static void test_measure_wrapping(void)
{
    TEST("text_measure: wrapping increases line count");
    ASSERT_TRUE(atlas_built);

    ForgeUiTextMetrics m_nowrap = forge_ui_text_measure(&test_atlas,
                                                         "ABCDEFGH", NULL);
    ASSERT_EQ_INT(m_nowrap.line_count, 1);

    /* Set max_width to fit ~3 characters */
    ForgeUiTextOpts opts;
    SDL_memset(&opts, 0, sizeof(opts));
    opts.max_width = m_nowrap.width * 0.4f;
    opts.r = 1.0f; opts.g = 1.0f; opts.b = 1.0f; opts.a = 1.0f;

    ForgeUiTextMetrics m_wrap = forge_ui_text_measure(&test_atlas,
                                                       "ABCDEFGH", &opts);
    ASSERT_TRUE(m_wrap.line_count > 1);
    ASSERT_TRUE(m_wrap.height > m_nowrap.height);
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

    SDL_Log("=== UI Library Tests ===");

    /* Font loading */
    test_font_load();
    test_font_load_nonexistent();

    /* Table directory */
    test_table_directory();
    test_table_lookup();
    test_table_entry_bounds();

    /* head table */
    test_head_units_per_em();
    test_head_index_to_loc_format();
    test_head_bounding_box();

    /* hhea table */
    test_hhea_metrics();
    test_hhea_num_hmetrics();

    /* maxp table */
    test_maxp_num_glyphs();

    /* cmap lookups */
    test_cmap_ascii_a();
    test_cmap_ascii_g();
    test_cmap_space();
    test_cmap_unmapped();
    test_cmap_beyond_bmp();

    /* Glyph loading */
    test_glyph_out_of_range();
    test_glyph_space();
    test_glyph_a_outline();
    test_glyph_free_zeroed();

    /* loca validation */
    test_loca_monotonic();
    test_head_index_to_loc_valid();
    test_glyph_reject_reversed_loca();
    test_glyph_reject_out_of_bounds_loca();

    /* Rasterizer */
    test_raster_basic();
    test_raster_donut();
    test_raster_whitespace();
    test_raster_antialiasing();
    test_raster_no_aa();
    test_raster_bitmap_free_zeroed();
    test_raster_default_opts();

    /* hmtx / advance width */
    test_hmtx_loaded();
    test_hmtx_advance_width_a();
    test_hmtx_advance_width_trailing();
    test_hmtx_last_advance();

    /* Font atlas */
    test_atlas_build();
    test_atlas_power_of_two();
    test_atlas_lookup_found();
    test_atlas_lookup_missing();
    test_atlas_uv_range();
    test_atlas_uv_ordering();
    test_atlas_white_pixel();
    test_atlas_uv_roundtrip();
    test_atlas_build_empty();
    test_atlas_free_zeroed();
    test_atlas_space_glyph();

    /* BMP writer */
    test_bmp_write_basic();
    test_bmp_write_odd_width();

    /* Text layout — atlas metrics */
    test_atlas_metrics_populated();

    /* Text layout — forge_ui_text_layout */
    test_layout_single_line();
    test_layout_empty_string();
    test_layout_null_params();
    test_layout_space_no_quad();
    test_layout_newline();
    test_layout_wrapping();
    test_layout_origin();
    test_layout_uv_range();
    test_layout_vertex_color();
    test_layout_index_bounds();
    test_layout_ccw_winding();
    test_layout_default_opts();
    test_layout_center_alignment();
    test_layout_right_alignment();

    /* Text layout — forge_ui_text_layout_free */
    test_layout_free_zeroed();
    test_layout_free_null();

    /* Text layout — invalid atlas (units_per_em == 0) */
    test_layout_invalid_atlas();
    test_measure_invalid_atlas();

    /* Text layout — forge_ui_text_measure */
    test_measure_matches_layout();
    test_measure_empty_string();
    test_measure_null_params();
    test_measure_multiline();
    test_measure_wrapping();

    /* Print summary before tearing down SDL (SDL_Log needs SDL alive) */
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
