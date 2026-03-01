/*
 * UI Lesson 05 -- Immediate-Mode Basics
 *
 * Demonstrates: The declare-then-draw loop at the heart of immediate-mode UI.
 * Introduces ForgeUiContext with mouse input state and the hot/active two-ID
 * state machine from Casey Muratori's IMGUI talk.  Implements labels and
 * buttons, with hit testing and draw data generation.
 *
 * This program:
 *   1. Loads a TrueType font and builds a font atlas
 *   2. Initializes a ForgeUiContext for immediate-mode widget declaration
 *   3. Simulates 6 frames of mouse input (movement, hover, click)
 *   4. Each frame: declares buttons and labels, generates vertex/index data,
 *      renders with the software rasterizer, and writes a BMP image
 *
 * Output images show button states (normal, hovered, pressed) as the
 * simulated mouse moves and clicks.  The rasterizer uses the font atlas
 * as a texture, rendering both text (via glyph UVs) and solid rectangles
 * (via the atlas white_uv region) in a single draw call.
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
#define FB_WIDTH   400  /* output image width in pixels */
#define FB_HEIGHT  300  /* output image height in pixels */

/* ── UI layout constants ─────────────────────────────────────────────────── */
#define MARGIN          20.0f   /* pixels of margin around the UI */
#define BUTTON_WIDTH   160.0f   /* button width in pixels */
#define BUTTON_HEIGHT   40.0f   /* button height in pixels */
#define BUTTON_SPACING  12.0f   /* vertical gap between buttons */
#define LABEL_OFFSET_Y  24.0f   /* vertical offset for title label */

/* Number of buttons in the demo */
#define BUTTON_COUNT     3

/* Vertical gap between the title label baseline and the first button */
#define TITLE_BTN_GAP   20.0f
/* Horizontal gap between the button column and the status label */
#define STATUS_GAP      20.0f

/* ── Background clear color (dark slate) ────────────────────────────────── */
#define BG_CLEAR_R      0.08f
#define BG_CLEAR_G      0.08f
#define BG_CLEAR_B      0.12f
#define BG_CLEAR_A      1.00f

/* ── Title label color (soft blue-gray) ─────────────────────────────────── */
#define TITLE_R         0.70f
#define TITLE_G         0.80f
#define TITLE_B         0.90f
#define TITLE_A         1.00f

/* ── Status label color (warm gold) ─────────────────────────────────────── */
#define STATUS_R        0.90f
#define STATUS_G        0.90f
#define STATUS_B        0.60f
#define STATUS_A        1.00f

/* ── Mouse cursor dot ───────────────────────────────────────────────────── */
#define CURSOR_DOT_RADIUS_SQ  5   /* squared pixel radius for circular shape */
#define CURSOR_DOT_R    255       /* red channel (uint8) */
#define CURSOR_DOT_G    220       /* green channel (uint8) */
#define CURSOR_DOT_B     50       /* blue channel (uint8) */
#define CURSOR_DOT_A    255       /* alpha channel (uint8) */

/* ── Simulated frame input ───────────────────────────────────────────────── */

/* Each frame specifies a mouse position and button state.  These simulate
 * a user moving the mouse over buttons, hovering, pressing, and releasing. */
typedef struct FrameInput {
    float mouse_x;            /* simulated cursor x in screen pixels */
    float mouse_y;            /* simulated cursor y in screen pixels */
    bool  mouse_down;         /* true if the primary button is held this frame */
    const char *description;  /* what this frame demonstrates (for logging) */
} FrameInput;

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

    SDL_Log("UI Lesson 05 -- Immediate-Mode Basics");
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

    /* ── Define button layout ─────────────────────────────────────────── */
    const char *btn_labels[BUTTON_COUNT] = { "Start", "Options", "Quit" };

    float btn_x = MARGIN;
    float btn_start_y = MARGIN + LABEL_OFFSET_Y + TITLE_BTN_GAP;

    ForgeUiRect btn_rects[BUTTON_COUNT];
    for (int i = 0; i < BUTTON_COUNT; i++) {
        btn_rects[i] = (ForgeUiRect){
            btn_x,
            btn_start_y + (float)i * (BUTTON_HEIGHT + BUTTON_SPACING),
            BUTTON_WIDTH,
            BUTTON_HEIGHT
        };
    }

    /* ── Define simulated frames ──────────────────────────────────────── */
    /* These frames walk through the key states of the hot/active state
     * machine.  The mouse cursor is shown as a yellow dot in the output. */
    FrameInput frames[] = {
        /* Frame 0: Mouse is far from any button -- all buttons normal */
        { 300.0f,  50.0f, false, "Mouse away from buttons -- all normal" },

        /* Frame 1: Mouse moves over the "Start" button -- it becomes hot */
        { btn_rects[0].x + btn_rects[0].w * 0.5f,
          btn_rects[0].y + btn_rects[0].h * 0.5f,
          false, "Mouse over Start -- Start becomes hot" },

        /* Frame 2: Mouse button pressed while over Start -- Start becomes active */
        { btn_rects[0].x + btn_rects[0].w * 0.5f,
          btn_rects[0].y + btn_rects[0].h * 0.5f,
          true, "Mouse pressed on Start -- Start becomes active" },

        /* Frame 3: Mouse button released over Start -- click detected */
        { btn_rects[0].x + btn_rects[0].w * 0.5f,
          btn_rects[0].y + btn_rects[0].h * 0.5f,
          false, "Mouse released on Start -- CLICK detected" },

        /* Frame 4: Mouse moves to Options button -- Options becomes hot */
        { btn_rects[1].x + btn_rects[1].w * 0.5f,
          btn_rects[1].y + btn_rects[1].h * 0.5f,
          false, "Mouse moves to Options -- Options becomes hot" },

        /* Frame 5: Mouse pressed on Options -- Options becomes active */
        { btn_rects[1].x + btn_rects[1].w * 0.5f,
          btn_rects[1].y + btn_rects[1].h * 0.5f,
          true, "Mouse pressed on Options -- Options becomes active" },
    };
    int frame_count = (int)(sizeof(frames) / sizeof(frames[0]));

    /* ── Process frames ───────────────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("SIMULATING %d FRAMES", frame_count);
    SDL_Log("%s", SEPARATOR);

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
        /* This is the core immediate-mode pattern: every frame, the
         * application declares all of its widgets.  The UI context
         * generates draw data and handles state transitions. */

        /* Title label */
        forge_ui_ctx_label(&ctx, "Immediate-Mode UI Demo",
                           MARGIN, MARGIN + ascender_px,
                           TITLE_R, TITLE_G, TITLE_B, TITLE_A);

        /* Buttons */
        bool clicked[BUTTON_COUNT];
        for (int i = 0; i < BUTTON_COUNT; i++) {
            clicked[i] = forge_ui_ctx_button(&ctx, btn_labels[i],
                                              btn_rects[i]);
        }

        /* Status label showing which button was clicked */
        const char *status = "Move the mouse and click a button";
        for (int i = 0; i < BUTTON_COUNT; i++) {
            if (clicked[i]) {
                /* Build a status message for the clicked button */
                static char click_msg[64];
                SDL_snprintf(click_msg, sizeof(click_msg),
                             "'%s' was clicked!", btn_labels[i]);
                status = click_msg;
                break;
            }
        }

        /* Right-side status area */
        forge_ui_ctx_label(&ctx, status,
                           MARGIN + BUTTON_WIDTH + STATUS_GAP,
                           btn_rects[0].y + ascender_px,
                           STATUS_R, STATUS_G, STATUS_B, STATUS_A);

        /* End frame: finalize hot/active transitions */
        forge_ui_ctx_end(&ctx);

        /* ── Log state ────────────────────────────────────────────────── */
        SDL_Log("  State after frame:");
        SDL_Log("    hot    = %u", (unsigned)ctx.hot);
        SDL_Log("    active = %u", (unsigned)ctx.active);
        for (int i = 0; i < BUTTON_COUNT; i++) {
            SDL_Log("    Button '%s': %s",
                    btn_labels[i],
                    clicked[i] ? "CLICKED" : "normal");
        }
        SDL_Log("  Draw data: %d vertices, %d indices (%d triangles)",
                ctx.vertex_count, ctx.index_count, ctx.index_count / 3);

        /* ── Render to BMP ────────────────────────────────────────────── */
        char bmp_path[64];
        SDL_snprintf(bmp_path, sizeof(bmp_path), "imgui_frame_%d.bmp", f);

        if (render_frame_bmp(bmp_path, &ctx, &atlas,
                             input->mouse_x, input->mouse_y)) {
            SDL_Log("  -> %s", bmp_path);
        } else {
            SDL_Log("  [!] Failed to write %s", bmp_path);
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
    SDL_Log("  Immediate-mode UI loop:");
    SDL_Log("    1. forge_ui_ctx_begin() -- input + reset draw data");
    SDL_Log("    2. Declare widgets (labels, buttons, ...)");
    SDL_Log("    3. forge_ui_ctx_end()   -- finalize hot/active state");
    SDL_Log("    4. Render ctx.vertices/indices with atlas texture");
    SDL_Log("%s", "");
    SDL_Log("  Hot/active state machine:");
    SDL_Log("    - hot:    widget under cursor (set each frame)");
    SDL_Log("    - active: widget being pressed (persists until release)");
    SDL_Log("    - click:  release while still over the active widget");
    SDL_Log("%s", "");
    SDL_Log("  Data output per frame:");
    SDL_Log("    - ForgeUiVertex array (pos, UV, color -- 32 bytes each)");
    SDL_Log("    - uint32 index array (CCW triangle pairs)");
    SDL_Log("    - Rect backgrounds: white_uv from atlas (solid color)");
    SDL_Log("    - Text glyphs: per-glyph UV from atlas (alpha coverage)");
    SDL_Log("%s", "");
    SDL_Log("  Both rectangles and text share the same vertex format");
    SDL_Log("  and atlas texture -> one draw call renders everything.");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("Done. Output files written to the current directory.");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return 0;
}
