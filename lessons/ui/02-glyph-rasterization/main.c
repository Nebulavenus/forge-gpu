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
/* Write a single-channel grayscale bitmap as a BMP file.
 *
 * BMP format stores pixels bottom-up (row 0 = bottom of image) with each
 * row padded to a 4-byte boundary.  We write an 8-bit indexed BMP with a
 * 256-entry grayscale palette (0=black, 255=white).
 *
 * This is the simplest BMP format that can represent our alpha coverage
 * values as visible grayscale pixels. */

#define BMP_HEADER_SIZE   14     /* BITMAPFILEHEADER size */
#define BMP_INFO_SIZE     40     /* BITMAPINFOHEADER size */
#define BMP_PALETTE_SIZE  1024   /* 256 entries * 4 bytes (BGRA) */

static bool write_grayscale_bmp(const char *path,
                                 const Uint8 *pixels,
                                 int width, int height)
{
    /* Each row must be padded to a 4-byte boundary */
    int row_stride = (width + 3) & ~3;
    Uint32 pixel_data_size = (Uint32)(row_stride * height);
    Uint32 file_size = BMP_HEADER_SIZE + BMP_INFO_SIZE +
                       BMP_PALETTE_SIZE + pixel_data_size;

    Uint8 *buf = (Uint8 *)SDL_calloc(file_size, 1);
    if (!buf) {
        SDL_Log("write_grayscale_bmp: allocation failed");
        return false;
    }

    /* BITMAPFILEHEADER (14 bytes) */
    buf[0] = 'B'; buf[1] = 'M';                /* signature */
    buf[2] = (Uint8)(file_size);                /* file size (little-endian) */
    buf[3] = (Uint8)(file_size >> 8);
    buf[4] = (Uint8)(file_size >> 16);
    buf[5] = (Uint8)(file_size >> 24);
    /* bytes 6-9: reserved (0) */
    Uint32 data_offset = BMP_HEADER_SIZE + BMP_INFO_SIZE + BMP_PALETTE_SIZE;
    buf[10] = (Uint8)(data_offset);
    buf[11] = (Uint8)(data_offset >> 8);
    buf[12] = (Uint8)(data_offset >> 16);
    buf[13] = (Uint8)(data_offset >> 24);

    /* BITMAPINFOHEADER (40 bytes) */
    Uint8 *info = buf + BMP_HEADER_SIZE;
    info[0] = BMP_INFO_SIZE;                     /* header size */
    info[4] = (Uint8)(width);                    /* width (little-endian) */
    info[5] = (Uint8)(width >> 8);
    info[6] = (Uint8)(width >> 16);
    info[7] = (Uint8)(width >> 24);
    info[8]  = (Uint8)(height);                  /* height (little-endian) */
    info[9]  = (Uint8)(height >> 8);
    info[10] = (Uint8)(height >> 16);
    info[11] = (Uint8)(height >> 24);
    info[12] = 1;                                /* planes = 1 */
    info[14] = 8;                                /* bits per pixel = 8 */
    /* bytes 16-19: compression = 0 (BI_RGB) */
    info[20] = (Uint8)(pixel_data_size);         /* image data size */
    info[21] = (Uint8)(pixel_data_size >> 8);
    info[22] = (Uint8)(pixel_data_size >> 16);
    info[23] = (Uint8)(pixel_data_size >> 24);

    /* Grayscale palette: 256 entries, each (B, G, R, 0) */
    Uint8 *palette = buf + BMP_HEADER_SIZE + BMP_INFO_SIZE;
    for (int i = 0; i < 256; i++) {
        palette[i * 4 + 0] = (Uint8)i;   /* blue */
        palette[i * 4 + 1] = (Uint8)i;   /* green */
        palette[i * 4 + 2] = (Uint8)i;   /* red */
        palette[i * 4 + 3] = 0;          /* reserved */
    }

    /* Pixel data: BMP stores rows bottom-up, so row 0 in the file is the
     * bottom of the image.  Our bitmap is stored top-down (row 0 = top),
     * so we need to flip. */
    Uint8 *pixel_dst = buf + data_offset;
    for (int y = 0; y < height; y++) {
        /* BMP row (height - 1 - y) gets our row y */
        int bmp_row = height - 1 - y;
        SDL_memcpy(&pixel_dst[bmp_row * row_stride],
                   &pixels[y * width],
                   (size_t)width);
    }

    /* Write to file using standard C I/O for portability.
     * SDL_SaveFile is available in real SDL3 but not in the minimal shim
     * used for console-only builds. */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        SDL_Log("write_grayscale_bmp: failed to open '%s' for writing", path);
        SDL_free(buf);
        return false;
    }
    size_t written = fwrite(buf, 1, file_size, fp);
    fclose(fp);
    SDL_free(buf);

    if (written != file_size) {
        SDL_Log("write_grayscale_bmp: incomplete write to '%s' "
                "(%zu of %u bytes)", path, written, file_size);
        return false;
    }
    return true;
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
