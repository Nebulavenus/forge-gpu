/*
 * UI Lesson 09 -- Panels and Scrolling
 *
 * Demonstrates: Fixed-position containers with clipping and vertical scroll.
 *
 * A panel is a rectangular region with a background fill, a title bar, and
 * a content area that clips child widgets to its bounds.  When the total
 * content height exceeds the visible area, a scrollbar appears on the right
 * edge, and the content can be scrolled via mouse wheel or by dragging the
 * scrollbar thumb.
 *
 * This program:
 *   1. Loads a TrueType font and builds a font atlas
 *   2. Initializes a ForgeUiContext with panel/clipping support
 *   3. Builds two side-by-side panels:
 *      - Left panel: 10 checkboxes (more than fit in the visible area,
 *        requiring scrolling)
 *      - Right panel: a short list of labels (fits without scrolling,
 *        no scrollbar drawn)
 *   4. Simulates ~10 frames demonstrating:
 *      - Initial view showing first few checkboxes
 *      - Scroll down via mouse wheel (content shifts up, new items appear)
 *      - Scroll to bottom (scrollbar thumb at bottom of track)
 *      - Drag scrollbar thumb back to top
 *      - Click a checkbox mid-scroll to verify interaction with scroll offset
 *   5. Each frame: declares widgets inside panels, generates vertex/index
 *      data, renders with the software rasterizer, writes a BMP image
 *
 * Output images show two panels with the left panel scrolling through its
 * content.  A yellow dot shows the simulated cursor position.
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
#define FB_WIDTH   640  /* output image width in pixels */
#define FB_HEIGHT  420  /* output image height in pixels */

/* ── Panel layout constants ─────────────────────────────────────────────── */
#define LEFT_PANEL_X       20.0f   /* left panel left edge */
#define LEFT_PANEL_Y       20.0f   /* left panel top edge */
#define LEFT_PANEL_W      280.0f   /* left panel width */
#define LEFT_PANEL_H      360.0f   /* left panel height */

#define RIGHT_PANEL_X     320.0f   /* right panel left edge */
#define RIGHT_PANEL_Y      20.0f   /* right panel top edge */
#define RIGHT_PANEL_W     280.0f   /* right panel width */
#define RIGHT_PANEL_H     360.0f   /* right panel height */

/* ── Widget dimensions ──────────────────────────────────────────────────── */
#define CHECKBOX_HEIGHT   28.0f   /* height of each checkbox row */
#define LABEL_HEIGHT      26.0f   /* height of each label row */

/* ── Number of checkboxes in the left panel ──────────────────────────────── */
#define CHECKBOX_COUNT      10

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

/* ── Status label positioning ────────────────────────────────────────────── */
#define STATUS_LABEL_GAP   12.0f  /* vertical gap between panel bottom and status text */

/* ── Simulated cursor margins ───────────────────────────────────────────── */
#define IDLE_CURSOR_MARGIN 20.0f  /* offset from framebuffer edge for idle cursor */
#define CB_CLICK_X_OFFSET  10.0f  /* horizontal offset into checkbox hit area */

/* ── Mouse cursor dot ────────────────────────────────────────────────────── */
#define CURSOR_DOT_RADIUS     2
#define CURSOR_DOT_RADIUS_SQ  5
#define CURSOR_DOT_R    255
#define CURSOR_DOT_G    220
#define CURSOR_DOT_B     50
#define CURSOR_DOT_A    255

/* ── Status label color (warm gold) ──────────────────────────────────────── */
#define STATUS_R        0.90f
#define STATUS_G        0.90f
#define STATUS_B        0.60f
#define STATUS_A        1.00f

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

/* ── Checkbox label names ────────────────────────────────────────────────── */

static const char *checkbox_labels[CHECKBOX_COUNT] = {
    "V-Sync",
    "Fullscreen",
    "Anti-aliasing",
    "Motion Blur",
    "Bloom",
    "Shadows",
    "Ambient Occlusion",
    "Depth of Field",
    "Reflections",
    "Volumetric Fog"
};

/* ── Helper: declare both panels ─────────────────────────────────────────── */

static void declare_panels(ForgeUiContext *ctx,
                           bool checkboxes[CHECKBOX_COUNT],
                           float *left_scroll_y,
                           float *right_scroll_y)
{
    /* ── Left panel: 10 checkboxes (requires scrolling) ────────────── */
    ForgeUiRect left_rect = {
        LEFT_PANEL_X, LEFT_PANEL_Y, LEFT_PANEL_W, LEFT_PANEL_H
    };
    if (forge_ui_ctx_panel_begin(ctx, "Graphics Settings",
                                  left_rect, left_scroll_y)) {
        for (int i = 0; i < CHECKBOX_COUNT; i++) {
            (void)forge_ui_ctx_checkbox_layout(
                ctx, checkbox_labels[i],
                &checkboxes[i], CHECKBOX_HEIGHT);
        }
        forge_ui_ctx_panel_end(ctx);
    }

    /* ── Right panel: short list of labels (no scrolling needed) ───── */
    ForgeUiRect right_rect = {
        RIGHT_PANEL_X, RIGHT_PANEL_Y, RIGHT_PANEL_W, RIGHT_PANEL_H
    };
    if (forge_ui_ctx_panel_begin(ctx, "System Info",
                                  right_rect, right_scroll_y)) {
        forge_ui_ctx_label_layout(ctx, "GPU: Integrated",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "API: Vulkan 1.3",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "Resolution: 1920x1080",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_label_layout(ctx, "FPS: 60",
                                   LABEL_HEIGHT,
                                   LABEL_R, LABEL_G, LABEL_B, LABEL_A);
        forge_ui_ctx_panel_end(ctx);
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

    SDL_Log("UI Lesson 09 -- Panels and Scrolling");
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
    bool checkboxes[CHECKBOX_COUNT];
    for (int i = 0; i < CHECKBOX_COUNT; i++) {
        checkboxes[i] = (i % 3 == 0);  /* every third checkbox starts checked */
    }

    float left_scroll_y = 0.0f;   /* left panel scroll offset */
    float right_scroll_y = 0.0f;  /* right panel scroll offset */

    bool had_render_error = false;

    /* ── Compute approximate scrollbar thumb position for dragging ─────── */
    /* Content: 10 checkboxes * (28 + 8 spacing) - 8 trailing spacing = 352
     * Content area: panel_h - title_h - 2 * padding = 360 - 30 - 20 = 310
     * max_scroll = 352 - 310 = 42 */
    float approx_content_h = CHECKBOX_COUNT
                              * (CHECKBOX_HEIGHT + FORGE_UI_PANEL_CONTENT_SPACING)
                              - FORGE_UI_PANEL_CONTENT_SPACING;
    float approx_visible_h = LEFT_PANEL_H - FORGE_UI_PANEL_TITLE_HEIGHT
                              - 2.0f * FORGE_UI_PANEL_PADDING;
    float approx_max_scroll = approx_content_h - approx_visible_h;
    if (approx_max_scroll < 0.0f) approx_max_scroll = 0.0f;

    /* Scrollbar track geometry for thumb drag targeting */
    float track_y = LEFT_PANEL_Y + FORGE_UI_PANEL_TITLE_HEIGHT
                    + FORGE_UI_PANEL_PADDING;
    float track_h = approx_visible_h;
    float thumb_h = track_h * approx_visible_h / approx_content_h;
    if (thumb_h < FORGE_UI_SCROLLBAR_MIN_THUMB) thumb_h = FORGE_UI_SCROLLBAR_MIN_THUMB;
    float thumb_range = track_h - thumb_h;
    float scrollbar_x = LEFT_PANEL_X + LEFT_PANEL_W - FORGE_UI_PANEL_PADDING
                         - FORGE_UI_SCROLLBAR_WIDTH
                         + FORGE_UI_SCROLLBAR_WIDTH * 0.5f;

    /* Idle mouse position (outside both panels) */
    float idle_mx = FB_WIDTH - IDLE_CURSOR_MARGIN;
    float idle_my = IDLE_CURSOR_MARGIN;

    /* Center of left panel content area (for mouse wheel scrolling) */
    float content_cx = LEFT_PANEL_X + LEFT_PANEL_W * 0.5f;
    float content_cy = LEFT_PANEL_Y + FORGE_UI_PANEL_TITLE_HEIGHT
                       + LEFT_PANEL_H * 0.35f;

    /* A checkbox center y for click verification (checkbox 5, ~middle) */
    float cb5_approx_x = LEFT_PANEL_X + FORGE_UI_PANEL_PADDING + CB_CLICK_X_OFFSET;

    /* ── Simulated frames ─────────────────────────────────────────────── */

    SDL_Log("%s", SEPARATOR);
    SDL_Log("SIMULATING FRAMES WITH TWO PANELS");
    SDL_Log("%s", SEPARATOR);

    FrameInput frames[] = {
        /* Frame 0: Initial view, scroll_y = 0 */
        { idle_mx, idle_my, false, 0.0f,
          "Initial view -- two panels, no scrolling" },

        /* Frame 1: Mouse wheel scroll down on left panel */
        { content_cx, content_cy, false, 1.0f,
          "Mouse wheel scroll down (left panel)" },

        /* Frame 2: Continue scrolling down */
        { content_cx, content_cy, false, 1.5f,
          "Continue scrolling down" },

        /* Frame 3: Scroll to maximum */
        { content_cx, content_cy, false, 3.0f,
          "Scroll to bottom (max scroll)" },

        /* Frame 4: Press on scrollbar thumb to start drag */
        { scrollbar_x, track_y + thumb_range + thumb_h * 0.5f, true, 0.0f,
          "Press scrollbar thumb at bottom" },

        /* Frame 5: Drag scrollbar thumb toward top */
        { scrollbar_x, track_y + thumb_range * 0.3f + thumb_h * 0.5f, true, 0.0f,
          "Drag scrollbar thumb upward (~30%)" },

        /* Frame 6: Continue drag to top */
        { scrollbar_x, track_y + thumb_h * 0.5f, true, 0.0f,
          "Drag scrollbar thumb to top" },

        /* Frame 7: Release scrollbar thumb */
        { scrollbar_x, track_y + thumb_h * 0.5f, false, 0.0f,
          "Release scrollbar thumb" },

        /* Frame 8: Click a checkbox while at current scroll position */
        { cb5_approx_x, content_cy, true, 0.0f,
          "Press on a checkbox mid-scroll" },

        /* Frame 9: Release checkbox (verify toggle) */
        { cb5_approx_x, content_cy, false, 0.0f,
          "Release checkbox -- verify toggle works with scroll offset" },
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

        declare_panels(&ctx, checkboxes, &left_scroll_y, &right_scroll_y);

        /* Status label below the panels showing scroll state */
        {
            float ascender_px = 0.0f;
            if (atlas.units_per_em > 0) {
                float scale = atlas.pixel_height / (float)atlas.units_per_em;
                ascender_px = (float)atlas.ascender * scale;
            }

            static char status_buf[128];
            SDL_snprintf(status_buf, sizeof(status_buf),
                         "scroll_y=%.0f  max=%.0f",
                         (double)left_scroll_y,
                         (double)approx_max_scroll);

            forge_ui_ctx_label(&ctx, status_buf,
                               LEFT_PANEL_X,
                               LEFT_PANEL_Y + LEFT_PANEL_H + STATUS_LABEL_GAP + ascender_px,
                               STATUS_R, STATUS_G, STATUS_B, STATUS_A);
        }

        forge_ui_ctx_end(&ctx);

        /* Log state */
        SDL_Log("  State: hot=%u  active=%u  scroll_y=%.1f",
                (unsigned)ctx.hot, (unsigned)ctx.active,
                (double)left_scroll_y);
        SDL_Log("  Draw data: %d vertices, %d indices (%d triangles)",
                ctx.vertex_count, ctx.index_count, ctx.index_count / 3);

        /* Log checkbox states */
        {
            static char cb_buf[128];
            int pos = 0;
            for (int i = 0; i < CHECKBOX_COUNT && pos < 120; i++) {
                pos += SDL_snprintf(cb_buf + pos, (size_t)(sizeof(cb_buf) - (size_t)pos),
                                    "%s%s", i > 0 ? " " : "",
                                    checkboxes[i] ? "ON" : "--");
            }
            SDL_Log("  Checkboxes: [%s]", cb_buf);
        }

        /* Render to BMP */
        char bmp_path[64];
        SDL_snprintf(bmp_path, sizeof(bmp_path), "panels_frame_%d.bmp", f);

        if (render_frame_bmp(bmp_path, &ctx, &atlas,
                             input->mouse_x, input->mouse_y)) {
            SDL_Log("  -> %s", bmp_path);
        } else {
            SDL_Log("  [!] Failed to write %s", bmp_path);
            had_render_error = true;
        }
    }

    /* ── Summary ────────────────────────────────────────────────────────── */
    SDL_Log("%s", "");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("SUMMARY");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("%s", "");
    SDL_Log("  Panel system:");
    SDL_Log("    - forge_ui_ctx_panel_begin(): title bar + clip rect + layout");
    SDL_Log("    - forge_ui_ctx_panel_end():   content overflow + scrollbar");
    SDL_Log("%s", "");
    SDL_Log("  Clipping:");
    SDL_Log("    - clip_rect (ForgeUiRect) + has_clip (bool) on context");
    SDL_Log("    - All vertex-emitting functions clip against clip_rect");
    SDL_Log("    - Axis-aligned rect-vs-rect: discard, trim, or remap UVs");
    SDL_Log("%s", "");
    SDL_Log("  Scrolling:");
    SDL_Log("    - layout_next subtracts scroll_y from widget y positions");
    SDL_Log("    - Mouse wheel: scroll_delta * SCROLL_SPEED adjusts scroll_y");
    SDL_Log("    - Scrollbar thumb: drag interaction maps thumb_y to scroll_y");
    SDL_Log("%s", "");
    SDL_Log("  Scrollbar formulas:");
    SDL_Log("    thumb_h = track_h * visible_h / content_h");
    SDL_Log("    thumb_y = track_y + scroll_y / max_scroll * (track_h - thumb_h)");
    SDL_Log("    max_scroll = content_h - visible_h");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("Done. Output files written to the current directory.");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return had_render_error ? 1 : 0;
}
