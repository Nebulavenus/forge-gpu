/*
 * UI Lesson 11 -- Widget ID System
 *
 * Demonstrates: FNV-1a hashed string IDs with hierarchical scope stacking.
 *
 * The old integer ID system required callers to manually assign unique
 * numeric IDs and reserved hidden ID ranges (id+1 for scrollbar, id+2
 * for collapse toggle), creating invisible collision zones.  The new
 * system uses FNV-1a string hashing with hierarchical scope stacking:
 *
 *   - Widget labels double as IDs (hashed automatically)
 *   - The "##" separator lets callers disambiguate widgets with the
 *     same display text: "Delete##item_1" displays "Delete" but hashes
 *     "##item_1"
 *   - Panels and windows push ID scopes automatically, so identical
 *     labels in different containers produce different hashes
 *
 * This program:
 *   1. Loads a TrueType font and builds a font atlas
 *   2. Initializes a ForgeUiContext and ForgeUiWindowContext
 *   3. Creates two windows with identically-labeled widgets:
 *      - "Audio" window: "Enable" checkbox, "Verbose" checkbox
 *      - "Video" window: "Enable" checkbox, "Verbose" checkbox
 *      (scoping prevents collision -- same labels, different hashes)
 *   4. Demonstrates ## disambiguation: two "Delete" buttons via
 *      "Delete##audio" and "Delete##video" in the same scope
 *   5. Prints hash values showing different IDs for same-label widgets
 *   6. Simulates mouse interactions across ~8 frames
 *   7. Renders each frame with the software rasterizer, writes BMP
 *
 * Output images show the two windows with identical widgets that
 * operate independently thanks to scope-based ID hashing.
 *
 * This is a console program -- no GPU or window is needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "ui/forge_ui.h"
#include "ui/forge_ui_window.h"
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
#define FB_WIDTH   720  /* output image width in pixels */
#define FB_HEIGHT  480  /* output image height in pixels */

/* ── Window layout constants ─────────────────────────────────────────────── */

/* Audio window -- left side */
#define AUDIO_X      30    /* audio window left edge */
#define AUDIO_Y      30    /* audio window top edge */
#define AUDIO_W     260    /* audio window width */
#define AUDIO_H     220    /* audio window height */

/* Video window -- right side */
#define VIDEO_X     350    /* video window left edge */
#define VIDEO_Y      30    /* video window top edge */
#define VIDEO_W     260    /* video window width */
#define VIDEO_H     220    /* video window height */

/* ── Widget dimensions ──────────────────────────────────────────────────── */
#define CHECKBOX_HEIGHT   28.0f   /* height of each checkbox row */

/* ── Delete button layout (outside any window, at bottom of screen) ────── */
#define DELETE_BTN_HEIGHT  36.0f  /* delete button height */
#define DELETE_BTN_WIDTH  100.0f  /* delete button width */
#define DELETE_BTN_Y      380.0f  /* delete buttons vertical position */
#define DELETE_BTN_A_X    150.0f  /* "Delete##audio" horizontal position */
#define DELETE_BTN_B_X    400.0f  /* "Delete##video" horizontal position */

/* ── Background clear color (dark slate) ─────────────────────────────────── */
#define BG_CLEAR_R      0.08f
#define BG_CLEAR_G      0.08f
#define BG_CLEAR_B      0.12f
#define BG_CLEAR_A      1.00f

/* ── Mouse cursor dot ────────────────────────────────────────────────────── */
#define CURSOR_DOT_RADIUS     2
#define CURSOR_DOT_RADIUS_SQ  5
#define CURSOR_DOT_R    255
#define CURSOR_DOT_G    220
#define CURSOR_DOT_B     50
#define CURSOR_DOT_A    255

/* ── Idle cursor position margin ─────────────────────────────────────────── */
#define IDLE_CURSOR_MARGIN  20.0f  /* pixels from edge for idle cursor */

/* ── Checkbox click nudge ────────────────────────────────────────────────── */
#define CB_CLICK_NUDGE  10.0f  /* horizontal offset into checkbox hit area */

/* ── Simulated frame input ───────────────────────────────────────────────── */

typedef struct FrameInput {
    float       mouse_x;        /* simulated cursor x in screen pixels */
    float       mouse_y;        /* simulated cursor y in screen pixels */
    bool        mouse_down;     /* true if the primary button is held */
    float       scroll_delta;   /* mouse wheel delta (positive = scroll down) */
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
        .pixels = atlas->pixels,
        .width  = atlas->width,
        .height = atlas->height
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

/* ── Helper: declare both windows and the Delete buttons ─────────────────── */

static void declare_ui(ForgeUiWindowContext *wctx,
                       ForgeUiWindowState *audio_win,
                       ForgeUiWindowState *video_win,
                       bool audio_checkboxes[2],
                       bool video_checkboxes[2])
{
    ForgeUiContext *ctx = wctx->ctx;

    /* Audio window */
    if (forge_ui_wctx_window_begin(wctx, "Audio", audio_win)) {
        (void)forge_ui_ctx_checkbox_layout(ctx, "Enable",
            &audio_checkboxes[0], CHECKBOX_HEIGHT);
        (void)forge_ui_ctx_checkbox_layout(ctx, "Verbose",
            &audio_checkboxes[1], CHECKBOX_HEIGHT);
        forge_ui_wctx_window_end(wctx);
    }

    /* Video window -- SAME labels, but different scope = different IDs */
    if (forge_ui_wctx_window_begin(wctx, "Video", video_win)) {
        (void)forge_ui_ctx_checkbox_layout(ctx, "Enable",
            &video_checkboxes[0], CHECKBOX_HEIGHT);
        (void)forge_ui_ctx_checkbox_layout(ctx, "Verbose",
            &video_checkboxes[1], CHECKBOX_HEIGHT);
        forge_ui_wctx_window_end(wctx);
    }

    /* ## disambiguation demo: two Delete buttons at bottom, outside any window */
    ForgeUiRect del_a_rect = { DELETE_BTN_A_X, DELETE_BTN_Y,
                               DELETE_BTN_WIDTH, DELETE_BTN_HEIGHT };
    ForgeUiRect del_b_rect = { DELETE_BTN_B_X, DELETE_BTN_Y,
                               DELETE_BTN_WIDTH, DELETE_BTN_HEIGHT };

    if (forge_ui_ctx_button(ctx, "Delete##audio", del_a_rect)) {
        SDL_Log("  [ACTION] Delete audio config");
    }
    if (forge_ui_ctx_button(ctx, "Delete##video", del_b_rect)) {
        SDL_Log("  [ACTION] Delete video config");
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

    SDL_Log("UI Lesson 11 -- Widget ID System");
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

    ForgeUiWindowContext wctx;
    if (!forge_ui_wctx_init(&wctx, &ctx)) {
        SDL_Log("Failed to initialize window context");
        forge_ui_ctx_free(&ctx);
        forge_ui_atlas_free(&atlas);
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    SDL_Log("  Window context initialized (max %d windows)",
            FORGE_UI_WINDOW_MAX);

    /* ── ID hash demonstration ────────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("ID HASH DEMONSTRATION");
    SDL_Log("%s", THIN_SEP);

    /* Hash "Enable" in the Audio window scope */
    forge_ui_push_id(&ctx, "Audio");
    Uint32 audio_enable_id  = forge_ui_hash_id(&ctx, "Enable");
    Uint32 audio_verbose_id = forge_ui_hash_id(&ctx, "Verbose");
    forge_ui_pop_id(&ctx);

    /* Hash "Enable" in the Video window scope */
    forge_ui_push_id(&ctx, "Video");
    Uint32 video_enable_id  = forge_ui_hash_id(&ctx, "Enable");
    Uint32 video_verbose_id = forge_ui_hash_id(&ctx, "Verbose");
    forge_ui_pop_id(&ctx);

    /* Hash ## disambiguation */
    Uint32 del_audio_id = forge_ui_hash_id(&ctx, "Delete##audio");
    Uint32 del_video_id = forge_ui_hash_id(&ctx, "Delete##video");

    SDL_Log("  Audio/Enable  hash: 0x%08X", (unsigned)audio_enable_id);
    SDL_Log("  Video/Enable  hash: 0x%08X", (unsigned)video_enable_id);
    SDL_Log("  Audio/Verbose hash: 0x%08X", (unsigned)audio_verbose_id);
    SDL_Log("  Video/Verbose hash: 0x%08X", (unsigned)video_verbose_id);
    SDL_Log("  Delete##audio hash: 0x%08X", (unsigned)del_audio_id);
    SDL_Log("  Delete##video hash: 0x%08X", (unsigned)del_video_id);
    SDL_Log("%s", "");
    SDL_Log("  Same labels, different scopes -> different IDs!");
    SDL_Log("  ## suffix -> different IDs for same display text!");

    /* ── Application-owned widget state ───────────────────────────────── */
    bool audio_cbs[2] = { true, false };   /* Enable=on, Verbose=off */
    bool video_cbs[2] = { false, true };   /* Enable=off, Verbose=on */

    /* Window states (application-owned, persist across frames) */
    ForgeUiWindowState audio_win = {
        .rect = { AUDIO_X, AUDIO_Y, AUDIO_W, AUDIO_H },
        .scroll_y = 0.0f,
        .collapsed = false,
        .z_order = 0
    };
    ForgeUiWindowState video_win = {
        .rect = { VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H },
        .scroll_y = 0.0f,
        .collapsed = false,
        .z_order = 1
    };

    bool had_render_error = false;

    /* ── Cursor positions for simulated interactions ──────────────────── */
    float idle_mx = FB_WIDTH - IDLE_CURSOR_MARGIN;
    float idle_my = IDLE_CURSOR_MARGIN;

    /* Checkbox click positions:
     * The first checkbox in each window is just below the title bar. */
    float audio_cb_x = AUDIO_X + FORGE_UI_WIN_PADDING + CB_CLICK_NUDGE;
    float audio_enable_cy = AUDIO_Y + FORGE_UI_WIN_TITLE_HEIGHT
                            + FORGE_UI_WIN_PADDING + CHECKBOX_HEIGHT * 0.5f;

    float video_cb_x = VIDEO_X + FORGE_UI_WIN_PADDING + CB_CLICK_NUDGE;
    float video_enable_cy = VIDEO_Y + FORGE_UI_WIN_TITLE_HEIGHT
                            + FORGE_UI_WIN_PADDING + CHECKBOX_HEIGHT * 0.5f;

    /* Delete##audio button center */
    float del_a_cx = DELETE_BTN_A_X + DELETE_BTN_WIDTH * 0.5f;
    float del_a_cy = DELETE_BTN_Y + DELETE_BTN_HEIGHT * 0.5f;

    /* ── Simulated frames ─────────────────────────────────────────────── */

    SDL_Log("%s", SEPARATOR);
    SDL_Log("SIMULATING FRAMES WITH WIDGET ID SYSTEM");
    SDL_Log("%s", SEPARATOR);

    FrameInput frames[] = {
        /* Frame 0: Initial view -- cursor idle top-right */
        { idle_mx, idle_my, false, 0.0f,
          "Initial view -- two windows with identical labels" },

        /* Frame 1: Hover over Audio "Enable" checkbox */
        { audio_cb_x, audio_enable_cy, false, 0.0f,
          "Hover over Audio/Enable checkbox" },

        /* Frame 2: Press down on Audio "Enable" checkbox */
        { audio_cb_x, audio_enable_cy, true, 0.0f,
          "Press Audio/Enable checkbox (currently ON)" },

        /* Frame 3: Release -- toggle Audio Enable off */
        { audio_cb_x, audio_enable_cy, false, 0.0f,
          "Release -- Audio/Enable toggles OFF, Video/Enable unchanged" },

        /* Frame 4: Hover over Video "Enable" checkbox */
        { video_cb_x, video_enable_cy, false, 0.0f,
          "Hover over Video/Enable checkbox" },

        /* Frame 5: Press down on Video "Enable" checkbox */
        { video_cb_x, video_enable_cy, true, 0.0f,
          "Press Video/Enable checkbox (currently OFF)" },

        /* Frame 6: Release -- toggle Video Enable on */
        { video_cb_x, video_enable_cy, false, 0.0f,
          "Release -- Video/Enable toggles ON, Audio/Enable unchanged" },

        /* Frame 7: Press Delete##audio button */
        { del_a_cx, del_a_cy, true, 0.0f,
          "Press Delete##audio button" },

        /* Frame 8: Release Delete##audio -- click fires */
        { del_a_cx, del_a_cy, false, 0.0f,
          "Release Delete##audio -- action fires" },
    };
    int frame_count = (int)(sizeof(frames) / sizeof(frames[0]));

    for (int f = 0; f < frame_count; f++) {
        const FrameInput *input = &frames[f];

        SDL_Log("%s", "");
        SDL_Log("--- Frame %d: %s ---", f, input->description);
        SDL_Log("  Input: mouse=(%.0f, %.0f) button=%s scroll_delta=%.1f",
                (double)input->mouse_x, (double)input->mouse_y,
                input->mouse_down ? "DOWN" : "UP",
                (double)input->scroll_delta);

        forge_ui_ctx_begin(&ctx, input->mouse_x, input->mouse_y,
                           input->mouse_down);
        ctx.scroll_delta = input->scroll_delta;

        forge_ui_wctx_begin(&wctx);

        declare_ui(&wctx, &audio_win, &video_win, audio_cbs, video_cbs);

        forge_ui_wctx_end(&wctx);
        forge_ui_ctx_end(&ctx);

        /* Log state */
        SDL_Log("  State: hot=%u  active=%u  hovered_window=%u",
                (unsigned)ctx.hot, (unsigned)ctx.active,
                (unsigned)wctx.hovered_window_id);
        SDL_Log("  Z-orders: Audio=%d  Video=%d",
                audio_win.z_order, video_win.z_order);
        SDL_Log("  Checkboxes: Audio=[Enable=%s Verbose=%s]  "
                "Video=[Enable=%s Verbose=%s]",
                audio_cbs[0] ? "ON" : "OFF",
                audio_cbs[1] ? "ON" : "OFF",
                video_cbs[0] ? "ON" : "OFF",
                video_cbs[1] ? "ON" : "OFF");
        SDL_Log("  Draw data: %d vertices, %d indices (%d triangles)",
                ctx.vertex_count, ctx.index_count, ctx.index_count / 3);

        /* ── Render to BMP ─────────────────────────────────────────────── */
        static char bmp_path[256];
        SDL_snprintf(bmp_path, sizeof(bmp_path),
                     "widget_id_frame_%d.bmp", f);

        if (!render_frame_bmp(bmp_path, &ctx, &atlas,
                              input->mouse_x, input->mouse_y)) {
            SDL_Log("  [!] Failed to write %s", bmp_path);
            had_render_error = true;
        } else {
            SDL_Log("  -> wrote %s", bmp_path);
        }
    }

    /* ── Summary ──────────────────────────────────────────────────────── */
    SDL_Log("%s", "");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("SUMMARY");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  Total frames rendered: %d", frame_count);
    SDL_Log("  Final checkbox state:");
    SDL_Log("    Audio: Enable=%s  Verbose=%s",
            audio_cbs[0] ? "ON" : "OFF",
            audio_cbs[1] ? "ON" : "OFF");
    SDL_Log("    Video: Enable=%s  Verbose=%s",
            video_cbs[0] ? "ON" : "OFF",
            video_cbs[1] ? "ON" : "OFF");
    SDL_Log("  Key observation: identical labels in different windows");
    SDL_Log("  operate independently thanks to scope-based ID hashing.");

    if (had_render_error) {
        SDL_Log("  [!] Some frames failed to render");
    } else {
        SDL_Log("  All frames rendered successfully");
    }

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    forge_ui_wctx_free(&wctx);
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    forge_ui_ttf_free(&font);
    SDL_Quit();

    return had_render_error ? 1 : 0;
}
