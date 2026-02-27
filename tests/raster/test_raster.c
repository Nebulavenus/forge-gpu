/*
 * Raster Library Tests
 *
 * Automated tests for common/raster/forge_raster.h -- CPU triangle
 * rasterizer including buffer management, clearing, edge-function
 * rasterization, barycentric interpolation, texture sampling,
 * alpha blending, indexed drawing, and BMP writing.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <assert.h>  /* assert() for defensive bounds checks in helpers */
#include <stdio.h>   /* remove() for cleaning up test BMP files */
#include "raster/forge_raster.h"

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

#define ASSERT_EQ_BYTE(a, b)                                      \
    do {                                                          \
        Uint8 _a = (a), _b = (b);                                 \
        if (_a != _b) {                                           \
            SDL_Log("    FAIL: %s == %u, expected %u (line %d)",  \
                    #a, _a, _b, __LINE__);                        \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

/* Allow a tolerance of +/- 1 for byte comparisons (rounding) */
#define ASSERT_NEAR_BYTE(a, b, tol)                               \
    do {                                                          \
        Uint8 _a = (a), _b = (b);                                 \
        int diff = (int)_a - (int)_b;                             \
        if (diff < -(tol) || diff > (tol)) {                      \
            SDL_Log("    FAIL: %s == %u, expected %u +/-%d "      \
                    "(line %d)", #a, _a, _b, (tol), __LINE__);    \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

/* ── Helper: read pixel at (x, y) ────────────────────────────────────────── */

static void get_pixel(const ForgeRasterBuffer *buf, int x, int y,
                      Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
    assert(x >= 0 && x < buf->width && "get_pixel: x out of bounds");
    assert(y >= 0 && y < buf->height && "get_pixel: y out of bounds");
    const Uint8 *p = buf->pixels + y * buf->stride + x * FORGE_RASTER_BPP;
    *r = p[0];
    *g = p[1];
    *b = p[2];
    *a = p[3];
}

/* ── Tests ───────────────────────────────────────────────────────────────── */

static void test_buffer_create(void)
{
    TEST("buffer_create: valid dimensions");
    ForgeRasterBuffer buf = forge_raster_buffer_create(64, 32);
    ASSERT_TRUE(buf.pixels != NULL);
    ASSERT_EQ_INT(buf.width, 64);
    ASSERT_EQ_INT(buf.height, 32);
    ASSERT_EQ_INT(buf.stride, 64 * FORGE_RASTER_BPP);
    forge_raster_buffer_destroy(&buf);

    TEST("buffer_create: zero dimensions");
    ForgeRasterBuffer bad = forge_raster_buffer_create(0, 0);
    ASSERT_TRUE(bad.pixels == NULL);
}

static void test_buffer_clear(void)
{
    TEST("buffer_clear: solid white");
    ForgeRasterBuffer buf = forge_raster_buffer_create(4, 4);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 1.0f, 1.0f, 1.0f, 1.0f);

    Uint8 r, g, b, a;
    get_pixel(&buf, 0, 0, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 255);
    ASSERT_EQ_BYTE(g, 255);
    ASSERT_EQ_BYTE(b, 255);
    ASSERT_EQ_BYTE(a, 255);

    get_pixel(&buf, 3, 3, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 255);
    ASSERT_EQ_BYTE(g, 255);
    ASSERT_EQ_BYTE(b, 255);
    ASSERT_EQ_BYTE(a, 255);

    TEST("buffer_clear: specific color");
    forge_raster_clear(&buf, 0.5f, 0.0f, 1.0f, 0.5f);
    get_pixel(&buf, 2, 2, &r, &g, &b, &a);
    ASSERT_NEAR_BYTE(r, 128, 1);
    ASSERT_EQ_BYTE(g, 0);
    ASSERT_EQ_BYTE(b, 255);
    ASSERT_NEAR_BYTE(a, 128, 1);

    forge_raster_buffer_destroy(&buf);
}

static void test_solid_triangle(void)
{
    TEST("solid_triangle: center pixel is filled");
    ForgeRasterBuffer buf = forge_raster_buffer_create(16, 16);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* Triangle covering the center of the 16x16 buffer.
     * CCW winding: top-center, bottom-left, bottom-right */
    ForgeRasterVertex v0 = { 8.0f,  2.0f,  0, 0,  1, 0, 0, 1 };
    ForgeRasterVertex v1 = { 2.0f,  14.0f, 0, 0,  1, 0, 0, 1 };
    ForgeRasterVertex v2 = { 14.0f, 14.0f, 0, 0,  1, 0, 0, 1 };
    forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

    /* The center of the triangle (roughly 8, 10) should be red */
    Uint8 r, g, b, a;
    get_pixel(&buf, 8, 10, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 255);
    ASSERT_EQ_BYTE(g, 0);
    ASSERT_EQ_BYTE(b, 0);
    ASSERT_EQ_BYTE(a, 255);

    /* A corner pixel (0, 0) should remain black (background) */
    get_pixel(&buf, 0, 0, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 0);
    ASSERT_EQ_BYTE(g, 0);
    ASSERT_EQ_BYTE(b, 0);

    forge_raster_buffer_destroy(&buf);
}

static void test_color_interpolation(void)
{
    TEST("color_interpolation: barycentric blending");
    ForgeRasterBuffer buf = forge_raster_buffer_create(32, 32);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* RGB triangle: red top, green bottom-left, blue bottom-right */
    ForgeRasterVertex v0 = { 16.0f, 2.0f,  0, 0,  1, 0, 0, 1 }; /* red */
    ForgeRasterVertex v1 = { 2.0f,  30.0f, 0, 0,  0, 1, 0, 1 }; /* green */
    ForgeRasterVertex v2 = { 30.0f, 30.0f, 0, 0,  0, 0, 1, 1 }; /* blue */
    forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

    /* Near the top vertex: should be mostly red */
    Uint8 r, g, b, a;
    get_pixel(&buf, 16, 6, &r, &g, &b, &a);
    ASSERT_TRUE(r > 150);
    ASSERT_TRUE(g < 100);
    ASSERT_TRUE(b < 100);

    /* Near the bottom-left vertex: should be mostly green */
    get_pixel(&buf, 6, 26, &r, &g, &b, &a);
    ASSERT_TRUE(g > 100);

    /* Near the bottom-right vertex: should be mostly blue */
    get_pixel(&buf, 26, 26, &r, &g, &b, &a);
    ASSERT_TRUE(b > 100);

    /* The centroid (average of all three vertices) should have roughly
     * equal contributions from each color channel */
    int cx = (16 + 2 + 30) / 3;
    int cy = (2 + 30 + 30) / 3;
    get_pixel(&buf, cx, cy, &r, &g, &b, &a);
    ASSERT_TRUE(r > 40 && r < 140);
    ASSERT_TRUE(g > 40 && g < 140);
    ASSERT_TRUE(b > 40 && b < 140);

    forge_raster_buffer_destroy(&buf);
}

static void test_indexed_drawing(void)
{
    TEST("indexed_drawing: quad from 4 vertices + 6 indices");
    ForgeRasterBuffer buf = forge_raster_buffer_create(16, 16);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* A white quad covering pixels 4..11 in x and y */
    ForgeRasterVertex verts[4] = {
        { 4.0f,  4.0f,  0, 0,  1, 1, 1, 1 },  /* top-left */
        { 12.0f, 4.0f,  0, 0,  1, 1, 1, 1 },  /* top-right */
        { 12.0f, 12.0f, 0, 0,  1, 1, 1, 1 },  /* bottom-right */
        { 4.0f,  12.0f, 0, 0,  1, 1, 1, 1 },  /* bottom-left */
    };
    /* Two CCW triangles: (0,1,2) and (0,2,3) */
    Uint32 indices[6] = { 0, 1, 2,  0, 2, 3 };

    forge_raster_triangles_indexed(&buf, verts, 4, indices, 6, NULL);

    /* Center of the quad should be white */
    Uint8 r, g, b, a;
    get_pixel(&buf, 8, 8, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 255);
    ASSERT_EQ_BYTE(g, 255);
    ASSERT_EQ_BYTE(b, 255);

    /* Outside the quad should remain black */
    get_pixel(&buf, 1, 1, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 0);
    ASSERT_EQ_BYTE(g, 0);
    ASSERT_EQ_BYTE(b, 0);

    forge_raster_buffer_destroy(&buf);
}

static void test_texture_sampling(void)
{
    TEST("texture_sampling: grayscale checkerboard");
    ForgeRasterBuffer buf = forge_raster_buffer_create(16, 16);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* 2x2 checkerboard: white(255), black(0), black(0), white(255) */
    Uint8 tex_pixels[4] = { 255, 0, 0, 255 };
    ForgeRasterTexture tex = { tex_pixels, 2, 2 };

    /* White quad with UV mapping across the full texture */
    ForgeRasterVertex verts[4] = {
        { 0.0f,  0.0f,  0.0f, 0.0f,  1, 1, 1, 1 },  /* TL, uv(0,0) */
        { 16.0f, 0.0f,  1.0f, 0.0f,  1, 1, 1, 1 },  /* TR, uv(1,0) */
        { 16.0f, 16.0f, 1.0f, 1.0f,  1, 1, 1, 1 },  /* BR, uv(1,1) */
        { 0.0f,  16.0f, 0.0f, 1.0f,  1, 1, 1, 1 },  /* BL, uv(0,1) */
    };
    Uint32 indices[6] = { 0, 1, 2,  0, 2, 3 };
    forge_raster_triangles_indexed(&buf, verts, 4, indices, 6, &tex);

    /* Top-left quadrant should sample texel (0,0) = white */
    Uint8 r, g, b, a;
    get_pixel(&buf, 2, 2, &r, &g, &b, &a);
    ASSERT_TRUE(r > 200);

    /* Top-right quadrant should sample texel (1,0) = black */
    get_pixel(&buf, 14, 2, &r, &g, &b, &a);
    ASSERT_TRUE(r < 50);

    forge_raster_buffer_destroy(&buf);
}

static void test_alpha_blending(void)
{
    TEST("alpha_blending: source-over compositing");
    ForgeRasterBuffer buf = forge_raster_buffer_create(8, 8);
    ASSERT_TRUE(buf.pixels != NULL);

    /* Start with solid white background */
    forge_raster_clear(&buf, 1.0f, 1.0f, 1.0f, 1.0f);

    /* Draw a 50% transparent red triangle covering the test pixel.
     * We use a single triangle instead of a quad to avoid the shared-edge
     * double-blend issue (this library has no fill rule to prevent it). */
    ForgeRasterVertex v0 = { -1.0f, -1.0f, 0, 0,  1, 0, 0, 0.5f };
    ForgeRasterVertex v1 = { 10.0f, -1.0f, 0, 0,  1, 0, 0, 0.5f };
    ForgeRasterVertex v2 = { -1.0f, 10.0f, 0, 0,  1, 0, 0, 0.5f };
    forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

    /* Source-over: out_r = 1.0 * 0.5 + 1.0 * 0.5 = 1.0 -> 255
     *              out_g = 0.0 * 0.5 + 1.0 * 0.5 = 0.5 -> ~128
     *              out_b = 0.0 * 0.5 + 1.0 * 0.5 = 0.5 -> ~128 */
    Uint8 r, g, b, a;
    get_pixel(&buf, 3, 3, &r, &g, &b, &a);
    ASSERT_NEAR_BYTE(r, 255, 1);
    ASSERT_NEAR_BYTE(g, 128, 2);
    ASSERT_NEAR_BYTE(b, 128, 2);

    forge_raster_buffer_destroy(&buf);
}

static void test_degenerate_triangle(void)
{
    TEST("degenerate_triangle: zero-area triangle is skipped");
    ForgeRasterBuffer buf = forge_raster_buffer_create(8, 8);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* Collinear vertices form a degenerate triangle (zero area) */
    ForgeRasterVertex v0 = { 1.0f, 1.0f, 0, 0,  1, 1, 1, 1 };
    ForgeRasterVertex v1 = { 4.0f, 4.0f, 0, 0,  1, 1, 1, 1 };
    ForgeRasterVertex v2 = { 7.0f, 7.0f, 0, 0,  1, 1, 1, 1 };
    forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

    /* No pixels should be modified -- still black */
    Uint8 r, g, b, a;
    get_pixel(&buf, 4, 4, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 0);
    ASSERT_EQ_BYTE(g, 0);
    ASSERT_EQ_BYTE(b, 0);

    forge_raster_buffer_destroy(&buf);
}

static void test_cw_winding(void)
{
    TEST("cw_winding: clockwise triangles are also rasterized");
    ForgeRasterBuffer buf = forge_raster_buffer_create(16, 16);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* CW winding: swap v1 and v2 compared to the CCW test */
    ForgeRasterVertex v0 = { 8.0f,  2.0f,  0, 0,  0, 1, 0, 1 };
    ForgeRasterVertex v1 = { 14.0f, 14.0f, 0, 0,  0, 1, 0, 1 };
    ForgeRasterVertex v2 = { 2.0f,  14.0f, 0, 0,  0, 1, 0, 1 };
    forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

    /* Center should be green */
    Uint8 r, g, b, a;
    get_pixel(&buf, 8, 10, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(g, 255);

    forge_raster_buffer_destroy(&buf);
}

static void test_off_screen_triangle(void)
{
    TEST("off_screen_triangle: partially off-screen is clipped");
    ForgeRasterBuffer buf = forge_raster_buffer_create(8, 8);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* Triangle extends far outside the buffer -- should not crash */
    ForgeRasterVertex v0 = { -10.0f, 4.0f,  0, 0,  1, 1, 0, 1 };
    ForgeRasterVertex v1 = { 4.0f,   -10.0f, 0, 0,  1, 1, 0, 1 };
    ForgeRasterVertex v2 = { 20.0f,  20.0f,  0, 0,  1, 1, 0, 1 };
    forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

    /* Some pixels should be filled (those inside the triangle AND
     * inside the buffer bounds) */
    Uint8 r, g, b, a;
    get_pixel(&buf, 4, 4, &r, &g, &b, &a);
    ASSERT_TRUE(r > 0 || g > 0);  /* at least partially filled */

    forge_raster_buffer_destroy(&buf);
}

static void test_bmp_write(void)
{
    TEST("bmp_write: creates a valid BMP file");
    ForgeRasterBuffer buf = forge_raster_buffer_create(4, 4);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 1.0f, 0.0f, 0.5f, 1.0f);

    const char *path = "test_output.bmp";
    bool ok = forge_raster_write_bmp(&buf, path);
    ASSERT_TRUE(ok);

    /* Verify the file exists and has the expected size:
     * header (14) + info (40) + pixel data (4 * 4 * 4 = 64) = 118 */
    FILE *fp = fopen(path, "rb");
    ASSERT_TRUE(fp != NULL);
    ASSERT_TRUE(fseek(fp, 0, SEEK_END) == 0);
    long file_size = ftell(fp);
    ASSERT_TRUE(file_size != -1L);
    fclose(fp);
    ASSERT_EQ_INT((int)file_size, 118);

    /* Verify BMP magic number */
    fp = fopen(path, "rb");
    ASSERT_TRUE(fp != NULL);
    Uint8 magic[2];
    size_t nread = fread(magic, 1, 2, fp);
    fclose(fp);
    ASSERT_EQ_INT((int)nread, 2);
    ASSERT_EQ_BYTE(magic[0], 'B');
    ASSERT_EQ_BYTE(magic[1], 'M');

    /* Clean up */
    remove(path);
    forge_raster_buffer_destroy(&buf);
}

static void test_vertex_layout_size(void)
{
    TEST("vertex_layout: ForgeRasterVertex is 32 bytes");
    ASSERT_EQ_INT((int)sizeof(ForgeRasterVertex), 32);
}

/* ── Safety & Validation Tests ───────────────────────────────────────────── */

static void test_buffer_create_max_dim(void)
{
    TEST("buffer_create_max_dim: oversized dimensions rejected");

    /* Width exceeding FORGE_RASTER_MAX_DIM should fail */
    ForgeRasterBuffer buf = forge_raster_buffer_create(
        FORGE_RASTER_MAX_DIM + 1, 64);
    ASSERT_TRUE(buf.pixels == NULL);
    ASSERT_EQ_INT(buf.width, 0);
    ASSERT_EQ_INT(buf.height, 0);
    ASSERT_EQ_INT(buf.stride, 0);

    /* Height exceeding FORGE_RASTER_MAX_DIM should fail */
    buf = forge_raster_buffer_create(64, FORGE_RASTER_MAX_DIM + 1);
    ASSERT_TRUE(buf.pixels == NULL);
    ASSERT_EQ_INT(buf.width, 0);

    /* Exactly FORGE_RASTER_MAX_DIM should succeed (boundary test).
     * Use a 1-pixel height to keep allocation small. */
    buf = forge_raster_buffer_create(FORGE_RASTER_MAX_DIM, 1);
    ASSERT_TRUE(buf.pixels != NULL);
    ASSERT_EQ_INT(buf.width, FORGE_RASTER_MAX_DIM);
    forge_raster_buffer_destroy(&buf);
}

static void test_buffer_create_zeroed_on_failure(void)
{
    TEST("buffer_create_zeroed: failed create zeros all struct fields");
    ForgeRasterBuffer buf = forge_raster_buffer_create(-10, 64);
    ASSERT_TRUE(buf.pixels == NULL);
    ASSERT_EQ_INT(buf.width, 0);
    ASSERT_EQ_INT(buf.height, 0);
    ASSERT_EQ_INT(buf.stride, 0);

    buf = forge_raster_buffer_create(64, -5);
    ASSERT_TRUE(buf.pixels == NULL);
    ASSERT_EQ_INT(buf.width, 0);
    ASSERT_EQ_INT(buf.height, 0);
    ASSERT_EQ_INT(buf.stride, 0);
}

static void test_null_vertex_pointers(void)
{
    TEST("null_vertex_pointers: NULL v0/v1/v2 do not crash");
    ForgeRasterBuffer buf = forge_raster_buffer_create(8, 8);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    ForgeRasterVertex v = { 4.0f, 4.0f, 0, 0, 1, 1, 1, 1 };

    /* Each of these should return silently without crashing */
    forge_raster_triangle(&buf, NULL, &v, &v, NULL);
    forge_raster_triangle(&buf, &v, NULL, &v, NULL);
    forge_raster_triangle(&buf, &v, &v, NULL, NULL);

    /* Buffer should be unmodified (still black) */
    Uint8 r, g, b, a;
    get_pixel(&buf, 4, 4, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 0);
    ASSERT_EQ_BYTE(g, 0);
    ASSERT_EQ_BYTE(b, 0);

    forge_raster_buffer_destroy(&buf);
}

static void test_nan_inf_vertex_coords(void)
{
    TEST("nan_inf_vertex_coords: non-finite coordinates rejected");
    ForgeRasterBuffer buf = forge_raster_buffer_create(8, 8);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* Construct NaN and Infinity without <math.h> */
    volatile float zero = 0.0f;
    float nan_val = zero / zero;   /* NaN */
    float inf_val = 1.0f / zero;   /* +Infinity */

    ForgeRasterVertex good_v1 = { 0.0f, 8.0f, 0, 0, 1, 0, 0, 1 };
    ForgeRasterVertex good_v2 = { 8.0f, 8.0f, 0, 0, 1, 0, 0, 1 };

    /* NaN x-coordinate -- should be rejected */
    ForgeRasterVertex nan_v = { nan_val, 4.0f, 0, 0, 1, 0, 0, 1 };
    forge_raster_triangle(&buf, &nan_v, &good_v1, &good_v2, NULL);

    /* Infinity y-coordinate -- should be rejected */
    ForgeRasterVertex inf_v = { 4.0f, inf_val, 0, 0, 1, 0, 0, 1 };
    forge_raster_triangle(&buf, &inf_v, &good_v1, &good_v2, NULL);

    /* Negative Infinity -- should be rejected */
    ForgeRasterVertex neg_inf_v = { -inf_val, 4.0f, 0, 0, 1, 0, 0, 1 };
    forge_raster_triangle(&buf, &neg_inf_v, &good_v1, &good_v2, NULL);

    /* Buffer should be completely unmodified */
    Uint8 r, g, b, a;
    get_pixel(&buf, 4, 6, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 0);
    ASSERT_EQ_BYTE(g, 0);
    ASSERT_EQ_BYTE(b, 0);

    forge_raster_buffer_destroy(&buf);
}

static void test_near_degenerate_triangle(void)
{
    TEST("near_degenerate_triangle: nearly-collinear vertices skipped");
    ForgeRasterBuffer buf = forge_raster_buffer_create(16, 16);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* Vertices almost collinear -- area ≈ 3e-7, below the 1e-6 epsilon.
     * Without the epsilon test, 1/area would produce extreme barycentric
     * values and garbage pixel colors. */
    ForgeRasterVertex v0 = { 1.0f,       1.0f, 0, 0, 1, 1, 1, 1 };
    ForgeRasterVertex v1 = { 4.0f,       4.0f, 0, 0, 1, 1, 1, 1 };
    ForgeRasterVertex v2 = { 7.0000001f, 7.0f, 0, 0, 1, 1, 1, 1 };
    forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

    /* Should be skipped -- still black */
    Uint8 r, g, b, a;
    get_pixel(&buf, 4, 4, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 0);
    ASSERT_EQ_BYTE(g, 0);
    ASSERT_EQ_BYTE(b, 0);

    forge_raster_buffer_destroy(&buf);
}

static void test_index_out_of_bounds(void)
{
    TEST("index_out_of_bounds: OOB indices skipped, valid drawn");
    ForgeRasterBuffer buf = forge_raster_buffer_create(16, 16);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    ForgeRasterVertex verts[3] = {
        { 0.0f,  0.0f,  0, 0, 1, 1, 1, 1 },
        { 16.0f, 0.0f,  0, 0, 1, 1, 1, 1 },
        { 8.0f,  16.0f, 0, 0, 1, 1, 1, 1 },
    };

    /* First triangle has OOB index 99, second triangle is valid */
    Uint32 indices[6] = { 0, 1, 99,  0, 1, 2 };
    forge_raster_triangles_indexed(&buf, verts, 3, indices, 6, NULL);

    /* The valid triangle (0,1,2) should still be drawn */
    Uint8 r, g, b, a;
    get_pixel(&buf, 8, 8, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 255);
    ASSERT_EQ_BYTE(g, 255);
    ASSERT_EQ_BYTE(b, 255);

    forge_raster_buffer_destroy(&buf);
}

static void test_negative_index_count(void)
{
    TEST("negative_index_count: negative count handled gracefully");
    ForgeRasterBuffer buf = forge_raster_buffer_create(8, 8);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    ForgeRasterVertex verts[3] = {
        { 0.0f, 0.0f, 0, 0, 1, 1, 1, 1 },
        { 8.0f, 0.0f, 0, 0, 1, 1, 1, 1 },
        { 4.0f, 8.0f, 0, 0, 1, 1, 1, 1 },
    };
    Uint32 indices[3] = { 0, 1, 2 };

    /* Negative index_count should draw nothing */
    forge_raster_triangles_indexed(&buf, verts, 3, indices, -5, NULL);

    Uint8 r, g, b, a;
    get_pixel(&buf, 4, 4, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 0);

    forge_raster_buffer_destroy(&buf);
}

static void test_zero_size_texture(void)
{
    TEST("zero_size_texture: zero-dimension texture skipped");
    ForgeRasterBuffer buf = forge_raster_buffer_create(8, 8);
    ASSERT_TRUE(buf.pixels != NULL);
    forge_raster_clear(&buf, 0.0f, 0.0f, 0.0f, 1.0f);

    /* Texture with zero width -- sampling should be skipped entirely,
     * falling back to vertex colors only */
    Uint8 tex_pixel = 128;
    ForgeRasterTexture tex = { &tex_pixel, 0, 1 };

    ForgeRasterVertex v0 = { 0.0f, 0.0f, 0, 0, 1, 1, 1, 1 };
    ForgeRasterVertex v1 = { 8.0f, 0.0f, 0, 0, 1, 1, 1, 1 };
    ForgeRasterVertex v2 = { 4.0f, 8.0f, 0, 0, 1, 1, 1, 1 };
    forge_raster_triangle(&buf, &v0, &v1, &v2, &tex);

    /* Center should be white (vertex color), not dimmed by texture */
    Uint8 r, g, b, a;
    get_pixel(&buf, 4, 3, &r, &g, &b, &a);
    ASSERT_EQ_BYTE(r, 255);
    ASSERT_EQ_BYTE(g, 255);
    ASSERT_EQ_BYTE(b, 255);

    forge_raster_buffer_destroy(&buf);
}

static void test_null_buffer_ops(void)
{
    TEST("null_buffer_ops: NULL arguments do not crash");

    /* All of these should return silently without crashing */
    forge_raster_clear(NULL, 1, 1, 1, 1);
    forge_raster_buffer_destroy(NULL);

    ForgeRasterVertex v = { 4.0f, 4.0f, 0, 0, 1, 1, 1, 1 };
    forge_raster_triangle(NULL, &v, &v, &v, NULL);

    Uint32 idx[3] = { 0, 0, 0 };
    forge_raster_triangles_indexed(NULL, &v, 1, idx, 3, NULL);

    bool ok = forge_raster_write_bmp(NULL, "test.bmp");
    ASSERT_TRUE(!ok);

    /* Also test NULL pixels buffer (created with bad dims) */
    ForgeRasterBuffer bad = forge_raster_buffer_create(0, 0);
    forge_raster_clear(&bad, 1, 1, 1, 1);
    forge_raster_triangle(&bad, &v, &v, &v, NULL);
    ok = forge_raster_write_bmp(&bad, "test.bmp");
    ASSERT_TRUE(!ok);
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

    SDL_Log("=== Raster Library Tests ===");
    SDL_Log("");

    SDL_Log("-- Buffer operations --");
    test_buffer_create();
    test_buffer_clear();

    SDL_Log("-- Triangle rasterization --");
    test_solid_triangle();
    test_color_interpolation();
    test_degenerate_triangle();
    test_cw_winding();
    test_off_screen_triangle();

    SDL_Log("-- Indexed drawing --");
    test_indexed_drawing();

    SDL_Log("-- Texture sampling --");
    test_texture_sampling();

    SDL_Log("-- Alpha blending --");
    test_alpha_blending();

    SDL_Log("-- BMP writing --");
    test_bmp_write();

    SDL_Log("-- Vertex layout --");
    test_vertex_layout_size();

    SDL_Log("-- Safety & validation --");
    test_buffer_create_max_dim();
    test_buffer_create_zeroed_on_failure();
    test_null_vertex_pointers();
    test_nan_inf_vertex_coords();
    test_near_degenerate_triangle();
    test_index_out_of_bounds();
    test_negative_index_count();
    test_zero_size_texture();
    test_null_buffer_ops();

    SDL_Log("");
    SDL_Log("=== Results: %d tests, %d assertions passed, %d failed ===",
            test_count, pass_count, fail_count);

    SDL_Quit();
    return fail_count > 0 ? 1 : 0;
}
