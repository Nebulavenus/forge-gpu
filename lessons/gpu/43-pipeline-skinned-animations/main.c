/*
 * GPU Lesson 43 — Pipeline Skinned Animations
 *
 * Demonstrates skeletal animation and transform animation using pre-processed
 * pipeline assets.  Three models show different animation patterns:
 *
 *   - CesiumMan  — walk cycle with 19-joint skeleton (.fmesh v3 + .fskin)
 *   - BrainStem  — articulated robot with 18-joint skeleton
 *   - AnimatedCube — rotation animation without skinning (transform only)
 *
 * Skinned models use 72-byte vertices with joint indices and blend weights.
 * Joint matrices are computed on the CPU each frame and uploaded to a GPU
 * storage buffer that the vertex shader reads for skinning.
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

/* Pipeline runtime — .fmesh, .fscene, .fmat, .fskin, .fanim loaders */
#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"

/* Scene renderer with model support (includes skinned model support) */
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
#define PANEL_W      340
#define PANEL_H      460
#define LABEL_HEIGHT 20
#define SLIDER_HEIGHT 24
#define SLIDER_W     200

/* Camera starting position */
#define CAM_START_X     0.0f
#define CAM_START_Y     3.0f
#define CAM_START_Z     8.0f
#define CAM_START_YAW   0.0f
#define CAM_START_PITCH (-0.2f)

/* Model placement — spread models along X axis */
#define CESIUMMAN_X    (-3.0f)
#define BRAINSTEM_X      0.0f
#define ANIMATED_CUBE_X  3.5f
#define MODEL_Y          0.0f
#define MODEL_Z          0.0f

/* CesiumMan is small, scale it up */
#define CESIUMMAN_SCALE  2.0f
/* BrainStem's native mesh is ~0.25 units tall — scale up to match scene */
#define BRAINSTEM_SCALE  3.0f
/* AnimatedCube is unit-sized */
#define CUBE_SCALE       1.0f
/* Lift AnimatedCube above the grid so rotation is visible */
#define CUBE_Y_OFFSET    1.5f

/* Default animation speed */
#define DEFAULT_ANIM_SPEED 1.0f

/* Maximum path buffer for exe-relative asset paths */
#define ASSET_PATH_SIZE 512

/* Speed slider range */
#define SPEED_MIN 0.0f
#define SPEED_MAX 3.0f

/* ── Application state ─────────────────────────────────────────────────── */

typedef struct {
    ForgeScene              scene;           /* rendering baseline (shadows, lighting, grid, UI) */

    /* Skinned models — use ForgeSceneSkinnedModel for joint-driven animation */
    ForgeSceneSkinnedModel  cesium_man;      /* 19-joint walk cycle */
    ForgeSceneSkinnedModel  brain_stem;      /* 18-joint articulated robot */

    /* Non-skinned model with transform animation */
    ForgeSceneModel         animated_cube;   /* standard 48-byte vertex model */
    ForgePipelineAnimFile   cube_anim;       /* animation data loaded separately from model */
    float                   cube_anim_time;  /* elapsed animation time (seconds) */
    float                   cube_anim_speed; /* playback rate multiplier (1.0 = normal) */

    /* UI slider state — copied to model anim_speed each frame */
    float                   cesium_speed;    /* CesiumMan playback rate (0..SPEED_MAX) */
    float                   brain_speed;     /* BrainStem playback rate (0..SPEED_MAX) */
    float                   cube_speed_ui;   /* AnimatedCube playback rate (0..SPEED_MAX) */
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
    state->cesium_speed  = DEFAULT_ANIM_SPEED;
    state->brain_speed   = DEFAULT_ANIM_SPEED;
    state->cube_speed_ui = DEFAULT_ANIM_SPEED;
    state->cube_anim_speed = DEFAULT_ANIM_SPEED;

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
        "GPU Lesson 43 \xe2\x80\x94 Pipeline Skinned Animations");
    config.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    config.cam_start_yaw   = CAM_START_YAW;
    config.cam_start_pitch = CAM_START_PITCH;
    config.font_path       = font;
    if (!forge_scene_init(&state->scene, &config, argc, argv)) {
        SDL_Log("forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* ── Load CesiumMan (skinned, 19 joints) ──────────────────────────── */
    {
        char fscene[ASSET_PATH_SIZE], fmesh[ASSET_PATH_SIZE];
        char fmat[ASSET_PATH_SIZE], fskin[ASSET_PATH_SIZE];
        char fanim[ASSET_PATH_SIZE], base_dir[ASSET_PATH_SIZE];
        if (!asset_path(fscene,   sizeof(fscene),   exe_dir, "assets/CesiumMan/CesiumMan.fscene") ||
            !asset_path(fmesh,    sizeof(fmesh),    exe_dir, "assets/CesiumMan/CesiumMan.fmesh")  ||
            !asset_path(fmat,     sizeof(fmat),     exe_dir, "assets/CesiumMan/CesiumMan.fmat")   ||
            !asset_path(fskin,    sizeof(fskin),    exe_dir, "assets/CesiumMan/CesiumMan.fskin")  ||
            !asset_path(fanim,    sizeof(fanim),    exe_dir, "assets/CesiumMan/CesiumMan.fanim")  ||
            !asset_path(base_dir, sizeof(base_dir), exe_dir, "assets/CesiumMan")) {
            return SDL_APP_FAILURE;
        }
        if (!forge_scene_load_skinned_model(&state->scene, &state->cesium_man,
                                             fscene, fmesh, fmat, fskin, fanim,
                                             base_dir)) {
            SDL_Log("Failed to load CesiumMan");
            return SDL_APP_FAILURE;
        }
    }

    /* ── Load BrainStem (skinned, 18 joints) ──────────────────────────── */
    {
        char fscene[ASSET_PATH_SIZE], fmesh[ASSET_PATH_SIZE];
        char fmat[ASSET_PATH_SIZE], fskin[ASSET_PATH_SIZE];
        char fanim[ASSET_PATH_SIZE], base_dir[ASSET_PATH_SIZE];
        if (!asset_path(fscene,   sizeof(fscene),   exe_dir, "assets/BrainStem/BrainStem.fscene") ||
            !asset_path(fmesh,    sizeof(fmesh),    exe_dir, "assets/BrainStem/BrainStem.fmesh")  ||
            !asset_path(fmat,     sizeof(fmat),     exe_dir, "assets/BrainStem/BrainStem.fmat")   ||
            !asset_path(fskin,    sizeof(fskin),    exe_dir, "assets/BrainStem/BrainStem.fskin")  ||
            !asset_path(fanim,    sizeof(fanim),    exe_dir, "assets/BrainStem/BrainStem.fanim")  ||
            !asset_path(base_dir, sizeof(base_dir), exe_dir, "assets/BrainStem")) {
            return SDL_APP_FAILURE;
        }
        if (!forge_scene_load_skinned_model(&state->scene, &state->brain_stem,
                                             fscene, fmesh, fmat, fskin, fanim,
                                             base_dir)) {
            SDL_Log("Failed to load BrainStem");
            return SDL_APP_FAILURE;
        }
    }

    /* ── Load AnimatedCube (non-skinned, transform animation) ─────────── */
    {
        char fscene[ASSET_PATH_SIZE], fmesh[ASSET_PATH_SIZE];
        char fmat[ASSET_PATH_SIZE], fanim[ASSET_PATH_SIZE];
        char base_dir[ASSET_PATH_SIZE];
        if (!asset_path(fscene,   sizeof(fscene),   exe_dir, "assets/AnimatedCube/AnimatedCube.fscene") ||
            !asset_path(fmesh,    sizeof(fmesh),    exe_dir, "assets/AnimatedCube/AnimatedCube.fmesh")  ||
            !asset_path(fmat,     sizeof(fmat),     exe_dir, "assets/AnimatedCube/AnimatedCube.fmat")   ||
            !asset_path(fanim,    sizeof(fanim),    exe_dir, "assets/AnimatedCube/AnimatedCube.fanim")  ||
            !asset_path(base_dir, sizeof(base_dir), exe_dir, "assets/AnimatedCube")) {
            return SDL_APP_FAILURE;
        }
        if (!forge_scene_load_model(&state->scene, &state->animated_cube,
                                     fscene, fmesh, fmat, base_dir)) {
            SDL_Log("Failed to load AnimatedCube");
            return SDL_APP_FAILURE;
        }
        /* Load animation data separately for transform-only animation */
        if (!forge_pipeline_load_animation(fanim, &state->cube_anim)) {
            SDL_Log("Failed to load AnimatedCube animation: %s", fanim);
            return SDL_APP_FAILURE;
        }
    }

    SDL_Log("Lesson 43: loaded 3 models (CesiumMan %u joints, BrainStem %u joints, AnimatedCube transform-only)",
            state->cesium_man.active_joint_count,
            state->brain_stem.active_joint_count);

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

    /* ── Update animations ───────────────────────────────────────────── */

    /* Skinned models — update animation and joint matrices */
    state->cesium_man.anim_speed = state->cesium_speed;
    forge_scene_update_skinned_animation(s, &state->cesium_man, dt);

    state->brain_stem.anim_speed = state->brain_speed;
    forge_scene_update_skinned_animation(s, &state->brain_stem, dt);

    /* AnimatedCube — apply transform animation manually */
    if (state->cube_anim.clip_count > 0) {
        state->cube_anim_time += dt * state->cube_anim_speed;
        /* Wrap to prevent float precision loss over long sessions */
        float cube_dur = state->cube_anim.clips[0].duration;
        if (cube_dur > FORGE_PIPELINE_ANIM_EPSILON) {
            state->cube_anim_time =
                SDL_fmodf(state->cube_anim_time, cube_dur);
            if (state->cube_anim_time < 0.0f)
                state->cube_anim_time += cube_dur;
        }
        forge_pipeline_anim_apply(
            &state->cube_anim.clips[0],
            state->animated_cube.scene_data.nodes,
            state->animated_cube.scene_data.node_count,
            state->cube_anim_time, true);
        forge_pipeline_scene_compute_world_transforms(
            state->animated_cube.scene_data.nodes,
            state->animated_cube.scene_data.node_count,
            state->animated_cube.scene_data.roots,
            state->animated_cube.scene_data.root_count,
            state->animated_cube.scene_data.children,
            state->animated_cube.scene_data.child_count);
    }

    /* Placement matrices */
    mat4 cesium_place = mat4_multiply(
        mat4_translate(vec3_create(CESIUMMAN_X, MODEL_Y, MODEL_Z)),
        mat4_scale_uniform(CESIUMMAN_SCALE));
    mat4 brain_place = mat4_multiply(
        mat4_translate(vec3_create(BRAINSTEM_X, MODEL_Y, MODEL_Z)),
        mat4_scale_uniform(BRAINSTEM_SCALE));
    mat4 cube_place = mat4_multiply(
        mat4_translate(vec3_create(ANIMATED_CUBE_X, CUBE_Y_OFFSET, MODEL_Z)),
        mat4_scale_uniform(CUBE_SCALE));

    /* ── Shadow pass ──────────────────────────────────────────────────── */
    forge_scene_begin_shadow_pass(s);
    forge_scene_draw_skinned_model_shadows(s, &state->cesium_man, cesium_place);
    forge_scene_draw_skinned_model_shadows(s, &state->brain_stem, brain_place);
    forge_scene_draw_model_shadows(s, &state->animated_cube, cube_place);
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ────────────────────────────────────────────────────── */
    forge_scene_begin_main_pass(s);
    forge_scene_draw_grid(s);
    forge_scene_draw_skinned_model(s, &state->cesium_man, cesium_place);
    forge_scene_draw_skinned_model(s, &state->brain_stem, brain_place);
    forge_scene_draw_model(s, &state->animated_cube, cube_place);
    forge_scene_end_main_pass(s);

    /* ── UI pass ──────────────────────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiContext *ui = forge_scene_ui(s);
        if (ui) {
            char buf[128];
            float scroll_y = 0.0f;
            ForgeUiRect panel = { PANEL_X, PANEL_Y, PANEL_W, PANEL_H };
            forge_ui_ctx_panel_begin(ui, "Skinned Animations", panel, &scroll_y);

            /* CesiumMan section */
            forge_ui_ctx_label_layout(ui, "CesiumMan (19 joints)", LABEL_HEIGHT);
            SDL_snprintf(buf, sizeof(buf), "  Time: %.2f s", state->cesium_man.anim_time);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
            SDL_snprintf(buf, sizeof(buf), "  Draws: %u", state->cesium_man.draw_calls);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
            forge_ui_ctx_slider_layout(ui, "cm_speed",
                                       &state->cesium_speed,
                                       SPEED_MIN, SPEED_MAX, SLIDER_HEIGHT);
            SDL_snprintf(buf, sizeof(buf), "  Speed: %.1fx", state->cesium_speed);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            forge_ui_ctx_label_layout(ui, "---", LABEL_HEIGHT);

            /* BrainStem section */
            forge_ui_ctx_label_layout(ui, "BrainStem (18 joints)", LABEL_HEIGHT);
            SDL_snprintf(buf, sizeof(buf), "  Time: %.2f s", state->brain_stem.anim_time);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
            SDL_snprintf(buf, sizeof(buf), "  Draws: %u", state->brain_stem.draw_calls);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
            forge_ui_ctx_slider_layout(ui, "bs_speed",
                                       &state->brain_speed,
                                       SPEED_MIN, SPEED_MAX, SLIDER_HEIGHT);
            SDL_snprintf(buf, sizeof(buf), "  Speed: %.1fx", state->brain_speed);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            forge_ui_ctx_label_layout(ui, "---", LABEL_HEIGHT);

            /* AnimatedCube section */
            forge_ui_ctx_label_layout(ui, "AnimatedCube (transform)", LABEL_HEIGHT);
            SDL_snprintf(buf, sizeof(buf), "  Time: %.2f s", state->cube_anim_time);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
            forge_ui_ctx_slider_layout(ui, "cube_speed",
                                       &state->cube_speed_ui,
                                       SPEED_MIN, SPEED_MAX, SLIDER_HEIGHT);
            state->cube_anim_speed = state->cube_speed_ui;
            SDL_snprintf(buf, sizeof(buf), "  Speed: %.1fx", state->cube_speed_ui);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            forge_ui_ctx_panel_end(ui);
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

    forge_scene_free_skinned_model(&state->scene, &state->cesium_man);
    forge_scene_free_skinned_model(&state->scene, &state->brain_stem);
    forge_scene_free_model(&state->scene, &state->animated_cube);
    forge_pipeline_free_animation(&state->cube_anim);
    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
