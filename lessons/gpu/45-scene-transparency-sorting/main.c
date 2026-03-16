/*
 * GPU Lesson 45 — Scene Transparency Sorting
 *
 * Demonstrates correct transparent rendering with forge_scene.h:
 *   - Two-pass draw: opaque + MASK submeshes first, then BLEND sorted
 *     back-to-front by precomputed submesh centroid distance
 *   - Alpha-masked shadow casting for MASK materials
 *   - Toggle to compare sorted vs unsorted BLEND rendering
 *
 * The CesiumMilkTruck model has its glass material set to BLEND mode
 * (alpha 0.3) to exercise the transparency sorting path.  Two trucks
 * are placed so their windshields overlap — this makes sorting artifacts
 * visible when sorting is disabled.
 *
 * Controls:
 *   WASD / Mouse  — move/look
 *   Space / Shift — fly up/down
 *   Escape        — release mouse cursor
 *   T             — toggle transparency sorting on/off
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>

/* Math library */
#include "math/forge_math.h"

/* Pipeline runtime — .fmesh, .fscene, .fmat loaders */
#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"

/* Scene renderer with model support */
#define FORGE_SCENE_MODEL_SUPPORT
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ─────────────────────────────────────────────────────────── */

/* Two trucks placed so windshields overlap when viewed from the front */
#define TRUCK_A_X    -1.5f
#define TRUCK_B_X     1.5f
#define MODEL_Y       0.0f

/* UI layout */
#define PANEL_X       10
#define PANEL_Y       10
#define PANEL_W       240
#define PANEL_H       160
#define LABEL_HEIGHT  20

/* Maximum path buffer for asset paths */
#define ASSET_PATH_SIZE 512

/* ── Application state ─────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;

    /* One model loaded, drawn at two placements */
    ForgeSceneModel truck;

    /* UI state */
    ForgeUiWindowState ui_window;
} app_state;

/* ── Helpers ───────────────────────────────────────────────────────────── */

/* Build an absolute asset path: exe_dir + relative. */
static const char *asset_path(char *buf, size_t buf_size,
                              const char *base, const char *relative)
{
    int len = SDL_snprintf(buf, buf_size, "%s%s", base, relative);
    if (len < 0 || (size_t)len >= buf_size) {
        SDL_Log("asset_path: path too long: %s%s", base, relative);
        return NULL;
    }
    return buf;
}

/* ── SDL_AppInit ───────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    const char *base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Font path for the UI */
    char font_buf[ASSET_PATH_SIZE];
    const char *font = asset_path(font_buf, sizeof(font_buf), base,
                        "assets/fonts/liberation_mono/LiberationMono-Regular.ttf");
    if (!font) return SDL_APP_FAILURE;

    ForgeSceneConfig cfg = forge_scene_default_config(
        "Lesson 45 \xe2\x80\x94 Scene Transparency Sorting");
    cfg.font_path = font;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* Load CesiumMilkTruck — glass material set to BLEND in the .fmat */
    {
        char p1[ASSET_PATH_SIZE], p2[ASSET_PATH_SIZE];
        char p3[ASSET_PATH_SIZE], p4[ASSET_PATH_SIZE];
        if (!asset_path(p1, sizeof(p1), base, "assets/CesiumMilkTruck/CesiumMilkTruck.fscene") ||
            !asset_path(p2, sizeof(p2), base, "assets/CesiumMilkTruck/CesiumMilkTruck.fmesh") ||
            !asset_path(p3, sizeof(p3), base, "assets/CesiumMilkTruck/CesiumMilkTruck.fmat") ||
            !asset_path(p4, sizeof(p4), base, "assets/CesiumMilkTruck")) {
            SDL_Log("ERROR: asset path too long");
            return SDL_APP_FAILURE;
        }
        if (!forge_scene_load_model(&state->scene, &state->truck,
                                     p1, p2, p3, p4)) {
            SDL_Log("ERROR: failed to load CesiumMilkTruck");
            return SDL_APP_FAILURE;
        }
    }

    /* UI window state */
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ──────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;
    SDL_AppResult r = forge_scene_handle_event(&state->scene, event);
    if (r != SDL_APP_CONTINUE) return r;

    /* Toggle transparency sorting with T key */
    if (event->type == SDL_EVENT_KEY_DOWN &&
        !event->key.repeat &&
        event->key.scancode == SDL_SCANCODE_T) {
        state->scene.transparency_sorting =
            !state->scene.transparency_sorting;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;

    /* ── Placement matrices ──────────────────────────────────────── */
    mat4 truck_a = mat4_translate(vec3_create(TRUCK_A_X, MODEL_Y, 0.0f));
    mat4 truck_b = mat4_translate(vec3_create(TRUCK_B_X, MODEL_Y, 0.0f));

    /* ── Shadow pass ─────────────────────────────────────────────── */
    forge_scene_begin_shadow_pass(s);
    forge_scene_draw_model_shadows(s, &state->truck, truck_a);
    forge_scene_draw_model_shadows(s, &state->truck, truck_b);
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */
    forge_scene_begin_main_pass(s);
    forge_scene_draw_model(s, &state->truck, truck_a);
    forge_scene_draw_model(s, &state->truck, truck_b);
    forge_scene_draw_grid(s);
    forge_scene_end_main_pass(s);

    /* ── UI overlay ──────────────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "Transparency",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;
                char buf[64];

                SDL_snprintf(buf, sizeof(buf), "Sorting (T): %s",
                             s->transparency_sorting ? "ON" : "OFF");
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                forge_ui_ctx_label_layout(ui, "---", LABEL_HEIGHT);

                /* Stats from the last draw_model call (truck_b) — per-call
                 * counts, not totals, since both calls share one model. */
                SDL_snprintf(buf, sizeof(buf), "Draw calls: %u",
                             state->truck.draw_calls);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf), "Transparent: %u",
                             state->truck.transparent_draw_calls);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf), "dt: %.1f ms  (%.0f FPS)",
                             (double)(forge_scene_dt(s) * 1000.0f),
                             (double)(forge_scene_dt(s) > 0.0f
                                      ? 1.0f / forge_scene_dt(s) : 0.0f));
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                forge_ui_wctx_window_end(wctx);
            }
        }
    }
    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
}

/* ── SDL_AppQuit ───────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    app_state *state = (app_state *)appstate;
    if (!state) return;

    if (forge_scene_device(&state->scene)) {
        if (!SDL_WaitForGPUIdle(forge_scene_device(&state->scene))) {
            SDL_Log("SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }
        forge_scene_free_model(&state->scene, &state->truck);
    }

    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
