/*
 * UI Lesson 14 — Game UI
 *
 * Demonstrates common game UI patterns composed from the immediate-mode
 * controls built in earlier lessons: progress bars for player stats,
 * inventory grids, HUD anchoring, action bars, and pause menus.
 *
 * Output: four BMP images showing each pattern.
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
#define HUD_WIDTH  1280           /* HUD demo uses 16:9 dimensions */
#define HUD_HEIGHT 720            /* HUD demo height (16:9) */

/* ── Font and layout ───────────────────────────────────────────────────── */

#define FONT_PATH       "assets/fonts/liberation_mono/LiberationMono-Regular.ttf" /* TTF font asset */
#define BASE_PIXEL_HEIGHT 18.0f   /* base font size in pixels */
#define ROW_HEIGHT       28.0f    /* widget row height for layout */
#define MARGIN           16.0f    /* outer margin from screen edges */
#define BAR_HEIGHT       20.0f    /* progress bar height */
#define BAR_LABEL_W      80.0f    /* label column width for stat bars */
#define BAR_TRACK_W      200.0f   /* track width for stat bars */
#define BAR_SPACING       6.0f    /* vertical gap between stat bars */
#define BAR_VALUE_GAP     8.0f    /* gap between bar end and numeric value */
#define BAR_LABEL_BASE   14.0f    /* vertical offset to center label with bar */
#define TITLE_BASELINE   20.0f    /* baseline offset for section titles */
#define TITLE_START_Y    44.0f    /* Y offset where first bar row begins */

/* ── Inventory grid ────────────────────────────────────────────────────── */

#define INV_COLS          4       /* inventory columns */
#define INV_ROWS          4       /* inventory rows */
#define INV_SLOT_SIZE    72.0f    /* slot button width and height */
#define INV_SLOT_GAP      4.0f    /* gap between inventory slots */
#define INV_PANEL_PAD    10.0f    /* panel padding around the grid */

/* ── HUD layout ────────────────────────────────────────────────────────── */

#define HUD_BAR_W        220.0f   /* stat bar width in HUD */
#define HUD_BAR_H         16.0f   /* stat bar height in HUD */
#define HUD_ACTION_BTN_W  64.0f   /* action bar button width */
#define HUD_ACTION_BTN_H  40.0f   /* action bar button height */
#define HUD_ACTION_GAP     6.0f   /* gap between action bar buttons */
#define HUD_ACTION_COUNT   6      /* number of action bar buttons */
#define HUD_LABEL_BASE    12.0f   /* vertical baseline offset for HUD labels */
#define HUD_LABEL_OFFSET  28.0f   /* horizontal offset past label to bar start */
#define HUD_BAR_GAP        4.0f   /* vertical gap between HUD stat bars */
/* HUD_LEVEL_TEXT_W removed — measured from the atlas at runtime */
#define HUD_XP_BAR_H       8.0f   /* XP bar height at screen bottom */
#define HUD_XP_LABEL_OFF   4.0f   /* vertical offset for XP label above bar */
#define HUD_XP_GAP        10.0f   /* gap between XP bar and action bar above */

/* ── Pause menu ────────────────────────────────────────────────────────── */

#define MENU_WIDTH       280.0f   /* pause menu panel width */
#define MENU_BTN_H        40.0f   /* pause menu button height */
#define MENU_SPACING      10.0f   /* gap between menu buttons */
#define MENU_PAD          20.0f   /* menu panel padding */
#define MENU_BTN_COUNT     3      /* number of pause menu buttons */
#define PANEL_TITLE_H     30.0f   /* height reserved for panel title bar */
#define DIM_ALPHA          0.5f   /* alpha for pause menu dim overlay */

/* ── Stat bar colors ───────────────────────────────────────────────────── */

/* Health: red-ish */
static const ForgeUiColor COLOR_HEALTH = { 0.85f, 0.20f, 0.18f, 1.0f };
/* Mana: blue */
static const ForgeUiColor COLOR_MANA   = { 0.20f, 0.45f, 0.90f, 1.0f };
/* Stamina: green */
static const ForgeUiColor COLOR_STAMINA = { 0.25f, 0.75f, 0.30f, 1.0f };
/* XP: gold */
static const ForgeUiColor COLOR_XP     = { 0.90f, 0.75f, 0.20f, 1.0f };

/* ── Codepoint table for font atlas ────────────────────────────────────── */

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
    /* Clear to surface_active — darker than bg so panels stand out */
    forge_raster_clear(&fb,
                       ctx->theme.surface_active.r,
                       ctx->theme.surface_active.g,
                       ctx->theme.surface_active.b,
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
 * Image 1: Status Bars
 *
 * Four progress bars showing health, mana, stamina, and XP with labels.
 * Demonstrates forge_ui_ctx_progress_bar alongside labels for player
 * stat display — the most common game UI element.
 * ═══════════════════════════════════════════════════════════════════════ */

static bool render_status_bars(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    /* Title */
    forge_ui_ctx_label(&ctx, "Player Status", MARGIN, MARGIN + TITLE_BASELINE);

    /* Each stat: label on the left, progress bar on the right */
    float y = MARGIN + TITLE_START_Y;

    /* Health: 72/100 */
    forge_ui_ctx_label(&ctx, "Health", MARGIN, y + BAR_LABEL_BASE);
    ForgeUiRect hp_rect = { MARGIN + BAR_LABEL_W, y, BAR_TRACK_W, BAR_HEIGHT };
    forge_ui_ctx_progress_bar(&ctx, 72.0f, 100.0f, COLOR_HEALTH, hp_rect);
    forge_ui_ctx_label(&ctx, "72 / 100",
                       MARGIN + BAR_LABEL_W + BAR_TRACK_W + BAR_VALUE_GAP,
                       y + BAR_LABEL_BASE);
    y += BAR_HEIGHT + BAR_SPACING;

    /* Mana: 45/80 */
    forge_ui_ctx_label(&ctx, "Mana", MARGIN, y + BAR_LABEL_BASE);
    ForgeUiRect mp_rect = { MARGIN + BAR_LABEL_W, y, BAR_TRACK_W, BAR_HEIGHT };
    forge_ui_ctx_progress_bar(&ctx, 45.0f, 80.0f, COLOR_MANA, mp_rect);
    forge_ui_ctx_label(&ctx, "45 / 80",
                       MARGIN + BAR_LABEL_W + BAR_TRACK_W + BAR_VALUE_GAP,
                       y + BAR_LABEL_BASE);
    y += BAR_HEIGHT + BAR_SPACING;

    /* Stamina: 90/100 */
    forge_ui_ctx_label(&ctx, "Stamina", MARGIN, y + BAR_LABEL_BASE);
    ForgeUiRect st_rect = { MARGIN + BAR_LABEL_W, y, BAR_TRACK_W, BAR_HEIGHT };
    forge_ui_ctx_progress_bar(&ctx, 90.0f, 100.0f, COLOR_STAMINA, st_rect);
    forge_ui_ctx_label(&ctx, "90 / 100",
                       MARGIN + BAR_LABEL_W + BAR_TRACK_W + BAR_VALUE_GAP,
                       y + BAR_LABEL_BASE);
    y += BAR_HEIGHT + BAR_SPACING;

    /* XP: 1250/2000 */
    forge_ui_ctx_label(&ctx, "XP", MARGIN, y + BAR_LABEL_BASE);
    ForgeUiRect xp_rect = { MARGIN + BAR_LABEL_W, y, BAR_TRACK_W, BAR_HEIGHT };
    forge_ui_ctx_progress_bar(&ctx, 1250.0f, 2000.0f, COLOR_XP, xp_rect);
    forge_ui_ctx_label(&ctx, "1250 / 2000",
                       MARGIN + BAR_LABEL_W + BAR_TRACK_W + BAR_VALUE_GAP,
                       y + BAR_LABEL_BASE);

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, FB_HEIGHT,
                               "status_bars.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image 2: Inventory Grid
 *
 * A panel titled "Inventory" containing a 5-column × 4-row grid of item
 * slots.  Each slot is a button.  Occupied slots show an item name;
 * empty slots are blank.  Uses nested layouts: a vertical layout for
 * rows, with a horizontal layout per row for columns.
 * ═══════════════════════════════════════════════════════════════════════ */

/* Item data — NULL means the slot is empty */
static const char *INVENTORY_ITEMS[INV_ROWS][INV_COLS] = {
    { "Sword",  "Shield", "Helm",   NULL     },
    { "Potion", "Potion", "Arrow",  "Arrow"  },
    { "Ring",   NULL,     "Scroll", "Gem"    },
    { NULL,     NULL,     NULL,     NULL     },
};

static bool render_inventory(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    /* Configure panel spacing to match the inventory layout constants */
    ctx.spacing.panel_padding = INV_PANEL_PAD;
    ctx.spacing.title_bar_height = PANEL_TITLE_H;
    ctx.spacing.item_spacing = INV_SLOT_GAP;

    /* Panel dimensions to fit the grid — derived from ctx.spacing values */
    float grid_w = INV_COLS * INV_SLOT_SIZE + (INV_COLS - 1) * ctx.spacing.item_spacing;
    float grid_h = INV_ROWS * INV_SLOT_SIZE + (INV_ROWS - 1) * ctx.spacing.item_spacing;
    float panel_w = grid_w + 2.0f * ctx.spacing.panel_padding
                  + ctx.spacing.scrollbar_width;
    float panel_h = grid_h + 2.0f * ctx.spacing.panel_padding + ctx.spacing.title_bar_height;

    float panel_x = (FB_WIDTH - panel_w) * 0.5f;
    float panel_y = (FB_HEIGHT - panel_h) * 0.5f;

    float scroll_y = 0.0f;
    ForgeUiRect panel_rect = { panel_x, panel_y, panel_w, panel_h };

    if (forge_ui_ctx_panel_begin(&ctx, "Inventory", panel_rect, &scroll_y)) {
        /* The panel pushes a vertical layout.  We draw rows manually
         * using nested horizontal layouts for precise grid control. */

        for (int row = 0; row < INV_ROWS; row++) {
            /* Get the next row-height slot from the panel's vertical layout */
            ForgeUiRect row_rect = forge_ui_ctx_layout_next(&ctx, INV_SLOT_SIZE);

            /* Push a horizontal layout for this row's columns */
            forge_ui_ctx_layout_push(&ctx, row_rect,
                                     FORGE_UI_LAYOUT_HORIZONTAL,
                                     -1.0f,  /* use default padding */
                                     INV_SLOT_GAP);

            for (int col = 0; col < INV_COLS; col++) {
                const char *item = INVENTORY_ITEMS[row][col];
                /* Build a unique ID for each slot: "slot_R_C" */
                char id_buf[32];
                SDL_snprintf(id_buf, sizeof(id_buf), "%s##slot_%d_%d",
                             item ? item : " ", row, col);
                forge_ui_ctx_button_layout(&ctx, id_buf, INV_SLOT_SIZE);
            }

            forge_ui_ctx_layout_pop(&ctx);
        }

        forge_ui_ctx_panel_end(&ctx);
    }

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, FB_HEIGHT,
                               "inventory.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image 3: HUD Layout
 *
 * A complete heads-up display at 1280×720 showing how to anchor UI
 * elements at screen edges.  Top-left: health and mana bars.  Bottom-
 * center: horizontal action bar with ability buttons.  Top-right: a
 * small "Level 12" label.  Proportional positioning uses the screen
 * width and height as the reference — no magic pixel offsets.
 * ═══════════════════════════════════════════════════════════════════════ */

/* Action bar button labels */
static const char *ACTION_LABELS[HUD_ACTION_COUNT] = {
    "1:Atk", "2:Def", "3:Mag", "4:Pot", "5:Run", "6:Map"
};

static bool render_hud(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    /* ── Top-left: player stat bars ────────────────────────────────────── */
    float stat_x = MARGIN;
    float stat_y = MARGIN;

    /* Health bar with label */
    forge_ui_ctx_label(&ctx, "HP", stat_x, stat_y + HUD_LABEL_BASE);
    ForgeUiRect hp = { stat_x + HUD_LABEL_OFFSET, stat_y, HUD_BAR_W, HUD_BAR_H };
    forge_ui_ctx_progress_bar(&ctx, 72.0f, 100.0f, COLOR_HEALTH, hp);
    stat_y += HUD_BAR_H + HUD_BAR_GAP;

    /* Mana bar with label */
    forge_ui_ctx_label(&ctx, "MP", stat_x, stat_y + HUD_LABEL_BASE);
    ForgeUiRect mp = { stat_x + HUD_LABEL_OFFSET, stat_y, HUD_BAR_W, HUD_BAR_H };
    forge_ui_ctx_progress_bar(&ctx, 45.0f, 80.0f, COLOR_MANA, mp);

    /* ── Top-right: level indicator ────────────────────────────────────── */
    /* Anchor from the right edge using measured text width from the atlas.
     * This adapts to the actual font metrics instead of a hardcoded guess. */
    ForgeUiTextMetrics level_m = forge_ui_text_measure(&atlas, "Level 12", NULL);
    float level_x = (float)HUD_WIDTH - MARGIN - level_m.width;
    forge_ui_ctx_label(&ctx, "Level 12", level_x, MARGIN + BAR_LABEL_BASE);

    /* ── Bottom-center: action bar ─────────────────────────────────────── */
    /* Compute total action bar width, then center it horizontally. */
    float bar_total_w = HUD_ACTION_COUNT * HUD_ACTION_BTN_W +
                        (HUD_ACTION_COUNT - 1) * HUD_ACTION_GAP;
    float bar_x = ((float)HUD_WIDTH - bar_total_w) * 0.5f;
    float bar_y = (float)HUD_HEIGHT - MARGIN - HUD_ACTION_BTN_H;

    /* Use a horizontal layout for evenly spaced buttons */
    ForgeUiRect bar_rect = { bar_x, bar_y, bar_total_w, HUD_ACTION_BTN_H };
    forge_ui_ctx_layout_push(&ctx, bar_rect,
                             FORGE_UI_LAYOUT_HORIZONTAL,
                             -1.0f,         /* no padding */
                             HUD_ACTION_GAP);

    for (int i = 0; i < HUD_ACTION_COUNT; i++) {
        forge_ui_ctx_button_layout(&ctx, ACTION_LABELS[i],
                                   HUD_ACTION_BTN_W);
    }

    forge_ui_ctx_layout_pop(&ctx);

    /* ── Bottom-left: XP bar spanning most of the screen width ─────────── */
    float xp_y = bar_y - MARGIN - HUD_XP_GAP;
    float xp_w = (float)HUD_WIDTH - 2.0f * MARGIN;
    ForgeUiRect xp = { MARGIN, xp_y, xp_w, HUD_XP_BAR_H };
    forge_ui_ctx_progress_bar(&ctx, 1250.0f, 2000.0f, COLOR_XP, xp);
    forge_ui_ctx_label(&ctx, "XP: 1250 / 2000", MARGIN, xp_y - HUD_XP_LABEL_OFF);

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, HUD_WIDTH, HUD_HEIGHT, "hud.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image 4: Pause Menu
 *
 * A centered panel overlay on a dimmed background.  The panel contains
 * a title ("Paused") and three buttons: Resume, Settings, Quit.  This
 * pattern covers modal overlays — the game world is still visible behind
 * a semi-transparent dim layer, and a single focused panel captures input.
 * ═══════════════════════════════════════════════════════════════════════ */

static bool render_pause_menu(const ForgeUiFont *font)
{
    ForgeUiFontAtlas atlas;
    ForgeUiContext ctx;
    if (!create_ui(font, &atlas, &ctx, 1.0f)) return false;

    forge_ui_ctx_begin(&ctx, -1.0f, -1.0f, false);

    /* ── Dim overlay ───────────────────────────────────────────────────── */
    /* A semi-transparent dark rectangle covering the entire screen.
     * In a real game this would overlay the rendered scene.  Here we
     * draw it first so the menu panel appears on top. */
    ForgeUiRect dim = { 0.0f, 0.0f, (float)FB_WIDTH, (float)FB_HEIGHT };
    /* A single semi-transparent black quad.  forge_ui_ctx_rect emits
     * one quad with no background track, so the overlay blends
     * correctly against the scene behind it on the GPU. */
    ForgeUiColor dim_color = { 0.0f, 0.0f, 0.0f, DIM_ALPHA };
    forge_ui_ctx_rect(&ctx, dim, dim_color);

    /* ── Centered menu panel ───────────────────────────────────────────── */
    float menu_h = PANEL_TITLE_H + 2.0f * MENU_PAD +
                   (float)MENU_BTN_COUNT * MENU_BTN_H +
                   (float)(MENU_BTN_COUNT - 1) * MENU_SPACING;
    float menu_x = ((float)FB_WIDTH - MENU_WIDTH) * 0.5f;
    float menu_y = ((float)FB_HEIGHT - menu_h) * 0.5f;

    float scroll_y = 0.0f;
    ForgeUiRect menu_rect = { menu_x, menu_y, MENU_WIDTH, menu_h };

    if (forge_ui_ctx_panel_begin(&ctx, "Paused", menu_rect, &scroll_y)) {
        forge_ui_ctx_button_layout(&ctx, "Resume", MENU_BTN_H);
        forge_ui_ctx_button_layout(&ctx, "Settings", MENU_BTN_H);
        forge_ui_ctx_button_layout(&ctx, "Quit", MENU_BTN_H);
        forge_ui_ctx_panel_end(&ctx);
    }

    forge_ui_ctx_end(&ctx);

    bool ok = rasterize_to_bmp(&ctx, &atlas, FB_WIDTH, FB_HEIGHT,
                               "pause_menu.bmp");
    forge_ui_ctx_free(&ctx);
    forge_ui_atlas_free(&atlas);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main — Load font, generate all four demo images, and report results.
 * ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    init_codepoints();

    /* Load font using a relative path — run from the repo root */
    ForgeUiFont font;
    if (!forge_ui_ttf_load(FONT_PATH, &font)) {
        SDL_Log("Failed to load font: %s", FONT_PATH);
        SDL_Quit();
        return 1;
    }

    SDL_Log("=== UI Lesson 14 -- Game UI ===");
    SDL_Log("");

    int pass = 0;
    int fail = 0;

    /* Generate all four demo images */
    if (render_status_bars(&font)) {
        SDL_Log("[OK] status_bars.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] status_bars.bmp");
        fail++;
    }

    if (render_inventory(&font)) {
        SDL_Log("[OK] inventory.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] inventory.bmp");
        fail++;
    }

    if (render_hud(&font)) {
        SDL_Log("[OK] hud.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] hud.bmp");
        fail++;
    }

    if (render_pause_menu(&font)) {
        SDL_Log("[OK] pause_menu.bmp");
        pass++;
    } else {
        SDL_Log("[FAIL] pause_menu.bmp");
        fail++;
    }

    SDL_Log("");
    SDL_Log("Results: %d passed, %d failed", pass, fail);

    forge_ui_ttf_free(&font);
    SDL_Quit();
    return fail > 0 ? 1 : 0;
}
