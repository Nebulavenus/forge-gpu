/*
 * Physics Lesson 03 — Particle Collisions
 *
 * Demonstrates: sphere-sphere collision detection, impulse-based collision
 * response, coefficient of restitution, conservation of momentum, positional
 * correction, and ground-plane collisions.
 *
 * Four selectable scenes:
 *   1. Two-Body — head-on elastic collision between equal-mass particles
 *   2. Particle Rain — 30 particles falling under gravity, colliding
 *   3. Newton's Cradle — 5 pendulums demonstrating momentum transfer
 *   4. Billiard Break — triangle formation hit by a cue ball
 *
 * Uses forge_scene.h for all rendering boilerplate (device, window, pipelines,
 * camera, grid, shadow map, sky, UI) — this file focuses on physics.
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   1-4               — select scene
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Physics library — particles, springs, constraints, collisions */
#include "physics/forge_physics.h"

/* Procedural geometry — sphere + cylinder meshes */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Physics simulation */
#define PHYSICS_DT          (1.0f / 60.0f)
#define MAX_DELTA_TIME       0.1f

/* Particle limits */
#define MAX_PARTICLES        50
#define MAX_CONTACTS         FORGE_PHYSICS_MAX_CONTACTS
#define MAX_CONSTRAINTS      20

/* Scene 1: Two-Body Collision */
#define S1_NUM_PARTICLES     2
#define S1_PARTICLE_MASS     1.0f
#define S1_PARTICLE_RADIUS   0.4f
#define S1_START_X           3.0f
#define S1_START_Y           2.0f
#define S1_INITIAL_SPEED     3.0f

/* Scene 2: Particle Rain */
#define S2_NUM_PARTICLES    30
#define S2_PARTICLE_MASS     1.0f
#define S2_PARTICLE_RADIUS   0.3f
#define S2_X_MIN            -4.0f
#define S2_X_MAX             4.0f
#define S2_Y_MIN             3.0f
#define S2_Y_MAX            12.0f
#define S2_Z_MIN            -4.0f
#define S2_Z_MAX             4.0f
#define S2_LCG_SEED         42

/* Scene 3: Newton's Cradle */
#define S3_NUM_BALLS         5
#define S3_ANCHOR_Y          6.0f
#define S3_BALL_Y            3.0f
#define S3_BALL_SPACING      0.8f
#define S3_BALL_MASS         1.0f
#define S3_BALL_RADIUS       0.35f
#define S3_BALL_RESTITUTION  1.0f
#define S3_SWING_OFFSET      2.5f
#define S3_SWING_SPEED       3.0f
#define S3_CONSTRAINT_STIFF  1.0f
#define S3_SOLVER_ITERS     10
#define S3_ANCHOR_RADIUS     0.1f

/* Scene 4: Billiard Break */
#define S4_TRIANGLE_ROWS     5
#define S4_BALL_RADIUS       0.3f
#define S4_BALL_SPACING      (S4_BALL_RADIUS * 2.0f + 0.05f)
#define S4_BALL_MASS         1.0f
#define S4_TRIANGLE_Z       -3.0f
#define S4_BALL_Y            S4_BALL_RADIUS
#define S4_CUE_Z             3.0f
#define S4_CUE_SPEED         8.0f
#define S4_BILLIARD_GRAVITY  1.0f
#define S4_BILLIARD_DRAG     0.3f

/* Triangle row packing factor: cos(30°) ≈ 0.866 for equilateral spacing */
#define TRIANGLE_ROW_FACTOR  0.866f

/* Common particle properties */
#define PARTICLE_DAMPING     0.01f
#define GROUND_Y             0.0f
#define DRAG_COEFF           0.02f

/* Camera start position */
#define CAM_START_X          0.0f
#define CAM_START_Y          5.0f
#define CAM_START_Z         12.0f
#define CAM_START_PITCH     -0.15f

/* Mesh resolution */
#define SPHERE_SLICES       16
#define SPHERE_STACKS        8
#define CYLINDER_SLICES     12
#define CYLINDER_STACKS      1

/* Connection rendering */
#define CONN_RADIUS          0.03f

/* Geometry math */
#define NEAR_PARALLEL_DOT    0.999f

/* UI panel layout */
#define PANEL_X             10.0f
#define PANEL_Y             10.0f
#define PANEL_W            260.0f
#define PANEL_H            500.0f
#define LABEL_HEIGHT        24.0f
#define BUTTON_HEIGHT       30.0f
#define SLIDER_HEIGHT       28.0f

/* Slider ranges */
#define RESTITUTION_MIN      0.0f
#define RESTITUTION_MAX      1.0f
#define GRAVITY_MIN          0.0f
#define GRAVITY_MAX         20.0f

/* Colors (RGBA) */
#define COLOR_FIXED_R       0.5f
#define COLOR_FIXED_G       0.5f
#define COLOR_FIXED_B       0.5f
#define COLOR_FIXED_A       1.0f

#define COLOR_S1_R          0.2f
#define COLOR_S1_G          0.6f
#define COLOR_S1_B          1.0f
#define COLOR_S1_A          1.0f

#define COLOR_S2_R          1.0f
#define COLOR_S2_G          0.4f
#define COLOR_S2_B          0.2f
#define COLOR_S2_A          1.0f

#define COLOR_S3_R          0.3f
#define COLOR_S3_G          0.9f
#define COLOR_S3_B          0.3f
#define COLOR_S3_A          1.0f

#define COLOR_S4_R          0.9f
#define COLOR_S4_G          0.3f
#define COLOR_S4_B          0.7f
#define COLOR_S4_A          1.0f

#define COLOR_CONN_R        0.8f
#define COLOR_CONN_G        0.8f
#define COLOR_CONN_B        0.2f
#define COLOR_CONN_A        1.0f

/* Number of scenes */
#define NUM_SCENES           4

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Sphere GPU geometry */
    SDL_GPUBuffer *sphere_vb;          /* vertex buffer for sphere mesh */
    SDL_GPUBuffer *sphere_ib;          /* index buffer for sphere mesh */
    Uint32         sphere_index_count; /* number of indices in sphere mesh */

    /* Cylinder GPU geometry (for constraint connections) */
    SDL_GPUBuffer *cylinder_vb;          /* vertex buffer for cylinder mesh */
    SDL_GPUBuffer *cylinder_ib;          /* index buffer for cylinder mesh */
    Uint32         cylinder_index_count; /* number of indices in cylinder mesh */

    /* Physics state */
    ForgePhysicsParticle           particles[MAX_PARTICLES];       /* current simulation state */
    ForgePhysicsParticle           initial_particles[MAX_PARTICLES]; /* snapshot for reset */
    ForgePhysicsDistanceConstraint constraints[MAX_CONSTRAINTS];   /* Newton's cradle ropes */
    ForgePhysicsContact            contacts[MAX_CONTACTS];         /* collision output buffer */

    int num_particles;    /* active particle count for current scene */
    int num_constraints;  /* active constraint count (0 except scene 3) */
    int active_contacts;  /* collisions detected in last physics step */

    /* Simulation control */
    int   scene_index;  /* selected scene, 0-3 */
    float accumulator;  /* leftover time for fixed-timestep loop [s] */
    float sim_time;     /* total elapsed simulation time [s] */
    bool  paused;       /* true when simulation is frozen */
    float speed_scale;  /* time multiplier: 1.0 normal, 0.25 slow motion */

    /* UI-adjustable parameters */
    float ui_restitution; /* bounce coefficient from slider [0..1] */
    float ui_gravity;     /* gravitational acceleration from slider [m/s²] */

    /* UI window state */
    ForgeUiWindowState ui_window;
} app_state;

/* ── Seeded LCG random number generator ──────────────────────────── */

static Uint32 lcg_state = 0;

static void lcg_seed(Uint32 seed)
{
    lcg_state = seed;
}

static float lcg_float(float lo, float hi)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    float t = (float)(lcg_state >> 8) / 16777216.0f;
    return lo + t * (hi - lo);
}

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

/* ── Helper: build cylinder model matrix between two points ──────── */

static mat4 cylinder_between(vec3 a, vec3 b)
{
    vec3 mid = vec3_scale(vec3_add(a, b), 0.5f);
    vec3 diff = vec3_sub(b, a);
    float dist = vec3_length(diff);

    mat4 scale = mat4_scale(vec3_create(CONN_RADIUS, dist * 0.5f, CONN_RADIUS));

    mat4 rot = mat4_identity();
    if (dist > FORGE_PHYSICS_EPSILON) {
        vec3 dir = vec3_scale(diff, 1.0f / dist);
        vec3 up = vec3_create(0.0f, 1.0f, 0.0f);

        float dot = vec3_dot(dir, up);
        if (fabsf(dot) > NEAR_PARALLEL_DOT) {
            if (dot < 0.0f) {
                rot = mat4_rotate_z(FORGE_PI);
            }
        } else {
            vec3 axis = vec3_normalize(vec3_cross(up, dir));
            float angle = acosf(dot);
            quat q = quat_from_axis_angle(axis, angle);
            rot = quat_to_mat4(q);
        }
    }

    mat4 translate = mat4_translate(mid);
    return mat4_multiply(translate, mat4_multiply(rot, scale));
}

/* ── Scene 1: Two-Body Collision ─────────────────────────────────── */

static void init_scene_1(app_state *state)
{
    state->num_particles   = S1_NUM_PARTICLES;
    state->num_constraints = 0;

    /* Particle 0: moving right */
    state->particles[0] = forge_physics_particle_create(
        vec3_create(-S1_START_X, S1_START_Y, 0.0f),
        S1_PARTICLE_MASS, PARTICLE_DAMPING, state->ui_restitution,
        S1_PARTICLE_RADIUS);
    state->particles[0].velocity = vec3_create(S1_INITIAL_SPEED, 0.0f, 0.0f);

    /* Particle 1: moving left */
    state->particles[1] = forge_physics_particle_create(
        vec3_create(S1_START_X, S1_START_Y, 0.0f),
        S1_PARTICLE_MASS, PARTICLE_DAMPING, state->ui_restitution,
        S1_PARTICLE_RADIUS);
    state->particles[1].velocity = vec3_create(-S1_INITIAL_SPEED, 0.0f, 0.0f);

    for (int i = 0; i < state->num_particles; i++) {
        state->initial_particles[i] = state->particles[i];
    }
}

/* ── Scene 2: Particle Rain ──────────────────────────────────────── */

static void init_scene_2(app_state *state)
{
    state->num_particles   = S2_NUM_PARTICLES;
    state->num_constraints = 0;

    lcg_seed(S2_LCG_SEED);

    for (int i = 0; i < S2_NUM_PARTICLES; i++) {
        float x = lcg_float(S2_X_MIN, S2_X_MAX);
        float y = lcg_float(S2_Y_MIN, S2_Y_MAX);
        float z = lcg_float(S2_Z_MIN, S2_Z_MAX);

        state->particles[i] = forge_physics_particle_create(
            vec3_create(x, y, z),
            S2_PARTICLE_MASS, PARTICLE_DAMPING, state->ui_restitution,
            S2_PARTICLE_RADIUS);
    }

    for (int i = 0; i < state->num_particles; i++) {
        state->initial_particles[i] = state->particles[i];
    }
}

/* ── Scene 3: Newton's Cradle ────────────────────────────────────── */

static void init_scene_3(app_state *state)
{
    /* 5 anchors (static) + 5 balls (dynamic) = 10 particles total */
    int total = S3_NUM_BALLS * 2;
    state->num_particles   = total;
    state->num_constraints = S3_NUM_BALLS;

    /* Center the cradle horizontally */
    float half_width = (float)(S3_NUM_BALLS - 1) * S3_BALL_SPACING * 0.5f;

    for (int i = 0; i < S3_NUM_BALLS; i++) {
        float x = (float)i * S3_BALL_SPACING - half_width;

        /* Anchor particle (static, small radius) */
        state->particles[i] = forge_physics_particle_create(
            vec3_create(x, S3_ANCHOR_Y, 0.0f),
            0.0f, 0.0f, 0.0f, S3_ANCHOR_RADIUS);

        /* Ball particle (dynamic) */
        int ball_idx = S3_NUM_BALLS + i;
        state->particles[ball_idx] = forge_physics_particle_create(
            vec3_create(x, S3_BALL_Y, 0.0f),
            S3_BALL_MASS, PARTICLE_DAMPING, S3_BALL_RESTITUTION,
            S3_BALL_RADIUS);

        /* Distance constraint: anchor to ball */
        float rope_length = S3_ANCHOR_Y - S3_BALL_Y;
        state->constraints[i] = forge_physics_constraint_distance_create(
            i, ball_idx, rope_length, S3_CONSTRAINT_STIFF);
    }

    /* Offset the leftmost ball to start the swing */
    int first_ball = S3_NUM_BALLS;
    state->particles[first_ball].position.x -= S3_SWING_OFFSET;
    state->particles[first_ball].velocity =
        vec3_create(S3_SWING_SPEED, 0.0f, 0.0f);

    for (int i = 0; i < state->num_particles; i++) {
        state->initial_particles[i] = state->particles[i];
    }
}

/* ── Scene 4: Billiard Break ─────────────────────────────────────── */

static void init_scene_4(app_state *state)
{
    state->num_constraints = 0;

    /* Build triangle formation: rows of 1, 2, 3, 4, 5 balls */
    int idx = 0;
    for (int row = 0; row < S4_TRIANGLE_ROWS; row++) {
        int balls_in_row = row + 1;
        float row_width = (float)(balls_in_row - 1) * S4_BALL_SPACING;
        float start_x = -row_width * 0.5f;
        float z = S4_TRIANGLE_Z - (float)row * S4_BALL_SPACING * TRIANGLE_ROW_FACTOR;

        for (int col = 0; col < balls_in_row && idx < MAX_PARTICLES; col++) {
            float x = start_x + (float)col * S4_BALL_SPACING;
            state->particles[idx] = forge_physics_particle_create(
                vec3_create(x, S4_BALL_Y, z),
                S4_BALL_MASS, PARTICLE_DAMPING, state->ui_restitution,
                S4_BALL_RADIUS);
            idx++;
        }
    }

    /* Cue ball — guard against MAX_PARTICLES if triangle rows were increased */
    if (idx >= MAX_PARTICLES) {
        state->num_particles = idx;
        return;
    }
    state->particles[idx] = forge_physics_particle_create(
        vec3_create(0.0f, S4_BALL_Y, S4_CUE_Z),
        S4_BALL_MASS, PARTICLE_DAMPING, state->ui_restitution,
        S4_BALL_RADIUS);
    state->particles[idx].velocity =
        vec3_create(0.0f, 0.0f, -S4_CUE_SPEED);
    idx++;

    state->num_particles = idx;

    for (int i = 0; i < state->num_particles; i++) {
        state->initial_particles[i] = state->particles[i];
    }
}

/* ── Scene initialization dispatch ───────────────────────────────── */

static void init_current_scene(app_state *state)
{
    state->num_particles   = 0;
    state->num_constraints = 0;
    state->active_contacts = 0;

    switch (state->scene_index) {
    case 0: init_scene_1(state); break;
    case 1: init_scene_2(state); break;
    case 2: init_scene_3(state); break;
    case 3: init_scene_4(state); break;
    default: init_scene_1(state); break;
    }

    state->accumulator = 0.0f;
    state->sim_time    = 0.0f;
}

/* ── Reset simulation ────────────────────────────────────────────── */

static void reset_simulation(app_state *state)
{
    for (int i = 0; i < state->num_particles; i++) {
        state->particles[i] = state->initial_particles[i];
    }
    state->active_contacts = 0;
    state->accumulator     = 0.0f;
    state->sim_time        = 0.0f;
}

/* ── Physics step ────────────────────────────────────────────────── */

static void physics_step(app_state *state)
{
    vec3 gravity = vec3_create(0.0f, -state->ui_gravity, 0.0f);

    /* Scene 4 overrides the UI gravity slider with low gravity and high
     * drag to simulate a flat billiard table.  The slider still displays
     * but has no effect while this scene is active. */
    float drag = DRAG_COEFF;
    if (state->scene_index == 3) {
        gravity = vec3_create(0.0f, -S4_BILLIARD_GRAVITY, 0.0f);
        drag = S4_BILLIARD_DRAG;
    }

    /* Apply gravity and drag to all dynamic particles */
    for (int i = 0; i < state->num_particles; i++) {
        forge_physics_apply_gravity(&state->particles[i], gravity);
        forge_physics_apply_drag(&state->particles[i], drag);
    }

    /* Integrate all particles */
    for (int i = 0; i < state->num_particles; i++) {
        forge_physics_integrate(&state->particles[i], PHYSICS_DT);
    }

    /* Solve distance constraints (Newton's cradle) */
    if (state->num_constraints > 0) {
        forge_physics_constraints_solve(
            state->constraints, state->num_constraints,
            state->particles, state->num_particles,
            S3_SOLVER_ITERS);
    }

    /* Detect and resolve all sphere-sphere collisions */
    state->active_contacts = forge_physics_collide_particles_step(
        state->particles, state->num_particles,
        state->contacts, MAX_CONTACTS);

    /* Ground plane collision for all particles */
    vec3 ground_normal = vec3_create(0.0f, 1.0f, 0.0f);
    for (int i = 0; i < state->num_particles; i++) {
        forge_physics_collide_plane(
            &state->particles[i], ground_normal, GROUND_Y);
    }

    /* Scene 4: clamp y to radius for billiard-table effect */
    if (state->scene_index == 3) {
        for (int i = 0; i < state->num_particles; i++) {
            if (state->particles[i].inv_mass > 0.0f &&
                state->particles[i].position.y < state->particles[i].radius) {
                state->particles[i].position.y = state->particles[i].radius;
                if (state->particles[i].velocity.y < 0.0f) {
                    state->particles[i].velocity.y = 0.0f;
                }
            }
        }
    }
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

    /* Configure the scene renderer */
    ForgeSceneConfig cfg = forge_scene_default_config(
        "Physics Lesson 03 \xe2\x80\x94 Particle Collisions");
    cfg.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";
    cfg.font_size       = 16.0f;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("ERROR: forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* Generate and upload sphere geometry */
    ForgeShape sphere = forge_shapes_sphere(SPHERE_SLICES, SPHERE_STACKS);
    if (sphere.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_sphere failed");
        goto init_fail;
    }
    state->sphere_vb = upload_shape_vb(&state->scene, &sphere);
    state->sphere_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, sphere.indices,
        (Uint32)sphere.index_count * (Uint32)sizeof(uint32_t));
    state->sphere_index_count = (Uint32)sphere.index_count;
    forge_shapes_free(&sphere);

    if (!state->sphere_vb || !state->sphere_ib) {
        SDL_Log("ERROR: Failed to upload sphere geometry");
        goto init_fail;
    }

    /* Generate and upload cylinder geometry */
    ForgeShape cylinder = forge_shapes_cylinder(CYLINDER_SLICES, CYLINDER_STACKS);
    if (cylinder.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_cylinder failed");
        goto init_fail;
    }
    state->cylinder_vb = upload_shape_vb(&state->scene, &cylinder);
    state->cylinder_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, cylinder.indices,
        (Uint32)cylinder.index_count * (Uint32)sizeof(uint32_t));
    state->cylinder_index_count = (Uint32)cylinder.index_count;
    forge_shapes_free(&cylinder);

    if (!state->cylinder_vb || !state->cylinder_ib) {
        SDL_Log("ERROR: Failed to upload cylinder geometry");
        goto init_fail;
    }

    /* Simulation state */
    state->scene_index     = 0;
    state->accumulator     = 0.0f;
    state->sim_time        = 0.0f;
    state->paused          = false;
    state->speed_scale     = 1.0f;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
    state->active_contacts = 0;

    /* Default UI parameters */
    state->ui_restitution = 0.8f;
    state->ui_gravity     = 9.81f;

    /* Initialize first scene */
    init_current_scene(state);

    return SDL_APP_CONTINUE;

init_fail:
    /* Release any GPU buffers allocated before the failure */
    if (forge_scene_device(&state->scene)) {
        if (state->sphere_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_vb);
        if (state->sphere_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_ib);
        if (state->cylinder_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->cylinder_vb);
        if (state->cylinder_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->cylinder_ib);
    }
    forge_scene_destroy(&state->scene);
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    /* Let scene handle camera, mouse capture, quit, escape */
    SDL_AppResult result = forge_scene_handle_event(&state->scene, event);
    if (result != SDL_APP_CONTINUE) return result;

    /* Physics-specific keys */
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
                (state->speed_scale < 0.5f) ? 1.0f : 0.25f;
            SDL_Log("Speed: %.2fx", (double)state->speed_scale);
            break;
        case SDL_SCANCODE_1:
            state->scene_index = 0;
            init_current_scene(state);
            SDL_Log("Scene 1: Two-Body Collision");
            break;
        case SDL_SCANCODE_2:
            state->scene_index = 1;
            init_current_scene(state);
            SDL_Log("Scene 2: Particle Rain");
            break;
        case SDL_SCANCODE_3:
            state->scene_index = 2;
            init_current_scene(state);
            SDL_Log("Scene 3: Newton's Cradle");
            break;
        case SDL_SCANCODE_4:
            state->scene_index = 3;
            init_current_scene(state);
            SDL_Log("Scene 4: Billiard Break");
            break;
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

        /* Update restitution on all dynamic particles from UI slider */
        for (int i = 0; i < state->num_particles; i++) {
            if (state->particles[i].inv_mass > 0.0f) {
                /* Newton's cradle overrides restitution to 1.0 */
                if (state->scene_index == 2) {
                    state->particles[i].restitution = S3_BALL_RESTITUTION;
                } else {
                    state->particles[i].restitution = state->ui_restitution;
                }
            }
        }

        while (state->accumulator >= PHYSICS_DT) {
            physics_step(state);
            state->accumulator -= PHYSICS_DT;
            state->sim_time    += PHYSICS_DT;
        }
    }

    /* Interpolation factor for smooth rendering */
    float alpha = state->accumulator / PHYSICS_DT;
    if (alpha > 1.0f) alpha = 1.0f;

    /* ── Determine particle color for this scene ─────────────────── */
    float dynamic_color[4];
    switch (state->scene_index) {
    case 0:
        dynamic_color[0] = COLOR_S1_R; dynamic_color[1] = COLOR_S1_G;
        dynamic_color[2] = COLOR_S1_B; dynamic_color[3] = COLOR_S1_A;
        break;
    case 1:
        dynamic_color[0] = COLOR_S2_R; dynamic_color[1] = COLOR_S2_G;
        dynamic_color[2] = COLOR_S2_B; dynamic_color[3] = COLOR_S2_A;
        break;
    case 2:
        dynamic_color[0] = COLOR_S3_R; dynamic_color[1] = COLOR_S3_G;
        dynamic_color[2] = COLOR_S3_B; dynamic_color[3] = COLOR_S3_A;
        break;
    case 3:
        dynamic_color[0] = COLOR_S4_R; dynamic_color[1] = COLOR_S4_G;
        dynamic_color[2] = COLOR_S4_B; dynamic_color[3] = COLOR_S4_A;
        break;
    default:
        dynamic_color[0] = 1.0f; dynamic_color[1] = 1.0f;
        dynamic_color[2] = 1.0f; dynamic_color[3] = 1.0f;
        break;
    }

    float fixed_color[4] = {
        COLOR_FIXED_R, COLOR_FIXED_G, COLOR_FIXED_B, COLOR_FIXED_A
    };
    float conn_color[4] = {
        COLOR_CONN_R, COLOR_CONN_G, COLOR_CONN_B, COLOR_CONN_A
    };

    /* Precompute interpolated positions */
    vec3 render_pos[MAX_PARTICLES];
    for (int i = 0; i < state->num_particles; i++) {
        render_pos[i] = vec3_lerp(
            state->particles[i].prev_position,
            state->particles[i].position, alpha);
    }

    /* ── Shadow pass ─────────────────────────────────────────────── */

    forge_scene_begin_shadow_pass(s);

    for (int i = 0; i < state->num_particles; i++) {
        float r = state->particles[i].radius;
        mat4 model = mat4_multiply(
            mat4_translate(render_pos[i]),
            mat4_scale_uniform(r));
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count, model);
    }

    /* Draw constraint connections as cylinders (Newton's cradle ropes) */
    for (int i = 0; i < state->num_constraints; i++) {
        int a = state->constraints[i].a;
        int b = state->constraints[i].b;
        mat4 model = cylinder_between(render_pos[a], render_pos[b]);
        forge_scene_draw_shadow_mesh(s, state->cylinder_vb, state->cylinder_ib,
                                     state->cylinder_index_count, model);
    }

    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */

    forge_scene_begin_main_pass(s);

    for (int i = 0; i < state->num_particles; i++) {
        float r = state->particles[i].radius;
        mat4 model = mat4_multiply(
            mat4_translate(render_pos[i]),
            mat4_scale_uniform(r));
        bool is_fixed = (state->particles[i].inv_mass == 0.0f);
        forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                              state->sphere_index_count, model,
                              is_fixed ? fixed_color : dynamic_color);
    }

    /* Draw constraint connections */
    for (int i = 0; i < state->num_constraints; i++) {
        int a = state->constraints[i].a;
        int b = state->constraints[i].b;
        mat4 model = cylinder_between(render_pos[a], render_pos[b]);
        forge_scene_draw_mesh(s, state->cylinder_vb, state->cylinder_ib,
                              state->cylinder_index_count, model, conn_color);
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
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "Particle Collisions",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* Scene selection label */
                {
                    const char *scene_names[NUM_SCENES] = {
                        "1: Two-Body",
                        "2: Particle Rain",
                        "3: Newton's Cradle",
                        "4: Billiard Break"
                    };
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Scene: %s",
                                 scene_names[state->scene_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* Scene selection buttons */
                {
                    if (forge_ui_ctx_button_layout(ui, "1: Two-Body",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 0;
                        init_current_scene(state);
                    }
                    if (forge_ui_ctx_button_layout(ui, "2: Rain",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 1;
                        init_current_scene(state);
                    }
                    if (forge_ui_ctx_button_layout(ui, "3: Cradle",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 2;
                        init_current_scene(state);
                    }
                    if (forge_ui_ctx_button_layout(ui, "4: Billiards",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 3;
                        init_current_scene(state);
                    }
                }

                /* Separator */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Restitution slider */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Restitution: %.2f",
                                 (double)state->ui_restitution);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##restitution",
                                           &state->ui_restitution,
                                           RESTITUTION_MIN, RESTITUTION_MAX,
                                           SLIDER_HEIGHT);

                /* Gravity slider */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Gravity: %.1f",
                                 (double)state->ui_gravity);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##gravity",
                                           &state->ui_gravity,
                                           GRAVITY_MIN, GRAVITY_MAX,
                                           SLIDER_HEIGHT);

                /* Separator */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Simulation info */
                {
                    char buf[64];

                    SDL_snprintf(buf, sizeof(buf), "Particles: %d",
                                 state->num_particles);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Active Collisions: %d",
                                 state->active_contacts);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* Kinetic energy and total momentum */
                {
                    float kinetic = 0.0f;
                    vec3 momentum = vec3_create(0.0f, 0.0f, 0.0f);
                    for (int i = 0; i < state->num_particles; i++) {
                        float speed_sq = vec3_length_squared(
                            state->particles[i].velocity);
                        kinetic += 0.5f * state->particles[i].mass * speed_sq;
                        momentum = vec3_add(momentum,
                            vec3_scale(state->particles[i].velocity,
                                       state->particles[i].mass));
                    }

                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Kinetic Energy: %.1f",
                                 (double)kinetic);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Momentum: %.2f",
                                 (double)vec3_length(momentum));
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

                /* Separator */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Pause checkbox */
                forge_ui_ctx_checkbox_layout(ui, "Paused",
                                             &state->paused, LABEL_HEIGHT);

                /* Slow motion checkbox */
                {
                    bool slow = (state->speed_scale < 0.5f);
                    if (forge_ui_ctx_checkbox_layout(ui, "Slow Motion",
                                                      &slow, LABEL_HEIGHT)) {
                        state->speed_scale = slow ? 0.25f : 1.0f;
                    }
                }

                /* Reset button */
                if (forge_ui_ctx_button_layout(ui, "Reset", BUTTON_HEIGHT)) {
                    reset_simulation(state);
                }

                forge_ui_wctx_window_end(wctx);
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

    /* Release lesson-specific GPU resources before destroying the scene */
    if (forge_scene_device(&state->scene)) {
        if (!SDL_WaitForGPUIdle(forge_scene_device(&state->scene))) {
            SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }
        if (state->sphere_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_vb);
        if (state->sphere_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_ib);
        if (state->cylinder_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->cylinder_vb);
        if (state->cylinder_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->cylinder_ib);
    }

    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
