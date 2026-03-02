/*
 * UI Lesson 12 — Font Scaling and Spacing
 *
 * Demonstrates: global scale factor, ForgeUiSpacing struct, consistent
 * themed spacing, and atlas rebuild at different scales.
 *
 * Frame 1: Three copies of the same settings panel rendered at scales
 *          0.75, 1.0, and 1.5 side by side — all widgets, text, padding,
 *          and spacing scale proportionally.
 *
 * Frame 2: Two panels at scale 1.0 with different spacing overrides —
 *          one with doubled widget_padding and item_spacing (spacious),
 *          one with halved values (compact).
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "math/forge_math.h"
#include "ui/forge_ui.h"
#include "ui/forge_ui_ctx.h"
#include "raster/forge_raster.h"

/* ── Configuration ─────────────────────────────────────────────────────── */

#define BASE_PIXEL_HEIGHT   16.0f   /* unscaled design font size (pixels) */
#define FRAMEBUFFER_W      900      /* output image width in pixels */
#define FRAMEBUFFER_H      480      /* output image height in pixels */

/* Widget row heights (unscaled pixels) — passed to layout widgets */
#define LABEL_ROW_HEIGHT    22.0f   /* title label widget height */
#define WIDGET_ROW_HEIGHT   24.0f   /* checkbox / slider widget height */
#define BUTTON_ROW_HEIGHT   32.0f   /* button widget height */

/* Layout margins and offsets (pixels) */
#define PANEL_TOP_OFFSET    20.0f   /* vertical space reserved for label above panel */
#define LABEL_PANEL_GAP      4.0f   /* gap between label baseline and panel top edge */
#define FRAME1_MARGIN       10.0f   /* outer margin for scale comparison frame */
#define FRAME2_MARGIN       15.0f   /* outer margin for spacing comparison frame */

/* Demo slider initial values (0-1 normalized) */
#define DEMO_VOLUME_SCALES   0.65f  /* slider value for scale comparison panels */
#define DEMO_VOLUME_SPACIOUS 0.80f  /* slider value for spacious spacing panel */
#define DEMO_VOLUME_COMPACT  0.35f  /* slider value for compact spacing panel */

/* Background color — neutral mid-gray to contrast with the dark navy panels.
 * The UI widgets use dark blue-gray tones (~0.12-0.18 luminance), so a
 * lighter neutral gray ensures the panel edges, headers, and widget
 * backgrounds are all clearly distinguishable. */
#define BG_R  0.30f
#define BG_G  0.30f
#define BG_B  0.32f
#define BG_A  1.0f

/* Accent label color — theme cyan #4fc3f7 */
#define ACCENT_R  0.310f
#define ACCENT_G  0.765f
#define ACCENT_B  0.969f
#define ACCENT_A  1.00f

/* ASCII printable codepoints (space 0x20 through tilde 0x7E) */
#define ASCII_PRINTABLE_START  32   /* first printable ASCII codepoint (space) */
#define ASCII_PRINTABLE_COUNT  95   /* number of printable ASCII chars (32-126) */
#define ATLAS_GLYPH_PADDING     2   /* pixels between packed glyphs in atlas */

static Uint32 codepoints[ASCII_PRINTABLE_COUNT];

static void init_codepoints(void)
{
    for (int i = 0; i < ASCII_PRINTABLE_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_PRINTABLE_START + i);
    }
}

/* ── Render a settings panel with every widget type ────────────────────── */

static void render_settings_panel(ForgeUiContext *ctx,
                                  ForgeUiRect panel_rect,
                                  float *scroll_y,
                                  const char *panel_title,
                                  bool *cb_audio, bool *cb_vsync,
                                  float *sl_volume)
{
    if (!forge_ui_ctx_panel_begin(ctx, panel_title, panel_rect, scroll_y)) {
        return;
    }

    /* Title label */
    forge_ui_ctx_label_colored_layout(ctx, "Settings", LABEL_ROW_HEIGHT * ctx->scale,
                              ACCENT_R, ACCENT_G, ACCENT_B, ACCENT_A);

    /* Two checkboxes */
    forge_ui_ctx_checkbox_layout(ctx, "Audio enabled", cb_audio,
                                 WIDGET_ROW_HEIGHT * ctx->scale);
    forge_ui_ctx_checkbox_layout(ctx, "V-Sync", cb_vsync,
                                 WIDGET_ROW_HEIGHT * ctx->scale);

    /* Slider */
    forge_ui_ctx_slider_layout(ctx, "##volume", sl_volume, 0.0f, 1.0f,
                               WIDGET_ROW_HEIGHT * ctx->scale);

    /* Button */
    forge_ui_ctx_button_layout(ctx, "Apply", BUTTON_ROW_HEIGHT * ctx->scale);

    forge_ui_ctx_panel_end(ctx);
}

/* ── Build atlas, init context with a given scale ──────────────────────── */

typedef struct ScaledUI {
    ForgeUiFontAtlas atlas;  /* font atlas built at base_pixel_height * scale */
    ForgeUiContext   ctx;    /* UI context configured with this scale factor */
    float            scale;  /* global scale factor (>0, e.g. 0.75, 1.0, 1.5) */
} ScaledUI;

static bool scaled_ui_init(ScaledUI *sui, const ForgeUiFont *font,
                           float scale)
{
    sui->scale = scale;

    /* Atlas pixel height = base * scale — glyphs render at scaled
     * resolution.  This is the key to scale-aware rendering: the atlas
     * must be rebuilt at the target size, not stretched. */
    float atlas_px = BASE_PIXEL_HEIGHT * scale;

    if (!forge_ui_atlas_build(font, atlas_px, codepoints,
                              ASCII_PRINTABLE_COUNT, ATLAS_GLYPH_PADDING,
                              &sui->atlas)) {
        SDL_Log("Failed to build atlas at scale %.2f", (double)scale);
        return false;
    }

    if (!forge_ui_ctx_init(&sui->ctx, &sui->atlas)) {
        SDL_Log("Failed to init context at scale %.2f", (double)scale);
        forge_ui_atlas_free(&sui->atlas);
        return false;
    }

    /* Set the scale factor — all FORGE_UI_SCALED reads use this */
    sui->ctx.scale = scale;
    sui->ctx.base_pixel_height = BASE_PIXEL_HEIGHT;
    sui->ctx.scaled_pixel_height = atlas_px;

    return true;
}

static void scaled_ui_free(ScaledUI *sui)
{
    forge_ui_ctx_free(&sui->ctx);
    forge_ui_atlas_free(&sui->atlas);
}

/* ── Render Frame 1: Three scales side by side ─────────────────────────── */

static bool render_frame_scales(ForgeRasterBuffer *fb,
                                const ForgeUiFont *font)
{
    static const float scales[] = { 0.75f, 1.0f, 1.5f };
    static const char *titles[] = { "Scale 0.75", "Scale 1.0", "Scale 1.5" };
    #define NUM_SCALES  3  /* number of scale factors demonstrated side by side */

    ScaledUI sui[NUM_SCALES];
    for (int i = 0; i < NUM_SCALES; i++) {
        if (!scaled_ui_init(&sui[i], font, scales[i])) {
            for (int j = 0; j < i; j++) scaled_ui_free(&sui[j]);
            return false;
        }
    }

    /* Clear framebuffer */
    forge_raster_clear(fb, BG_R, BG_G, BG_B, BG_A);

    /* Compute panel positions — evenly spaced across the framebuffer */
    float col_w = (float)FRAMEBUFFER_W / (float)NUM_SCALES;
    float margin = FRAME1_MARGIN;

    for (int i = 0; i < NUM_SCALES; i++) {
        ForgeUiContext *ctx = &sui[i].ctx;
        float scroll_y = 0.0f;
        bool cb_audio = true, cb_vsync = false;
        float sl_volume = DEMO_VOLUME_SCALES;

        ForgeUiRect panel_rect = {
            col_w * (float)i + margin,
            margin + PANEL_TOP_OFFSET,
            col_w - 2.0f * margin,
            (float)FRAMEBUFFER_H - 2.0f * margin - PANEL_TOP_OFFSET
        };

        forge_ui_ctx_begin(ctx, -1.0f, -1.0f, false);

        /* Scale label above the panel */
        forge_ui_ctx_label_colored(ctx, titles[i],
                           panel_rect.x, panel_rect.y - LABEL_PANEL_GAP,
                           ACCENT_R, ACCENT_G, ACCENT_B, ACCENT_A);

        render_settings_panel(ctx, panel_rect, &scroll_y, "Settings",
                              &cb_audio, &cb_vsync, &sl_volume);

        forge_ui_ctx_end(ctx);

        /* Rasterize this context's draw data into the shared framebuffer */
        ForgeRasterTexture tex = {
            sui[i].atlas.pixels,
            sui[i].atlas.width,
            sui[i].atlas.height
        };
        forge_raster_triangles_indexed(
            fb,
            (const ForgeRasterVertex *)ctx->vertices,
            ctx->vertex_count,
            ctx->indices,
            ctx->index_count,
            &tex);
    }

    /* Write BMP output — propagate failure so main() can return non-zero */
    if (!forge_raster_write_bmp(fb, "scaling_comparison.bmp")) {
        SDL_Log("Failed to write scaling_comparison.bmp");
        for (int i = 0; i < NUM_SCALES; i++) scaled_ui_free(&sui[i]);
        return false;
    }

    for (int i = 0; i < NUM_SCALES; i++) {
        scaled_ui_free(&sui[i]);
    }

    SDL_Log("Frame 1: Rendered three panels at scales 0.75, 1.0, 1.5");
    return true;
}

/* ── Render Frame 2: Spacing overrides comparison ──────────────────────── */

static bool render_frame_spacing(ForgeRasterBuffer *fb,
                                 const ForgeUiFont *font)
{
    /* Both panels at scale 1.0 but different spacing */
    ScaledUI spacious, compact;
    if (!scaled_ui_init(&spacious, font, 1.0f)) return false;
    if (!scaled_ui_init(&compact, font, 1.0f)) {
        scaled_ui_free(&spacious);
        return false;
    }

    /* Spacious: double widget_padding and item_spacing */
    spacious.ctx.spacing.widget_padding = FORGE_UI_WIDGET_PADDING * 2.0f;
    spacious.ctx.spacing.item_spacing   = FORGE_UI_PANEL_CONTENT_SPACING * 2.0f;

    /* Compact: halve widget_padding and item_spacing */
    compact.ctx.spacing.widget_padding = FORGE_UI_WIDGET_PADDING * 0.5f;
    compact.ctx.spacing.item_spacing   = FORGE_UI_PANEL_CONTENT_SPACING * 0.5f;

    /* Clear framebuffer */
    forge_raster_clear(fb, BG_R, BG_G, BG_B, BG_A);

    float half_w = (float)FRAMEBUFFER_W * 0.5f;
    float margin = FRAME2_MARGIN;
    float panel_h = (float)FRAMEBUFFER_H - 2.0f * margin - PANEL_TOP_OFFSET;

    /* Spacious panel (left) */
    {
        ForgeUiContext *ctx = &spacious.ctx;
        float scroll_y = 0.0f;
        bool cb_audio = true, cb_vsync = true;
        float sl_volume = DEMO_VOLUME_SPACIOUS;

        ForgeUiRect panel_rect = {
            margin, margin + PANEL_TOP_OFFSET,
            half_w - 2.0f * margin, panel_h
        };

        forge_ui_ctx_begin(ctx, -1.0f, -1.0f, false);

        forge_ui_ctx_label_colored(ctx, "Spacious (2x padding)",
                           panel_rect.x, panel_rect.y - LABEL_PANEL_GAP,
                           ACCENT_R, ACCENT_G, ACCENT_B, ACCENT_A);

        render_settings_panel(ctx, panel_rect, &scroll_y, "Spacious",
                              &cb_audio, &cb_vsync, &sl_volume);

        forge_ui_ctx_end(ctx);

        ForgeRasterTexture tex = {
            spacious.atlas.pixels,
            spacious.atlas.width,
            spacious.atlas.height
        };
        forge_raster_triangles_indexed(
            fb,
            (const ForgeRasterVertex *)ctx->vertices,
            ctx->vertex_count,
            ctx->indices,
            ctx->index_count,
            &tex);
    }

    /* Compact panel (right) */
    {
        ForgeUiContext *ctx = &compact.ctx;
        float scroll_y = 0.0f;
        bool cb_audio = false, cb_vsync = true;
        float sl_volume = DEMO_VOLUME_COMPACT;

        ForgeUiRect panel_rect = {
            half_w + margin, margin + PANEL_TOP_OFFSET,
            half_w - 2.0f * margin, panel_h
        };

        forge_ui_ctx_begin(ctx, -1.0f, -1.0f, false);

        forge_ui_ctx_label_colored(ctx, "Compact (0.5x padding)",
                           panel_rect.x, panel_rect.y - LABEL_PANEL_GAP,
                           ACCENT_R, ACCENT_G, ACCENT_B, ACCENT_A);

        render_settings_panel(ctx, panel_rect, &scroll_y, "Compact",
                              &cb_audio, &cb_vsync, &sl_volume);

        forge_ui_ctx_end(ctx);

        ForgeRasterTexture tex = {
            compact.atlas.pixels,
            compact.atlas.width,
            compact.atlas.height
        };
        forge_raster_triangles_indexed(
            fb,
            (const ForgeRasterVertex *)ctx->vertices,
            ctx->vertex_count,
            ctx->indices,
            ctx->index_count,
            &tex);
    }

    if (!forge_raster_write_bmp(fb, "spacing_comparison.bmp")) {
        SDL_Log("Failed to write spacing_comparison.bmp");
        scaled_ui_free(&spacious);
        scaled_ui_free(&compact);
        return false;
    }

    SDL_Log("Frame 2: Rendered spacious (2x) vs compact (0.5x) spacing");

    scaled_ui_free(&spacious);
    scaled_ui_free(&compact);
    return true;
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

    init_codepoints();

    /* Load font */
    ForgeUiFont font;
    if (!forge_ui_ttf_load("assets/fonts/liberation_mono/LiberationMono-Regular.ttf", &font)) {
        SDL_Log("Failed to load font");
        SDL_Quit();
        return 1;
    }

    SDL_Log("=== UI Lesson 12: Font Scaling and Spacing ===");
    SDL_Log("");
    SDL_Log("Base pixel height: %.0f", (double)BASE_PIXEL_HEIGHT);
    SDL_Log("Framebuffer: %dx%d", FRAMEBUFFER_W, FRAMEBUFFER_H);
    SDL_Log("");

    /* Print default spacing values */
    SDL_Log("Default ForgeUiSpacing values (unscaled):");
    SDL_Log("  widget_padding      = %.1f", (double)FORGE_UI_WIDGET_PADDING);
    SDL_Log("  item_spacing        = %.1f", (double)FORGE_UI_PANEL_CONTENT_SPACING);
    SDL_Log("  panel_padding       = %.1f", (double)FORGE_UI_PANEL_PADDING);
    SDL_Log("  title_bar_height    = %.1f", (double)FORGE_UI_PANEL_TITLE_HEIGHT);
    SDL_Log("  checkbox_box_size   = %.1f", (double)FORGE_UI_CB_BOX_SIZE);
    SDL_Log("  slider_thumb_width  = %.1f", (double)FORGE_UI_SL_THUMB_WIDTH);
    SDL_Log("  slider_thumb_height = %.1f", (double)FORGE_UI_SL_THUMB_HEIGHT);
    SDL_Log("  slider_track_height = %.1f", (double)FORGE_UI_SL_TRACK_HEIGHT);
    SDL_Log("  text_input_padding  = %.1f", (double)FORGE_UI_TI_PADDING);
    SDL_Log("  scrollbar_width     = %.1f", (double)FORGE_UI_SCROLLBAR_WIDTH);
    SDL_Log("");

    /* Print scaled examples */
    SDL_Log("FORGE_UI_SCALED examples (widget_padding = %.1f):",
            (double)FORGE_UI_WIDGET_PADDING);
    SDL_Log("  scale 0.75 -> %.1f px", (double)(FORGE_UI_WIDGET_PADDING * 0.75f));
    SDL_Log("  scale 1.00 -> %.1f px", (double)(FORGE_UI_WIDGET_PADDING * 1.00f));
    SDL_Log("  scale 1.50 -> %.1f px", (double)(FORGE_UI_WIDGET_PADDING * 1.50f));
    SDL_Log("  scale 2.00 -> %.1f px", (double)(FORGE_UI_WIDGET_PADDING * 2.00f));
    SDL_Log("");

    /* Print atlas sizes at each scale */
    SDL_Log("Atlas pixel_height at each scale:");
    SDL_Log("  scale 0.75 -> %.0f px", (double)(BASE_PIXEL_HEIGHT * 0.75f));
    SDL_Log("  scale 1.00 -> %.0f px", (double)(BASE_PIXEL_HEIGHT * 1.00f));
    SDL_Log("  scale 1.50 -> %.0f px", (double)(BASE_PIXEL_HEIGHT * 1.50f));
    SDL_Log("");

    /* Create framebuffer */
    ForgeRasterBuffer fb = forge_raster_buffer_create(FRAMEBUFFER_W,
                                                       FRAMEBUFFER_H);
    if (!fb.pixels) {
        SDL_Log("Failed to create framebuffer");
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    int exit_code = 0;

    /* Frame 1: Scale comparison */
    SDL_Log("--- Frame 1: Scale comparison ---");
    if (!render_frame_scales(&fb, &font)) {
        SDL_Log("Frame 1 failed");
        exit_code = 1;
    }
    SDL_Log("");

    /* Frame 2: Spacing override comparison */
    SDL_Log("--- Frame 2: Spacing override comparison ---");
    if (!render_frame_spacing(&fb, &font)) {
        SDL_Log("Frame 2 failed");
        exit_code = 1;
    }
    SDL_Log("");

    if (exit_code == 0) {
        SDL_Log("Output: scaling_comparison.bmp, spacing_comparison.bmp");
    }
    SDL_Log("Done.");

    forge_raster_buffer_destroy(&fb);
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return exit_code;
}
