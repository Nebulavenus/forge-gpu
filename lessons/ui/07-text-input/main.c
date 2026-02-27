/*
 * UI Lesson 07 -- Text Input
 *
 * Demonstrates: Single-line text input with keyboard focus and cursor.
 *
 * This lesson introduces the focused ID to ForgeUiContext -- only one
 * widget receives keyboard input at a time.  Focus is acquired when a
 * text input is clicked (same press-release-over pattern as button click),
 * and lost by clicking outside or pressing Escape.
 *
 * The text input widget operates on an application-owned
 * ForgeUiTextInputState struct containing a char buffer, capacity,
 * length, and cursor (byte index into the buffer).
 *
 * This program:
 *   1. Loads a TrueType font and builds a font atlas
 *   2. Initializes a ForgeUiContext with keyboard input support
 *   3. Simulates 10 frames of mouse + keyboard input:
 *      - Click to focus an empty field
 *      - Type characters, move cursor, insert mid-string, delete
 *      - Click outside to unfocus
 *   4. Each frame: declares text inputs and labels, generates vertex/index
 *      data, renders with the software rasterizer, writes a BMP image
 *
 * Output images show the text input being focused, characters typed,
 * cursor moved, mid-string insertion and deletion, and unfocusing.
 * A yellow dot shows the simulated cursor position.
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
#define FIELD_WIDTH    300.0f   /* text input field width */
#define FIELD_HEIGHT    32.0f   /* text input field height */
#define FIELD_SPACING   12.0f   /* vertical gap between fields */
#define FIELD_LABEL_GAP  6.0f   /* gap between field label and field */

/* ── Text input buffer size ──────────────────────────────────────────────── */
#define TEXT_BUF_SIZE  128      /* maximum bytes per text input (including '\0') */

/* ── Widget IDs ──────────────────────────────────────────────────────────── */
/* Each interactive widget needs a unique non-zero ID for the hot/active
 * and focus state machines. */
#define TI_USERNAME_ID   1
#define TI_EMAIL_ID      2

/* ── Background clear color (dark slate, same as lessons 05-06) ──────────── */
#define BG_CLEAR_R      0.08f
#define BG_CLEAR_G      0.08f
#define BG_CLEAR_B      0.12f
#define BG_CLEAR_A      1.00f

/* ── Title label color (soft blue-gray) ──────────────────────────────────── */
#define TITLE_R         0.70f
#define TITLE_G         0.80f
#define TITLE_B         0.90f
#define TITLE_A         1.00f

/* ── Field label color (dim gray) ────────────────────────────────────────── */
#define FIELD_LABEL_R   0.65f
#define FIELD_LABEL_G   0.65f
#define FIELD_LABEL_B   0.70f
#define FIELD_LABEL_A   1.00f

/* ── Status label color (warm gold) ──────────────────────────────────────── */
#define STATUS_R        0.90f
#define STATUS_G        0.90f
#define STATUS_B        0.60f
#define STATUS_A        1.00f

/* ── Mouse cursor dot ────────────────────────────────────────────────────── */
#define CURSOR_DOT_RADIUS_SQ  5   /* squared pixel radius for circular shape */
#define CURSOR_DOT_R    255       /* red channel (uint8) */
#define CURSOR_DOT_G    220       /* green channel (uint8) */
#define CURSOR_DOT_B     50       /* blue channel (uint8) */
#define CURSOR_DOT_A    255       /* alpha channel (uint8) */

/* ── Cursor blink parameters ─────────────────────────────────────────────── */
/* In a real application, the cursor blinks on/off every ~30 frames
 * (roughly 500 ms at 60 fps).  Since we only render 10 simulated frames,
 * the cursor is visible in all focused frames. */
#define BLINK_ON_FRAMES   30   /* frames the cursor is visible */
#define BLINK_OFF_FRAMES  30   /* frames the cursor is hidden */
#define BLINK_PERIOD      (BLINK_ON_FRAMES + BLINK_OFF_FRAMES)

/* ── Simulated frame input ───────────────────────────────────────────────── */

/* Each frame specifies mouse position/button state and keyboard input.
 * These simulate a user clicking a text field, typing characters, moving
 * the cursor, performing mid-string insertion/deletion, and unfocusing. */
typedef struct FrameInput {
    float       mouse_x;        /* simulated cursor x in screen pixels */
    float       mouse_y;        /* simulated cursor y in screen pixels */
    bool        mouse_down;     /* true if the primary button is held */
    const char *text_input;     /* UTF-8 characters typed this frame */
    bool        key_backspace;  /* Backspace pressed */
    bool        key_delete;     /* Delete pressed */
    bool        key_left;       /* Left arrow pressed */
    bool        key_right;      /* Right arrow pressed */
    bool        key_home;       /* Home pressed */
    bool        key_end;        /* End pressed */
    bool        key_escape;     /* Escape pressed */
    const char *description;    /* what this frame demonstrates (for logging) */
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
     * ForgeUiVertex in memory layout, so we can cast directly. */
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

    /* Draw a small yellow dot at the mouse position */
    int mx = (int)(mouse_x + 0.5f);
    int my = (int)(mouse_y + 0.5f);
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
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

    SDL_Log("UI Lesson 07 -- Text Input");
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
    SDL_Log("  hot = %u, active = %u, focused = %u",
            (unsigned)ctx.hot, (unsigned)ctx.active, (unsigned)ctx.focused);

    /* ── Compute font metrics for baseline positioning ────────────────── */
    float scale = 0.0f;
    float ascender_px = 0.0f;
    if (atlas.units_per_em > 0) {
        scale = atlas.pixel_height / (float)atlas.units_per_em;
        ascender_px = (float)atlas.ascender * scale;
    }

    /* ── Define widget layout ─────────────────────────────────────────── */

    /* Vertical layout: title, field labels, text input fields */
    float cursor_y = MARGIN + LABEL_OFFSET_Y + TITLE_GAP;

    /* "Username:" label position */
    float username_label_y = cursor_y + ascender_px;
    cursor_y += atlas.pixel_height + FIELD_LABEL_GAP;

    /* Username text input rect */
    ForgeUiRect username_rect = {
        MARGIN, cursor_y, FIELD_WIDTH, FIELD_HEIGHT
    };
    cursor_y += FIELD_HEIGHT + FIELD_SPACING;

    /* "Email:" label position */
    float email_label_y = cursor_y + ascender_px;
    cursor_y += atlas.pixel_height + FIELD_LABEL_GAP;

    /* Email text input rect */
    ForgeUiRect email_rect = {
        MARGIN, cursor_y, FIELD_WIDTH, FIELD_HEIGHT
    };

    /* Status label position */
    float status_y = email_rect.y + email_rect.h + FIELD_SPACING + FIELD_SPACING;

    /* ── Application-owned text input state ────────────────────────────── */
    /* The text input widget operates on these application-owned structs.
     * The buffer, capacity, length, and cursor are managed by the widget
     * when it has focus, but the application owns the memory. */
    char username_buf[TEXT_BUF_SIZE] = "";
    ForgeUiTextInputState username_state = {
        username_buf, TEXT_BUF_SIZE, 0, 0
    };

    char email_buf[TEXT_BUF_SIZE] = "";
    ForgeUiTextInputState email_state = {
        email_buf, TEXT_BUF_SIZE, 0, 0
    };

    /* ── Define simulated frames ──────────────────────────────────────── */
    /* These frames demonstrate the complete text input interaction cycle:
     * click to focus, type, move cursor, insert mid-string, delete, and
     * click outside to unfocus. */
    float field1_cx = username_rect.x + username_rect.w * 0.3f;
    float field1_cy = username_rect.y + username_rect.h * 0.5f;

    FrameInput frames[] = {
        /* Frame 0: Mouse away from all widgets -- both fields unfocused */
        { 420.0f, 50.0f, false,
          NULL, false, false, false, false, false, false, false,
          "Mouse away -- both fields unfocused" },

        /* Frame 1: Mouse pressed on username field */
        { field1_cx, field1_cy, true,
          NULL, false, false, false, false, false, false, false,
          "Mouse pressed on username field" },

        /* Frame 2: Mouse released on username field -- FOCUSED */
        { field1_cx, field1_cy, false,
          NULL, false, false, false, false, false, false, false,
          "Mouse released -- username field FOCUSED (empty, cursor at 0)" },

        /* Frame 3: Type "Hi" -- characters inserted, cursor advances */
        { field1_cx, field1_cy, false,
          "Hi", false, false, false, false, false, false, false,
          "Type 'Hi' -- buffer='Hi', cursor=2" },

        /* Frame 4: Press Left -- cursor moves between H and i */
        { field1_cx, field1_cy, false,
          NULL, false, false, true, false, false, false, false,
          "Press Left -- cursor=1 (between 'H' and 'i')" },

        /* Frame 5: Type "e" -- mid-string insertion produces "Hei" */
        { field1_cx, field1_cy, false,
          "e", false, false, false, false, false, false, false,
          "Type 'e' at cursor=1 -- buffer='Hei', cursor=2" },

        /* Frame 6: Press Backspace -- removes "e", back to "Hi" */
        { field1_cx, field1_cy, false,
          NULL, true, false, false, false, false, false, false,
          "Backspace -- removes 'e', buffer='Hi', cursor=1" },

        /* Frame 7: Press End -- cursor jumps to end */
        { field1_cx, field1_cy, false,
          NULL, false, false, false, false, false, true, false,
          "Press End -- cursor=2 (end of text)" },

        /* Frame 8: Type "!" -- appended at end */
        { field1_cx, field1_cy, false,
          "!", false, false, false, false, false, false, false,
          "Type '!' -- buffer='Hi!', cursor=3" },

        /* Frame 9: Click outside (mouse pressed far away) -- UNFOCUSED */
        { 420.0f, 50.0f, true,
          NULL, false, false, false, false, false, false, false,
          "Mouse pressed outside -- username field UNFOCUSED" },
    };
    int frame_count = (int)(sizeof(frames) / sizeof(frames[0]));

    /* ── Cursor blink counter ─────────────────────────────────────────── */
    /* In a real application, this would increment every frame and toggle
     * the cursor on/off.  For this 10-frame demo, cursor_visible is
     * always true since all frames fall within the first blink-on period. */
    int blink_counter = 0;

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
        if (input->text_input) {
            SDL_Log("  Keyboard: text_input='%s'", input->text_input);
        }
        if (input->key_backspace) SDL_Log("  Keyboard: Backspace");
        if (input->key_left)      SDL_Log("  Keyboard: Left");
        if (input->key_right)     SDL_Log("  Keyboard: Right");
        if (input->key_home)      SDL_Log("  Keyboard: Home");
        if (input->key_end)       SDL_Log("  Keyboard: End");

        /* Begin frame: update mouse input, reset draw buffers */
        forge_ui_ctx_begin(&ctx, input->mouse_x, input->mouse_y,
                           input->mouse_down);

        /* Set keyboard input for this frame */
        forge_ui_ctx_set_keyboard(&ctx,
                                  input->text_input,
                                  input->key_backspace,
                                  input->key_delete,
                                  input->key_left,
                                  input->key_right,
                                  input->key_home,
                                  input->key_end,
                                  input->key_escape);

        /* ── Declare widgets ──────────────────────────────────────────── */

        /* Title label */
        forge_ui_ctx_label(&ctx, "Text Input",
                           MARGIN, MARGIN + ascender_px,
                           TITLE_R, TITLE_G, TITLE_B, TITLE_A);

        /* "Username:" label */
        forge_ui_ctx_label(&ctx, "Username:",
                           MARGIN, username_label_y,
                           FIELD_LABEL_R, FIELD_LABEL_G,
                           FIELD_LABEL_B, FIELD_LABEL_A);

        /* Username text input (this is the field we interact with) */
        bool cursor_visible = (blink_counter % BLINK_PERIOD) < BLINK_ON_FRAMES;
        bool username_changed = forge_ui_ctx_text_input(
            &ctx, TI_USERNAME_ID, &username_state, username_rect,
            cursor_visible);

        /* "Email:" label */
        forge_ui_ctx_label(&ctx, "Email:",
                           MARGIN, email_label_y,
                           FIELD_LABEL_R, FIELD_LABEL_G,
                           FIELD_LABEL_B, FIELD_LABEL_A);

        /* Email text input (never focused in this demo -- shows
         * the unfocused visual state for comparison) */
        forge_ui_ctx_text_input(
            &ctx, TI_EMAIL_ID, &email_state, email_rect,
            cursor_visible);

        /* Status label */
        const char *status = "Click a field to start typing";
        static char status_buf[128];
        if (ctx.focused == TI_USERNAME_ID) {
            SDL_snprintf(status_buf, sizeof(status_buf),
                         "Username: \"%s\"  cursor=%d  len=%d",
                         username_state.buffer,
                         username_state.cursor,
                         username_state.length);
            status = status_buf;
        } else if (username_state.length > 0) {
            SDL_snprintf(status_buf, sizeof(status_buf),
                         "Username: \"%s\"  (unfocused)",
                         username_state.buffer);
            status = status_buf;
        }

        forge_ui_ctx_label(&ctx, status, MARGIN, status_y + ascender_px,
                           STATUS_R, STATUS_G, STATUS_B, STATUS_A);

        /* End frame: finalize hot/active/focus transitions */
        forge_ui_ctx_end(&ctx);

        /* Advance blink counter */
        blink_counter++;

        /* ── Log state ────────────────────────────────────────────────── */
        SDL_Log("  State after frame:");
        SDL_Log("    hot     = %u", (unsigned)ctx.hot);
        SDL_Log("    active  = %u", (unsigned)ctx.active);
        SDL_Log("    focused = %u", (unsigned)ctx.focused);
        SDL_Log("    Username: buffer=\"%s\"  len=%d  cursor=%d",
                username_state.buffer, username_state.length,
                username_state.cursor);
        if (username_changed) {
            SDL_Log("    -> Content CHANGED");
        }
        SDL_Log("  Draw data: %d vertices, %d indices (%d triangles)",
                ctx.vertex_count, ctx.index_count, ctx.index_count / 3);

        /* ── Render to BMP ────────────────────────────────────────────── */
        char bmp_path[64];
        SDL_snprintf(bmp_path, sizeof(bmp_path), "text_input_frame_%d.bmp", f);

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
    SDL_Log("  Focus system:");
    SDL_Log("    - Only one widget receives keyboard input (focused ID)");
    SDL_Log("    - Acquired by click (press-release-over, same as button)");
    SDL_Log("    - Lost by clicking outside any text input or pressing Escape");
    SDL_Log("%s", "");
    SDL_Log("  Text input state (application-owned):");
    SDL_Log("    - buffer:   char array (null-terminated)");
    SDL_Log("    - capacity: total size including '\\0'");
    SDL_Log("    - length:   current text length in bytes");
    SDL_Log("    - cursor:   byte index for insertion point");
    SDL_Log("%s", "");
    SDL_Log("  Keyboard input:");
    SDL_Log("    - text_input: UTF-8 chars spliced at cursor, trailing bytes shift right");
    SDL_Log("    - Backspace:  remove byte before cursor, shift trailing left");
    SDL_Log("    - Delete:     remove byte at cursor, shift trailing left");
    SDL_Log("    - Left/Right: move cursor one byte");
    SDL_Log("    - Home/End:   jump cursor to 0 / length");
    SDL_Log("%s", "");
    SDL_Log("  Draw elements:");
    SDL_Log("    - Background rect (white_uv, color varies by state)");
    SDL_Log("    - Focused border (4 thin edge rects, accent cyan)");
    SDL_Log("    - Text quads (glyph UVs from left edge + padding)");
    SDL_Log("    - Cursor bar (2px-wide rect, pen_x from text_measure on substring)");
    SDL_Log("%s", SEPARATOR);
    SDL_Log("Done. Output files written to the current directory.");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return had_render_error ? 1 : 0;
}
