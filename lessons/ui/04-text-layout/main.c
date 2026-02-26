/*
 * UI Lesson 04 -- Text Layout
 *
 * Demonstrates: Converting a string of characters into positioned, textured
 * quads using the pen/cursor model, horizontal metrics, baseline positioning,
 * line breaking, text alignment, and generating vertex/index data suitable
 * for GPU rendering.
 *
 * This program:
 *   1. Loads a TrueType font (Liberation Mono) via forge_ui_ttf_load
 *   2. Builds an atlas of printable ASCII (codepoints 32-126) at 32px height
 *   3. Lays out several test strings and writes each as a BMP:
 *      - layout_hello.bmp       -- "Hello, World!" single line
 *      - layout_multiline.bmp   -- multi-line with explicit \n
 *      - layout_wrapped.bmp     -- long text with max_width wrapping
 *      - layout_alignment.bmp   -- left/center/right alignment comparison
 *   4. Prints per-character pen advance, vertex/index counts, and metrics
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

/* ── BMP rendering parameters ────────────────────────────────────────────── */
#define BMP_MARGIN       8      /* pixels of margin around rendered text */

/* ── Quad topology — must match the UI module contract ──────────────────── */
#define VERTICES_PER_QUAD  4    /* 4 corners per character quad */
#define INDICES_PER_QUAD   6    /* 2 CCW triangles per quad */

/* ── Helper: render laid-out text into a grayscale BMP ───────────────────── */
/* Composites each glyph from the atlas at its computed quad position onto a
 * black background.  Uses alpha blending (max) to combine overlapping glyphs.
 * The resulting image is what the GPU renderer would produce (minus color). */

static bool render_layout_to_bmp(const char *path,
                                   const ForgeUiFontAtlas *atlas,
                                   const ForgeUiTextLayout *layout,
                                   int img_w, int img_h)
{
    if (!path || !atlas || !layout) {
        SDL_Log("render_layout_to_bmp: NULL argument");
        return false;
    }
    if (img_w <= 0 || img_h <= 0) {
        SDL_Log("render_layout_to_bmp: invalid dimensions %dx%d", img_w, img_h);
        return false;
    }

    Uint8 *pixels = (Uint8 *)SDL_calloc((size_t)img_w * (size_t)img_h, 1);
    if (!pixels) {
        SDL_Log("render_layout_to_bmp: allocation failed");
        return false;
    }

    /* For each quad (4 vertices), composite the glyph bitmap from the atlas.
     * Each quad's vertices define the screen-space rectangle and atlas UVs. */
    int quad_count = layout->vertex_count / VERTICES_PER_QUAD;
    for (int q = 0; q < quad_count; q++) {
        const ForgeUiVertex *v = &layout->vertices[q * VERTICES_PER_QUAD];

        /* Screen-space quad rectangle (from vertex positions) */
        int sx0 = (int)(v[0].pos_x + 0.5f);  /* top-left x */
        int sy0 = (int)(v[0].pos_y + 0.5f);  /* top-left y */
        int sx1 = (int)(v[2].pos_x + 0.5f);  /* bottom-right x */
        int sy1 = (int)(v[2].pos_y + 0.5f);  /* bottom-right y */

        /* Atlas pixel rectangle (from UV coordinates) */
        int ax0 = (int)(v[0].uv_u * (float)atlas->width + 0.5f);
        int ay0 = (int)(v[0].uv_v * (float)atlas->height + 0.5f);

        int glyph_w = sx1 - sx0;
        int glyph_h = sy1 - sy0;

        /* Blit the glyph from the atlas onto the output image */
        for (int dy = 0; dy < glyph_h; dy++) {
            int dst_y = sy0 + dy;
            int src_y = ay0 + dy;

            if (dst_y < 0 || dst_y >= img_h) continue;
            if (src_y < 0 || src_y >= atlas->height) continue;

            for (int dx = 0; dx < glyph_w; dx++) {
                int dst_x = sx0 + dx;
                int src_x = ax0 + dx;

                if (dst_x < 0 || dst_x >= img_w) continue;
                if (src_x < 0 || src_x >= atlas->width) continue;

                Uint8 src_val = atlas->pixels[src_y * atlas->width + src_x];
                Uint8 *dst = &pixels[dst_y * img_w + dst_x];

                /* Alpha blending: take the maximum (works for single-channel) */
                if (src_val > *dst) {
                    *dst = src_val;
                }
            }
        }
    }

    bool ok = forge_ui__write_grayscale_bmp(path, pixels, img_w, img_h);
    SDL_free(pixels);
    return ok;
}

/* ── Helper: print per-character pen advance for a string ────────────────── */

static void print_pen_advance(const ForgeUiFontAtlas *atlas, const char *text)
{
    if (atlas->units_per_em == 0) {
        SDL_Log("  print_pen_advance: units_per_em is 0 (invalid atlas)");
        return;
    }
    float scale = atlas->pixel_height / (float)atlas->units_per_em;
    float pen_x = 0.0f;

    SDL_Log("  Per-character pen advance:");
    int len = (int)SDL_strlen(text);
    for (int i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)text[i];
        const ForgeUiPackedGlyph *g = forge_ui_atlas_lookup(atlas, ch);
        if (!g) continue;

        float advance = (float)g->advance_width * scale;
        float old_pen = pen_x;
        pen_x += advance;

        if (ch == ' ') {
            SDL_Log("    ' '  pen: %.1f -> %.1f  (advance: %.1f px)",
                    (double)old_pen, (double)pen_x, (double)advance);
        } else {
            SDL_Log("    '%c'  pen: %.1f -> %.1f  (advance: %.1f px, "
                    "bearing: %d,%d  bitmap: %dx%d)",
                    ch, (double)old_pen, (double)pen_x, (double)advance,
                    g->bearing_x, g->bearing_y,
                    g->bitmap_w, g->bitmap_h);
        }
    }
    SDL_Log("  Final pen position: %.1f px", (double)pen_x);
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    const char *font_path = (argc > 1) ? argv[1] : DEFAULT_FONT_PATH;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("UI Lesson 04 -- Text Layout");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("Loading font: %s", font_path);

    /* ── Load font ──────────────────────────────────────────────────── */
    ForgeUiFont font;
    if (!forge_ui_ttf_load(font_path, &font)) {
        SDL_Log("Failed to load font -- see errors above");
        SDL_Quit();
        return 1;
    }

    SDL_Log("  unitsPerEm:  %u", font.head.units_per_em);
    SDL_Log("  ascender:    %d (font units)", font.hhea.ascender);
    SDL_Log("  descender:   %d (font units)", font.hhea.descender);
    SDL_Log("  lineGap:     %d (font units)", font.hhea.line_gap);

    /* Guard against malformed fonts with zero unitsPerEm */
    if (font.head.units_per_em == 0) {
        SDL_Log("  [!] units_per_em is 0 (invalid font), cannot compute scale");
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    float scale = PIXEL_HEIGHT / (float)font.head.units_per_em;
    float ascender_px  = (float)font.hhea.ascender * scale;
    float descender_px = (float)font.hhea.descender * scale;
    float line_gap_px  = (float)font.hhea.line_gap * scale;
    float line_height  = ascender_px - descender_px + line_gap_px;

    SDL_Log("  scale:       %.6f (pixel_height / unitsPerEm)",
            (double)scale);
    SDL_Log("  ascender:    %.1f px", (double)ascender_px);
    SDL_Log("  descender:   %.1f px", (double)descender_px);
    SDL_Log("  lineGap:     %.1f px", (double)line_gap_px);
    SDL_Log("  lineHeight:  %.1f px (ascender - descender + lineGap)",
            (double)line_height);

    /* ── Build atlas ────────────────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("BUILDING ATLAS");
    SDL_Log("%s", THIN_SEP);

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    if (!forge_ui_atlas_build(&font, PIXEL_HEIGHT, codepoints, ASCII_COUNT,
                               ATLAS_PADDING, &atlas)) {
        SDL_Log("Failed to build font atlas -- see errors above");
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    SDL_Log("  atlas: %d x %d pixels, %d glyphs packed",
            atlas.width, atlas.height, atlas.glyph_count);

    /* ══════════════════════════════════════════════════════════════════ */
    /* ── Test 1: "Hello, World!" — single-line layout ─────────────── */
    /* ══════════════════════════════════════════════════════════════════ */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("TEST 1: Single-line layout -- \"Hello, World!\"");
    SDL_Log("%s", THIN_SEP);

    const char *hello_text = "Hello, World!";

    /* Measure first to determine BMP dimensions */
    ForgeUiTextMetrics hello_metrics = forge_ui_text_measure(&atlas, hello_text, NULL);
    SDL_Log("  Measured: %.1f x %.1f px, %d line(s)",
            (double)hello_metrics.width, (double)hello_metrics.height,
            hello_metrics.line_count);

    /* Layout with pen at (margin, margin + ascender) so the baseline
     * is positioned correctly within the image. */
    float baseline_y = (float)BMP_MARGIN + ascender_px;

    ForgeUiTextLayout hello_layout;
    if (!forge_ui_text_layout(&atlas, hello_text,
                                (float)BMP_MARGIN, baseline_y,
                                NULL, &hello_layout)) {
        SDL_Log("  [!] Layout failed");
    } else {
        int quads = hello_layout.vertex_count / VERTICES_PER_QUAD;
        SDL_Log("  Layout result:");
        SDL_Log("    quads:    %d", quads);
        SDL_Log("    vertices: %d (4 per quad)", hello_layout.vertex_count);
        SDL_Log("    indices:  %d (6 per quad)", hello_layout.index_count);
        SDL_Log("    bounds:   %.1f x %.1f px",
                (double)hello_layout.total_width,
                (double)hello_layout.total_height);
        SDL_Log("    lines:    %d", hello_layout.line_count);

        /* Print per-character pen advance */
        print_pen_advance(&atlas, hello_text);

        /* Render to BMP */
        int img_w = (int)(hello_layout.total_width + 0.5f) + BMP_MARGIN * 2;
        int img_h = (int)(hello_layout.total_height + 0.5f) + BMP_MARGIN * 2;
        if (render_layout_to_bmp("layout_hello.bmp", &atlas,
                                   &hello_layout, img_w, img_h)) {
            SDL_Log("  -> layout_hello.bmp: %d x %d", img_w, img_h);
        } else {
            SDL_Log("  [!] Failed to write layout_hello.bmp");
        }

        forge_ui_text_layout_free(&hello_layout);
    }

    /* ══════════════════════════════════════════════════════════════════ */
    /* ── Test 2: Multi-line text with explicit \n ─────────────────── */
    /* ══════════════════════════════════════════════════════════════════ */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("TEST 2: Multi-line layout (explicit newlines)");
    SDL_Log("%s", THIN_SEP);

    const char *multi_text =
        "Line 1: The pen model\n"
        "Line 2: Advance width\n"
        "Line 3: Baseline + bearings\n"
        "Line 4: Vertex + index data";

    ForgeUiTextMetrics multi_metrics = forge_ui_text_measure(&atlas, multi_text, NULL);
    SDL_Log("  Measured: %.1f x %.1f px, %d line(s)",
            (double)multi_metrics.width, (double)multi_metrics.height,
            multi_metrics.line_count);
    SDL_Log("  Line height: %.1f px", (double)line_height);

    ForgeUiTextLayout multi_layout;
    if (!forge_ui_text_layout(&atlas, multi_text,
                                (float)BMP_MARGIN, (float)BMP_MARGIN + ascender_px,
                                NULL, &multi_layout)) {
        SDL_Log("  [!] Layout failed");
    } else {
        SDL_Log("  Layout result:");
        SDL_Log("    quads:    %d", multi_layout.vertex_count / VERTICES_PER_QUAD);
        SDL_Log("    vertices: %d", multi_layout.vertex_count);
        SDL_Log("    indices:  %d", multi_layout.index_count);
        SDL_Log("    bounds:   %.1f x %.1f px",
                (double)multi_layout.total_width,
                (double)multi_layout.total_height);
        SDL_Log("    lines:    %d", multi_layout.line_count);

        int img_w = (int)(multi_layout.total_width + 0.5f) + BMP_MARGIN * 2;
        int img_h = (int)(multi_layout.total_height + 0.5f) + BMP_MARGIN * 2;
        if (render_layout_to_bmp("layout_multiline.bmp", &atlas,
                                   &multi_layout, img_w, img_h)) {
            SDL_Log("  -> layout_multiline.bmp: %d x %d", img_w, img_h);
        } else {
            SDL_Log("  [!] Failed to write layout_multiline.bmp");
        }

        forge_ui_text_layout_free(&multi_layout);
    }

    /* ══════════════════════════════════════════════════════════════════ */
    /* ── Test 3: Word/character wrapping with max_width ───────────── */
    /* ══════════════════════════════════════════════════════════════════ */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("TEST 3: Wrapped layout (max_width = 300 px)");
    SDL_Log("%s", THIN_SEP);

    const char *wrap_text =
        "Text layout converts a string of characters into positioned, "
        "textured quads that a GPU can render in a single draw call.";

    ForgeUiTextOpts wrap_opts = {
        300.0f,                     /* max_width: wrap at 300 pixels */
        FORGE_UI_TEXT_ALIGN_LEFT,   /* left-aligned */
        1.0f, 1.0f, 1.0f, 1.0f     /* white */
    };

    ForgeUiTextMetrics wrap_metrics = forge_ui_text_measure(&atlas, wrap_text,
                                                             &wrap_opts);
    SDL_Log("  max_width: %.0f px", (double)wrap_opts.max_width);
    SDL_Log("  Measured: %.1f x %.1f px, %d line(s)",
            (double)wrap_metrics.width, (double)wrap_metrics.height,
            wrap_metrics.line_count);

    ForgeUiTextLayout wrap_layout;
    if (!forge_ui_text_layout(&atlas, wrap_text,
                                (float)BMP_MARGIN, (float)BMP_MARGIN + ascender_px,
                                &wrap_opts, &wrap_layout)) {
        SDL_Log("  [!] Layout failed");
    } else {
        SDL_Log("  Layout result:");
        SDL_Log("    quads:    %d", wrap_layout.vertex_count / VERTICES_PER_QUAD);
        SDL_Log("    vertices: %d", wrap_layout.vertex_count);
        SDL_Log("    indices:  %d", wrap_layout.index_count);
        SDL_Log("    bounds:   %.1f x %.1f px",
                (double)wrap_layout.total_width,
                (double)wrap_layout.total_height);
        SDL_Log("    lines:    %d", wrap_layout.line_count);

        int img_w = (int)(wrap_opts.max_width) + BMP_MARGIN * 2;
        int img_h = (int)(wrap_layout.total_height + 0.5f) + BMP_MARGIN * 2;
        if (render_layout_to_bmp("layout_wrapped.bmp", &atlas,
                                   &wrap_layout, img_w, img_h)) {
            SDL_Log("  -> layout_wrapped.bmp: %d x %d", img_w, img_h);
        } else {
            SDL_Log("  [!] Failed to write layout_wrapped.bmp");
        }

        forge_ui_text_layout_free(&wrap_layout);
    }

    /* ══════════════════════════════════════════════════════════════════ */
    /* ── Test 4: Text alignment comparison ────────────────────────── */
    /* ══════════════════════════════════════════════════════════════════ */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("TEST 4: Alignment comparison (left / center / right)");
    SDL_Log("%s", THIN_SEP);

    const char *align_text = "Align me!";
    float align_width = 300.0f;

    /* Lay out three copies: left, center, right */
    ForgeUiTextOpts align_opts[3] = {
        { align_width, FORGE_UI_TEXT_ALIGN_LEFT,   1.0f, 1.0f, 1.0f, 1.0f },
        { align_width, FORGE_UI_TEXT_ALIGN_CENTER, 1.0f, 1.0f, 1.0f, 1.0f },
        { align_width, FORGE_UI_TEXT_ALIGN_RIGHT,  1.0f, 1.0f, 1.0f, 1.0f },
    };
    const char *align_labels[] = { "LEFT", "CENTER", "RIGHT" };

    /* Compute total image height: 3 lines of text + margins + spacing */
    float spacing = line_height * 0.5f;
    int align_img_w = (int)align_width + BMP_MARGIN * 2;
    int align_img_h = (int)(3.0f * line_height + 2.0f * spacing) + BMP_MARGIN * 2;

    Uint8 *align_pixels = NULL;
    if (align_img_w <= 0 || align_img_h <= 0) {
        SDL_Log("  [!] Invalid alignment image dimensions %dx%d",
                align_img_w, align_img_h);
    } else {
        align_pixels = (Uint8 *)SDL_calloc(
            (size_t)align_img_w * (size_t)align_img_h, 1);
    }

    if (align_pixels) {
        float y_offset = (float)BMP_MARGIN + ascender_px;

        for (int a = 0; a < 3; a++) {
            ForgeUiTextLayout align_layout;
            if (forge_ui_text_layout(&atlas, align_text,
                                       (float)BMP_MARGIN, y_offset,
                                       &align_opts[a], &align_layout)) {
                SDL_Log("  %s: quads=%d, vertices=%d, indices=%d",
                        align_labels[a],
                        align_layout.vertex_count / VERTICES_PER_QUAD,
                        align_layout.vertex_count,
                        align_layout.index_count);

                /* Composite into the shared image */
                int quad_count = align_layout.vertex_count / VERTICES_PER_QUAD;
                for (int q = 0; q < quad_count; q++) {
                    const ForgeUiVertex *v = &align_layout.vertices[q * VERTICES_PER_QUAD];
                    int sx0 = (int)(v[0].pos_x + 0.5f);
                    int sy0 = (int)(v[0].pos_y + 0.5f);
                    int sx1 = (int)(v[2].pos_x + 0.5f);
                    int sy1 = (int)(v[2].pos_y + 0.5f);
                    int ax0 = (int)(v[0].uv_u * (float)atlas.width + 0.5f);
                    int ay0 = (int)(v[0].uv_v * (float)atlas.height + 0.5f);

                    for (int dy = 0; dy < sy1 - sy0; dy++) {
                        int dst_y = sy0 + dy;
                        int src_y = ay0 + dy;
                        if (dst_y < 0 || dst_y >= align_img_h) continue;
                        if (src_y < 0 || src_y >= atlas.height) continue;

                        for (int dx = 0; dx < sx1 - sx0; dx++) {
                            int dst_x = sx0 + dx;
                            int src_x = ax0 + dx;
                            if (dst_x < 0 || dst_x >= align_img_w) continue;
                            if (src_x < 0 || src_x >= atlas.width) continue;

                            Uint8 val = atlas.pixels[src_y * atlas.width + src_x];
                            Uint8 *dst = &align_pixels[dst_y * align_img_w + dst_x];
                            if (val > *dst) *dst = val;
                        }
                    }
                }

                forge_ui_text_layout_free(&align_layout);
            }

            y_offset += line_height + spacing;
        }

        if (forge_ui__write_grayscale_bmp("layout_alignment.bmp",
                                            align_pixels,
                                            align_img_w, align_img_h)) {
            SDL_Log("  -> layout_alignment.bmp: %d x %d", align_img_w, align_img_h);
        } else {
            SDL_Log("  [!] Failed to write layout_alignment.bmp");
        }

        SDL_free(align_pixels);
    } else {
        SDL_Log("  [!] Allocation failed for alignment test image");
    }

    /* ══════════════════════════════════════════════════════════════════ */
    /* ── Vertex layout description ────────────────────────────────── */
    /* ══════════════════════════════════════════════════════════════════ */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("VERTEX LAYOUT (ForgeUiVertex)");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  struct ForgeUiVertex {");
    SDL_Log("    float pos_x, pos_y;   // offset  0, stride 32 bytes");
    SDL_Log("    float uv_u,  uv_v;    // offset  8");
    SDL_Log("    float r, g, b, a;     // offset 16");
    SDL_Log("  };");
    SDL_Log("  Stride: %d bytes", (int)sizeof(ForgeUiVertex));
    SDL_Log("  Position: offset  0, 2 x float (vec2)");
    SDL_Log("  UV:       offset  8, 2 x float (vec2)");
    SDL_Log("  Color:    offset 16, 4 x float (vec4)");
    SDL_Log("");
    SDL_Log("  Per quad:  %d vertices (%d bytes) + %d indices (%d bytes)",
            VERTICES_PER_QUAD,
            (int)(VERTICES_PER_QUAD * sizeof(ForgeUiVertex)),
            INDICES_PER_QUAD,
            (int)(INDICES_PER_QUAD * sizeof(Uint32)));
    SDL_Log("  100 chars: %d vertices + %d indices",
            100 * VERTICES_PER_QUAD, 100 * INDICES_PER_QUAD);
    SDL_Log("             vs %d vertices without indexing (33%% savings)",
            100 * INDICES_PER_QUAD);

    /* ══════════════════════════════════════════════════════════════════ */
    /* ── Pipeline summary ─────────────────────────────────────────── */
    /* ══════════════════════════════════════════════════════════════════ */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("PIPELINE SUMMARY");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  1. Load font:        forge_ui_ttf_load()");
    SDL_Log("  2. Build atlas:      forge_ui_atlas_build()");
    SDL_Log("  3. Layout text:      forge_ui_text_layout()");
    SDL_Log("     - For each character in the input string:");
    SDL_Log("       a. Look up glyph in atlas (forge_ui_atlas_lookup)");
    SDL_Log("       b. Compute quad position: pen + bearings");
    SDL_Log("       c. Emit 4 vertices (pos, UV, color)");
    SDL_Log("       d. Emit 6 indices (two CCW triangles)");
    SDL_Log("       e. Advance pen by glyph advance width");
    SDL_Log("     - Handle newlines (reset pen x, advance pen y)");
    SDL_Log("     - Handle wrapping (check pen x vs max_width)");
    SDL_Log("     - Apply alignment (post-process vertex positions)");
    SDL_Log("  4. Measure text:     forge_ui_text_measure()");
    SDL_Log("  5. Free layout:      forge_ui_text_layout_free()");
    SDL_Log("");
    SDL_Log("  Output: vertices[] + indices[] + atlas texture");
    SDL_Log("  -> Upload to GPU vertex/index buffers");
    SDL_Log("  -> Bind atlas as single-channel texture");
    SDL_Log("  -> Draw with orthographic projection");
    SDL_Log("  -> One draw call renders all text");

    SDL_Log("%s", SEPARATOR);
    SDL_Log("Done. Output files written to the current directory.");

    /* ── Cleanup ────────────────────────────────────────────────────── */
    forge_ui_atlas_free(&atlas);
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return 0;
}
