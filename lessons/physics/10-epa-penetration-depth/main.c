/*
 * Physics Lesson 10 — EPA Penetration Depth
 *
 * Demonstrates: Expanding Polytope Algorithm (EPA) for computing
 * penetration depth and contact normal from a GJK simplex. EPA takes
 * GJK's "yes, these shapes overlap" answer and computes "by how much
 * and in which direction?" — the data needed for collision response.
 *
 * Two selectable scenes:
 *   1. Two-Body EPA Inspector — Move shape A with arrow keys. When
 *      shapes overlap, EPA computes penetration depth and contact
 *      normal. A green arrow shows the MTV, small spheres mark the
 *      contact points on each shape. UI shows EPA depth, normal, and
 *      iteration count.
 *   2. Full Physics Pipeline — 15 mixed bodies falling under gravity.
 *      SAP broadphase -> GJK narrowphase -> EPA contact generation ->
 *      impulse-based contact resolution. Bodies stack, bounce, settle.
 *
 * Controls:
 *   WASD              — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   Arrow keys        — move shape A (Scene 1)
 *   1-6               — select shape pair (Scene 1)
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   Tab               — cycle scenes
 *   V                 — toggle AABB visualization (Scene 2)
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Physics library — rigid bodies, contacts, collision shapes, SAP, GJK, EPA */
#include "physics/forge_physics.h"

/* Procedural geometry — sphere, cube, capsule meshes */
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
#define MAX_BODIES           64
#define MAX_CONTACTS         256

/* Scene 1: Two-Body EPA Inspector */
#define S1_MOVE_SPEED        5.0f
#define S1_BODY_A_X         -1.5f
#define S1_BODY_A_Y          1.5f
#define S1_BODY_B_X          1.5f
#define S1_BODY_B_Y          1.5f
#define S1_SPHERE_RADIUS     0.8f
#define S1_BOX_HALF          0.7f
#define S1_CAPSULE_RADIUS    0.4f
#define S1_CAPSULE_HALF_H    0.5f

/* Shape pair configurations for Scene 1 */
#define S1_NUM_PAIRS         6

/* Scene 2: SAP + GJK + EPA Pipeline */
#define S2_NUM_BODIES        15
#define S2_MASS              5.0f
#define S2_SPHERE_RADIUS     0.5f
#define S2_BOX_HALF          0.45f
#define S2_CAPSULE_RADIUS    0.3f
#define S2_CAPSULE_HALF_H    0.4f
#define S2_DROP_BASE         5.0f
#define S2_DROP_RANGE       12.0f
#define S2_SPREAD_X          6.0f
#define S2_SPREAD_Z          6.0f

/* Common physics parameters */
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

/* Camera */
#define CAM_START_X          0.0f
#define CAM_START_Y          6.0f
#define CAM_START_Z         16.0f
#define CAM_START_PITCH     -0.2f

/* Mesh resolution */
#define SPHERE_SLICES       16
#define SPHERE_STACKS        8
#define CUBE_SLICES          1
#define CUBE_STACKS          1
#define CAPSULE_SLICES       12
#define CAPSULE_STACKS        4
#define CAPSULE_CAP_STACKS    4

/* Contact visualization — small sphere for contact point markers */
#define MARKER_RADIUS        0.06f
#define MARKER_SLICES        8
#define MARKER_STACKS        4
/* Arrow shaft — thin cube scaled to represent the contact normal */
#define ARROW_THICKNESS      0.03f
#define ARROW_MIN_LENGTH     0.001f   /* below this length, skip drawing */
#define ARROW_MIN_VISIBLE    0.3f     /* minimum visual length for visibility */
#define ARROW_PARALLEL_DOT   0.999f   /* dot threshold for direction ~= up */

/* UI panel layout */
#define PANEL_X             10.0f
#define PANEL_Y             10.0f
#define PANEL_W            360.0f
#define PANEL_H            560.0f
#define LABEL_HEIGHT        24.0f
#define LABEL_SPACER        (LABEL_HEIGHT * 0.5f)

/* Movement input dead-zone */
#define MOVE_INPUT_EPSILON   0.001f

/* Capsule mesh aspect-ratio comparison tolerance */
#define CAPSULE_ASPECT_TOL   0.01f

/* Ground contact reservation — a box can generate up to 8 corner contacts */
#define GROUND_CONTACT_RESERVE  8

/* Number of scenes */
#define NUM_SCENES           2

/* Maximum distinct capsule aspect-ratio meshes */
#define MAX_CAPSULE_MESHES   4

/* AABB wireframe colors */
#define AABB_COLOR_R         0.2f
#define AABB_COLOR_G         0.9f
#define AABB_COLOR_B         0.2f
#define AABB_COLOR_A         0.25f

#define AABB_OVERLAP_R       0.95f
#define AABB_OVERLAP_G       0.6f
#define AABB_OVERLAP_B       0.1f
#define AABB_OVERLAP_A       0.35f

/* Shape colors */
#define COLOR_DEFAULT_R      0.5f
#define COLOR_DEFAULT_G      0.7f
#define COLOR_DEFAULT_B      0.9f

#define COLOR_INTERSECT_R    0.95f
#define COLOR_INTERSECT_G    0.2f
#define COLOR_INTERSECT_B    0.15f

#define COLOR_RED_R          0.9f
#define COLOR_RED_G          0.3f
#define COLOR_RED_B          0.2f

#define COLOR_BLUE_R         0.2f
#define COLOR_BLUE_G         0.5f
#define COLOR_BLUE_B         0.9f

#define COLOR_GREEN_R        0.3f
#define COLOR_GREEN_G        0.8f
#define COLOR_GREEN_B        0.3f

#define COLOR_ORANGE_R       0.9f
#define COLOR_ORANGE_G       0.6f
#define COLOR_ORANGE_B       0.1f

#define COLOR_PURPLE_R       0.7f
#define COLOR_PURPLE_G       0.3f
#define COLOR_PURPLE_B       0.9f

#define COLOR_TEAL_R         0.2f
#define COLOR_TEAL_G         0.8f
#define COLOR_TEAL_B         0.8f

#define COLOR_PINK_R         0.9f
#define COLOR_PINK_G         0.4f
#define COLOR_PINK_B         0.6f

#define COLOR_LIME_R         0.6f
#define COLOR_LIME_G         0.9f
#define COLOR_LIME_B         0.2f

#define COLOR_GOLD_R         0.9f
#define COLOR_GOLD_G         0.8f
#define COLOR_GOLD_B         0.2f

/* Pseudo-random LCG for deterministic spawning */
#define SPAWN_SEED           42
#define LCG_MULTIPLIER       1664525u
#define LCG_INCREMENT        1013904223u
#define LCG_SHIFT            8
#define LCG_DIVISOR          (1u << 24)

/* Number of distinct body colors */
#define NUM_BODY_COLORS      9

/* Shared body color palette */
static const float BODY_PALETTE[NUM_BODY_COLORS][3] = {
    {COLOR_RED_R,    COLOR_RED_G,    COLOR_RED_B},
    {COLOR_BLUE_R,   COLOR_BLUE_G,   COLOR_BLUE_B},
    {COLOR_GREEN_R,  COLOR_GREEN_G,  COLOR_GREEN_B},
    {COLOR_ORANGE_R, COLOR_ORANGE_G, COLOR_ORANGE_B},
    {COLOR_PURPLE_R, COLOR_PURPLE_G, COLOR_PURPLE_B},
    {COLOR_TEAL_R,   COLOR_TEAL_G,   COLOR_TEAL_B},
    {COLOR_PINK_R,   COLOR_PINK_G,   COLOR_PINK_B},
    {COLOR_LIME_R,   COLOR_LIME_G,   COLOR_LIME_B},
    {COLOR_GOLD_R,   COLOR_GOLD_G,   COLOR_GOLD_B},
};

/* Shape pair names for Scene 1 UI */
static const char *S1_PAIR_NAMES[S1_NUM_PAIRS] = {
    "Sphere - Sphere",
    "Sphere - Box",
    "Sphere - Capsule",
    "Box - Box",
    "Box - Capsule",
    "Capsule - Capsule",
};

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Custom pipelines */
    SDL_GPUGraphicsPipeline *wireframe_pipeline;  /* AABB wireframe overlay */

    /* GPU geometry — vertex/index buffers for each shape type */
    SDL_GPUBuffer *cube_vb;
    SDL_GPUBuffer *cube_ib;
    Uint32         cube_index_count;

    SDL_GPUBuffer *sphere_vb;
    SDL_GPUBuffer *sphere_ib;
    Uint32         sphere_index_count;

    /* Small sphere for contact point markers */
    SDL_GPUBuffer *marker_vb;
    SDL_GPUBuffer *marker_ib;
    Uint32         marker_index_count;

    /* Per-aspect-ratio capsule meshes */
    struct {
        SDL_GPUBuffer *vb;
        SDL_GPUBuffer *ib;
        Uint32         index_count;
        float          aspect;
    } capsule_meshes[MAX_CAPSULE_MESHES];
    int num_capsule_meshes;

    /* Physics state */
    ForgePhysicsRigidBody      bodies[MAX_BODIES];      /* rigid body dynamics  */
    ForgePhysicsCollisionShape shapes[MAX_BODIES];       /* collision geometry   */
    float                      body_colors[MAX_BODIES][4]; /* RGBA display color */
    int                        num_bodies;               /* active body count   */

    /* Cached AABBs — recomputed each physics step from body pose + shape */
    ForgePhysicsAABB           cached_aabbs[MAX_BODIES];

    /* SAP broadphase — finds AABB-overlapping pairs for narrowphase */
    ForgePhysicsSAPWorld       sap;

    /* GJK/EPA results for current frame */
    int  gjk_pair_count;
    int  gjk_hit_count;
    int  epa_contact_count;
    bool body_gjk_hit[MAX_BODIES];

    /* Scene 1: EPA result for the two-body test */
    ForgePhysicsEPAResult s1_epa_result;

    /* Scene 2: contact state */
    int last_contact_count;  /* total contacts (ground + EPA) last step */

    /* Simulation control */
    int   scene_index;    /* 0 = EPA Inspector, 1 = Full Pipeline    */
    float accumulator;    /* fixed-timestep accumulator (seconds)     */
    bool  paused;         /* true when physics is frozen              */
    float speed_scale;    /* 1.0 normal, 0.25 slow motion            */

    /* AABB visualization toggle */
    bool show_aabbs;

    /* Which bodies are in an overlapping SAP pair */
    bool body_in_pair[MAX_BODIES];

    /* Scene 1 — user-controlled movement (arrow keys) */
    bool move_left;      /* left arrow held   */
    bool move_right;     /* right arrow held  */
    bool move_fwd;       /* up arrow held     */
    bool move_back;      /* down arrow held   */
    int  s1_pair_index;  /* active shape pair 0..5 */

    /* UI window state */
    ForgeUiWindowState ui_window;
} app_state;

/* ── Helper: upload shape vertex buffer ───────────────────────────── */

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

static void set_body_color(app_state *state, int idx,
                            float r, float g, float b)
{
    state->body_colors[idx][0] = r;
    state->body_colors[idx][1] = g;
    state->body_colors[idx][2] = b;
    state->body_colors[idx][3] = 1.0f;
}

/* ── Helper: init body with collision shape ───────────────────────── */

static void init_body(app_state *state, int idx, vec3 pos,
                       ForgePhysicsCollisionShape shape,
                       float mass, float restit,
                       float cr, float cg, float cb)
{
    state->bodies[idx] = forge_physics_rigid_body_create(
        pos, mass, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, restit);
    forge_physics_rigid_body_set_inertia_from_shape(
        &state->bodies[idx], &shape);
    state->shapes[idx] = shape;
    set_body_color(state, idx, cr, cg, cb);
}

/* ── Helper: render scale from collision shape ────────────────────── */

static vec3 shape_render_scale(const ForgePhysicsCollisionShape *shape)
{
    switch (shape->type) {
    case FORGE_PHYSICS_SHAPE_SPHERE: {
        float r = shape->data.sphere.radius;
        return vec3_create(r, r, r);
    }
    case FORGE_PHYSICS_SHAPE_BOX:
        return shape->data.box.half_extents;
    case FORGE_PHYSICS_SHAPE_CAPSULE: {
        float r = shape->data.capsule.radius;
        return vec3_create(r, r, r);
    }
    default:
        return vec3_create(1.0f, 1.0f, 1.0f);
    }
}

/* ── Helper: interpolated model matrix ────────────────────────────── */

static mat4 get_body_model_matrix(const ForgePhysicsRigidBody *rb,
                                    const ForgePhysicsCollisionShape *shape,
                                    float alpha)
{
    vec3 pos = vec3_lerp(rb->prev_position, rb->position, alpha);
    quat orient = quat_slerp(rb->prev_orientation, rb->orientation, alpha);
    mat4 translation = mat4_translate(pos);
    mat4 rotation    = quat_to_mat4(orient);
    vec3 sc = shape_render_scale(shape);
    mat4 scale       = mat4_scale(sc);
    return mat4_multiply(translation, mat4_multiply(rotation, scale));
}

/* ── Helper: deterministic pseudo-random ──────────────────────────── */

static uint32_t lcg_state = SPAWN_SEED;

static float rand_float(float lo, float hi)
{
    lcg_state = lcg_state * LCG_MULTIPLIER + LCG_INCREMENT;
    float t = (float)(lcg_state >> LCG_SHIFT) / (float)LCG_DIVISOR;
    return lo + t * (hi - lo);
}

/* ── Helper: find capsule mesh for a given shape ──────────────────── */

static int find_capsule_mesh(const app_state *state,
                              const ForgePhysicsCollisionShape *shape)
{
    float aspect = shape->data.capsule.half_height / shape->data.capsule.radius;
    for (int i = 0; i < state->num_capsule_meshes; i++) {
        if (SDL_fabsf(state->capsule_meshes[i].aspect - aspect) < CAPSULE_ASPECT_TOL)
            return i;
    }
    return 0;
}

/* ── Helper: get shape A/B types for Scene 1 pair index ──────────── */

static void s1_pair_shapes(int pair_index,
                            ForgePhysicsCollisionShape *sa,
                            ForgePhysicsCollisionShape *sb)
{
    switch (pair_index) {
    case 0: /* sphere-sphere */
        *sa = forge_physics_shape_sphere(S1_SPHERE_RADIUS);
        *sb = forge_physics_shape_sphere(S1_SPHERE_RADIUS);
        break;
    case 1: /* sphere-box */
        *sa = forge_physics_shape_sphere(S1_SPHERE_RADIUS);
        *sb = forge_physics_shape_box(vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF));
        break;
    case 2: /* sphere-capsule */
        *sa = forge_physics_shape_sphere(S1_SPHERE_RADIUS);
        *sb = forge_physics_shape_capsule(S1_CAPSULE_RADIUS, S1_CAPSULE_HALF_H);
        break;
    case 3: /* box-box */
        *sa = forge_physics_shape_box(vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF));
        *sb = forge_physics_shape_box(vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF));
        break;
    case 4: /* box-capsule */
        *sa = forge_physics_shape_box(vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF));
        *sb = forge_physics_shape_capsule(S1_CAPSULE_RADIUS, S1_CAPSULE_HALF_H);
        break;
    default: /* capsule-capsule */
        *sa = forge_physics_shape_capsule(S1_CAPSULE_RADIUS, S1_CAPSULE_HALF_H);
        *sb = forge_physics_shape_capsule(S1_CAPSULE_RADIUS, S1_CAPSULE_HALF_H);
        break;
    }
}

/* Forward declarations */
static void rebuild_body_in_pair(app_state *state);
static void run_narrowphase(app_state *state);

/* ── Scene initialization ────────────────────────────────────────── */

/* Scene 1: Two-Body EPA Inspector — user moves shape A toward/away from B */
static void init_scene_1(app_state *state)
{
    state->num_bodies = 2;

    ForgePhysicsCollisionShape sa, sb;
    s1_pair_shapes(state->s1_pair_index, &sa, &sb);

    /* Body A — user-controlled (static mass = 0) */
    init_body(state, 0,
        vec3_create(S1_BODY_A_X, S1_BODY_A_Y, 0.0f),
        sa, 0.0f, DEFAULT_RESTIT,
        COLOR_DEFAULT_R, COLOR_DEFAULT_G, COLOR_DEFAULT_B);

    /* Body B — stationary target (static mass = 0) */
    init_body(state, 1,
        vec3_create(S1_BODY_B_X, S1_BODY_B_Y, 0.0f),
        sb, 0.0f, DEFAULT_RESTIT,
        COLOR_DEFAULT_R, COLOR_DEFAULT_G, COLOR_DEFAULT_B);

    /* Clear EPA result */
    SDL_memset(&state->s1_epa_result, 0, sizeof(state->s1_epa_result));
}

/* Scene 2: SAP + GJK + EPA Pipeline — mixed bodies falling */
static void init_scene_2(app_state *state)
{
    state->num_bodies = S2_NUM_BODIES;
    lcg_state = SPAWN_SEED;

    for (int i = 0; i < S2_NUM_BODIES; i++) {
        float x = rand_float(-S2_SPREAD_X, S2_SPREAD_X);
        float y = S2_DROP_BASE + rand_float(0.0f, S2_DROP_RANGE);
        float z = rand_float(-S2_SPREAD_Z, S2_SPREAD_Z);

        int ci = i % NUM_BODY_COLORS;
        ForgePhysicsCollisionShape shape;

        int shape_type = i % 3;
        switch (shape_type) {
        case 0:
            shape = forge_physics_shape_sphere(S2_SPHERE_RADIUS);
            break;
        case 1:
            shape = forge_physics_shape_box(
                vec3_create(S2_BOX_HALF, S2_BOX_HALF, S2_BOX_HALF));
            break;
        default:
            shape = forge_physics_shape_capsule(
                S2_CAPSULE_RADIUS, S2_CAPSULE_HALF_H);
            break;
        }

        init_body(state, i, vec3_create(x, y, z),
                  shape, S2_MASS, DEFAULT_RESTIT,
                  BODY_PALETTE[ci][0], BODY_PALETTE[ci][1],
                  BODY_PALETTE[ci][2]);
    }
}

/* ── Initialize current scene ────────────────────────────────────── */

static void init_current_scene(app_state *state)
{
    switch (state->scene_index) {
    case 0: init_scene_1(state); break;
    case 1: init_scene_2(state); break;
    }

    /* Reset physics state */
    state->accumulator  = 0.0f;
    state->gjk_pair_count   = 0;
    state->gjk_hit_count    = 0;
    state->epa_contact_count = 0;
    state->last_contact_count = 0;
    SDL_memset(state->body_gjk_hit, 0, sizeof(state->body_gjk_hit));
    SDL_memset(state->body_in_pair, 0, sizeof(state->body_in_pair));

    /* Initialize SAP */
    forge_physics_sap_destroy(&state->sap);
    SDL_memset(&state->sap, 0, sizeof(state->sap));
    forge_physics_sap_init(&state->sap);

    /* Initial AABB update */
    for (int i = 0; i < state->num_bodies; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i], state->bodies[i].position,
            state->bodies[i].orientation);
    }

    forge_physics_sap_update(&state->sap, state->cached_aabbs,
                              state->num_bodies);
    rebuild_body_in_pair(state);

    /* Run initial narrowphase */
    run_narrowphase(state);
}

/* ── Reset simulation ────────────────────────────────────────────── */

static void reset_simulation(app_state *state)
{
    init_current_scene(state);
}

/* ── Switch scene ────────────────────────────────────────────────── */

static void set_scene(app_state *state, int index)
{
    if (index < 0 || index >= NUM_SCENES) index = 0;
    state->scene_index = index;
    init_current_scene(state);
}

/* ── Narrowphase: run GJK + EPA on SAP pairs ─────────────────────── */

static void rebuild_body_in_pair(app_state *state)
{
    SDL_memset(state->body_in_pair, 0, sizeof(state->body_in_pair));
    const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(&state->sap);
    int pair_count = forge_physics_sap_pair_count(&state->sap);
    for (int i = 0; i < pair_count; i++) {
        int a = pairs[i].a;
        int b = pairs[i].b;
        if (a >= 0 && a < state->num_bodies) state->body_in_pair[a] = true;
        if (b >= 0 && b < state->num_bodies) state->body_in_pair[b] = true;
    }
}

static void run_narrowphase(app_state *state)
{
    SDL_memset(state->body_gjk_hit, 0, sizeof(state->body_gjk_hit));
    state->gjk_hit_count  = 0;
    state->epa_contact_count = 0;

    const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(&state->sap);
    int pair_count = forge_physics_sap_pair_count(&state->sap);
    state->gjk_pair_count = pair_count;

    /* Clear Scene 1 EPA result */
    SDL_memset(&state->s1_epa_result, 0, sizeof(state->s1_epa_result));

    for (int i = 0; i < pair_count; i++) {
        int a = pairs[i].a;
        int b = pairs[i].b;
        if (a < 0 || a >= state->num_bodies ||
            b < 0 || b >= state->num_bodies) continue;

        ForgePhysicsGJKResult gjk = forge_physics_gjk_test_bodies(
            &state->bodies[a], &state->shapes[a],
            &state->bodies[b], &state->shapes[b]);

        if (gjk.intersecting) {
            state->body_gjk_hit[a] = true;
            state->body_gjk_hit[b] = true;
            state->gjk_hit_count++;

            /* Run EPA to get penetration depth */
            ForgePhysicsEPAResult epa = forge_physics_epa_bodies(
                &gjk,
                &state->bodies[a], &state->shapes[a],
                &state->bodies[b], &state->shapes[b]);

            if (epa.valid) {
                state->epa_contact_count++;

                /* Scene 1: store EPA result for visualization */
                if (state->scene_index == 0 && a == 0 && b == 1) {
                    state->s1_epa_result = epa;
                }
            }
        }
    }
}

/* ── Physics step (Scene 2 only) ─────────────────────────────────── */

static void physics_step(app_state *state, float dt)
{
    /* Scene 1 has no dynamics — user moves bodies directly */
    if (state->scene_index == 0) return;

    int n = state->num_bodies;

    /* Store previous state for interpolation */
    for (int i = 0; i < n; i++) {
        state->bodies[i].prev_position    = state->bodies[i].position;
        state->bodies[i].prev_orientation = state->bodies[i].orientation;
    }

    /* Apply gravity */
    vec3 gravity = vec3_create(0.0f, -DEFAULT_GRAVITY, 0.0f);
    for (int i = 0; i < n; i++) {
        if (state->bodies[i].inv_mass == 0.0f) continue;
        forge_physics_rigid_body_apply_force(&state->bodies[i],
            vec3_scale(gravity, state->bodies[i].mass));
    }

    /* Integrate */
    for (int i = 0; i < n; i++) {
        forge_physics_rigid_body_integrate(&state->bodies[i], dt);
    }

    /* Update AABBs */
    for (int i = 0; i < n; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i], state->bodies[i].position,
            state->bodies[i].orientation);
    }

    /* SAP broadphase */
    forge_physics_sap_update(&state->sap, state->cached_aabbs, n);
    rebuild_body_in_pair(state);

    /* Ground collision — analytical sphere/box/capsule vs plane */
    ForgePhysicsRBContact ground_contacts[MAX_CONTACTS];
    int ground_count = 0;
    vec3 plane_pt = vec3_create(0.0f, GROUND_Y, 0.0f);
    vec3 plane_n  = vec3_create(0.0f, 1.0f, 0.0f);

    for (int i = 0; i < n && ground_count < MAX_CONTACTS - GROUND_CONTACT_RESERVE; i++) {
        if (state->bodies[i].inv_mass == 0.0f) continue;

        int count = 0;
        switch (state->shapes[i].type) {
        case FORGE_PHYSICS_SHAPE_SPHERE:
            count = forge_physics_rb_collide_sphere_plane(
                &state->bodies[i], i,
                state->shapes[i].data.sphere.radius,
                plane_pt, plane_n,
                DEFAULT_MU_S, DEFAULT_MU_D,
                &ground_contacts[ground_count]) ? 1 : 0;
            break;
        case FORGE_PHYSICS_SHAPE_BOX:
            count = forge_physics_rb_collide_box_plane(
                &state->bodies[i], i,
                state->shapes[i].data.box.half_extents,
                plane_pt, plane_n,
                DEFAULT_MU_S, DEFAULT_MU_D,
                &ground_contacts[ground_count],
                MAX_CONTACTS - ground_count);
            break;
        case FORGE_PHYSICS_SHAPE_CAPSULE:
            /* Capsule-plane collision: find the point on the capsule's
             * internal segment closest to the plane, then test as a sphere.
             * The capsule segment endpoints in local space are (0, ±half_h, 0).
             * The closest point on the segment to the plane is the endpoint
             * with the smallest signed distance, clamped to the segment. */
            {
                float cr = state->shapes[i].data.capsule.radius;
                float ch = state->shapes[i].data.capsule.half_height;
                mat3 R = quat_to_mat3(state->bodies[i].orientation);
                vec3 local_top = vec3_create(0.0f,  ch, 0.0f);
                vec3 local_bot = vec3_create(0.0f, -ch, 0.0f);
                vec3 world_top = vec3_add(state->bodies[i].position,
                                           mat3_multiply_vec3(R, local_top));
                vec3 world_bot = vec3_add(state->bodies[i].position,
                                           mat3_multiply_vec3(R, local_bot));

                float dist_top = vec3_dot(vec3_sub(world_top, plane_pt), plane_n);
                float dist_bot = vec3_dot(vec3_sub(world_bot, plane_pt), plane_n);

                /* The closest point on the segment to the plane is the
                 * endpoint with the smaller signed distance. For a
                 * horizontal capsule, both endpoints may be equidistant —
                 * test both to generate two contacts for stable resting. */
                float min_dist = dist_bot < dist_top ? dist_bot : dist_top;
                float max_dist = dist_bot < dist_top ? dist_top : dist_bot;
                vec3  min_pt   = dist_bot < dist_top ? world_bot : world_top;
                vec3  max_pt   = dist_bot < dist_top ? world_top : world_bot;

                /* Primary contact: closest endpoint */
                if (min_dist < cr) {
                    ForgePhysicsRBContact *gc = &ground_contacts[ground_count + count];
                    gc->point = vec3_sub(min_pt, vec3_scale(plane_n, min_dist));
                    gc->normal = plane_n;
                    gc->penetration = cr - min_dist;
                    gc->body_a = i;
                    gc->body_b = -1;
                    gc->static_friction  = DEFAULT_MU_S;
                    gc->dynamic_friction = DEFAULT_MU_D;
                    count++;
                }
                /* Secondary contact: other endpoint (for stable resting) */
                if (max_dist < cr && ground_count + count < MAX_CONTACTS) {
                    ForgePhysicsRBContact *gc = &ground_contacts[ground_count + count];
                    gc->point = vec3_sub(max_pt, vec3_scale(plane_n, max_dist));
                    gc->normal = plane_n;
                    gc->penetration = cr - max_dist;
                    gc->body_a = i;
                    gc->body_b = -1;
                    gc->static_friction  = DEFAULT_MU_S;
                    gc->dynamic_friction = DEFAULT_MU_D;
                    count++;
                }
            }
            break;
        default:
            break;
        }

        ground_count += count;
    }

    /* Body-body contacts via GJK + EPA */
    ForgePhysicsRBContact body_contacts[MAX_CONTACTS];
    int body_count = 0;

    const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(&state->sap);
    int pair_count = forge_physics_sap_pair_count(&state->sap);

    SDL_memset(state->body_gjk_hit, 0, sizeof(state->body_gjk_hit));
    state->gjk_pair_count = pair_count;
    state->gjk_hit_count  = 0;
    state->epa_contact_count = 0;

    for (int i = 0; i < pair_count && body_count < MAX_CONTACTS; i++) {
        int a = pairs[i].a;
        int b = pairs[i].b;
        if (a < 0 || a >= n || b < 0 || b >= n) continue;

        ForgePhysicsRBContact contact;
        if (forge_physics_gjk_epa_contact(
                &state->bodies[a], &state->shapes[a],
                &state->bodies[b], &state->shapes[b],
                a, b, DEFAULT_MU_S, DEFAULT_MU_D, &contact))
        {
            state->body_gjk_hit[a] = true;
            state->body_gjk_hit[b] = true;
            state->gjk_hit_count++;
            state->epa_contact_count++;
            body_contacts[body_count++] = contact;
        }
    }

    /* Resolve contacts — ground first, then body-body */
    if (ground_count > 0) {
        forge_physics_rb_resolve_contacts(
            ground_contacts, ground_count,
            state->bodies, n,
            DEFAULT_SOLVER_ITERS, dt);
    }
    if (body_count > 0) {
        forge_physics_rb_resolve_contacts(
            body_contacts, body_count,
            state->bodies, n,
            DEFAULT_SOLVER_ITERS, dt);
    }

    state->last_contact_count = ground_count + body_count;
}

/* ── Drawing helpers ──────────────────────────────────────────────── */

/* Draw an AABB wireframe overlay */
static void draw_aabb_wireframe(app_state *state, ForgePhysicsAABB aabb,
                                 bool in_pair, bool gjk_hit)
{
    vec3 center = forge_physics_aabb_center(aabb);
    vec3 he = forge_physics_aabb_extents(aabb);
    mat4 model = mat4_multiply(mat4_translate(center), mat4_scale(he));

    float color[4];
    if (gjk_hit) {
        color[0] = COLOR_INTERSECT_R;
        color[1] = COLOR_INTERSECT_G;
        color[2] = COLOR_INTERSECT_B;
        color[3] = AABB_OVERLAP_A;
    } else if (in_pair) {
        color[0] = AABB_OVERLAP_R;
        color[1] = AABB_OVERLAP_G;
        color[2] = AABB_OVERLAP_B;
        color[3] = AABB_OVERLAP_A;
    } else {
        color[0] = AABB_COLOR_R;
        color[1] = AABB_COLOR_G;
        color[2] = AABB_COLOR_B;
        color[3] = AABB_COLOR_A;
    }

    forge_scene_draw_mesh_ex(&state->scene, state->wireframe_pipeline,
        state->cube_vb, state->cube_ib,
        state->cube_index_count, model, color);
}

/* Draw a small marker sphere at a world position with a given color */
static void draw_marker(app_state *state, vec3 pos,
                         float r, float g, float b)
{
    mat4 model = mat4_multiply(
        mat4_translate(pos),
        mat4_scale(vec3_create(MARKER_RADIUS, MARKER_RADIUS, MARKER_RADIUS)));
    float color[4] = {r, g, b, 1.0f};
    forge_scene_draw_mesh(&state->scene,
        state->marker_vb, state->marker_ib,
        state->marker_index_count, model, color);
}

/* Draw an arrow (thin cube) from origin along direction with length.
 * The arrow is rendered as a thin scaled cube oriented along the
 * direction vector. */
static void draw_arrow(app_state *state, vec3 origin, vec3 dir,
                        float length, float r, float g, float b)
{
    if (length < ARROW_MIN_LENGTH) return;

    /* Scale: thin in X/Z, length in Y */
    float visual_length = length > ARROW_MIN_VISIBLE ? length : ARROW_MIN_VISIBLE;
    vec3 scale = vec3_create(ARROW_THICKNESS, visual_length * 0.5f,
                              ARROW_THICKNESS);

    /* Build orientation: align Y axis with dir */
    vec3 up = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 d = vec3_normalize(dir);

    /* If dir is nearly parallel to up, use a different reference axis */
    mat4 rotation;
    float dot = SDL_fabsf(vec3_dot(d, up));
    if (dot > ARROW_PARALLEL_DOT) {
        /* Direction is nearly vertical — use a simpler rotation */
        if (d.y > 0.0f) {
            rotation = mat4_identity();
        } else {
            /* 180 degree rotation around X */
            rotation = quat_to_mat4(quat_from_euler(0.0f, 0.0f,
                                                     FORGE_PI));
        }
    } else {
        /* Build rotation from Y to dir using cross product */
        vec3 axis = vec3_normalize(vec3_cross(up, d));
        float cos_angle = vec3_dot(up, d);
        /* Clamp to [-1, 1] — floating-point imprecision can produce
         * values slightly outside this range, causing SDL_acosf to
         * return NaN. */
        if (cos_angle >  1.0f) cos_angle =  1.0f;
        if (cos_angle < -1.0f) cos_angle = -1.0f;
        float angle = SDL_acosf(cos_angle);
        quat q = quat_from_axis_angle(axis, angle);
        rotation = quat_to_mat4(q);
    }

    /* Position: center of the arrow shaft (halfway along direction) */
    vec3 center = vec3_add(origin, vec3_scale(d, visual_length * 0.5f));

    mat4 model = mat4_multiply(
        mat4_translate(center),
        mat4_multiply(rotation, mat4_scale(scale)));

    float color[4] = {r, g, b, 1.0f};
    forge_scene_draw_mesh(&state->scene,
        state->cube_vb, state->cube_ib,
        state->cube_index_count, model, color);
}

/* ── SDL_AppEvent ────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    SDL_AppResult result = forge_scene_handle_event(&state->scene, event);
    if (result != SDL_APP_CONTINUE) return result;

    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (!event->key.repeat) {
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
            case SDL_SCANCODE_V:
                state->show_aabbs = !state->show_aabbs;
                SDL_Log("AABB visualization: %s",
                        state->show_aabbs ? "ON" : "OFF");
                break;
            case SDL_SCANCODE_TAB:
                set_scene(state, (state->scene_index + 1) % NUM_SCENES);
                SDL_Log("Scene %d", state->scene_index + 1);
                break;
            /* Scene 1: shape pair selection with number keys 1-6 */
            case SDL_SCANCODE_1:
            case SDL_SCANCODE_2:
            case SDL_SCANCODE_3:
            case SDL_SCANCODE_4:
            case SDL_SCANCODE_5:
            case SDL_SCANCODE_6:
                if (state->scene_index == 0) {
                    int idx = event->key.scancode - SDL_SCANCODE_1;
                    if (idx >= 0 && idx < S1_NUM_PAIRS) {
                        state->s1_pair_index = idx;
                        init_scene_1(state);
                        /* Re-run narrowphase for new shapes */
                        for (int i = 0; i < state->num_bodies; i++) {
                            state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
                                &state->shapes[i],
                                state->bodies[i].position,
                                state->bodies[i].orientation);
                        }
                        forge_physics_sap_update(&state->sap,
                            state->cached_aabbs, state->num_bodies);
                        rebuild_body_in_pair(state);
                        run_narrowphase(state);
                        SDL_Log("Shape pair: %s",
                                S1_PAIR_NAMES[state->s1_pair_index]);
                    }
                }
                break;
            default:
                break;
            }
        }

        /* Movement keys for Scene 1 (hold-to-move) */
        switch (event->key.scancode) {
        case SDL_SCANCODE_LEFT:  state->move_left  = true; break;
        case SDL_SCANCODE_RIGHT: state->move_right = true; break;
        case SDL_SCANCODE_UP:    state->move_fwd   = true; break;
        case SDL_SCANCODE_DOWN:  state->move_back  = true; break;
        default: break;
        }
    }

    if (event->type == SDL_EVENT_KEY_UP) {
        switch (event->key.scancode) {
        case SDL_SCANCODE_LEFT:  state->move_left  = false; break;
        case SDL_SCANCODE_RIGHT: state->move_right = false; break;
        case SDL_SCANCODE_UP:    state->move_fwd   = false; break;
        case SDL_SCANCODE_DOWN:  state->move_back  = false; break;
        default: break;
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

    /* ── Scene 1: user movement ─────────────────────────────────── */
    if (state->scene_index == 0 && !state->paused) {
        vec3 move = vec3_create(0.0f, 0.0f, 0.0f);
        if (state->move_left)  move.x -= 1.0f;
        if (state->move_right) move.x += 1.0f;
        if (state->move_fwd)   move.z -= 1.0f;
        if (state->move_back)  move.z += 1.0f;

        if (vec3_length(move) > MOVE_INPUT_EPSILON) {
            move = vec3_scale(vec3_normalize(move),
                              S1_MOVE_SPEED * dt);
            state->bodies[0].prev_position = state->bodies[0].position;
            state->bodies[0].position = vec3_add(
                state->bodies[0].position, move);

            /* Recompute AABBs and run narrowphase */
            for (int i = 0; i < state->num_bodies; i++) {
                state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
                    &state->shapes[i], state->bodies[i].position,
                    state->bodies[i].orientation);
            }
            forge_physics_sap_update(&state->sap, state->cached_aabbs,
                                      state->num_bodies);
            rebuild_body_in_pair(state);
            run_narrowphase(state);
        }
    }

    /* ── Scene 2: fixed-timestep physics ─────────────────────────── */
    if (state->scene_index == 1 && !state->paused) {
        float step_dt = dt * state->speed_scale;
        state->accumulator += step_dt;

        while (state->accumulator >= PHYSICS_DT) {
            physics_step(state, PHYSICS_DT);
            state->accumulator -= PHYSICS_DT;
        }
    }

    /* Interpolation factor */
    float alpha = state->accumulator / PHYSICS_DT;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    if (state->paused) alpha = 1.0f;
    /* Scene 1 has no dynamics — use alpha = 1 */
    if (state->scene_index == 0) alpha = 1.0f;

    /* ── Collect instanced data ──────────────────────────────────── */

    ForgeSceneColoredInstance sphere_instances[MAX_BODIES];
    ForgeSceneColoredInstance cube_instances[MAX_BODIES];
    ForgeSceneColoredInstance capsule_instances[MAX_CAPSULE_MESHES][MAX_BODIES];
    int sphere_count = 0, cube_count = 0;
    int capsule_counts[MAX_CAPSULE_MESHES];
    SDL_memset(capsule_counts, 0, sizeof(capsule_counts));

    for (int i = 0; i < state->num_bodies; i++) {
        const ForgePhysicsCollisionShape *shape = &state->shapes[i];
        mat4 model = get_body_model_matrix(&state->bodies[i], shape, alpha);

        /* GJK intersection → red; otherwise use body color */
        float color[4];
        if (state->body_gjk_hit[i]) {
            color[0] = COLOR_INTERSECT_R;
            color[1] = COLOR_INTERSECT_G;
            color[2] = COLOR_INTERSECT_B;
            color[3] = 1.0f;
        } else {
            SDL_memcpy(color, state->body_colors[i], sizeof(color));
        }

        ForgeSceneColoredInstance inst;
        inst.transform = model;
        SDL_memcpy(inst.color, color, sizeof(inst.color));

        switch (shape->type) {
        case FORGE_PHYSICS_SHAPE_SPHERE:
            sphere_instances[sphere_count++] = inst;
            break;
        case FORGE_PHYSICS_SHAPE_BOX:
            cube_instances[cube_count++] = inst;
            break;
        case FORGE_PHYSICS_SHAPE_CAPSULE: {
            int ci = find_capsule_mesh(state, shape);
            if (ci >= 0 && ci < state->num_capsule_meshes)
                capsule_instances[ci][capsule_counts[ci]++] = inst;
            break;
        }
        default: break;
        }
    }

    /* Upload instance buffers — batch into one copy pass */
    forge_scene_begin_deferred_uploads(s);
    SDL_GPUBuffer *sphere_inst_buf = NULL;
    SDL_GPUBuffer *cube_inst_buf = NULL;
    SDL_GPUBuffer *capsule_inst_bufs[MAX_CAPSULE_MESHES];
    SDL_memset(capsule_inst_bufs, 0, sizeof(capsule_inst_bufs));

    if (sphere_count > 0)
        sphere_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX, sphere_instances,
            (Uint32)(sphere_count * sizeof(ForgeSceneColoredInstance)));
    if (cube_count > 0)
        cube_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX, cube_instances,
            (Uint32)(cube_count * sizeof(ForgeSceneColoredInstance)));
    for (int ci = 0; ci < state->num_capsule_meshes; ci++) {
        if (capsule_counts[ci] > 0)
            capsule_inst_bufs[ci] = forge_scene_upload_buffer_deferred(
                s, SDL_GPU_BUFFERUSAGE_VERTEX, capsule_instances[ci],
                (Uint32)(capsule_counts[ci] * sizeof(ForgeSceneColoredInstance)));
    }
    forge_scene_end_deferred_uploads(s);

    /* ── Shadow pass ─────────────────────────────────────────────── */

    forge_scene_begin_shadow_pass(s);
    if (sphere_inst_buf)
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            state->sphere_index_count, sphere_inst_buf, (Uint32)sphere_count);
    if (cube_inst_buf)
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->cube_vb, state->cube_ib,
            state->cube_index_count, cube_inst_buf, (Uint32)cube_count);
    for (int ci = 0; ci < state->num_capsule_meshes; ci++) {
        if (capsule_inst_bufs[ci])
            forge_scene_draw_shadow_mesh_instanced_colored(
                s, state->capsule_meshes[ci].vb, state->capsule_meshes[ci].ib,
                state->capsule_meshes[ci].index_count,
                capsule_inst_bufs[ci], (Uint32)capsule_counts[ci]);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */

    forge_scene_begin_main_pass(s);
    {
        float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        if (sphere_inst_buf)
            forge_scene_draw_mesh_instanced_colored(
                s, state->sphere_vb, state->sphere_ib,
                state->sphere_index_count, sphere_inst_buf,
                (Uint32)sphere_count, white);
        if (cube_inst_buf)
            forge_scene_draw_mesh_instanced_colored(
                s, state->cube_vb, state->cube_ib,
                state->cube_index_count, cube_inst_buf,
                (Uint32)cube_count, white);
        for (int ci = 0; ci < state->num_capsule_meshes; ci++) {
            if (capsule_inst_bufs[ci])
                forge_scene_draw_mesh_instanced_colored(
                    s, state->capsule_meshes[ci].vb,
                    state->capsule_meshes[ci].ib,
                    state->capsule_meshes[ci].index_count,
                    capsule_inst_bufs[ci], (Uint32)capsule_counts[ci], white);
        }
    }

    /* Scene 1: draw EPA visualization — contact normal arrow and
     * contact point markers */
    if (state->scene_index == 0 && state->s1_epa_result.valid) {
        ForgePhysicsEPAResult *epa = &state->s1_epa_result;

        /* Green arrow: contact normal from midpoint, length = depth */
        draw_arrow(state, epa->point, epa->normal, epa->depth,
                   COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B);

        /* Red marker: contact point on shape A */
        draw_marker(state, epa->point_a,
                    COLOR_RED_R, COLOR_RED_G, COLOR_RED_B);

        /* Blue marker: contact point on shape B */
        draw_marker(state, epa->point_b,
                    COLOR_BLUE_R, COLOR_BLUE_G, COLOR_BLUE_B);

        /* Gold marker: midpoint contact */
        draw_marker(state, epa->point,
                    COLOR_GOLD_R, COLOR_GOLD_G, COLOR_GOLD_B);
    }

    /* AABB wireframes (Scene 2, toggled with V) */
    if (state->scene_index == 1 && state->show_aabbs) {
        for (int i = 0; i < state->num_bodies; i++) {
            draw_aabb_wireframe(state, state->cached_aabbs[i],
                                state->body_in_pair[i],
                                state->body_gjk_hit[i]);
        }
    }

    forge_scene_draw_grid(s);
    forge_scene_end_main_pass(s);

    /* Release per-frame instance buffers */
    SDL_GPUDevice *dev = forge_scene_device(s);
    if (sphere_inst_buf)  SDL_ReleaseGPUBuffer(dev, sphere_inst_buf);
    if (cube_inst_buf)    SDL_ReleaseGPUBuffer(dev, cube_inst_buf);
    for (int ci = 0; ci < state->num_capsule_meshes; ci++) {
        if (capsule_inst_bufs[ci]) SDL_ReleaseGPUBuffer(dev, capsule_inst_bufs[ci]);
    }

    /* ── UI pass ─────────────────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "EPA Penetration Depth",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;
                char buf[128];

                /* Scene title */
                if (state->scene_index == 0) {
                    forge_ui_ctx_label_layout(ui, "Scene 1: EPA Inspector",
                                               LABEL_HEIGHT);
                } else {
                    forge_ui_ctx_label_layout(ui, "Scene 2: SAP+GJK+EPA",
                                               LABEL_HEIGHT);
                }
                forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

                if (state->scene_index == 0) {
                    /* Scene 1 UI: shape pair and EPA results */
                    SDL_snprintf(buf, sizeof(buf), "Pair: %s",
                                 S1_PAIR_NAMES[state->s1_pair_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    forge_ui_ctx_label_layout(ui, "(Keys 1-6 to change)",
                                               LABEL_HEIGHT);
                    forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

                    if (state->s1_epa_result.valid) {
                        ForgePhysicsEPAResult *epa = &state->s1_epa_result;
                        forge_ui_ctx_label_layout(ui, "EPA: INTERSECTING",
                                                   LABEL_HEIGHT);

                        SDL_snprintf(buf, sizeof(buf), "Depth: %.4f m",
                                     (double)epa->depth);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                        SDL_snprintf(buf, sizeof(buf),
                                     "Normal: (%.3f, %.3f, %.3f)",
                                     (double)epa->normal.x,
                                     (double)epa->normal.y,
                                     (double)epa->normal.z);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                        SDL_snprintf(buf, sizeof(buf), "Iterations: %d",
                                     epa->iterations);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

                        SDL_snprintf(buf, sizeof(buf),
                                     "Pt A: (%.2f, %.2f, %.2f)",
                                     (double)epa->point_a.x,
                                     (double)epa->point_a.y,
                                     (double)epa->point_a.z);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                        SDL_snprintf(buf, sizeof(buf),
                                     "Pt B: (%.2f, %.2f, %.2f)",
                                     (double)epa->point_b.x,
                                     (double)epa->point_b.y,
                                     (double)epa->point_b.z);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                        SDL_snprintf(buf, sizeof(buf),
                                     "Mid:  (%.2f, %.2f, %.2f)",
                                     (double)epa->point.x,
                                     (double)epa->point.y,
                                     (double)epa->point.z);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    } else {
                        forge_ui_ctx_label_layout(ui, "EPA: separated",
                                                   LABEL_HEIGHT);
                        forge_ui_ctx_label_layout(ui, "(Move with arrow keys)",
                                                   LABEL_HEIGHT);
                    }
                } else {
                    /* Scene 2 UI: pipeline statistics */
                    SDL_snprintf(buf, sizeof(buf), "Bodies: %d",
                                 state->num_bodies);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

                    forge_ui_ctx_label_layout(ui, "Pipeline:",
                                               LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "  SAP pairs: %d",
                                 state->gjk_pair_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "  GJK hits: %d",
                                 state->gjk_hit_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "  EPA contacts: %d",
                                 state->epa_contact_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "  Total contacts: %d",
                                 state->last_contact_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

                    SDL_snprintf(buf, sizeof(buf), "Speed: %.2fx",
                                 (double)state->speed_scale);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    forge_ui_ctx_label_layout(ui,
                        state->paused ? "PAUSED" : "Running",
                        LABEL_HEIGHT);
                }

                forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);
                forge_ui_ctx_label_layout(ui, "Tab: switch scene",
                                           LABEL_HEIGHT);
                if (state->scene_index == 0) {
                    forge_ui_ctx_label_layout(ui, "Arrows: move shape A",
                                               LABEL_HEIGHT);
                } else {
                    forge_ui_ctx_label_layout(ui, "V: toggle AABBs",
                                               LABEL_HEIGHT);
                }
                forge_ui_ctx_label_layout(ui, "P:pause R:reset T:speed",
                                           LABEL_HEIGHT);

                forge_ui_wctx_window_end(wctx);
            }
        }
    }
    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
}

/* ── SDL_AppInit ─────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state *state = SDL_calloc(1, sizeof(*state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    ForgeSceneConfig cfg = forge_scene_default_config(
        "Physics 10 - EPA Penetration Depth");
    cfg.cam_start_pos = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";

    if (!forge_scene_init(&state->scene, &cfg, argc, argv))
        return SDL_APP_FAILURE;

    /* Create wireframe pipeline for AABB visualization */
    state->wireframe_pipeline = forge_scene_create_pipeline(
        &state->scene, SDL_GPU_CULLMODE_NONE, SDL_GPU_FILLMODE_LINE);
    if (!state->wireframe_pipeline) {
        SDL_Log("ERROR: Failed to create wireframe pipeline");
        return SDL_APP_FAILURE;
    }

    /* Generate and upload cube geometry */
    ForgeShape cube = forge_shapes_cube(CUBE_SLICES, CUBE_STACKS);
    if (cube.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_cube failed");
        goto init_fail;
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
        goto init_fail;
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

    /* Generate and upload small marker sphere for contact points */
    {
        ForgeShape marker = forge_shapes_sphere(MARKER_SLICES, MARKER_STACKS);
        if (marker.vertex_count == 0) {
            SDL_Log("ERROR: forge_shapes_sphere (marker) failed");
            goto init_fail;
        }
        state->marker_vb = upload_shape_vb(&state->scene, &marker);
        state->marker_ib = forge_scene_upload_buffer(&state->scene,
            SDL_GPU_BUFFERUSAGE_INDEX, marker.indices,
            (Uint32)marker.index_count * (Uint32)sizeof(uint32_t));
        state->marker_index_count = (Uint32)marker.index_count;
        forge_shapes_free(&marker);
        if (!state->marker_vb || !state->marker_ib) {
            SDL_Log("ERROR: Failed to upload marker geometry");
            goto init_fail;
        }
    }

    /* Generate capsule meshes for all aspect ratios used */
    {
        const float capsule_aspects[] = {
            S1_CAPSULE_HALF_H / S1_CAPSULE_RADIUS,
            S2_CAPSULE_HALF_H / S2_CAPSULE_RADIUS,
        };
        int num_aspects = (int)(sizeof(capsule_aspects) / sizeof(capsule_aspects[0]));
        state->num_capsule_meshes = 0;

        for (int ci = 0; ci < num_aspects && ci < MAX_CAPSULE_MESHES; ci++) {
            /* Skip duplicate aspects */
            bool dup = false;
            for (int j = 0; j < state->num_capsule_meshes; j++) {
                if (SDL_fabsf(state->capsule_meshes[j].aspect - capsule_aspects[ci]) < CAPSULE_ASPECT_TOL) {
                    dup = true;
                    break;
                }
            }
            if (dup) continue;

            ForgeShape cap = forge_shapes_capsule(
                CAPSULE_SLICES, CAPSULE_STACKS, CAPSULE_CAP_STACKS,
                capsule_aspects[ci]);
            if (cap.vertex_count == 0) {
                SDL_Log("ERROR: forge_shapes_capsule failed (aspect %.2f)",
                        (double)capsule_aspects[ci]);
                goto init_fail;
            }
            int idx = state->num_capsule_meshes;
            state->capsule_meshes[idx].vb = upload_shape_vb(&state->scene, &cap);
            state->capsule_meshes[idx].ib = forge_scene_upload_buffer(&state->scene,
                SDL_GPU_BUFFERUSAGE_INDEX, cap.indices,
                (Uint32)cap.index_count * (Uint32)sizeof(uint32_t));
            state->capsule_meshes[idx].index_count = (Uint32)cap.index_count;
            state->capsule_meshes[idx].aspect = capsule_aspects[ci];
            forge_shapes_free(&cap);
            if (!state->capsule_meshes[idx].vb || !state->capsule_meshes[idx].ib) {
                SDL_Log("ERROR: Failed to upload capsule geometry (aspect %.2f)",
                        (double)capsule_aspects[ci]);
                goto init_fail;
            }
            state->num_capsule_meshes++;
        }
    }

    /* Default state */
    state->scene_index   = 0;
    state->accumulator   = 0.0f;
    state->paused        = false;
    state->speed_scale   = NORMAL_SPEED_SCALE;
    state->show_aabbs    = true;
    state->s1_pair_index = 0;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    init_current_scene(state);

    return SDL_APP_CONTINUE;

init_fail:
    return SDL_APP_FAILURE;
}

/* ── SDL_AppQuit ─────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (!state) return;

    SDL_GPUDevice *dev = forge_scene_device(&state->scene);
    if (dev) {
        if (!SDL_WaitForGPUIdle(dev)) {
            SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }
        if (state->wireframe_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(dev, state->wireframe_pipeline);
        if (state->cube_vb)    SDL_ReleaseGPUBuffer(dev, state->cube_vb);
        if (state->cube_ib)    SDL_ReleaseGPUBuffer(dev, state->cube_ib);
        if (state->sphere_vb)  SDL_ReleaseGPUBuffer(dev, state->sphere_vb);
        if (state->sphere_ib)  SDL_ReleaseGPUBuffer(dev, state->sphere_ib);
        if (state->marker_vb)  SDL_ReleaseGPUBuffer(dev, state->marker_vb);
        if (state->marker_ib)  SDL_ReleaseGPUBuffer(dev, state->marker_ib);
        for (int ci = 0; ci < state->num_capsule_meshes; ci++) {
            if (state->capsule_meshes[ci].vb)
                SDL_ReleaseGPUBuffer(dev, state->capsule_meshes[ci].vb);
            if (state->capsule_meshes[ci].ib)
                SDL_ReleaseGPUBuffer(dev, state->capsule_meshes[ci].ib);
        }
    }

    forge_physics_sap_destroy(&state->sap);
    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
