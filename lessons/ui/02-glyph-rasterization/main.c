/*
 * UI Lesson 02 -- Glyph Rasterization
 *
 * Demonstrates: Converting TrueType glyph outlines (quadratic Bezier curves)
 * into pixel bitmaps using scanline rasterization with the non-zero winding
 * fill rule, and supersampled anti-aliasing.
 *
 * This program:
 *   1. Loads a TrueType font (Liberation Mono) via forge_ui_ttf_load
 *   2. Rasterizes several glyphs at 64px height: 'A', 'O', 'g', 'i'
 *   3. Writes each glyph as a BMP file (grayscale alpha bitmap)
 *   4. Writes 'A' without anti-aliasing and with anti-aliasing for comparison
 *   5. Prints detailed metrics and rasterization statistics
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

/* ── Rasterization parameters ────────────────────────────────────────────── */
#define PIXEL_HEIGHT 64.0f       /* render glyphs at 64 pixels tall */
#define SS_LEVEL     4           /* 4x4 supersampling for anti-aliasing */

/* ── BMP file writing ────────────────────────────────────────────────────── */
/* Uses the shared BMP writer from forge_ui.h (forge_ui__write_grayscale_bmp).
 * This local wrapper preserves the original function name for readability. */

static bool write_grayscale_bmp(const char *path,
                                 const Uint8 *pixels,
                                 int width, int height)
{
    return forge_ui__write_grayscale_bmp(path, pixels, width, height);
}

/* ── Rasterize and report one glyph ─────────────────────────────────────── */
/* Loads glyph contours, rasterizes to bitmap, writes BMP, and prints metrics.
 * Returns true on success. */

static bool rasterize_and_report(const ForgeUiFont *font,
                                  Uint32 codepoint,
                                  const char *label,
                                  const char *bmp_path,
                                  float pixel_height,
                                  int ss_level)
{
    Uint16 glyph_idx = forge_ui_ttf_glyph_index(font, codepoint);

    SDL_Log("  glyph index:   %u", glyph_idx);

    /* Load the glyph outline to print contour statistics */
    ForgeUiTtfGlyph glyph;
    if (!forge_ui_ttf_load_glyph(font, glyph_idx, &glyph)) {
        SDL_Log("  Failed to load glyph outline for '%s'", label);
        return false;
    }

    SDL_Log("  contours:      %u", glyph.contour_count);
    SDL_Log("  points:        %u", glyph.point_count);
    SDL_Log("  bbox (font):   (%d, %d) to (%d, %d)",
            glyph.x_min, glyph.y_min, glyph.x_max, glyph.y_max);

    /* Count edges: on-curve and off-curve points determine edge types */
    int on_curve_count = 0;
    int off_curve_count = 0;
    for (Uint16 i = 0; i < glyph.point_count; i++) {
        if (glyph.flags[i] & FORGE_UI_FLAG_ON_CURVE) {
            on_curve_count++;
        } else {
            off_curve_count++;
        }
    }
    SDL_Log("  on-curve pts:  %d", on_curve_count);
    SDL_Log("  off-curve pts: %d", off_curve_count);

    forge_ui_ttf_glyph_free(&glyph);

    /* Rasterize */
    float scale = pixel_height / (float)font->head.units_per_em;
    SDL_Log("  scale factor:  %.6f (%.0fpx / %u unitsPerEm)",
            scale, pixel_height, font->head.units_per_em);

    ForgeUiRasterOpts opts;
    opts.supersample_level = ss_level;

    ForgeUiGlyphBitmap bitmap;
    if (!forge_ui_rasterize_glyph(font, glyph_idx, pixel_height,
                                   &opts, &bitmap)) {
        SDL_Log("  Failed to rasterize '%s'", label);
        return false;
    }

    if (bitmap.width == 0 || bitmap.height == 0) {
        SDL_Log("  (whitespace glyph -- no bitmap produced)");
        forge_ui_glyph_bitmap_free(&bitmap);
        return true;
    }

    SDL_Log("  bitmap size:   %d x %d pixels", bitmap.width, bitmap.height);
    SDL_Log("  bearing:       (%d, %d)", bitmap.bearing_x, bitmap.bearing_y);
    SDL_Log("  anti-aliasing: %dx%d supersampling (%d samples/pixel)",
            ss_level, ss_level, ss_level * ss_level);

    /* Write BMP */
    if (bmp_path) {
        if (!write_grayscale_bmp(bmp_path, bitmap.pixels,
                                  bitmap.width, bitmap.height)) {
            forge_ui_glyph_bitmap_free(&bitmap);
            return false;
        }
        SDL_Log("  saved:         %s", bmp_path);
    }

    forge_ui_glyph_bitmap_free(&bitmap);
    return true;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    const char *font_path = (argc > 1) ? argv[1] : DEFAULT_FONT_PATH;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("UI Lesson 02 -- Glyph Rasterization");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("Loading font: %s", font_path);

    /* ── Load font ──────────────────────────────────────────────────── */
    ForgeUiFont font;
    if (!forge_ui_ttf_load(font_path, &font)) {
        SDL_Log("Failed to load font -- see errors above");
        SDL_Quit();
        return 1;
    }

    SDL_Log("  unitsPerEm:    %u", font.head.units_per_em);
    SDL_Log("  ascender:      %d", font.hhea.ascender);
    SDL_Log("  descender:     %d", font.hhea.descender);

    /* ── Rasterize test glyphs ──────────────────────────────────────── */
    bool ok = true;

    /* 'A' -- two contours (outer triangle + inner counter/hole) */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("GLYPH: 'A' (two contours: outer shape + triangular hole)");
    SDL_Log("%s", THIN_SEP);
    if (!rasterize_and_report(&font, 'A', "A", "glyph_A.bmp",
                               PIXEL_HEIGHT, SS_LEVEL)) {
        ok = false;
    }

    /* 'O' -- two contours (outer oval + inner hole, demonstrates winding) */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("GLYPH: 'O' (two contours: outer + inner counter)");
    SDL_Log("%s", THIN_SEP);
    if (!rasterize_and_report(&font, 'O', "O", "glyph_O.bmp",
                               PIXEL_HEIGHT, SS_LEVEL)) {
        ok = false;
    }

    /* 'g' -- has a descender (extends below baseline) */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("GLYPH: 'g' (has descender -- extends below baseline)");
    SDL_Log("%s", THIN_SEP);
    if (!rasterize_and_report(&font, 'g', "g", "glyph_g.bmp",
                               PIXEL_HEIGHT, SS_LEVEL)) {
        ok = false;
    }

    /* 'i' -- simple glyph with dot (two separate contours) */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("GLYPH: 'i' (two contours: stem + dot)");
    SDL_Log("%s", THIN_SEP);
    if (!rasterize_and_report(&font, 'i', "i", "glyph_i.bmp",
                               PIXEL_HEIGHT, SS_LEVEL)) {
        ok = false;
    }

    /* ── Anti-aliasing comparison: 'A' with and without ─────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("ANTI-ALIASING COMPARISON: 'A'");
    SDL_Log("%s", THIN_SEP);

    /* No anti-aliasing (1x1 = binary on/off) */
    SDL_Log("Without anti-aliasing (1x1, binary):");
    if (!rasterize_and_report(&font, 'A', "A_noaa", "glyph_A_noaa.bmp",
                               PIXEL_HEIGHT, 1)) {
        ok = false;
    }

    SDL_Log("");

    /* With anti-aliasing (4x4 supersampling) */
    SDL_Log("With anti-aliasing (4x4 supersampling):");
    if (!rasterize_and_report(&font, 'A', "A_aa", "glyph_A_aa.bmp",
                               PIXEL_HEIGHT, SS_LEVEL)) {
        ok = false;
    }

    /* ── Summary ────────────────────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("PIPELINE SUMMARY");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  1. Load font:     forge_ui_ttf_load()");
    SDL_Log("  2. Look up glyph: forge_ui_ttf_glyph_index()");
    SDL_Log("  3. Rasterize:     forge_ui_rasterize_glyph()");
    SDL_Log("  4. Use bitmap:    width, height, pixels (alpha coverage)");
    SDL_Log("  5. Free bitmap:   forge_ui_glyph_bitmap_free()");
    SDL_Log("");
    SDL_Log("Each bitmap is a single-channel alpha coverage map:");
    SDL_Log("  0   = pixel is fully outside the glyph");
    SDL_Log("  255 = pixel is fully inside the glyph");
    SDL_Log("  1-254 = partial coverage (anti-aliased edge)");
    SDL_Log("");
    SDL_Log("The font atlas lesson (UI 03) will pack these bitmaps");
    SDL_Log("into a single texture.  The GPU samples the alpha and");
    SDL_Log("multiplies by a text color -- color is NOT in the bitmap.");

    SDL_Log("%s", SEPARATOR);
    if (ok) {
        SDL_Log("Done. BMP files written to the current directory.");
    } else {
        SDL_Log("Done with errors — see messages above.");
    }

    /* ── Cleanup ────────────────────────────────────────────────────── */
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return ok ? 0 : 1;
}
