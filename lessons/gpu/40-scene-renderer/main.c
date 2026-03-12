/*
 * GPU Lesson 40 — Scene Renderer Library
 *
 * Demonstrates forge_scene.h, a reusable header-only library that packages
 * the entire rendering stack from lessons 01–39 into a single init call.
 * One forge_scene_init() replaces ~500 lines of per-lesson boilerplate:
 * SDL init, window, GPU device, depth/shadow textures, pipelines, camera,
 * grid floor, sky gradient, and UI.
 *
 * This lesson renders three lit spheres and a torus on a grid floor with
 * directional shadows, a sky gradient, and an info panel — all in under
 * 200 lines.
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Procedural geometry — sphere, torus */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer library — the star of this lesson */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ─────────────────────────────────────────────────────────── */

#define SPHERE_SLICES   48
#define SPHERE_STACKS   24
#define TORUS_SLICES    32
#define TORUS_STACKS    16
#define TORUS_RING_R    1.0f
#define TORUS_TUBE_R    0.35f

/* Sphere placement (three spheres in a row at y=1) */
#define SPHERE_SPACING  2.0f
#define SPHERE_HEIGHT   1.0f

/* Torus animation */
#define TORUS_HEIGHT    2.5f
#define TORUS_ROT_SPEED 0.5f

/* Material colors (RGBA) */
static const float COLOR_RED[4]   = { 0.9f, 0.2f, 0.2f, 1.0f };
static const float COLOR_GREEN[4] = { 0.2f, 0.8f, 0.3f, 1.0f };
static const float COLOR_BLUE[4]  = { 0.2f, 0.3f, 0.9f, 1.0f };
static const float COLOR_GOLD[4]  = { 0.9f, 0.7f, 0.2f, 1.0f };

/* UI panel position and size */
#define PANEL_X  10
#define PANEL_Y  10
#define PANEL_W  300
#define PANEL_H  130

/* Camera starting view */
#define CAM_START_Y      2.0f
#define CAM_START_Z      5.0f
#define CAM_START_PITCH  -0.3f

/* UI label height */
#define LABEL_HEIGHT     20

/* Unit conversions */
#define MS_PER_SEC       1000.0f

/* ── Application state ─────────────────────────────────────────────────── */

typedef struct {
    ForgeScene scene;              /* rendering stack (device, pipelines, camera, UI) */

    /* Sphere mesh (shared by all three spheres) */
    SDL_GPUBuffer *sphere_vb;      /* interleaved position+normal vertex buffer       */
    SDL_GPUBuffer *sphere_ib;      /* uint32 index buffer                             */
    Uint32         sphere_index_count; /* number of indices to draw                   */

    /* Torus mesh */
    SDL_GPUBuffer *torus_vb;       /* interleaved position+normal vertex buffer       */
    SDL_GPUBuffer *torus_ib;       /* uint32 index buffer                             */
    Uint32         torus_index_count;  /* number of indices to draw                   */
} app_state;

/* ── Helper: interleave ForgeShape into ForgeSceneVertex buffer ──────── */

static SDL_GPUBuffer *upload_shape_vb(ForgeScene *scene,
                                       const ForgeShape *shape)
{
    int count = shape->vertex_count;
    Uint32 size = (Uint32)count * (Uint32)sizeof(ForgeSceneVertex);
    ForgeSceneVertex *verts = (ForgeSceneVertex *)SDL_malloc(size);
    if (!verts) return NULL;

    for (int i = 0; i < count; i++) {
        verts[i].position = shape->positions[i];
        verts[i].normal   = shape->normals[i];
    }

    SDL_GPUBuffer *buf = forge_scene_upload_buffer(
        scene, SDL_GPU_BUFFERUSAGE_VERTEX, verts, size);
    SDL_free(verts);
    return buf;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  SDL callbacks
 * ══════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    /* Configure the scene — override a few defaults */
    ForgeSceneConfig cfg = forge_scene_default_config("Lesson 40 — Scene Renderer");
    cfg.cam_start_pos = vec3_create(0.0f, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";

    /* One call replaces ~500 lines of boilerplate */
    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* ── Generate and upload geometry ──────────────────────────────── */

    ForgeShape sphere = forge_shapes_sphere(SPHERE_SLICES, SPHERE_STACKS);
    state->sphere_vb = upload_shape_vb(&state->scene, &sphere);
    if (!state->sphere_vb) {
        SDL_Log("Failed to upload sphere vertex buffer");
        forge_shapes_free(&sphere);
        return SDL_APP_FAILURE;
    }
    state->sphere_ib = forge_scene_upload_buffer(
        &state->scene, SDL_GPU_BUFFERUSAGE_INDEX,
        sphere.indices, (Uint32)sphere.index_count * sizeof(uint32_t));
    if (!state->sphere_ib) {
        SDL_Log("Failed to upload sphere index buffer");
        forge_shapes_free(&sphere);
        return SDL_APP_FAILURE;
    }
    state->sphere_index_count = (Uint32)sphere.index_count;
    forge_shapes_free(&sphere);

    ForgeShape torus = forge_shapes_torus(
        TORUS_SLICES, TORUS_STACKS, TORUS_RING_R, TORUS_TUBE_R);
    state->torus_vb = upload_shape_vb(&state->scene, &torus);
    if (!state->torus_vb) {
        SDL_Log("Failed to upload torus vertex buffer");
        forge_shapes_free(&torus);
        return SDL_APP_FAILURE;
    }
    state->torus_ib = forge_scene_upload_buffer(
        &state->scene, SDL_GPU_BUFFERUSAGE_INDEX,
        torus.indices, (Uint32)torus.index_count * sizeof(uint32_t));
    if (!state->torus_ib) {
        SDL_Log("Failed to upload torus index buffer");
        forge_shapes_free(&torus);
        return SDL_APP_FAILURE;
    }
    state->torus_index_count = (Uint32)torus.index_count;
    forge_shapes_free(&torus);

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

    /* Compute transforms once so shadow and main passes use identical data */
    mat4 m0 = mat4_translate(vec3_create(-SPHERE_SPACING, SPHERE_HEIGHT, 0.0f));
    mat4 m1 = mat4_translate(vec3_create( 0.0f,           SPHERE_HEIGHT, 0.0f));
    mat4 m2 = mat4_translate(vec3_create( SPHERE_SPACING,  SPHERE_HEIGHT, 0.0f));

    float time = (float)SDL_GetTicks() / MS_PER_SEC;
    mat4 torus_model = mat4_multiply(
        mat4_translate(vec3_create(0.0f, TORUS_HEIGHT, 0.0f)),
        mat4_rotate_y(time * TORUS_ROT_SPEED));

    /* ── Shadow pass ──────────────────────────────────────────────── */

    forge_scene_begin_shadow_pass(s);
    {
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count, m0);
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count, m1);
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count, m2);
        forge_scene_draw_shadow_mesh(s, state->torus_vb, state->torus_ib,
                                     state->torus_index_count, torus_model);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Main pass (sky, meshes, grid) ────────────────────────────── */

    forge_scene_begin_main_pass(s);
    {
        forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                              state->sphere_index_count, m0, COLOR_RED);
        forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                              state->sphere_index_count, m1, COLOR_GREEN);
        forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                              state->sphere_index_count, m2, COLOR_BLUE);
        forge_scene_draw_mesh(s, state->torus_vb, state->torus_ib,
                              state->torus_index_count, torus_model, COLOR_GOLD);

        forge_scene_draw_grid(s);
    }
    forge_scene_end_main_pass(s);

    /* ── UI pass ──────────────────────────────────────────────────── */

    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiContext *ui = forge_scene_ui(s);
        if (ui) {
            float scroll_y = 0.0f;
            ForgeUiRect panel = { PANEL_X, PANEL_Y, PANEL_W, PANEL_H };
            forge_ui_ctx_panel_begin(ui, "Scene Info", panel, &scroll_y);

            forge_ui_ctx_label_layout(ui, "forge_scene.h demo", LABEL_HEIGHT);

            char buf[64];
            SDL_snprintf(buf, sizeof(buf), "dt: %.1f ms",
                         (double)(forge_scene_dt(s) * MS_PER_SEC));
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            forge_ui_ctx_panel_end(ui);
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
        if (state->sphere_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_vb);
        if (state->sphere_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_ib);
        if (state->torus_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->torus_vb);
        if (state->torus_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->torus_ib);
    }

    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
