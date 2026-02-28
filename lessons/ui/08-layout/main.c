/*
 * UI Lesson 08 -- Layout
 *
 * Demonstrates: Automatic widget layout with ForgeUiLayout, a stack-based
 * system that replaces manual rect calculations with a cursor model.
 *
 * A layout defines a rectangular region, a direction (vertical or
 * horizontal), padding (inset from edges), spacing (gap between widgets),
 * and a cursor that advances after each widget is placed.
 *
 * This program:
 *   1. Loads a TrueType font and builds a font atlas
 *   2. Initializes a ForgeUiContext with layout support
 *   3. Renders a "manual layout" frame where all widget rects are
 *      computed by hand -- this establishes the baseline
 *   4. Renders the same UI using the layout system -- identical output
 *      proves the layout cursor model works correctly
 *   5. Simulates 8 frames of a settings panel built entirely with the
 *      layout system: a title label, three checkboxes, a horizontal row
 *      of two buttons (OK and Cancel), and a slider -- all without any
 *      manual rect calculations
 *   6. Each frame: declares widgets via _layout() variants, generates
 *      vertex/index data, renders with the software rasterizer, writes
 *      a BMP image
 *
 * Output images show the settings panel responding to mouse interaction
 * across frames.  A yellow dot shows the simulated cursor position.
 *
 * This is a console program -- no GPU or window is needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "ui/forge_ui.h"
#include "ui/forge_ui_ctx.h"
#include "raster/forge_raster.h"

/* ── Default font path ──────────────────────────────────────────────────── */
#define DEFAULT_FONT_PATH "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"

/* ── Section separators for console output ───────────────────────────────── */
#define SEPARATOR "============================================================"
#define THIN_SEP  "------------------------------------------------------------"

/* ── Atlas parameters ────────────────────────────────────────────────────── */
#define PIXEL_HEIGHT     24.0f  /* render glyphs at 24 pixels tall */
#define ATLAS_PADDING    1      /* 1 pixel padding between glyphs */
#define ASCII_START      32     /* first printable ASCII codepoint (space) */
#define ASCII_END        126    /* last printable ASCII codepoint (tilde) */
#define ASCII_COUNT      (ASCII_END - ASCII_START + 1)  /* 95 glyphs */

/* ── Framebuffer dimensions ──────────────────────────────────────────────── */
#define FB_WIDTH   512  /* output image width in pixels */
#define FB_HEIGHT  400  /* output image height in pixels */

/* ── Settings panel layout constants ─────────────────────────────────────── */
#define PANEL_X          30.0f   /* panel left edge */
#define PANEL_Y          20.0f   /* panel top edge */
#define PANEL_W         360.0f   /* panel width */
#define PANEL_H         340.0f   /* panel height */
#define PANEL_PADDING    16.0f   /* inset from panel edges */
#define WIDGET_SPACING    8.0f   /* vertical gap between widgets */
#define LABEL_HEIGHT     30.0f   /* height of a label row */
#define CHECKBOX_HEIGHT  28.0f   /* height of a checkbox row */
#define BUTTON_HEIGHT    34.0f   /* height of a button */
#define SLIDER_HEIGHT    32.0f   /* height of a slider */
#define BUTTON_ROW_H     34.0f   /* height of the horizontal button row */
#define BUTTON_SPACING   10.0f   /* horizontal gap between buttons */

/* ── Widget IDs ──────────────────────────────────────────────────────────── */
#define ID_CB_VSYNC       1
#define ID_CB_FULLSCREEN  2
#define ID_CB_AA          3
#define ID_BTN_OK         4
#define ID_BTN_CANCEL     5
#define ID_SLIDER_VOL     6

/* ── Background clear color (dark slate, same as lessons 05-07) ──────────── */
#define BG_CLEAR_R      0.08f
#define BG_CLEAR_G      0.08f
#define BG_CLEAR_B      0.12f
#define BG_CLEAR_A      1.00f

/* ── Title label color (soft blue-gray) ──────────────────────────────────── */
#define TITLE_R         0.70f
#define TITLE_G         0.80f
#define TITLE_B         0.90f
#define TITLE_A         1.00f

/* ── Status label color (warm gold) ──────────────────────────────────────── */
#define STATUS_R        0.90f
#define STATUS_G        0.90f
#define STATUS_B        0.60f
#define STATUS_A        1.00f

/* ── Panel background color ──────────────────────────────────────────────── */
#define PANEL_BG_R      0.12f
#define PANEL_BG_G      0.12f
#define PANEL_BG_B      0.16f
#define PANEL_BG_A      1.00f

/* ── Mouse cursor dot ────────────────────────────────────────────────────── */
#define CURSOR_DOT_RADIUS     2   /* pixel radius for the circular dot */
#define CURSOR_DOT_RADIUS_SQ  5   /* slightly > RADIUS*RADIUS (4) to include
                                   * diagonal pixels and produce a rounder dot */
#define CURSOR_DOT_R    255       /* red channel (uint8) */
#define CURSOR_DOT_G    220       /* green channel (uint8) */
#define CURSOR_DOT_B     50       /* blue channel (uint8) */
#define CURSOR_DOT_A    255       /* alpha channel (uint8) */

/* ── Slider demo parameters ─────────────────────────────────────────────── */
#define VOLUME_MIN       0.0f    /* slider minimum value */
#define VOLUME_MAX     100.0f    /* slider maximum value */
#define INITIAL_VOLUME  50.0f    /* default volume level */

/* ── Simulated interaction targets ──────────────────────────────────────── */
#define CB_BOX_CENTER_X     10.0f   /* approx horizontal center of checkbox box */
#define STATUS_LABEL_GAP    16.0f   /* pixels below panel to status text */
#define SLIDER_DRAG_FRAC     0.75f  /* fraction along slider track for drag demo */

/* ── Simulated frame input ───────────────────────────────────────────────── */

typedef struct FrameInput {
    float       mouse_x;        /* simulated cursor x in screen pixels */
    float       mouse_y;        /* simulated cursor y in screen pixels */
    bool        mouse_down;     /* true if the primary button is held */
    const char *description;    /* what this frame demonstrates (for logging) */
} FrameInput;

/* ── Helper: render a frame's draw data to BMP ───────────────────────────── */

static bool render_frame_bmp(const char *path,
                             const ForgeUiContext *ctx,
                             const ForgeUiFontAtlas *atlas,
                             float mouse_x, float mouse_y)
{
    ForgeRasterBuffer fb = forge_raster_buffer_create(FB_WIDTH, FB_HEIGHT);
    if (!fb.pixels) {
        SDL_Log("  [!] Failed to create framebuffer");
        return false;
    }

    forge_raster_clear(&fb, BG_CLEAR_R, BG_CLEAR_G, BG_CLEAR_B, BG_CLEAR_A);

    ForgeRasterTexture tex = {
        atlas->pixels,
        atlas->width,
        atlas->height
    };

    forge_raster_triangles_indexed(
        &fb,
        (const ForgeRasterVertex *)ctx->vertices,
        ctx->vertex_count,
        ctx->indices,
        ctx->index_count,
        &tex
    );

    /* Draw a small yellow dot at the mouse position */
    int mx = (int)(mouse_x + 0.5f);
    int my = (int)(mouse_y + 0.5f);
    for (int dy = -CURSOR_DOT_RADIUS; dy <= CURSOR_DOT_RADIUS; dy++) {
        for (int dx = -CURSOR_DOT_RADIUS; dx <= CURSOR_DOT_RADIUS; dx++) {
            if (dx * dx + dy * dy > CURSOR_DOT_RADIUS_SQ) continue;
            int px = mx + dx;
            int py = my + dy;
            if (px < 0 || px >= FB_WIDTH || py < 0 || py >= FB_HEIGHT) continue;
            Uint8 *pixel = fb.pixels + (size_t)py * (size_t)fb.stride
                         + (size_t)px * FORGE_RASTER_BPP;
            pixel[0] = CURSOR_DOT_R;
            pixel[1] = CURSOR_DOT_G;
            pixel[2] = CURSOR_DOT_B;
            pixel[3] = CURSOR_DOT_A;
        }
    }

    bool ok = forge_raster_write_bmp(&fb, path);
    forge_raster_buffer_destroy(&fb);
    return ok;
}

/* ── Helper: declare the settings panel using the layout system ──────────── */

static void declare_settings_panel_layout(ForgeUiContext *ctx,
                                          bool *vsync, bool *fullscreen,
                                          bool *aa, float *volume)
{
    /* Panel background */
    ForgeUiRect panel = { PANEL_X, PANEL_Y, PANEL_W, PANEL_H };
    forge_ui__emit_rect(ctx, panel, PANEL_BG_R, PANEL_BG_G,
                        PANEL_BG_B, PANEL_BG_A);

    /* Push the outer vertical layout that covers the panel */
    if (!forge_ui_ctx_layout_push(ctx, panel,
                                  FORGE_UI_LAYOUT_VERTICAL,
                                  PANEL_PADDING, WIDGET_SPACING)) {
        return;
    }

    /* Title label */
    forge_ui_ctx_label_layout(ctx, "Settings", LABEL_HEIGHT,
                              TITLE_R, TITLE_G, TITLE_B, TITLE_A);

    /* Three checkboxes stacked vertically (return value = toggled this
     * frame; we don't need per-frame toggle info in this demo because
     * checkbox state is tracked via the bool pointer) */
    (void)forge_ui_ctx_checkbox_layout(ctx, ID_CB_VSYNC, "V-Sync",
                                       vsync, CHECKBOX_HEIGHT);
    (void)forge_ui_ctx_checkbox_layout(ctx, ID_CB_FULLSCREEN, "Fullscreen",
                                       fullscreen, CHECKBOX_HEIGHT);
    (void)forge_ui_ctx_checkbox_layout(ctx, ID_CB_AA, "Anti-aliasing",
                                       aa, CHECKBOX_HEIGHT);

    /* Horizontal sub-layout for OK and Cancel buttons side by side.
     * First, get the rect for the button row from the outer layout. */
    ForgeUiRect button_row = forge_ui_ctx_layout_next(ctx, BUTTON_ROW_H);

    if (!forge_ui_ctx_layout_push(ctx, button_row,
                                  FORGE_UI_LAYOUT_HORIZONTAL,
                                  0.0f, BUTTON_SPACING)) {
        forge_ui_ctx_layout_pop(ctx);  /* pop outer layout before returning */
        return;
    }

    /* Each button gets half the row width minus half the spacing */
    float btn_w = (button_row.w - BUTTON_SPACING) * 0.5f;
    /* Button return values intentionally discarded — this demo
     * demonstrates layout positioning, not button click handling. */
    (void)forge_ui_ctx_button_layout(ctx, ID_BTN_OK, "OK", btn_w);
    (void)forge_ui_ctx_button_layout(ctx, ID_BTN_CANCEL, "Cancel", btn_w);

    forge_ui_ctx_layout_pop(ctx);  /* end horizontal button row */

    /* Slider for volume (return value = changed this frame; the value
     * itself is tracked via the float pointer) */
    (void)forge_ui_ctx_slider_layout(ctx, ID_SLIDER_VOL, volume,
                                     VOLUME_MIN, VOLUME_MAX, SLIDER_HEIGHT);

    forge_ui_ctx_layout_pop(ctx);  /* end outer vertical layout */
}

/* ── Helper: declare the same panel with manual rect calculations ────────── */
/* This produces identical output to the layout version, proving that the
 * layout cursor model computes the same positions as hand-written code. */

static void declare_settings_panel_manual(ForgeUiContext *ctx,
                                          bool *vsync, bool *fullscreen,
                                          bool *aa, float *volume)
{
    /* Panel background */
    ForgeUiRect panel = { PANEL_X, PANEL_Y, PANEL_W, PANEL_H };
    forge_ui__emit_rect(ctx, panel, PANEL_BG_R, PANEL_BG_G,
                        PANEL_BG_B, PANEL_BG_A);

    /* Available width inside the panel after padding */
    float inner_w = PANEL_W - 2.0f * PANEL_PADDING;
    float cx = PANEL_X + PANEL_PADDING;  /* cursor x = left edge + padding */
    float cy = PANEL_Y + PANEL_PADDING;  /* cursor y = top edge + padding */

    /* Compute baseline for label vertical centering */
    float ascender_px = 0.0f;
    if (ctx->atlas->units_per_em > 0) {
        float scale = ctx->atlas->pixel_height / (float)ctx->atlas->units_per_em;
        ascender_px = (float)ctx->atlas->ascender * scale;
    }

    /* Title label */
    {
        float text_y = cy + (LABEL_HEIGHT - ctx->atlas->pixel_height) * 0.5f
                       + ascender_px;
        forge_ui_ctx_label(ctx, "Settings", cx, text_y,
                           TITLE_R, TITLE_G, TITLE_B, TITLE_A);
        cy += LABEL_HEIGHT + WIDGET_SPACING;
    }

    /* Three checkboxes */
    {
        ForgeUiRect r = { cx, cy, inner_w, CHECKBOX_HEIGHT };
        (void)forge_ui_ctx_checkbox(ctx, ID_CB_VSYNC, "V-Sync", vsync, r);
        cy += CHECKBOX_HEIGHT + WIDGET_SPACING;
    }
    {
        ForgeUiRect r = { cx, cy, inner_w, CHECKBOX_HEIGHT };
        (void)forge_ui_ctx_checkbox(ctx, ID_CB_FULLSCREEN, "Fullscreen", fullscreen, r);
        cy += CHECKBOX_HEIGHT + WIDGET_SPACING;
    }
    {
        ForgeUiRect r = { cx, cy, inner_w, CHECKBOX_HEIGHT };
        (void)forge_ui_ctx_checkbox(ctx, ID_CB_AA, "Anti-aliasing", aa, r);
        cy += CHECKBOX_HEIGHT + WIDGET_SPACING;
    }

    /* Button row: two buttons side by side */
    {
        float btn_w = (inner_w - BUTTON_SPACING) * 0.5f;
        ForgeUiRect ok_r = { cx, cy, btn_w, BUTTON_ROW_H };
        (void)forge_ui_ctx_button(ctx, ID_BTN_OK, "OK", ok_r);
        ForgeUiRect cancel_r = { cx + btn_w + BUTTON_SPACING, cy,
                                 btn_w, BUTTON_ROW_H };
        (void)forge_ui_ctx_button(ctx, ID_BTN_CANCEL, "Cancel", cancel_r);
        cy += BUTTON_ROW_H + WIDGET_SPACING;
    }

    /* Slider */
    {
        ForgeUiRect r = { cx, cy, inner_w, SLIDER_HEIGHT };
        (void)forge_ui_ctx_slider(ctx, ID_SLIDER_VOL, volume,
                                 VOLUME_MIN, VOLUME_MAX, r);
    }
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    const char *font_path = (argc > 1) ? argv[1] : DEFAULT_FONT_PATH;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("UI Lesson 08 -- Layout");
    SDL_Log("%s", SEPARATOR);

    /* ── Load font and build atlas ────────────────────────────────────── */
    SDL_Log("Loading font: %s", font_path);

    ForgeUiFont font;
    if (!forge_ui_ttf_load(font_path, &font)) {
        SDL_Log("Failed to load font");
        SDL_Quit();
        return 1;
    }

    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    ForgeUiFontAtlas atlas;
    if (!forge_ui_atlas_build(&font, PIXEL_HEIGHT, codepoints, ASCII_COUNT,
                               ATLAS_PADDING, &atlas)) {
        SDL_Log("Failed to build font atlas");
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    SDL_Log("  Atlas: %d x %d pixels, %d glyphs",
            atlas.width, atlas.height, atlas.glyph_count);

    /* ── Initialize UI context ────────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("INITIALIZING UI CONTEXT");
    SDL_Log("%s", THIN_SEP);

    ForgeUiContext ctx;
    if (!forge_ui_ctx_init(&ctx, &atlas)) {
        SDL_Log("Failed to initialize UI context");
        forge_ui_atlas_free(&atlas);
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    SDL_Log("  Layout stack capacity: %d", FORGE_UI_LAYOUT_MAX_DEPTH);
    SDL_Log("  Initial vertex capacity: %d", ctx.vertex_capacity);
    SDL_Log("  Initial index capacity:  %d", ctx.index_capacity);

    /* ── Application-owned widget state ───────────────────────────────── */
    bool vsync_on = true;
    bool fullscreen_on = false;
    bool aa_on = true;
    float volume = INITIAL_VOLUME;

    bool had_render_error = false;

    /* ══════════════════════════════════════════════════════════════════════ */
    /* ── Phase 1: Manual vs Layout comparison ─────────────────────────── */
    /* ══════════════════════════════════════════════════════════════════════ */

    SDL_Log("%s", SEPARATOR);
    SDL_Log("PHASE 1: MANUAL vs LAYOUT COMPARISON");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("Rendering the same settings panel two ways to verify the");
    SDL_Log("layout system produces identical positions.");

    float idle_mx = 450.0f;
    float idle_my = 50.0f;

    /* Manual version */
    {
        bool m_vsync = true, m_fs = false, m_aa = true;
        float m_vol = INITIAL_VOLUME;

        forge_ui_ctx_begin(&ctx, idle_mx, idle_my, false);
        declare_settings_panel_manual(&ctx, &m_vsync, &m_fs, &m_aa, &m_vol);
        forge_ui_ctx_end(&ctx);

        SDL_Log("%s", "");
        SDL_Log("  Manual layout: %d vertices, %d indices",
                ctx.vertex_count, ctx.index_count);

        if (!render_frame_bmp("layout_manual.bmp", &ctx, &atlas,
                              idle_mx, idle_my)) {
            SDL_Log("  [!] Failed to write layout_manual.bmp");
            had_render_error = true;
        } else {
            SDL_Log("  -> layout_manual.bmp");
        }
    }

    int manual_verts = ctx.vertex_count;
    int manual_idxs = ctx.index_count;

    /* Layout version */
    {
        bool l_vsync = true, l_fs = false, l_aa = true;
        float l_vol = INITIAL_VOLUME;

        forge_ui_ctx_begin(&ctx, idle_mx, idle_my, false);
        declare_settings_panel_layout(&ctx, &l_vsync, &l_fs, &l_aa, &l_vol);
        forge_ui_ctx_end(&ctx);

        SDL_Log("  Layout system: %d vertices, %d indices",
                ctx.vertex_count, ctx.index_count);

        if (!render_frame_bmp("layout_auto.bmp", &ctx, &atlas,
                              idle_mx, idle_my)) {
            SDL_Log("  [!] Failed to write layout_auto.bmp");
            had_render_error = true;
        } else {
            SDL_Log("  -> layout_auto.bmp");
        }
    }

    /* Save Phase 1 layout counts now — later phases overwrite ctx counts */
    int layout_verts = ctx.vertex_count;
    int layout_idxs  = ctx.index_count;

    /* Verify vertex/index counts match */
    bool counts_match = (layout_verts == manual_verts
                         && layout_idxs == manual_idxs);
    if (counts_match) {
        SDL_Log("%s", "");
        SDL_Log("  [OK] Draw data counts match: %d vertices, %d indices",
                manual_verts, manual_idxs);
    } else {
        SDL_Log("  [!] MISMATCH: manual=%d/%d  layout=%d/%d",
                manual_verts, manual_idxs,
                layout_verts, layout_idxs);
    }

    /* ══════════════════════════════════════════════════════════════════════ */
    /* ── Phase 2: Interactive settings panel (8 frames) ──────────────── */
    /* ══════════════════════════════════════════════════════════════════════ */

    SDL_Log("%s", SEPARATOR);
    SDL_Log("PHASE 2: INTERACTIVE SETTINGS PANEL (8 frames)");
    SDL_Log("%s", SEPARATOR);

    /* Compute interactive targets for the settings panel.
     * The layout positions are: panel padding insets, then each widget's
     * center.  We compute the y positions matching the layout cursor. */
    float inner_w = PANEL_W - 2.0f * PANEL_PADDING;
    float left    = PANEL_X + PANEL_PADDING;
    float cy      = PANEL_Y + PANEL_PADDING;

    /* Skip title label */
    cy += LABEL_HEIGHT + WIDGET_SPACING;

    /* Checkbox centers (V-Sync, Fullscreen, Anti-aliasing) */
    float cb_vsync_y = cy + CHECKBOX_HEIGHT * 0.5f;
    cy += CHECKBOX_HEIGHT + WIDGET_SPACING;

    float cb_fs_y = cy + CHECKBOX_HEIGHT * 0.5f;
    cy += CHECKBOX_HEIGHT + WIDGET_SPACING;

    float cb_aa_y = cy + CHECKBOX_HEIGHT * 0.5f;
    cy += CHECKBOX_HEIGHT + WIDGET_SPACING;

    /* Button row: OK is left half, Cancel is right half */
    float btn_w = (inner_w - BUTTON_SPACING) * 0.5f;
    float btn_ok_cx = left + btn_w * 0.5f;
    float btn_cancel_cx = left + btn_w + BUTTON_SPACING + btn_w * 0.5f;
    float btn_cy = cy + BUTTON_ROW_H * 0.5f;
    cy += BUTTON_ROW_H + WIDGET_SPACING;

    /* Slider center -- use right side of track for a ~75% value */
    float slider_cx = left + inner_w * SLIDER_DRAG_FRAC;
    float slider_cy = cy + SLIDER_HEIGHT * 0.5f;

    /* Checkbox x center (the box is at the left edge) */
    float cb_cx = left + CB_BOX_CENTER_X;

    FrameInput frames[] = {
        /* Frame 0: Mouse idle -- no interaction */
        { idle_mx, idle_my, false,
          "Mouse idle -- all widgets in normal state" },

        /* Frame 1: Mouse hovers over V-Sync checkbox */
        { cb_cx, cb_vsync_y, false,
          "Hover over V-Sync checkbox (hot)" },

        /* Frame 2: Mouse pressed on V-Sync -- becomes active */
        { cb_cx, cb_vsync_y, true,
          "Press V-Sync checkbox (active)" },

        /* Frame 3: Mouse released on V-Sync -- toggles off */
        { cb_cx, cb_vsync_y, false,
          "Release on V-Sync -- toggled OFF" },

        /* Frame 4: Mouse hovers over OK button */
        { btn_ok_cx, btn_cy, false,
          "Hover over OK button (hot)" },

        /* Frame 5: Mouse pressed on OK button */
        { btn_ok_cx, btn_cy, true,
          "Press OK button (active)" },

        /* Frame 6: Mouse released on OK -- click */
        { btn_ok_cx, btn_cy, false,
          "Release on OK button -- CLICKED" },

        /* Frame 7: Mouse drags slider to ~75% */
        { slider_cx, slider_cy, true,
          "Drag slider to ~75% position" },
    };
    int frame_count = (int)(sizeof(frames) / sizeof(frames[0]));

    for (int f = 0; f < frame_count; f++) {
        const FrameInput *input = &frames[f];

        SDL_Log("%s", "");
        SDL_Log("--- Frame %d: %s ---", f, input->description);
        SDL_Log("  Input: mouse=(%.0f, %.0f) button=%s",
                (double)input->mouse_x, (double)input->mouse_y,
                input->mouse_down ? "DOWN" : "UP");

        forge_ui_ctx_begin(&ctx, input->mouse_x, input->mouse_y,
                           input->mouse_down);

        declare_settings_panel_layout(&ctx, &vsync_on, &fullscreen_on,
                                      &aa_on, &volume);

        /* Status label below the panel */
        {
            float ascender_px = 0.0f;
            if (atlas.units_per_em > 0) {
                float scale = atlas.pixel_height / (float)atlas.units_per_em;
                ascender_px = (float)atlas.ascender * scale;
            }

            static char status_buf[128];
            SDL_snprintf(status_buf, sizeof(status_buf),
                         "vsync=%s  fs=%s  aa=%s  vol=%.0f",
                         vsync_on ? "ON" : "OFF",
                         fullscreen_on ? "ON" : "OFF",
                         aa_on ? "ON" : "OFF",
                         (double)volume);

            forge_ui_ctx_label(&ctx, status_buf,
                               PANEL_X, PANEL_Y + PANEL_H + STATUS_LABEL_GAP + ascender_px,
                               STATUS_R, STATUS_G, STATUS_B, STATUS_A);
        }

        forge_ui_ctx_end(&ctx);

        /* Log state */
        SDL_Log("  State: hot=%u  active=%u",
                (unsigned)ctx.hot, (unsigned)ctx.active);
        SDL_Log("  Values: vsync=%s  fullscreen=%s  aa=%s  volume=%.1f",
                vsync_on ? "ON" : "OFF",
                fullscreen_on ? "ON" : "OFF",
                aa_on ? "ON" : "OFF",
                (double)volume);
        SDL_Log("  Draw data: %d vertices, %d indices (%d triangles)",
                ctx.vertex_count, ctx.index_count, ctx.index_count / 3);

        /* Render to BMP */
        char bmp_path[64];
        SDL_snprintf(bmp_path, sizeof(bmp_path), "layout_frame_%d.bmp", f);

        if (render_frame_bmp(bmp_path, &ctx, &atlas,
                             input->mouse_x, input->mouse_y)) {
            SDL_Log("  -> %s", bmp_path);
        } else {
            SDL_Log("  [!] Failed to write %s", bmp_path);
            had_render_error = true;
        }
    }

    /* ══════════════════════════════════════════════════════════════════════ */
    /* ── Summary ─────────────────────────────────────────────────────────── */
    /* ══════════════════════════════════════════════════════════════════════ */
    SDL_Log("%s", "");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("SUMMARY");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("%s", "");
    SDL_Log("  Layout system:");
    SDL_Log("    - ForgeUiLayout: rect, direction, padding, spacing, cursor");
    SDL_Log("    - Stack-based: push/pop for nested layouts (max depth %d)",
            FORGE_UI_LAYOUT_MAX_DEPTH);
    SDL_Log("    - layout_next(): returns next widget rect, advances cursor");
    SDL_Log("%s", "");
    SDL_Log("  Layout directions:");
    SDL_Log("    - VERTICAL:   full width, caller-specified height, cursor moves down");
    SDL_Log("    - HORIZONTAL: full height, caller-specified width, cursor moves right");
    SDL_Log("%s", "");
    SDL_Log("  Layout-aware widgets:");
    SDL_Log("    - label_layout():    text at layout position");
    SDL_Log("    - button_layout():   button with auto rect");
    SDL_Log("    - checkbox_layout(): checkbox with auto rect");
    SDL_Log("    - slider_layout():   slider with auto rect");
    SDL_Log("%s", "");
    SDL_Log("  Nesting example (settings panel):");
    SDL_Log("    push(vertical)    -- outer panel");
    SDL_Log("      label_layout()  -- title");
    SDL_Log("      checkbox_layout() x3");
    SDL_Log("      layout_next()   -- reserve button row rect");
    SDL_Log("      push(horizontal) -- button row");
    SDL_Log("        button_layout() x2");
    SDL_Log("      pop()");
    SDL_Log("      slider_layout()");
    SDL_Log("    pop()");
    SDL_Log("%s", "");
    if (counts_match) {
        SDL_Log("  Comparison: manual vs layout produced identical draw data");
        SDL_Log("    (%d vertices, %d indices)", manual_verts, manual_idxs);
    } else {
        SDL_Log("  Comparison: MISMATCH — manual=%d/%d  layout=%d/%d",
                manual_verts, manual_idxs,
                layout_verts, layout_idxs);
    }
    SDL_Log("%s", SEPARATOR);
    SDL_Log("Done. Output files written to the current directory.");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return had_render_error ? 1 : 0;
}
