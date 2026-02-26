/*
 * UI Lesson 03 -- Font Atlas Packing
 *
 * Demonstrates: Building a font atlas from rasterized glyphs using shelf
 * (row-based) rectangle packing, computing UV coordinates per glyph, and
 * producing a single-channel texture with per-glyph metadata.
 *
 * This program:
 *   1. Loads a TrueType font (Liberation Mono) via forge_ui_ttf_load
 *   2. Builds an atlas of printable ASCII (codepoints 32-126) at 32px height
 *   3. Writes atlas.bmp -- the full atlas as a grayscale BMP
 *   4. Writes atlas_debug.bmp -- atlas with glyph rectangles outlined
 *   5. Writes glyph_A_from_atlas.bmp -- glyph 'A' extracted via UV coords
 *   6. Prints atlas dimensions, packing stats, and per-glyph metadata
 *
 * This is a console program -- no GPU or window is needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "ui/forge_ui.h"

/* ── Default font path ──────────────────────────────────────────────────── */
#define DEFAULT_FONT_PATH "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"

/* ── Section separators for console output ───────────────────────────────── */
#define SEPARATOR "============================================================"
#define THIN_SEP  "------------------------------------------------------------"

/* ── Atlas parameters ────────────────────────────────────────────────────── */
#define PIXEL_HEIGHT     32.0f  /* render glyphs at 32 pixels tall */
#define ATLAS_PADDING    1      /* 1 pixel padding between glyphs */
#define ASCII_START      32     /* first printable ASCII codepoint (space) */
#define ASCII_END        126    /* last printable ASCII codepoint (tilde) */
#define ASCII_COUNT      (ASCII_END - ASCII_START + 1)  /* 95 glyphs */

/* ── Helper: draw a debug outline around a glyph in the atlas ────────────── */
/* Draws a 1px mid-gray border around the given rectangle in the atlas.
 * Used for the debug BMP that visualizes shelf rows and glyph placement. */

/* Outline brightness — use a mid-gray value so the outline contrasts against
 * both empty (black) regions and bright glyph pixels. */
#define OUTLINE_VALUE    160

static void draw_glyph_outline(Uint8 *pixels, int atlas_w, int atlas_h,
                                 int x, int y, int w, int h)
{
    /* Top and bottom edges */
    for (int dx = 0; dx < w && x + dx < atlas_w; dx++) {
        if (y > 0)
            pixels[(y - 1) * atlas_w + (x + dx)] = OUTLINE_VALUE;
        if (y + h < atlas_h)
            pixels[(y + h) * atlas_w + (x + dx)] = OUTLINE_VALUE;
    }
    /* Left and right edges */
    for (int dy = 0; dy < h && y + dy < atlas_h; dy++) {
        if (x > 0)
            pixels[(y + dy) * atlas_w + (x - 1)] = OUTLINE_VALUE;
        if (x + w < atlas_w)
            pixels[(y + dy) * atlas_w + (x + w)] = OUTLINE_VALUE;
    }
}

/* ── Helper: extract a glyph region from the atlas using UV coordinates ─── */
/* Proves that UV coordinates round-trip correctly by extracting a glyph's
 * pixels from the atlas using its UV rect and writing to a separate BMP. */

static bool extract_glyph_from_atlas(const ForgeUiFontAtlas *atlas,
                                      const ForgeUiPackedGlyph *glyph,
                                      const char *bmp_path)
{
    if (glyph->bitmap_w == 0 || glyph->bitmap_h == 0) {
        SDL_Log("  (whitespace glyph -- no bitmap to extract)");
        return true;
    }

    /* Convert UV coordinates back to pixel coordinates */
    int px = (int)(glyph->uv.u0 * (float)atlas->width + 0.5f);
    int py = (int)(glyph->uv.v0 * (float)atlas->height + 0.5f);
    int pw = glyph->bitmap_w;
    int ph = glyph->bitmap_h;

    /* Allocate and copy the glyph region */
    Uint8 *region = (Uint8 *)SDL_calloc((size_t)pw * (size_t)ph, 1);
    if (!region) {
        SDL_Log("extract_glyph_from_atlas: allocation failed");
        return false;
    }

    for (int row = 0; row < ph; row++) {
        int src_y = py + row;
        if (src_y >= 0 && src_y < atlas->height) {
            int copy_w = pw;
            if (px + copy_w > atlas->width) {
                copy_w = atlas->width - px;
            }
            if (copy_w > 0 && px >= 0) {
                SDL_memcpy(&region[row * pw],
                           &atlas->pixels[src_y * atlas->width + px],
                           (size_t)copy_w);
            }
        }
    }

    bool ok = forge_ui__write_grayscale_bmp(bmp_path, region, pw, ph);
    SDL_free(region);
    return ok;
}

/* ── Helper: print metadata for one glyph ────────────────────────────────── */

static void print_glyph_info(const ForgeUiFontAtlas *atlas,
                               Uint32 codepoint,
                               const char *label)
{
    const ForgeUiPackedGlyph *g = forge_ui_atlas_lookup(atlas, codepoint);
    if (!g) {
        SDL_Log("  '%s': not found in atlas", label);
        return;
    }

    SDL_Log("  '%s' (U+%04X, glyph %u):", label, g->codepoint, g->glyph_index);
    SDL_Log("    UV rect:       (%.6f, %.6f) to (%.6f, %.6f)",
            (double)g->uv.u0, (double)g->uv.v0,
            (double)g->uv.u1, (double)g->uv.v1);
    SDL_Log("    bitmap size:   %d x %d pixels", g->bitmap_w, g->bitmap_h);
    SDL_Log("    bearing:       (%d, %d)", g->bearing_x, g->bearing_y);
    SDL_Log("    advance width: %u (font units)", g->advance_width);
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    const char *font_path = (argc > 1) ? argv[1] : DEFAULT_FONT_PATH;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("UI Lesson 03 -- Font Atlas Packing");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("Loading font: %s", font_path);

    /* ── Load font ──────────────────────────────────────────────────── */
    ForgeUiFont font;
    if (!forge_ui_ttf_load(font_path, &font)) {
        SDL_Log("Failed to load font -- see errors above");
        SDL_Quit();
        return 1;
    }

    SDL_Log("  unitsPerEm:         %u", font.head.units_per_em);
    SDL_Log("  ascender:           %d", font.hhea.ascender);
    SDL_Log("  descender:          %d", font.hhea.descender);
    SDL_Log("  numberOfHMetrics:   %u", font.hhea.number_of_h_metrics);
    SDL_Log("  numGlyphs:          %u", font.maxp.num_glyphs);

    /* ── Build codepoint array for printable ASCII ──────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("BUILDING ATLAS");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  character set:   printable ASCII (U+0020 to U+007E)");
    SDL_Log("  codepoint count: %d", ASCII_COUNT);
    SDL_Log("  pixel height:    %.0f px", (double)PIXEL_HEIGHT);
    SDL_Log("  padding:         %d px", ATLAS_PADDING);

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    /* ── Build the atlas ────────────────────────────────────────────── */
    ForgeUiFontAtlas atlas;
    if (!forge_ui_atlas_build(&font, PIXEL_HEIGHT, codepoints, ASCII_COUNT,
                               ATLAS_PADDING, &atlas)) {
        SDL_Log("Failed to build font atlas -- see errors above");
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    /* ── Atlas metrics ──────────────────────────────────────────────── */
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  atlas dimensions:  %d x %d pixels", atlas.width, atlas.height);

    /* Calculate utilization */
    int total_glyph_area = 0;
    for (int i = 0; i < atlas.glyph_count; i++) {
        total_glyph_area += atlas.glyphs[i].bitmap_w * atlas.glyphs[i].bitmap_h;
    }
    int atlas_area = atlas.width * atlas.height;
    float utilization = (float)total_glyph_area / (float)atlas_area * 100.0f;

    SDL_Log("  glyphs packed:     %d", atlas.glyph_count);
    SDL_Log("  total glyph area:  %d pixels", total_glyph_area);
    SDL_Log("  atlas area:        %d pixels (%d x %d)",
            atlas_area, atlas.width, atlas.height);
    SDL_Log("  utilization:       %.1f%%", (double)utilization);
    SDL_Log("  atlas memory:      %d bytes (single-channel)", atlas_area);

    /* ── Write atlas.bmp ────────────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("WRITING OUTPUT FILES");
    SDL_Log("%s", THIN_SEP);

    if (forge_ui__write_grayscale_bmp("atlas.bmp", atlas.pixels,
                                        atlas.width, atlas.height)) {
        SDL_Log("  atlas.bmp:             %d x %d -- full atlas",
                atlas.width, atlas.height);
    } else {
        SDL_Log("  [!] Failed to write atlas.bmp");
    }

    /* ── Write atlas_debug.bmp (with glyph outlines) ───────────────── */
    /* Copy atlas pixels and draw debug outlines around each glyph rect */
    size_t atlas_size = (size_t)atlas.width * (size_t)atlas.height;
    Uint8 *debug_pixels = (Uint8 *)SDL_malloc(atlas_size);
    if (debug_pixels) {
        SDL_memcpy(debug_pixels, atlas.pixels, atlas_size);

        for (int i = 0; i < atlas.glyph_count; i++) {
            const ForgeUiPackedGlyph *g = &atlas.glyphs[i];
            if (g->bitmap_w > 0 && g->bitmap_h > 0) {
                /* Recover pixel position from UV coordinates */
                int px = (int)(g->uv.u0 * (float)atlas.width + 0.5f);
                int py = (int)(g->uv.v0 * (float)atlas.height + 0.5f);
                draw_glyph_outline(debug_pixels, atlas.width, atlas.height,
                                    px, py, g->bitmap_w, g->bitmap_h);
            }
        }

        if (forge_ui__write_grayscale_bmp("atlas_debug.bmp", debug_pixels,
                                            atlas.width, atlas.height)) {
            SDL_Log("  atlas_debug.bmp:       %d x %d -- with glyph outlines",
                    atlas.width, atlas.height);
        } else {
            SDL_Log("  [!] Failed to write atlas_debug.bmp");
        }

        SDL_free(debug_pixels);
    }

    /* ── Write glyph_A_from_atlas.bmp (UV round-trip test) ─────────── */
    const ForgeUiPackedGlyph *glyph_a = forge_ui_atlas_lookup(&atlas, 'A');
    if (glyph_a) {
        if (extract_glyph_from_atlas(&atlas, glyph_a, "glyph_A_from_atlas.bmp")) {
            SDL_Log("  glyph_A_from_atlas.bmp: %d x %d -- extracted via UVs",
                    glyph_a->bitmap_w, glyph_a->bitmap_h);
        } else {
            SDL_Log("  [!] Failed to write glyph_A_from_atlas.bmp");
        }
    }

    /* ── Per-glyph metadata for selected glyphs ────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("PER-GLYPH METADATA");
    SDL_Log("%s", THIN_SEP);

    print_glyph_info(&atlas, 'A', "A");
    SDL_Log("");
    print_glyph_info(&atlas, 'g', "g");
    SDL_Log("");
    print_glyph_info(&atlas, ' ', " ");

    /* ── White pixel region ─────────────────────────────────────────── */
    SDL_Log("%s", THIN_SEP);
    SDL_Log("WHITE PIXEL REGION");
    SDL_Log("  UV rect: (%.6f, %.6f) to (%.6f, %.6f)",
            (double)atlas.white_uv.u0, (double)atlas.white_uv.v0,
            (double)atlas.white_uv.u1, (double)atlas.white_uv.v1);
    SDL_Log("  Use this UV rect for solid-colored geometry (lines,");
    SDL_Log("  rectangles, backgrounds) to avoid texture switching.");

    /* ── Pipeline summary ───────────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("PIPELINE SUMMARY");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  1. Load font:       forge_ui_ttf_load()");
    SDL_Log("  2. Build atlas:     forge_ui_atlas_build()");
    SDL_Log("     - Rasterize each glyph (forge_ui_rasterize_glyph)");
    SDL_Log("     - Sort by height (tallest first)");
    SDL_Log("     - Shelf-pack into power-of-two texture");
    SDL_Log("     - Compute UV coordinates per glyph");
    SDL_Log("     - Record per-glyph metadata (UVs, bearings, advance)");
    SDL_Log("  3. Use atlas:       atlas.pixels (upload to GPU)");
    SDL_Log("                      atlas.glyphs (per-glyph UV + metrics)");
    SDL_Log("  4. Look up glyphs:  forge_ui_atlas_lookup(atlas, codepoint)");
    SDL_Log("  5. Free atlas:      forge_ui_atlas_free()");
    SDL_Log("");
    SDL_Log("The text layout lesson (UI 04) will use the glyph table to");
    SDL_Log("build positioned quads.  A GPU lesson will upload the atlas");
    SDL_Log("as a single-channel texture and render text by sampling it.");

    SDL_Log("%s", SEPARATOR);
    SDL_Log("Done. Output files written to the current directory.");

    /* ── Cleanup ────────────────────────────────────────────────────── */
    forge_ui_atlas_free(&atlas);
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return 0;
}
