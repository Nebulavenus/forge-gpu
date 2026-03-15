/*
 * GPU Lesson 41 — Scene Model Loading
 *
 * Extends forge_scene.h with pipeline model rendering: loads .fscene + .fmesh +
 * .fmat assets, uploads vertex/index buffers, loads per-material textures, and
 * traverses the scene node hierarchy to draw each submesh with its correct
 * world transform and per-primitive material.
 *
 * Three models demonstrate different aspects of the pipeline:
 *   - CesiumMilkTruck — 4 materials, mesh instancing (2 wheel nodes share
 *     the same mesh), 6-node hierarchy
 *   - Suzanne — baseColor + metallicRoughness textures
 *   - Duck — single textured material, 3-node hierarchy
 *
 * Controls:
 *   WASD / Mouse  — move/look
 *   Space / Shift — fly up/down
 *   Escape        — release mouse cursor
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

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Constants ─────────────────────────────────────────────────────────── */

/* Model placement — X positions along a row on the ground plane */
#define TRUCK_X     -3.0f   /* CesiumMilkTruck placed left of center       */
#define SUZANNE_X    0.0f   /* Suzanne at center                            */
#define DUCK_X       3.5f   /* Duck placed right of center                  */
#define MODEL_Y      0.0f   /* all models sit on the ground plane           */

/* Per-model scale adjustments */
#define SUZANNE_SCALE  0.8f    /* Suzanne is close to unit size; slight down-scale */
#define SUZANNE_Y      1.0f    /* elevate Suzanne above ground for better visibility */
#define DUCK_SCALE     1.0f    /* Duck.glTF has 0.01 scale in its scene hierarchy  */

/* UI panel layout */
#define PANEL_X      10
#define PANEL_Y      10
#define PANEL_W      320
#define PANEL_H      230
#define LABEL_HEIGHT 20

/* Unit conversions */
#define MS_PER_SEC   1000.0f

/* Camera starting position and orientation */
#define CAM_START_Y      2.5f   /* height above ground */
#define CAM_START_Z      8.0f   /* distance back from models */
#define CAM_START_PITCH -0.2f   /* slight downward tilt (radians) */

/* ── Application state ─────────────────────────────────────────────────── */

typedef struct {
    ForgeScene         scene;      /* rendering stack (device, pipelines, camera, UI) */
    ForgeSceneModel    truck;      /* CesiumMilkTruck — 4 materials, 6-node hierarchy */
    ForgeSceneModel    suzanne;    /* Suzanne — baseColor + metallicRoughness textures */
    ForgeSceneModel    duck;       /* Duck — single textured material, 3-node hierarchy */
    ForgeUiWindowState ui_window;  /* draggable window state */
} app_state;

/* Maximum path buffer for base_path + relative asset path */
#define ASSET_PATH_SIZE 512

/* ══════════════════════════════════════════════════════════════════════════
 *  SDL callbacks
 * ══════════════════════════════════════════════════════════════════════════ */

/* Build an absolute asset path: exe_dir + relative.
 * `base` must end with a path separator (SDL_GetBasePath guarantees this).
 * Returns a pointer to buf on success, NULL on overflow. */
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

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    /* Resolve asset paths relative to the executable directory so the
     * lesson works regardless of the current working directory. */
    const char *base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Configure the scene — override camera defaults for this lesson */
    char font_buf[ASSET_PATH_SIZE];
    const char *font = asset_path(font_buf, sizeof(font_buf), base,
                        "assets/fonts/liberation_mono/LiberationMono-Regular.ttf");
    if (!font) {
        SDL_Log("Failed to build font path — base path too long");
        return SDL_APP_FAILURE;
    }

    ForgeSceneConfig cfg = forge_scene_default_config("Lesson 41 — Scene Model Loading");
    cfg.cam_start_pos   = vec3_create(0.0f, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = font;

    /* One call initialises SDL, window, GPU device, pipelines, shadow map,
     * grid, sky, camera, and UI font */
    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* ── Load models ────────────────────────────────────────────────── */

    char p1[ASSET_PATH_SIZE], p2[ASSET_PATH_SIZE];
    char p3[ASSET_PATH_SIZE], p4[ASSET_PATH_SIZE];

    /* CesiumMilkTruck: 4 materials, mesh instancing (both wheel nodes share
     * the same mesh), 6-node scene hierarchy */
    if (asset_path(p1, sizeof(p1), base, "assets/CesiumMilkTruck/CesiumMilkTruck.fscene") &&
        asset_path(p2, sizeof(p2), base, "assets/CesiumMilkTruck/CesiumMilkTruck.fmesh") &&
        asset_path(p3, sizeof(p3), base, "assets/CesiumMilkTruck/CesiumMilkTruck.fmat") &&
        asset_path(p4, sizeof(p4), base, "assets/CesiumMilkTruck")) {
        if (!forge_scene_load_model(&state->scene, &state->truck,
                                     p1, p2, p3, p4)) {
            SDL_Log("Warning: failed to load CesiumMilkTruck — continuing without it");
        }
    }

    /* Suzanne: demonstrates baseColor and metallicRoughness texture binding */
    if (asset_path(p1, sizeof(p1), base, "assets/Suzanne/Suzanne.fscene") &&
        asset_path(p2, sizeof(p2), base, "assets/Suzanne/Suzanne.fmesh") &&
        asset_path(p3, sizeof(p3), base, "assets/Suzanne/Suzanne.fmat") &&
        asset_path(p4, sizeof(p4), base, "assets/Suzanne")) {
        if (!forge_scene_load_model(&state->scene, &state->suzanne,
                                     p1, p2, p3, p4)) {
            SDL_Log("Warning: failed to load Suzanne — continuing without it");
        }
    }

    /* Duck: single textured material, 3-node hierarchy */
    if (asset_path(p1, sizeof(p1), base, "assets/Duck/Duck.fscene") &&
        asset_path(p2, sizeof(p2), base, "assets/Duck/Duck.fmesh") &&
        asset_path(p3, sizeof(p3), base, "assets/Duck/Duck.fmat") &&
        asset_path(p4, sizeof(p4), base, "assets/Duck")) {
        if (!forge_scene_load_model(&state->scene, &state->duck,
                                     p1, p2, p3, p4)) {
            SDL_Log("Warning: failed to load Duck — continuing without it");
        }
    }

    /* ── UI window state ──────────────────────────────────────────── */
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;
    return forge_scene_handle_event(&state->scene, event);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    ForgeScene *s = &state->scene;
    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;

    /* Compute placement matrices for each model */
    mat4 truck_placement = mat4_translate(vec3_create(TRUCK_X, MODEL_Y, 0.0f));

    mat4 suzanne_placement = mat4_multiply(
        mat4_translate(vec3_create(SUZANNE_X, MODEL_Y + SUZANNE_Y, 0.0f)),
        mat4_scale_uniform(SUZANNE_SCALE));

    mat4 duck_placement = mat4_multiply(
        mat4_translate(vec3_create(DUCK_X, MODEL_Y, 0.0f)),
        mat4_scale_uniform(DUCK_SCALE));

    /* Shadow pass */
    forge_scene_begin_shadow_pass(s);
    {
        forge_scene_draw_model_shadows(s, &state->truck, truck_placement);
        forge_scene_draw_model_shadows(s, &state->suzanne, suzanne_placement);
        forge_scene_draw_model_shadows(s, &state->duck, duck_placement);
    }
    forge_scene_end_shadow_pass(s);

    /* Main pass */
    forge_scene_begin_main_pass(s);
    {
        forge_scene_draw_model(s, &state->truck, truck_placement);
        forge_scene_draw_model(s, &state->suzanne, suzanne_placement);
        forge_scene_draw_model(s, &state->duck, duck_placement);
        forge_scene_draw_grid(s);
    }
    forge_scene_end_main_pass(s);

    /* UI pass — show model stats panel */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "Model Stats",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;
                char buf[128];

                /* Per-model stats */
                SDL_snprintf(buf, sizeof(buf),
                             "Truck:   %u nodes, %u mats, %u draws",
                             state->truck.scene_data.node_count,
                             state->truck.materials.material_count,
                             state->truck.draw_calls);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf),
                             "Suzanne: %u nodes, %u mats, %u draws",
                             state->suzanne.scene_data.node_count,
                             state->suzanne.materials.material_count,
                             state->suzanne.draw_calls);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf),
                             "Duck:    %u nodes, %u mats, %u draws",
                             state->duck.scene_data.node_count,
                             state->duck.materials.material_count,
                             state->duck.draw_calls);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                /* Totals */
                uint32_t total_draws = state->truck.draw_calls +
                                       state->suzanne.draw_calls +
                                       state->duck.draw_calls;
                SDL_snprintf(buf, sizeof(buf), "Total draw calls: %u",
                             total_draws);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                /* Separator + timing */
                forge_ui_ctx_label_layout(ui, "---", LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf), "dt: %.1f ms  (%.0f FPS)",
                             (double)(forge_scene_dt(s) * MS_PER_SEC),
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
        forge_scene_free_model(&state->scene, &state->suzanne);
        forge_scene_free_model(&state->scene, &state->duck);
    }

    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
