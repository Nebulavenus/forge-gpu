/*
 * Physics Lesson 02 — Springs and Constraints
 *
 * Demonstrates: Hooke's law springs, velocity-damped springs, distance
 * constraints (position-based dynamics), spring chains, constraint chains,
 * and a 10x10 cloth grid with structural + shear springs.
 *
 * Four selectable scenes:
 *   1. Single spring — fixed anchor + 1 dynamic particle oscillating
 *   2. Spring chain — 8 particles hanging from a fixed top point
 *   3. Constraint chain — same layout using distance constraints (rigid)
 *   4. Cloth grid — 10x10 particle grid with structural + shear springs
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

/* Physics library — particles, springs, constraints */
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
#define MAX_PARTICLES        120   /* enough for 10x10 cloth + margin */
#define MAX_SPRINGS          350   /* structural + shear for 10x10 cloth: 90+90+162=342 */
#define MAX_CONSTRAINTS      300

/* Scene 1: Single spring */
#define S1_ANCHOR_Y          5.0f
#define S1_PARTICLE_Y        2.0f
#define S1_REST_LENGTH       2.0f
#define S1_STIFFNESS        50.0f
#define S1_SPRING_DAMPING    2.0f

/* Scene 2: Spring chain */
#define S2_NUM_PARTICLES     8
#define S2_ANCHOR_Y          8.0f
#define S2_LINK_LENGTH       0.8f
#define S2_STIFFNESS       100.0f
#define S2_SPRING_DAMPING    3.0f

/* Scene 3: Constraint chain */
#define S3_NUM_PARTICLES     8
#define S3_ANCHOR_Y          8.0f
#define S3_LINK_LENGTH       0.8f
#define S3_CONSTRAINT_STIFF  1.0f
#define S3_SOLVER_ITERS     10

/* Scene 4: Cloth grid */
#define CLOTH_COLS          10
#define CLOTH_ROWS          10
#define CLOTH_SPACING        0.6f
#define CLOTH_STIFFNESS    200.0f
#define CLOTH_SPRING_DAMP    4.0f
#define CLOTH_ANCHOR_Y       8.0f

/* Particle physical properties */
#define PARTICLE_MASS        1.0f
#define PARTICLE_DAMPING     0.01f
#define PARTICLE_RESTITUTION 0.2f
#define PARTICLE_RADIUS      0.15f
#define GROUND_Y             0.0f
#define GRAVITY_DEFAULT     -9.81f
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
#define SQRT2                1.41421356f  /* sqrt(2) for diagonal distances */
#define NEAR_PARALLEL_DOT    0.999f       /* dot threshold for near-parallel vectors */

/* Shear spring scaling — diagonals use half the structural stiffness/damping */
#define SHEAR_SPRING_FACTOR  0.5f
#define SHEAR_DETECT_TOL     0.01f        /* tolerance for detecting shear rest length */

/* UI panel layout */
#define PANEL_X             10.0f
#define PANEL_Y             10.0f
#define PANEL_W            260.0f
#define PANEL_H            460.0f
#define LABEL_HEIGHT        24.0f
#define BUTTON_HEIGHT       30.0f
#define SLIDER_HEIGHT       28.0f

/* Slider ranges */
#define STIFFNESS_MIN        1.0f
#define STIFFNESS_MAX      500.0f
#define DAMPING_MIN          0.0f
#define DAMPING_MAX         10.0f
#define CONSTRAINT_ITER_MIN  1.0f
#define CONSTRAINT_ITER_MAX 20.0f
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

    /* Cylinder GPU geometry (for spring/constraint connections) */
    SDL_GPUBuffer *cylinder_vb;          /* vertex buffer for cylinder mesh */
    SDL_GPUBuffer *cylinder_ib;          /* index buffer for cylinder mesh */
    Uint32         cylinder_index_count; /* number of indices in cylinder mesh */

    /* Physics state */
    ForgePhysicsParticle           particles[MAX_PARTICLES];       /* current simulation state */
    ForgePhysicsParticle           initial_particles[MAX_PARTICLES]; /* snapshot for reset */
    ForgePhysicsSpring             springs[MAX_SPRINGS];           /* active spring connections */
    ForgePhysicsDistanceConstraint constraints[MAX_CONSTRAINTS];   /* active distance constraints */

    int num_particles;   /* number of active particles in current scene */
    int num_springs;     /* number of active springs in current scene */
    int num_constraints; /* number of active distance constraints in current scene */

    /* Simulation control */
    int   scene_index;   /* current scene selection, 0..3 for scenes 1..4 */
    float accumulator;   /* leftover time for fixed-step physics (s) */
    float sim_time;      /* total simulated time (s) */
    bool  paused;        /* true = physics frozen, camera still works */
    float speed_scale;   /* simulation speed multiplier (0.25 or 1.0) */

    /* UI-adjustable parameters */
    float ui_stiffness;        /* spring constant k (N/m), from slider */
    float ui_damping;          /* damping coefficient b (Ns/m), from slider */
    float ui_constraint_iters; /* Gauss-Seidel iterations, from slider */
    float ui_gravity;          /* gravity magnitude (m/s^2, positive), from slider */

    /* UI scroll */
    float panel_scroll;  /* vertical scroll offset for UI panel (px) */
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

/* ── Scene 1: Single spring ──────────────────────────────────────── */

static void init_scene_1(app_state *state)
{
    state->num_particles  = 2;
    state->num_springs    = 1;
    state->num_constraints = 0;

    /* Particle 0: fixed anchor at top */
    state->particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, S1_ANCHOR_Y, 0.0f),
        0.0f, 0.0f, 0.0f, PARTICLE_RADIUS);

    /* Particle 1: dynamic, hanging below anchor */
    state->particles[1] = forge_physics_particle_create(
        vec3_create(0.0f, S1_PARTICLE_Y, 0.0f),
        PARTICLE_MASS, PARTICLE_DAMPING, PARTICLE_RESTITUTION,
        PARTICLE_RADIUS);

    /* Spring connecting anchor to dynamic particle */
    state->springs[0] = forge_physics_spring_create(
        0, 1, S1_REST_LENGTH, state->ui_stiffness, state->ui_damping);

    /* Save initial state for reset */
    for (int i = 0; i < state->num_particles; i++) {
        state->initial_particles[i] = state->particles[i];
    }
}

/* ── Scene 2: Spring chain ───────────────────────────────────────── */

static void init_scene_2(app_state *state)
{
    state->num_particles   = S2_NUM_PARTICLES;
    state->num_springs     = S2_NUM_PARTICLES - 1;
    state->num_constraints = 0;

    for (int i = 0; i < S2_NUM_PARTICLES; i++) {
        float y = S2_ANCHOR_Y - (float)i * S2_LINK_LENGTH;
        float mass = (i == 0) ? 0.0f : PARTICLE_MASS;

        state->particles[i] = forge_physics_particle_create(
            vec3_create(0.0f, y, 0.0f),
            mass, PARTICLE_DAMPING, PARTICLE_RESTITUTION,
            PARTICLE_RADIUS);
    }

    /* Springs connecting consecutive particles */
    for (int i = 0; i < S2_NUM_PARTICLES - 1; i++) {
        state->springs[i] = forge_physics_spring_create(
            i, i + 1, S2_LINK_LENGTH,
            state->ui_stiffness, state->ui_damping);
    }

    for (int i = 0; i < state->num_particles; i++) {
        state->initial_particles[i] = state->particles[i];
    }
}

/* ── Scene 3: Constraint chain ───────────────────────────────────── */

static void init_scene_3(app_state *state)
{
    state->num_particles   = S3_NUM_PARTICLES;
    state->num_springs     = 0;
    state->num_constraints = S3_NUM_PARTICLES - 1;

    for (int i = 0; i < S3_NUM_PARTICLES; i++) {
        float y = S3_ANCHOR_Y - (float)i * S3_LINK_LENGTH;
        float mass = (i == 0) ? 0.0f : PARTICLE_MASS;

        state->particles[i] = forge_physics_particle_create(
            vec3_create(0.0f, y, 0.0f),
            mass, PARTICLE_DAMPING, PARTICLE_RESTITUTION,
            PARTICLE_RADIUS);
    }

    /* Distance constraints connecting consecutive particles */
    for (int i = 0; i < S3_NUM_PARTICLES - 1; i++) {
        state->constraints[i] = forge_physics_constraint_distance_create(
            i, i + 1, S3_LINK_LENGTH, S3_CONSTRAINT_STIFF);
    }

    for (int i = 0; i < state->num_particles; i++) {
        state->initial_particles[i] = state->particles[i];
    }
}

/* ── Scene 4: Cloth grid ─────────────────────────────────────────── */

static void init_scene_4(app_state *state)
{
    int total = CLOTH_ROWS * CLOTH_COLS;
    state->num_particles   = total;
    state->num_springs     = 0;
    state->num_constraints = 0;

    /* Create particles in a grid — top row (row 0) is fixed */
    float half_w = (float)(CLOTH_COLS - 1) * CLOTH_SPACING * 0.5f;
    for (int row = 0; row < CLOTH_ROWS; row++) {
        for (int col = 0; col < CLOTH_COLS; col++) {
            int idx = row * CLOTH_COLS + col;
            float x = (float)col * CLOTH_SPACING - half_w;
            float y = CLOTH_ANCHOR_Y - (float)row * CLOTH_SPACING;
            float z = 0.0f;
            float mass = (row == 0) ? 0.0f : PARTICLE_MASS;

            state->particles[idx] = forge_physics_particle_create(
                vec3_create(x, y, z),
                mass, PARTICLE_DAMPING, PARTICLE_RESTITUTION,
                PARTICLE_RADIUS);
        }
    }

    /* Structural springs — horizontal connections */
    for (int row = 0; row < CLOTH_ROWS; row++) {
        for (int col = 0; col < CLOTH_COLS - 1; col++) {
            int a = row * CLOTH_COLS + col;
            int b = a + 1;
            state->springs[state->num_springs++] =
                forge_physics_spring_create(
                    a, b, CLOTH_SPACING,
                    state->ui_stiffness, state->ui_damping);
        }
    }

    /* Structural springs — vertical connections */
    for (int row = 0; row < CLOTH_ROWS - 1; row++) {
        for (int col = 0; col < CLOTH_COLS; col++) {
            int a = row * CLOTH_COLS + col;
            int b = (row + 1) * CLOTH_COLS + col;
            state->springs[state->num_springs++] =
                forge_physics_spring_create(
                    a, b, CLOTH_SPACING,
                    state->ui_stiffness, state->ui_damping);
        }
    }

    /* Shear springs — diagonal connections (both directions) */
    float diag_len = CLOTH_SPACING * SQRT2; /* sqrt(2) * spacing */
    for (int row = 0; row < CLOTH_ROWS - 1; row++) {
        for (int col = 0; col < CLOTH_COLS - 1; col++) {
            int tl = row * CLOTH_COLS + col;
            int tr = tl + 1;
            int bl = (row + 1) * CLOTH_COLS + col;
            int br = bl + 1;

            /* Top-left to bottom-right */
            state->springs[state->num_springs++] =
                forge_physics_spring_create(
                    tl, br, diag_len,
                    state->ui_stiffness * SHEAR_SPRING_FACTOR,
                    state->ui_damping * SHEAR_SPRING_FACTOR);

            /* Top-right to bottom-left */
            state->springs[state->num_springs++] =
                forge_physics_spring_create(
                    tr, bl, diag_len,
                    state->ui_stiffness * SHEAR_SPRING_FACTOR,
                    state->ui_damping * SHEAR_SPRING_FACTOR);
        }
    }

    for (int i = 0; i < state->num_particles; i++) {
        state->initial_particles[i] = state->particles[i];
    }
}

/* ── Scene initialization dispatch ───────────────────────────────── */

static void init_current_scene(app_state *state)
{
    /* Reset counts */
    state->num_particles   = 0;
    state->num_springs     = 0;
    state->num_constraints = 0;

    switch (state->scene_index) {
    case 0: init_scene_1(state); break;
    case 1: init_scene_2(state); break;
    case 2: init_scene_3(state); break;
    case 3: init_scene_4(state); break;
    default: init_scene_1(state); break;
    }

    /* Seed UI parameters from the active scene's defaults */
    switch (state->scene_index) {
    case 0:
        state->ui_stiffness = S1_STIFFNESS;
        state->ui_damping   = S1_SPRING_DAMPING;
        break;
    case 1:
        state->ui_stiffness = S2_STIFFNESS;
        state->ui_damping   = S2_SPRING_DAMPING;
        break;
    case 3:
        state->ui_stiffness = CLOTH_STIFFNESS;
        state->ui_damping   = CLOTH_SPRING_DAMP;
        break;
    default:
        break;
    }
    state->ui_constraint_iters = (float)S3_SOLVER_ITERS;
    state->ui_gravity          = -GRAVITY_DEFAULT;

    state->accumulator = 0.0f;
    state->sim_time    = 0.0f;
}

/* ── Reset simulation ────────────────────────────────────────────── */

static void reset_simulation(app_state *state)
{
    for (int i = 0; i < state->num_particles; i++) {
        state->particles[i] = state->initial_particles[i];
    }
    state->accumulator = 0.0f;
    state->sim_time    = 0.0f;
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
        "Physics Lesson 02 \xe2\x80\x94 Springs and Constraints");
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

    /* Generate and upload cylinder geometry */
    ForgeShape cylinder = forge_shapes_cylinder(CYLINDER_SLICES, CYLINDER_STACKS);
    if (cylinder.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_cylinder failed");
        return SDL_APP_FAILURE;
    }
    state->cylinder_vb = upload_shape_vb(&state->scene, &cylinder);
    state->cylinder_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, cylinder.indices,
        (Uint32)cylinder.index_count * (Uint32)sizeof(uint32_t));
    state->cylinder_index_count = (Uint32)cylinder.index_count;
    forge_shapes_free(&cylinder);

    if (!state->cylinder_vb || !state->cylinder_ib) {
        SDL_Log("ERROR: Failed to upload cylinder geometry");
        return SDL_APP_FAILURE;
    }

    /* Simulation state */
    state->scene_index  = 0;
    state->accumulator  = 0.0f;
    state->sim_time     = 0.0f;
    state->paused       = false;
    state->speed_scale  = 1.0f;
    state->panel_scroll = 0.0f;

    /* Initialize first scene */
    init_current_scene(state);

    return SDL_APP_CONTINUE;
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
            SDL_Log("Scene 1: Single Spring");
            break;
        case SDL_SCANCODE_2:
            state->scene_index = 1;
            init_current_scene(state);
            SDL_Log("Scene 2: Spring Chain");
            break;
        case SDL_SCANCODE_3:
            state->scene_index = 2;
            init_current_scene(state);
            SDL_Log("Scene 3: Constraint Chain");
            break;
        case SDL_SCANCODE_4:
            state->scene_index = 3;
            init_current_scene(state);
            SDL_Log("Scene 4: Cloth Grid");
            break;
        default:
            break;
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── Helper: build cylinder model matrix between two points ──────── */

/* Compute a model matrix that places a unit cylinder (radius 1, height
 * 2, Y-axis aligned from -1 to +1) between two world-space positions.
 * The cylinder is scaled thin (CONN_RADIUS) and stretched to the
 * distance between the points. */
static mat4 cylinder_between(vec3 a, vec3 b)
{
    vec3 mid = vec3_scale(vec3_add(a, b), 0.5f);
    vec3 diff = vec3_sub(b, a);
    float dist = vec3_length(diff);

    /* Scale: thin radius, half the distance (cylinder is height 2) */
    mat4 scale = mat4_scale(vec3_create(CONN_RADIUS, dist * 0.5f, CONN_RADIUS));

    /* Rotation: align Y-axis to the direction between particles */
    mat4 rot = mat4_identity();
    if (dist > FORGE_PHYSICS_EPSILON) {
        vec3 dir = vec3_scale(diff, 1.0f / dist);
        vec3 up = vec3_create(0.0f, 1.0f, 0.0f);

        /* If direction is nearly parallel to up, use a different axis */
        float dot = vec3_dot(dir, up);
        if (fabsf(dot) > NEAR_PARALLEL_DOT) {
            /* Direction is nearly vertical — use identity rotation
             * (cylinder is already Y-aligned) or flip if pointing down */
            if (dot < 0.0f) {
                rot = mat4_rotate_z(FORGE_PI);
            }
        } else {
            /* Cross product gives rotation axis, convert via quaternion */
            vec3 axis = vec3_normalize(vec3_cross(up, dir));
            float angle = acosf(dot);
            quat q = quat_from_axis_angle(axis, angle);
            rot = quat_to_mat4(q);
        }
    }

    mat4 translate = mat4_translate(mid);
    return mat4_multiply(translate, mat4_multiply(rot, scale));
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

        /* Update spring/constraint parameters from UI sliders */
        for (int i = 0; i < state->num_springs; i++) {
            /* For cloth shear springs, use half stiffness/damping */
            if (state->scene_index == 3) {
                /* Detect shear springs: they have diagonal rest length */
                float diag = CLOTH_SPACING * SQRT2;
                float diff = fabsf(state->springs[i].rest_length - diag);
                if (diff < SHEAR_DETECT_TOL) {
                    state->springs[i].stiffness = state->ui_stiffness * SHEAR_SPRING_FACTOR;
                    state->springs[i].damping   = state->ui_damping * SHEAR_SPRING_FACTOR;
                } else {
                    state->springs[i].stiffness = state->ui_stiffness;
                    state->springs[i].damping   = state->ui_damping;
                }
            } else {
                state->springs[i].stiffness = state->ui_stiffness;
                state->springs[i].damping   = state->ui_damping;
            }
        }

        while (state->accumulator >= PHYSICS_DT) {
            vec3 gravity = vec3_create(0.0f, -state->ui_gravity, 0.0f);

            /* Apply gravity and drag to all dynamic particles */
            for (int i = 0; i < state->num_particles; i++) {
                forge_physics_apply_gravity(&state->particles[i], gravity);
                forge_physics_apply_drag(&state->particles[i], DRAG_COEFF);
            }

            /* Apply spring forces */
            for (int i = 0; i < state->num_springs; i++) {
                forge_physics_spring_apply(
                    &state->springs[i],
                    state->particles, state->num_particles);
            }

            /* Integrate all particles */
            for (int i = 0; i < state->num_particles; i++) {
                forge_physics_integrate(&state->particles[i], PHYSICS_DT);
            }

            /* Solve distance constraints */
            if (state->num_constraints > 0) {
                forge_physics_constraints_solve(
                    state->constraints, state->num_constraints,
                    state->particles, state->num_particles,
                    (int)state->ui_constraint_iters);
            }

            /* Ground plane collision */
            vec3 ground_normal = vec3_create(0.0f, 1.0f, 0.0f);
            for (int i = 0; i < state->num_particles; i++) {
                forge_physics_collide_plane(
                    &state->particles[i], ground_normal, GROUND_Y);
            }

            state->accumulator -= PHYSICS_DT;
            state->sim_time    += PHYSICS_DT;
        }
    }

    /* Interpolation factor for smooth rendering (clamped to [0,1] to
     * prevent overshoot after pause/resume or large frame deltas). */
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

    /* Draw particles as spheres */
    for (int i = 0; i < state->num_particles; i++) {
        mat4 model = mat4_multiply(
            mat4_translate(render_pos[i]),
            mat4_scale_uniform(PARTICLE_RADIUS));
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count, model);
    }

    /* Draw spring connections as cylinders */
    for (int i = 0; i < state->num_springs; i++) {
        int a = state->springs[i].a;
        int b = state->springs[i].b;
        mat4 model = cylinder_between(render_pos[a], render_pos[b]);
        forge_scene_draw_shadow_mesh(s, state->cylinder_vb, state->cylinder_ib,
                                     state->cylinder_index_count, model);
    }

    /* Draw constraint connections as cylinders */
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

    /* Draw particles — fixed particles gray, dynamic particles colored */
    for (int i = 0; i < state->num_particles; i++) {
        mat4 model = mat4_multiply(
            mat4_translate(render_pos[i]),
            mat4_scale_uniform(PARTICLE_RADIUS));
        bool is_fixed = (state->particles[i].inv_mass == 0.0f);
        forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                              state->sphere_index_count, model,
                              is_fixed ? fixed_color : dynamic_color);
    }

    /* Draw spring connections */
    for (int i = 0; i < state->num_springs; i++) {
        int a = state->springs[i].a;
        int b = state->springs[i].b;
        mat4 model = cylinder_between(render_pos[a], render_pos[b]);
        forge_scene_draw_mesh(s, state->cylinder_vb, state->cylinder_ib,
                              state->cylinder_index_count, model, conn_color);
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
        ForgeUiContext *ui = forge_scene_ui(s);
        if (ui) {
            ForgeUiRect panel = { PANEL_X, PANEL_Y, PANEL_W, PANEL_H };
            if (forge_ui_ctx_panel_begin(ui, "Springs & Constraints",
                                         panel, &state->panel_scroll)) {

                /* Scene selection label */
                {
                    const char *scene_names[NUM_SCENES] = {
                        "1: Single Spring",
                        "2: Spring Chain",
                        "3: Constraint Chain",
                        "4: Cloth Grid"
                    };
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Scene: %s",
                                 scene_names[state->scene_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* Scene selection buttons */
                {
                    if (forge_ui_ctx_button_layout(ui, "1: Spring",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 0;
                        init_current_scene(state);
                    }
                    if (forge_ui_ctx_button_layout(ui, "2: Chain",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 1;
                        init_current_scene(state);
                    }
                    if (forge_ui_ctx_button_layout(ui, "3: Constraint",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 2;
                        init_current_scene(state);
                    }
                    if (forge_ui_ctx_button_layout(ui, "4: Cloth",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 3;
                        init_current_scene(state);
                    }
                }

                /* Separator */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Stiffness slider */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Stiffness: %.0f",
                                 (double)state->ui_stiffness);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##stiffness",
                                           &state->ui_stiffness,
                                           STIFFNESS_MIN, STIFFNESS_MAX,
                                           SLIDER_HEIGHT);

                /* Damping slider */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Damping: %.1f",
                                 (double)state->ui_damping);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##damping",
                                           &state->ui_damping,
                                           DAMPING_MIN, DAMPING_MAX,
                                           SLIDER_HEIGHT);

                /* Constraint iterations slider */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf),
                                 "Constraint Iters: %d",
                                 (int)state->ui_constraint_iters);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##iters",
                                           &state->ui_constraint_iters,
                                           CONSTRAINT_ITER_MIN,
                                           CONSTRAINT_ITER_MAX,
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

                    SDL_snprintf(buf, sizeof(buf), "Springs: %d",
                                 state->num_springs);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Constraints: %d",
                                 state->num_constraints);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* Energy display */
                {
                    float kinetic = 0.0f;
                    float gravitational = 0.0f;
                    float elastic = 0.0f;
                    for (int i = 0; i < state->num_particles; i++) {
                        float speed_sq = vec3_length_squared(
                            state->particles[i].velocity);
                        kinetic += 0.5f * state->particles[i].mass * speed_sq;
                        gravitational += state->particles[i].mass
                                       * state->ui_gravity
                                       * state->particles[i].position.y;
                    }
                    for (int i = 0; i < state->num_springs; i++) {
                        const ForgePhysicsSpring *sp = &state->springs[i];
                        vec3 delta = vec3_sub(
                            state->particles[sp->b].position,
                            state->particles[sp->a].position);
                        float ext = vec3_length(delta) - sp->rest_length;
                        elastic += 0.5f * sp->stiffness * ext * ext;
                    }

                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf),
                                 "KE: %.1f  GPE: %.1f  SPE: %.1f",
                                 (double)kinetic, (double)gravitational,
                                 (double)elastic);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Total: %.1f",
                                 (double)(kinetic + gravitational + elastic));
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
