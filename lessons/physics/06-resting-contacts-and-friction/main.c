/*
 * Physics Lesson 06 — Resting Contacts and Friction
 *
 * Demonstrates: plane contact detection for rigid body boxes and spheres,
 * Coulomb friction (static and dynamic), resting contact resolution with
 * iterative solver, and stacking with simple shapes. Features a forge UI
 * overlay with sliders for friction, restitution, and solver iterations,
 * plus contact normal visualization and energy/contact readouts.
 *
 * Three selectable scenes:
 *   1. Resting Contacts — spheres and boxes settle on the ground
 *   2. Friction Comparison — objects slide with different friction
 *   3. Stacking — sphere tower with body-body collisions
 *
 * Uses forge_scene.h for all rendering boilerplate (device, window, pipelines,
 * camera, grid, shadow map, sky, UI) — this file focuses on physics.
 *
 * Controls:
 *   WASD              — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   1-3               — select scene
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Physics library — rigid bodies, contacts, friction */
#include "physics/forge_physics.h"

/* Procedural geometry — sphere, cube meshes */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Physics simulation */
#define PHYSICS_DT          (1.0f / 60.0f)
#define MAX_DELTA_TIME       0.1f

/* Scene limits */
#define MAX_BODIES           16

/* Scene 1: Resting Contacts — objects settle on ground */
#define S1_NUM_BODIES        5
#define S1_MASS              5.0f
#define S1_SPHERE_RADIUS     0.4f
#define S1_BOX_HALF          0.4f
#define S1_DROP_HEIGHT_BASE  3.0f
#define S1_DROP_HEIGHT_STEP  1.0f
#define S1_DROP_SPACING      2.0f

/* Scene 2: Friction Comparison — objects slide on ground */
#define S2_NUM_BODIES        4
#define S2_MASS              5.0f
#define S2_BOX_HALF          0.4f
#define S2_SPHERE_RADIUS     0.4f
#define S2_START_Y           0.5f
#define S2_SPACING           2.5f
#define S2_INIT_VEL          8.0f
#define S2_START_Z          (-3.75f)
#define S2_START_X      (-3.0f)

/* Scene 3: Stacking — tower of objects */
#define S3_NUM_BODIES        6
#define S3_MASS              5.0f
#define S3_BOX_HALF          0.5f
#define S3_SPHERE_RADIUS     0.5f
#define S3_BASE_Y            0.5f
#define S3_LAYER_HEIGHT      1.05f
#define S3_LAYER_X_OFFSET    0.05f

/* Common */
#define GROUND_Y             0.0f
#define DEFAULT_DAMPING      0.99f
#define DEFAULT_ANG_DAMPING  0.99f
#define DEFAULT_RESTIT       0.3f
#define DEFAULT_MU_S         0.6f
#define DEFAULT_MU_D         0.4f
#define DEFAULT_GRAVITY      9.81f
#define DEFAULT_SOLVER_ITERS 10

/* Speed control */
#define SLOW_MOTION_SCALE    0.25f
#define NORMAL_SPEED_SCALE   1.0f
#define SLOW_MOTION_THRESH   0.5f

/* Camera start position */
#define CAM_START_X          0.0f
#define CAM_START_Y          5.0f
#define CAM_START_Z         14.0f
#define CAM_START_PITCH     -0.15f

/* Mesh resolution */
#define SPHERE_SLICES       16
#define SPHERE_STACKS        8
#define CUBE_SLICES          1
#define CUBE_STACKS          1

/* UI panel layout */
#define PANEL_X             10.0f
#define PANEL_Y             10.0f
#define PANEL_W            280.0f
#define PANEL_H            600.0f
#define LABEL_HEIGHT        24.0f
#define BUTTON_HEIGHT       30.0f
#define SLIDER_HEIGHT       28.0f

/* Slider ranges */
#define GRAVITY_MIN          0.0f
#define GRAVITY_MAX         20.0f
#define MU_S_MIN             0.0f
#define MU_S_MAX             2.0f
#define MU_D_MIN             0.0f
#define MU_D_MAX             2.0f
#define RESTIT_MIN           0.0f
#define RESTIT_MAX           1.0f
#define SOLVER_ITER_MIN      1.0f
#define SOLVER_ITER_MAX     30.0f
#define INIT_VEL_MIN         0.0f
#define INIT_VEL_MAX        20.0f

/* Number of scenes */
#define NUM_SCENES           3

/* Body shape type for rendering */
#define SHAPE_CUBE           0
#define SHAPE_SPHERE         1

/* Colors (RGBA) */
#define COLOR_RED_R          0.9f
#define COLOR_RED_G          0.3f
#define COLOR_RED_B          0.2f
#define COLOR_RED_A          1.0f

#define COLOR_BLUE_R         0.2f
#define COLOR_BLUE_G         0.5f
#define COLOR_BLUE_B         0.9f
#define COLOR_BLUE_A         1.0f

#define COLOR_GREEN_R        0.3f
#define COLOR_GREEN_G        0.8f
#define COLOR_GREEN_B        0.3f
#define COLOR_GREEN_A        1.0f

#define COLOR_ORANGE_R       0.9f
#define COLOR_ORANGE_G       0.6f
#define COLOR_ORANGE_B       0.1f
#define COLOR_ORANGE_A       1.0f

#define COLOR_PURPLE_R       0.7f
#define COLOR_PURPLE_G       0.3f
#define COLOR_PURPLE_B       0.9f
#define COLOR_PURPLE_A       1.0f

#define COLOR_TEAL_R         0.2f
#define COLOR_TEAL_G         0.8f
#define COLOR_TEAL_B         0.8f
#define COLOR_TEAL_A         1.0f

/* ── Types ────────────────────────────────────────────────────────── */

/* Per-body rendering metadata */
typedef struct BodyRenderInfo {
    int   shape_type;     /* SHAPE_CUBE or SHAPE_SPHERE */
    vec3  render_scale;   /* scale factors for the unit mesh */
    float color[4];       /* RGBA color for this body */
    float half_h;         /* half-height for ground collision */
    vec3  half_extents;   /* box half-extents (for box-plane detection) */
    float sphere_radius;  /* sphere radius (for sphere-plane detection) */
} BodyRenderInfo;

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* GPU geometry — vertex/index buffers for each shape type */
    SDL_GPUBuffer *cube_vb;
    SDL_GPUBuffer *cube_ib;
    Uint32         cube_index_count;

    SDL_GPUBuffer *sphere_vb;
    SDL_GPUBuffer *sphere_ib;
    Uint32         sphere_index_count;

    /* Physics state */
    ForgePhysicsRigidBody bodies[MAX_BODIES];
    BodyRenderInfo        body_info[MAX_BODIES];
    int                   num_bodies;

    /* Contact state (for UI readout) */
    int last_contact_count;

    /* Simulation control */
    int   scene_index;
    float accumulator;
    bool  paused;
    float speed_scale;

    /* UI-adjustable parameters */
    float ui_gravity;
    float ui_mu_s;            /* static friction coefficient */
    float ui_mu_d;            /* dynamic friction coefficient */
    float ui_restitution;     /* coefficient of restitution */
    float ui_solver_iters_f;  /* solver iterations (float for slider) */
    float ui_init_velocity;   /* Scene 2: initial sliding velocity */
    /* UI scroll */
    float panel_scroll;
} app_state;

/* ── Helper: upload_shape_vb ─────────────────────────────────────── */

static SDL_GPUBuffer *upload_shape_vb(ForgeScene *scene,
                                       const ForgeShape *shape)
{
    ForgeSceneVertex *verts = SDL_calloc((size_t)shape->vertex_count,
                                         sizeof(ForgeSceneVertex));
    if (!verts) {
        SDL_Log("ERROR: Failed to allocate %d vertices for shape upload",
                shape->vertex_count);
        return NULL;
    }

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

/* ── Helper: set body color ──────────────────────────────────────── */

static void set_color(BodyRenderInfo *info, float r, float g, float b)
{
    info->color[0] = r;
    info->color[1] = g;
    info->color[2] = b;
    info->color[3] = 1.0f;
}

/* ── Helper: init a box body ─────────────────────────────────────── */

static void init_box_body(app_state *state, int idx, vec3 pos,
                           vec3 half_ext, float mass, float restit,
                           float cr, float cg, float cb)
{
    state->bodies[idx] = forge_physics_rigid_body_create(
        pos, mass, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, restit);
    forge_physics_rigid_body_set_inertia_box(&state->bodies[idx], half_ext);

    state->body_info[idx].shape_type = SHAPE_CUBE;
    state->body_info[idx].render_scale = half_ext;
    state->body_info[idx].half_h = half_ext.y;
    state->body_info[idx].half_extents = half_ext;
    state->body_info[idx].sphere_radius = 0.0f;
    set_color(&state->body_info[idx], cr, cg, cb);
}

/* ── Helper: init a sphere body ──────────────────────────────────── */

static void init_sphere_body(app_state *state, int idx, vec3 pos,
                              float radius, float mass, float restit,
                              float cr, float cg, float cb)
{
    state->bodies[idx] = forge_physics_rigid_body_create(
        pos, mass, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, restit);
    forge_physics_rigid_body_set_inertia_sphere(&state->bodies[idx], radius);

    state->body_info[idx].shape_type = SHAPE_SPHERE;
    state->body_info[idx].render_scale = vec3_create(radius, radius, radius);
    state->body_info[idx].half_h = radius;
    state->body_info[idx].half_extents = vec3_create(0, 0, 0);
    state->body_info[idx].sphere_radius = radius;
    set_color(&state->body_info[idx], cr, cg, cb);
}

/* ── Scene initialization ────────────────────────────────────────── */

/* Scene 1: Resting Contacts — objects drop and settle */
static void init_scene_1(app_state *state)
{
    state->num_bodies = S1_NUM_BODIES;

    /* Alternating boxes and spheres at different heights */
    float colors[S1_NUM_BODIES][3] = {
        {COLOR_RED_R,    COLOR_RED_G,    COLOR_RED_B},
        {COLOR_BLUE_R,   COLOR_BLUE_G,   COLOR_BLUE_B},
        {COLOR_GREEN_R,  COLOR_GREEN_G,  COLOR_GREEN_B},
        {COLOR_ORANGE_R, COLOR_ORANGE_G, COLOR_ORANGE_B},
        {COLOR_PURPLE_R, COLOR_PURPLE_G, COLOR_PURPLE_B}
    };

    for (int i = 0; i < S1_NUM_BODIES; i++) {
        float x = ((float)i - (float)(S1_NUM_BODIES - 1) * 0.5f) * S1_DROP_SPACING;
        float y = S1_DROP_HEIGHT_BASE + (float)i * S1_DROP_HEIGHT_STEP;

        if (i % 2 == 0) {
            /* Box */
            vec3 he = vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF);
            init_box_body(state, i, vec3_create(x, y, 0.0f), he,
                          S1_MASS, state->ui_restitution,
                          colors[i][0], colors[i][1], colors[i][2]);
        } else {
            /* Sphere */
            init_sphere_body(state, i, vec3_create(x, y, 0.0f),
                             S1_SPHERE_RADIUS, S1_MASS,
                             state->ui_restitution,
                             colors[i][0], colors[i][1], colors[i][2]);
        }
    }
}

/* Scene 2: Friction Comparison — objects slide with different friction */
static void init_scene_2(app_state *state)
{
    state->num_bodies = S2_NUM_BODIES;

    float colors[S2_NUM_BODIES][3] = {
        {COLOR_RED_R,    COLOR_RED_G,    COLOR_RED_B},
        {COLOR_BLUE_R,   COLOR_BLUE_G,   COLOR_BLUE_B},
        {COLOR_GREEN_R,  COLOR_GREEN_G,  COLOR_GREEN_B},
        {COLOR_ORANGE_R, COLOR_ORANGE_G, COLOR_ORANGE_B}
    };

    for (int i = 0; i < S2_NUM_BODIES; i++) {
        float z = S2_START_Z + (float)i * S2_SPACING;

        if (i < 2) {
            /* Boxes */
            vec3 he = vec3_create(S2_BOX_HALF, S2_BOX_HALF, S2_BOX_HALF);
            init_box_body(state, i, vec3_create(S2_START_X, S2_START_Y, z), he,
                          S2_MASS, state->ui_restitution,
                          colors[i][0], colors[i][1], colors[i][2]);
        } else {
            /* Spheres */
            init_sphere_body(state, i,
                             vec3_create(S2_START_X, S2_SPHERE_RADIUS, z),
                             S2_SPHERE_RADIUS, S2_MASS,
                             state->ui_restitution,
                             colors[i][0], colors[i][1], colors[i][2]);
        }

        /* All start with horizontal velocity (set from UI slider;
         * changing the slider mid-simulation requires pressing R
         * to reset for the new value to take effect) */
        state->bodies[i].velocity =
            vec3_create(state->ui_init_velocity, 0.0f, 0.0f);
    }
}

/* Scene 3: Stacking — tower of spheres.
 * All spheres so body-body collision works (sphere-sphere detection).
 * Box-box and sphere-box require GJK (Lesson 08). */
static void init_scene_3(app_state *state)
{
    state->num_bodies = S3_NUM_BODIES;

    float colors[S3_NUM_BODIES][3] = {
        {COLOR_RED_R,    COLOR_RED_G,    COLOR_RED_B},
        {COLOR_BLUE_R,   COLOR_BLUE_G,   COLOR_BLUE_B},
        {COLOR_GREEN_R,  COLOR_GREEN_G,  COLOR_GREEN_B},
        {COLOR_ORANGE_R, COLOR_ORANGE_G, COLOR_ORANGE_B},
        {COLOR_PURPLE_R, COLOR_PURPLE_G, COLOR_PURPLE_B},
        {COLOR_TEAL_R,   COLOR_TEAL_G,   COLOR_TEAL_B}
    };

    for (int i = 0; i < S3_NUM_BODIES; i++) {
        float y = S3_BASE_Y + (float)i * S3_LAYER_HEIGHT;
        /* Slight offset on alternating layers for visual interest */
        float x = (i % 2 == 0) ? 0.0f : S3_LAYER_X_OFFSET;

        init_sphere_body(state, i, vec3_create(x, y, 0.0f),
                         S3_SPHERE_RADIUS, S3_MASS,
                         state->ui_restitution,
                         colors[i][0], colors[i][1], colors[i][2]);
    }
}

/* Initialize current scene and save initial state for reset */
static void init_current_scene(app_state *state)
{
    switch (state->scene_index) {
    case 0: init_scene_1(state); break;
    case 1: init_scene_2(state); break;
    case 2: init_scene_3(state); break;
    default:
        state->scene_index = 0;
        init_scene_1(state);
        break;
    }

    state->accumulator       = 0.0f;
    state->last_contact_count = 0;
}

/* Reset simulation */
static void reset_simulation(app_state *state)
{
    init_current_scene(state);
}

/* ── Physics step ────────────────────────────────────────────────── */

static void physics_step(app_state *state)
{
    int solver_iters = (int)state->ui_solver_iters_f;
    if (solver_iters < 1) solver_iters = 1;
    if (solver_iters > (int)SOLVER_ITER_MAX) solver_iters = (int)SOLVER_ITER_MAX;

    vec3 gravity   = vec3_create(0.0f, -state->ui_gravity, 0.0f);
    vec3 plane_pt  = vec3_create(0.0f, GROUND_Y, 0.0f);
    vec3 plane_n   = vec3_create(0.0f, 1.0f, 0.0f);

    /* UI sliders allow mu_d > mu_s; the resolve function sanitizes
     * this internally (clamps mu_d <= mu_s per Coulomb model). */
    float mu_s = state->ui_mu_s;
    float mu_d = state->ui_mu_d;

    /* Apply forces */
    for (int i = 0; i < state->num_bodies; i++) {
        forge_physics_rigid_body_apply_gravity(&state->bodies[i], gravity);
    }

    /* Integrate */
    for (int i = 0; i < state->num_bodies; i++) {
        forge_physics_rigid_body_integrate(&state->bodies[i], PHYSICS_DT);
    }

    /* Detect contacts with ground plane */
    ForgePhysicsRBContact contacts[FORGE_PHYSICS_MAX_RB_CONTACTS];
    int num_contacts = 0;

    for (int i = 0; i < state->num_bodies; i++) {
        const BodyRenderInfo *info = &state->body_info[i];

        if (info->shape_type == SHAPE_SPHERE) {
            ForgePhysicsRBContact c;
            if (forge_physics_rb_collide_sphere_plane(
                    &state->bodies[i], i, info->sphere_radius,
                    plane_pt, plane_n, mu_s, mu_d, &c)) {
                if (num_contacts < FORGE_PHYSICS_MAX_RB_CONTACTS) {
                    contacts[num_contacts++] = c;
                }
            }
        } else {
            /* Box */
            int n = forge_physics_rb_collide_box_plane(
                &state->bodies[i], i, info->half_extents,
                plane_pt, plane_n, mu_s, mu_d,
                &contacts[num_contacts],
                FORGE_PHYSICS_MAX_RB_CONTACTS - num_contacts);
            num_contacts += n;
        }
    }

    /* Detect pairwise sphere-sphere contacts (body-body collisions).
     * O(n^2) all-pairs — acceptable for small body counts in this lesson.
     * Broadphase optimization is covered in Lesson 07. */
    for (int i = 0; i < state->num_bodies && num_contacts < FORGE_PHYSICS_MAX_RB_CONTACTS; i++) {
        if (state->body_info[i].shape_type != SHAPE_SPHERE) continue;
        for (int j = i + 1; j < state->num_bodies && num_contacts < FORGE_PHYSICS_MAX_RB_CONTACTS; j++) {
            if (state->body_info[j].shape_type != SHAPE_SPHERE) continue;
            ForgePhysicsRBContact c;
            if (forge_physics_rb_collide_sphere_sphere(
                    &state->bodies[i], i, state->body_info[i].sphere_radius,
                    &state->bodies[j], j, state->body_info[j].sphere_radius,
                    mu_s, mu_d, &c)) {
                contacts[num_contacts++] = c;
            }
        }
    }

    /* Resolve contacts with iterative solver */
    if (num_contacts > 0) {
        /* Set restitution from UI for all bodies */
        for (int i = 0; i < state->num_bodies; i++) {
            state->bodies[i].restitution = state->ui_restitution;
        }

        forge_physics_rb_resolve_contacts(contacts, num_contacts,
                                           state->bodies, state->num_bodies,
                                           solver_iters, PHYSICS_DT);
    }

    state->last_contact_count = num_contacts;
}

/* ── Helper: get interpolated model matrix ────────────────────────── */

static mat4 get_body_interp_matrix(const ForgePhysicsRigidBody *rb,
                                     const BodyRenderInfo *info,
                                     float alpha)
{
    vec3 pos = vec3_lerp(rb->prev_position, rb->position, alpha);
    quat orient = quat_slerp(rb->prev_orientation, rb->orientation, alpha);
    mat4 translation = mat4_translate(pos);
    mat4 rotation    = quat_to_mat4(orient);
    mat4 scale       = mat4_scale(info->render_scale);
    return mat4_multiply(translation, mat4_multiply(rotation, scale));
}

/* ── Helper: compute total kinetic energy ─────────────────────────── */

static void compute_kinetic_energy(const app_state *state,
                                    float *out_linear_ke,
                                    float *out_rotational_ke)
{
    float lin_ke = 0.0f;
    float rot_ke = 0.0f;

    for (int i = 0; i < state->num_bodies; i++) {
        const ForgePhysicsRigidBody *rb = &state->bodies[i];
        if (rb->inv_mass == 0.0f) continue;

        float speed_sq = vec3_length_squared(rb->velocity);
        lin_ke += 0.5f * rb->mass * speed_sq;

        vec3 Iw = mat3_multiply_vec3(rb->inertia_world, rb->angular_velocity);
        rot_ke += 0.5f * vec3_dot(rb->angular_velocity, Iw);
    }

    *out_linear_ke     = lin_ke;
    *out_rotational_ke = rot_ke;
}

/* ── SDL_AppInit ─────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("ERROR: Failed to allocate app_state");
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    ForgeSceneConfig cfg = forge_scene_default_config(
        "Physics Lesson 06 \xe2\x80\x94 Resting Contacts and Friction");
    cfg.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";
    cfg.font_size       = 16.0f;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("ERROR: forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* Generate and upload cube geometry (with flat normals) */
    ForgeShape cube = forge_shapes_cube(CUBE_SLICES, CUBE_STACKS);
    if (cube.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_cube failed");
        return SDL_APP_FAILURE;
    }
    forge_shapes_compute_flat_normals(&cube);
    state->cube_vb = upload_shape_vb(&state->scene, &cube);
    state->cube_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, cube.indices,
        (Uint32)cube.index_count * (Uint32)sizeof(uint32_t));
    state->cube_index_count = (Uint32)cube.index_count;
    forge_shapes_free(&cube);

    if (!state->cube_vb || !state->cube_ib) {
        SDL_Log("ERROR: Failed to upload cube geometry");
        return SDL_APP_FAILURE;
    }

    /* Generate and upload sphere geometry */
    ForgeShape sphere = forge_shapes_sphere(SPHERE_SLICES, SPHERE_STACKS);
    if (sphere.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_sphere failed");
        return SDL_APP_FAILURE;
    }
    state->sphere_vb = upload_shape_vb(&state->scene, &sphere);
    state->sphere_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, sphere.indices,
        (Uint32)sphere.index_count * (Uint32)sizeof(uint32_t));
    state->sphere_index_count = (Uint32)sphere.index_count;
    forge_shapes_free(&sphere);

    if (!state->sphere_vb || !state->sphere_ib) {
        SDL_Log("ERROR: Failed to upload sphere geometry");
        return SDL_APP_FAILURE;
    }

    /* Default UI parameters */
    state->scene_index       = 0;
    state->accumulator       = 0.0f;
    state->paused            = false;
    state->speed_scale       = NORMAL_SPEED_SCALE;
    state->panel_scroll      = 0.0f;
    state->ui_gravity        = DEFAULT_GRAVITY;
    state->ui_mu_s           = DEFAULT_MU_S;
    state->ui_mu_d           = DEFAULT_MU_D;
    state->ui_restitution    = DEFAULT_RESTIT;
    state->ui_solver_iters_f = (float)DEFAULT_SOLVER_ITERS;
    state->ui_init_velocity  = S2_INIT_VEL;
    state->last_contact_count = 0;

    init_current_scene(state);

    return SDL_APP_CONTINUE;
}

/* Switch to a scene by index and reinitialize simulation state */
static void set_scene(app_state *state, int index)
{
    if (index < 0 || index >= NUM_SCENES) index = 0;
    state->scene_index = index;
    init_current_scene(state);
}

/* ── SDL_AppEvent ────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    SDL_AppResult result = forge_scene_handle_event(&state->scene, event);
    if (result != SDL_APP_CONTINUE) return result;

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        switch (event->key.scancode) {
        case SDL_SCANCODE_R:
            reset_simulation(state);
            SDL_Log("Simulation reset");
            break;
        case SDL_SCANCODE_P:
            state->paused = !state->paused;
            SDL_Log("Simulation %s",
                    state->paused ? "paused" : "resumed");
            break;
        case SDL_SCANCODE_T:
            state->speed_scale =
                (state->speed_scale < SLOW_MOTION_THRESH)
                    ? NORMAL_SPEED_SCALE : SLOW_MOTION_SCALE;
            SDL_Log("Speed: %.2fx", (double)state->speed_scale);
            break;
        case SDL_SCANCODE_1: set_scene(state, 0); break;
        case SDL_SCANCODE_2: set_scene(state, 1); break;
        case SDL_SCANCODE_3: set_scene(state, 2); break;
        default:
            break;
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;

    float dt = forge_scene_dt(s);
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* ── Fixed-timestep physics ──────────────────────────────────── */

    if (!state->paused) {
        float sim_dt = dt * state->speed_scale;
        state->accumulator += sim_dt;

        while (state->accumulator >= PHYSICS_DT) {
            physics_step(state);
            state->accumulator -= PHYSICS_DT;
        }
    }

    /* Interpolation factor for smooth rendering */
    float alpha = state->accumulator / PHYSICS_DT;
    if (alpha > 1.0f) alpha = 1.0f;

    /* ── Shadow pass ─────────────────────────────────────────────── */

    forge_scene_begin_shadow_pass(s);

    for (int i = 0; i < state->num_bodies; i++) {
        mat4 model = get_body_interp_matrix(&state->bodies[i],
                                              &state->body_info[i], alpha);

        switch (state->body_info[i].shape_type) {
        case SHAPE_CUBE:
            forge_scene_draw_shadow_mesh(s, state->cube_vb, state->cube_ib,
                                         state->cube_index_count, model);
            break;
        case SHAPE_SPHERE:
            forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                         state->sphere_index_count, model);
            break;
        }
    }

    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */

    forge_scene_begin_main_pass(s);

    for (int i = 0; i < state->num_bodies; i++) {
        mat4 model = get_body_interp_matrix(&state->bodies[i],
                                              &state->body_info[i], alpha);

        switch (state->body_info[i].shape_type) {
        case SHAPE_CUBE:
            forge_scene_draw_mesh(s, state->cube_vb, state->cube_ib,
                                  state->cube_index_count, model,
                                  state->body_info[i].color);
            break;
        case SHAPE_SPHERE:
            forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                                  state->sphere_index_count, model,
                                  state->body_info[i].color);
            break;
        }
    }

    forge_scene_draw_grid(s);
    forge_scene_end_main_pass(s);

    /* ── UI pass ─────────────────────────────────────────────────── */

    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiContext *ui = forge_scene_ui(s);
        if (ui) {
            ForgeUiRect panel = { PANEL_X, PANEL_Y, PANEL_W, PANEL_H };
            if (forge_ui_ctx_panel_begin(ui, "Contacts & Friction",
                                         panel, &state->panel_scroll)) {

                /* Scene selection */
                {
                    const char *scene_names[NUM_SCENES] = {
                        "1: Resting", "2: Friction", "3: Stacking"
                    };
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Scene: %s",
                                 scene_names[state->scene_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                if (forge_ui_ctx_button_layout(ui, "1: Resting Contacts",
                                                BUTTON_HEIGHT))
                    set_scene(state, 0);
                if (forge_ui_ctx_button_layout(ui, "2: Friction",
                                                BUTTON_HEIGHT))
                    set_scene(state, 1);
                if (forge_ui_ctx_button_layout(ui, "3: Stacking",
                                                BUTTON_HEIGHT))
                    set_scene(state, 2);

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Gravity slider */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Gravity: %.1f m/s2",
                                 (double)state->ui_gravity);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##gravity",
                                           &state->ui_gravity,
                                           GRAVITY_MIN, GRAVITY_MAX,
                                           SLIDER_HEIGHT);

                /* Static friction */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Static mu: %.2f",
                                 (double)state->ui_mu_s);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##mus",
                                           &state->ui_mu_s,
                                           MU_S_MIN, MU_S_MAX,
                                           SLIDER_HEIGHT);

                /* Dynamic friction */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Dynamic mu: %.2f",
                                 (double)state->ui_mu_d);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##mud",
                                           &state->ui_mu_d,
                                           MU_D_MIN, MU_D_MAX,
                                           SLIDER_HEIGHT);

                /* Restitution */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Restitution: %.2f",
                                 (double)state->ui_restitution);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##restit",
                                           &state->ui_restitution,
                                           RESTIT_MIN, RESTIT_MAX,
                                           SLIDER_HEIGHT);

                /* Solver iterations */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Solver iters: %d",
                                 (int)state->ui_solver_iters_f);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##iters",
                                           &state->ui_solver_iters_f,
                                           SOLVER_ITER_MIN, SOLVER_ITER_MAX,
                                           SLIDER_HEIGHT);

                /* Scene 2: initial velocity */
                if (state->scene_index == 1) {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Init Vel: %.1f m/s",
                                 (double)state->ui_init_velocity);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    forge_ui_ctx_slider_layout(ui, "##initvel",
                                               &state->ui_init_velocity,
                                               INIT_VEL_MIN, INIT_VEL_MAX,
                                               SLIDER_HEIGHT);
                }

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Info readouts */
                {
                    char buf[80];

                    SDL_snprintf(buf, sizeof(buf), "Bodies: %d",
                                 state->num_bodies);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Contacts: %d",
                                 state->last_contact_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    /* Per-body velocity */
                    for (int i = 0; i < state->num_bodies && i < 4; i++) {
                        float speed = vec3_length(state->bodies[i].velocity);
                        SDL_snprintf(buf, sizeof(buf),
                                     "Body %d vel: %.1f m/s", i,
                                     (double)speed);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }

                    /* Kinetic energy */
                    float lin_ke, rot_ke;
                    compute_kinetic_energy(state, &lin_ke, &rot_ke);

                    SDL_snprintf(buf, sizeof(buf), "Linear KE: %.1f",
                                 (double)lin_ke);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Rotational KE: %.1f",
                                 (double)rot_ke);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Total KE: %.1f",
                                 (double)(lin_ke + rot_ke));
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* FPS */
                {
                    char buf[64];
                    float frame_dt = forge_scene_dt(s);
                    float fps = (frame_dt > 0.0f) ? 1.0f / frame_dt : 0.0f;
                    SDL_snprintf(buf, sizeof(buf), "FPS: %.0f",
                                 (double)fps);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Pause checkbox */
                forge_ui_ctx_checkbox_layout(ui, "Paused",
                                             &state->paused, LABEL_HEIGHT);

                /* Slow motion checkbox */
                {
                    bool slow = (state->speed_scale < SLOW_MOTION_THRESH);
                    if (forge_ui_ctx_checkbox_layout(ui, "Slow Motion",
                                                      &slow, LABEL_HEIGHT)) {
                        state->speed_scale = slow
                            ? SLOW_MOTION_SCALE : NORMAL_SPEED_SCALE;
                    }
                }

                /* Reset button */
                if (forge_ui_ctx_button_layout(ui, "Reset", BUTTON_HEIGHT)) {
                    reset_simulation(state);
                }

                forge_ui_ctx_panel_end(ui);
            }
        }
    }
    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
}

/* ── SDL_AppQuit ─────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (!state) return;

    if (forge_scene_device(&state->scene)) {
        if (!SDL_WaitForGPUIdle(forge_scene_device(&state->scene))) {
            SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }
        if (state->cube_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->cube_vb);
        if (state->cube_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->cube_ib);
        if (state->sphere_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_vb);
        if (state->sphere_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_ib);
    }

    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
