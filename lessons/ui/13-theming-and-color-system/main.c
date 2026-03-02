/*
 * UI Lesson 13 -- Theming and Color System
 *
 * Demonstrates: ForgeUiTheme color palette, WCAG contrast validation,
 * and how every widget derives its colors from a centralized theme.
 *
 * Frame 1: Theme palette panel -- every color slot rendered as a labeled
 *          swatch rect with hex value, organized by role (backgrounds,
 *          text, accents, borders/chrome).
 *
 * Frame 2: Widget showcase panel -- real widgets (button, checkboxes,
 *          slider, text input) rendered with the default theme to show
 *          how theme colors map to interactive elements.
 *
 * Frame 3: Bad theme demo -- a deliberately low-contrast theme is
 *          validated with forge_ui_theme_validate(), and failing pairs
 *          are listed as labels.
 *
 * This is a console program -- no GPU or window is needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "forge.h"
#include "ui/forge_ui.h"
#include "ui/forge_ui_ctx.h"
#include "ui/forge_ui_window.h"
#include "raster/forge_raster.h"

/* ── Configuration ─────────────────────────────────────────────────────── */

#define FRAME_W            800   /* output image width in pixels */
#define FRAME_H            600   /* output image height in pixels */
#define FONT_PATH          "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"
#define FONT_PX            18.0f /* font rasterization height in pixels */
#define SWATCH_SIZE        40.0f /* color swatch square side length */
#define SWATCH_PITCH      150.0f /* horizontal distance between swatch left edges —
                                  * wide enough for the longest slot name
                                  * ("scrollbar_track", ~15 chars × ~11px) */
#define SWATCH_VGAP        10.0f /* vertical gap between swatch rows */
#define SWATCH_LABEL_GAP    6.0f /* vertical gap between swatch and its label */
#define MARGIN             20.0f /* outer margin from framebuffer edges */
#define LABEL_HEIGHT       24.0f /* default label row height for text lines */
#define SWATCH_BORDER_W     1.0f /* swatch outline width in pixels */

/* ASCII printable codepoints (space 0x20 through tilde 0x7E) */
#define ASCII_PRINTABLE_START  32  /* first printable ASCII codepoint (space) */
#define ASCII_PRINTABLE_COUNT  95  /* number of printable ASCII chars (32-126) */
#define ATLAS_GLYPH_PADDING     2  /* pixels between packed glyphs in atlas */

/* Section separators for console output */
#define SEPARATOR "============================================================"

/* Panel layout constants */
#define GROUP_TITLE_HEIGHT 28.0f  /* height allocated for group title labels */
#define SWATCH_ROW_HEIGHT  (SWATCH_SIZE + SWATCH_LABEL_GAP + LABEL_HEIGHT + SWATCH_VGAP)

/* Widget showcase constants */
#define WIDGET_ROW_HEIGHT  28.0f  /* height for checkbox/slider rows */
#define BUTTON_ROW_HEIGHT  36.0f  /* height for button rows */
#define TEXT_INPUT_HEIGHT   28.0f  /* height for text input rows */
#define TEXT_INPUT_BUF_SIZE 64    /* text input buffer capacity in bytes */

/* Number of swatch groups and max swatches per group */
#define SWATCH_GROUP_COUNT       4  /* backgrounds, text, accents, borders */
#define SWATCH_MAX_PER_GROUP     5  /* max color slots in any group */

/* Maximum number of validation pairs to check */
#define MAX_VALIDATION_PAIRS  20

/* Expected pair count from build_validation_pairs().  This MUST match the
 * number of pairs in forge_ui_theme_validate() so the two tables stay in
 * sync.  If you add a pair to either list, update this constant and the
 * other list.  The runtime assertion in render_frame_bad_theme() will
 * catch any mismatch. */
#define EXPECTED_VALIDATION_PAIR_COUNT  17

/* Bad theme failure label colors -- red for FAIL, green for PASS.
 * These are applied to the result labels so users can immediately
 * distinguish passing and failing contrast checks. */
#define FAIL_LABEL_R  0.95f
#define FAIL_LABEL_G  0.30f
#define FAIL_LABEL_B  0.30f
#define FAIL_LABEL_A  1.00f

#define PASS_LABEL_R  0.30f
#define PASS_LABEL_G  0.85f
#define PASS_LABEL_B  0.30f
#define PASS_LABEL_A  1.00f

/* ── Codepoint table ───────────────────────────────────────────────────── */

static Uint32 codepoints[ASCII_PRINTABLE_COUNT];

static void init_codepoints(void)
{
    for (int i = 0; i < ASCII_PRINTABLE_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_PRINTABLE_START + i);
    }
}

/* ── Color-to-hex string conversion ────────────────────────────────────── */

static void color_to_hex(ForgeUiColor c, char *buf, int buf_size)
{
    if (!buf || buf_size < 1) return;
    int ri = (int)(c.r * 255.0f + 0.5f);
    int gi = (int)(c.g * 255.0f + 0.5f);
    int bi = (int)(c.b * 255.0f + 0.5f);
    if (ri < 0) ri = 0; else if (ri > 255) ri = 255;
    if (gi < 0) gi = 0; else if (gi > 255) gi = 255;
    if (bi < 0) bi = 0; else if (bi > 255) bi = 255;
    SDL_snprintf(buf, buf_size, "#%02x%02x%02x", ri, gi, bi);
}

/* ── Swatch descriptor ─────────────────────────────────────────────────── */

/* Pairs a color slot with its display name for the palette visualization. */
typedef struct SwatchInfo {
    const char  *name;   /* display name (e.g. "bg", "surface_hot") */
    ForgeUiColor color;  /* color value from the theme */
} SwatchInfo;

/* A named group of related swatches (e.g. "Backgrounds", "Text"). */
typedef struct SwatchGroup {
    const char *title;                          /* group heading label */
    SwatchInfo  swatches[SWATCH_MAX_PER_GROUP]; /* color slots in this group */
    int         count;                          /* number of valid swatches */
} SwatchGroup;

/* ── Build swatch groups from a theme ──────────────────────────────────── */

static void build_swatch_groups(const ForgeUiTheme *theme,
                                SwatchGroup groups[SWATCH_GROUP_COUNT])
{
    if (!theme || !groups) return;
    /* Group 0: Backgrounds */
    groups[0].title = "Backgrounds";
    groups[0].count = 4;
    groups[0].swatches[0] = (SwatchInfo){ "bg",             theme->bg };
    groups[0].swatches[1] = (SwatchInfo){ "surface",        theme->surface };
    groups[0].swatches[2] = (SwatchInfo){ "surface_hot",    theme->surface_hot };
    groups[0].swatches[3] = (SwatchInfo){ "surface_active", theme->surface_active };

    /* Group 1: Text */
    groups[1].title = "Text";
    groups[1].count = 3;
    groups[1].swatches[0] = (SwatchInfo){ "text",           theme->text };
    groups[1].swatches[1] = (SwatchInfo){ "text_dim",       theme->text_dim };
    groups[1].swatches[2] = (SwatchInfo){ "title_bar_text", theme->title_bar_text };

    /* Group 2: Accents */
    groups[2].title = "Accents";
    groups[2].count = 2;
    groups[2].swatches[0] = (SwatchInfo){ "accent",     theme->accent };
    groups[2].swatches[1] = (SwatchInfo){ "accent_hot", theme->accent_hot };

    /* Group 3: Borders / Chrome */
    groups[3].title = "Borders / Chrome";
    groups[3].count = 5;
    groups[3].swatches[0] = (SwatchInfo){ "border",       theme->border };
    groups[3].swatches[1] = (SwatchInfo){ "brdr_focused", theme->border_focused };
    groups[3].swatches[2] = (SwatchInfo){ "title_bar",    theme->title_bar };
    groups[3].swatches[3] = (SwatchInfo){ "sb_track",     theme->scrollbar_track };
    groups[3].swatches[4] = (SwatchInfo){ "cursor",       theme->cursor };
}

/* ── Render helper: rasterize and write BMP ────────────────────────────── */

static bool render_and_save(ForgeRasterBuffer *fb,
                            const ForgeUiContext *ctx,
                            const ForgeUiFontAtlas *atlas,
                            const char *path)
{
    if (!fb || !ctx || !atlas || !path) return false;
    ForgeRasterTexture tex = {
        atlas->pixels,
        atlas->width,
        atlas->height
    };

    forge_raster_triangles_indexed(
        fb,
        (const ForgeRasterVertex *)ctx->vertices,
        ctx->vertex_count,
        ctx->indices,
        ctx->index_count,
        &tex
    );

    if (!forge_raster_write_bmp(fb, path)) {
        SDL_Log("Failed to write %s", path);
        return false;
    }

    SDL_Log("  -> wrote %s (%d vertices, %d indices)",
            path, ctx->vertex_count, ctx->index_count);
    return true;
}

/* ── Frame 1: Theme palette panel ──────────────────────────────────────── */

static bool render_frame_palette(ForgeRasterBuffer *fb,
                                 ForgeUiContext *ctx,
                                 const ForgeUiFontAtlas *atlas)
{
    ForgeUiTheme theme = forge_ui_theme_default();

    SwatchGroup groups[SWATCH_GROUP_COUNT];
    build_swatch_groups(&theme, groups);

    /* Clear to the theme background color */
    forge_raster_clear(fb, theme.bg.r, theme.bg.g, theme.bg.b, theme.bg.a);

    /* Compute the font ascender for baseline positioning.  The label
     * functions expect the y coordinate at the text baseline, which is
     * offset from the top of the em square by the ascender distance. */
    float ascender_px = 0.0f;
    if (atlas->units_per_em > 0) {
        float scale = atlas->pixel_height / (float)atlas->units_per_em;
        ascender_px = (float)atlas->ascender * scale;
    }

    forge_ui_ctx_begin(ctx, -1.0f, -1.0f, false);

    /* Page title */
    forge_ui_ctx_label_colored(ctx, "Theme Palette -- Default",
                               MARGIN, MARGIN + ascender_px,
                               theme.accent.r, theme.accent.g,
                               theme.accent.b, theme.accent.a);

    /* Draw each swatch group: a title label followed by a row of colored
     * rectangles, each annotated with the slot name and hex color value. */
    float group_y = MARGIN + LABEL_HEIGHT + SWATCH_VGAP;

    for (int g = 0; g < SWATCH_GROUP_COUNT; g++) {
        const SwatchGroup *grp = &groups[g];

        /* Group title */
        forge_ui_ctx_label_colored(ctx, grp->title,
                                   MARGIN, group_y + ascender_px,
                                   theme.text.r, theme.text.g,
                                   theme.text.b, theme.text.a);
        float swatch_y = group_y + GROUP_TITLE_HEIGHT;

        for (int s = 0; s < grp->count; s++) {
            float swatch_x = MARGIN + (float)s * SWATCH_PITCH;

            /* Draw the colored swatch rectangle.  We deliberately use
             * internal emitters here because the public API has no
             * "draw colored rect" primitive -- adding one just for this
             * lesson would be over-engineering.  The lesson already
             * includes forge_ui_ctx.h which exposes these symbols. */
            ForgeUiRect swatch_rect = {
                swatch_x, swatch_y, SWATCH_SIZE, SWATCH_SIZE
            };
            forge_ui__emit_rect(ctx, swatch_rect,
                                grp->swatches[s].color.r,
                                grp->swatches[s].color.g,
                                grp->swatches[s].color.b,
                                grp->swatches[s].color.a);

            /* Draw a thin border around the swatch so dark colors remain
             * visible against the dark background */
            forge_ui__emit_border(ctx, swatch_rect, SWATCH_BORDER_W,
                                  theme.border.r, theme.border.g,
                                  theme.border.b, theme.border.a);

            /* Slot name label below the swatch */
            float name_y = swatch_y + SWATCH_SIZE + SWATCH_LABEL_GAP
                           + ascender_px;
            forge_ui_ctx_label_colored(ctx, grp->swatches[s].name,
                                       swatch_x, name_y,
                                       theme.text_dim.r, theme.text_dim.g,
                                       theme.text_dim.b, theme.text_dim.a);

            /* Hex color label below the name */
            char hex_buf[16];
            color_to_hex(grp->swatches[s].color, hex_buf, sizeof(hex_buf));
            float hex_y = name_y + LABEL_HEIGHT;
            forge_ui_ctx_label_colored(ctx, hex_buf,
                                       swatch_x, hex_y,
                                       theme.text_dim.r, theme.text_dim.g,
                                       theme.text_dim.b, theme.text_dim.a);
        }

        /* Advance to the next group row */
        group_y = swatch_y + SWATCH_ROW_HEIGHT + LABEL_HEIGHT;
    }

    forge_ui_ctx_end(ctx);

    return render_and_save(fb, ctx, atlas, "frame1_palette.bmp");
}

/* ── Frame 2: Widget showcase panel ────────────────────────────────────── */

static bool render_frame_widgets(ForgeRasterBuffer *fb,
                                 ForgeUiContext *ctx,
                                 const ForgeUiFontAtlas *atlas)
{
    ForgeUiTheme theme = forge_ui_theme_default();

    forge_raster_clear(fb, theme.bg.r, theme.bg.g, theme.bg.b, theme.bg.a);

    float ascender_px = 0.0f;
    if (atlas->units_per_em > 0) {
        float scale = atlas->pixel_height / (float)atlas->units_per_em;
        ascender_px = (float)atlas->ascender * scale;
    }

    forge_ui_ctx_begin(ctx, -1.0f, -1.0f, false);

    /* Page title */
    forge_ui_ctx_label_colored(ctx, "Widget Showcase -- Default Theme",
                               MARGIN, MARGIN + ascender_px,
                               theme.accent.r, theme.accent.g,
                               theme.accent.b, theme.accent.a);

    /* Panel containing the widget demo.  The panel draws its own title bar,
     * background, and scrollable content area using theme colors. */
    float panel_x = MARGIN;
    float panel_y = MARGIN + LABEL_HEIGHT + SWATCH_VGAP;
    float panel_w = (float)FRAME_W - 2.0f * MARGIN;
    float panel_h = (float)FRAME_H - panel_y - MARGIN;
    ForgeUiRect panel_rect = { panel_x, panel_y, panel_w, panel_h };
    float scroll_y = 0.0f;

    if (forge_ui_ctx_panel_begin(ctx, "Themed Widgets", panel_rect,
                                  &scroll_y)) {

        /* Description label */
        forge_ui_ctx_label_layout(ctx,
            "All colors below come from ctx->theme.",
            LABEL_HEIGHT);

        /* Separator label for button section */
        forge_ui_ctx_label_colored_layout(ctx,
            "--- Button (normal state) ---",
            LABEL_HEIGHT,
            theme.text_dim.r, theme.text_dim.g,
            theme.text_dim.b, theme.text_dim.a);

        /* Button in normal state (no mouse hover) */
        forge_ui_ctx_button_layout(ctx, "Apply Settings", BUTTON_ROW_HEIGHT);

        /* Separator label for checkbox section */
        forge_ui_ctx_label_colored_layout(ctx,
            "--- Checkboxes ---",
            LABEL_HEIGHT,
            theme.text_dim.r, theme.text_dim.g,
            theme.text_dim.b, theme.text_dim.a);

        /* Checkbox: checked -- the check fill uses the accent color */
        bool cb_checked = true;
        forge_ui_ctx_checkbox_layout(ctx, "Audio enabled (checked)",
                                     &cb_checked, WIDGET_ROW_HEIGHT);

        /* Checkbox: unchecked -- only the box border is drawn */
        bool cb_unchecked = false;
        forge_ui_ctx_checkbox_layout(ctx, "V-Sync (unchecked)",
                                     &cb_unchecked, WIDGET_ROW_HEIGHT);

        /* Separator label for slider section */
        forge_ui_ctx_label_colored_layout(ctx,
            "--- Slider ---",
            LABEL_HEIGHT,
            theme.text_dim.r, theme.text_dim.g,
            theme.text_dim.b, theme.text_dim.a);

        /* Slider at 65% -- track and thumb use theme surface/accent */
        float slider_val = 0.65f;
        forge_ui_ctx_slider_layout(ctx, "##volume", &slider_val,
                                   0.0f, 1.0f, WIDGET_ROW_HEIGHT);

        /* Slider value annotation */
        char slider_text[64];
        SDL_snprintf(slider_text, sizeof(slider_text),
                     "Volume: %.0f%%", (double)(slider_val * 100.0f));
        forge_ui_ctx_label_layout(ctx, slider_text, LABEL_HEIGHT);

        /* Separator label for text input section */
        forge_ui_ctx_label_colored_layout(ctx,
            "--- Text Input ---",
            LABEL_HEIGHT,
            theme.text_dim.r, theme.text_dim.g,
            theme.text_dim.b, theme.text_dim.a);

        /* Text input field -- surface, border, accent, and cursor colors
         * are all derived from the theme. */
        char ti_buffer[TEXT_INPUT_BUF_SIZE] = "Hello, theme!";
        int ti_len = (int)SDL_strlen(ti_buffer);
        ForgeUiTextInputState ti_state = {
            .buffer   = ti_buffer,
            .capacity = TEXT_INPUT_BUF_SIZE,
            .length   = ti_len,
            .cursor   = ti_len
        };
        /* Compute a rect via layout_next since there is no layout
         * variant of forge_ui_ctx_text_input. */
        ForgeUiRect ti_rect = forge_ui_ctx_layout_next(ctx,
                                                        TEXT_INPUT_HEIGHT);
        forge_ui_ctx_text_input(ctx, "##username", &ti_state,
                                ti_rect, true);

        /* Annotation */
        forge_ui_ctx_label_colored_layout(ctx,
            "Surface, border, accent, and cursor colors are all themed.",
            LABEL_HEIGHT,
            theme.text_dim.r, theme.text_dim.g,
            theme.text_dim.b, theme.text_dim.a);

        forge_ui_ctx_panel_end(ctx);
    }

    forge_ui_ctx_end(ctx);

    return render_and_save(fb, ctx, atlas, "frame2_widgets.bmp");
}

/* ── Frame 3: Bad theme with validation ────────────────────────────────── */

/* The validation pair table mirrors forge_ui_theme_validate() so we can
 * report per-pair results.  This duplicates the pair list intentionally
 * because the validate function only returns a count, not which pairs
 * failed.  Keeping the list here lets us label each failure in the frame. */

typedef struct ValidationPair {
    const ForgeUiColor *fg;    /* foreground color */
    const ForgeUiColor *bg;    /* background color */
    const char         *name;  /* human-readable pair description */
    float               min_ratio; /* WCAG threshold: 4.5 text, 3.0 non-text */
} ValidationPair;

static int build_validation_pairs(const ForgeUiTheme *theme,
                                  ValidationPair *pairs,
                                  int max_pairs)
{
    if (!theme || !pairs || max_pairs <= 0) return 0;
    /* WCAG AA thresholds: 4.5:1 for text (SC 1.4.3),
     * 3.0:1 for non-text UI components (SC 1.4.11). */
    const float AA_TEXT    = 4.5f;
    const float AA_NONTEXT = 3.0f;
    int n = 0;

    /* Text on backgrounds (4.5:1) */
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->text, &theme->bg, "text / bg", AA_TEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->text, &theme->surface, "text / surface", AA_TEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->text, &theme->surface_hot, "text / surface_hot", AA_TEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->text, &theme->surface_active, "text / surface_active", AA_TEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->title_bar_text, &theme->title_bar,
        "title_bar_text / title_bar", AA_TEXT };

    /* Dim text on backgrounds (4.5:1) */
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->text_dim, &theme->bg, "text_dim / bg", AA_TEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->text_dim, &theme->surface, "text_dim / surface", AA_TEXT };

    /* Accent text on backgrounds (4.5:1) */
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->accent, &theme->bg, "accent / bg", AA_TEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->accent, &theme->surface, "accent / surface", AA_TEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->accent, &theme->surface_active,
        "accent / surface_active", AA_TEXT };
    /* Non-text graphical component (3.0:1) */
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->accent_hot, &theme->surface_hot,
        "accent_hot / surface_hot", AA_NONTEXT };

    /* Border visibility — non-text UI components (3.0:1, SC 1.4.11) */
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->border, &theme->bg, "border / bg", AA_NONTEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->border_focused, &theme->surface,
        "border_focused / surface", AA_NONTEXT };

    /* Scrollbar — non-text UI components (3.0:1) */
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->accent, &theme->scrollbar_track,
        "accent / scrollbar_track", AA_NONTEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->accent_hot, &theme->scrollbar_track,
        "accent_hot / scrollbar_track", AA_NONTEXT };
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->surface_hot, &theme->scrollbar_track,
        "surface_hot / scrollbar_track", AA_NONTEXT };

    /* Cursor — thin bar, requires text-level contrast (4.5:1, SC 1.4.3) */
    if (n < max_pairs) pairs[n++] = (ValidationPair){
        &theme->cursor, &theme->surface_active,
        "cursor / surface_active", AA_TEXT };

    return n;
}

static bool render_frame_bad_theme(ForgeRasterBuffer *fb,
                                   ForgeUiContext *ctx,
                                   const ForgeUiFontAtlas *atlas)
{
    /* Build a deliberately bad theme: dark text on dark background,
     * producing very low contrast ratios that fail WCAG AA validation.
     * This demonstrates why contrast checking matters for readability. */
    ForgeUiTheme bad = forge_ui_theme_default();
    bad.bg              = (ForgeUiColor){ 0.10f, 0.10f, 0.12f, 1.0f };
    bad.surface         = (ForgeUiColor){ 0.12f, 0.12f, 0.14f, 1.0f };
    bad.surface_hot     = (ForgeUiColor){ 0.14f, 0.14f, 0.16f, 1.0f };
    bad.surface_active  = (ForgeUiColor){ 0.11f, 0.11f, 0.13f, 1.0f };
    bad.text            = (ForgeUiColor){ 0.20f, 0.20f, 0.22f, 1.0f };
    bad.text_dim        = (ForgeUiColor){ 0.15f, 0.15f, 0.17f, 1.0f };
    bad.title_bar       = (ForgeUiColor){ 0.13f, 0.13f, 0.15f, 1.0f };
    bad.title_bar_text  = (ForgeUiColor){ 0.18f, 0.18f, 0.20f, 1.0f };
    bad.accent          = (ForgeUiColor){ 0.16f, 0.16f, 0.20f, 1.0f };
    bad.accent_hot      = (ForgeUiColor){ 0.18f, 0.18f, 0.22f, 1.0f };
    bad.border          = (ForgeUiColor){ 0.11f, 0.11f, 0.13f, 1.0f };
    bad.border_focused  = (ForgeUiColor){ 0.14f, 0.14f, 0.16f, 1.0f };
    bad.scrollbar_track = (ForgeUiColor){ 0.09f, 0.09f, 0.11f, 1.0f };
    bad.cursor          = (ForgeUiColor){ 0.13f, 0.13f, 0.15f, 1.0f };

    /* Validate the bad theme and count failures */
    int failure_count = forge_ui_theme_validate(&bad);
    SDL_Log("  Bad theme validation: %d pair(s) failed WCAG AA "
            "(4.5:1 text / 3.0:1 non-text)", failure_count);

    /* We render the validation results using the DEFAULT theme for
     * readability -- the bad theme's text would be nearly invisible.
     * The bad theme is only used as the data source for contrast checks. */
    ForgeUiTheme good = forge_ui_theme_default();
    forge_raster_clear(fb, good.bg.r, good.bg.g, good.bg.b, good.bg.a);

    float ascender_px = 0.0f;
    if (atlas->units_per_em > 0) {
        float scale = atlas->pixel_height / (float)atlas->units_per_em;
        ascender_px = (float)atlas->ascender * scale;
    }

    forge_ui_ctx_begin(ctx, -1.0f, -1.0f, false);

    /* Page title */
    forge_ui_ctx_label_colored(ctx,
        "Bad Theme -- WCAG Contrast Validation",
        MARGIN, MARGIN + ascender_px,
        good.accent.r, good.accent.g, good.accent.b, good.accent.a);

    /* Subtitle with failure summary */
    char summary[128];
    SDL_snprintf(summary, sizeof(summary),
                 "%d pair(s) fail WCAG AA minimum contrast ratio",
                 failure_count);
    forge_ui_ctx_label(ctx, summary,
                       MARGIN,
                       MARGIN + LABEL_HEIGHT + ascender_px);

    /* Build the validation pair list and check each one against the
     * bad theme, labeling PASS or FAIL with the measured ratio. */
    ValidationPair pairs[MAX_VALIDATION_PAIRS];
    int pair_count = build_validation_pairs(&bad, pairs,
                                             MAX_VALIDATION_PAIRS);

    /* Sanity check: our local pair table must stay in sync with the
     * canonical validator in forge_ui_theme_validate().  If someone adds
     * a pair to one list but not the other, this will fire and the
     * mismatch can be fixed before it ships.
     * SDL_assert is stripped in release builds, so also log and cap at
     * runtime to keep the lesson correct in all configurations. */
    SDL_assert(pair_count == EXPECTED_VALIDATION_PAIR_COUNT &&
               "build_validation_pairs pair count drifted from "
               "EXPECTED_VALIDATION_PAIR_COUNT -- update both lists");
    if (pair_count != EXPECTED_VALIDATION_PAIR_COUNT) {
        SDL_Log("WARNING: pair_count %d != EXPECTED %d — table drift",
                pair_count, EXPECTED_VALIDATION_PAIR_COUNT);
        if (pair_count > EXPECTED_VALIDATION_PAIR_COUNT)
            pair_count = EXPECTED_VALIDATION_PAIR_COUNT;
    }

    float row_y = MARGIN + 2.0f * LABEL_HEIGHT + SWATCH_VGAP;

    for (int i = 0; i < pair_count; i++) {
        float ratio = forge_ui_theme_contrast_ratio(
            pairs[i].fg->r, pairs[i].fg->g, pairs[i].fg->b,
            pairs[i].bg->r, pairs[i].bg->g, pairs[i].bg->b);

        bool passes = (ratio >= pairs[i].min_ratio);

        /* Format: "[FAIL] pair_name  ratio: 1.23:1  (min 4.5)" */
        char line[256];
        SDL_snprintf(line, sizeof(line),
                     "[%s] %-30s  ratio: %.2f:1  (min %.1f)",
                     passes ? "PASS" : "FAIL",
                     pairs[i].name,
                     (double)ratio,
                     (double)pairs[i].min_ratio);

        float label_r = passes ? PASS_LABEL_R : FAIL_LABEL_R;
        float label_g = passes ? PASS_LABEL_G : FAIL_LABEL_G;
        float label_b = passes ? PASS_LABEL_B : FAIL_LABEL_B;
        float label_a = passes ? PASS_LABEL_A : FAIL_LABEL_A;

        forge_ui_ctx_label_colored(ctx, line,
                                   MARGIN, row_y + ascender_px,
                                   label_r, label_g, label_b, label_a);

        SDL_Log("    %s", line);

        row_y += LABEL_HEIGHT;
    }

    forge_ui_ctx_end(ctx);

    return render_and_save(fb, ctx, atlas, "frame3_bad_theme.bmp");
}

/* ── Main ──────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== UI Lesson 13: Theming and Color System ===");
    SDL_Log("%s", SEPARATOR);

    /* ── Initialize codepoints ───────────────────────────────────────── */
    init_codepoints();

    /* ── Load font ───────────────────────────────────────────────────── */
    ForgeUiFont font;
    if (!forge_ui_ttf_load(FONT_PATH, &font)) {
        SDL_Log("Failed to load font: %s", FONT_PATH);
        SDL_Quit();
        return 1;
    }

    SDL_Log("Loaded font: %s", FONT_PATH);

    /* ── Build font atlas ────────────────────────────────────────────── */
    ForgeUiFontAtlas atlas;
    if (!forge_ui_atlas_build(&font, FONT_PX, codepoints,
                               ASCII_PRINTABLE_COUNT, ATLAS_GLYPH_PADDING,
                               &atlas)) {
        SDL_Log("Failed to build font atlas");
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    SDL_Log("  Atlas: %d x %d pixels, %d glyphs at %.0f px",
            atlas.width, atlas.height, atlas.glyph_count,
            (double)atlas.pixel_height);

    /* ── Initialize UI context ───────────────────────────────────────── */
    ForgeUiContext ctx;
    if (!forge_ui_ctx_init(&ctx, &atlas)) {
        SDL_Log("Failed to initialize UI context");
        forge_ui_atlas_free(&atlas);
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    /* ── Create framebuffer ──────────────────────────────────────────── */
    ForgeRasterBuffer fb = forge_raster_buffer_create(FRAME_W, FRAME_H);
    if (!fb.pixels) {
        SDL_Log("Failed to create %dx%d framebuffer", FRAME_W, FRAME_H);
        forge_ui_ctx_free(&ctx);
        forge_ui_atlas_free(&atlas);
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    /* ── Log default theme validation ────────────────────────────────── */
    ForgeUiTheme theme = forge_ui_theme_default();
    int default_failures = forge_ui_theme_validate(&theme);
    SDL_Log("");
    SDL_Log("Default theme WCAG validation: %d failure(s)",
            default_failures);
    SDL_Log("");

    int exit_code = 0;

    /* ── Frame 1: Theme palette ──────────────────────────────────────── */
    SDL_Log("--- Frame 1: Theme palette ---");
    if (!render_frame_palette(&fb, &ctx, &atlas)) {
        SDL_Log("Frame 1 failed");
        exit_code = 1;
    }

    /* ── Frame 2: Widget showcase ────────────────────────────────────── */
    SDL_Log("--- Frame 2: Widget showcase ---");
    if (!render_frame_widgets(&fb, &ctx, &atlas)) {
        SDL_Log("Frame 2 failed");
        exit_code = 1;
    }

    /* ── Frame 3: Bad theme demo ─────────────────────────────────────── */
    SDL_Log("--- Frame 3: Bad theme validation ---");
    if (!render_frame_bad_theme(&fb, &ctx, &atlas)) {
        SDL_Log("Frame 3 failed");
        exit_code = 1;
    }

    /* ── Summary ─────────────────────────────────────────────────────── */
    SDL_Log("");
    if (exit_code == 0) {
        SDL_Log("Output: frame1_palette.bmp, frame2_widgets.bmp, "
                "frame3_bad_theme.bmp");
    }
    SDL_Log("Done.");

    /* ── Cleanup ─────────────────────────────────────────────────────── */
    forge_raster_buffer_destroy(&fb);
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    forge_ui_ttf_free(&font);
    SDL_Quit();

    return exit_code;
}
