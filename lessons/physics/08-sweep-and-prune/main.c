/*
 * Physics Lesson 08 — Sweep-and-Prune Broadphase
 *
 * Demonstrates: sort-and-sweep broadphase collision detection using
 * ForgePhysicsSAPWorld. Projects AABBs onto a single axis, insertion-sorts
 * the endpoints, and sweeps to find overlapping pairs in near-linear time.
 *
 * Two selectable scenes:
 *   1. Falling Objects — 40 spheres fall with gravity, bounce off ground.
 *      SAP runs each physics step. AABB wireframes shown (green default,
 *      orange for bodies in overlapping pairs). UI shows pair count vs
 *      brute-force count.
 *   2. Axis Visualization — 12 bodies at scripted positions. Number line shows
 *      sorted endpoints on the sweep axis. Interval bars colored per body.
 *
 * Controls:
 *   WASD              — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   1-2               — select scene
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

/* Physics library — rigid bodies, contacts, collision shapes, SAP broadphase */
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
#define MAX_BODIES           48

/* Scene 1: Falling Objects — spheres dropping with SAP broadphase */
#define S1_NUM_BODIES        40
#define S1_MASS              5.0f
#define S1_SPHERE_RADIUS     0.4f
#define S1_DROP_BASE         5.0f
#define S1_DROP_RANGE       15.0f
#define S1_SPREAD_X          8.0f
#define S1_SPREAD_Z          8.0f

/* Scene 2: Axis Visualization — scripted positions for endpoint display */
#define S2_NUM_BODIES       12
#define S2_BODY_Y            1.0f
/* Contact buffer — 40 spheres produce up to 40 ground contacts plus
 * sphere-sphere contacts from SAP pairs. 256 gives comfortable headroom. */
#define MAX_CONTACTS         256

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
#define PANEL_H            520.0f
#define LABEL_HEIGHT        24.0f
#define BUTTON_HEIGHT       30.0f

/* Number of scenes */
#define NUM_SCENES           2

/* AABB wireframe colors */
#define AABB_COLOR_R         0.2f
#define AABB_COLOR_G         0.9f
#define AABB_COLOR_B         0.2f
#define AABB_COLOR_A         0.25f

/* AABB overlap highlight color (orange) */
#define AABB_OVERLAP_R       0.95f
#define AABB_OVERLAP_G       0.6f
#define AABB_OVERLAP_B       0.1f
#define AABB_OVERLAP_A       0.35f

/* Shape colors */
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
    SDL_GPUBuffer *cube_vb;          /* cube vertex buffer */
    SDL_GPUBuffer *cube_ib;          /* cube index buffer */
    Uint32         cube_index_count; /* number of cube indices */

    SDL_GPUBuffer *sphere_vb;          /* sphere vertex buffer */
    SDL_GPUBuffer *sphere_ib;          /* sphere index buffer */
    Uint32         sphere_index_count; /* number of sphere indices */

    SDL_GPUBuffer *capsule_vb;          /* capsule vertex buffer */
    SDL_GPUBuffer *capsule_ib;          /* capsule index buffer */
    Uint32         capsule_index_count; /* number of capsule indices */
    float          capsule_mesh_half_h; /* half-height used to generate the mesh (unit) */

    /* Physics state */
    ForgePhysicsRigidBody      bodies[MAX_BODIES];      /* rigid body state per body */
    ForgePhysicsCollisionShape shapes[MAX_BODIES];       /* collision shape per body */
    float                      body_colors[MAX_BODIES][4]; /* RGBA color per body */
    int                        num_bodies;               /* active body count */

    /* Cached AABBs (updated each physics step) */
    ForgePhysicsAABB           cached_aabbs[MAX_BODIES];

    /* SAP broadphase */
    ForgePhysicsSAPWorld       sap;

    /* Brute-force comparison count (for UI) */
    int brute_force_pairs;

    /* Contact state (for UI readout) */
    int last_contact_count;

    /* Simulation control */
    int   scene_index;   /* 0 = falling objects, 1 = axis visualization */
    float accumulator;   /* fixed-timestep accumulator (seconds) */
    bool  paused;        /* true when simulation is paused */
    float speed_scale;   /* time scale: 1.0 = normal, 0.25 = slow motion */

    /* AABB visualization toggle */
    bool show_aabbs;

    /* Which bodies are in an overlapping SAP pair (for AABB coloring) */
    bool body_in_pair[MAX_BODIES];

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

static vec3 shape_render_scale(const ForgePhysicsCollisionShape *shape,
                                float capsule_mesh_half_h)
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
        float h = shape->data.capsule.half_height;
        float mesh_total_half = capsule_mesh_half_h + 1.0f;
        float shape_total_half = h + r;
        return vec3_create(r, shape_total_half / mesh_total_half, r);
    }
    default:
        return vec3_create(1, 1, 1);
    }
}

/* ── Helper: interpolated model matrix ────────────────────────────── */

static mat4 get_body_model_matrix(const ForgePhysicsRigidBody *rb,
                                    const ForgePhysicsCollisionShape *shape,
                                    float capsule_mesh_half_h,
                                    float alpha)
{
    vec3 pos = vec3_lerp(rb->prev_position, rb->position, alpha);
    quat orient = quat_slerp(rb->prev_orientation, rb->orientation, alpha);
    mat4 translation = mat4_translate(pos);
    mat4 rotation    = quat_to_mat4(orient);
    vec3 sc = shape_render_scale(shape, capsule_mesh_half_h);
    mat4 scale       = mat4_scale(sc);
    return mat4_multiply(translation, mat4_multiply(rotation, scale));
}

/* ── Helper: brute-force pair count ──────────────────────────────── */

static int brute_force_count(const ForgePhysicsAABB *aabbs, int count)
{
    int n = 0;
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (forge_physics_aabb_overlap(aabbs[i], aabbs[j]))
                n++;
        }
    }
    return n;
}

/* ── Helper: simple deterministic pseudo-random ──────────────────── */

static uint32_t lcg_state = SPAWN_SEED;

static float rand_float(float lo, float hi)
{
    /* Linear congruential generator — deterministic, not cryptographic */
    lcg_state = lcg_state * LCG_MULTIPLIER + LCG_INCREMENT;
    float t = (float)(lcg_state >> LCG_SHIFT) / (float)LCG_DIVISOR;
    return lo + t * (hi - lo);
}

/* ── Scene initialization ────────────────────────────────────────── */

/* Scene 1: Falling Objects — 40 spheres with random spawn positions */
static void init_scene_1(app_state *state)
{
    state->num_bodies = S1_NUM_BODIES;
    lcg_state = SPAWN_SEED;  /* reset for determinism */

    for (int i = 0; i < S1_NUM_BODIES; i++) {
        float x = rand_float(-S1_SPREAD_X, S1_SPREAD_X);
        float y = S1_DROP_BASE + rand_float(0.0f, S1_DROP_RANGE);
        float z = rand_float(-S1_SPREAD_Z, S1_SPREAD_Z);

        int ci = i % NUM_BODY_COLORS;

        /* Sphere-only: the narrowphase currently handles sphere-sphere
         * collisions. Using only spheres ensures every broadphase overlap
         * is resolved, so the visualization stays consistent. */
        ForgePhysicsCollisionShape shape =
            forge_physics_shape_sphere(S1_SPHERE_RADIUS);

        init_body(state, i, vec3_create(x, y, z),
                  shape, S1_MASS, DEFAULT_RESTIT,
                  BODY_PALETTE[ci][0], BODY_PALETTE[ci][1],
                  BODY_PALETTE[ci][2]);
    }
}

/* Scene 2: Axis Visualization — 12 bodies at scripted positions */
static void init_scene_2(app_state *state)
{
    state->num_bodies = S2_NUM_BODIES;

    /* Scripted X positions — some overlapping, some separated */
    float positions[][3] = {
        {-5.0f, S2_BODY_Y,  0.0f},
        {-4.0f, S2_BODY_Y,  1.0f},
        {-2.0f, S2_BODY_Y, -1.0f},
        {-1.0f, S2_BODY_Y,  0.5f},
        { 0.0f, S2_BODY_Y,  0.0f},
        { 0.5f, S2_BODY_Y, -0.5f},
        { 2.0f, S2_BODY_Y,  0.0f},
        { 3.0f, S2_BODY_Y,  1.0f},
        { 3.5f, S2_BODY_Y, -1.0f},
        { 5.0f, S2_BODY_Y,  0.0f},
        { 6.0f, S2_BODY_Y,  0.5f},
        { 8.0f, S2_BODY_Y, -0.5f},
    };

    /* no local palette — use BODY_PALETTE with modulo */

    for (int i = 0; i < S2_NUM_BODIES; i++) {
        /* All spheres for simplicity in visualization scene */
        init_body(state, i,
            vec3_create(positions[i][0], positions[i][1], positions[i][2]),
            forge_physics_shape_sphere(S1_SPHERE_RADIUS),
            0.0f, DEFAULT_RESTIT,  /* static bodies — no gravity */
            BODY_PALETTE[i % NUM_BODY_COLORS][0],
            BODY_PALETTE[i % NUM_BODY_COLORS][1],
            BODY_PALETTE[i % NUM_BODY_COLORS][2]);
    }
}

static void init_current_scene(app_state *state)
{
    switch (state->scene_index) {
    case 0: init_scene_1(state); break;
    case 1: init_scene_2(state); break;
    default:
        state->scene_index = 0;
        init_scene_1(state);
        break;
    }

    state->accumulator        = 0.0f;
    state->last_contact_count = 0;
    state->brute_force_pairs  = 0;

    /* Compute initial AABBs */
    for (int i = 0; i < state->num_bodies; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i], state->bodies[i].position,
            state->bodies[i].orientation);
    }

    /* Run initial SAP — destroy first in case this is a reset/scene change */
    forge_physics_sap_destroy(&state->sap);
    forge_physics_sap_init(&state->sap);
    state->sap.sweep_axis = forge_physics_sap_select_axis(
        state->cached_aabbs, state->num_bodies);
    forge_physics_sap_update(&state->sap, state->cached_aabbs,
                              state->num_bodies);
    state->brute_force_pairs = brute_force_count(state->cached_aabbs,
                                                  state->num_bodies);

    /* Mark bodies in pairs */
    SDL_memset(state->body_in_pair, 0, sizeof(state->body_in_pair));
    const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(&state->sap);
    for (int i = 0; i < forge_physics_sap_pair_count(&state->sap); i++) {
        state->body_in_pair[pairs[i].a] = true;
        state->body_in_pair[pairs[i].b] = true;
    }
}

static void reset_simulation(app_state *state)
{
    init_current_scene(state);
}

/* ── Physics step ────────────────────────────────────────────────── */

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

    /* Detect contacts with ground plane — dispatch based on shape type.
     * Use a local buffer sized for this lesson's body count. */
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
            /* Two-sphere approximation for capsule-plane contact */
            float r = shape->data.capsule.radius;
            float h = shape->data.capsule.half_height;
            vec3 local_y = quat_rotate_vec3(
                state->bodies[i].orientation, vec3_create(0, 1, 0));

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

    /* Update AABBs for narrowphase sphere-sphere tests */
    for (int i = 0; i < state->num_bodies; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i], state->bodies[i].position,
            state->bodies[i].orientation);
    }

    /* SAP broadphase — find overlapping pairs */
    state->sap.sweep_axis = forge_physics_sap_select_axis(
        state->cached_aabbs, state->num_bodies);
    forge_physics_sap_update(&state->sap, state->cached_aabbs,
                              state->num_bodies);

    /* Narrowphase: test sphere-sphere for SAP pairs */
    const ForgePhysicsSAPPair *sap_pairs = forge_physics_sap_get_pairs(
        &state->sap);
    int sap_count = forge_physics_sap_pair_count(&state->sap);
    for (int pi = 0; pi < sap_count &&
         num_contacts < MAX_CONTACTS; pi++) {
        int a = sap_pairs[pi].a;
        int b = sap_pairs[pi].b;
        if (state->shapes[a].type != FORGE_PHYSICS_SHAPE_SPHERE) continue;
        if (state->shapes[b].type != FORGE_PHYSICS_SHAPE_SPHERE) continue;
        ForgePhysicsRBContact c;
        if (forge_physics_rb_collide_sphere_sphere(
                &state->bodies[a], a, state->shapes[a].data.sphere.radius,
                &state->bodies[b], b, state->shapes[b].data.sphere.radius,
                DEFAULT_MU_S, DEFAULT_MU_D, &c)) {
            contacts[num_contacts++] = c;
        }
    }

    /* Resolve contacts */
    if (num_contacts > 0) {
        forge_physics_rb_resolve_contacts(contacts, num_contacts,
                                           state->bodies, state->num_bodies,
                                           DEFAULT_SOLVER_ITERS, PHYSICS_DT);
    }

    state->last_contact_count = num_contacts;

    /* Recompute AABBs after solver — visualization must match post-solve state */
    for (int i = 0; i < state->num_bodies; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i], state->bodies[i].position,
            state->bodies[i].orientation);
    }

    /* Re-run SAP on post-solve positions for accurate visualization */
    state->sap.sweep_axis = forge_physics_sap_select_axis(
        state->cached_aabbs, state->num_bodies);
    forge_physics_sap_update(&state->sap, state->cached_aabbs,
                              state->num_bodies);
    state->brute_force_pairs = brute_force_count(state->cached_aabbs,
                                                  state->num_bodies);

    /* Mark bodies in pairs for AABB coloring (post-solve) */
    SDL_memset(state->body_in_pair, 0, sizeof(state->body_in_pair));
    sap_pairs = forge_physics_sap_get_pairs(&state->sap);
    sap_count = forge_physics_sap_pair_count(&state->sap);
    for (int pi = 0; pi < sap_count; pi++) {
        state->body_in_pair[sap_pairs[pi].a] = true;
        state->body_in_pair[sap_pairs[pi].b] = true;
    }
}

/* ── Helper: draw AABB wireframe with overlap coloring ───────────── */

static void draw_aabb_wireframe(app_state *state, ForgePhysicsAABB aabb,
                                 bool in_pair)
{
    vec3 center = forge_physics_aabb_center(aabb);
    vec3 he = forge_physics_aabb_extents(aabb);

    mat4 model = mat4_multiply(
        mat4_translate(center),
        mat4_scale(he));

    float color[4];
    if (in_pair) {
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

/* ── Helper: draw a shape body (shadow pass) ─────────────────────── */

static void draw_body_shadow(app_state *state, int idx, float alpha)
{
    const ForgePhysicsCollisionShape *shape = &state->shapes[idx];
    mat4 model = get_body_model_matrix(&state->bodies[idx], shape,
                                         state->capsule_mesh_half_h, alpha);

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
    case FORGE_PHYSICS_SHAPE_CAPSULE:
        forge_scene_draw_shadow_mesh(&state->scene,
            state->capsule_vb, state->capsule_ib,
            state->capsule_index_count, model);
        break;
    default:
        break;
    }
}

/* ── Helper: draw a shape body (main pass) ───────────────────────── */

static void draw_body_main(app_state *state, int idx, float alpha)
{
    const ForgePhysicsCollisionShape *shape = &state->shapes[idx];
    mat4 model = get_body_model_matrix(&state->bodies[idx], shape,
                                         state->capsule_mesh_half_h, alpha);

    switch (shape->type) {
    case FORGE_PHYSICS_SHAPE_SPHERE:
        forge_scene_draw_mesh(&state->scene,
            state->sphere_vb, state->sphere_ib,
            state->sphere_index_count, model,
            state->body_colors[idx]);
        break;
    case FORGE_PHYSICS_SHAPE_BOX:
        forge_scene_draw_mesh(&state->scene,
            state->cube_vb, state->cube_ib,
            state->cube_index_count, model,
            state->body_colors[idx]);
        break;
    case FORGE_PHYSICS_SHAPE_CAPSULE:
        forge_scene_draw_mesh(&state->scene,
            state->capsule_vb, state->capsule_ib,
            state->capsule_index_count, model,
            state->body_colors[idx]);
        break;
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
        draw_body_shadow(state, i, alpha);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */

    forge_scene_begin_main_pass(s);

    /* Draw all bodies */
    for (int i = 0; i < state->num_bodies; i++) {
        draw_body_main(state, i, alpha);
    }

    /* Draw AABB wireframes if enabled — color depends on SAP overlap */
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
            draw_aabb_wireframe(state, aabb, state->body_in_pair[i]);
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
            if (forge_ui_wctx_window_begin(wctx, "Sweep-and-Prune",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* Scene selection */
                {
                    const char *scene_names[NUM_SCENES] = {
                        "1: Falling", "2: Axis Viz"
                    };
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Scene: %s",
                                 scene_names[state->scene_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                if (forge_ui_ctx_button_layout(ui, "1: Falling Objects",
                                                BUTTON_HEIGHT))
                    set_scene(state, 0);
                if (forge_ui_ctx_button_layout(ui, "2: Axis Visualization",
                                                BUTTON_HEIGHT))
                    set_scene(state, 1);

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* SAP stats */
                {
                    char buf[80];
                    SDL_snprintf(buf, sizeof(buf), "Bodies: %d",
                                 state->num_bodies);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "SAP pairs: %d",
                                 forge_physics_sap_pair_count(&state->sap));
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Brute-force: %d",
                                 state->brute_force_pairs);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Sweep axis: %c",
                                 "XYZ"[state->sap.sweep_axis]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Sort ops: %d",
                                 state->sap.sort_ops);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Contacts: %d",
                                 state->last_contact_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* AABB toggle */
                forge_ui_ctx_checkbox_layout(ui, "Show AABBs (V)",
                                             &state->show_aabbs, LABEL_HEIGHT);

                /* Pause checkbox */
                forge_ui_ctx_checkbox_layout(ui, "Paused (P)",
                                             &state->paused, LABEL_HEIGHT);

                /* Slow motion checkbox */
                {
                    bool slow = (state->speed_scale < SLOW_MOTION_THRESH);
                    if (forge_ui_ctx_checkbox_layout(ui, "Slow Motion (T)",
                                                      &slow, LABEL_HEIGHT)) {
                        state->speed_scale = slow
                            ? SLOW_MOTION_SCALE : NORMAL_SPEED_SCALE;
                    }
                }

                /* Reset button */
                if (forge_ui_ctx_button_layout(ui, "Reset (R)",
                                                BUTTON_HEIGHT)) {
                    reset_simulation(state);
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
        case SDL_SCANCODE_V:
            state->show_aabbs = !state->show_aabbs;
            SDL_Log("AABB visualization: %s",
                    state->show_aabbs ? "ON" : "OFF");
            break;
        case SDL_SCANCODE_1: set_scene(state, 0); break;
        case SDL_SCANCODE_2: set_scene(state, 1); break;
        default:
            break;
        }
    }

    return SDL_APP_CONTINUE;
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
        "Physics Lesson 08 \xe2\x80\x94 Sweep-and-Prune Broadphase");
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

    /* Generate and upload capsule geometry */
    state->capsule_mesh_half_h = 1.0f;
    ForgeShape capsule = forge_shapes_capsule(
        CAPSULE_SLICES, CAPSULE_STACKS, CAPSULE_CAP_STACKS,
        state->capsule_mesh_half_h);
    if (capsule.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_capsule failed");
        goto init_fail;
    }
    state->capsule_vb = upload_shape_vb(&state->scene, &capsule);
    state->capsule_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, capsule.indices,
        (Uint32)capsule.index_count * (Uint32)sizeof(uint32_t));
    state->capsule_index_count = (Uint32)capsule.index_count;
    forge_shapes_free(&capsule);
    if (!state->capsule_vb || !state->capsule_ib) {
        SDL_Log("ERROR: Failed to upload capsule geometry");
        goto init_fail;
    }

    /* Default state */
    state->scene_index   = 0;
    state->accumulator   = 0.0f;
    state->paused        = false;
    state->speed_scale   = NORMAL_SPEED_SCALE;
    state->show_aabbs    = true;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    init_current_scene(state);

    return SDL_APP_CONTINUE;

init_fail:
    /* SDL calls SDL_AppQuit after SDL_AppInit returns failure (because
     * *appstate is already set), so let SDL_AppQuit handle all GPU buffer,
     * SAP, and scene cleanup — releasing here would double-free. */
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
        if (state->capsule_vb) SDL_ReleaseGPUBuffer(dev, state->capsule_vb);
        if (state->capsule_ib) SDL_ReleaseGPUBuffer(dev, state->capsule_ib);
    }

    forge_physics_sap_destroy(&state->sap);
    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
