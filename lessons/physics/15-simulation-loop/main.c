/*
 * Physics Lesson 15 — Simulation Loop
 *
 * Demonstrates: Unified ForgePhysicsWorld with one-call step function,
 * island detection via union-find, and body sleeping for performance.
 *
 * Four selectable scenes (same as Lesson 14):
 *   1. Tall Tower (12)  — 12 boxes stacked vertically
 *   2. Pyramid          — 7-layer pyramid (28 boxes)
 *   3. Wall             — 6 columns x 5 rows (30 boxes)
 *   4. Stress Test (20) — 20 boxes in a single tower
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   Tab               — cycle scenes
 *   I                 — toggle island coloring
 *   C                 — toggle contact visualization
 *   F                 — launch ball at scene (wakes sleeping bodies)
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "math/forge_math.h"
#include "physics/forge_physics.h"

#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Physics ──────────────────────────────────────────────────────────────── */
#define MAX_DELTA_TIME        0.1f

/* ── Scene limits ─────────────────────────────────────────────────────────── */
#define MAX_BODIES            50
#define MAX_MANIFOLDS         256
#define MAX_CONTACT_MARKERS   (MAX_MANIFOLDS * FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS)

/* ── Box parameters ───────────────────────────────────────────────────────── */
#define BOX_HALF              0.5f
#define BOX_MASS              2.0f
#define STACK_OFFSET          0.001f

/* ── Ball launcher ────────────────────────────────────────────────────────── */
#define BALL_RADIUS           0.3f
#define BALL_MASS             5.0f
#define BALL_SPEED            15.0f
#define BALL_SPAWN_OFFSET     1.5f

/* Mesh resolution */
#define SPHERE_SLICES         16
#define SPHERE_STACKS          8
#define CUBE_SUBDIVS           1

/* ── Common physics parameters ────────────────────────────────────────────── */
#define GROUND_Y              0.0f
#define DEFAULT_DAMPING       0.99f
#define DEFAULT_ANG_DAMPING   0.98f
#define DEFAULT_RESTIT        0.2f
#define DEFAULT_SOLVER_ITERS  40

/* Scene-specific solver iterations */
#define TOWER_SOLVER_ITERS    40
#define PYRAMID_SOLVER_ITERS  20
#define WALL_SOLVER_ITERS     20
#define STRESS_SOLVER_ITERS   60

/* Scene body counts */
#define TOWER_BOX_COUNT       12
#define PYRAMID_ROW_COUNT      7
#define WALL_COLS              6
#define WALL_ROWS              5
#define STRESS_BOX_COUNT      20

/* ── Contact visualization ────────────────────────────────────────────────── */
#define CONTACT_MARKER_RADIUS 0.04f
#define NORMAL_LINE_SCALE     10.0f
#define NORMAL_LINE_MIN       0.05f
#define DEPTH_COLOR_SCALE     10.0f

/* ── UI layout ────────────────────────────────────────────────────────────── */
#define PANEL_X              10.0f
#define PANEL_Y              10.0f
#define PANEL_W             450.0f
#define PANEL_H             700.0f
#define LABEL_HEIGHT          18.0f
#define LABEL_SPACER          4.0f

/* ── Number of scenes ─────────────────────────────────────────────────────── */
#define NUM_SCENES            4

/* ── Sleep dimming factor ─────────────────────────────────────────────────── */
#define SLEEP_DIM_FACTOR      0.35f

/* ── Scene names ──────────────────────────────────────────────────────────── */
static const char *SCENE_NAMES[] = {
    "Tall Tower (12)",
    "Pyramid (28)",
    "Wall (30)",
    "Stress Test (20)"
};

/* ── Body colors (8 colors, same as L14) ─────────────────────────────────── */
static const float BODY_COLORS[][4] = {
    { 0.85f, 0.20f, 0.20f, 1.0f },
    { 0.20f, 0.65f, 0.30f, 1.0f },
    { 0.25f, 0.45f, 0.85f, 1.0f },
    { 0.90f, 0.75f, 0.15f, 1.0f },
    { 0.70f, 0.30f, 0.70f, 1.0f },
    { 0.20f, 0.75f, 0.75f, 1.0f },
    { 0.90f, 0.55f, 0.20f, 1.0f },
    { 0.55f, 0.55f, 0.55f, 1.0f },
};
#define NUM_BODY_COLORS 8

/* ── Island colors (8 distinct, more saturated for visibility) ────────────── */
static const float ISLAND_COLORS[][4] = {
    { 1.00f, 0.30f, 0.30f, 1.0f },
    { 0.30f, 1.00f, 0.40f, 1.0f },
    { 0.30f, 0.50f, 1.00f, 1.0f },
    { 1.00f, 0.85f, 0.20f, 1.0f },
    { 0.80f, 0.35f, 0.80f, 1.0f },
    { 0.25f, 0.85f, 0.85f, 1.0f },
    { 1.00f, 0.60f, 0.25f, 1.0f },
    { 0.65f, 0.65f, 0.65f, 1.0f },
};
#define NUM_ISLAND_COLORS 8

/* ── Application state ────────────────────────────────────────────────────── */
typedef struct app_state {
    ForgeScene scene;

    /* GPU resources */
    SDL_GPUBuffer *sphere_vb, *sphere_ib;
    SDL_GPUBuffer *cube_vb, *cube_ib;
    int sphere_index_count;
    int cube_index_count;

    /* Physics world (unified API) */
    ForgePhysicsWorld world;
    float body_colors[MAX_BODIES][4];  /* per-body RGBA (not part of world) */
    int num_bodies;                     /* for color lookup (matches world body count) */

    /* Timing / simulation control */
    float accumulator;
    bool  paused;
    float speed_scale;

    /* Scene management */
    int   scene_index;

    /* Display toggles */
    bool  show_contacts;
    bool  show_islands;

    /* UI window state */
    ForgeUiWindowState ui_window;
} app_state;

/* ── Helper: upload_shape_vb ──────────────────────────────────────────────── */
/*
 * Converts a ForgeShape's vertex data into ForgeSceneVertex format and uploads
 * it to a GPU vertex buffer via the scene renderer helper.
 */
static SDL_GPUBuffer *upload_shape_vb(ForgeScene *scene, const ForgeShape *shape)
{
    ForgeSceneVertex *verts = SDL_calloc((size_t)shape->vertex_count, sizeof(ForgeSceneVertex));
    if (!verts) return NULL;
    for (int i = 0; i < shape->vertex_count; i++) {
        verts[i].position = shape->positions[i];
        verts[i].normal   = shape->normals[i];
    }
    SDL_GPUBuffer *buf = forge_scene_upload_buffer(scene,
        SDL_GPU_BUFFERUSAGE_VERTEX, verts,
        (Uint32)shape->vertex_count * (Uint32)sizeof(ForgeSceneVertex));
    SDL_free(verts);
    return buf;
}

/* ── Helper: add_box_to_world ─────────────────────────────────────────────── */
/*
 * Creates a rigid body with a box collision shape at the given position,
 * adds it to the physics world, and records the display color for rendering.
 */
static int add_box_to_world(app_state *state, vec3 pos, float mass,
                            vec3 half_ext, const float color[4])
{
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        pos, mass, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, DEFAULT_RESTIT);
    if (mass > 0.0f) {
        forge_physics_rigid_body_set_inertia_box(&rb, half_ext);
    }

    ForgePhysicsCollisionShape sh = forge_physics_shape_box(half_ext);
    int idx = forge_physics_world_add_body(&state->world, &rb, &sh);

    if (idx >= 0 && idx < MAX_BODIES) {
        SDL_memcpy(state->body_colors[idx], color, 4 * sizeof(float));
    }
    state->num_bodies = forge_physics_world_body_count(&state->world);
    return idx;
}

/* ── Helper: add_sphere_to_world (for ball launcher) ─────────────────────── */
/*
 * Creates a rigid body with a sphere collision shape at the given position,
 * adds it to the physics world, and records the display color for rendering.
 */
static int add_sphere_to_world(app_state *state, vec3 pos, float mass,
                                float radius, const float color[4])
{
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        pos, mass, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, DEFAULT_RESTIT);
    if (mass > 0.0f) {
        forge_physics_rigid_body_set_inertia_sphere(&rb, radius);
    }

    ForgePhysicsCollisionShape sh = forge_physics_shape_sphere(radius);
    int idx = forge_physics_world_add_body(&state->world, &rb, &sh);

    if (idx >= 0 && idx < MAX_BODIES) {
        SDL_memcpy(state->body_colors[idx], color, 4 * sizeof(float));
    }
    state->num_bodies = forge_physics_world_body_count(&state->world);
    return idx;
}

/* ── World config helper ──────────────────────────────────────────────────── */
/*
 * Returns a default world config with the specified solver iteration count.
 * Higher iteration counts improve stacking stability at the cost of CPU time.
 */
static ForgePhysicsWorldConfig make_world_config(int solver_iters)
{
    ForgePhysicsWorldConfig cfg = forge_physics_world_config_default();
    cfg.solver_iterations = solver_iters;
    return cfg;
}

/* ── Scene setup: Tall Tower (12 boxes) ───────────────────────────────────── */
/*
 * Stacks 12 boxes vertically along the Y axis. A demanding test for the
 * sequential impulse solver — taller stacks require more iterations to
 * propagate impulses from the ground up through the entire column.
 */
static void setup_scene_tower(app_state *state)
{
    forge_physics_world_destroy(&state->world);
    forge_physics_world_init(&state->world, make_world_config(TOWER_SOLVER_ITERS));

    vec3 half = vec3_create(BOX_HALF, BOX_HALF, BOX_HALF);
    for (int i = 0; i < TOWER_BOX_COUNT; i++) {
        float y = BOX_HALF + (float)i * (2.0f * BOX_HALF + STACK_OFFSET);
        int ci = i % NUM_BODY_COLORS;
        add_box_to_world(state, vec3_create(0.0f, y, 0.0f),
                         BOX_MASS, half, BODY_COLORS[ci]);
    }
}

/* ── Scene setup: Pyramid (28 boxes, 7 layers) ────────────────────────────── */
/*
 * Builds a 2D pyramid with 7 boxes on the base tapering to 1 at the top.
 * Exercises horizontal contact propagation — each layer must be supported
 * by the one below across a wide contact area.
 */
static void setup_scene_pyramid(app_state *state)
{
    forge_physics_world_destroy(&state->world);
    forge_physics_world_init(&state->world, make_world_config(PYRAMID_SOLVER_ITERS));

    vec3 half = vec3_create(BOX_HALF, BOX_HALF, BOX_HALF);
    float box_size = 2.0f * BOX_HALF;

    int idx = 0;
    for (int row = 0; row < PYRAMID_ROW_COUNT; row++) {
        int boxes_in_row = PYRAMID_ROW_COUNT - row;
        float row_width = (float)boxes_in_row * box_size;
        float start_x = -row_width * 0.5f + BOX_HALF;
        float y = BOX_HALF + (float)row * (box_size + STACK_OFFSET);

        for (int col = 0; col < boxes_in_row; col++) {
            float x = start_x + (float)col * box_size;
            int ci = idx % NUM_BODY_COLORS;
            add_box_to_world(state, vec3_create(x, y, 0.0f),
                             BOX_MASS, half, BODY_COLORS[ci]);
            idx++;
        }
    }
}

/* ── Scene setup: Wall (30 boxes, 6 columns x 5 rows) ────────────────────── */
/*
 * Arranges 30 boxes in a flat wall — 6 wide, 5 tall. A good test for
 * island detection: each column is a separate island until they collide,
 * at which point they merge into a single island.
 */
static void setup_scene_wall(app_state *state)
{
    forge_physics_world_destroy(&state->world);
    forge_physics_world_init(&state->world, make_world_config(WALL_SOLVER_ITERS));

    vec3 half = vec3_create(BOX_HALF, BOX_HALF, BOX_HALF);
    float box_size = 2.0f * BOX_HALF;
    int cols = WALL_COLS, rows = WALL_ROWS;
    float wall_width = (float)cols * box_size;
    float start_x = -wall_width * 0.5f + BOX_HALF;

    int idx = 0;
    for (int row = 0; row < rows; row++) {
        float y = BOX_HALF + (float)row * (box_size + STACK_OFFSET);
        for (int col = 0; col < cols; col++) {
            float x = start_x + (float)col * box_size;
            int ci = idx % NUM_BODY_COLORS;
            add_box_to_world(state, vec3_create(x, y, 0.0f),
                             BOX_MASS, half, BODY_COLORS[ci]);
            idx++;
        }
    }
}

/* ── Scene setup: Stress Test (20 boxes) ─────────────────────────────────── */
/*
 * A single 20-box tower — taller than the standard tower and using 60 solver
 * iterations. Demonstrates how iteration count directly affects stability:
 * compare with the 12-box tower at 40 iterations to see the difference.
 */
static void setup_scene_stress(app_state *state)
{
    forge_physics_world_destroy(&state->world);
    forge_physics_world_init(&state->world, make_world_config(STRESS_SOLVER_ITERS));

    vec3 half = vec3_create(BOX_HALF, BOX_HALF, BOX_HALF);
    for (int i = 0; i < STRESS_BOX_COUNT; i++) {
        float y = BOX_HALF + (float)i * (2.0f * BOX_HALF + STACK_OFFSET);
        int ci = i % NUM_BODY_COLORS;
        add_box_to_world(state, vec3_create(0.0f, y, 0.0f),
                         BOX_MASS, half, BODY_COLORS[ci]);
    }
}

/* ── Scene dispatcher ─────────────────────────────────────────────────────── */
/*
 * Tears down the current scene and rebuilds the selected one. Resets the
 * physics accumulator so the new scene starts from a clean state.
 */
static void set_scene(app_state *state, int idx)
{
    state->scene_index = idx % NUM_SCENES;
    state->accumulator = 0.0f;

    switch (state->scene_index) {
    case 0: setup_scene_tower(state);   break;
    case 1: setup_scene_pyramid(state); break;
    case 2: setup_scene_wall(state);    break;
    case 3: setup_scene_stress(state);  break;
    }
}
/* ═══════════════════════════════════════════════════════════════════════════
 * SDL App Callbacks — Init and Event
 * ═══════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state *state = SDL_calloc(1, sizeof(*state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    ForgeSceneConfig cfg = forge_scene_default_config(
        "Physics 15 - Simulation Loop");
    cfg.cam_start_pos   = vec3_create(0.0f, 8.0f, 20.0f);
    cfg.cam_start_yaw   = 0.0f;
    cfg.cam_start_pitch = -0.20f;
    cfg.font_path = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";

    if (!forge_scene_init(&state->scene, &cfg, argc, argv))
        return SDL_APP_FAILURE;

    /* Generate and upload sphere mesh (for ball launcher + contact markers) */
    ForgeShape sphere = forge_shapes_sphere(SPHERE_SLICES, SPHERE_STACKS);
    if (sphere.vertex_count == 0) {
        SDL_Log("Failed to generate sphere");
        return SDL_APP_FAILURE;
    }
    state->sphere_vb = upload_shape_vb(&state->scene, &sphere);
    state->sphere_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, sphere.indices,
        (Uint32)sphere.index_count * (Uint32)sizeof(uint32_t));
    state->sphere_index_count = (int)sphere.index_count;
    forge_shapes_free(&sphere);
    if (!state->sphere_vb || !state->sphere_ib) {
        SDL_Log("Failed to upload sphere");
        return SDL_APP_FAILURE;
    }

    /* Generate and upload cube mesh with flat normals */
    ForgeShape cube = forge_shapes_cube(CUBE_SUBDIVS, CUBE_SUBDIVS);
    if (cube.vertex_count == 0) {
        SDL_Log("Failed to generate cube");
        return SDL_APP_FAILURE;
    }
    forge_shapes_compute_flat_normals(&cube);
    state->cube_vb = upload_shape_vb(&state->scene, &cube);
    state->cube_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, cube.indices,
        (Uint32)cube.index_count * (Uint32)sizeof(uint32_t));
    state->cube_index_count = (int)cube.index_count;
    forge_shapes_free(&cube);
    if (!state->cube_vb || !state->cube_ib) {
        SDL_Log("Failed to upload cube");
        return SDL_APP_FAILURE;
    }

    /* Initialize simulation state */
    state->speed_scale   = 1.0f;
    state->show_contacts = false;
    state->show_islands  = false;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    /* Parse --scene argument for starting scene index */
    int start_scene = 0;
    for (int i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            start_scene = SDL_atoi(argv[i + 1]);
            if (start_scene < 0) start_scene = 0;
            if (start_scene >= NUM_SCENES) start_scene = NUM_SCENES - 1;
            break;
        }
    }

    set_scene(state, start_scene);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = appstate;
    ForgeScene *s = &state->scene;

    /* Let the scene renderer handle mouse capture, escape, and quit */
    SDL_AppResult result = forge_scene_handle_event(s, event);
    if (result != SDL_APP_CONTINUE) return result;

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        switch (event->key.scancode) {

        /* P — pause / resume simulation */
        case SDL_SCANCODE_P:
            state->paused = !state->paused;
            break;

        /* R — reset simulation to current scene */
        case SDL_SCANCODE_R:
            set_scene(state, state->scene_index);
            break;

        /* T — toggle slow motion (1x / 0.25x) */
        case SDL_SCANCODE_T:
            state->speed_scale = (state->speed_scale == 1.0f) ? 0.25f : 1.0f;
            break;

        /* Tab — cycle to next scene */
        case SDL_SCANCODE_TAB:
            set_scene(state, (state->scene_index + 1) % NUM_SCENES);
            break;

        /* C — toggle contact point visualization (only when mouse not captured) */
        case SDL_SCANCODE_C:
            if (!s->mouse_captured)
                state->show_contacts = !state->show_contacts;
            break;

        /* I — toggle island coloring (only when mouse not captured) */
        case SDL_SCANCODE_I:
            if (!s->mouse_captured)
                state->show_islands = !state->show_islands;
            break;

        /* F — launch a ball from camera position in view direction */
        case SDL_SCANCODE_F:
            if (s->mouse_captured &&
                forge_physics_world_body_count(&state->world) < MAX_BODIES) {
                /* Launch a ball from camera position in view direction */
                quat cam_orient = quat_from_euler(s->cam_yaw, s->cam_pitch, 0.0f);
                vec3 forward = quat_forward(cam_orient);
                vec3 spawn = vec3_add(s->cam_position,
                                      vec3_scale(forward, BALL_SPAWN_OFFSET));
                float launch_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                int ball_idx = add_sphere_to_world(
                    state, spawn, BALL_MASS, BALL_RADIUS, launch_color);
                if (ball_idx >= 0) {
                    vec3 impulse = vec3_scale(forward, BALL_SPEED * BALL_MASS);
                    forge_physics_world_apply_impulse(
                        &state->world, ball_idx, impulse);
                }
            }
            break;

        default:
            break;
        }
    }

    return SDL_APP_CONTINUE;
}
/* ═══════════════════════════════════════════════════════════════════════════
 * SDL App Callbacks — Iterate and Quit
 * ═══════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;
    float dt = forge_scene_dt(s);
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* ── Fixed-timestep physics ──────────────────────────────────────────── */
    if (!state->paused) {
        float step_dt = dt * state->speed_scale;
        state->accumulator += step_dt;
        while (state->accumulator >= state->world.config.fixed_dt) {
            forge_physics_world_step(&state->world);
            state->accumulator -= state->world.config.fixed_dt;
        }
    }

    /* ── Interpolation alpha ─────────────────────────────────────────────── */
    float alpha = state->accumulator / state->world.config.fixed_dt;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    if (state->paused) alpha = 1.0f;

    /* ── Build per-body instance arrays ─────────────────────────────────── */
    /* Separate instances by shape type: boxes and spheres */
    ForgeSceneColoredInstance box_instances[MAX_BODIES];
    ForgeSceneColoredInstance sphere_instances[MAX_BODIES];
    int box_count = 0;
    int sphere_count = 0;

    int nb = forge_physics_world_body_count(&state->world);
    for (int i = 0; i < nb && i < MAX_BODIES; i++) {
        ForgePhysicsRigidBody *b = &state->world.bodies[i];
        const ForgePhysicsCollisionShape *sh = &state->world.shapes[i];

        vec3 pos = vec3_lerp(b->prev_position, b->position, alpha);
        quat ori = quat_slerp(b->prev_orientation, b->orientation, alpha);

        /* Choose color based on display mode */
        float color[4];
        if (state->show_islands) {
            /* Color by island ID */
            int island = forge_physics_world_island_id(&state->world, i);
            int ci = (island >= 0) ? (island % NUM_ISLAND_COLORS) : 0;
            SDL_memcpy(color, ISLAND_COLORS[ci], 4 * sizeof(float));
        } else {
            SDL_memcpy(color, state->body_colors[i], 4 * sizeof(float));
        }

        /* Dim sleeping bodies */
        if (forge_physics_world_is_sleeping(&state->world, i)) {
            color[0] *= SLEEP_DIM_FACTOR;
            color[1] *= SLEEP_DIM_FACTOR;
            color[2] *= SLEEP_DIM_FACTOR;
        }

        /* Build transform based on shape type */
        mat4 model;
        switch (sh->type) {
        case FORGE_PHYSICS_SHAPE_BOX: {
            vec3 he = sh->data.box.half_extents;
            mat4 scale_m = mat4_scale(vec3_create(he.x, he.y, he.z));
            mat4 rot_m = quat_to_mat4(ori);
            mat4 trans_m = mat4_translate(pos);
            model = mat4_multiply(trans_m, mat4_multiply(rot_m, scale_m));

            box_instances[box_count].transform = model;
            SDL_memcpy(box_instances[box_count].color, color, 4 * sizeof(float));
            box_count++;
            break;
        }
        case FORGE_PHYSICS_SHAPE_SPHERE: {
            float r = sh->data.sphere.radius;
            mat4 scale_m = mat4_scale_uniform(r);
            mat4 rot_m = quat_to_mat4(ori);
            mat4 trans_m = mat4_translate(pos);
            model = mat4_multiply(trans_m, mat4_multiply(rot_m, scale_m));

            sphere_instances[sphere_count].transform = model;
            SDL_memcpy(sphere_instances[sphere_count].color, color, 4 * sizeof(float));
            sphere_count++;
            break;
        }
        default:
            break;
        }
    }

    /* ── Contact markers ─────────────────────────────────────────────────── */
    ForgeSceneColoredInstance contact_instances[MAX_CONTACT_MARKERS];
    int contact_count = 0;

    if (state->show_contacts) {
        int nm = state->world.manifold_count;
        for (int mi = 0; mi < nm; mi++) {
            const ForgePhysicsManifold *m = &state->world.manifolds[mi];
            for (int ci = 0; ci < m->count; ci++) {
                if (contact_count >= MAX_CONTACT_MARKERS) break;
                const ForgePhysicsManifoldContact *c = &m->contacts[ci];
                float depth_frac = c->penetration * DEPTH_COLOR_SCALE;
                if (depth_frac < 0.0f) depth_frac = 0.0f;
                if (depth_frac > 1.0f) depth_frac = 1.0f;

                mat4 ct_model = mat4_multiply(
                    mat4_translate(c->world_point),
                    mat4_scale_uniform(CONTACT_MARKER_RADIUS));
                contact_instances[contact_count].transform = ct_model;
                contact_instances[contact_count].color[0] = depth_frac;
                contact_instances[contact_count].color[1] = 1.0f - depth_frac;
                contact_instances[contact_count].color[2] = 0.0f;
                contact_instances[contact_count].color[3] = 1.0f;
                contact_count++;
            }
        }
    }

    /* ── Upload instance buffers ─────────────────────────────────────────── */
    SDL_GPUBuffer *box_inst_buf     = NULL;
    SDL_GPUBuffer *sphere_inst_buf  = NULL;
    SDL_GPUBuffer *contact_inst_buf = NULL;

    forge_scene_begin_deferred_uploads(s);
    if (box_count > 0) {
        box_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            box_instances, (Uint32)(box_count * sizeof(ForgeSceneColoredInstance)));
    }
    if (sphere_count > 0) {
        sphere_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            sphere_instances, (Uint32)(sphere_count * sizeof(ForgeSceneColoredInstance)));
    }
    if (contact_count > 0) {
        contact_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            contact_instances, (Uint32)(contact_count * sizeof(ForgeSceneColoredInstance)));
    }
    forge_scene_end_deferred_uploads(s);

    /* ── Shadow pass ─────────────────────────────────────────────────────── */
    forge_scene_begin_shadow_pass(s);
    if (box_inst_buf) {
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->cube_vb, state->cube_ib,
            (Uint32)state->cube_index_count,
            box_inst_buf, (Uint32)box_count);
    }
    if (sphere_inst_buf) {
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            (Uint32)state->sphere_index_count,
            sphere_inst_buf, (Uint32)sphere_count);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────────────── */
    forge_scene_begin_main_pass(s);
    float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (box_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->cube_vb, state->cube_ib,
            (Uint32)state->cube_index_count,
            box_inst_buf, (Uint32)box_count, white);
    }
    if (sphere_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            (Uint32)state->sphere_index_count,
            sphere_inst_buf, (Uint32)sphere_count, white);
    }
    if (contact_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            (Uint32)state->sphere_index_count,
            contact_inst_buf, (Uint32)contact_count, white);
    }

    /* ── Contact normal debug lines ──────────────────────────────────────── */
    if (state->show_contacts) {
        int nm = state->world.manifold_count;
        for (int mi = 0; mi < nm; mi++) {
            const ForgePhysicsManifold *m = &state->world.manifolds[mi];
            for (int ci = 0; ci < m->count; ci++) {
                const ForgePhysicsManifoldContact *c = &m->contacts[ci];
                float line_len = c->penetration * NORMAL_LINE_SCALE;
                if (line_len < NORMAL_LINE_MIN) line_len = NORMAL_LINE_MIN;
                vec3 end = vec3_add(c->world_point, vec3_scale(m->normal, line_len));
                float depth_frac = c->penetration * DEPTH_COLOR_SCALE;
                if (depth_frac < 0.0f) depth_frac = 0.0f;
                if (depth_frac > 1.0f) depth_frac = 1.0f;
                vec4 line_color = vec4_create(depth_frac, 1.0f - depth_frac, 0.0f, 1.0f);
                forge_scene_debug_line(s, c->world_point, end, line_color, false);
            }
        }
        forge_scene_draw_debug_lines(s);
    }

    forge_scene_draw_grid(s);
    forge_scene_end_main_pass(s);

    /* ── Release per-frame instance buffers ──────────────────────────────── */
    SDL_GPUDevice *dev = forge_scene_device(s);
    if (box_inst_buf)     SDL_ReleaseGPUBuffer(dev, box_inst_buf);
    if (sphere_inst_buf)  SDL_ReleaseGPUBuffer(dev, sphere_inst_buf);
    if (contact_inst_buf) SDL_ReleaseGPUBuffer(dev, contact_inst_buf);

    /* ── UI pass ─────────────────────────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);

    ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
    char buf[128];

    if (wctx && forge_ui_wctx_window_begin(wctx, "Simulation Loop",
                                            &state->ui_window)) {
        ForgeUiContext *ui = wctx->ctx;

        /* Scene info */
        SDL_snprintf(buf, sizeof(buf), "Scene %d: %s",
                     state->scene_index + 1, SCENE_NAMES[state->scene_index]);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        SDL_snprintf(buf, sizeof(buf), "Bodies: %d", nb);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* World stats */
        SDL_snprintf(buf, sizeof(buf), "Active:   %d", state->world.active_body_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        SDL_snprintf(buf, sizeof(buf), "Sleeping: %d", state->world.sleeping_body_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        SDL_snprintf(buf, sizeof(buf), "Static:   %d", state->world.static_body_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        SDL_snprintf(buf, sizeof(buf), "Islands:  %d", state->world.island_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Contact stats */
        SDL_snprintf(buf, sizeof(buf), "Contacts:  %d", state->world.total_contact_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        SDL_snprintf(buf, sizeof(buf), "Manifolds: %d", state->world.manifold_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Solver config */
        SDL_snprintf(buf, sizeof(buf), "Solver iters: %d",
                     state->world.config.solver_iterations);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        SDL_snprintf(buf, sizeof(buf), "Warm-start:   %s",
                     state->world.config.warm_start ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        SDL_snprintf(buf, sizeof(buf), "Sleeping:     %s",
                     state->world.config.enable_sleeping ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Display toggles */
        SDL_snprintf(buf, sizeof(buf), "Show islands: %s  [I]",
                     state->show_islands ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        SDL_snprintf(buf, sizeof(buf), "Show contacts: %s  [C]",
                     state->show_contacts ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Speed / pause */
        SDL_snprintf(buf, sizeof(buf), "Speed: %.2gx  %s",
                     (double)state->speed_scale,
                     state->paused ? "[PAUSED]" : "");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Controls */
        forge_ui_ctx_label_layout(ui, "Tab: next scene  R: reset",  LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "P: pause  T: slow-mo",       LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "I: islands  C: contacts",    LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "F: launch ball (mouse captured)", LABEL_HEIGHT);

        forge_ui_wctx_window_end(wctx);
    }

    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    if (!appstate) return;
    app_state *state = appstate;

    SDL_GPUDevice *device = forge_scene_device(&state->scene);

    /* Release GPU resources */
    if (state->sphere_vb) SDL_ReleaseGPUBuffer(device, state->sphere_vb);
    if (state->sphere_ib) SDL_ReleaseGPUBuffer(device, state->sphere_ib);
    if (state->cube_vb)   SDL_ReleaseGPUBuffer(device, state->cube_vb);
    if (state->cube_ib)   SDL_ReleaseGPUBuffer(device, state->cube_ib);

    /* Destroy physics world */
    forge_physics_world_destroy(&state->world);

    /* Destroy scene renderer */
    forge_scene_destroy(&state->scene);

    SDL_free(state);
}
