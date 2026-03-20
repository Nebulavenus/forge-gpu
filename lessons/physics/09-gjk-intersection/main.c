/*
 * Physics Lesson 09 — GJK Intersection Testing
 *
 * Demonstrates: Gilbert-Johnson-Keerthi (GJK) algorithm for boolean
 * intersection testing between convex shapes. GJK works by iteratively
 * building a simplex in the Minkowski difference — if the simplex
 * encloses the origin, the shapes intersect.
 *
 * Three selectable scenes:
 *   1. Two-Body Test — Move shape A with arrow keys. Shapes turn red
 *      when GJK reports intersection. UI shows iteration count.
 *   2. SAP + GJK Pipeline — 25 mixed bodies falling. SAP finds
 *      broadphase pairs, GJK confirms intersections. Orange AABB = SAP
 *      pair, red tint = GJK confirmed.
 *   3. Shape Gallery — 9 shape-pair combos in a 3x3 grid. Toggle
 *      overlap with keys 1-9. One algorithm handles all convex pairs.
 *
 * Controls:
 *   WASD              — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   Arrow keys        — move shape A (Scene 1)
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   Tab               — cycle scenes
 *   V                 — toggle AABB visualization
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Physics library — rigid bodies, contacts, collision shapes, SAP, GJK */
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

/* Scene 1: Two-Body Test */
#define S1_MOVE_SPEED        5.0f
#define S1_BODY_A_X         -2.0f
#define S1_BODY_A_Y          1.5f
#define S1_BODY_B_X          2.0f
#define S1_BODY_B_Y          1.5f
#define S1_SPHERE_RADIUS     0.8f
#define S1_BOX_HALF          0.7f

/* Scene 2: SAP + GJK Pipeline */
#define S2_NUM_BODIES        25
#define S2_MASS              5.0f
#define S2_SPHERE_RADIUS     0.4f
#define S2_BOX_HALF          0.4f
#define S2_CAPSULE_RADIUS    0.25f
#define S2_CAPSULE_HALF_H    0.35f
#define S2_DROP_BASE         5.0f
#define S2_DROP_RANGE       15.0f
#define S2_SPREAD_X          8.0f
#define S2_SPREAD_Z          8.0f
#define MAX_CONTACTS         256

/* Scene 3: Shape Gallery — 3x3 grid of shape pairs */
#define S3_GRID_SPACING      4.0f
#define S3_PAIR_OFFSET       0.6f
#define S3_OVERLAP_OFFSET    0.3f
#define S3_NUM_PAIRS         9
#define S3_BASE_Y            1.5f
#define S3_SPHERE_RADIUS     0.5f
#define S3_BOX_HALF          0.4f
#define S3_CAPSULE_RADIUS    0.3f
#define S3_CAPSULE_HALF_H    0.3f

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
#define CAM_START_Y          8.0f
#define CAM_START_Z         20.0f
#define CAM_START_PITCH     -0.2f

/* Mesh resolution */
#define SPHERE_SLICES       16
#define SPHERE_STACKS        8
#define CUBE_SLICES          1
#define CUBE_STACKS          1
#define CAPSULE_SLICES       12
#define CAPSULE_STACKS        4
#define CAPSULE_CAP_STACKS    4

/* UI panel layout */
#define PANEL_X             10.0f
#define PANEL_Y             10.0f
#define PANEL_W            280.0f
#define PANEL_H            560.0f
#define LABEL_HEIGHT        24.0f
#define BUTTON_HEIGHT       30.0f

/* Movement input dead-zone — ignore move vectors shorter than this */
#define MOVE_INPUT_EPSILON   0.001f

/* Number of scenes */
#define NUM_SCENES           3

/* AABB wireframe colors */
#define AABB_COLOR_R         0.2f
#define AABB_COLOR_G         0.9f
#define AABB_COLOR_B         0.2f
#define AABB_COLOR_A         0.25f

/* AABB overlap highlight color (orange — SAP pair) */
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

/* Pseudo-random seed for deterministic spawning */
#define SPAWN_SEED           42

/* LCG constants (Numerical Recipes) for deterministic pseudo-random spawning */
#define LCG_MULTIPLIER       1664525u
#define LCG_INCREMENT        1013904223u
#define LCG_SHIFT            8
#define LCG_DIVISOR          (1u << 24)

/* Number of distinct body colors in the palette */
#define NUM_BODY_COLORS      9

/* Shared body color palette — both scenes index into this */
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

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Custom pipelines for non-default rasterizer state */
    SDL_GPUGraphicsPipeline *wireframe_pipeline;  /* AABB wireframe overlay */

    /* GPU geometry — vertex/index buffers for each shape type */
    SDL_GPUBuffer *cube_vb;          /* unit cube vertex buffer */
    SDL_GPUBuffer *cube_ib;          /* unit cube index buffer */
    Uint32         cube_index_count; /* number of indices in cube mesh */

    SDL_GPUBuffer *sphere_vb;          /* unit sphere vertex buffer */
    SDL_GPUBuffer *sphere_ib;          /* unit sphere index buffer */
    Uint32         sphere_index_count; /* number of indices in sphere mesh */

    /* Per-aspect-ratio capsule meshes — each generated with the correct
     * half_height/radius ratio so we can scale uniformly by radius without
     * distorting the hemispherical caps. */
    #define MAX_CAPSULE_MESHES 4
    struct {
        SDL_GPUBuffer *vb;
        SDL_GPUBuffer *ib;
        Uint32         index_count;
        float          aspect;  /* half_height / radius used to generate this mesh */
    } capsule_meshes[MAX_CAPSULE_MESHES];
    int num_capsule_meshes;

    /* Physics state */
    ForgePhysicsRigidBody      bodies[MAX_BODIES];  /* rigid body state per object */
    ForgePhysicsCollisionShape shapes[MAX_BODIES];   /* collision shape per object */
    float                      body_colors[MAX_BODIES][4]; /* RGBA per body */
    int                        num_bodies;           /* active body count */

    /* Cached AABBs (updated each physics step) */
    ForgePhysicsAABB           cached_aabbs[MAX_BODIES];

    /* SAP broadphase */
    ForgePhysicsSAPWorld       sap;

    /* GJK results for current frame */
    int  gjk_pair_count;       /* number of SAP pairs tested with GJK */
    int  gjk_hit_count;        /* number confirmed intersecting by GJK */
    int  gjk_last_iterations;  /* max iteration count across GJK tests this frame */
    bool body_gjk_hit[MAX_BODIES];  /* true if GJK confirmed intersection */

    /* Contact state (for UI readout) */
    int last_contact_count;  /* ground contacts detected last physics step */

    /* Simulation control */
    int   scene_index;   /* active scene: 0=Two-Body, 1=SAP+GJK, 2=Gallery */
    float accumulator;   /* fixed-timestep accumulator (seconds) */
    bool  paused;        /* true while simulation is paused */
    float speed_scale;   /* time multiplier: 1.0 normal, 0.25 slow-mo */

    /* AABB visualization toggle */
    bool show_aabbs;

    /* Which bodies are in an overlapping SAP pair (for AABB coloring) */
    bool body_in_pair[MAX_BODIES];

    /* Scene 1 — user-controlled body A movement */
    bool move_left;
    bool move_right;
    bool move_fwd;     /* up arrow */
    bool move_back;    /* down arrow */

    /* Scene 3 — gallery overlap toggles */
    bool gallery_overlap[S3_NUM_PAIRS];

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

/* ── Helper: get render scale from collision shape ────────────────── */

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
        /* Uniform scaling — the mesh is generated with the correct
         * half_height/radius aspect ratio, so radius-only scaling
         * preserves hemispherical cap geometry exactly. */
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

/* ── Helper: simple deterministic pseudo-random ──────────────────── */

static uint32_t lcg_state = SPAWN_SEED;

static float rand_float(float lo, float hi)
{
    lcg_state = lcg_state * LCG_MULTIPLIER + LCG_INCREMENT;
    float t = (float)(lcg_state >> LCG_SHIFT) / (float)LCG_DIVISOR;
    return lo + t * (hi - lo);
}

/* Forward declarations — defined below init_current_scene */
static void rebuild_body_in_pair(app_state *state);
static void run_gjk_on_pairs(app_state *state);

/* ── Scene initialization ────────────────────────────────────────── */

/* Scene 1: Two-Body Test — user moves shape A toward/away from B */
static void init_scene_1(app_state *state)
{
    state->num_bodies = 2;

    /* Body A — sphere controlled by user */
    init_body(state, 0,
        vec3_create(S1_BODY_A_X, S1_BODY_A_Y, 0.0f),
        forge_physics_shape_sphere(S1_SPHERE_RADIUS),
        0.0f, DEFAULT_RESTIT,  /* static — user moves it */
        COLOR_DEFAULT_R, COLOR_DEFAULT_G, COLOR_DEFAULT_B);

    /* Body B — box, stationary target */
    init_body(state, 1,
        vec3_create(S1_BODY_B_X, S1_BODY_B_Y, 0.0f),
        forge_physics_shape_box(vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF)),
        0.0f, DEFAULT_RESTIT,
        COLOR_DEFAULT_R, COLOR_DEFAULT_G, COLOR_DEFAULT_B);
}

/* Scene 2: SAP + GJK Pipeline — mixed bodies falling with gravity */
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

        /* Cycle through shape types: sphere, box, capsule */
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

/* Scene 3: Shape Gallery — 3x3 grid of all shape-pair combos */
static void init_scene_3(app_state *state)
{
    /* 9 pairs: sphere-sphere, sphere-box, sphere-capsule,
     *          box-sphere, box-box, box-capsule,
     *          capsule-sphere, capsule-box, capsule-capsule */
    state->num_bodies = S3_NUM_PAIRS * 2;

    ForgePhysicsShapeType types_a[S3_NUM_PAIRS] = {
        FORGE_PHYSICS_SHAPE_SPHERE,  FORGE_PHYSICS_SHAPE_SPHERE,  FORGE_PHYSICS_SHAPE_SPHERE,
        FORGE_PHYSICS_SHAPE_BOX,     FORGE_PHYSICS_SHAPE_BOX,     FORGE_PHYSICS_SHAPE_BOX,
        FORGE_PHYSICS_SHAPE_CAPSULE, FORGE_PHYSICS_SHAPE_CAPSULE, FORGE_PHYSICS_SHAPE_CAPSULE,
    };
    ForgePhysicsShapeType types_b[S3_NUM_PAIRS] = {
        FORGE_PHYSICS_SHAPE_SPHERE,  FORGE_PHYSICS_SHAPE_BOX,     FORGE_PHYSICS_SHAPE_CAPSULE,
        FORGE_PHYSICS_SHAPE_SPHERE,  FORGE_PHYSICS_SHAPE_BOX,     FORGE_PHYSICS_SHAPE_CAPSULE,
        FORGE_PHYSICS_SHAPE_SPHERE,  FORGE_PHYSICS_SHAPE_BOX,     FORGE_PHYSICS_SHAPE_CAPSULE,
    };

    for (int i = 0; i < S3_NUM_PAIRS; i++) {
        int row = i / 3;
        int col = i % 3;
        float cx = (col - 1) * S3_GRID_SPACING;
        float cy = S3_BASE_Y + row * S3_GRID_SPACING;
        float cz = 0.0f;

        /* Determine offset: separated or overlapping based on toggle */
        float offset = state->gallery_overlap[i]
            ? S3_OVERLAP_OFFSET : S3_PAIR_OFFSET;

        /* Create shape A */
        ForgePhysicsCollisionShape sa;
        switch (types_a[i]) {
        case FORGE_PHYSICS_SHAPE_SPHERE:
            sa = forge_physics_shape_sphere(S3_SPHERE_RADIUS); break;
        case FORGE_PHYSICS_SHAPE_BOX:
            sa = forge_physics_shape_box(vec3_create(S3_BOX_HALF, S3_BOX_HALF, S3_BOX_HALF)); break;
        default:
            sa = forge_physics_shape_capsule(S3_CAPSULE_RADIUS, S3_CAPSULE_HALF_H); break;
        }

        /* Create shape B */
        ForgePhysicsCollisionShape sb;
        switch (types_b[i]) {
        case FORGE_PHYSICS_SHAPE_SPHERE:
            sb = forge_physics_shape_sphere(S3_SPHERE_RADIUS); break;
        case FORGE_PHYSICS_SHAPE_BOX:
            sb = forge_physics_shape_box(vec3_create(S3_BOX_HALF, S3_BOX_HALF, S3_BOX_HALF)); break;
        default:
            sb = forge_physics_shape_capsule(S3_CAPSULE_RADIUS, S3_CAPSULE_HALF_H); break;
        }

        int ia = i * 2;
        int ib = i * 2 + 1;

        init_body(state, ia, vec3_create(cx - offset, cy, cz),
                  sa, 0.0f, DEFAULT_RESTIT,
                  COLOR_DEFAULT_R, COLOR_DEFAULT_G, COLOR_DEFAULT_B);
        init_body(state, ib, vec3_create(cx + offset, cy, cz),
                  sb, 0.0f, DEFAULT_RESTIT,
                  COLOR_DEFAULT_R, COLOR_DEFAULT_G, COLOR_DEFAULT_B);
    }
}

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

    state->accumulator        = 0.0f;
    state->last_contact_count = 0;
    state->gjk_pair_count     = 0;
    state->gjk_hit_count      = 0;
    state->gjk_last_iterations = 0;
    SDL_memset(state->body_gjk_hit, 0, sizeof(state->body_gjk_hit));

    /* Compute initial AABBs */
    for (int i = 0; i < state->num_bodies; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i], state->bodies[i].position,
            state->bodies[i].orientation);
    }

    /* Run initial SAP */
    forge_physics_sap_destroy(&state->sap);
    forge_physics_sap_init(&state->sap);
    state->sap.sweep_axis = forge_physics_sap_select_axis(
        state->cached_aabbs, state->num_bodies);
    forge_physics_sap_update(&state->sap, state->cached_aabbs,
                              state->num_bodies);

    /* Mark bodies in pairs and run initial GJK */
    rebuild_body_in_pair(state);
    run_gjk_on_pairs(state);
}

static void reset_simulation(app_state *state)
{
    /* Restore Scene 3's gallery toggles to their default (all separated) state
     * before rebuilding — gallery_overlap is only zeroed at app startup. */
    SDL_memset(state->gallery_overlap, 0, sizeof(state->gallery_overlap));
    init_current_scene(state);
}

/* ── SAP pair flag helper ────────────────────────────────────────── */

/* Rebuild the body_in_pair flags from the current SAP pair list.
 * Called after SAP update to mark which bodies have overlapping AABBs. */
static void rebuild_body_in_pair(app_state *state)
{
    SDL_memset(state->body_in_pair, 0, sizeof(state->body_in_pair));
    const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(&state->sap);
    int pair_count = forge_physics_sap_pair_count(&state->sap);
    for (int i = 0; i < pair_count; i++) {
        state->body_in_pair[pairs[i].a] = true;
        state->body_in_pair[pairs[i].b] = true;
    }
}

/* ── GJK narrowphase on SAP pairs ────────────────────────────────── */

static void run_gjk_on_pairs(app_state *state)
{
    SDL_memset(state->body_gjk_hit, 0, sizeof(state->body_gjk_hit));
    state->gjk_pair_count      = 0;
    state->gjk_hit_count       = 0;
    state->gjk_last_iterations = 0;

    const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(&state->sap);
    int pair_count = forge_physics_sap_pair_count(&state->sap);

    for (int pi = 0; pi < pair_count; pi++) {
        int a = pairs[pi].a;
        int b = pairs[pi].b;

        /* Bounds check — SAP indices must be within our body array */
        if (a < 0 || a >= state->num_bodies || b < 0 || b >= state->num_bodies)
            continue;

        ForgePhysicsGJKResult result = forge_physics_gjk_intersect(
            &state->shapes[a], state->bodies[a].position,
            state->bodies[a].orientation,
            &state->shapes[b], state->bodies[b].position,
            state->bodies[b].orientation);

        state->gjk_pair_count++;
        if (result.iterations > state->gjk_last_iterations)
            state->gjk_last_iterations = result.iterations;

        if (result.intersecting) {
            state->gjk_hit_count++;
            state->body_gjk_hit[a] = true;
            state->body_gjk_hit[b] = true;
        }
    }

    /* For Scene 1 (two bodies only), also run GJK directly */
    if (state->scene_index == 0 && state->num_bodies == 2) {
        ForgePhysicsGJKResult result = forge_physics_gjk_intersect(
            &state->shapes[0], state->bodies[0].position,
            state->bodies[0].orientation,
            &state->shapes[1], state->bodies[1].position,
            state->bodies[1].orientation);

        state->gjk_pair_count = 1;
        state->gjk_last_iterations = result.iterations;
        state->gjk_hit_count = result.intersecting ? 1 : 0;

        state->body_gjk_hit[0] = result.intersecting;
        state->body_gjk_hit[1] = result.intersecting;
    }
}

/* ── Physics step (Scene 2 only — falling bodies) ────────────────── */

static void physics_step(app_state *state)
{
    vec3 gravity   = vec3_create(0.0f, -DEFAULT_GRAVITY, 0.0f);
    vec3 plane_pt  = vec3_create(0.0f, GROUND_Y, 0.0f);
    vec3 plane_n   = vec3_create(0.0f, 1.0f, 0.0f);

    /* Apply forces */
    for (int i = 0; i < state->num_bodies; i++) {
        forge_physics_rigid_body_apply_gravity(&state->bodies[i], gravity);
    }

    /* Integrate */
    for (int i = 0; i < state->num_bodies; i++) {
        forge_physics_rigid_body_integrate(&state->bodies[i], PHYSICS_DT);
    }

    /* Detect contacts with ground plane */
    ForgePhysicsRBContact contacts[MAX_CONTACTS];
    int num_contacts = 0;

    for (int i = 0; i < state->num_bodies; i++) {
        const ForgePhysicsCollisionShape *shape = &state->shapes[i];

        switch (shape->type) {
        case FORGE_PHYSICS_SHAPE_SPHERE: {
            ForgePhysicsRBContact c;
            if (forge_physics_rb_collide_sphere_plane(
                    &state->bodies[i], i, shape->data.sphere.radius,
                    plane_pt, plane_n, DEFAULT_MU_S, DEFAULT_MU_D, &c)) {
                if (num_contacts < MAX_CONTACTS)
                    contacts[num_contacts++] = c;
            }
            break;
        }
        case FORGE_PHYSICS_SHAPE_BOX: {
            int n = forge_physics_rb_collide_box_plane(
                &state->bodies[i], i, shape->data.box.half_extents,
                plane_pt, plane_n, DEFAULT_MU_S, DEFAULT_MU_D,
                &contacts[num_contacts],
                MAX_CONTACTS - num_contacts);
            num_contacts += n;
            break;
        }
        case FORGE_PHYSICS_SHAPE_CAPSULE: {
            float r = shape->data.capsule.radius;
            float h = shape->data.capsule.half_height;
            vec3 local_y = quat_rotate_vec3(
                state->bodies[i].orientation, vec3_create(0.0f, 1.0f, 0.0f));

            vec3 top_center = vec3_add(state->bodies[i].position,
                vec3_scale(local_y, h));
            ForgePhysicsRigidBody temp_top = state->bodies[i];
            temp_top.position = top_center;
            ForgePhysicsRBContact c_top;
            if (forge_physics_rb_collide_sphere_plane(
                    &temp_top, i, r, plane_pt, plane_n,
                    DEFAULT_MU_S, DEFAULT_MU_D, &c_top)) {
                if (num_contacts < MAX_CONTACTS)
                    contacts[num_contacts++] = c_top;
            }

            vec3 bot_center = vec3_sub(state->bodies[i].position,
                vec3_scale(local_y, h));
            ForgePhysicsRigidBody temp_bot = state->bodies[i];
            temp_bot.position = bot_center;
            ForgePhysicsRBContact c_bot;
            if (forge_physics_rb_collide_sphere_plane(
                    &temp_bot, i, r, plane_pt, plane_n,
                    DEFAULT_MU_S, DEFAULT_MU_D, &c_bot)) {
                if (num_contacts < MAX_CONTACTS)
                    contacts[num_contacts++] = c_bot;
            }
            break;
        }
        default:
            break;
        }
    }

    /* Resolve contacts — the solver changes velocities only, not positions,
     * so AABBs and SAP are computed once after resolution (below). */
    if (num_contacts > 0) {
        forge_physics_rb_resolve_contacts(contacts, num_contacts,
                                           state->bodies, state->num_bodies,
                                           DEFAULT_SOLVER_ITERS, PHYSICS_DT);
    }

    state->last_contact_count = num_contacts;

    /* Recompute AABBs after solver */
    for (int i = 0; i < state->num_bodies; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i], state->bodies[i].position,
            state->bodies[i].orientation);
    }

    /* Re-run SAP on post-solve positions */
    state->sap.sweep_axis = forge_physics_sap_select_axis(
        state->cached_aabbs, state->num_bodies);
    forge_physics_sap_update(&state->sap, state->cached_aabbs,
                              state->num_bodies);

    /* Mark bodies in SAP pairs and run GJK narrowphase */
    rebuild_body_in_pair(state);
    run_gjk_on_pairs(state);
}

/* ── Helper: draw AABB wireframe with overlap coloring ───────────── */

static void draw_aabb_wireframe(app_state *state, ForgePhysicsAABB aabb,
                                 bool in_pair, bool gjk_hit)
{
    vec3 center = forge_physics_aabb_center(aabb);
    vec3 he = forge_physics_aabb_extents(aabb);

    mat4 model = mat4_multiply(
        mat4_translate(center),
        mat4_scale(he));

    float color[4];
    if (gjk_hit) {
        /* Red — GJK confirmed intersection */
        color[0] = COLOR_INTERSECT_R;
        color[1] = COLOR_INTERSECT_G;
        color[2] = COLOR_INTERSECT_B;
        color[3] = AABB_OVERLAP_A;
    } else if (in_pair) {
        /* Orange — SAP broadphase pair (not confirmed by GJK) */
        color[0] = AABB_OVERLAP_R;
        color[1] = AABB_OVERLAP_G;
        color[2] = AABB_OVERLAP_B;
        color[3] = AABB_OVERLAP_A;
    } else {
        /* Green — no overlap */
        color[0] = AABB_COLOR_R;
        color[1] = AABB_COLOR_G;
        color[2] = AABB_COLOR_B;
        color[3] = AABB_COLOR_A;
    }

    forge_scene_draw_mesh_ex(&state->scene, state->wireframe_pipeline,
        state->cube_vb, state->cube_ib,
        state->cube_index_count, model, color);
}

/* ── Helper: find the capsule mesh matching a shape's aspect ratio ── */

static int find_capsule_mesh(const app_state *state,
                              const ForgePhysicsCollisionShape *shape)
{
    float aspect = shape->data.capsule.half_height / shape->data.capsule.radius;
    for (int i = 0; i < state->num_capsule_meshes; i++) {
        if (SDL_fabsf(state->capsule_meshes[i].aspect - aspect) < 0.01f)
            return i;
    }
    return 0; /* fallback to first mesh */
}

/* ── Helper: draw a shape body (shadow pass) ─────────────────────── */

static void draw_body_shadow(app_state *state, int idx, float alpha)
{
    const ForgePhysicsCollisionShape *shape = &state->shapes[idx];
    mat4 model = get_body_model_matrix(&state->bodies[idx], shape,
                                         alpha);

    switch (shape->type) {
    case FORGE_PHYSICS_SHAPE_SPHERE:
        forge_scene_draw_shadow_mesh(&state->scene,
            state->sphere_vb, state->sphere_ib,
            state->sphere_index_count, model);
        break;
    case FORGE_PHYSICS_SHAPE_BOX:
        forge_scene_draw_shadow_mesh(&state->scene,
            state->cube_vb, state->cube_ib,
            state->cube_index_count, model);
        break;
    case FORGE_PHYSICS_SHAPE_CAPSULE: {
        int ci = find_capsule_mesh(state, shape);
        forge_scene_draw_shadow_mesh(&state->scene,
            state->capsule_meshes[ci].vb, state->capsule_meshes[ci].ib,
            state->capsule_meshes[ci].index_count, model);
        break;
    }
    default:
        break;
    }
}

/* ── Helper: draw a shape body (main pass) ───────────────────────── */

static void draw_body_main(app_state *state, int idx, float alpha)
{
    const ForgePhysicsCollisionShape *shape = &state->shapes[idx];
    mat4 model = get_body_model_matrix(&state->bodies[idx], shape,
                                         alpha);

    /* If GJK confirmed intersection, tint red; otherwise use body color */
    float color[4];
    if (state->body_gjk_hit[idx]) {
        color[0] = COLOR_INTERSECT_R;
        color[1] = COLOR_INTERSECT_G;
        color[2] = COLOR_INTERSECT_B;
        color[3] = 1.0f;
    } else {
        color[0] = state->body_colors[idx][0];
        color[1] = state->body_colors[idx][1];
        color[2] = state->body_colors[idx][2];
        color[3] = state->body_colors[idx][3];
    }

    switch (shape->type) {
    case FORGE_PHYSICS_SHAPE_SPHERE:
        forge_scene_draw_mesh(&state->scene,
            state->sphere_vb, state->sphere_ib,
            state->sphere_index_count, model, color);
        break;
    case FORGE_PHYSICS_SHAPE_BOX:
        forge_scene_draw_mesh(&state->scene,
            state->cube_vb, state->cube_ib,
            state->cube_index_count, model, color);
        break;
    case FORGE_PHYSICS_SHAPE_CAPSULE: {
        int ci = find_capsule_mesh(state, shape);
        forge_scene_draw_mesh(&state->scene,
            state->capsule_meshes[ci].vb, state->capsule_meshes[ci].ib,
            state->capsule_meshes[ci].index_count, model, color);
        break;
    }
    default:
        break;
    }
}

/* ── Switch to a scene by index ──────────────────────────────────── */

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
            /* Scene 3 gallery toggles: number keys toggle overlap */
            case SDL_SCANCODE_1:
            case SDL_SCANCODE_2:
            case SDL_SCANCODE_3:
            case SDL_SCANCODE_4:
            case SDL_SCANCODE_5:
            case SDL_SCANCODE_6:
            case SDL_SCANCODE_7:
            case SDL_SCANCODE_8:
            case SDL_SCANCODE_9:
                if (state->scene_index == 2) {
                    int idx = event->key.scancode - SDL_SCANCODE_1;
                    if (idx >= 0 && idx < S3_NUM_PAIRS) {
                        state->gallery_overlap[idx] =
                            !state->gallery_overlap[idx];
                        init_current_scene(state);
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

    /* ── Scene 1: move body A with arrow keys ──────────────────────── */
    if (state->scene_index == 0) {
        vec3 move = vec3_create(0.0f, 0.0f, 0.0f);
        if (state->move_left)  move.x -= 1.0f;
        if (state->move_right) move.x += 1.0f;
        if (state->move_fwd)   move.z -= 1.0f;
        if (state->move_back)  move.z += 1.0f;

        if (vec3_length(move) > MOVE_INPUT_EPSILON) {
            move = vec3_scale(vec3_normalize(move), S1_MOVE_SPEED * dt);
            state->bodies[0].position = vec3_add(
                state->bodies[0].position, move);
            state->bodies[0].prev_position = state->bodies[0].position;
        }

        /* Recompute AABBs and run GJK for Scene 1 */
        for (int i = 0; i < state->num_bodies; i++) {
            state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
                &state->shapes[i], state->bodies[i].position,
                state->bodies[i].orientation);
        }
        forge_physics_sap_update(&state->sap, state->cached_aabbs,
                                  state->num_bodies);
        rebuild_body_in_pair(state);
        run_gjk_on_pairs(state);
    }

    /* ── Scene 2: fixed-timestep physics ──────────────────────────── */
    if (state->scene_index == 1 && !state->paused) {
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
    if (state->scene_index != 1 || state->paused) alpha = 1.0f;

    /* ── Shadow pass ─────────────────────────────────────────────── */

    forge_scene_begin_shadow_pass(s);
    for (int i = 0; i < state->num_bodies; i++) {
        draw_body_shadow(state, i, alpha);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */

    forge_scene_begin_main_pass(s);

    for (int i = 0; i < state->num_bodies; i++) {
        draw_body_main(state, i, alpha);
    }

    /* Draw AABB wireframes if enabled */
    if (state->show_aabbs) {
        for (int i = 0; i < state->num_bodies; i++) {
            vec3 interp_pos = vec3_lerp(
                state->bodies[i].prev_position,
                state->bodies[i].position, alpha);
            quat interp_orient = quat_slerp(
                state->bodies[i].prev_orientation,
                state->bodies[i].orientation, alpha);
            ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(
                &state->shapes[i], interp_pos, interp_orient);
            draw_aabb_wireframe(state, aabb,
                state->body_in_pair[i], state->body_gjk_hit[i]);
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
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "GJK Intersection",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* Scene info */
                {
                    const char *scene_names[NUM_SCENES] = {
                        "1: Two-Body", "2: SAP+GJK", "3: Gallery"
                    };
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Scene: %s",
                                 scene_names[state->scene_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                if (forge_ui_ctx_button_layout(ui, "Next Scene (Tab)",
                                                BUTTON_HEIGHT)) {
                    set_scene(state, (state->scene_index + 1) % NUM_SCENES);
                }

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* GJK stats */
                {
                    char buf[80];
                    SDL_snprintf(buf, sizeof(buf), "Bodies: %d",
                                 state->num_bodies);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "SAP pairs: %d",
                                 forge_physics_sap_pair_count(&state->sap));
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "GJK tested: %d",
                                 state->gjk_pair_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "GJK hits: %d",
                                 state->gjk_hit_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Max iters: %d",
                                 state->gjk_last_iterations);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    if (state->scene_index == 1) {
                        SDL_snprintf(buf, sizeof(buf), "Contacts: %d",
                                     state->last_contact_count);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                }

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Controls */
                forge_ui_ctx_checkbox_layout(ui, "Show AABBs (V)",
                                             &state->show_aabbs, LABEL_HEIGHT);
                forge_ui_ctx_checkbox_layout(ui, "Paused (P)",
                                             &state->paused, LABEL_HEIGHT);
                {
                    bool slow = (state->speed_scale < SLOW_MOTION_THRESH);
                    if (forge_ui_ctx_checkbox_layout(ui, "Slow Motion (T)",
                                                      &slow, LABEL_HEIGHT)) {
                        state->speed_scale = slow
                            ? SLOW_MOTION_SCALE : NORMAL_SPEED_SCALE;
                    }
                }

                if (forge_ui_ctx_button_layout(ui, "Reset (R)",
                                                BUTTON_HEIGHT)) {
                    reset_simulation(state);
                }

                /* Scene-specific hints */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);
                if (state->scene_index == 0) {
                    forge_ui_ctx_label_layout(ui, "Arrow keys: move A",
                                               LABEL_HEIGHT);
                } else if (state->scene_index == 2) {
                    forge_ui_ctx_label_layout(ui, "1-9: toggle overlap",
                                               LABEL_HEIGHT);
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

                forge_ui_wctx_window_end(wctx);
            }
        }
    }
    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
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
        "Physics Lesson 09 \xe2\x80\x94 GJK Intersection Testing");
    cfg.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";
    cfg.font_size       = 16.0f;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("ERROR: forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* Wireframe pipeline for AABB visualization */
    state->wireframe_pipeline = forge_scene_create_pipeline(
        &state->scene, SDL_GPU_CULLMODE_NONE, SDL_GPU_FILLMODE_LINE);
    if (!state->wireframe_pipeline) {
        SDL_Log("ERROR: Failed to create wireframe pipeline: %s",
                SDL_GetError());
        goto init_fail;
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

    /* Generate capsule meshes — one per distinct aspect ratio (half_h/radius)
     * so we can scale uniformly by radius without distorting the caps. */
    {
        /* Aspect ratios for all capsule shapes used in this lesson:
         * S2: half_h/radius = 0.35/0.25 = 1.4
         * S3: half_h/radius = 0.3/0.3   = 1.0 */
        const float capsule_aspects[] = {
            S2_CAPSULE_HALF_H / S2_CAPSULE_RADIUS,
            S3_CAPSULE_HALF_H / S3_CAPSULE_RADIUS,
        };
        int num_aspects = (int)(sizeof(capsule_aspects) / sizeof(capsule_aspects[0]));
        state->num_capsule_meshes = 0;

        for (int ci = 0; ci < num_aspects && ci < MAX_CAPSULE_MESHES; ci++) {
            /* Skip duplicate aspects (e.g. if S2 and S3 happen to match) */
            bool dup = false;
            for (int j = 0; j < state->num_capsule_meshes; j++) {
                if (SDL_fabsf(state->capsule_meshes[j].aspect - capsule_aspects[ci]) < 0.01f) {
                    dup = true;
                    break;
                }
            }
            if (dup) continue;

            ForgeShape cap = forge_shapes_capsule(
                CAPSULE_SLICES, CAPSULE_STACKS, CAPSULE_CAP_STACKS,
                capsule_aspects[ci]);
            if (cap.vertex_count == 0) {
                SDL_Log("ERROR: forge_shapes_capsule failed for aspect %.2f",
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
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    /* All gallery pairs start separated */
    SDL_memset(state->gallery_overlap, 0, sizeof(state->gallery_overlap));

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
