/*
 * Physics Lesson 11 — Contact Manifold
 *
 * Demonstrates: contact manifold generation and persistent caching.
 * A manifold collects all contact points between two shapes in a single
 * collision — up to four points for stable, non-rocking stacking. Persistent
 * manifolds carry warm-start impulses across frames, which reduces solver
 * iterations and improves stacking stability.
 *
 * Two selectable scenes:
 *   1. Two-Body Manifold Inspector — Move shape A with arrow keys. When
 *      shapes overlap, the manifold generator computes up to four contact
 *      points. Each point is visualized as a small sphere on the contact
 *      surface; the shared contact normal is drawn as an arrow. The UI
 *      panel shows manifold count, each contact's penetration depth, and
 *      warm-start impulse accumulation across frames.
 *   2. Multi-Body Stacking — 15 mixed bodies with a full SAP -> GJK -> EPA
 *      -> manifold pipeline. A manifold cache persists contacts across
 *      frames. Toggle the manifold cache on/off with M to compare stability.
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
 *   M                 — toggle manifold cache (Scene 2)
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

/* Physics library — rigid bodies, contacts, collision shapes, SAP, GJK, EPA,
 * manifold generation and persistent caching */
#include "physics/forge_physics.h"

/* Procedural geometry — sphere, cube, capsule meshes */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Physics simulation */
#define PHYSICS_DT           (1.0f / 60.0f)  /* fixed physics timestep in seconds */
#define MAX_DELTA_TIME        0.1f            /* cap frame dt to avoid spiral of death */

/* Scene limits */
#define MAX_BODIES            64              /* maximum rigid bodies across both scenes */
#define MAX_CONTACTS          256             /* maximum contacts per physics step */

/* Scene 1: Two-Body Manifold Inspector */
#define S1_MOVE_SPEED         5.0f           /* arrow-key translation speed (m/s) */
#define S1_BODY_A_X          -1.5f           /* initial X position of shape A */
#define S1_BODY_A_Y           1.5f           /* initial Y position of shape A */
#define S1_BODY_B_X           1.5f           /* initial X position of shape B */
#define S1_BODY_B_Y           1.5f           /* initial Y position of shape B */
#define S1_SPHERE_RADIUS      0.8f           /* sphere collision radius (Scene 1) */
#define S1_BOX_HALF           0.7f           /* box half-extent on each axis (Scene 1) */
#define S1_CAPSULE_RADIUS     0.4f           /* capsule cross-section radius (Scene 1) */
#define S1_CAPSULE_HALF_H     0.5f           /* capsule half-height along axis (Scene 1) */

/* Shape pair configurations for Scene 1 */
#define S1_NUM_PAIRS          6              /* number of selectable shape pair types */

/* Scene 2: SAP + GJK + EPA + Manifold Pipeline */
#define S2_NUM_BODIES         15             /* number of dynamic bodies in Scene 2 */
#define S2_MASS               5.0f           /* mass of each body in Scene 2 (kg) */
#define S2_SPHERE_RADIUS      0.5f           /* sphere radius for Scene 2 bodies */
#define S2_BOX_HALF           0.45f          /* box half-extent for Scene 2 bodies */
#define S2_CAPSULE_RADIUS     0.3f           /* capsule radius for Scene 2 bodies */
#define S2_CAPSULE_HALF_H     0.4f           /* capsule half-height for Scene 2 bodies */
#define S2_DROP_BASE          5.0f           /* minimum drop height above ground (m) */
#define S2_DROP_RANGE        12.0f           /* additional random height range (m) */
#define S2_SPREAD_X           6.0f           /* lateral spawn spread on X axis (m) */
#define S2_SPREAD_Z           6.0f           /* lateral spawn spread on Z axis (m) */

/* Common physics parameters */
#define GROUND_Y              0.0f           /* ground plane Y coordinate */
#define DEFAULT_DAMPING       0.99f          /* linear velocity damping per step */
#define DEFAULT_ANG_DAMPING   0.99f          /* angular velocity damping per step */
#define DEFAULT_RESTIT        0.3f           /* coefficient of restitution */
#define DEFAULT_MU_S          0.6f           /* static friction coefficient */
#define DEFAULT_MU_D          0.4f           /* dynamic friction coefficient */
#define DEFAULT_GRAVITY       9.81f          /* gravitational acceleration (m/s^2) */
#define DEFAULT_SOLVER_ITERS  10             /* impulse solver iterations per step */

/* Speed control */
#define SLOW_MOTION_SCALE     0.25f          /* time scale when slow motion is active */
#define NORMAL_SPEED_SCALE    1.0f           /* normal time scale */
#define SLOW_MOTION_THRESH    0.5f           /* midpoint for display label decision */

/* Camera */
#define CAM_START_X           0.0f           /* camera start position X */
#define CAM_START_Y           6.0f           /* camera start position Y */
#define CAM_START_Z          16.0f           /* camera start position Z (back from origin) */
#define CAM_START_PITCH      -0.2f           /* slight downward look angle (radians) */

/* Mesh resolution */
#define SPHERE_SLICES        16              /* longitude subdivisions for sphere mesh */
#define SPHERE_STACKS         8              /* latitude subdivisions for sphere mesh */
#define CUBE_SLICES           1              /* not subdivided — cube needs 1 pass */
#define CUBE_STACKS           1
#define CAPSULE_SLICES       12              /* longitude subdivisions for capsule */
#define CAPSULE_STACKS        4              /* body ring subdivisions for capsule */
#define CAPSULE_CAP_STACKS    4              /* hemisphere subdivisions for capsule ends */

/* Contact visualization — small sphere placed at each manifold contact point */
#define MARKER_RADIUS         0.07f          /* radius of the contact point indicator */
#define MARKER_SLICES         8              /* longitude resolution of the marker sphere */
#define MARKER_STACKS         4              /* latitude resolution of the marker sphere */
/* Arrow shaft — thin cube scaled to represent the contact normal direction */
#define ARROW_THICKNESS       0.03f          /* half-thickness of the arrow shaft cube */
#define ARROW_MIN_LENGTH      0.001f         /* skip drawing the arrow below this length */
#define ARROW_MIN_VISIBLE     0.3f           /* minimum visual scale for readability */
#define ARROW_PARALLEL_DOT    0.999f         /* dot threshold: direction nearly equals up */

/* UI panel layout */
#define PANEL_X              10.0f           /* panel left edge in window pixels */
#define PANEL_Y              10.0f           /* panel top edge in window pixels */
#define PANEL_W             310.0f           /* panel width in pixels */
#define PANEL_H             600.0f           /* panel height in pixels */
#define LABEL_HEIGHT         24.0f           /* single row height in the panel */
#define LABEL_SPACER         (LABEL_HEIGHT * 0.5f)  /* half-row vertical gap */

/* Movement input dead-zone */
#define MOVE_INPUT_EPSILON    0.001f         /* ignore move vectors smaller than this */

/* Capsule mesh aspect-ratio comparison tolerance */
#define CAPSULE_ASPECT_TOL    0.01f          /* two aspects are "same" within this delta */

/* Ground contact reservation — a box can generate up to 8 corner contacts */
#define GROUND_CONTACT_RESERVE  8

/* Number of scenes in this lesson */
#define NUM_SCENES            2

/* Maximum distinct capsule aspect-ratio meshes to cache */
#define MAX_CAPSULE_MESHES    4

/* AABB wireframe colors — green when not overlapping */
#define AABB_COLOR_R          0.2f
#define AABB_COLOR_G          0.9f
#define AABB_COLOR_B          0.2f
#define AABB_COLOR_A          0.25f

/* AABB wireframe colors — orange tint when pair is overlapping */
#define AABB_OVERLAP_R        0.95f
#define AABB_OVERLAP_G        0.6f
#define AABB_OVERLAP_B        0.1f
#define AABB_OVERLAP_A        0.35f

/* Shape colors */
#define COLOR_DEFAULT_R       0.5f
#define COLOR_DEFAULT_G       0.7f
#define COLOR_DEFAULT_B       0.9f

#define COLOR_INTERSECT_R     0.95f
#define COLOR_INTERSECT_G     0.2f
#define COLOR_INTERSECT_B     0.15f

#define COLOR_RED_R           0.9f
#define COLOR_RED_G           0.3f
#define COLOR_RED_B           0.2f

#define COLOR_BLUE_R          0.2f
#define COLOR_BLUE_G          0.5f
#define COLOR_BLUE_B          0.9f

#define COLOR_GREEN_R         0.3f
#define COLOR_GREEN_G         0.8f
#define COLOR_GREEN_B         0.3f

#define COLOR_ORANGE_R        0.9f
#define COLOR_ORANGE_G        0.6f
#define COLOR_ORANGE_B        0.1f

#define COLOR_PURPLE_R        0.7f
#define COLOR_PURPLE_G        0.3f
#define COLOR_PURPLE_B        0.9f

#define COLOR_TEAL_R          0.2f
#define COLOR_TEAL_G          0.8f
#define COLOR_TEAL_B          0.8f

#define COLOR_PINK_R          0.9f
#define COLOR_PINK_G          0.4f
#define COLOR_PINK_B          0.6f

#define COLOR_LIME_R          0.6f
#define COLOR_LIME_G          0.9f
#define COLOR_LIME_B          0.2f

#define COLOR_GOLD_R          0.9f
#define COLOR_GOLD_G          0.8f
#define COLOR_GOLD_B          0.2f

/* Contact point marker colors */
#define CONTACT_MARKER_R      1.0f           /* bright yellow for contact point markers */
#define CONTACT_MARKER_G      0.9f
#define CONTACT_MARKER_B      0.1f

/* Warm-started contact marker — distinguishable from fresh contacts */
#define WARM_MARKER_R         0.1f           /* cyan for warm-started contacts */
#define WARM_MARKER_G         0.9f
#define WARM_MARKER_B         0.9f

/* Pseudo-random LCG for deterministic spawning */
#define SPAWN_SEED            42u            /* fixed seed ensures reproducible layout */
#define LCG_MULTIPLIER        1664525u       /* Numerical Recipes multiplier */
#define LCG_INCREMENT         1013904223u    /* Numerical Recipes increment */
#define LCG_SHIFT             8              /* discard low-quality low bits */
#define LCG_DIVISOR           (1u << 24)    /* normalize to [0, 1) range */

/* Number of distinct body colors in the palette */
#define NUM_BODY_COLORS       9

/* Shared body color palette — assigned round-robin during Scene 2 spawn */
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

/* Shape pair names for Scene 1 UI selector */
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

    /* Per-aspect-ratio capsule meshes — one mesh per unique radius:half_height ratio */
    struct {
        SDL_GPUBuffer *vb;
        SDL_GPUBuffer *ib;
        Uint32         index_count;
        float          aspect;   /* half_height / radius — identifies the mesh */
    } capsule_meshes[MAX_CAPSULE_MESHES];
    int num_capsule_meshes;

    /* Physics state */
    ForgePhysicsRigidBody      bodies[MAX_BODIES];        /* rigid body dynamics */
    ForgePhysicsCollisionShape shapes[MAX_BODIES];         /* collision geometry */
    float                      body_colors[MAX_BODIES][4]; /* RGBA display color */
    int                        num_bodies;                 /* active body count */

    /* Cached AABBs — recomputed each physics step from body pose + shape */
    ForgePhysicsAABB           cached_aabbs[MAX_BODIES];

    /* SAP broadphase — finds AABB-overlapping pairs for narrowphase */
    ForgePhysicsSAPWorld       sap;

    /* GJK/EPA results for current frame */
    int  gjk_pair_count;      /* total SAP pairs tested with GJK */
    int  gjk_hit_count;       /* pairs where GJK reported intersection */
    int  epa_contact_count;   /* contacts generated by EPA this step */
    bool body_gjk_hit[MAX_BODIES]; /* true if this body was in a GJK hit */

    /* Scene 1: manifold for the two-body test pair */
    ForgePhysicsManifold s1_manifold;

    /* Scene 2: persistent manifold cache — keyed by body-pair ID */
    ForgePhysicsManifoldCacheEntry *manifold_cache;  /* stb_ds hash map (NULL = empty) */

    /* Scene 2: transient manifolds — rebuilt each step for rendering when
     * persistence is disabled (use_manifolds == false). */
    ForgePhysicsManifold transient_manifolds[MAX_CONTACTS];
    int transient_manifold_count;

    /* Scene 2: manifold statistics for the UI panel */
    int manifold_count;       /* number of cached manifold entries this step */
    int total_contact_count;  /* sum of contact points across all manifolds */
    int cache_entry_count;    /* number of entries in the manifold cache    */

    /* Scene 2: toggle — true uses persistent manifold cache, false rebuilds fresh */
    bool use_manifolds;

    /* Scene 2: contact state */
    int last_contact_count;   /* total contacts (ground + manifold) last step */

    /* Simulation control */
    int   scene_index;        /* 0 = Manifold Inspector, 1 = Multi-Body Stacking */
    float accumulator;        /* fixed-timestep accumulator (seconds) */
    bool  paused;             /* true when physics is frozen */
    float speed_scale;        /* 1.0 normal, 0.25 slow motion */

    /* AABB visualization toggle */
    bool show_aabbs;

    /* Which bodies are currently in an overlapping SAP pair */
    bool body_in_pair[MAX_BODIES];

    /* Scene 1 — user-controlled movement (arrow keys) */
    bool move_left;       /* left arrow held  */
    bool move_right;      /* right arrow held */
    bool move_fwd;        /* up arrow held    */
    bool move_back;       /* down arrow held  */
    int  s1_pair_index;   /* active shape pair 0..5 */

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
/*
 * Blends between prev_position/prev_orientation (start of physics step) and
 * position/orientation (end of step) using alpha = leftover accumulator time
 * divided by PHYSICS_DT. This decouples render rate from physics rate so
 * fast monitors do not cause jitter.
 */
static mat4 get_body_model_matrix(const ForgePhysicsRigidBody *rb,
                                    const ForgePhysicsCollisionShape *shape,
                                    float alpha)
{
    vec3 pos      = vec3_lerp(rb->prev_position, rb->position, alpha);
    quat orient   = quat_slerp(rb->prev_orientation, rb->orientation, alpha);
    mat4 translation = mat4_translate(pos);
    mat4 rotation    = quat_to_mat4(orient);
    vec3 sc          = shape_render_scale(shape);
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

/* Forward declarations */
static void rebuild_body_in_pair(app_state *state);
static void run_narrowphase(app_state *state);
static void refresh_contact_stats(app_state *state);

/* ── Scene initialization ────────────────────────────────────────── */

/* Scene 1: Two-Body Manifold Inspector — user moves shape A toward/away from B.
 * Starts with a box-box pair so the face-clipping path that generates up to
 * 4 contact points is immediately visible. */
static void init_scene_1(app_state *state)
{
    state->num_bodies = 2;

    /* Select shapes based on the active pair index (keys 1-6):
     * 0: sphere-sphere, 1: sphere-box, 2: sphere-capsule,
     * 3: box-box, 4: box-capsule, 5: capsule-capsule */
    ForgePhysicsCollisionShape sa, sb;
    switch (state->s1_pair_index) {
    case 0: /* sphere-sphere */
        sa = forge_physics_shape_sphere(S1_SPHERE_RADIUS);
        sb = forge_physics_shape_sphere(S1_SPHERE_RADIUS);
        break;
    case 1: /* sphere-box */
        sa = forge_physics_shape_sphere(S1_SPHERE_RADIUS);
        sb = forge_physics_shape_box(
            vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF));
        break;
    case 2: /* sphere-capsule */
        sa = forge_physics_shape_sphere(S1_SPHERE_RADIUS);
        sb = forge_physics_shape_capsule(S1_CAPSULE_RADIUS, S1_CAPSULE_HALF_H);
        break;
    case 3: /* box-box */
    default:
        sa = forge_physics_shape_box(
            vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF));
        sb = forge_physics_shape_box(
            vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF));
        break;
    case 4: /* box-capsule */
        sa = forge_physics_shape_box(
            vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF));
        sb = forge_physics_shape_capsule(S1_CAPSULE_RADIUS, S1_CAPSULE_HALF_H);
        break;
    case 5: /* capsule-capsule */
        sa = forge_physics_shape_capsule(S1_CAPSULE_RADIUS, S1_CAPSULE_HALF_H);
        sb = forge_physics_shape_capsule(S1_CAPSULE_RADIUS, S1_CAPSULE_HALF_H);
        break;
    }

    init_body(state, 0,
        vec3_create(S1_BODY_A_X, S1_BODY_A_Y, 0.0f),
        sa, 0.0f, DEFAULT_RESTIT,
        COLOR_DEFAULT_R, COLOR_DEFAULT_G, COLOR_DEFAULT_B);

    init_body(state, 1,
        vec3_create(S1_BODY_B_X, S1_BODY_B_Y, 0.0f),
        sb, 0.0f, DEFAULT_RESTIT,
        COLOR_DEFAULT_R, COLOR_DEFAULT_G, COLOR_DEFAULT_B);

    /* Clear the manifold result and the cache for the two-body inspector.
     * The cache persists across frames so we wipe it on scene reset to
     * guarantee a clean warm-start baseline. */
    SDL_memset(&state->s1_manifold, 0, sizeof(state->s1_manifold));
    forge_physics_manifold_cache_free(&state->manifold_cache);
    state->manifold_cache = NULL;
}

/* Scene 2: Full Physics Pipeline — 15 mixed bodies (spheres, boxes, capsules)
 * dropped from height with a deterministic LCG layout. */
static void init_scene_2(app_state *state)
{
    state->num_bodies = S2_NUM_BODIES;
    lcg_state = SPAWN_SEED;

    /* Wipe the manifold cache so stale entries from a previous run do not
     * carry over — persistent contacts only make sense within a continuous
     * simulation, not across resets. */
    forge_physics_manifold_cache_free(&state->manifold_cache);
    state->manifold_cache = NULL;

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
    state->accumulator       = 0.0f;
    state->gjk_pair_count    = 0;
    state->gjk_hit_count     = 0;
    state->manifold_count    = 0;
    state->total_contact_count = 0;
    state->transient_manifold_count = 0;
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

    /* Sync stats for UI panel */
    refresh_contact_stats(state);
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

/* ── Narrowphase: SAP pairs → GJK → EPA → manifold generation ────── */

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
    state->manifold_count = 0;

    const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(&state->sap);
    int pair_count = forge_physics_sap_pair_count(&state->sap);
    state->gjk_pair_count = pair_count;

    /* Clear Scene 1 manifold result */
    SDL_memset(&state->s1_manifold, 0, sizeof(state->s1_manifold));

    /* Track active manifold keys — only pairs that produce a manifold
     * (count > 0) should keep their cache entries alive. */
    uint64_t active_keys[MAX_CONTACTS];
    int key_count = 0;

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

            /* EPA gives penetration depth and contact normal */
            ForgePhysicsEPAResult epa = forge_physics_epa_bodies(
                &gjk,
                &state->bodies[a], &state->shapes[a],
                &state->bodies[b], &state->shapes[b]);

            if (epa.valid) {
                /* Generate full contact manifold via face clipping */
                ForgePhysicsManifold m = forge_physics_manifold_generate(
                    &epa,
                    &state->shapes[a], state->bodies[a].position,
                    state->bodies[a].orientation,
                    &state->shapes[b], state->bodies[b].position,
                    state->bodies[b].orientation,
                    a, b,
                    DEFAULT_MU_S, DEFAULT_MU_D);

                if (m.count > 0) {
                    state->manifold_count++;

                    /* Warm-start: merge with cached manifold when enabled */
                    if (state->use_manifolds) {
                        forge_physics_manifold_cache_update(
                            &state->manifold_cache, &m);
                    }

                    /* Record active key — only manifold-producing pairs */
                    if (key_count < MAX_CONTACTS) {
                        active_keys[key_count++] =
                            forge_physics_manifold_pair_key(a, b);
                    }

                    /* Scene 1: store the live manifold for visualization */
                    if (state->scene_index == 0 && a == 0 && b == 1) {
                        state->s1_manifold = m;
                    }
                }
            }
        }
    }

    /* Prune stale cache entries — only pairs that actually produced a
     * manifold (count > 0) should keep their cache entries alive.
     * Zero-contact broadphase pairs must not prevent stale removal. */
    if (state->use_manifolds) {
        forge_physics_manifold_cache_prune(
            &state->manifold_cache, active_keys, key_count);
    }
}

/* ── SDL_AppInit ─────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state *state = SDL_calloc(1, sizeof(*state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    ForgeSceneConfig cfg = forge_scene_default_config(
        "Physics 11 - Contact Manifold");
    cfg.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
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

    /* Generate and upload small marker sphere for contact point visualization */
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

    /* Generate capsule meshes for all aspect ratios used in the lesson.
     * Scene 1 and Scene 2 may use different capsule proportions; we build
     * one mesh per unique aspect ratio so the render loop can pick the
     * closest match without distorting the collision shape. */
    {
        const float capsule_aspects[] = {
            S1_CAPSULE_HALF_H / S1_CAPSULE_RADIUS,
            S2_CAPSULE_HALF_H / S2_CAPSULE_RADIUS,
        };
        int num_aspects =
            (int)(sizeof(capsule_aspects) / sizeof(capsule_aspects[0]));
        state->num_capsule_meshes = 0;

        for (int ci = 0; ci < num_aspects && ci < MAX_CAPSULE_MESHES; ci++) {
            /* Skip duplicate aspect ratios */
            bool dup = false;
            for (int j = 0; j < state->num_capsule_meshes; j++) {
                if (SDL_fabsf(state->capsule_meshes[j].aspect -
                              capsule_aspects[ci]) < CAPSULE_ASPECT_TOL) {
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
            state->capsule_meshes[idx].vb =
                upload_shape_vb(&state->scene, &cap);
            state->capsule_meshes[idx].ib = forge_scene_upload_buffer(
                &state->scene,
                SDL_GPU_BUFFERUSAGE_INDEX, cap.indices,
                (Uint32)cap.index_count * (Uint32)sizeof(uint32_t));
            state->capsule_meshes[idx].index_count = (Uint32)cap.index_count;
            state->capsule_meshes[idx].aspect = capsule_aspects[ci];
            forge_shapes_free(&cap);
            if (!state->capsule_meshes[idx].vb ||
                !state->capsule_meshes[idx].ib) {
                SDL_Log("ERROR: Failed to upload capsule geometry (aspect %.2f)",
                        (double)capsule_aspects[ci]);
                goto init_fail;
            }
            state->num_capsule_meshes++;
        }
    }

    /* Default state */
    state->scene_index    = 0;
    state->accumulator    = 0.0f;
    state->paused         = false;
    state->speed_scale    = NORMAL_SPEED_SCALE;
    state->show_aabbs     = true;
    state->use_manifolds  = true;
    state->manifold_cache = NULL;  /* fat-pointer hash map — NULL = empty */
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    init_current_scene(state);

    return SDL_APP_CONTINUE;

init_fail:
    return SDL_APP_FAILURE;
}
/* ── Physics step ────────────────────────────────────────────────── */

/* Refresh contact and cache statistics for the UI panel.
 * Called after narrowphase and also after reset/toggle so the panel
 * stays in sync even when the fixed-step loop is not running. */
static void refresh_contact_stats(app_state *state)
{
    if (state->use_manifolds) {
        ptrdiff_t len = forge_hm_length(state->manifold_cache);
        state->cache_entry_count = (int)(len > 0 ? len : 0);
    } else {
        state->cache_entry_count = 0;
    }
}

/* Scene 1: Run GJK+EPA+manifold on the two-body test pair.
 * No dynamics — body A is moved by the user in SDL_AppIterate. */
static void physics_step_scene1(app_state *state)
{
    /* Clear previous manifold */
    SDL_memset(&state->s1_manifold, 0, sizeof(state->s1_manifold));
    state->body_gjk_hit[0] = false;
    state->body_gjk_hit[1] = false;

    ForgePhysicsManifold manifold;
    if (forge_physics_gjk_epa_manifold(
            &state->bodies[0], &state->shapes[0],
            &state->bodies[1], &state->shapes[1],
            0, 1,
            DEFAULT_MU_S, DEFAULT_MU_D,
            &manifold))
    {
        state->s1_manifold      = manifold;
        state->body_gjk_hit[0]  = true;
        state->body_gjk_hit[1]  = true;
    }
}

/* Scene 2: Full pipeline — gravity, integrate, broadphase, manifold
 * generation with cache, contact resolution, ground collisions. */
static void physics_step(app_state *state, float dt)
{
    if (state->scene_index == 0) {
        physics_step_scene1(state);
        return;
    }

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

    /* Narrowphase: GJK + EPA + manifold generation */
    SDL_memset(state->body_gjk_hit, 0, sizeof(state->body_gjk_hit));
    state->gjk_pair_count   = 0;
    state->gjk_hit_count    = 0;
    state->manifold_count = 0;
    state->total_contact_count  = 0;
    state->transient_manifold_count = 0;

    const ForgePhysicsSAPPair *pairs = forge_physics_sap_get_pairs(&state->sap);
    int pair_count = forge_physics_sap_pair_count(&state->sap);
    state->gjk_pair_count = pair_count;

    /* Collect active pair keys for cache pruning */
    uint64_t active_keys[MAX_CONTACTS];
    int active_key_count = 0;

    /* Body-body contacts via GJK + EPA + manifold */
    ForgePhysicsRBContact body_contacts[MAX_CONTACTS];
    int body_count = 0;

    for (int i = 0; i < pair_count; i++) {
        int a = pairs[i].a;
        int b = pairs[i].b;
        if (a < 0 || a >= n || b < 0 || b >= n) continue;

        ForgePhysicsManifold manifold;
        if (forge_physics_gjk_epa_manifold(
                &state->bodies[a], &state->shapes[a],
                &state->bodies[b], &state->shapes[b],
                a, b,
                DEFAULT_MU_S, DEFAULT_MU_D,
                &manifold))
        {
            state->body_gjk_hit[a] = true;
            state->body_gjk_hit[b] = true;
            state->gjk_hit_count++;
            state->manifold_count++;

            /* Store transient manifold for rendering when persistence is off */
            if (state->transient_manifold_count < MAX_CONTACTS) {
                state->transient_manifolds[state->transient_manifold_count++] =
                    manifold;
            }

            /* Update manifold cache (only when manifold mode is active) */
            if (state->use_manifolds) {
                forge_physics_manifold_cache_update(&state->manifold_cache,
                                                     &manifold);

                /* Record active key for pruning */
                if (active_key_count < MAX_CONTACTS) {
                    active_keys[active_key_count++] =
                        forge_physics_manifold_pair_key(a, b);
                }
            }

            /* Convert manifold to RBContacts for the solver */
            int added = forge_physics_manifold_to_rb_contacts(
                &manifold,
                &body_contacts[body_count],
                MAX_CONTACTS - body_count);
            body_count          += added;
            state->total_contact_count += added;
        }
    }

    /* Prune stale cache entries (only when manifold mode is active) */
    if (state->use_manifolds) {
        forge_physics_manifold_cache_prune(&state->manifold_cache,
                                            active_keys, active_key_count);
    }

    /* Update cache/contact stats for UI */
    refresh_contact_stats(state);

    /* Ground collision — analytical per shape type */
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
        case FORGE_PHYSICS_SHAPE_CAPSULE: {
            /* Find the capsule segment endpoint closest to the ground
             * plane by comparing signed distances. Test both endpoints
             * when they are both below the surface — a horizontal resting
             * capsule generates two contacts for stable support. */
            float cr = state->shapes[i].data.capsule.radius;
            float ch = state->shapes[i].data.capsule.half_height;
            mat3  R  = quat_to_mat3(state->bodies[i].orientation);
            vec3 local_top = vec3_create(0.0f,  ch, 0.0f);
            vec3 local_bot = vec3_create(0.0f, -ch, 0.0f);
            vec3 world_top = vec3_add(state->bodies[i].position,
                                       mat3_multiply_vec3(R, local_top));
            vec3 world_bot = vec3_add(state->bodies[i].position,
                                       mat3_multiply_vec3(R, local_bot));

            float dist_top = vec3_dot(vec3_sub(world_top, plane_pt), plane_n);
            float dist_bot = vec3_dot(vec3_sub(world_bot, plane_pt), plane_n);

            float min_dist = dist_bot < dist_top ? dist_bot : dist_top;
            float max_dist = dist_bot < dist_top ? dist_top : dist_bot;
            vec3  min_pt   = dist_bot < dist_top ? world_bot : world_top;
            vec3  max_pt   = dist_bot < dist_top ? world_top : world_bot;

            /* Primary contact: closest endpoint */
            if (min_dist < cr && ground_count + count < MAX_CONTACTS) {
                ForgePhysicsRBContact *gc = &ground_contacts[ground_count + count];
                gc->point        = vec3_sub(min_pt, vec3_scale(plane_n, min_dist));
                gc->normal       = plane_n;
                gc->penetration  = cr - min_dist;
                gc->body_a       = i;
                gc->body_b       = -1;
                gc->static_friction  = DEFAULT_MU_S;
                gc->dynamic_friction = DEFAULT_MU_D;
                count++;
            }
            /* Secondary contact: far endpoint when it also penetrates */
            if (max_dist < cr && ground_count + count < MAX_CONTACTS) {
                ForgePhysicsRBContact *gc = &ground_contacts[ground_count + count];
                gc->point        = vec3_sub(max_pt, vec3_scale(plane_n, max_dist));
                gc->normal       = plane_n;
                gc->penetration  = cr - max_dist;
                gc->body_a       = i;
                gc->body_b       = -1;
                gc->static_friction  = DEFAULT_MU_S;
                gc->dynamic_friction = DEFAULT_MU_D;
                count++;
            }
            break;
        }
        default:
            break;
        }

        ground_count += count;
    }

    /* Resolve contacts — ground first for stable stacking, then body-body */
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

/* Draw a shape body (shadow pass) */
static void draw_body_shadow(app_state *state, int idx, float alpha)
{
    const ForgePhysicsCollisionShape *shape = &state->shapes[idx];
    mat4 model = get_body_model_matrix(&state->bodies[idx], shape, alpha);

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

/* Draw a shape body (main pass) */
static void draw_body_main(app_state *state, int idx, float alpha)
{
    const ForgePhysicsCollisionShape *shape = &state->shapes[idx];
    mat4 model = get_body_model_matrix(&state->bodies[idx], shape, alpha);

    /* Color: red tint if GJK confirmed intersection, else body color */
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

/* Draw an AABB as a wireframe cube using the wireframe pipeline */
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

/* Draw an arrow (thin cube) from origin along direction with length */
static void draw_arrow(app_state *state, vec3 origin, vec3 dir,
                        float length, float r, float g, float b)
{
    if (length < ARROW_MIN_LENGTH) return;

    float visual_length = length > ARROW_MIN_VISIBLE ? length : ARROW_MIN_VISIBLE;
    vec3 scale = vec3_create(ARROW_THICKNESS, visual_length * 0.5f,
                              ARROW_THICKNESS);

    vec3 up = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 d = vec3_normalize(dir);

    mat4 rotation;
    float dot = SDL_fabsf(vec3_dot(d, up));
    if (dot > ARROW_PARALLEL_DOT) {
        if (d.y > 0.0f) {
            rotation = mat4_identity();
        } else {
            rotation = quat_to_mat4(quat_from_euler(0.0f, 0.0f, FORGE_PI));
        }
    } else {
        vec3 axis = vec3_normalize(vec3_cross(up, d));
        float cos_angle = vec3_dot(up, d);
        if (cos_angle >  1.0f) cos_angle =  1.0f;
        if (cos_angle < -1.0f) cos_angle = -1.0f;
        float angle = SDL_acosf(cos_angle);
        quat q = quat_from_axis_angle(axis, angle);
        rotation = quat_to_mat4(q);
    }

    vec3 center = vec3_add(origin, vec3_scale(d, visual_length * 0.5f));

    mat4 model = mat4_multiply(
        mat4_translate(center),
        mat4_multiply(rotation, mat4_scale(scale)));

    float color[4] = {r, g, b, 1.0f};
    forge_scene_draw_mesh(&state->scene,
        state->cube_vb, state->cube_ib,
        state->cube_index_count, model, color);
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
            move = vec3_scale(vec3_normalize(move), S1_MOVE_SPEED * dt);
            state->bodies[0].prev_position = state->bodies[0].position;
            state->bodies[0].position = vec3_add(
                state->bodies[0].position, move);

            /* Recompute AABBs */
            for (int i = 0; i < state->num_bodies; i++) {
                state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
                    &state->shapes[i], state->bodies[i].position,
                    state->bodies[i].orientation);
            }
            forge_physics_sap_update(&state->sap, state->cached_aabbs,
                                      state->num_bodies);
            rebuild_body_in_pair(state);
        }

        /* Run manifold generation for the two-body inspector */
        physics_step_scene1(state);
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

    /* Interpolation factor — force 1.0 when paused or in the non-dynamic
     * scene so the rendered pose matches the cached physics state exactly. */
    float alpha = state->accumulator / PHYSICS_DT;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    if (state->scene_index == 0 || state->paused) alpha = 1.0f;

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

    /* Scene 1: draw manifold contacts — point markers and normal arrows */
    if (state->scene_index == 0 && state->s1_manifold.count > 0) {
        const ForgePhysicsManifold *m = &state->s1_manifold;
        for (int c = 0; c < m->count; c++) {
            const ForgePhysicsManifoldContact *mc = &m->contacts[c];

            /* Green sphere at the world contact point */
            draw_marker(state, mc->world_point,
                        COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B);

            /* Green arrow: contact normal from contact point, length = depth */
            draw_arrow(state, mc->world_point,
                       m->normal, mc->penetration,
                       COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B);
        }
    }

    /* Scene 2: draw AABB wireframes when enabled */
    if (state->scene_index == 1 && state->show_aabbs) {
        for (int i = 0; i < state->num_bodies; i++) {
            draw_aabb_wireframe(state, state->cached_aabbs[i],
                                state->body_in_pair[i],
                                state->body_gjk_hit[i]);
        }
    }

    /* Scene 2: draw contact markers for all active manifolds.
     * When persistence is on, iterate the cache; when off, use the
     * transient per-step manifolds so markers are still visible. */
    if (state->scene_index == 1) {
        if (state->use_manifolds) {
            ptrdiff_t ci;
            forge_hm_iter(state->manifold_cache, ci) {
                const ForgePhysicsManifold *m =
                    &state->manifold_cache[ci + 1].manifold;
                for (int c = 0; c < m->count; c++) {
                    const ForgePhysicsManifoldContact *mc = &m->contacts[c];
                    draw_marker(state, mc->world_point,
                                COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B);
                    draw_arrow(state, mc->world_point,
                               m->normal, mc->penetration,
                               COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B);
                }
            }
        } else {
            for (int mi = 0; mi < state->transient_manifold_count; mi++) {
                const ForgePhysicsManifold *m = &state->transient_manifolds[mi];
                for (int c = 0; c < m->count; c++) {
                    const ForgePhysicsManifoldContact *mc = &m->contacts[c];
                    draw_marker(state, mc->world_point,
                                COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B);
                    draw_arrow(state, mc->world_point,
                               m->normal, mc->penetration,
                               COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B);
                }
            }
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
            if (forge_ui_wctx_window_begin(wctx, "Contact Manifold",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;
                char buf[128];

                /* Scene title */
                if (state->scene_index == 0) {
                    forge_ui_ctx_label_layout(ui, "Scene 1: Manifold Inspector",
                                               LABEL_HEIGHT);
                } else {
                    forge_ui_ctx_label_layout(ui, "Scene 2: Full Pipeline",
                                               LABEL_HEIGHT);
                }
                forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

                if (state->scene_index == 0) {
                    /* Shape pair selection */
                    SDL_snprintf(buf, sizeof(buf), "Pair: %s",
                                 S1_PAIR_NAMES[state->s1_pair_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    forge_ui_ctx_label_layout(ui, "(Keys 1-6 to change)",
                                               LABEL_HEIGHT);
                    forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

                    const ForgePhysicsManifold *m = &state->s1_manifold;
                    if (m->count > 0) {
                        /* Manifold summary */
                        SDL_snprintf(buf, sizeof(buf), "Contacts: %d",
                                     m->count);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                        SDL_snprintf(buf, sizeof(buf),
                                     "Normal: (%.3f, %.3f, %.3f)",
                                     (double)m->normal.x,
                                     (double)m->normal.y,
                                     (double)m->normal.z);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

                        /* Per-contact details */
                        for (int c = 0; c < m->count; c++) {
                            const ForgePhysicsManifoldContact *mc =
                                &m->contacts[c];

                            SDL_snprintf(buf, sizeof(buf),
                                         "Contact %d — ID %u",
                                         c, mc->id);
                            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                            SDL_snprintf(buf, sizeof(buf),
                                         "  Depth: %.4f m",
                                         (double)mc->penetration);
                            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                            SDL_snprintf(buf, sizeof(buf),
                                         "  Pt: (%.2f, %.2f, %.2f)",
                                         (double)mc->world_point.x,
                                         (double)mc->world_point.y,
                                         (double)mc->world_point.z);
                            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                        }
                    } else {
                        forge_ui_ctx_label_layout(ui, "Separated",
                                                   LABEL_HEIGHT);
                        forge_ui_ctx_label_layout(ui, "(Move with arrow keys)",
                                                   LABEL_HEIGHT);
                    }
                } else {
                    /* Scene 2: pipeline statistics */
                    SDL_snprintf(buf, sizeof(buf), "Bodies: %d",
                                 state->num_bodies);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

                    forge_ui_ctx_label_layout(ui, "Pipeline:",
                                               LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "  SAP pairs: %d",
                                 state->gjk_pair_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "  Manifolds: %d",
                                 state->manifold_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "  Contacts: %d",
                                 state->total_contact_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "  Cache entries: %d",
                                 state->cache_entry_count);
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
                    forge_ui_ctx_label_layout(ui,
                                               "P:pause R:reset T:speed M:cache V:AABB",
                                               LABEL_HEIGHT);
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

    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (!event->key.repeat) {
            switch (event->key.scancode) {
            case SDL_SCANCODE_R:
                /* Clear the manifold cache before re-initialising so warm-start
                 * data from the previous run does not bleed into the new one. */
                forge_physics_manifold_cache_free(&state->manifold_cache);
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
                /* Toggle AABB visualization — meaningful only in Scene 2 */
                state->show_aabbs = !state->show_aabbs;
                SDL_Log("AABB visualization: %s",
                        state->show_aabbs ? "ON" : "OFF");
                break;
            case SDL_SCANCODE_M:
                /* Toggle manifold persistence — Scene 2 only.
                 * When off, each frame still generates a fresh manifold, but
                 * skips cache reuse/update so contacts are rebuilt from
                 * scratch. When on, cached contacts persist across frames and
                 * can warm-start the solver. */
                if (state->scene_index == 1) {
                    state->use_manifolds = !state->use_manifolds;
                    /* Reset cache so toggling starts from a clean slate */
                    forge_physics_manifold_cache_free(&state->manifold_cache);
                    refresh_contact_stats(state);
                    SDL_Log("Manifold persistence: %s",
                            state->use_manifolds ? "ON" : "OFF");
                }
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
                        /* Recompute AABBs and run narrowphase for new shapes */
                        for (int i = 0; i < state->num_bodies; i++) {
                            state->cached_aabbs[i] =
                                forge_physics_shape_compute_aabb(
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

        /* Movement keys for Scene 1 (hold-to-move, repeat OK) */
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

    /* Release manifold cache before the SAP world — both reference body
     * indices, so order does not matter, but clearing here keeps all
     * physics teardown in one block. */
    forge_physics_manifold_cache_free(&state->manifold_cache);
    forge_physics_sap_destroy(&state->sap);
    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
