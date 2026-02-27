/*
 * Engine Lesson 10 -- CPU Rasterization
 *
 * Demonstrates: the edge function triangle rasterization algorithm,
 * barycentric coordinate interpolation, texture sampling, alpha blending,
 * and indexed drawing -- the same operations your GPU performs in hardware.
 *
 * This program produces six BMP images showing each concept progressively:
 *   1. solid_triangle.bmp   -- edge-function rasterization
 *   2. color_triangle.bmp   -- barycentric color interpolation
 *   3. indexed_quad.bmp     -- indexed drawing (4 vertices, 6 indices)
 *   4. textured_quad.bmp    -- UV interpolation + texture sampling
 *   5. alpha_blend.bmp      -- source-over alpha compositing
 *   6. scene.bmp            -- combined scene with all techniques
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "raster/forge_raster.h"

/* ── Canvas Dimensions ───────────────────────────────────────────────────── */

/* Power-of-two size large enough to show rasterization detail in the demos
 * without producing excessively large BMP files (~1 MB each at 512x512). */
#define CANVAS_W 512
#define CANVAS_H 512

/* ── Checkerboard Texture ────────────────────────────────────────────────── */

#define CHECKER_SIZE  8    /* small enough to see individual squares clearly */
#define CHECKER_LIGHT 220  /* off-white so the pattern is visible against a
                            * white vertex color without being too bright */
#define CHECKER_DARK  40   /* dark gray rather than black for a softer look */

/* Background clear colors -- used by every demo for a consistent dark canvas */
#define BG_R 0.08f
#define BG_G 0.08f
#define BG_B 0.10f

/* Scene demo uses a slightly darker background for contrast */
#define SCENE_BG_R 0.06f
#define SCENE_BG_G 0.06f
#define SCENE_BG_B 0.09f

/* Generate an 8x8 checkerboard grayscale texture.
 * Each 1x1 texel alternates between light and dark, producing a
 * checkerboard pattern when sampled with UV coordinates. */
static void make_checkerboard(Uint8 *pixels, int size)
{
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            /* Alternate: even+even=light, even+odd=dark, etc. */
            bool dark = ((x + y) % 2) != 0;
            pixels[y * size + x] = dark ? CHECKER_DARK : CHECKER_LIGHT;
        }
    }
}

/* ── Helper: draw a colored quad via indexed triangles ────────────────────── */

/* Build a quad from (x0,y0) to (x1,y1) with the given color and alpha.
 * UVs span (u0,v0) to (u1,v1) for texture mapping.
 * Writes 4 vertices starting at verts[vert_base] and 6 indices starting
 * at indices[idx_base]. */
static void make_quad(ForgeRasterVertex *verts, int vert_base,
                      Uint32 *indices, int idx_base,
                      float x0, float y0, float x1, float y1,
                      float u0, float v0, float u1, float v1,
                      float r, float g, float b, float a)
{
    /*   0 --- 1       Triangle 0: (0, 1, 2) -- CCW
     *   |   / |       Triangle 1: (0, 2, 3) -- CCW
     *   | /   |
     *   3 --- 2  */
    ForgeRasterVertex *v = &verts[vert_base];
    v[0] = (ForgeRasterVertex){ x0, y0, u0, v0, r, g, b, a };
    v[1] = (ForgeRasterVertex){ x1, y0, u1, v0, r, g, b, a };
    v[2] = (ForgeRasterVertex){ x1, y1, u1, v1, r, g, b, a };
    v[3] = (ForgeRasterVertex){ x0, y1, u0, v1, r, g, b, a };

    Uint32 base = (Uint32)vert_base;
    indices[idx_base + 0] = base + 0;
    indices[idx_base + 1] = base + 1;
    indices[idx_base + 2] = base + 2;
    indices[idx_base + 3] = base + 0;
    indices[idx_base + 4] = base + 2;
    indices[idx_base + 5] = base + 3;
}

/* ── Demo 1: Solid Triangle ──────────────────────────────────────────────── */

/* Triangle vertex positions (screen-space pixels) */
#define SOLID_TRI_TOP_X    256.0f  /* top vertex, centered horizontally */
#define SOLID_TRI_TOP_Y     60.0f
#define SOLID_TRI_LEFT_X    80.0f  /* bottom-left vertex */
#define SOLID_TRI_LEFT_Y   440.0f
#define SOLID_TRI_RIGHT_X  432.0f  /* bottom-right vertex */
#define SOLID_TRI_RIGHT_Y  440.0f

/* Teal color for all three vertices (uniform -- no interpolation) */
#define SOLID_TRI_R 0.20f
#define SOLID_TRI_G 0.80f
#define SOLID_TRI_B 0.75f

static void demo_solid_triangle(void)
{
    ForgeRasterBuffer buf = forge_raster_buffer_create(CANVAS_W, CANVAS_H);
    if (!buf.pixels) return;
    forge_raster_clear(&buf, BG_R, BG_G, BG_B, 1.0f);

    /* A single teal triangle with uniform color on all three vertices.
     * Using the same color everywhere isolates the rasterizer's core job --
     * the edge function inside/outside test -- without any interpolation. */
    ForgeRasterVertex v0 = { SOLID_TRI_TOP_X,   SOLID_TRI_TOP_Y,
                             0, 0,  SOLID_TRI_R, SOLID_TRI_G, SOLID_TRI_B, 1.0f };
    ForgeRasterVertex v1 = { SOLID_TRI_LEFT_X,  SOLID_TRI_LEFT_Y,
                             0, 0,  SOLID_TRI_R, SOLID_TRI_G, SOLID_TRI_B, 1.0f };
    ForgeRasterVertex v2 = { SOLID_TRI_RIGHT_X, SOLID_TRI_RIGHT_Y,
                             0, 0,  SOLID_TRI_R, SOLID_TRI_G, SOLID_TRI_B, 1.0f };
    forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

    if (forge_raster_write_bmp(&buf, "solid_triangle.bmp"))
        SDL_Log("Wrote solid_triangle.bmp");
    forge_raster_buffer_destroy(&buf);
}

/* ── Demo 2: Color Triangle ──────────────────────────────────────────────── */

static void demo_color_triangle(void)
{
    ForgeRasterBuffer buf = forge_raster_buffer_create(CANVAS_W, CANVAS_H);
    if (!buf.pixels) return;
    forge_raster_clear(&buf, BG_R, BG_G, BG_B, 1.0f);

    /* Classic RGB triangle: each vertex a different primary color.
     * Barycentric interpolation blends them smoothly across the surface --
     * the software equivalent of GPU Lesson 02's first triangle. */
    ForgeRasterVertex v0 = { 256.0f,  50.0f,  0, 0,  1.0f, 0.0f, 0.0f, 1.0f };
    ForgeRasterVertex v1 = { 60.0f,  450.0f,  0, 0,  0.0f, 1.0f, 0.0f, 1.0f };
    ForgeRasterVertex v2 = { 452.0f, 450.0f,  0, 0,  0.0f, 0.0f, 1.0f, 1.0f };
    forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);

    if (forge_raster_write_bmp(&buf, "color_triangle.bmp"))
        SDL_Log("Wrote color_triangle.bmp");
    forge_raster_buffer_destroy(&buf);
}

/* ── Demo 3: Indexed Quad ────────────────────────────────────────────────── */

static void demo_indexed_quad(void)
{
    ForgeRasterBuffer buf = forge_raster_buffer_create(CANVAS_W, CANVAS_H);
    if (!buf.pixels) return;
    forge_raster_clear(&buf, BG_R, BG_G, BG_B, 1.0f);

    /* A quad drawn from 4 vertices and 6 indices -- the fundamental
     * UI primitive.  Each vertex has a different color to show that
     * interpolation works across the two-triangle quad seam. */
    ForgeRasterVertex verts[4] = {
        { 100.0f, 100.0f, 0, 0,  1.0f, 0.3f, 0.2f, 1.0f }, /* TL: warm red */
        { 412.0f, 100.0f, 0, 0,  1.0f, 0.8f, 0.1f, 1.0f }, /* TR: amber */
        { 412.0f, 412.0f, 0, 0,  0.2f, 0.4f, 1.0f, 1.0f }, /* BR: blue */
        { 100.0f, 412.0f, 0, 0,  0.6f, 0.1f, 0.9f, 1.0f }, /* BL: purple */
    };
    /* Two CCW triangles forming the quad */
    Uint32 indices[6] = { 0, 1, 2,  0, 2, 3 };

    forge_raster_triangles_indexed(&buf, verts, 4, indices, 6, NULL);

    if (forge_raster_write_bmp(&buf, "indexed_quad.bmp"))
        SDL_Log("Wrote indexed_quad.bmp");
    forge_raster_buffer_destroy(&buf);
}

/* ── Demo 4: Textured Quad ───────────────────────────────────────────────── */

static void demo_textured_quad(void)
{
    ForgeRasterBuffer buf = forge_raster_buffer_create(CANVAS_W, CANVAS_H);
    if (!buf.pixels) return;
    forge_raster_clear(&buf, BG_R, BG_G, BG_B, 1.0f);

    /* Generate a small checkerboard texture -- its repeating pattern makes
     * it easy to verify that UV interpolation and sampling are correct. */
    Uint8 tex_pixels[CHECKER_SIZE * CHECKER_SIZE];
    make_checkerboard(tex_pixels, CHECKER_SIZE);
    ForgeRasterTexture tex = { tex_pixels, CHECKER_SIZE, CHECKER_SIZE };

    /* A white quad with UVs spanning the full texture.  The vertex color
     * is white so the texture value shows through unmodified -- the texel
     * multiplies with the vertex color (all 1.0). */
    ForgeRasterVertex verts[4] = {
        {  80.0f,  80.0f, 0.0f, 0.0f,  1, 1, 1, 1 },  /* TL */
        { 432.0f,  80.0f, 1.0f, 0.0f,  1, 1, 1, 1 },  /* TR */
        { 432.0f, 432.0f, 1.0f, 1.0f,  1, 1, 1, 1 },  /* BR */
        {  80.0f, 432.0f, 0.0f, 1.0f,  1, 1, 1, 1 },  /* BL */
    };
    Uint32 indices[6] = { 0, 1, 2,  0, 2, 3 };

    forge_raster_triangles_indexed(&buf, verts, 4, indices, 6, &tex);

    if (forge_raster_write_bmp(&buf, "textured_quad.bmp"))
        SDL_Log("Wrote textured_quad.bmp");
    forge_raster_buffer_destroy(&buf);
}

/* ── Demo 5: Alpha Blending ──────────────────────────────────────────────── */

static void demo_alpha_blend(void)
{
    ForgeRasterBuffer buf = forge_raster_buffer_create(CANVAS_W, CANVAS_H);
    if (!buf.pixels) return;
    forge_raster_clear(&buf, BG_R, BG_G, BG_B, 1.0f);

    /* Three overlapping semi-transparent quads demonstrate source-over
     * compositing.  Where two colors overlap, the source-over formula
     * produces the blended result:
     *   out = src * src_a + dst * (1 - src_a) */
    ForgeRasterVertex verts[12];
    Uint32 indices[18];

    /* Red quad (back, drawn first).  Draw order matters for source-over
     * compositing: later draws blend on top of earlier ones (painter's
     * algorithm), so back-to-front ordering produces correct results. */
    make_quad(verts, 0, indices, 0,
              80.0f, 120.0f, 300.0f, 380.0f,
              0, 0, 0, 0,
              0.95f, 0.20f, 0.20f, 0.65f);

    /* Green quad (middle) */
    make_quad(verts, 4, indices, 6,
              180.0f, 80.0f, 400.0f, 340.0f,
              0, 0, 0, 0,
              0.20f, 0.90f, 0.30f, 0.65f);

    /* Blue quad (front, drawn last) */
    make_quad(verts, 8, indices, 12,
              140.0f, 220.0f, 360.0f, 440.0f,
              0, 0, 0, 0,
              0.25f, 0.35f, 0.95f, 0.65f);

    forge_raster_triangles_indexed(&buf, verts, 12, indices, 18, NULL);

    if (forge_raster_write_bmp(&buf, "alpha_blend.bmp"))
        SDL_Log("Wrote alpha_blend.bmp");
    forge_raster_buffer_destroy(&buf);
}

/* ── Demo 6: Composed Scene ──────────────────────────────────────────────── */

static void demo_scene(void)
{
    ForgeRasterBuffer buf = forge_raster_buffer_create(CANVAS_W, CANVAS_H);
    if (!buf.pixels) return;
    forge_raster_clear(&buf, SCENE_BG_R, SCENE_BG_G, SCENE_BG_B, 1.0f);

    /* Generate checkerboard texture for the textured region */
    Uint8 tex_pixels[CHECKER_SIZE * CHECKER_SIZE];
    make_checkerboard(tex_pixels, CHECKER_SIZE);
    ForgeRasterTexture tex = { tex_pixels, CHECKER_SIZE, CHECKER_SIZE };

    /* ── Background: textured region ─────────────────────────────────── */
    /* A subtle checkered area in the lower portion */
    {
        ForgeRasterVertex verts[4];
        Uint32 indices[6];
        make_quad(verts, 0, indices, 0,
                  20.0f, 280.0f, 492.0f, 492.0f,
                  0.0f, 0.0f, 1.0f, 1.0f,
                  0.30f, 0.30f, 0.35f, 1.0f);
        forge_raster_triangles_indexed(&buf, verts, 4, indices, 6, &tex);
    }

    /* ── Solid colored triangles ─────────────────────────────────────── */
    /* A warm-colored triangle on the left */
    {
        ForgeRasterVertex v0 = { 60.0f,  40.0f,  0, 0,  1.0f, 0.55f, 0.10f, 1.0f };
        ForgeRasterVertex v1 = { 20.0f,  260.0f, 0, 0,  0.90f, 0.25f, 0.10f, 1.0f };
        ForgeRasterVertex v2 = { 200.0f, 200.0f, 0, 0,  1.0f, 0.80f, 0.20f, 1.0f };
        forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);
    }

    /* A cool-colored triangle on the right */
    {
        ForgeRasterVertex v0 = { 450.0f, 30.0f,  0, 0,  0.15f, 0.45f, 0.95f, 1.0f };
        ForgeRasterVertex v1 = { 300.0f, 220.0f, 0, 0,  0.30f, 0.70f, 0.90f, 1.0f };
        ForgeRasterVertex v2 = { 495.0f, 250.0f, 0, 0,  0.10f, 0.30f, 0.80f, 1.0f };
        forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);
    }

    /* An RGB interpolated triangle in the center */
    {
        ForgeRasterVertex v0 = { 256.0f, 100.0f, 0, 0,  1.0f, 0.2f, 0.2f, 1.0f };
        ForgeRasterVertex v1 = { 170.0f, 270.0f, 0, 0,  0.2f, 1.0f, 0.3f, 1.0f };
        ForgeRasterVertex v2 = { 340.0f, 270.0f, 0, 0,  0.2f, 0.3f, 1.0f, 1.0f };
        forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);
    }

    /* ── Translucent panels (UI preview) ─────────────────────────────── */
    /* These simulate the kind of translucent panels a UI system renders:
     * dark panels with colored borders, overlapping to show blending. */
    {
        ForgeRasterVertex verts[8];
        Uint32 indices[12];

        /* Panel 1: dark translucent panel with slight blue tint */
        make_quad(verts, 0, indices, 0,
                  40.0f, 320.0f, 250.0f, 470.0f,
                  0, 0, 0, 0,
                  0.10f, 0.12f, 0.20f, 0.80f);

        /* Panel 2: overlapping panel with slight warm tint */
        make_quad(verts, 4, indices, 6,
                  180.0f, 350.0f, 470.0f, 490.0f,
                  0, 0, 0, 0,
                  0.22f, 0.15f, 0.12f, 0.75f);

        forge_raster_triangles_indexed(&buf, verts, 8, indices, 12, NULL);
    }

    /* ── Accent: small bright quad ───────────────────────────────────── */
    {
        ForgeRasterVertex verts[4];
        Uint32 indices[6];
        make_quad(verts, 0, indices, 0,
                  60.0f, 340.0f, 130.0f, 370.0f,
                  0, 0, 0, 0,
                  0.95f, 0.65f, 0.15f, 0.90f);
        forge_raster_triangles_indexed(&buf, verts, 4, indices, 6, NULL);
    }

    if (forge_raster_write_bmp(&buf, "scene.bmp"))
        SDL_Log("Wrote scene.bmp");
    forge_raster_buffer_destroy(&buf);
}

/* ── Entry Point ─────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("Engine Lesson 10 -- CPU Rasterization");
    SDL_Log("Generating BMP images...");
    SDL_Log("");

    demo_solid_triangle();
    demo_color_triangle();
    demo_indexed_quad();
    demo_textured_quad();
    demo_alpha_blend();
    demo_scene();

    SDL_Log("");
    SDL_Log("Done. Open the BMP files to see the results.");

    SDL_Quit();
    return 0;
}
