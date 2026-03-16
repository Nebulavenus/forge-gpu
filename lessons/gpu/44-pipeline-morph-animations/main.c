/*
 * GPU Lesson 44 — Pipeline Morph Target Animations
 *
 * Demonstrates morph target (blend shape) animation using pre-processed
 * pipeline assets.  Two models show different morph deformations:
 *
 *   - AnimatedMorphCube — 2 morph targets deforming a cube
 *   - SimpleMorph       — 2 morph targets deforming a triangle
 *
 * Morph target deltas are loaded from .fmesh files (FLAG_MORPHS).  Each
 * frame, the CPU evaluates morph weights from .fanim keyframes, blends
 * per-vertex position and normal deltas, and uploads the result to GPU
 * storage buffers.  The vertex shader reads these deltas via SV_VertexID
 * and adds them to the base mesh attributes.
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

/* Pipeline runtime — .fmesh, .fscene, .fmat, .fanim loaders */
#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"

/* Scene renderer with model support (includes morph model support) */
#define FORGE_SCENE_MODEL_SUPPORT
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Constants ─────────────────────────────────────────────────────────── */

/* UI panel layout */
#define PANEL_X      10
#define PANEL_Y      10
#define PANEL_W      360
#define PANEL_H      520
#define LABEL_HEIGHT 20
#define SLIDER_HEIGHT 24
#define SLIDER_W     200

/* Camera starting position — offset right so the UI panel on the left
 * does not occlude either model. */
#define CAM_START_X      3.0f
#define CAM_START_Y      2.5f
#define CAM_START_Z     10.0f
#define CAM_START_YAW    0.0f
#define CAM_START_PITCH  (-0.1f)

/* Model placement — both right of center, away from the UI panel. */
#define MORPH_CUBE_X      1.0f
#define SIMPLE_MORPH_X    5.0f
#define MODEL_Z           0.0f

/* AnimatedMorphCube: base mesh is 0.02 units, but the .fscene node has
 * a 100x scale + Z-up→Y-up rotation baked in (Blender export convention).
 * After the node transform, the cube is ~2 units in Y-up space.
 * Placement scale 1.0 keeps it at that natural size. */
#define MORPH_CUBE_SCALE       1.0f
#define MORPH_CUBE_Y_OFFSET    1.5f

/* SimpleMorph is a triangle in the XY plane, positions [0,1].
 * Scale up and lift above the grid so the morph deformation is visible. */
#define SIMPLE_MORPH_SCALE     2.5f
#define SIMPLE_MORPH_Y_OFFSET  1.5f

/* Default animation speed */
#define DEFAULT_ANIM_SPEED 1.0f

/* Maximum path buffer for exe-relative asset paths */
#define ASSET_PATH_SIZE 512

/* Speed slider range */
#define SPEED_MIN 0.0f
#define SPEED_MAX 3.0f

/* Weight slider range */
#define WEIGHT_MIN 0.0f
#define WEIGHT_MAX 1.0f

/* ── Application state ─────────────────────────────────────────────────── */

typedef struct {
    ForgeScene             scene;          /* rendering baseline */

    /* Morph models */
    ForgeSceneMorphModel   morph_cube;     /* AnimatedMorphCube: 2 targets */
    ForgeSceneMorphModel   simple_morph;   /* SimpleMorph: 2 targets */

    /* UI slider state */
    float                  cube_speed;     /* animation speed */
    float                  simple_speed;   /* animation speed */

    /* Manual weight overrides (used when manual_weights is toggled) */
    float                  cube_w0;
    float                  cube_w1;
    float                  simple_w0;
    float                  simple_w1;

    ForgeUiWindowState     ui_window;      /* draggable window state */
} AppState;

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

/* ── SDL_AppInit ────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    AppState *state = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    /* Default animation speeds */
    state->cube_speed   = DEFAULT_ANIM_SPEED;
    state->simple_speed = DEFAULT_ANIM_SPEED;

    const char *exe_dir = SDL_GetBasePath();
    if (!exe_dir) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Build font path */
    char font_buf[ASSET_PATH_SIZE];
    const char *font = asset_path(font_buf, sizeof(font_buf), exe_dir,
                        "assets/fonts/liberation_mono/LiberationMono-Regular.ttf");
    if (!font) return SDL_APP_FAILURE;

    /* Configure scene */
    ForgeSceneConfig config = forge_scene_default_config(
        "GPU Lesson 44 \xe2\x80\x94 Pipeline Morph Target Animations");
    config.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    config.cam_start_yaw   = CAM_START_YAW;
    config.cam_start_pitch = CAM_START_PITCH;
    config.font_path       = font;
    if (!forge_scene_init(&state->scene, &config, argc, argv)) {
        SDL_Log("forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* ── Load AnimatedMorphCube ────────────────────────────────────── */
    {
        char fscene[ASSET_PATH_SIZE], fmesh[ASSET_PATH_SIZE];
        char fmat[ASSET_PATH_SIZE], fanim[ASSET_PATH_SIZE];
        char base_dir[ASSET_PATH_SIZE];
        if (!asset_path(fscene,   sizeof(fscene),   exe_dir, "assets/AnimatedMorphCube/AnimatedMorphCube.fscene") ||
            !asset_path(fmesh,    sizeof(fmesh),    exe_dir, "assets/AnimatedMorphCube/AnimatedMorphCube.fmesh")  ||
            !asset_path(fmat,     sizeof(fmat),     exe_dir, "assets/AnimatedMorphCube/AnimatedMorphCube.fmat")   ||
            !asset_path(fanim,    sizeof(fanim),    exe_dir, "assets/AnimatedMorphCube/AnimatedMorphCube.fanim")  ||
            !asset_path(base_dir, sizeof(base_dir), exe_dir, "assets/AnimatedMorphCube")) {
            return SDL_APP_FAILURE;
        }
        if (!forge_scene_load_morph_model(&state->scene, &state->morph_cube,
                                           fscene, fmesh, fmat, fanim,
                                           base_dir)) {
            SDL_Log("Failed to load AnimatedMorphCube");
            return SDL_APP_FAILURE;
        }
    }

    /* ── Load SimpleMorph ──────────────────────────────────────────── */
    {
        char fscene[ASSET_PATH_SIZE], fmesh[ASSET_PATH_SIZE];
        char fanim[ASSET_PATH_SIZE], base_dir[ASSET_PATH_SIZE];
        if (!asset_path(fscene,   sizeof(fscene),   exe_dir, "assets/SimpleMorph/SimpleMorph.fscene") ||
            !asset_path(fmesh,    sizeof(fmesh),    exe_dir, "assets/SimpleMorph/SimpleMorph.fmesh")  ||
            !asset_path(fanim,    sizeof(fanim),    exe_dir, "assets/SimpleMorph/SimpleMorph.fanim")  ||
            !asset_path(base_dir, sizeof(base_dir), exe_dir, "assets/SimpleMorph")) {
            return SDL_APP_FAILURE;
        }
        /* SimpleMorph has no .fmat — pass NULL for materials */
        if (!forge_scene_load_morph_model(&state->scene, &state->simple_morph,
                                           fscene, fmesh, NULL, fanim,
                                           base_dir)) {
            SDL_Log("Failed to load SimpleMorph");
            return SDL_APP_FAILURE;
        }
    }

    SDL_Log("Lesson 44: loaded 2 morph models (MorphCube %u targets, SimpleMorph %u targets)",
            state->morph_cube.morph_target_count,
            state->simple_morph.morph_target_count);

    /* ── UI window state ──────────────────────────────────────── */
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ──────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    AppState *state = (AppState *)appstate;
    return forge_scene_handle_event(&state->scene, event);
}

/* ── SDL_AppIterate ─────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = (AppState *)appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;
    float dt = forge_scene_dt(s);

    /* ── Update morph animations ─────────────────────────────────────── */

    /* AnimatedMorphCube */
    state->morph_cube.anim_speed = state->cube_speed;
    if (state->morph_cube.manual_weights) {
        if (state->morph_cube.morph_target_count > 0)
            state->morph_cube.morph_weights[0] = state->cube_w0;
        if (state->morph_cube.morph_target_count > 1)
            state->morph_cube.morph_weights[1] = state->cube_w1;
    }
    forge_scene_update_morph_animation(s, &state->morph_cube, dt);

    /* SimpleMorph */
    state->simple_morph.anim_speed = state->simple_speed;
    if (state->simple_morph.manual_weights) {
        if (state->simple_morph.morph_target_count > 0)
            state->simple_morph.morph_weights[0] = state->simple_w0;
        if (state->simple_morph.morph_target_count > 1)
            state->simple_morph.morph_weights[1] = state->simple_w1;
    }
    forge_scene_update_morph_animation(s, &state->simple_morph, dt);

    /* Placement matrices */
    mat4 cube_place = mat4_multiply(
        mat4_translate(vec3_create(MORPH_CUBE_X, MORPH_CUBE_Y_OFFSET, MODEL_Z)),
        mat4_scale_uniform(MORPH_CUBE_SCALE));
    mat4 simple_place = mat4_multiply(
        mat4_translate(vec3_create(SIMPLE_MORPH_X, SIMPLE_MORPH_Y_OFFSET, MODEL_Z)),
        mat4_scale_uniform(SIMPLE_MORPH_SCALE));

    /* ── Shadow pass ──────────────────────────────────────────────────── */
    forge_scene_begin_shadow_pass(s);
    forge_scene_draw_morph_model_shadows(s, &state->morph_cube, cube_place);
    forge_scene_draw_morph_model_shadows(s, &state->simple_morph, simple_place);
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ────────────────────────────────────────────────────── */
    forge_scene_begin_main_pass(s);
    forge_scene_draw_grid(s);
    forge_scene_draw_morph_model(s, &state->morph_cube, cube_place);
    forge_scene_draw_morph_model(s, &state->simple_morph, simple_place);
    forge_scene_end_main_pass(s);

    /* ── UI pass ──────────────────────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "Morph Animations",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;
                char buf[128];

                /* ── AnimatedMorphCube section ─────────────────────── */
                forge_ui_ctx_label_layout(ui, "AnimatedMorphCube (2 targets)",
                                           LABEL_HEIGHT);
                SDL_snprintf(buf, sizeof(buf), "  Time: %.2f s",
                             state->morph_cube.anim_time);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                /* Animation speed */
                forge_ui_ctx_slider_layout(ui, "mc_speed",
                                           &state->cube_speed,
                                           SPEED_MIN, SPEED_MAX,
                                           SLIDER_HEIGHT);
                SDL_snprintf(buf, sizeof(buf), "  Speed: %.1fx",
                             state->cube_speed);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                /* Manual weights toggle */
                forge_ui_ctx_checkbox_layout(ui, "Manual weights (Cube)",
                    &state->morph_cube.manual_weights, LABEL_HEIGHT);

                /* Per-target weight sliders */
                if (state->morph_cube.manual_weights) {
                    forge_ui_ctx_slider_layout(ui, "mc_w0",
                                               &state->cube_w0,
                                               WEIGHT_MIN, WEIGHT_MAX,
                                               SLIDER_HEIGHT);
                    SDL_snprintf(buf, sizeof(buf), "  Target 0: %.2f",
                                 state->cube_w0);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    forge_ui_ctx_slider_layout(ui, "mc_w1",
                                               &state->cube_w1,
                                               WEIGHT_MIN, WEIGHT_MAX,
                                               SLIDER_HEIGHT);
                    SDL_snprintf(buf, sizeof(buf), "  Target 1: %.2f",
                                 state->cube_w1);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                } else {
                    /* Show animated weights */
                    float cw0 = state->morph_cube.morph_target_count > 0
                                ? state->morph_cube.morph_weights[0] : 0.0f;
                    float cw1 = state->morph_cube.morph_target_count > 1
                                ? state->morph_cube.morph_weights[1] : 0.0f;
                    SDL_snprintf(buf, sizeof(buf), "  W0: %.3f  W1: %.3f",
                                 cw0, cw1);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                forge_ui_ctx_label_layout(ui, "---", LABEL_HEIGHT);

                /* ── SimpleMorph section ───────────────────────────── */
                forge_ui_ctx_label_layout(ui, "SimpleMorph (2 targets)",
                                           LABEL_HEIGHT);
                SDL_snprintf(buf, sizeof(buf), "  Time: %.2f s",
                             state->simple_morph.anim_time);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                /* Animation speed */
                forge_ui_ctx_slider_layout(ui, "sm_speed",
                                           &state->simple_speed,
                                           SPEED_MIN, SPEED_MAX,
                                           SLIDER_HEIGHT);
                SDL_snprintf(buf, sizeof(buf), "  Speed: %.1fx",
                             state->simple_speed);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                /* Manual weights toggle */
                forge_ui_ctx_checkbox_layout(ui, "Manual weights (Simple)",
                    &state->simple_morph.manual_weights, LABEL_HEIGHT);

                /* Per-target weight sliders */
                if (state->simple_morph.manual_weights) {
                    forge_ui_ctx_slider_layout(ui, "sm_w0",
                                               &state->simple_w0,
                                               WEIGHT_MIN, WEIGHT_MAX,
                                               SLIDER_HEIGHT);
                    SDL_snprintf(buf, sizeof(buf), "  Target 0: %.2f",
                                 state->simple_w0);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    forge_ui_ctx_slider_layout(ui, "sm_w1",
                                               &state->simple_w1,
                                               WEIGHT_MIN, WEIGHT_MAX,
                                               SLIDER_HEIGHT);
                    SDL_snprintf(buf, sizeof(buf), "  Target 1: %.2f",
                                 state->simple_w1);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                } else {
                    /* Show animated weights */
                    float sw0 = state->simple_morph.morph_target_count > 0
                                ? state->simple_morph.morph_weights[0] : 0.0f;
                    float sw1 = state->simple_morph.morph_target_count > 1
                                ? state->simple_morph.morph_weights[1] : 0.0f;
                    SDL_snprintf(buf, sizeof(buf), "  W0: %.3f  W1: %.3f",
                                 sw0, sw1);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

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
    AppState *state = (AppState *)appstate;
    if (!state) return;

    forge_scene_free_morph_model(&state->scene, &state->morph_cube);
    forge_scene_free_morph_model(&state->scene, &state->simple_morph);
    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
