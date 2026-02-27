/*
 * UI Lesson 06 -- Checkboxes and Sliders
 *
 * Demonstrates: External mutable state and drag interaction with the
 * hot/active state machine from lesson 05.  Checkboxes toggle a bool*
 * on click; sliders introduce drag interaction where active persists
 * as the mouse moves outside the widget bounds.
 *
 * This program:
 *   1. Loads a TrueType font and builds a font atlas
 *   2. Initializes a ForgeUiContext for immediate-mode widget declaration
 *   3. Simulates 9 frames of mouse input (hover, click, toggle, drag)
 *   4. Each frame: declares checkboxes, sliders, and labels, generates
 *      vertex/index data, renders with the software rasterizer, and
 *      writes a BMP image
 *
 * Output images show a checkbox being hovered, clicked, and toggled,
 * followed by a slider being clicked (value snap), dragged right, dragged
 * past the track edge (clamped to max), and released.  A yellow dot shows
 * the simulated cursor position in each frame.
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
#define PIXEL_HEIGHT     28.0f  /* render glyphs at 28 pixels tall */
#define ATLAS_PADDING    1      /* 1 pixel padding between glyphs */
#define ASCII_START      32     /* first printable ASCII codepoint (space) */
#define ASCII_END        126    /* last printable ASCII codepoint (tilde) */
#define ASCII_COUNT      (ASCII_END - ASCII_START + 1)  /* 95 glyphs */

/* ── Framebuffer dimensions ──────────────────────────────────────────────── */
#define FB_WIDTH   480  /* output image width in pixels */
#define FB_HEIGHT  300  /* output image height in pixels */

/* ── UI layout constants ─────────────────────────────────────────────────── */
#define MARGIN          20.0f   /* pixels of margin around the UI */
#define LABEL_OFFSET_Y  24.0f   /* vertical offset for title label baseline */
#define TITLE_GAP       16.0f   /* gap between title and first widget */
#define WIDGET_HEIGHT   28.0f   /* height of checkbox and slider widget rects */
#define WIDGET_SPACING  14.0f   /* vertical gap between widgets */

/* Checkbox layout */
#define CB_WIDTH       200.0f   /* total checkbox widget width (box + label) */

/* Slider layout */
#define SL_LABEL_GAP    10.0f   /* gap between slider name label and track */
#define SL_TRACK_WIDTH 200.0f   /* slider track width in pixels */
#define SL_VALUE_GAP    12.0f   /* gap between track right edge and value text */

/* Slider value range */
#define SL_MIN           0.0f   /* minimum volume */
#define SL_MAX         100.0f   /* maximum volume */
#define SL_INITIAL      50.0f   /* initial volume value */

/* ── Widget IDs ──────────────────────────────────────────────────────────── */
/* Each interactive widget needs a unique non-zero ID for the hot/active
 * state machine.  In a real application these could be generated from
 * hashed strings or a counter. */
#define CB_AUDIO_ID      1
#define SL_VOLUME_ID     2

/* ── Background clear color (dark slate, same as lesson 05) ──────────────── */
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

/* ── Slider name/value label color (dim gray) ────────────────────────────── */
#define SL_LABEL_R      0.75f
#define SL_LABEL_G      0.75f
#define SL_LABEL_B      0.80f
#define SL_LABEL_A      1.00f

/* ── Mouse cursor dot ────────────────────────────────────────────────────── */
#define CURSOR_DOT_RADIUS_SQ  5   /* squared pixel radius for circular shape */
#define CURSOR_DOT_R    255       /* red channel (uint8) */
#define CURSOR_DOT_G    220       /* green channel (uint8) */
#define CURSOR_DOT_B     50       /* blue channel (uint8) */
#define CURSOR_DOT_A    255       /* alpha channel (uint8) */

/* ── Simulated frame input ───────────────────────────────────────────────── */

/* Each frame specifies a mouse position and button state.  These simulate
 * a user hovering, clicking a checkbox, then dragging a slider. */
typedef struct FrameInput {
    float mouse_x;            /* simulated cursor x in screen pixels */
    float mouse_y;            /* simulated cursor y in screen pixels */
    bool  mouse_down;         /* true if the primary button is held this frame */
    const char *description;  /* what this frame demonstrates (for logging) */
} FrameInput;

/* ── Helper: state name for logging ──────────────────────────────────────── */

static const char *state_name(const ForgeUiContext *ctx, Uint32 id)
{
    if (ctx->active == id) return "ACTIVE (pressed)";
    if (ctx->hot == id)    return "HOT (hovered)";
    return "normal";
}

/* ── Helper: render a frame's draw data to BMP ───────────────────────────── */

static bool render_frame_bmp(const char *path,
                             const ForgeUiContext *ctx,
                             const ForgeUiFontAtlas *atlas,
                             float mouse_x, float mouse_y)
{
    /* Create framebuffer */
    ForgeRasterBuffer fb = forge_raster_buffer_create(FB_WIDTH, FB_HEIGHT);
    if (!fb.pixels) {
        SDL_Log("  [!] Failed to create framebuffer");
        return false;
    }

    /* Clear to dark background */
    forge_raster_clear(&fb, BG_CLEAR_R, BG_CLEAR_G, BG_CLEAR_B, BG_CLEAR_A);

    /* Set up the atlas as a raster texture.  ForgeRasterVertex matches
     * ForgeUiVertex in memory layout, so we can cast the vertex pointer
     * directly. */
    ForgeRasterTexture tex = {
        atlas->pixels,
        atlas->width,
        atlas->height
    };

    /* Draw all UI triangles in one batch */
    forge_raster_triangles_indexed(
        &fb,
        (const ForgeRasterVertex *)ctx->vertices,
        ctx->vertex_count,
        ctx->indices,
        ctx->index_count,
        &tex
    );

    /* Draw a small crosshair at the mouse position for visualization.
     * We draw a 5x5 yellow dot using direct pixel writes. */
    int mx = (int)(mouse_x + 0.5f);
    int my = (int)(mouse_y + 0.5f);
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            /* Circular shape: skip corners */
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

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    const char *font_path = (argc > 1) ? argv[1] : DEFAULT_FONT_PATH;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("UI Lesson 06 -- Checkboxes and Sliders");
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
    SDL_Log("  White UV: (%.4f, %.4f) - (%.4f, %.4f)",
            (double)atlas.white_uv.u0, (double)atlas.white_uv.v0,
            (double)atlas.white_uv.u1, (double)atlas.white_uv.v1);

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

    SDL_Log("  Initial vertex capacity: %d", ctx.vertex_capacity);
    SDL_Log("  Initial index capacity:  %d", ctx.index_capacity);
    SDL_Log("  hot = %u, active = %u", (unsigned)ctx.hot, (unsigned)ctx.active);

    /* ── Compute font metrics for baseline positioning ────────────────── */
    float scale = 0.0f;
    float ascender_px = 0.0f;
    if (atlas.units_per_em > 0) {
        scale = atlas.pixel_height / (float)atlas.units_per_em;
        ascender_px = (float)atlas.ascender * scale;
    }

    /* ── Define widget layout ─────────────────────────────────────────── */

    /* Vertical layout: title, then checkbox, then slider label + track */
    float cursor_y = MARGIN + LABEL_OFFSET_Y + TITLE_GAP;

    /* Checkbox: "Enable Audio" */
    ForgeUiRect cb_rect = {
        MARGIN, cursor_y, CB_WIDTH, WIDGET_HEIGHT
    };
    cursor_y += WIDGET_HEIGHT + WIDGET_SPACING;

    /* Slider label position (baseline y) */
    float sl_label_y = cursor_y + ascender_px;
    cursor_y += WIDGET_HEIGHT;

    /* Slider track area */
    ForgeUiRect sl_rect = {
        MARGIN, cursor_y, SL_TRACK_WIDTH, WIDGET_HEIGHT
    };

    /* Status label position */
    float status_y = sl_rect.y + sl_rect.h + WIDGET_SPACING + WIDGET_SPACING;

    /* ── External mutable state ───────────────────────────────────────── */
    /* These variables live in the application, not the UI library.
     * The checkbox and slider widgets take pointers to them and modify
     * them on interaction.  This is the core principle of external
     * mutable state in immediate-mode UI: the library writes back
     * into your data. */
    bool  audio_enabled = false;
    float volume = SL_INITIAL;

    /* ── Define simulated frames ──────────────────────────────────────── */
    /* These frames walk through the key interactions: checkbox toggle
     * cycle, then slider click, drag, edge clamping, and release. */
    float cb_center_x = cb_rect.x + cb_rect.w * 0.3f;
    float cb_center_y = cb_rect.y + cb_rect.h * 0.5f;
    float sl_center_y = sl_rect.y + sl_rect.h * 0.5f;

    FrameInput frames[] = {
        /* Frame 0: Mouse away from all widgets -- everything normal */
        { 420.0f, 50.0f, false,
          "Mouse away from widgets -- all normal" },

        /* Frame 1: Mouse hovers over checkbox -- becomes hot */
        { cb_center_x, cb_center_y, false,
          "Mouse over checkbox -- checkbox becomes hot" },

        /* Frame 2: Mouse pressed on checkbox -- becomes active */
        { cb_center_x, cb_center_y, true,
          "Mouse pressed on checkbox -- checkbox becomes active" },

        /* Frame 3: Mouse released on checkbox -- TOGGLED to true */
        { cb_center_x, cb_center_y, false,
          "Mouse released on checkbox -- TOGGLED (true)" },

        /* Frame 4: Mouse moves to slider track -- slider becomes hot */
        { sl_rect.x + sl_rect.w * 0.25f, sl_center_y, false,
          "Mouse over slider track -- slider becomes hot" },

        /* Frame 5: Mouse pressed on slider at ~25% -- snaps value */
        { sl_rect.x + sl_rect.w * 0.25f, sl_center_y, true,
          "Mouse pressed on slider -- value snaps to 25%" },

        /* Frame 6: Mouse dragged right to ~75% -- value follows */
        { sl_rect.x + sl_rect.w * 0.75f, sl_center_y, true,
          "Mouse dragged right -- value follows to ~75%" },

        /* Frame 7: Mouse dragged past right edge -- value clamped */
        { sl_rect.x + sl_rect.w + 60.0f, sl_center_y, true,
          "Mouse past right edge -- value clamped to max" },

        /* Frame 8: Mouse released -- slider deactivated */
        { sl_rect.x + sl_rect.w + 60.0f, sl_center_y, false,
          "Mouse released -- slider deactivated" },
    };
    int frame_count = (int)(sizeof(frames) / sizeof(frames[0]));

    /* ── Process frames ───────────────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("SIMULATING %d FRAMES", frame_count);
    SDL_Log("%s", SEPARATOR);

    bool had_render_error = false;

    for (int f = 0; f < frame_count; f++) {
        const FrameInput *input = &frames[f];

        SDL_Log("%s", "");
        SDL_Log("--- Frame %d: %s ---", f, input->description);
        SDL_Log("  Input: mouse=(%.0f, %.0f) button=%s",
                (double)input->mouse_x, (double)input->mouse_y,
                input->mouse_down ? "DOWN" : "UP");

        /* Begin frame: update input, reset draw buffers */
        forge_ui_ctx_begin(&ctx, input->mouse_x, input->mouse_y,
                           input->mouse_down);

        /* ── Declare widgets ──────────────────────────────────────────── */

        /* Title label */
        forge_ui_ctx_label(&ctx, "Checkboxes & Sliders",
                           MARGIN, MARGIN + ascender_px,
                           TITLE_R, TITLE_G, TITLE_B, TITLE_A);

        /* Checkbox: "Enable Audio" */
        bool cb_toggled = forge_ui_ctx_checkbox(
            &ctx, CB_AUDIO_ID, "Enable Audio", &audio_enabled, cb_rect);

        /* Slider name label: "Volume:" */
        forge_ui_ctx_label(&ctx, "Volume:", MARGIN, sl_label_y,
                           SL_LABEL_R, SL_LABEL_G, SL_LABEL_B, SL_LABEL_A);

        /* Slider: volume control */
        bool sl_changed = forge_ui_ctx_slider(
            &ctx, SL_VOLUME_ID, &volume, SL_MIN, SL_MAX, sl_rect);

        /* Value label: show current slider value to the right of the track.
         * This is the "optional value label" draw element -- we format the
         * float as text and render it with the same atlas and vertex format. */
        char val_str[32];
        SDL_snprintf(val_str, sizeof(val_str), "%.1f", (double)volume);
        forge_ui_ctx_label(&ctx, val_str,
                           sl_rect.x + sl_rect.w + SL_VALUE_GAP,
                           sl_rect.y + (sl_rect.h - atlas.pixel_height) * 0.5f
                               + ascender_px,
                           SL_LABEL_R, SL_LABEL_G, SL_LABEL_B, SL_LABEL_A);

        /* Status label */
        const char *status = "Hover and click to interact";
        static char status_buf[64];
        if (cb_toggled) {
            SDL_snprintf(status_buf, sizeof(status_buf),
                         "Audio %s!", audio_enabled ? "enabled" : "disabled");
            status = status_buf;
        } else if (sl_changed) {
            SDL_snprintf(status_buf, sizeof(status_buf),
                         "Volume -> %.1f", (double)volume);
            status = status_buf;
        }

        forge_ui_ctx_label(&ctx, status, MARGIN, status_y + ascender_px,
                           STATUS_R, STATUS_G, STATUS_B, STATUS_A);

        /* End frame: finalize hot/active transitions */
        forge_ui_ctx_end(&ctx);

        /* ── Log state ────────────────────────────────────────────────── */
        SDL_Log("  State after frame:");
        SDL_Log("    hot    = %u", (unsigned)ctx.hot);
        SDL_Log("    active = %u", (unsigned)ctx.active);
        SDL_Log("    Checkbox (id=%u): %s  value=%s%s",
                (unsigned)CB_AUDIO_ID, state_name(&ctx, CB_AUDIO_ID),
                audio_enabled ? "true" : "false",
                cb_toggled ? " -> TOGGLED" : "");
        SDL_Log("    Slider  (id=%u): %s  value=%.1f%s",
                (unsigned)SL_VOLUME_ID, state_name(&ctx, SL_VOLUME_ID),
                (double)volume,
                sl_changed ? " -> CHANGED" : "");
        SDL_Log("  Draw data: %d vertices, %d indices (%d triangles)",
                ctx.vertex_count, ctx.index_count, ctx.index_count / 3);

        /* ── Render to BMP ────────────────────────────────────────────── */
        char bmp_path[64];
        SDL_snprintf(bmp_path, sizeof(bmp_path), "controls_frame_%d.bmp", f);

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
    SDL_Log("  Checkbox interaction:");
    SDL_Log("    - Same hot/active state machine as buttons");
    SDL_Log("    - Takes bool *value, flips it on click");
    SDL_Log("    - Draw: outer box (white_uv) + inner fill (if checked) + label");
    SDL_Log("    - Hit test covers entire widget rect (box + label area)");
    SDL_Log("%s", "");
    SDL_Log("  Slider interaction:");
    SDL_Log("    - Active persists while mouse is held (drag interaction)");
    SDL_Log("    - Value tracks mouse_x regardless of cursor position");
    SDL_Log("    - t = clamp((mouse_x - track_x) / track_w, 0, 1)");
    SDL_Log("    - value = min + t * (max - min)");
    SDL_Log("    - Clicking anywhere on track snaps value and begins drag");
    SDL_Log("    - Draw: track (white_uv) + thumb (white_uv, color by state)");
    SDL_Log("%s", "");
    SDL_Log("  Shared patterns:");
    SDL_Log("    - External mutable state: widgets write to caller's variables");
    SDL_Log("    - Same ForgeUiVertex format and atlas texture as lesson 05");
    SDL_Log("    - All widgets rendered in one draw call");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("Done. Output files written to the current directory.");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return had_render_error ? 1 : 0;
}
