/*
 * UI Lesson 15 — Dev UI
 *
 * Demonstrates developer-facing UI patterns composed from the immediate-mode
 * controls: a read-only property inspector, an editable property editor with
 * drag-float and drag-int fields, a scrollable console log, performance
 * overlays with sparkline graphs, a hierarchical scene tree, selection
 * controls (listbox, dropdown, radio buttons), and an HSV color picker.
 *
 * Output: seven BMP images showing each pattern.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <string.h>

#include "math/forge_math.h"
#include "ui/forge_ui.h"
#include "ui/forge_ui_ctx.h"
#include "raster/forge_raster.h"

/* ── Framebuffer dimensions ────────────────────────────────────────────── */

#define FB_WIDTH   800            /* output image width in pixels */
#define FB_HEIGHT  600            /* output image height in pixels */

/* ── Font and layout ───────────────────────────────────────────────────── */

#define FONT_PATH       "assets/fonts/liberation_mono/LiberationMono-Regular.ttf" /* TTF font asset */
#define BASE_PIXEL_HEIGHT 16.0f   /* base font size — slightly smaller for dense dev UI */
#define ROW_HEIGHT       24.0f    /* widget row height for layout */
#define MARGIN           16.0f    /* outer margin from screen edges */
#define INDENT           20.0f    /* indentation per tree level */

/* ── Inspector panel dimensions ───────────────────────────────────────── */

#define INSPECTOR_W      350.0f   /* property editor panel width */
#define INSPECTOR_H      450.0f   /* property editor panel height */
#define PROP_LABEL_W     100.0f   /* label column width for property names */
#define SWATCH_SIZE       16.0f   /* color swatch square size */
#define SWATCH_GAP         6.0f   /* gap between swatch and texture filename */
#define BASELINE_NUDGE     4.0f   /* vertical offset to visually center text in a row */

/* Console, performance overlay, and scene tree dimensions are defined
 * locally near their respective render_*() functions below. */

/* ── Codepoint table for font atlas ──────────────────────────────────── */

#define ASCII_START   32          /* space — first printable ASCII */
#define ASCII_END    127          /* DEL — one past last printable ASCII */

static Uint32 g_codepoints[ASCII_END - ASCII_START];
static int g_codepoint_count;

static void init_codepoints(void)
{
    g_codepoint_count = 0;
    for (Uint32 cp = ASCII_START; cp < ASCII_END; cp++) {
        g_codepoints[g_codepoint_count++] = cp;
    }
}

/* ── Helper: create atlas + context at a given scale ───────────────────── */

static bool create_ui(const ForgeUiFont *font, ForgeUiFontAtlas *atlas,
                      ForgeUiContext *ctx, float scale)
{
    float pixel_h = BASE_PIXEL_HEIGHT * scale;
    if (!forge_ui_atlas_build(font, pixel_h,
                              g_codepoints, g_codepoint_count,
                              1, atlas)) {
        SDL_Log("Failed to build font atlas");
        return false;
    }
    if (!forge_ui_ctx_init(ctx, atlas)) {
        SDL_Log("Failed to init UI context");
        forge_ui_atlas_free(atlas);
        return false;
    }
    ctx->scale = scale;
    return true;
}

/* ── Helper: rasterize context draw data to a BMP ──────────────────────── */

static bool rasterize_to_bmp(ForgeUiContext *ctx,
                             const ForgeUiFontAtlas *atlas,
                             int width, int height,
                             const char *filename)
{
    ForgeRasterBuffer fb = forge_raster_buffer_create(width, height);
    if (!fb.pixels) {
        SDL_Log("Failed to create framebuffer for %s", filename);
        return false;
    }
    /* Clear to theme background — dev UI uses a dark bg */
    forge_raster_clear(&fb,
                       ctx->theme.bg.r,
                       ctx->theme.bg.g,
                       ctx->theme.bg.b,
                       1.0f);

    ForgeRasterTexture tex;
    tex.pixels = atlas->pixels;
    tex.width  = atlas->width;
    tex.height = atlas->height;

    forge_raster_triangles_indexed(&fb,
                                   (const ForgeRasterVertex *)ctx->vertices,
                                   ctx->vertex_count,
                                   ctx->indices, ctx->index_count,
                                   &tex);

    bool ok = forge_raster_write_bmp(&fb, filename);
    if (!ok) SDL_Log("Failed to write %s", filename);
    forge_raster_buffer_destroy(&fb);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image 1: Property Inspector (read-only)
 *
 * A panel titled "Inspector" containing collapsible sections showing
 * entity properties.  Each section uses tree_push/tree_pop for expand/
 * collapse.  Within each expanded section, sliders, checkboxes, and
 * labeled values show the entity's transform, material, and physics
 * properties — the same pattern used by editors like Unity and Godot.
 * ═══════════════════════════════════════════════════════════════════════ */

/* Helper: emit a label at the vertical center of a row rectangle.
 * The label is drawn in the theme's dimmed text color, offset from the
 * left edge of the rect by a given x_offset. */
static void prop_label(ForgeUiContext *ctx, const char *text,
                       ForgeUiRect row, float x_offset)
{
    float baseline_y = row.y + row.h * 0.5f + BASELINE_NUDGE;
    forge_ui_ctx_label_colored(ctx, text,
                               row.x + x_offset, baseline_y,
                               ctx->theme.text_dim.r,
                               ctx->theme.text_dim.g,
                               ctx->theme.text_dim.b,
                               ctx->theme.text_dim.a);
}

/* Helper: emit a value label at the vertical center of a row rectangle.
 * Drawn in the theme's primary text color, to the right of the label column. */
static void prop_value(ForgeUiContext *ctx, const char *text,
                       ForgeUiRect row, float x_offset)
{
    float baseline_y = row.y + row.h * 0.5f + BASELINE_NUDGE;
    forge_ui_ctx_label(ctx, text, row.x + x_offset, baseline_y);
}

static bool render_property_inspector(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    /* Center the inspector panel in the framebuffer */
    float panel_x = ((float)FB_WIDTH - INSPECTOR_W) * 0.5f;
    float panel_y = ((float)FB_HEIGHT - INSPECTOR_H) * 0.5f;

    float scroll_y = 0.0f;
    ForgeUiRect panel_rect = { panel_x, panel_y, INSPECTOR_W, INSPECTOR_H };

    if (forge_ui_ctx_panel_begin(&ctx, "Inspector", panel_rect, &scroll_y)) {

        /* ── Entity name header ──────────────────────────────────────── */
        forge_ui_ctx_label_layout(&ctx, "Stone Pillar", ROW_HEIGHT);

        /* ── Transform section ───────────────────────────────────────── */
        bool transform_open = true;
        if (forge_ui_ctx_tree_push_layout(&ctx, "Transform",
                                           &transform_open, ROW_HEIGHT)) {
            /* Position — three coordinate values displayed as text.
             * Each row is fetched from the panel's vertical layout,
             * then indented to sit under the tree header. */
            ForgeUiRect row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Position", row, INDENT);
            prop_value(&ctx, "X: 12.5  Y: 0.0  Z: -3.2",
                       row, INDENT + PROP_LABEL_W);

            /* Rotation — Euler angles in degrees */
            row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Rotation", row, INDENT);
            prop_value(&ctx, "X: 0  Y: 45  Z: 0",
                       row, INDENT + PROP_LABEL_W);

            /* Scale — editable via slider */
            row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Scale", row, INDENT);
            float scale_val = 1.0f;
            ForgeUiRect slider_rect = {
                row.x + INDENT + PROP_LABEL_W,
                row.y,
                row.w - INDENT - PROP_LABEL_W,
                row.h
            };
            forge_ui_push_id(&ctx, "xform_scale");
            forge_ui_ctx_slider(&ctx, "##scale", &scale_val,
                                0.1f, 10.0f, slider_rect);
            forge_ui_pop_id(&ctx);

            /* Separator to visually close the section */
            forge_ui_ctx_separator_layout(&ctx, ROW_HEIGHT * 0.5f);
        }
        forge_ui_ctx_tree_pop(&ctx);

        /* ── Material section ────────────────────────────────────────── */
        bool material_open = true;
        if (forge_ui_ctx_tree_push_layout(&ctx, "Material",
                                           &material_open, ROW_HEIGHT)) {
            /* Albedo — color swatch + texture filename */
            ForgeUiRect row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Albedo", row, INDENT);

            /* Stone-colored swatch preview — intentionally a literal color
             * to demonstrate how a material editor shows the albedo tint */
            ForgeUiRect swatch = {
                row.x + INDENT + PROP_LABEL_W,
                row.y + (ROW_HEIGHT - SWATCH_SIZE) * 0.5f,
                SWATCH_SIZE,
                SWATCH_SIZE
            };
            ForgeUiColor stone_color = { 0.65f, 0.62f, 0.55f, 1.0f };
            forge_ui_ctx_rect(&ctx, swatch, stone_color);

            float name_x = row.x + INDENT + PROP_LABEL_W
                         + SWATCH_SIZE + SWATCH_GAP;
            float name_y = row.y + row.h * 0.5f + BASELINE_NUDGE;
            forge_ui_ctx_label(&ctx, "Stone_diffuse.png", name_x, name_y);

            /* Roughness — slider at 0.7 */
            row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Roughness", row, INDENT);
            float roughness_val = 0.7f;
            ForgeUiRect rough_slider = {
                row.x + INDENT + PROP_LABEL_W,
                row.y,
                row.w - INDENT - PROP_LABEL_W,
                row.h
            };
            forge_ui_push_id(&ctx, "mat_roughness");
            forge_ui_ctx_slider(&ctx, "##roughness", &roughness_val,
                                0.0f, 1.0f, rough_slider);
            forge_ui_pop_id(&ctx);

            /* Metallic — checkbox, unchecked */
            row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            bool metallic_val = false;
            ForgeUiRect cb_rect = {
                row.x + INDENT,
                row.y,
                row.w - INDENT,
                row.h
            };
            forge_ui_push_id(&ctx, "mat_metallic");
            forge_ui_ctx_checkbox(&ctx, "Metallic", &metallic_val, cb_rect);
            forge_ui_pop_id(&ctx);

            /* Separator */
            forge_ui_ctx_separator_layout(&ctx, ROW_HEIGHT * 0.5f);
        }
        forge_ui_ctx_tree_pop(&ctx);

        /* ── Physics section (collapsed) ─────────────────────────────── */
        /* Start collapsed to demonstrate how tree_push shows only the
         * header when the section is closed.  The tree_pop call is still
         * required to balance the ID scope. */
        bool physics_open = false;
        forge_ui_ctx_tree_push_layout(&ctx, "Physics",
                                       &physics_open, ROW_HEIGHT);
        /* Children are skipped because physics_open is false, but
         * tree_pop must still be called to pop the ID scope. */
        forge_ui_ctx_tree_pop(&ctx);

        forge_ui_ctx_panel_end(&ctx);
    }

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, FB_HEIGHT,
                               "property_inspector.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image 2: Property Editor (editable)
 *
 * A panel titled "Editor" with editable property fields: drag_float for
 * position/rotation/scale, drag_int for layer/priority, checkboxes, and
 * sliders.  Demonstrates the new drag-value widgets that let users modify
 * values by click-dragging horizontally.
 * ═══════════════════════════════════════════════════════════════════════ */

#define EDITOR_W     400.0f   /* editor panel width */
#define EDITOR_H     520.0f   /* editor panel height */

static bool render_property_editor(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    float panel_x = ((float)FB_WIDTH - EDITOR_W) * 0.5f;
    float panel_y = ((float)FB_HEIGHT - EDITOR_H) * 0.5f;

    float scroll_y = 0.0f;
    ForgeUiRect panel_rect = { panel_x, panel_y, EDITOR_W, EDITOR_H };

    if (forge_ui_ctx_panel_begin(&ctx, "Editor", panel_rect, &scroll_y)) {

        /* ── Entity name ───────────────────────────────────────────────── */
        forge_ui_ctx_label_layout(&ctx, "Stone Pillar", ROW_HEIGHT);

        /* ── Transform section ─────────────────────────────────────────── */
        bool transform_open = true;
        if (forge_ui_ctx_tree_push_layout(&ctx, "Transform",
                                           &transform_open, ROW_HEIGHT)) {
            /* Position — 3-component drag float */
            ForgeUiRect row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Position", row, INDENT);
            float pos[3] = { 12.5f, 0.0f, -3.2f };
            ForgeUiRect field = {
                row.x + INDENT + PROP_LABEL_W, row.y,
                row.w - INDENT - PROP_LABEL_W, row.h
            };
            forge_ui_push_id(&ctx, "xform_pos");
            forge_ui_ctx_drag_float_n(&ctx, "##pos", pos, 3,
                                       0.1f, -100.0f, 100.0f, field);
            forge_ui_pop_id(&ctx);

            /* Rotation — 3-component drag float (degrees) */
            row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Rotation", row, INDENT);
            float rot[3] = { 0.0f, 45.0f, 0.0f };
            field = (ForgeUiRect){
                row.x + INDENT + PROP_LABEL_W, row.y,
                row.w - INDENT - PROP_LABEL_W, row.h
            };
            forge_ui_push_id(&ctx, "xform_rot");
            forge_ui_ctx_drag_float_n(&ctx, "##rot", rot, 3,
                                       1.0f, -360.0f, 360.0f, field);
            forge_ui_pop_id(&ctx);

            /* Scale — single drag float */
            row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Scale", row, INDENT);
            float scale_val = 1.0f;
            field = (ForgeUiRect){
                row.x + INDENT + PROP_LABEL_W, row.y,
                row.w - INDENT - PROP_LABEL_W, row.h
            };
            forge_ui_push_id(&ctx, "xform_scale");
            forge_ui_ctx_drag_float(&ctx, "##scale", &scale_val,
                                     0.01f, 0.01f, 100.0f, field);
            forge_ui_pop_id(&ctx);

            forge_ui_ctx_separator_layout(&ctx, ROW_HEIGHT * 0.5f);
        }
        forge_ui_ctx_tree_pop(&ctx);

        /* ── Material section ──────────────────────────────────────────── */
        bool material_open = true;
        if (forge_ui_ctx_tree_push_layout(&ctx, "Material",
                                           &material_open, ROW_HEIGHT)) {
            /* Roughness — drag float 0..1 */
            ForgeUiRect row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Roughness", row, INDENT);
            float roughness = 0.7f;
            ForgeUiRect field = {
                row.x + INDENT + PROP_LABEL_W, row.y,
                row.w - INDENT - PROP_LABEL_W, row.h
            };
            forge_ui_push_id(&ctx, "mat_rough");
            forge_ui_ctx_drag_float(&ctx, "##rough", &roughness,
                                     0.01f, 0.0f, 1.0f, field);
            forge_ui_pop_id(&ctx);

            /* Metallic — checkbox */
            row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            bool metallic = false;
            ForgeUiRect cb_rect = { row.x + INDENT, row.y,
                                     row.w - INDENT, row.h };
            forge_ui_push_id(&ctx, "mat_metal");
            forge_ui_ctx_checkbox(&ctx, "Metallic", &metallic, cb_rect);
            forge_ui_pop_id(&ctx);

            forge_ui_ctx_separator_layout(&ctx, ROW_HEIGHT * 0.5f);
        }
        forge_ui_ctx_tree_pop(&ctx);

        /* ── Rendering section ─────────────────────────────────────────── */
        bool render_open = true;
        if (forge_ui_ctx_tree_push_layout(&ctx, "Rendering",
                                           &render_open, ROW_HEIGHT)) {
            /* Layer — drag int */
            ForgeUiRect row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Layer", row, INDENT);
            int layer = 3;
            ForgeUiRect field = {
                row.x + INDENT + PROP_LABEL_W, row.y,
                row.w - INDENT - PROP_LABEL_W, row.h
            };
            forge_ui_push_id(&ctx, "rend_layer");
            forge_ui_ctx_drag_int(&ctx, "##layer", &layer,
                                   0.2f, 0, 31, field);
            forge_ui_pop_id(&ctx);

            /* Priority — drag int */
            row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Priority", row, INDENT);
            int priority = 100;
            field = (ForgeUiRect){
                row.x + INDENT + PROP_LABEL_W, row.y,
                row.w - INDENT - PROP_LABEL_W, row.h
            };
            forge_ui_push_id(&ctx, "rend_prio");
            forge_ui_ctx_drag_int(&ctx, "##prio", &priority,
                                   1.0f, 0, 9999, field);
            forge_ui_pop_id(&ctx);

            /* Resolution — 2-component drag int */
            row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);
            prop_label(&ctx, "Resolution", row, INDENT);
            int resolution[2] = { 1920, 1080 };
            field = (ForgeUiRect){
                row.x + INDENT + PROP_LABEL_W, row.y,
                row.w - INDENT - PROP_LABEL_W, row.h
            };
            forge_ui_push_id(&ctx, "rend_res");
            forge_ui_ctx_drag_int_n(&ctx, "##res", resolution, 2,
                                     1.0f, 1, 8192, field);
            forge_ui_pop_id(&ctx);

            forge_ui_ctx_separator_layout(&ctx, ROW_HEIGHT * 0.5f);
        }
        forge_ui_ctx_tree_pop(&ctx);

        forge_ui_ctx_panel_end(&ctx);
    }

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, FB_HEIGHT,
                               "property_editor.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image 3: Console / Log Viewer
 *
 * A scrollable panel showing color-coded log entries — the kind of output
 * console every game engine needs for debugging.  Each entry has a
 * severity tag (INFO, WARN, ERROR, DEBUG) rendered in a distinct color,
 * followed by the message text in the theme's default color.
 *
 * The severity colors are deliberately hardcoded rather than pulled from
 * ctx->theme because they represent domain-specific semantics (error = red,
 * warning = yellow) that should be stable across any theme.
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Console layout constants ─────────────────────────────────────────── */

#define CONSOLE_WIDTH    700.0f    /* panel width in pixels */
#define CONSOLE_HEIGHT   450.0f    /* panel height in pixels */
#define LEVEL_TAG_WIDTH   70.0f   /* horizontal space reserved for "[LEVEL]" */

/* ── Log entry definition ─────────────────────────────────────────────── */

typedef struct LogEntry {
    const char *level;            /* severity: "INFO", "WARN", "ERROR", "DEBUG" */
    const char *message;          /* human-readable log message */
    float r, g, b;                /* color for the severity tag */
} LogEntry;

/* Severity colors — fixed by convention, not theme-dependent */
#define CLR_INFO_R   0.78f        /* light gray for informational messages */
#define CLR_INFO_G   0.78f
#define CLR_INFO_B   0.78f

#define CLR_WARN_R   0.95f        /* yellow for warnings */
#define CLR_WARN_G   0.85f
#define CLR_WARN_B   0.20f

#define CLR_ERROR_R  0.90f        /* red for errors */
#define CLR_ERROR_G  0.25f
#define CLR_ERROR_B  0.20f

#define CLR_DEBUG_R  0.40f        /* cyan for debug/diagnostic output */
#define CLR_DEBUG_G  0.80f
#define CLR_DEBUG_B  0.90f

/* Simulated engine output — the entries a developer would see during a
 * typical frame of a 3D game.  The mix of severities demonstrates that
 * color-coding lets a developer spot problems without reading every line. */
static const LogEntry LOG_ENTRIES[] = {
    { "[INFO]",  "Engine initialized",                   CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
    { "[INFO]",  "Loading scene: forest.gltf",           CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
    { "[DEBUG]", "GC: freed 12 objects",                 CLR_DEBUG_R, CLR_DEBUG_G, CLR_DEBUG_B },
    { "[INFO]",  "Texture cache: 24 entries",            CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
    { "[WARN]",  "Shader compilation slow: 340ms",       CLR_WARN_R,  CLR_WARN_G,  CLR_WARN_B  },
    { "[INFO]",  "Audio: 3 sources active",              CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
    { "[ERROR]", "Failed to load audio: explosion.wav",  CLR_ERROR_R, CLR_ERROR_G, CLR_ERROR_B },
    { "[INFO]",  "Physics step: 2.1ms avg",              CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
    { "[WARN]",  "Texture missing: bark_normal.png",     CLR_WARN_R,  CLR_WARN_G,  CLR_WARN_B  },
    { "[INFO]",  "Draw calls: 142",                      CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
    { "[INFO]",  "Triangles: 84,291",                    CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
    { "[DEBUG]", "Net: ping 42ms",                       CLR_DEBUG_R, CLR_DEBUG_G, CLR_DEBUG_B },
    { "[WARN]",  "Frame budget exceeded: 18.2ms",        CLR_WARN_R,  CLR_WARN_G,  CLR_WARN_B  },
    { "[INFO]",  "Shadow map: 2048x2048",                CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
    { "[ERROR]", "GPU memory low: 87% used",             CLR_ERROR_R, CLR_ERROR_G, CLR_ERROR_B },
    { "[INFO]",  "Post-process: bloom enabled",          CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
    { "[DEBUG]", "Entity pool: 1247 / 2048 active",      CLR_DEBUG_R, CLR_DEBUG_G, CLR_DEBUG_B },
    { "[INFO]",  "Scene rendered in 14.3ms",             CLR_INFO_R,  CLR_INFO_G,  CLR_INFO_B  },
};

#define LOG_COUNT ((int)(sizeof(LOG_ENTRIES) / sizeof(LOG_ENTRIES[0])))

static bool render_console(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    /* Center the console panel on the framebuffer */
    float panel_x = ((float)FB_WIDTH - CONSOLE_WIDTH) * 0.5f;
    float panel_y = ((float)FB_HEIGHT - CONSOLE_HEIGHT) * 0.5f;

    float scroll_y = 0.0f;
    ForgeUiRect panel_rect = { panel_x, panel_y, CONSOLE_WIDTH, CONSOLE_HEIGHT };

    if (forge_ui_ctx_panel_begin(&ctx, "Console", panel_rect, &scroll_y)) {
        /* panel_begin pushes a vertical layout — each layout_next call
         * returns the next row rect.  We position the severity tag and
         * message text manually within each row for precise alignment. */
        float ascender_px = forge_ui__ascender_px(&atlas);

        for (int i = 0; i < LOG_COUNT; i++) {
            ForgeUiRect row = forge_ui_ctx_layout_next(&ctx, ROW_HEIGHT);

            /* Baseline: vertically center the text within the row using
             * the font's pixel height and ascender metrics */
            float text_y = row.y + (row.h - atlas.pixel_height) * 0.5f
                         + ascender_px;

            /* Severity tag — colored by log level */
            forge_ui_ctx_label_colored(&ctx, LOG_ENTRIES[i].level,
                                       row.x, text_y,
                                       LOG_ENTRIES[i].r,
                                       LOG_ENTRIES[i].g,
                                       LOG_ENTRIES[i].b, 1.0f);

            /* Message text — uses the theme's default text color */
            forge_ui_ctx_label(&ctx, LOG_ENTRIES[i].message,
                               row.x + LEVEL_TAG_WIDTH, text_y);
        }

        forge_ui_ctx_panel_end(&ctx);
    }

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, FB_HEIGHT,
                               "console.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image 4: Performance Overlay
 *
 * A compact panel anchored to the top-right corner showing the metrics a
 * developer watches during optimization: FPS, frame time with a sparkline
 * history, draw call and triangle counts, GPU memory usage with a progress
 * bar, and entity counts.
 *
 * All values are hardcoded — this is a UI layout demonstration, not a
 * profiler.  The sparkline uses synthetic frame time data with a few
 * intentional spikes to show how the graph highlights anomalies.
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Performance overlay constants ────────────────────────────────────── */

#define PERF_PANEL_W         350.0f   /* overlay panel width */
#define PERF_PANEL_H         380.0f   /* overlay panel height */
#define PERF_FPS_ROW_H        32.0f   /* taller row for the large FPS counter */
#define PERF_STAT_ROW_H       22.0f   /* standard row height for stat labels */
#define PERF_SPARKLINE_H      50.0f   /* height of the frame time graph */
#define PERF_BAR_H            14.0f   /* height of the GPU memory progress bar */
#define PERF_SEPARATOR_H       8.0f   /* spacing for horizontal separators */
#define GRAPH_SAMPLES          60     /* number of frame time samples */
#define FRAME_TIME_MIN         0.0f   /* sparkline Y-axis minimum (ms) */
#define FRAME_TIME_MAX        33.3f   /* sparkline Y-axis maximum (ms, ~30 FPS) */
#define FRAME_TIME_NOISE_MOD    7     /* modulus for synthetic noise pattern */
#define FRAME_TIME_NOISE_AMP  1.2f    /* amplitude of noise variation (ms) */
#define FRAME_TIME_FLOOR     14.0f    /* minimum synthetic frame time (ms) */
#define SPIKE_INDEX_A          23     /* first spike position in the sample array */
#define SPIKE_INDEX_B          45     /* second spike position */
#define SPIKE_VALUE_A         22.0f   /* first spike frame time (ms) */
#define SPIKE_VALUE_B         25.0f   /* second spike frame time (ms) */

/* GPU memory usage for the progress bar */
#define GPU_MEM_USED         412.0f   /* current usage in MB */
#define GPU_MEM_TOTAL        512.0f   /* total available in MB */

/* ── Color for the GPU memory bar ─────────────────────────────────────── */

static const ForgeUiColor COLOR_GPU_MEM = { 0.40f, 0.70f, 0.95f, 1.0f };

static bool render_perf_overlay(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    /* ── Generate synthetic frame time data ────────────────────────────── */
    /* The pattern produces a sawtooth wave with two deliberate spikes,
     * simulating the kind of frame time variability that a developer
     * would want the overlay to surface. */
    float frame_times[GRAPH_SAMPLES];
    for (int i = 0; i < GRAPH_SAMPLES; i++) {
        frame_times[i] = FRAME_TIME_FLOOR
                       + (float)(i % FRAME_TIME_NOISE_MOD) * FRAME_TIME_NOISE_AMP;
    }
    frame_times[SPIKE_INDEX_A] = SPIKE_VALUE_A;
    frame_times[SPIKE_INDEX_B] = SPIKE_VALUE_B;

    /* ── Panel position: top-right corner ──────────────────────────────── */
    float panel_x = (float)FB_WIDTH - PERF_PANEL_W - MARGIN;
    float panel_y = MARGIN;

    float scroll_y = 0.0f;
    ForgeUiRect panel_rect = { panel_x, panel_y, PERF_PANEL_W, PERF_PANEL_H };

    if (forge_ui_ctx_panel_begin(&ctx, "Performance", panel_rect, &scroll_y)) {
        /* ── FPS counter ───────────────────────────────────────────────── */
        /* Large, prominent label — the single most-watched number */
        forge_ui_ctx_label_colored_layout(&ctx, "60 FPS", PERF_FPS_ROW_H,
                                          ctx.theme.accent.r,
                                          ctx.theme.accent.g,
                                          ctx.theme.accent.b,
                                          ctx.theme.accent.a);

        /* ── Frame time + sparkline ────────────────────────────────────── */
        forge_ui_ctx_label_layout(&ctx, "Frame: 16.7 ms", PERF_STAT_ROW_H);

        /* Sparkline showing recent frame time history — spikes are
         * immediately visible as peaks above the baseline band */
        ForgeUiColor spark_color = ctx.theme.accent;
        forge_ui_ctx_sparkline_layout(&ctx, frame_times, GRAPH_SAMPLES,
                                      FRAME_TIME_MIN, FRAME_TIME_MAX,
                                      spark_color, PERF_SPARKLINE_H);

        /* ── Separator between timing and resource stats ───────────────── */
        forge_ui_ctx_separator_layout(&ctx, PERF_SEPARATOR_H);

        /* ── Rendering stats ───────────────────────────────────────────── */
        forge_ui_ctx_label_layout(&ctx, "Draw Calls:    142", PERF_STAT_ROW_H);
        forge_ui_ctx_label_layout(&ctx, "Triangles:     84,291", PERF_STAT_ROW_H);

        /* GPU memory: label + progress bar showing utilization */
        char mem_buf[64];
        SDL_snprintf(mem_buf, sizeof(mem_buf),
                     "GPU Memory:    %.0f / %.0f MB",
                     GPU_MEM_USED, GPU_MEM_TOTAL);
        forge_ui_ctx_label_layout(&ctx, mem_buf, PERF_STAT_ROW_H);
        forge_ui_ctx_progress_bar_layout(&ctx, GPU_MEM_USED, GPU_MEM_TOTAL,
                                         COLOR_GPU_MEM, PERF_BAR_H);

        /* CPU and GPU frame times — useful for identifying the bottleneck */
        forge_ui_ctx_label_layout(&ctx, "CPU Time:      8.4 ms", PERF_STAT_ROW_H);
        forge_ui_ctx_label_layout(&ctx, "GPU Time:      11.2 ms", PERF_STAT_ROW_H);

        /* ── Separator before entity counts ────────────────────────────── */
        forge_ui_ctx_separator_layout(&ctx, PERF_SEPARATOR_H);

        /* ── Entity counts ─────────────────────────────────────────────── */
        forge_ui_ctx_label_layout(&ctx, "Entities:      1,247", PERF_STAT_ROW_H);
        forge_ui_ctx_label_layout(&ctx, "Active:        892", PERF_STAT_ROW_H);
        forge_ui_ctx_label_layout(&ctx, "Lights:        6", PERF_STAT_ROW_H);

        forge_ui_ctx_panel_end(&ctx);
    }

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, FB_HEIGHT,
                               "perf_overlay.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}
/* ═══════════════════════════════════════════════════════════════════════════
 * Image 5: Scene Tree
 *
 * A hierarchical tree view of game objects in a scrollable panel.
 * Demonstrates the tree_push / tree_pop pattern at multiple nesting
 * levels — the same structure used in game-engine scene graphs where
 * objects parent other objects (camera attached to player, terrain
 * meshes grouped under environment, etc.).
 *
 * Branch nodes use tree_push_layout and draw a +/- toggle indicator.
 * Leaf nodes are plain labels indented to the current depth.
 * ═══════════════════════════════════════════════════════════════════════ */

#define SCENE_PANEL_W    400.0f   /* scene tree panel width */
#define SCENE_PANEL_H    500.0f   /* scene tree panel height */

/* ── Helper: emit a leaf label indented to a given depth ───────────────
 *
 * Tree nodes drawn by tree_push_layout already include the +/- indicator
 * and handle their own sizing.  Leaf nodes have no toggle — they are
 * ordinary labels shifted right by (depth * INDENT) so they visually
 * nest under their parent branch. */

static void emit_leaf(ForgeUiContext *ctx, const char *text, float depth)
{
    /* Reserve a row from the active layout */
    ForgeUiRect row = forge_ui_ctx_layout_next(ctx, ROW_HEIGHT);

    /* Indent the label to match the tree depth */
    float indent = depth * INDENT;

    /* Vertically center the text within the row using the ascender-based
     * baseline calculation — same approach as label_colored_layout. */
    float asc = forge_ui__ascender_px(ctx->atlas);
    float text_y = row.y + (row.h - ctx->atlas->pixel_height) * 0.5f + asc;

    forge_ui_ctx_label(ctx, text, row.x + indent, text_y);
}

/* ── Helper: emit a branch node (tree_push) at a given depth ──────────
 *
 * Allocates a row from the active layout and passes an indented sub-rect
 * to tree_push so the +/- indicator and label appear at the correct
 * nesting level.  Returns the current open state — callers must still
 * call tree_pop after emitting children. */

static bool emit_branch(ForgeUiContext *ctx, const char *label,
                         bool *open, float depth)
{
    ForgeUiRect row = forge_ui_ctx_layout_next(ctx, ROW_HEIGHT);
    float indent = depth * INDENT;
    ForgeUiRect node_rect = {
        row.x + indent, row.y,
        row.w - indent, row.h
    };
    return forge_ui_ctx_tree_push(ctx, label, open, node_rect);
}

static bool render_scene_tree(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    /* Center the panel in the framebuffer */
    float panel_x = (FB_WIDTH  - SCENE_PANEL_W) * 0.5f;
    float panel_y = (FB_HEIGHT - SCENE_PANEL_H) * 0.5f;
    ForgeUiRect panel_rect = { panel_x, panel_y, SCENE_PANEL_W, SCENE_PANEL_H };

    float scroll_y = 0.0f;
    if (forge_ui_ctx_panel_begin(&ctx, "Scene", panel_rect, &scroll_y)) {

        /* The panel pushes an automatic vertical layout — every
         * layout_next / tree_push_layout call advances top-to-bottom.
         *
         * Open/closed state for each branch is stored in local bools.
         * In a real editor these would live in persistent per-node data;
         * here we initialise them to show the desired default view. */

        /* ── Root: World (depth 0, expanded) ──────────────────────── */
        bool world_open = true;
        if (emit_branch(&ctx, "World", &world_open, 0.0f)) {

            /* ── Player (depth 1, expanded) ───────────────────────── */
            bool player_open = true;
            if (emit_branch(&ctx, "Player", &player_open, 1.0f)) {
                emit_leaf(&ctx, "Camera",         2.0f);
                emit_leaf(&ctx, "Mesh: hero.gltf", 2.0f);
                emit_leaf(&ctx, "PointLight",     2.0f);
            }
            forge_ui_ctx_tree_pop(&ctx);

            /* ── Environment (depth 1, expanded) ──────────────────── */
            bool env_open = true;
            if (emit_branch(&ctx, "Environment", &env_open, 1.0f)) {

                /* Terrain (depth 2, expanded) */
                bool terrain_open = true;
                if (emit_branch(&ctx, "Terrain", &terrain_open, 2.0f)) {
                    emit_leaf(&ctx, "Ground Mesh",     3.0f);
                    emit_leaf(&ctx, "Grass Instances", 3.0f);
                }
                forge_ui_ctx_tree_pop(&ctx);

                /* Trees (depth 2, collapsed) */
                bool trees_open = false;
                emit_branch(&ctx, "Trees", &trees_open, 2.0f);
                forge_ui_ctx_tree_pop(&ctx);

                emit_leaf(&ctx, "Skybox", 2.0f);
            }
            forge_ui_ctx_tree_pop(&ctx);

            /* ── Enemies (depth 1, expanded) ──────────────────────── */
            bool enemies_open = true;
            if (emit_branch(&ctx, "Enemies", &enemies_open, 1.0f)) {
                emit_leaf(&ctx, "Enemy_001", 2.0f);
                emit_leaf(&ctx, "Enemy_002", 2.0f);

                /* Enemy_003 (depth 2, collapsed) */
                bool e003_open = false;
                emit_branch(&ctx, "Enemy_003", &e003_open, 2.0f);
                forge_ui_ctx_tree_pop(&ctx);
            }
            forge_ui_ctx_tree_pop(&ctx);

            /* ── UI Canvas (depth 1, collapsed) ───────────────────── */
            bool canvas_open = false;
            emit_branch(&ctx, "UI Canvas", &canvas_open, 1.0f);
            forge_ui_ctx_tree_pop(&ctx);

            /* ── DirectionalLight (depth 1, leaf) ─────────────────── */
            emit_leaf(&ctx, "DirectionalLight", 1.0f);
        }
        forge_ui_ctx_tree_pop(&ctx);

        /* Separator after the tree for visual closure */
        forge_ui_ctx_separator_layout(&ctx, ROW_HEIGHT * 0.5f);

        forge_ui_ctx_panel_end(&ctx);
    }

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, FB_HEIGHT,
                                "scene_tree.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image 6: Controls — Listbox, Dropdown, and Radio Buttons
 *
 * Demonstrates the selection-type controls: a listbox with clickable items,
 * a dropdown combo box (shown in its expanded state), and a group of radio
 * buttons sharing a single selection state.
 * ═══════════════════════════════════════════════════════════════════════ */

#define CONTROLS_W    400.0f   /* controls panel width */
#define CONTROLS_H    680.0f   /* controls panel height */
#define CONTROLS_FB_H 750      /* taller framebuffer to fit all controls */

static bool render_controls(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    float panel_x = ((float)FB_WIDTH - CONTROLS_W) * 0.5f;
    float panel_y = ((float)CONTROLS_FB_H - CONTROLS_H) * 0.5f;

    float scroll_y = 0.0f;
    ForgeUiRect panel_rect = { panel_x, panel_y, CONTROLS_W, CONTROLS_H };

    if (forge_ui_ctx_panel_begin(&ctx, "Controls", panel_rect, &scroll_y)) {

        /* ── Listbox ───────────────────────────────────────────────────── */
        forge_ui_ctx_label_layout(&ctx, "Shader List", ROW_HEIGHT);

        static const char *const shaders[] = {
            "Blinn-Phong", "PBR Metallic", "Toon / Cel",
            "Wireframe", "Normal Debug", "Depth Only"
        };
        int shader_sel = 1;  /* PBR Metallic selected by default */
        float list_h = FORGE_UI_LB_ITEM_HEIGHT * 6.0f;
        forge_ui_ctx_listbox_layout(&ctx, "##shaders", &shader_sel,
                                     shaders, 6, list_h);

        forge_ui_ctx_separator_layout(&ctx, ROW_HEIGHT * 0.5f);

        /* ── Dropdown (shown expanded) ─────────────────────────────────── */
        forge_ui_ctx_label_layout(&ctx, "Render Mode", ROW_HEIGHT);

        static const char *const modes[] = {
            "Forward", "Deferred", "Forward+", "Raytraced"
        };
        int mode_sel = 0;
        bool dropdown_open = true;  /* show expanded for the demo */
        forge_ui_ctx_dropdown_layout(&ctx, "##mode", &mode_sel,
                                      &dropdown_open, modes, 4,
                                      FORGE_UI_DD_HEADER_HEIGHT);

        /* Skip extra space to account for the expanded dropdown items
         * that extend below the header (4 items * item height). */
        float dropdown_extra = FORGE_UI_LB_ITEM_HEIGHT * 4.0f;
        forge_ui_ctx_layout_next(&ctx, dropdown_extra);

        forge_ui_ctx_separator_layout(&ctx, ROW_HEIGHT * 0.5f);

        /* ── Radio buttons ─────────────────────────────────────────────── */
        forge_ui_ctx_label_layout(&ctx, "Shadow Quality", ROW_HEIGHT);

        int shadow_quality = 2;  /* High selected */
        forge_ui_push_id(&ctx, "shadow_q");
        forge_ui_ctx_radio_layout(&ctx, "Off",    &shadow_quality, 0, ROW_HEIGHT);
        forge_ui_ctx_radio_layout(&ctx, "Low",    &shadow_quality, 1, ROW_HEIGHT);
        forge_ui_ctx_radio_layout(&ctx, "High",   &shadow_quality, 2, ROW_HEIGHT);
        forge_ui_ctx_radio_layout(&ctx, "Ultra",  &shadow_quality, 3, ROW_HEIGHT);
        forge_ui_pop_id(&ctx);

        forge_ui_ctx_panel_end(&ctx);
    }

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, CONTROLS_FB_H,
                               "controls.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image 7: Color Picker
 *
 * An HSV color picker with a saturation-value gradient area, a horizontal
 * hue slider bar, and a color preview swatch with RGB readout.
 * ═══════════════════════════════════════════════════════════════════════ */

#define PICKER_W     350.0f   /* color picker panel width */
#define PICKER_H     400.0f   /* color picker panel height */

static bool render_color_picker(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    float panel_x = ((float)FB_WIDTH - PICKER_W) * 0.5f;
    float panel_y = ((float)FB_HEIGHT - PICKER_H) * 0.5f;

    float scroll_y = 0.0f;
    ForgeUiRect panel_rect = { panel_x, panel_y, PICKER_W, PICKER_H };

    if (forge_ui_ctx_panel_begin(&ctx, "Color Picker", panel_rect, &scroll_y)) {

        /* Start with a cyan-ish color */
        float h = 195.0f, s = 0.75f, v = 0.85f;

        /* The color picker uses the remaining panel height */
        float picker_h = PICKER_H - FORGE_UI_PANEL_TITLE_HEIGHT
                        - 2.0f * FORGE_UI_PANEL_PADDING;
        forge_ui_ctx_color_picker_layout(&ctx, "##picker",
                                          &h, &s, &v, picker_h);

        forge_ui_ctx_panel_end(&ctx);
    }

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, FB_HEIGHT,
                               "color_picker.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main — Load font, generate all seven dev-UI demo images, report results.
 *
 * Each render function creates its own atlas and context, draws one image,
 * rasterizes to BMP, and frees all resources before returning.  This keeps
 * each demo self-contained and simplifies error handling.
 * ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* SDL_Init(0) initialises the base subsystem (timers, file I/O).
     * No video or audio — we only need the logging and file helpers. */
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    /* Build the ASCII codepoint table used by every font atlas */
    init_codepoints();

    /* Load the TTF font file — all seven images share this font data,
     * but each builds its own atlas at the size it needs. */
    ForgeUiFont font;
    if (!forge_ui_ttf_load(FONT_PATH, &font)) {
        SDL_Log("Failed to load font: %s", FONT_PATH);
        SDL_Quit();
        return 1;
    }

    SDL_Log("=== UI Lesson 15 -- Dev UI ===");
    SDL_Log(" ");

    int pass = 0;
    int fail = 0;

    /* Image 1: Property Inspector — read-only collapsible sections with
     * sliders, checkboxes, and labels arranged in a panel layout. */
    if (render_property_inspector(&font)) {
        SDL_Log("[OK] property_inspector.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] property_inspector.bmp");
        fail++;
    }

    /* Image 2: Property Editor — editable values with drag_float,
     * drag_int, and multi-component drag fields. */
    if (render_property_editor(&font)) {
        SDL_Log("[OK] property_editor.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] property_editor.bmp");
        fail++;
    }

    /* Image 3: Console — scrollable log output with colored severity
     * tags, mimicking an in-game developer console. */
    if (render_console(&font)) {
        SDL_Log("[OK] console.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] console.bmp");
        fail++;
    }

    /* Image 4: Performance Overlay — real-time stats display with
     * sparkline graphs, frame time counters, and GPU memory readouts. */
    if (render_perf_overlay(&font)) {
        SDL_Log("[OK] perf_overlay.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] perf_overlay.bmp");
        fail++;
    }

    /* Image 5: Scene Tree — hierarchical tree view of game objects
     * demonstrating nested tree_push / tree_pop at multiple depths. */
    if (render_scene_tree(&font)) {
        SDL_Log("[OK] scene_tree.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] scene_tree.bmp");
        fail++;
    }

    /* Image 6: Controls — listbox, dropdown, and radio buttons
     * demonstrating selection-type input controls. */
    if (render_controls(&font)) {
        SDL_Log("[OK] controls.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] controls.bmp");
        fail++;
    }

    /* Image 7: Color Picker — HSV color picker with saturation-value
     * gradient, hue bar, and color preview swatch. */
    if (render_color_picker(&font)) {
        SDL_Log("[OK] color_picker.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] color_picker.bmp");
        fail++;
    }

    SDL_Log(" ");
    SDL_Log("Results: %d passed, %d failed", pass, fail);

    forge_ui_ttf_free(&font);
    SDL_Quit();
    return fail > 0 ? 1 : 0;
}
