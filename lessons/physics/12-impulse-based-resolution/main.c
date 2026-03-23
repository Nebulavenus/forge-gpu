/*
 * Physics Lesson 12 — Impulse-Based Resolution
 *
 * Demonstrates: Erin Catto's sequential impulse solver with accumulated
 * impulse clamping and warm-starting. The manifold-aware solver operates
 * directly on ForgePhysicsManifold arrays, using precomputed effective
 * masses and Baumgarte stabilization for position correction.
 *
 * Two selectable scenes:
 *   1. Impulse Inspector — Drop a single body onto the ground plane.
 *      UI shows per-contact impulse values (normal, tangent1, tangent2),
 *      effective mass, velocity bias, and warm-start status. Toggle
 *      warm-starting and solver type to observe convergence differences.
 *   2. Stacking Challenge — Stack of boxes with full SAP -> GJK -> EPA
 *      -> manifold -> SI solver pipeline. Toggle between the new SI
 *      solver and the old L06 solver, adjust iteration count, and
 *      toggle warm-starting.
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   Tab               — cycle scenes
 *   I                 — toggle SI solver vs L06 solver
 *   W                 — toggle warm-starting (release mouse first)
 *   = / -             — increase / decrease solver iterations
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

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Physics simulation */
#define PHYSICS_DT           (1.0f / 60.0f)  /* fixed physics timestep (s) */
#define MAX_DELTA_TIME        0.1f            /* cap frame dt to avoid spiral of death */

/* Scene limits */
#define MAX_BODIES            32              /* maximum rigid bodies */
#define MAX_MANIFOLDS         128             /* maximum manifolds per step */
#define MAX_CONTACTS          256             /* maximum contacts (for L06 path) */

/* Scene 1: Impulse Inspector */
#define S1_NUM_BODIES         1               /* single dynamic body */
#define S1_MASS               2.0f            /* body mass (kg) */
#define S1_SPHERE_RADIUS      0.5f            /* sphere radius (m) */
#define S1_BOX_HALF           0.5f            /* box half-extent (m) */
#define S1_DROP_HEIGHT         3.0f           /* initial drop height (m) */

/* Scene 2: Stacking Challenge — two stacks side by side */
#define S2_STABLE_COUNT        5              /* stable stack (5 boxes) */
#define S2_UNSTABLE_COUNT      8              /* unstable stack (8 boxes) */
#define S2_MASS               2.0f            /* per-box mass (kg) */
#define S2_BOX_HALF           0.5f            /* box half-extent (m) */
#define S2_STACK_OFFSET       0.001f          /* small gap to avoid initial overlap */
#define S2_STACK_SPACING      6.0f            /* X distance between stack centers */
#define S2_STABLE_Z           2.0f            /* Z offset: stable stack (closer) */
#define S2_UNSTABLE_Z        -2.0f            /* Z offset: unstable stack (farther) */

/* Common physics parameters */
#define GROUND_Y              0.0f            /* ground plane Y coordinate */
#define DEFAULT_DAMPING       0.99f           /* linear velocity damping per step */
#define DEFAULT_ANG_DAMPING   0.98f           /* angular velocity damping per step */
#define DEFAULT_RESTIT        0.2f            /* coefficient of restitution */
#define DEFAULT_MU_S          0.6f            /* static friction coefficient */
#define DEFAULT_MU_D          0.4f            /* dynamic friction coefficient */
#define DEFAULT_SOLVER_ITERS  20              /* default solver iteration count */

/* Number of scenes */
#define NUM_SCENES            2

/* UI layout */
#define PANEL_X              10.0f            /* panel left edge */
#define PANEL_Y              10.0f            /* panel top edge */
#define PANEL_W             440.0f            /* panel width */
#define PANEL_H             600.0f            /* panel height */
#define LABEL_HEIGHT          18.0f           /* text label height in UI panel */
#define LABEL_SPACER          4.0f            /* vertical spacing between labels */

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
    ForgePhysicsAABB cached_aabbs[MAX_BODIES]; /* per-body AABBs, recomputed each step */
    ForgePhysicsSAPWorld sap;                  /* sweep-and-prune broadphase state     */

    /* Contact / solver state */
    ForgePhysicsManifold manifolds[MAX_MANIFOLDS];       /* per-step manifolds       */
    ForgePhysicsSIManifold si_workspace[MAX_MANIFOLDS];  /* SI solver scratch space   */
    ForgePhysicsManifoldCacheEntry *manifold_cache;      /* persistent warm-start map */
    int   manifold_count;                                /* manifolds this step       */

    /* Solver configuration */
    bool  use_si_solver;     /* true = SI solver (L12), false = L06 solver */
    bool  use_warm_start;    /* true = warm-start from cached impulses */
    int   solver_iterations; /* configurable iteration count */

    /* Per-frame stats for UI */
    float total_normal_impulse;   /* sum of |j_n| across all contacts */
    float total_tangent_impulse;  /* sum of |j_t1| + |j_t2| */
    int   total_contact_count;    /* contacts resolved this frame */
    int   gjk_pair_count;         /* broadphase pairs tested */
    int   gjk_hit_count;          /* narrowphase hits */

    /* Timing / simulation control */
    float accumulator;       /* fixed-timestep accumulator (s) */
    bool  paused;            /* true = physics frozen */
    float speed_scale;       /* 1.0 = normal, 0.25 = slow motion */

    /* Scene management */
    int   scene_index;       /* 0 = inspector, 1 = stacking */

    /* UI window state */
    ForgeUiWindowState ui_window; /* draggable panel position and collapse state */
} app_state;

/* ── Helper: Upload a ForgeShape to GPU vertex + index buffers ─────── */

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

/* ── Helper: Create a body with sphere shape ─────────────────────── */

static void make_sphere_body(app_state *state, int idx,
                             vec3 pos, float mass, float radius)
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
        forge_physics_rigid_body_set_inertia_sphere(b, radius);
    }

    state->shapes[idx].type = FORGE_PHYSICS_SHAPE_SPHERE;
    state->shapes[idx].data.sphere.radius = radius;
}

/* ── Helper: Create a body with box shape ────────────────────────── */

static void make_box_body(app_state *state, int idx,
                          vec3 pos, float mass, vec3 half_ext)
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
}

/* ── Scene 1 initialization: Impulse Inspector ───────────────────── */

static void init_scene_1(app_state *state)
{
    state->num_bodies = S1_NUM_BODIES;

    /* Single dynamic box dropped from height */
    make_box_body(state, 0,
                  vec3_create(0.0f, S1_DROP_HEIGHT, 0.0f),
                  S1_MASS,
                  vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF));

    /* Warm orange color */
    state->body_colors[0][0] = 0.9f;
    state->body_colors[0][1] = 0.4f;
    state->body_colors[0][2] = 0.1f;
    state->body_colors[0][3] = 1.0f;

    /* Reset manifold cache */
    if (state->manifold_cache) {
        forge_physics_manifold_cache_free(&state->manifold_cache);
        state->manifold_cache = NULL;
    }

    /* Reset SAP */
    forge_physics_sap_destroy(&state->sap);
    forge_physics_sap_init(&state->sap);
}

/* ── Scene 2 initialization: Stacking Challenge ──────────────────── */

static void init_scene_2(app_state *state)
{
    state->num_bodies = S2_STABLE_COUNT + S2_UNSTABLE_COUNT;

    vec3 half = vec3_create(S2_BOX_HALF, S2_BOX_HALF, S2_BOX_HALF);

    /* Material colors (distinguishable from each other) */
    float colors[][4] = {
        {0.85f, 0.20f, 0.20f, 1.0f},  /* red */
        {0.20f, 0.65f, 0.30f, 1.0f},  /* green */
        {0.25f, 0.45f, 0.85f, 1.0f},  /* blue */
        {0.90f, 0.75f, 0.15f, 1.0f},  /* gold */
        {0.70f, 0.30f, 0.70f, 1.0f},  /* purple */
        {0.20f, 0.75f, 0.75f, 1.0f},  /* teal */
        {0.90f, 0.55f, 0.20f, 1.0f},  /* orange */
        {0.55f, 0.55f, 0.55f, 1.0f},  /* gray */
    };

    int idx = 0;

    /* Left stack: 5 boxes (stable with 20 iterations) — closer to camera */
    float left_x = -S2_STACK_SPACING * 0.5f;
    float left_z = S2_STABLE_Z;
    for (int i = 0; i < S2_STABLE_COUNT; i++) {
        float y = S2_BOX_HALF + (float)i * (2.0f * S2_BOX_HALF + S2_STACK_OFFSET);
        make_box_body(state, idx, vec3_create(left_x, y, left_z), S2_MASS, half);

        int ci = i % 8;
        state->body_colors[idx][0] = colors[ci][0];
        state->body_colors[idx][1] = colors[ci][1];
        state->body_colors[idx][2] = colors[ci][2];
        state->body_colors[idx][3] = colors[ci][3];
        idx++;
    }

    /* Right stack: 8 boxes (exceeds solver stability limit — will collapse) */
    float right_x = S2_STACK_SPACING * 0.5f;
    float right_z = S2_UNSTABLE_Z;
    for (int i = 0; i < S2_UNSTABLE_COUNT; i++) {
        float y = S2_BOX_HALF + (float)i * (2.0f * S2_BOX_HALF + S2_STACK_OFFSET);
        make_box_body(state, idx, vec3_create(right_x, y, right_z), S2_MASS, half);

        int ci = i % 8;
        state->body_colors[idx][0] = colors[ci][0];
        state->body_colors[idx][1] = colors[ci][1];
        state->body_colors[idx][2] = colors[ci][2];
        state->body_colors[idx][3] = colors[ci][3];
        idx++;
    }

    /* Reset manifold cache */
    if (state->manifold_cache) {
        forge_physics_manifold_cache_free(&state->manifold_cache);
        state->manifold_cache = NULL;
    }

    /* Reset SAP */
    forge_physics_sap_destroy(&state->sap);
    forge_physics_sap_init(&state->sap);
}

/* ── Scene switching ─────────────────────────────────────────────── */

static void init_current_scene(app_state *state)
{
    if (state->scene_index == 0) init_scene_1(state);
    else                         init_scene_2(state);

    state->accumulator = 0.0f;
    state->manifold_count = 0;
    state->total_normal_impulse = 0.0f;
    state->total_tangent_impulse = 0.0f;
    state->total_contact_count = 0;
    state->gjk_pair_count = 0;
    state->gjk_hit_count = 0;
}

static void reset_simulation(app_state *state)
{
    init_current_scene(state);
}

static void set_scene(app_state *state, int index)
{
    state->scene_index = index % NUM_SCENES;
    init_current_scene(state);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Physics Step
 * ═══════════════════════════════════════════════════════════════════════════ */

static const vec3 GRAVITY = {0.0f, -9.81f, 0.0f};

/* Collect ground contacts as manifolds for the SI solver.
 * Ground manifolds are routed through the manifold cache so that
 * warm-start impulses persist across frames, just like body-body
 * contacts. Without this, the solver must reconverge the full
 * weight-support force from zero every frame. */
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

        ForgePhysicsManifold manifold;
        bool has_contact = false;

        if (shape->type == FORGE_PHYSICS_SHAPE_SPHERE) {
            ForgePhysicsRBContact gc;
            if (forge_physics_rb_collide_sphere_plane(
                    body, i, shape->data.sphere.radius,
                    ground_pt, ground_n,
                    DEFAULT_MU_S, DEFAULT_MU_D, &gc)) {
                forge_physics_si_rb_contacts_to_manifold(
                    &gc, 1, DEFAULT_MU_S, DEFAULT_MU_D, &manifold);
                has_contact = true;
            }
        } else if (shape->type == FORGE_PHYSICS_SHAPE_BOX) {
            ForgePhysicsRBContact gc[8];
            int ng = forge_physics_rb_collide_box_plane(
                body, i, shape->data.box.half_extents,
                ground_pt, ground_n,
                DEFAULT_MU_S, DEFAULT_MU_D, gc, 8);
            if (ng > 0) {
                forge_physics_si_rb_contacts_to_manifold(
                    gc, ng, DEFAULT_MU_S, DEFAULT_MU_D, &manifold);
                has_contact = true;
            }
        }

        if (has_contact) {
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

/* Resolve contacts using the selected solver. */
static void solve_contacts(app_state *state,
                           ForgePhysicsManifold *manifolds,
                           int manifold_count)
{
    state->total_normal_impulse = 0.0f;
    state->total_tangent_impulse = 0.0f;
    state->total_contact_count = 0;

    if (manifold_count <= 0) return;

    if (state->use_si_solver) {
        /* ── SI solver (Lesson 12) ───────────────────────────────── */
        forge_physics_si_solve(manifolds, manifold_count,
                               state->bodies, state->num_bodies,
                               state->solver_iterations,
                               PHYSICS_DT, state->use_warm_start,
                               state->si_workspace);

        /* Compute stats */
        for (int mi = 0; mi < manifold_count; mi++) {
            for (int ci = 0; ci < state->si_workspace[mi].count; ci++) {
                const ForgePhysicsSIConstraint *sc =
                    &state->si_workspace[mi].constraints[ci];
                state->total_normal_impulse  += SDL_fabsf(sc->j_n);
                state->total_tangent_impulse += SDL_fabsf(sc->j_t1)
                                              + SDL_fabsf(sc->j_t2);
                state->total_contact_count++;
            }
        }
    } else {
        /* ── L06 solver (legacy) ─────────────────────────────────── */
        ForgePhysicsRBContact rb_contacts[MAX_CONTACTS];
        int rb_count = 0;

        for (int mi = 0; mi < manifold_count; mi++) {
            int added = forge_physics_manifold_to_rb_contacts(
                &manifolds[mi], &rb_contacts[rb_count],
                MAX_CONTACTS - rb_count);
            rb_count += added;
        }

        forge_physics_rb_resolve_contacts(rb_contacts, rb_count,
                                           state->bodies,
                                           state->num_bodies,
                                           state->solver_iterations,
                                           PHYSICS_DT);

        state->total_contact_count = rb_count;
    }
}

/* ── Physics step (both scenes) ──────────────────────────────────── */

static void physics_step(app_state *state, float dt)
{
    (void)dt;  /* we always use PHYSICS_DT */
    int n = state->num_bodies;

    /* Save previous state for render interpolation — must be done
     * before any physics changes so lerp spans one full step. */
    for (int i = 0; i < n; i++) {
        state->bodies[i].prev_position    = state->bodies[i].position;
        state->bodies[i].prev_orientation = state->bodies[i].orientation;
    }

    /* ── Phase 1: Apply gravity and integrate velocities ───────────
     * Update velocities from forces (v += a*dt) but do NOT update
     * positions yet. Positions stay at their current values so that
     * collision detection runs at the pre-integration state. */
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

    /* ── Phase 2: Detect collisions at current positions ────────── */
    state->manifold_count = 0;

    /* Shared active pair keys for cache pruning (ground + body-body) */
    uint64_t active_keys[MAX_MANIFOLDS];
    int active_key_count = 0;

    /* Ground contacts (warm-started through cache) */
    int ground_count = collect_ground_manifolds(
        state, &state->manifolds[0], MAX_MANIFOLDS,
        active_keys, &active_key_count);
    state->manifold_count += ground_count;

    /* Body-body contacts (Scene 2 only, when >1 body) */
    if (n > 1) {
        int body_count = collect_body_manifolds(
            state, &state->manifolds[ground_count],
            MAX_MANIFOLDS - ground_count,
            active_keys, &active_key_count);
        state->manifold_count += body_count;
    }

    /* Prune stale cache entries (both ground and body-body) */
    forge_physics_manifold_cache_prune(
        &state->manifold_cache, active_keys, active_key_count);

    /* ── Phase 3: Solve velocity constraints ───────────────────── */
    solve_contacts(state, state->manifolds, state->manifold_count);

    /* ── Phase 3b: Write solved impulses back to manifold cache ──
     * si_solve stores converged impulses into the local manifold
     * array. We must push those back into the persistent cache so
     * the next frame can warm-start from them. */
    for (int mi = 0; mi < state->manifold_count; mi++) {
        forge_physics_manifold_cache_update(
            &state->manifold_cache, &state->manifolds[mi]);
    }

    /* ── Phase 4: Position correction ──────────────────────────────
     * Push penetrating bodies apart along the contact normal.
     * This resolves overlap that the velocity-level Baumgarte bias
     * would otherwise take multiple frames to correct. */
    if (state->manifold_count > 0) {
        forge_physics_si_correct_positions(
            state->manifolds, state->manifold_count,
            state->bodies, n,
            0.4f,                          /* correction fraction */
            FORGE_PHYSICS_PENETRATION_SLOP /* slop tolerance */);
    }

    /* ── Phase 5: Integrate positions with corrected velocities ── */
    for (int i = 0; i < n; i++) {
        forge_physics_rigid_body_integrate_positions(
            &state->bodies[i], PHYSICS_DT);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Event Handling
 * ═══════════════════════════════════════════════════════════════════════════ */

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
            if (state->manifold_cache) {
                forge_physics_manifold_cache_free(&state->manifold_cache);
                state->manifold_cache = NULL;
            }
            reset_simulation(state);
            break;

        /* Toggle slow motion */
        case SDL_SCANCODE_T:
            state->speed_scale = (state->speed_scale < 0.5f) ? 1.0f : 0.25f;
            break;

        /* Cycle scenes */
        case SDL_SCANCODE_TAB:
            set_scene(state, state->scene_index + 1);
            break;

        /* Toggle SI solver vs L06 solver */
        case SDL_SCANCODE_I:
            state->use_si_solver = !state->use_si_solver;
            break;

        /* Toggle warm-starting (only when mouse is free) */
        case SDL_SCANCODE_W:
            if (!s->mouse_captured)
                state->use_warm_start = !state->use_warm_start;
            break;

        /* Increase solver iterations */
        case SDL_SCANCODE_EQUALS:
            if (state->solver_iterations < 50)
                state->solver_iterations++;
            break;

        /* Decrease solver iterations */
        case SDL_SCANCODE_MINUS:
            if (state->solver_iterations > 1)
                state->solver_iterations--;
            break;

        default:
            break;
        }
    }

    return SDL_APP_CONTINUE;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UI Panel
 * ═══════════════════════════════════════════════════════════════════════════ */

static void draw_ui(app_state *state, float mx, float my, bool mouse_down)
{
    ForgeScene *s = &state->scene;
    char buf[128];

    forge_scene_begin_ui(s, mx, my, mouse_down);

    ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
    if (wctx) {
        if (forge_ui_wctx_window_begin(wctx, "Impulse Solver",
                                        &state->ui_window)) {
            ForgeUiContext *ui = wctx->ctx;

            /* Scene label */
            const char *scene_name = (state->scene_index == 0)
                ? "Scene 1: Impulse Inspector"
                : "Scene 2: Stacking Challenge";
            forge_ui_ctx_label_layout(ui, scene_name, LABEL_HEIGHT);
            forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

            /* Solver type */
            SDL_snprintf(buf, sizeof(buf), "Solver: %s  [I]",
                         state->use_si_solver ? "SI (L12)" : "Iterative (L06)");
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            /* Warm-start */
            SDL_snprintf(buf, sizeof(buf), "Warm-start: %s  [W]",
                         state->use_warm_start ? "ON" : "OFF");
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            /* Iterations */
            SDL_snprintf(buf, sizeof(buf), "Iterations: %d  [+/-]",
                         state->solver_iterations);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

            /* Speed */
            SDL_snprintf(buf, sizeof(buf), "Speed: %.0fx  [T]",
                         (double)state->speed_scale);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            /* Paused */
            SDL_snprintf(buf, sizeof(buf), "Paused: %s  [P]",
                         state->paused ? "YES" : "NO");
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

            /* Contact stats */
            SDL_snprintf(buf, sizeof(buf), "Contacts: %d",
                         state->total_contact_count);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            SDL_snprintf(buf, sizeof(buf), "Manifolds: %d",
                         state->manifold_count);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

            if (state->scene_index == 1) {
                SDL_snprintf(buf, sizeof(buf), "SAP pairs: %d, hits: %d",
                             state->gjk_pair_count, state->gjk_hit_count);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
            }

            forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

            /* Impulse stats (SI solver only) */
            if (state->use_si_solver) {
                SDL_snprintf(buf, sizeof(buf), "Normal J: %.3f",
                             (double)state->total_normal_impulse);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf), "Tangent J: %.3f",
                             (double)state->total_tangent_impulse);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                /* Per-contact detail for Scene 1 */
                if (state->scene_index == 0 && state->manifold_count > 0) {
                    forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);
                    forge_ui_ctx_label_layout(ui, "Per-contact:", LABEL_HEIGHT);

                    const ForgePhysicsSIManifold *si = &state->si_workspace[0];
                    for (int ci = 0; ci < si->count && ci < 4; ci++) {
                        const ForgePhysicsSIConstraint *sc =
                            &si->constraints[ci];
                        SDL_snprintf(buf, sizeof(buf),
                                     " [%d] jn=%.2f jt1=%.2f jt2=%.2f",
                                     ci, (double)sc->j_n,
                                     (double)sc->j_t1, (double)sc->j_t2);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                        SDL_snprintf(buf, sizeof(buf),
                                     "     Kn=%.3f bias=%.3f",
                                     (double)sc->eff_mass_n,
                                     (double)sc->velocity_bias);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                }
            }

            forge_ui_wctx_window_end(wctx);
        }
    }

    forge_scene_end_ui(s);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Init / Iterate / Quit
 * ═══════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state *state = SDL_calloc(1, sizeof(*state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    /* Defaults */
    state->use_si_solver    = true;
    state->use_warm_start   = true;
    state->solver_iterations = DEFAULT_SOLVER_ITERS;
    state->speed_scale      = 1.0f;
    state->scene_index      = 1;  /* start on stacking scene */
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    /* Initialize scene renderer */
    ForgeSceneConfig cfg = forge_scene_default_config(
        "Physics 12 -- Impulse-Based Resolution");
    cfg.cam_start_pos = vec3_create(0.0f, 5.0f, 18.0f);
    cfg.cam_start_yaw = 0.0f;
    cfg.cam_start_pitch = -0.25f;
    cfg.font_path = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";

    if (!forge_scene_init(&state->scene, &cfg, argc, argv))
        return SDL_APP_FAILURE;

    /* ── Generate and upload shapes ──────────────────────────────── */

    /* Sphere */
    ForgeShape sphere = forge_shapes_sphere(32, 16);
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

    /* Cube */
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

    /* ── Initialize default scene ────────────────────────────────── */
    init_current_scene(state);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;
    float dt = forge_scene_dt(s);

    /* Cap delta time */
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* Fixed-timestep physics */
    if (!state->paused) {
        float step_dt = dt * state->speed_scale;
        state->accumulator += step_dt;
        while (state->accumulator >= PHYSICS_DT) {
            physics_step(state, PHYSICS_DT);
            state->accumulator -= PHYSICS_DT;
        }
    }

    /* Interpolation factor for smooth rendering */
    float alpha = state->accumulator / PHYSICS_DT;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    if (state->paused) alpha = 1.0f;

    /* ── Collect instanced draw data ────────────────────────────── */
    ForgeSceneColoredInstance box_instances[MAX_BODIES];
    ForgeSceneColoredInstance sphere_instances[MAX_BODIES];
    int box_count    = 0;
    int sphere_count = 0;

    for (int i = 0; i < state->num_bodies; i++) {
        /* Interpolate position and orientation */
        vec3 pos = vec3_lerp(state->bodies[i].prev_position,
                             state->bodies[i].position, alpha);
        quat ori = quat_slerp(state->bodies[i].prev_orientation,
                              state->bodies[i].orientation, alpha);

        if (state->shapes[i].type == FORGE_PHYSICS_SHAPE_BOX) {
            vec3 he = state->shapes[i].data.box.half_extents;
            mat4 scale_m = mat4_scale(vec3_create(he.x, he.y, he.z));
            mat4 rot_m   = quat_to_mat4(ori);
            mat4 trans_m = mat4_translate(pos);
            box_instances[box_count].transform =
                mat4_multiply(trans_m, mat4_multiply(rot_m, scale_m));
            SDL_memcpy(box_instances[box_count].color,
                       state->body_colors[i], 4 * sizeof(float));
            box_count++;
        } else {
            float r = state->shapes[i].data.sphere.radius;
            mat4 scale_m = mat4_scale(vec3_create(r, r, r));
            mat4 trans_m = mat4_translate(pos);
            sphere_instances[sphere_count].transform =
                mat4_multiply(trans_m, scale_m);
            SDL_memcpy(sphere_instances[sphere_count].color,
                       state->body_colors[i], 4 * sizeof(float));
            sphere_count++;
        }
    }

    /* Upload instance buffers — batch into one copy pass */
    SDL_GPUBuffer *box_inst_buf = NULL;
    SDL_GPUBuffer *sphere_inst_buf = NULL;

    forge_scene_begin_deferred_uploads(s);
    if (box_count > 0) {
        box_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            box_instances,
            (Uint32)(box_count * sizeof(ForgeSceneColoredInstance)));
    }
    if (sphere_count > 0) {
        sphere_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            sphere_instances,
            (Uint32)(sphere_count * sizeof(ForgeSceneColoredInstance)));
    }
    forge_scene_end_deferred_uploads(s);

    /* ── Shadow pass ─────────────────────────────────────────────── */
    forge_scene_begin_shadow_pass(s);

    if (box_inst_buf) {
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->cube_vb, state->cube_ib,
            state->cube_index_count, box_inst_buf, (Uint32)box_count);
    }
    if (sphere_inst_buf) {
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            state->sphere_index_count, sphere_inst_buf, (Uint32)sphere_count);
    }

    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */
    forge_scene_begin_main_pass(s);

    float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (box_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->cube_vb, state->cube_ib,
            state->cube_index_count, box_inst_buf, (Uint32)box_count, white);
    }
    if (sphere_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            state->sphere_index_count, sphere_inst_buf, (Uint32)sphere_count,
            white);
    }

    forge_scene_draw_grid(s);
    forge_scene_end_main_pass(s);

    /* Release per-frame instance buffers */
    SDL_GPUDevice *dev = forge_scene_device(s);
    if (box_inst_buf)    SDL_ReleaseGPUBuffer(dev, box_inst_buf);
    if (sphere_inst_buf) SDL_ReleaseGPUBuffer(dev, sphere_inst_buf);

    /* ── UI pass ─────────────────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = (buttons & SDL_BUTTON_LMASK) != 0;

    draw_ui(state, mx, my, mouse_down);

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
