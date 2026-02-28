/*
 * UI Lesson 10 -- Windows
 *
 * Demonstrates: Draggable windows with z-ordering and collapse/expand.
 *
 * A window is a panel (lesson 09) that can be dragged by its title bar,
 * reordered in depth by clicking, and collapsed to show only the title
 * bar.  This lesson introduces ForgeUiWindowState as persistent
 * application-owned state and ForgeUiWindowContext as the wrapper that
 * adds deferred draw ordering and input routing by z-order.
 *
 * This program:
 *   1. Loads a TrueType font and builds a font atlas
 *   2. Initializes a ForgeUiContext and ForgeUiWindowContext
 *   3. Creates three overlapping windows:
 *      - Settings: checkboxes and a slider
 *      - Status: labels showing current values
 *      - Info: scrollable text content
 *   4. Simulates ~12 frames demonstrating:
 *      - Initial arrangement with all three visible
 *      - Click Settings title bar to bring to front (z reorder)
 *      - Drag Settings window to new position (grab offset in action)
 *      - Collapse Info window (content disappears, only title bar)
 *      - Expand Info window again
 *      - Scroll content in Info window
 *      - Interact with a checkbox in Settings while it overlaps Status
 *        (verifies input routing respects z-order)
 *   5. Each frame: declares widgets inside windows, generates vertex/index
 *      data, renders with the software rasterizer, writes a BMP image
 *
 * Output images show three overlapping windows with various interactions.
 * A yellow dot shows the simulated cursor position.
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

/* Settings window -- starts at top-left */
#define SETTINGS_X      30.0f
#define SETTINGS_Y      30.0f
#define SETTINGS_W     260.0f
#define SETTINGS_H     280.0f

/* Status window -- overlaps Settings */
#define STATUS_X       200.0f
#define STATUS_Y        80.0f
#define STATUS_W       260.0f
#define STATUS_H       200.0f

/* Info window -- bottom right area */
#define INFO_X         380.0f
#define INFO_Y          40.0f
#define INFO_W         300.0f
#define INFO_H         300.0f

/* ── Widget dimensions ──────────────────────────────────────────────────── */
#define CHECKBOX_HEIGHT   28.0f   /* height of each checkbox row */
#define SLIDER_HEIGHT     30.0f   /* height of slider widget */
#define LABEL_HEIGHT      26.0f   /* height of each label row */

/* ── Slider demo range ─────────────────────────────────────────────────── */
#define SLIDER_MIN        0.0f    /* slider minimum value */
#define SLIDER_MAX      100.0f    /* slider maximum value */
#define SLIDER_INITIAL   50.0f    /* slider starting value */

/* ── Simulated scroll amount ───────────────────────────────────────────── */
#define SCROLL_STEP       2.0f    /* mouse wheel delta per simulated scroll */

/* ── Idle cursor position margin ───────────────────────────────────────── */
#define IDLE_CURSOR_MARGIN  20.0f  /* pixels from edge for idle cursor */

/* ── Checkbox click nudge ──────────────────────────────────────────────── */
#define CB_CLICK_NUDGE   10.0f    /* horizontal offset into checkbox hit area */

/* ── Drag displacement (how far Settings is dragged in the demo) ─────── */
#define DRAG_OFFSET_X  150.0f    /* horizontal drag displacement (pixels) */
#define DRAG_OFFSET_Y   80.0f    /* vertical drag displacement (pixels) */

/* ── Widget IDs ──────────────────────────────────────────────────────────── */
/* Settings window: ID 100, scrollbar 101, collapse toggle 102 */
#define ID_SETTINGS_WIN     100
/* Status window: ID 200, scrollbar 201, collapse toggle 202 */
#define ID_STATUS_WIN       200
/* Info window: ID 300, scrollbar 301, collapse toggle 302 */
#define ID_INFO_WIN         300

/* Checkbox IDs: 110-114 for 5 checkboxes */
#define ID_CB_BASE          110
#define CHECKBOX_COUNT      5

/* Slider ID */
#define ID_SLIDER           120

/* ── Label colors ────────────────────────────────────────────────────────── */
#define LABEL_R         0.80f
#define LABEL_G         0.85f
#define LABEL_B         0.90f
#define LABEL_A         1.00f

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

/* ── Simulated frame input ───────────────────────────────────────────────── */

typedef struct FrameInput {
    float       mouse_x;        /* simulated cursor x in screen pixels */
    float       mouse_y;        /* simulated cursor y in screen pixels */
    bool        mouse_down;     /* true if the primary button is held */
    float       scroll_delta;   /* mouse wheel delta (positive = scroll down) */
    const char *description;    /* what this frame demonstrates (for logging) */
} FrameInput;

/* ── Checkbox label names ────────────────────────────────────────────────── */

static const char *checkbox_labels[CHECKBOX_COUNT] = {
    "V-Sync",
    "Fullscreen",
    "Anti-aliasing",
    "Shadows",
    "Bloom"
};

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

/* ── Helper: declare all three windows ───────────────────────────────────── */

static void declare_windows(ForgeUiWindowContext *wctx,
                            ForgeUiWindowState *settings_win,
                            ForgeUiWindowState *status_win,
                            ForgeUiWindowState *info_win,
                            bool checkboxes[CHECKBOX_COUNT],
                            float *slider_val)
{
    ForgeUiContext *ctx = wctx->ctx;

    /* ── Settings window: checkboxes + slider ────────────────────────── */
    if (forge_ui_wctx_window_begin(wctx, ID_SETTINGS_WIN, "Settings",
                                    settings_win)) {
        for (int i = 0; i < CHECKBOX_COUNT; i++) {
            (void)forge_ui_ctx_checkbox_layout(
                ctx, (Uint32)(ID_CB_BASE + i),
                checkbox_labels[i],
                &checkboxes[i], CHECKBOX_HEIGHT);
        }
        (void)forge_ui_ctx_slider_layout(
            ctx, ID_SLIDER, slider_val, SLIDER_MIN, SLIDER_MAX, SLIDER_HEIGHT);
        forge_ui_wctx_window_end(wctx);
    }

    /* ── Status window: labels showing current values ────────────────── */
    if (forge_ui_wctx_window_begin(wctx, ID_STATUS_WIN, "Status",
                                    status_win)) {
        static char buf[128];

        SDL_snprintf(buf, sizeof(buf), "V-Sync: %s",
                     checkboxes[0] ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ctx, buf, LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);

        SDL_snprintf(buf, sizeof(buf), "Fullscreen: %s",
                     checkboxes[1] ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ctx, buf, LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);

        SDL_snprintf(buf, sizeof(buf), "AA: %s",
                     checkboxes[2] ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ctx, buf, LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);

        SDL_snprintf(buf, sizeof(buf), "Slider: %.0f",
                     (double)*slider_val);
        forge_ui_ctx_label_layout(ctx, buf, LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);

        forge_ui_wctx_window_end(wctx);
    }

    /* ── Info window: scrollable text content ────────────────────────── */
    if (forge_ui_wctx_window_begin(wctx, ID_INFO_WIN, "Info",
                                    info_win)) {
        forge_ui_ctx_label_layout(ctx, "Welcome to forge-gpu!",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "This is UI Lesson 10.",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "Windows can be dragged",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "by their title bar.",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "Click to bring to front.",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "Collapse with the toggle.",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "Z-order controls overlap.",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "Scroll for more content.",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "Input respects z-order.",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "Panels become windows.",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "The final container.",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "Build amazing UIs!",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_wctx_window_end(wctx);
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

    SDL_Log("UI Lesson 10 -- Windows");
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

    /* ── Application-owned widget state ───────────────────────────────── */
    bool checkboxes[CHECKBOX_COUNT];
    for (int i = 0; i < CHECKBOX_COUNT; i++) {
        checkboxes[i] = (i % 2 == 0);  /* every other checkbox starts checked */
    }
    float slider_val = SLIDER_INITIAL;

    /* Window states (application-owned, persist across frames) */
    ForgeUiWindowState settings_win = {
        .rect = { SETTINGS_X, SETTINGS_Y, SETTINGS_W, SETTINGS_H },
        .scroll_y = 0.0f,
        .collapsed = false,
        .z_order = 0
    };
    ForgeUiWindowState status_win = {
        .rect = { STATUS_X, STATUS_Y, STATUS_W, STATUS_H },
        .scroll_y = 0.0f,
        .collapsed = false,
        .z_order = 1
    };
    ForgeUiWindowState info_win = {
        .rect = { INFO_X, INFO_Y, INFO_W, INFO_H },
        .scroll_y = 0.0f,
        .collapsed = false,
        .z_order = 2
    };

    bool had_render_error = false;

    /* ── Cursor positions for simulated interactions ──────────────────── */
    float idle_mx = FB_WIDTH - IDLE_CURSOR_MARGIN;
    float idle_my = IDLE_CURSOR_MARGIN;

    /* Settings title bar center for dragging */
    float settings_title_cx = SETTINGS_X + SETTINGS_W * 0.5f;
    float settings_title_cy = SETTINGS_Y + FORGE_UI_WIN_TITLE_HEIGHT * 0.5f;

    /* Info collapse toggle center */
    float info_toggle_cx = INFO_X + FORGE_UI_WIN_TOGGLE_PAD
                           + FORGE_UI_WIN_TOGGLE_SIZE * 0.5f;
    float info_toggle_cy = INFO_Y + FORGE_UI_WIN_TITLE_HEIGHT * 0.5f;

    /* Info content area center for scrolling */
    float info_content_cx = INFO_X + INFO_W * 0.5f;
    float info_content_cy = INFO_Y + FORGE_UI_WIN_TITLE_HEIGHT + INFO_H * 0.3f;

    /* Checkbox click position in Settings (first checkbox) */
    float cb_click_x = SETTINGS_X + FORGE_UI_WIN_PADDING + CB_CLICK_NUDGE;
    float cb_click_y = SETTINGS_Y + FORGE_UI_WIN_TITLE_HEIGHT
                       + FORGE_UI_WIN_PADDING + CHECKBOX_HEIGHT * 0.5f;

    /* ── Simulated frames ─────────────────────────────────────────────── */

    SDL_Log("%s", SEPARATOR);
    SDL_Log("SIMULATING FRAMES WITH THREE WINDOWS");
    SDL_Log("%s", SEPARATOR);

    FrameInput frames[] = {
        /* Frame 0: Initial view -- three windows arranged */
        { idle_mx, idle_my, false, 0.0f,
          "Initial view -- three overlapping windows" },

        /* Frame 1: Press on Settings title bar (bring to front) */
        { settings_title_cx, settings_title_cy, true, 0.0f,
          "Press Settings title bar (bring to front, start drag)" },

        /* Frame 2: Drag Settings window to new position */
        { settings_title_cx + DRAG_OFFSET_X, settings_title_cy + DRAG_OFFSET_Y,
          true, 0.0f, "Drag Settings to new position" },

        /* Frame 3: Release drag */
        { settings_title_cx + DRAG_OFFSET_X, settings_title_cy + DRAG_OFFSET_Y,
          false, 0.0f, "Release drag -- Settings at new position" },

        /* Frame 4: Click Info collapse toggle to collapse */
        { info_toggle_cx, info_toggle_cy, true, 0.0f,
          "Press Info collapse toggle" },

        /* Frame 5: Release collapse toggle -- Info collapses */
        { info_toggle_cx, info_toggle_cy, false, 0.0f,
          "Release -- Info window collapsed (title bar only)" },

        /* Frame 6: Click Info toggle again to expand */
        { info_toggle_cx, info_toggle_cy, true, 0.0f,
          "Press Info toggle again to expand" },

        /* Frame 7: Release -- Info expands */
        { info_toggle_cx, info_toggle_cy, false, 0.0f,
          "Release -- Info window expanded again" },

        /* Frame 8: Scroll down in Info window */
        { info_content_cx, info_content_cy, false, SCROLL_STEP,
          "Mouse wheel scroll down in Info window" },

        /* Frame 9: Continue scrolling */
        { info_content_cx, info_content_cy, false, SCROLL_STEP,
          "Continue scrolling Info content" },

        /* Frame 10: Click checkbox in Settings (overlapping Status) */
        { cb_click_x + DRAG_OFFSET_X, cb_click_y + DRAG_OFFSET_Y, true, 0.0f,
          "Press checkbox in Settings (overlaps Status, tests z-routing)" },

        /* Frame 11: Release checkbox */
        { cb_click_x + DRAG_OFFSET_X, cb_click_y + DRAG_OFFSET_Y, false, 0.0f,
          "Release checkbox -- verify toggle, z-order input routing" },
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

        declare_windows(&wctx, &settings_win, &status_win, &info_win,
                        checkboxes, &slider_val);

        forge_ui_wctx_end(&wctx);
        forge_ui_ctx_end(&ctx);

        /* Log state */
        SDL_Log("  State: hot=%u  active=%u  hovered_window=%u",
                (unsigned)ctx.hot, (unsigned)ctx.active,
                (unsigned)wctx.hovered_window_id);
        SDL_Log("  Z-orders: Settings=%d  Status=%d  Info=%d",
                settings_win.z_order, status_win.z_order, info_win.z_order);
        SDL_Log("  Windows: Settings at (%.0f,%.0f) collapsed=%s",
                (double)settings_win.rect.x, (double)settings_win.rect.y,
                settings_win.collapsed ? "yes" : "no");
        SDL_Log("           Status at (%.0f,%.0f) collapsed=%s",
                (double)status_win.rect.x, (double)status_win.rect.y,
                status_win.collapsed ? "yes" : "no");
        SDL_Log("           Info at (%.0f,%.0f) collapsed=%s scroll=%.0f",
                (double)info_win.rect.x, (double)info_win.rect.y,
                info_win.collapsed ? "yes" : "no",
                (double)info_win.scroll_y);
        SDL_Log("  Draw data: %d vertices, %d indices (%d triangles)",
                ctx.vertex_count, ctx.index_count, ctx.index_count / 3);

        /* Log checkbox states */
        {
            static char cb_buf[128];
            int pos = 0;
            for (int i = 0; i < CHECKBOX_COUNT && pos < 120; i++) {
                pos += SDL_snprintf(cb_buf + pos,
                                    (size_t)(sizeof(cb_buf) - (size_t)pos),
                                    "%s%s", i > 0 ? " " : "",
                                    checkboxes[i] ? "ON" : "--");
            }
            SDL_Log("  Checkboxes: [%s]  Slider: %.0f",
                    cb_buf, (double)slider_val);
        }

        /* ── Render to BMP ─────────────────────────────────────────────── */
        static char bmp_path[256];
        SDL_snprintf(bmp_path, sizeof(bmp_path),
                     "windows_frame_%d.bmp", f);

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
    SDL_Log("  Settings z=%d  Status z=%d  Info z=%d",
            settings_win.z_order, status_win.z_order, info_win.z_order);
    SDL_Log("  Settings at (%.0f, %.0f)",
            (double)settings_win.rect.x, (double)settings_win.rect.y);
    SDL_Log("  Info scroll_y=%.0f  collapsed=%s",
            (double)info_win.scroll_y, info_win.collapsed ? "yes" : "no");

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
