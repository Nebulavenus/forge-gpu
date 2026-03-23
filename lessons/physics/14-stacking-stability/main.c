/*
 * Physics Lesson 14 — Stacking Stability
 *
 * Demonstrates: Tuning the sequential impulse solver for stable box
 * stacking via warm-starting, Baumgarte stabilization, penetration
 * slop, and configurable solver iteration count. Visual debugging of
 * contact points and normals shows the solver at work.
 *
 * Four selectable scenes:
 *   1. Tall Tower (12)  — 12 boxes stacked vertically
 *   2. Pyramid          — 7-layer pyramid (28 boxes)
 *   3. Wall             — 6 columns x 5 rows (30 boxes)
 *   4. Stress Test (15) — 15 boxes in a single tower
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   Tab               — cycle scenes
 *   C                 — toggle contact visualization
 *   W                 — toggle warm-starting (release mouse first)
 *   = / -             — increase / decrease solver iterations
 *   B / N             — increase / decrease Baumgarte factor
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
 * manifold generation, persistent caching, sequential impulse solver */
#include "physics/forge_physics.h"

/* Procedural geometry — sphere, cube meshes */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — Blinn-Phong, shadow map, grid, sky, camera, UI */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Physics simulation */
#define PHYSICS_DT           (1.0f / 60.0f)  /* fixed physics timestep (s) */
#define MAX_DELTA_TIME        0.1f            /* cap frame dt (spiral of death) */

/* Scene limits */
#define MAX_BODIES            40              /* maximum rigid bodies */
#define MAX_MANIFOLDS         128             /* maximum manifolds per step */

/* Box parameters */
#define BOX_HALF              0.5f            /* all boxes are unit cubes */
#define BOX_MASS              2.0f            /* per-box mass (kg) */
#define STACK_OFFSET          0.001f          /* small gap to avoid initial overlap */

/* Common physics parameters */
#define GROUND_Y              0.0f            /* ground plane Y coordinate */
#define DEFAULT_DAMPING       0.99f           /* linear velocity damping */
#define DEFAULT_ANG_DAMPING   0.98f           /* angular velocity damping */
#define DEFAULT_RESTIT        0.2f            /* coefficient of restitution */
#define DEFAULT_MU_S          0.6f            /* static friction */
#define DEFAULT_MU_D          0.4f            /* dynamic friction */
#define DEFAULT_SOLVER_ITERS  60              /* solver iteration count — tuned for 15-box stacks */
#define MAX_SOLVER_ITERS     100              /* upper cap for iterations */

/* Contact visualization */
#define CONTACT_MARKER_RADIUS 0.04f           /* sphere radius for contact points */
#define NORMAL_LINE_SCALE     10.0f           /* penetration × scale = line length */
#define NORMAL_LINE_MIN       0.05f           /* minimum line length for visibility */
#define DEPTH_COLOR_SCALE     10.0f           /* maps [0, 0.1m] penetration to [0, 1] color */
#define MAX_CONTACT_MARKERS   (MAX_MANIFOLDS * FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS)

/* Gravity */
static const vec3 GRAVITY = { 0.0f, -9.81f, 0.0f };

/* Number of scenes */
#define NUM_SCENES            4

/* UI layout */
#define PANEL_X              10.0f
#define PANEL_Y              10.0f
#define PANEL_W             450.0f
#define PANEL_H             650.0f
#define LABEL_HEIGHT          18.0f
#define LABEL_SPACER          4.0f

/* Scene names */
static const char *SCENE_NAMES[] = {
    "Tall Tower (12)",
    "Pyramid (28)",
    "Wall (30)",
    "Stress Test (15)"
};

/* Body color palette (8 distinguishable colors, cycled by body index) */
static const float BODY_COLORS[][4] = {
    { 0.85f, 0.20f, 0.20f, 1.0f },  /* red */
    { 0.20f, 0.65f, 0.30f, 1.0f },  /* green */
    { 0.25f, 0.45f, 0.85f, 1.0f },  /* blue */
    { 0.90f, 0.75f, 0.15f, 1.0f },  /* gold */
    { 0.70f, 0.30f, 0.70f, 1.0f },  /* purple */
    { 0.20f, 0.75f, 0.75f, 1.0f },  /* teal */
    { 0.90f, 0.55f, 0.20f, 1.0f },  /* orange */
    { 0.55f, 0.55f, 0.55f, 1.0f },  /* gray */
};
#define NUM_BODY_COLORS 8

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;       /* rendering, camera, shadow map, grid, sky, UI */

    /* GPU resources — app-owned geometry */
    SDL_GPUBuffer *sphere_vb, *sphere_ib;   /* sphere mesh */
    SDL_GPUBuffer *cube_vb, *cube_ib;       /* cube mesh */
    int sphere_index_count;                  /* sphere index count */
    int cube_index_count;                    /* cube index count */

    /* Physics state */
    ForgePhysicsRigidBody bodies[MAX_BODIES];       /* rigid body dynamics  */
    ForgePhysicsCollisionShape shapes[MAX_BODIES];  /* collision geometry   */
    float body_colors[MAX_BODIES][4];               /* per-body RGBA       */
    int   num_bodies;                               /* active body count   */

    /* Broadphase */
    ForgePhysicsAABB cached_aabbs[MAX_BODIES]; /* per-body AABBs */
    ForgePhysicsSAPWorld sap;                  /* sweep-and-prune state */

    /* Contact / solver state */
    ForgePhysicsManifold manifolds[MAX_MANIFOLDS];       /* per-step manifolds     */
    ForgePhysicsSIManifold si_workspace[MAX_MANIFOLDS];  /* SI solver scratch      */
    ForgePhysicsManifoldCacheEntry *manifold_cache;      /* persistent warm-start  */
    int   manifold_count;                                /* manifolds this step    */

    /* Solver configuration — tunable via UI */
    bool  use_warm_start;        /* warm-start from cached impulses */
    int   solver_iterations;     /* configurable iteration count */
    float baumgarte_factor;      /* velocity bias rate [0..1] */
    float penetration_slop;      /* tolerated overlap (m) */

    /* Per-frame stats for UI */
    int   total_contact_count;   /* contacts resolved this frame */
    int   gjk_pair_count;        /* broadphase pairs tested */
    int   gjk_hit_count;         /* narrowphase hits */

    /* Timing / simulation control */
    float accumulator;           /* fixed-timestep accumulator (s) */
    bool  paused;                /* true = physics frozen */
    float speed_scale;           /* 1.0 = normal, 0.25 = slow motion */

    /* Scene management */
    int   scene_index;           /* current scene (0..NUM_SCENES-1) */

    /* Display toggles */
    bool  show_contacts;         /* true = draw contact markers + normals */

    /* UI window state */
    ForgeUiWindowState ui_window; /* draggable panel position and collapse */
} app_state;

/* ── Helper: Upload a ForgeShape to GPU vertex buffer ─────────────── */

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

/* ── Helper: Create a body with box shape ────────────────────────── */

static void make_box_body(app_state *state, int idx,
                          vec3 pos, float mass, vec3 half_ext,
                          const float color[4])
{
    ForgePhysicsRigidBody *b = &state->bodies[idx];
    SDL_memset(b, 0, sizeof(*b));
    b->position         = pos;
    b->prev_position    = pos;
    b->orientation      = quat_identity();
    b->prev_orientation = quat_identity();
    b->mass        = mass;
    b->inv_mass    = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
    b->restitution = DEFAULT_RESTIT;
    b->damping     = DEFAULT_DAMPING;
    b->angular_damping = DEFAULT_ANG_DAMPING;

    if (mass > 0.0f) {
        forge_physics_rigid_body_set_inertia_box(b, half_ext);
    }

    state->shapes[idx].type = FORGE_PHYSICS_SHAPE_BOX;
    state->shapes[idx].data.box.half_extents = half_ext;

    SDL_memcpy(state->body_colors[idx], color, 4 * sizeof(float));
}

/* ── Collision Detection ─────────────────────────────────────────── */

/* Collect ground contacts as manifolds for the SI solver.
 * Ground manifolds are routed through the manifold cache so that
 * warm-start impulses persist across frames. */
static int collect_ground_manifolds(app_state *state,
                                    ForgePhysicsManifold *out,
                                    int max_out,
                                    uint64_t *active_keys,
                                    int *active_key_count)
{
    int count = 0;
    vec3 ground_pt = vec3_create(0, GROUND_Y, 0);
    vec3 ground_n  = vec3_create(0, 1, 0);

    for (int i = 0; i < state->num_bodies && count < max_out; i++) {
        const ForgePhysicsCollisionShape *shape = &state->shapes[i];
        const ForgePhysicsRigidBody *body = &state->bodies[i];

        ForgePhysicsRBContact gc[8];
        int ng = forge_physics_rb_collide_box_plane(
            body, i, shape->data.box.half_extents,
            ground_pt, ground_n,
            DEFAULT_MU_S, DEFAULT_MU_D, gc, 8);

        if (ng > 0) {
            ForgePhysicsManifold manifold;
            if (forge_physics_si_rb_contacts_to_manifold(
                    gc, ng, DEFAULT_MU_S, DEFAULT_MU_D, &manifold)) {

                /* Route through manifold cache for warm-starting */
                forge_physics_manifold_cache_update(
                    &state->manifold_cache, &manifold);

                uint64_t key = forge_physics_manifold_pair_key(i, -1);
                if (*active_key_count < MAX_MANIFOLDS) {
                    active_keys[(*active_key_count)++] = key;
                }

                /* Retrieve cached manifold (with warm-start impulses) */
                ForgePhysicsManifoldCacheEntry *cached =
                    forge_hm_get_ptr_or_null(state->manifold_cache, key);
                if (cached && cached->key == key) {
                    out[count++] = cached->manifold;
                } else {
                    out[count++] = manifold;
                }
            }
        }
    }

    return count;
}

/* Collect body-body contacts via SAP + GJK + EPA + manifold generation. */
static int collect_body_manifolds(app_state *state,
                                  ForgePhysicsManifold *out,
                                  int max_out,
                                  uint64_t *active_keys,
                                  int *active_key_count)
{
    int count = 0;

    /* Update AABBs */
    for (int i = 0; i < state->num_bodies; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i],
            state->bodies[i].position,
            state->bodies[i].orientation);
    }

    /* SAP broadphase */
    forge_physics_sap_update(&state->sap, state->cached_aabbs,
                              state->num_bodies);

    state->gjk_pair_count = 0;
    state->gjk_hit_count = 0;

    /* Narrowphase: GJK + EPA + manifold generation */
    ForgePhysicsSAPPair *pairs = state->sap.pairs;
    ptrdiff_t num_pairs = forge_arr_length(pairs);

    for (ptrdiff_t pi = 0; pi < num_pairs && count < max_out; pi++) {
        int a = pairs[pi].a;
        int b = pairs[pi].b;
        state->gjk_pair_count++;

        ForgePhysicsManifold manifold;
        if (forge_physics_gjk_epa_manifold(
                &state->bodies[a], &state->shapes[a],
                &state->bodies[b], &state->shapes[b],
                a, b, DEFAULT_MU_S, DEFAULT_MU_D, &manifold)) {
            state->gjk_hit_count++;

            /* Update manifold cache (warm-start impulses) */
            forge_physics_manifold_cache_update(
                &state->manifold_cache, &manifold);

            uint64_t key = forge_physics_manifold_pair_key(a, b);
            if (*active_key_count < MAX_MANIFOLDS) {
                active_keys[(*active_key_count)++] = key;
            }

            /* Retrieve the cached manifold (with warm-start impulses) */
            ForgePhysicsManifoldCacheEntry *cached =
                forge_hm_get_ptr_or_null(state->manifold_cache, key);
            if (cached && cached->key == key) {
                out[count++] = cached->manifold;
            } else {
                out[count++] = manifold;
            }
        }
    }

    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Scene Setup Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Scene 1: Tall Tower — 12 boxes stacked vertically */
static void setup_scene_tower(app_state *state)
{
    state->num_bodies = 12;
    vec3 half = vec3_create(BOX_HALF, BOX_HALF, BOX_HALF);

    for (int i = 0; i < 12; i++) {
        float y = BOX_HALF + (float)i * (2.0f * BOX_HALF + STACK_OFFSET);
        int ci = i % NUM_BODY_COLORS;
        make_box_body(state, i,
                      vec3_create(0.0f, y, 0.0f),
                      BOX_MASS, half, BODY_COLORS[ci]);
    }
}

/* Scene 2: Pyramid — 7-6-5-4-3-2-1 (28 boxes total) */
static void setup_scene_pyramid(app_state *state)
{
    state->num_bodies = 0;
    vec3 half = vec3_create(BOX_HALF, BOX_HALF, BOX_HALF);
    float box_size = 2.0f * BOX_HALF;

    int idx = 0;
    /* Build rows from bottom (7) to top (1) */
    for (int row = 0; row < 7; row++) {
        int boxes_in_row = 7 - row;
        /* Center the row: offset so middle of row is at x=0 */
        float row_width = (float)boxes_in_row * box_size;
        float start_x = -row_width * 0.5f + BOX_HALF;
        float y = BOX_HALF + (float)row * (box_size + STACK_OFFSET);

        for (int col = 0; col < boxes_in_row; col++) {
            float x = start_x + (float)col * box_size;
            int ci = idx % NUM_BODY_COLORS;
            make_box_body(state, idx,
                          vec3_create(x, y, 0.0f),
                          BOX_MASS, half, BODY_COLORS[ci]);
            idx++;
        }
    }
    state->num_bodies = idx;
}

/* Scene 3: Wall — 6 columns x 5 rows (30 boxes) */
static void setup_scene_wall(app_state *state)
{
    state->num_bodies = 0;
    vec3 half = vec3_create(BOX_HALF, BOX_HALF, BOX_HALF);
    float box_size = 2.0f * BOX_HALF;

    int idx = 0;
    int cols = 6;
    int rows = 5;
    float wall_width = (float)cols * box_size;
    float start_x = -wall_width * 0.5f + BOX_HALF;

    for (int row = 0; row < rows; row++) {
        float y = BOX_HALF + (float)row * (box_size + STACK_OFFSET);
        for (int col = 0; col < cols; col++) {
            float x = start_x + (float)col * box_size;
            int ci = idx % NUM_BODY_COLORS;
            make_box_body(state, idx,
                          vec3_create(x, y, 0.0f),
                          BOX_MASS, half, BODY_COLORS[ci]);
            idx++;
        }
    }
    state->num_bodies = idx;
}

/* Scene 4: Stress Test — 15 boxes in a single tower */
static void setup_scene_stress(app_state *state)
{
    state->num_bodies = 15;
    vec3 half = vec3_create(BOX_HALF, BOX_HALF, BOX_HALF);

    for (int i = 0; i < 15; i++) {
        float y = BOX_HALF + (float)i * (2.0f * BOX_HALF + STACK_OFFSET);
        int ci = i % NUM_BODY_COLORS;
        make_box_body(state, i,
                      vec3_create(0.0f, y, 0.0f),
                      BOX_MASS, half, BODY_COLORS[ci]);
    }
}

/* Dispatch scene setup by index */
static void set_scene(app_state *state, int idx)
{
    state->scene_index = idx % NUM_SCENES;

    /* Reset accumulator */
    state->accumulator = 0.0f;

    /* Clear manifold cache */
    if (state->manifold_cache) {
        forge_physics_manifold_cache_free(&state->manifold_cache);
        state->manifold_cache = NULL;
    }
    state->manifold_count = 0;
    state->total_contact_count = 0;
    state->gjk_pair_count = 0;
    state->gjk_hit_count = 0;

    /* Reset SAP */
    forge_physics_sap_destroy(&state->sap);
    forge_physics_sap_init(&state->sap);

    switch (state->scene_index) {
    case 0: setup_scene_tower(state);   state->solver_iterations = 40; break;
    case 1: setup_scene_pyramid(state); state->solver_iterations = 20; break;
    case 2: setup_scene_wall(state);    state->solver_iterations = 20; break;
    case 3: setup_scene_stress(state);  state->solver_iterations = 60; break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Physics Step
 * ═══════════════════════════════════════════════════════════════════════════ */

static void physics_step(app_state *state, float dt)
{
    (void)dt; /* always use PHYSICS_DT */
    int n = state->num_bodies;

    /* Save previous state for render interpolation */
    for (int i = 0; i < n; i++) {
        state->bodies[i].prev_position    = state->bodies[i].position;
        state->bodies[i].prev_orientation = state->bodies[i].orientation;
    }

    /* ── Phase 1: Apply gravity and integrate velocities ───────── */
    for (int i = 0; i < n; i++) {
        if (state->bodies[i].inv_mass > 0.0f) {
            vec3 gforce = vec3_scale(GRAVITY, state->bodies[i].mass);
            forge_physics_rigid_body_apply_force(&state->bodies[i], gforce);
        }
    }
    for (int i = 0; i < n; i++) {
        forge_physics_rigid_body_integrate_velocities(
            &state->bodies[i], PHYSICS_DT);
    }

    /* ── Phase 2: Detect collisions at current positions ───────── */
    state->manifold_count = 0;

    /* Shared active pair keys for cache pruning */
    uint64_t active_keys[MAX_MANIFOLDS];
    int active_key_count = 0;

    /* Ground contacts */
    int ground_count = collect_ground_manifolds(
        state, &state->manifolds[0], MAX_MANIFOLDS,
        active_keys, &active_key_count);
    state->manifold_count += ground_count;

    /* Body-body contacts (when >1 body) */
    if (n > 1) {
        int body_count = collect_body_manifolds(
            state, &state->manifolds[ground_count],
            MAX_MANIFOLDS - ground_count,
            active_keys, &active_key_count);
        state->manifold_count += body_count;
    }

    /* Prune stale cache entries */
    forge_physics_manifold_cache_prune(
        &state->manifold_cache, active_keys, active_key_count);

    /* ── Phase 3: Build solver config from UI values ──────────── */
    ForgePhysicsSolverConfig cfg = forge_physics_solver_config_default();
    cfg.baumgarte_factor    = state->baumgarte_factor;
    cfg.penetration_slop    = state->penetration_slop;

    /* ── Phase 4: Solve velocity constraints ──────────────────── */
    int mc = state->manifold_count;
    state->total_contact_count = 0;

    if (mc > 0) {
        forge_physics_si_solve(state->manifolds, mc,
                               state->bodies, n,
                               state->solver_iterations,
                               PHYSICS_DT, state->use_warm_start,
                               state->si_workspace, &cfg);

        /* Count total contacts */
        for (int mi = 0; mi < mc; mi++) {
            state->total_contact_count += state->si_workspace[mi].count;
        }
    }

    /* ── Phase 5: Write solved impulses back to manifold cache ──
     * forge_physics_si_solve() stores converged impulses into the
     * manifold array internally (Phase 4). We push those into the
     * persistent cache so the next frame can warm-start from them. */
    if (mc > 0) {
        for (int mi = 0; mi < mc; mi++) {
            forge_physics_manifold_cache_store(
                &state->manifold_cache, &state->manifolds[mi]);
        }
    }

    /* ── Phase 6: Position correction ─────────────────────────── */
    if (mc > 0) {
        forge_physics_si_correct_positions(
            state->manifolds, mc, state->bodies, n,
            cfg.correction_fraction, cfg.correction_slop);
    }

    /* ── Phase 7: Integrate positions ─────────────────────────── */
    for (int i = 0; i < n; i++) {
        forge_physics_rigid_body_integrate_positions(
            &state->bodies[i], PHYSICS_DT);
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

    /* Initialize scene renderer (Blinn-Phong, shadow map, grid, sky, UI) */
    ForgeSceneConfig cfg = forge_scene_default_config(
        "Physics 14 - Stacking Stability");
    cfg.cam_start_pos = vec3_create(0.0f, 8.0f, 20.0f);
    cfg.cam_start_yaw = 0.0f;
    cfg.cam_start_pitch = -0.20f;
    cfg.font_path = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";

    if (!forge_scene_init(&state->scene, &cfg, argc, argv))
        return SDL_APP_FAILURE;

    /* Generate and upload sphere mesh (for contact markers) */
    ForgeShape sphere = forge_shapes_sphere(16, 8);
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
    ForgeShape cube = forge_shapes_cube(1, 1);
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
    state->solver_iterations = DEFAULT_SOLVER_ITERS;
    state->use_warm_start    = true;
    state->baumgarte_factor  = 0.15f;   /* gentler than default 0.2 — less jitter for tall stacks */
    state->penetration_slop  = 0.005f;  /* tighter than default 0.01 — cleaner contact resolution */
    state->speed_scale       = 1.0f;
    state->show_contacts     = false;
    state->manifold_cache    = NULL;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    /* Initialize SAP */
    forge_physics_sap_init(&state->sap);

    /* Parse --scene argument for starting scene index (0-3) */
    int start_scene = 0;
    for (int i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            start_scene = SDL_atoi(argv[i + 1]);
            if (start_scene < 0) start_scene = 0;
            if (start_scene >= NUM_SCENES) start_scene = NUM_SCENES - 1;
            break;
        }
    }

    /* Set up first scene */
    set_scene(state, start_scene);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = appstate;
    ForgeScene *s = &state->scene;

    /* forge_scene_handle_event manages mouse capture, Escape, and quit */
    SDL_AppResult result = forge_scene_handle_event(s, event);
    if (result != SDL_APP_CONTINUE) return result;

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        switch (event->key.scancode) {
        /* Pause / resume */
        case SDL_SCANCODE_P:
            state->paused = !state->paused;
            break;

        /* Reset simulation */
        case SDL_SCANCODE_R:
            set_scene(state, state->scene_index);
            break;

        /* Toggle slow motion */
        case SDL_SCANCODE_T:
            state->speed_scale = (state->speed_scale < 0.5f) ? 1.0f : 0.25f;
            break;

        /* Cycle scenes */
        case SDL_SCANCODE_TAB:
            set_scene(state, state->scene_index + 1);
            break;

        /* Toggle contact visualization */
        case SDL_SCANCODE_C:
            if (!s->mouse_captured)
                state->show_contacts = !state->show_contacts;
            break;

        /* Toggle warm-starting (only when mouse is free) */
        case SDL_SCANCODE_W:
            if (!s->mouse_captured)
                state->use_warm_start = !state->use_warm_start;
            break;

        /* Increase solver iterations */
        case SDL_SCANCODE_EQUALS:
            if (state->solver_iterations < MAX_SOLVER_ITERS)
                state->solver_iterations += 5;
            break;

        /* Decrease solver iterations */
        case SDL_SCANCODE_MINUS:
            state->solver_iterations -= 5;
            if (state->solver_iterations < 1)
                state->solver_iterations = 1;
            break;

        /* Increase Baumgarte factor */
        case SDL_SCANCODE_B:
            if (!s->mouse_captured) {
                state->baumgarte_factor += 0.05f;
                if (state->baumgarte_factor > 0.5f)
                    state->baumgarte_factor = 0.5f;
            }
            break;

        /* Decrease Baumgarte factor */
        case SDL_SCANCODE_N:
            if (!s->mouse_captured) {
                state->baumgarte_factor -= 0.05f;
                if (state->baumgarte_factor < 0.05f)
                    state->baumgarte_factor = 0.05f;
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

    /* ── Fixed-timestep physics ────────────────────────────────── */
    if (!state->paused) {
        float step_dt = dt * state->speed_scale;
        state->accumulator += step_dt;
        while (state->accumulator >= PHYSICS_DT) {
            physics_step(state, PHYSICS_DT);
            state->accumulator -= PHYSICS_DT;
        }
    }

    /* ── Collect instanced draw data ──────────────────────────── */
    float alpha = state->accumulator / PHYSICS_DT;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    if (state->paused) alpha = 1.0f;

    /* Box instances — all bodies are cubes */
    ForgeSceneColoredInstance box_instances[MAX_BODIES];
    int box_count = 0;

    for (int i = 0; i < state->num_bodies; i++) {
        vec3 pos = vec3_lerp(state->bodies[i].prev_position,
                             state->bodies[i].position, alpha);
        quat ori = quat_slerp(state->bodies[i].prev_orientation,
                               state->bodies[i].orientation, alpha);

        vec3 he = state->shapes[i].data.box.half_extents;
        mat4 scale_m = mat4_scale(vec3_create(he.x, he.y, he.z));
        mat4 rot_m   = quat_to_mat4(ori);
        mat4 trans_m = mat4_translate(pos);
        box_instances[box_count].transform =
            mat4_multiply(trans_m, mat4_multiply(rot_m, scale_m));
        SDL_memcpy(box_instances[box_count].color,
                   state->body_colors[i], 4 * sizeof(float));
        box_count++;
    }

    /* Contact marker instances — small spheres at contact points */
    ForgeSceneColoredInstance contact_instances[MAX_CONTACT_MARKERS];
    int contact_count = 0;

    if (state->show_contacts) {
        for (int mi = 0; mi < state->manifold_count; mi++) {
            const ForgePhysicsManifold *m = &state->manifolds[mi];
            for (int ci = 0; ci < m->count; ci++) {
                if (contact_count >= MAX_CONTACT_MARKERS) break;

                const ForgePhysicsManifoldContact *c = &m->contacts[ci];
                vec3 pt = c->world_point;

                /* Color: lerp from green (shallow) to red (deep penetration).
                 * Clamp penetration to [0, 0.1] for color mapping. */
                float depth_frac = c->penetration * DEPTH_COLOR_SCALE;
                if (depth_frac < 0.0f) depth_frac = 0.0f;
                if (depth_frac > 1.0f) depth_frac = 1.0f;

                float cr = depth_frac;
                float cg = 1.0f - depth_frac;

                mat4 ct_model = mat4_multiply(
                    mat4_translate(pt),
                    mat4_scale_uniform(CONTACT_MARKER_RADIUS));
                contact_instances[contact_count].transform = ct_model;
                contact_instances[contact_count].color[0] = cr;
                contact_instances[contact_count].color[1] = cg;
                contact_instances[contact_count].color[2] = 0.0f;
                contact_instances[contact_count].color[3] = 1.0f;
                contact_count++;
            }
        }
    }

    /* Upload all instance buffers — batch into one copy pass */
    SDL_GPUBuffer *box_inst_buf     = NULL;
    SDL_GPUBuffer *contact_inst_buf = NULL;

    forge_scene_begin_deferred_uploads(s);
    if (box_count > 0) {
        box_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            box_instances,
            (Uint32)(box_count * sizeof(ForgeSceneColoredInstance)));
    }
    if (contact_count > 0) {
        contact_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            contact_instances,
            (Uint32)(contact_count * sizeof(ForgeSceneColoredInstance)));
    }
    forge_scene_end_deferred_uploads(s);

    /* ── Shadow pass ─────────────────────────────────────────────── */
    forge_scene_begin_shadow_pass(s);

    if (box_inst_buf) {
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->cube_vb, state->cube_ib,
            (Uint32)state->cube_index_count,
            box_inst_buf, (Uint32)box_count);
    }
    /* Contact markers are too small to cast meaningful shadows */
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */
    forge_scene_begin_main_pass(s);

    float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (box_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->cube_vb, state->cube_ib,
            (Uint32)state->cube_index_count,
            box_inst_buf, (Uint32)box_count, white);
    }
    if (contact_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            (Uint32)state->sphere_index_count,
            contact_inst_buf, (Uint32)contact_count, white);
    }

    /* Contact normal debug lines */
    if (state->show_contacts) {
        for (int mi = 0; mi < state->manifold_count; mi++) {
            const ForgePhysicsManifold *m = &state->manifolds[mi];
            for (int ci = 0; ci < m->count; ci++) {
                const ForgePhysicsManifoldContact *c = &m->contacts[ci];
                vec3 pt = c->world_point;
                float line_len = c->penetration * NORMAL_LINE_SCALE;
                if (line_len < NORMAL_LINE_MIN) line_len = NORMAL_LINE_MIN;
                vec3 end = vec3_add(pt, vec3_scale(m->normal, line_len));

                /* Color: same green-to-red gradient as marker */
                float depth_frac = c->penetration * DEPTH_COLOR_SCALE;
                if (depth_frac < 0.0f) depth_frac = 0.0f;
                if (depth_frac > 1.0f) depth_frac = 1.0f;

                vec4 line_color = vec4_create(depth_frac, 1.0f - depth_frac,
                                              0.0f, 1.0f);
                forge_scene_debug_line(s, pt, end, line_color, false);
            }
        }
        forge_scene_draw_debug_lines(s);
    }

    forge_scene_draw_grid(s);
    forge_scene_end_main_pass(s);

    /* Release per-frame instance buffers */
    SDL_GPUDevice *dev = forge_scene_device(s);
    if (box_inst_buf)     SDL_ReleaseGPUBuffer(dev, box_inst_buf);
    if (contact_inst_buf) SDL_ReleaseGPUBuffer(dev, contact_inst_buf);

    /* ── UI pass ─────────────────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);

    ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
    char buf[128];

    if (wctx && forge_ui_wctx_window_begin(wctx, "Stacking Stability",
                                            &state->ui_window)) {
        ForgeUiContext *ui = wctx->ctx;

        /* Scene title */
        SDL_snprintf(buf, sizeof(buf), "Scene %d: %s",
                     state->scene_index + 1, SCENE_NAMES[state->scene_index]);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        SDL_snprintf(buf, sizeof(buf), "Bodies: %d", state->num_bodies);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Solver iterations */
        SDL_snprintf(buf, sizeof(buf), "Iterations: %d  [+/-]",
                     state->solver_iterations);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        /* Baumgarte factor */
        SDL_snprintf(buf, sizeof(buf), "Baumgarte: %.3f  [B/N]",
                     (double)state->baumgarte_factor);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        /* Penetration slop */
        SDL_snprintf(buf, sizeof(buf), "Slop: %.4f m",
                     (double)state->penetration_slop);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        /* Warm-start toggle */
        SDL_snprintf(buf, sizeof(buf), "Warm-Start: %s  [W]",
                     state->use_warm_start ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        /* Show contacts toggle */
        SDL_snprintf(buf, sizeof(buf), "Show Contacts: %s  [C]",
                     state->show_contacts ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Contact stats */
        SDL_snprintf(buf, sizeof(buf), "Contacts: %d",
                     state->total_contact_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        SDL_snprintf(buf, sizeof(buf), "Manifolds: %d",
                     state->manifold_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        SDL_snprintf(buf, sizeof(buf), "SAP pairs: %d, hits: %d",
                     state->gjk_pair_count, state->gjk_hit_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Speed / pause */
        SDL_snprintf(buf, sizeof(buf), "Speed: %.0fx  %s",
                     (double)state->speed_scale,
                     state->paused ? "[PAUSED]" : "");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Controls */
        forge_ui_ctx_label_layout(ui, "Tab: next scene  R: reset",
                                  LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "P: pause  T: slow-mo",
                                  LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "C: contacts  W: warm-start",
                                  LABEL_HEIGHT);

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

    /* Free SAP */
    forge_physics_sap_destroy(&state->sap);

    /* Free manifold cache */
    if (state->manifold_cache) {
        forge_physics_manifold_cache_free(&state->manifold_cache);
    }

    /* Destroy scene renderer */
    forge_scene_destroy(&state->scene);

    SDL_free(state);
}
