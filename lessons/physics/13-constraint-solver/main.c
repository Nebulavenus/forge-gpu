/*
 * Physics Lesson 13 — Constraint Solver
 *
 * Demonstrates: Generalized velocity-level joint constraints with
 * sequential impulse solving (Gauss-Seidel iteration). Three joint
 * types — ball-socket, hinge, and slider — are solved alongside
 * contact constraints in a unified iteration loop. Warm-starting
 * from previous-frame impulses accelerates convergence.
 *
 * Four selectable scenes:
 *   1. Ball-Socket Pendulum — chain of bodies swinging freely
 *   2. Hinge Gate — door and turnstile rotating on fixed axes
 *   3. Slider Piston — bodies sliding along constrained axes
 *   4. Combined — all joint types interacting together
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   Tab               — cycle scenes
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

/* Physics library — rigid bodies, contacts, joint constraints */
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
#define MAX_BODIES            32              /* maximum rigid bodies */
#define MAX_JOINTS            32              /* maximum joint constraints */
#define MAX_MANIFOLDS         64              /* maximum contact manifolds */

/* Common physics parameters */
#define GROUND_Y              0.0f            /* ground plane Y coordinate */
#define DEFAULT_DAMPING       0.99f           /* linear velocity damping */
#define DEFAULT_ANG_DAMPING   0.98f           /* angular velocity damping */
#define DEFAULT_RESTIT        0.3f            /* coefficient of restitution */
#define DEFAULT_MU_S          0.6f            /* static friction */
#define DEFAULT_MU_D          0.4f            /* dynamic friction */
#define DEFAULT_SOLVER_ITERS  20              /* solver iteration count */
#define MAX_SOLVER_ITERS     100              /* upper cap for iterations */
#define CONTACT_CORRECTION    0.4f            /* contact position correction fraction */
#define JOINT_CORRECTION      0.2f            /* joint position correction fraction */
#define DEFAULT_MASS          2.0f            /* default body mass (kg) */

/* Gravity */
static const vec3 GRAVITY = { 0.0f, -9.81f, 0.0f };

/* Number of scenes */
#define NUM_SCENES            4

/* UI layout */
#define PANEL_X              10.0f            /* window initial X position */
#define PANEL_Y              10.0f            /* window initial Y position */
#define PANEL_W             450.0f            /* window width */
#define PANEL_H             550.0f            /* window height */
#define LABEL_HEIGHT          18.0f
#define LABEL_SPACER          4.0f

/* Joint anchor visualization sphere scale */
#define ANCHOR_SPHERE_SCALE   0.06f

/* Colors */
static const float COLOR_RED[]     = { 0.85f, 0.2f,  0.2f,  1.0f };
static const float COLOR_BLUE[]    = { 0.2f,  0.35f, 0.85f, 1.0f };
static const float COLOR_GREEN[]   = { 0.2f,  0.75f, 0.3f,  1.0f };
static const float COLOR_YELLOW[]  = { 0.9f,  0.8f,  0.2f,  1.0f };
static const float COLOR_ORANGE[]  = { 0.9f,  0.5f,  0.15f, 1.0f };
static const float COLOR_PURPLE[]  = { 0.6f,  0.25f, 0.8f,  1.0f };
static const float COLOR_CYAN[]    = { 0.2f,  0.7f,  0.8f,  1.0f };
static const float COLOR_GRAY[]    = { 0.5f,  0.5f,  0.5f,  1.0f };
static const float COLOR_ANCHOR_A[] = { 0.1f, 0.9f, 0.2f, 1.0f };  /* green */
static const float COLOR_ANCHOR_B[] = { 0.9f, 0.1f, 0.2f, 1.0f };  /* red */

/* Slider axis debug line visualization */
#define SLIDER_LINE_HALF_LENGTH 4.0f   /* how far the axis line extends each way */

/* Scene names */
static const char *SCENE_NAMES[] = {
    "Ball-Socket Pendulum",
    "Hinge Gate",
    "Slider Piston",
    "Combined"
};

/* ── Types ────────────────────────────────────────────────────────── */

/* Which shape to render for a body */
typedef enum {
    SHAPE_SPHERE = 0,
    SHAPE_CUBE   = 1
} ShapeType;

/* Per-body render metadata */
typedef struct {
    ShapeType shape;
    float     scale[3];    /* render scale (x, y, z) */
    float     color[4];
} BodyRenderInfo;

typedef struct app_state {
    ForgeScene scene;       /* rendering, camera, shadow map, grid, sky, UI */

    /* GPU resources — app-owned geometry */
    SDL_GPUBuffer *sphere_vb;    /* sphere vertex buffer (interleaved pos+norm)  */
    SDL_GPUBuffer *sphere_ib;    /* sphere index buffer (uint32)                 */
    SDL_GPUBuffer *cube_vb;      /* cube vertex buffer (interleaved pos+norm)    */
    SDL_GPUBuffer *cube_ib;      /* cube index buffer (uint32)                   */
    int sphere_index_count;      /* number of indices in sphere mesh             */
    int cube_index_count;        /* number of indices in cube mesh               */

    /* Physics state */
    ForgePhysicsRigidBody bodies[MAX_BODIES];  /* rigid body dynamics array     */
    BodyRenderInfo        render_info[MAX_BODIES]; /* per-body render metadata  */
    int                   num_bodies;          /* active body count             */

    /* Joint constraints */
    ForgePhysicsJoint          joints[MAX_JOINTS];      /* joint definitions    */
    ForgePhysicsJointSolverData joint_workspace[MAX_JOINTS]; /* solver scratch  */
    int                        num_joints;              /* active joint count   */

    /* Contact manifolds (ground contacts) */
    ForgePhysicsManifold       manifolds[MAX_MANIFOLDS];   /* per-step manifolds */
    ForgePhysicsSIManifold     si_workspace[MAX_MANIFOLDS]; /* SI solver scratch */
    ForgePhysicsManifoldCacheEntry *manifold_cache; /* persistent warm-start map */
    int                        manifold_count;      /* manifolds this step      */

    /* Solver configuration */
    int   solver_iterations;     /* Gauss-Seidel iteration count (1..100)       */
    bool  use_warm_start;        /* true = apply cached impulses before solve   */

    /* Timing / simulation control */
    float accumulator;           /* fixed-timestep accumulator (seconds)        */
    bool  paused;                /* true = physics frozen, camera still works   */
    float speed_scale;           /* 1.0 = normal, 0.25 = slow motion           */

    /* Scene management */
    int   scene_index;           /* current scene (0..NUM_SCENES-1)             */

    /* UI window state */
    ForgeUiWindowState ui_window; /* draggable panel position and collapse      */
} app_state;

/* ── Helper: Create a dynamic rigid body ──────────────────────────── */

static int add_body(app_state *state, vec3 pos, float mass,
                    ShapeType shape, float sx, float sy, float sz,
                    const float color[4])
{
    if (state->num_bodies >= MAX_BODIES) return -1;
    int idx = state->num_bodies++;

    ForgePhysicsRigidBody *b = &state->bodies[idx];
    SDL_memset(b, 0, sizeof(*b));
    b->position         = pos;
    b->prev_position    = pos;
    b->orientation      = quat_identity();
    b->prev_orientation = quat_identity();
    b->mass             = mass;
    b->inv_mass    = (mass > FORGE_PHYSICS_EPSILON) ? (1.0f / mass) : 0.0f;
    b->restitution = DEFAULT_RESTIT;
    b->damping     = DEFAULT_DAMPING;
    b->angular_damping = DEFAULT_ANG_DAMPING;

    /* Box inertia for cubes, sphere inertia for spheres */
    if (mass > FORGE_PHYSICS_EPSILON) {
        if (shape == SHAPE_CUBE) {
            forge_physics_rigid_body_set_inertia_box(b,
                vec3_create(sx, sy, sz));
        } else {
            forge_physics_rigid_body_set_inertia_sphere(b, sx);
        }
    }

    BodyRenderInfo *ri = &state->render_info[idx];
    ri->shape = shape;
    ri->scale[0] = sx;
    ri->scale[1] = sy;
    ri->scale[2] = sz;
    SDL_memcpy(ri->color, color, sizeof(float) * 4);

    return idx;
}

/* ── Scene Setup Functions ────────────────────────────────────────── */

/* Scene 1: Ball-Socket Pendulum — chain of 4 bodies hanging from world anchor */
static void setup_scene_pendulum(app_state *state)
{
    state->num_bodies = 0;
    state->num_joints = 0;

    float link_size = 0.25f;
    float top_y = 5.0f;
    /* Arm length between link centers — longer than 2*link_size so the
     * chain hangs with visible gaps between the spheres. */
    float link_gap = 1.2f;
    float half_arm = link_gap * 0.5f;

    const float *link_colors[] = { COLOR_RED, COLOR_BLUE, COLOR_YELLOW, COLOR_GREEN };

    /* Create chain links — positions aligned so joint anchors coincide.
     * Body 0 center sits one half-arm below the world anchor (top_y). */
    for (int i = 0; i < 4; i++) {
        float y = top_y - half_arm - (float)i * link_gap;
        add_body(state, vec3_create(0, y, 0), DEFAULT_MASS,
                 SHAPE_SPHERE, link_size, link_size, link_size,
                 link_colors[i]);
    }
    /* Start swinging via initial velocity instead of offset position */
    state->bodies[0].velocity = vec3_create(3.0f, 0, 0);

    /* Joint 0: world anchor to body 0 */
    state->joints[0] = forge_physics_joint_ball_socket(
        0, -1,
        vec3_create(0, half_arm, 0),             /* top anchor on body 0 */
        vec3_create(0, top_y, 0));               /* world anchor */
    state->num_joints = 1;

    /* Joints 1-3: body-to-body chain */
    for (int i = 0; i < 3; i++) {
        state->joints[state->num_joints] = forge_physics_joint_ball_socket(
            i, i + 1,
            vec3_create(0, -half_arm, 0),        /* bottom anchor on body i */
            vec3_create(0,  half_arm, 0));        /* top anchor on body i+1 */
        state->num_joints++;
    }
}

/* Scene 2: Hinge Gate — door and turnstile */
static void setup_scene_hinge(app_state *state)
{
    state->num_bodies = 0;
    state->num_joints = 0;

    /* Door: box hinged on left edge along Y axis */
    float door_w = 2.0f, door_h = 3.0f, door_d = 0.2f;
    int door = add_body(state,
        vec3_create(door_w * 0.5f, door_h * 0.5f + 0.5f, 0),
        3.0f, SHAPE_CUBE,
        door_w * 0.5f, door_h * 0.5f, door_d * 0.5f,
        COLOR_BLUE);

    /* Hinge at left edge, world anchor */
    state->joints[0] = forge_physics_joint_hinge(
        door, -1,
        vec3_create(-door_w * 0.5f, 0, 0),      /* left edge of door */
        vec3_create(0, door_h * 0.5f + 0.5f, 0), /* world hinge point */
        vec3_create(0, 1, 0));                    /* Y-axis rotation */
    state->num_joints = 1;

    /* Give door initial angular velocity to swing */
    state->bodies[door].angular_velocity = vec3_create(0, 2.0f, 0);

    /* Turnstile: box rotating around vertical axis through center */
    float turn_w = 2.5f, turn_h = 0.3f, turn_d = 0.3f;
    int turn = add_body(state,
        vec3_create(5, 1.5f, 0),
        2.0f, SHAPE_CUBE,
        turn_w * 0.5f, turn_h * 0.5f, turn_d * 0.5f,
        COLOR_ORANGE);

    state->joints[1] = forge_physics_joint_hinge(
        turn, -1,
        vec3_create(0, 0, 0),                    /* center of turnstile */
        vec3_create(5, 1.5f, 0),                 /* world pivot */
        vec3_create(0, 1, 0));                    /* Y-axis */
    state->num_joints = 2;

    state->bodies[turn].angular_velocity = vec3_create(0, 3.0f, 0);

    /* Second hinge: pendulum with Z-axis rotation */
    float pend_r = 0.2f;
    float pend_arm = 0.6f;      /* distance from sphere center to hinge point */
    int pend = add_body(state,
        vec3_create(-4, 3, 0),
        1.5f, SHAPE_SPHERE, pend_r, pend_r, pend_r,
        COLOR_PURPLE);

    state->joints[2] = forge_physics_joint_hinge(
        pend, -1,
        vec3_create(0, pend_arm, 0),
        vec3_create(-4, 5, 0),
        vec3_create(0, 0, 1));                    /* Z-axis rotation */
    state->num_joints = 3;
}

/* Scene 3: Slider Piston — bodies sliding along constrained axes */
static void setup_scene_slider(app_state *state)
{
    state->num_bodies = 0;
    state->num_joints = 0;

    /* Horizontal slider: box sliding along X axis */
    float box_h = 0.4f;
    int hslider = add_body(state,
        vec3_create(0, 2, 0),
        2.0f, SHAPE_CUBE,
        box_h, box_h, box_h,
        COLOR_CYAN);

    state->joints[0] = forge_physics_joint_slider(
        hslider, -1,
        vec3_create(0, 0, 0),
        vec3_create(0, 2, 0),
        vec3_create(1, 0, 0));  /* slide along X */
    state->num_joints = 1;

    /* Give initial velocity */
    state->bodies[hslider].velocity = vec3_create(3.0f, 0, 0);

    /* Vertical slider: sphere sliding along Y axis (gravity pulls down) */
    float sph_r = 0.35f;
    int vslider = add_body(state,
        vec3_create(4, 4, 0),
        1.5f, SHAPE_SPHERE,
        sph_r, sph_r, sph_r,
        COLOR_YELLOW);

    state->joints[1] = forge_physics_joint_slider(
        vslider, -1,
        vec3_create(0, 0, 0),
        vec3_create(4, 4, 0),
        vec3_create(0, 1, 0));  /* slide along Y */
    state->num_joints = 2;

    /* Diagonal slider: box on a 45-degree track */
    int dslider = add_body(state,
        vec3_create(-4, 3, 0),
        2.0f, SHAPE_CUBE,
        box_h, box_h, box_h,
        COLOR_PURPLE);

    vec3 diag_axis = vec3_normalize(vec3_create(1, 1, 0));
    state->joints[2] = forge_physics_joint_slider(
        dslider, -1,
        vec3_create(0, 0, 0),
        vec3_create(-4, 3, 0),
        diag_axis);
    state->num_joints = 3;
}

/* Scene 4: Combined — all joint types together */
static void setup_scene_combined(app_state *state)
{
    state->num_bodies = 0;
    state->num_joints = 0;

    /* Ball-socket pendulum (2 links) on the left */
    float link_r = 0.2f;
    float arm_len = 1.0f;       /* distance between link centers */
    float half_arm = arm_len * 0.5f;
    float world_anchor_y = 5.0f;
    /* Place p0 one half-arm below the world anchor */
    int p0 = add_body(state,
        vec3_create(-4, world_anchor_y - half_arm, 0), 2.0f,
        SHAPE_SPHERE, link_r, link_r, link_r, COLOR_RED);
    /* Position p1 one arm-length below p0 */
    int p1 = add_body(state,
        vec3_create(-4, world_anchor_y - half_arm - arm_len, 0), 2.0f,
        SHAPE_SPHERE, link_r, link_r, link_r, COLOR_ORANGE);

    state->joints[0] = forge_physics_joint_ball_socket(
        p0, -1,
        vec3_create(0, half_arm, 0),
        vec3_create(-4, world_anchor_y, 0));
    state->joints[1] = forge_physics_joint_ball_socket(
        p0, p1,
        vec3_create(0, -half_arm, 0),
        vec3_create(0, half_arm, 0));
    state->num_joints = 2;

    /* Give pendulum a sideways push */
    state->bodies[p0].velocity = vec3_create(3.0f, 0, 1.0f);

    /* Hinge door in the center */
    float door_w = 1.5f, door_h = 2.5f, door_d = 0.15f;
    int door = add_body(state,
        vec3_create(door_w * 0.5f, door_h * 0.5f + 0.3f, 0), 3.0f,
        SHAPE_CUBE,
        door_w * 0.5f, door_h * 0.5f, door_d * 0.5f,
        COLOR_BLUE);

    state->joints[2] = forge_physics_joint_hinge(
        door, -1,
        vec3_create(-door_w * 0.5f, 0, 0),
        vec3_create(0, door_h * 0.5f + 0.3f, 0),
        vec3_create(0, 1, 0));
    state->num_joints = 3;
    state->bodies[door].angular_velocity = vec3_create(0, 1.5f, 0);

    /* Slider on the right */
    float box_h = 0.35f;
    int slider = add_body(state,
        vec3_create(5, 3, 0), 2.0f,
        SHAPE_CUBE,
        box_h, box_h, box_h,
        COLOR_GREEN);

    state->joints[3] = forge_physics_joint_slider(
        slider, -1,
        vec3_create(0, 0, 0),
        vec3_create(5, 3, 0),
        vec3_create(0, 1, 0));  /* vertical slider */
    state->num_joints = 4;
}

/* Dispatch scene setup by index */
static void set_scene(app_state *state, int idx)
{
    state->scene_index = idx % NUM_SCENES;

    /* Reset accumulator so residual time from previous scene does not
     * leak into the fresh simulation (determinism on R/Tab reset). */
    state->accumulator = 0.0f;

    /* Clear manifold cache */
    if (state->manifold_cache) {
        forge_physics_manifold_cache_free(&state->manifold_cache);
        state->manifold_cache = NULL;
    }
    state->manifold_count = 0;

    switch (state->scene_index) {
    case 0: setup_scene_pendulum(state); break;
    case 1: setup_scene_hinge(state);    break;
    case 2: setup_scene_slider(state);   break;
    case 3: setup_scene_combined(state); break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Ground Contact Collection
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Detect ground plane contacts for all dynamic bodies. */
static int collect_ground_manifolds(
    app_state *state, ForgePhysicsManifold *out, int max_out)
{
    int count = 0;
    vec3 plane_pt = vec3_create(0, GROUND_Y, 0);
    vec3 plane_n  = vec3_create(0, 1, 0);

    for (int i = 0; i < state->num_bodies && count < max_out; i++) {
        ForgePhysicsRigidBody *b = &state->bodies[i];
        if (b->inv_mass == 0.0f) continue;

        BodyRenderInfo *ri = &state->render_info[i];
        ForgePhysicsRBContact contacts[8];
        int nc = 0;

        if (ri->shape == SHAPE_SPHERE) {
            ForgePhysicsRBContact c;
            if (forge_physics_rb_collide_sphere_plane(
                    b, i, ri->scale[0], plane_pt, plane_n,
                    DEFAULT_MU_S, DEFAULT_MU_D, &c)) {
                contacts[nc++] = c;
            }
        } else {
            vec3 half = vec3_create(ri->scale[0], ri->scale[1], ri->scale[2]);
            nc = forge_physics_rb_collide_box_plane(
                b, i, half, plane_pt, plane_n,
                DEFAULT_MU_S, DEFAULT_MU_D, contacts, 8);
        }

        if (nc > 0 && count < max_out) {
            ForgePhysicsManifold m;
            if (forge_physics_si_rb_contacts_to_manifold(
                    contacts, nc, DEFAULT_MU_S, DEFAULT_MU_D, &m)) {

                /* Warm-start from cache if available */
                forge_physics_manifold_cache_update(&state->manifold_cache, &m);

                out[count++] = m;
            }
        }
    }

    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Physics Step
 * ═══════════════════════════════════════════════════════════════════════════ */

static void physics_step(app_state *state, float dt)
{
    (void)dt; /* always use PHYSICS_DT */
    int nb = state->num_bodies;
    int nj = state->num_joints;

    /* Save previous state for render interpolation */
    for (int i = 0; i < nb; i++) {
        state->bodies[i].prev_position    = state->bodies[i].position;
        state->bodies[i].prev_orientation = state->bodies[i].orientation;
    }

    /* ── Phase 1: Apply gravity and integrate velocities ───────── */
    for (int i = 0; i < nb; i++) {
        if (state->bodies[i].inv_mass > 0.0f) {
            vec3 gforce = vec3_scale(GRAVITY, state->bodies[i].mass);
            forge_physics_rigid_body_apply_force(&state->bodies[i], gforce);
        }
    }
    for (int i = 0; i < nb; i++) {
        forge_physics_rigid_body_integrate_velocities(
            &state->bodies[i], PHYSICS_DT);
    }

    /* ── Phase 2: Detect ground contacts ───────────────────────── */
    state->manifold_count = collect_ground_manifolds(
        state, state->manifolds, MAX_MANIFOLDS);

    /* ── Phase 3: Prepare constraints ──────────────────────────── */
    int mc = state->manifold_count;
    int iters = state->solver_iterations;

    /* Prepare joint constraints */
    if (nj > 0) {
        forge_physics_joint_prepare(
            state->joints, nj, state->bodies, nb,
            PHYSICS_DT, state->joint_workspace);
    }

    /* Prepare contact constraints */
    if (mc > 0) {
        forge_physics_si_prepare(
            state->manifolds, mc, state->bodies, nb,
            PHYSICS_DT, state->use_warm_start,
            state->si_workspace);
    }

    /* ── Phase 4: Warm-start ───────────────────────────────────── */
    if (state->use_warm_start) {
        if (nj > 0) {
            forge_physics_joint_warm_start(
                state->joints, state->joint_workspace, nj,
                state->bodies, nb);
        }
        if (mc > 0) {
            forge_physics_si_warm_start(
                state->si_workspace, mc, state->bodies, nb);
        }
    }

    /* ── Phase 5: Iterative velocity solving ───────────────────── */
    for (int iter = 0; iter < iters; iter++) {
        if (nj > 0) {
            forge_physics_joint_solve_velocities(
                state->joints, state->joint_workspace, nj,
                state->bodies, nb);
        }
        if (mc > 0) {
            forge_physics_si_solve_velocities(
                state->si_workspace, mc, state->bodies, nb);
        }
    }

    /* ── Phase 6: Store converged impulses ──────────────────────── */
    if (nj > 0) {
        forge_physics_joint_store_impulses(
            state->joints, state->joint_workspace, nj);
    }
    if (mc > 0) {
        forge_physics_si_store_impulses(
            state->si_workspace, mc, state->manifolds);
        for (int mi = 0; mi < mc; mi++) {
            forge_physics_manifold_cache_update(
                &state->manifold_cache, &state->manifolds[mi]);
        }
    }

    /* ── Phase 7: Position correction ──────────────────────────── */
    if (mc > 0) {
        forge_physics_si_correct_positions(
            state->manifolds, mc, state->bodies, nb,
            CONTACT_CORRECTION, FORGE_PHYSICS_PENETRATION_SLOP);
    }
    if (nj > 0) {
        forge_physics_joint_correct_positions(
            state->joints, nj, state->bodies, nb,
            JOINT_CORRECTION, FORGE_PHYSICS_JOINT_SLOP);
    }

    /* ── Phase 8: Integrate positions ──────────────────────────── */
    for (int i = 0; i < nb; i++) {
        forge_physics_rigid_body_integrate_positions(
            &state->bodies[i], PHYSICS_DT);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Shape Upload Helper
 * ═══════════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════════
 * Joint Visualization
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Compute world-space anchor for rendering */
static vec3 get_world_anchor(const app_state *state, int body_idx,
                              vec3 local_anchor, float alpha)
{
    if (body_idx < 0 || body_idx >= state->num_bodies)
        return local_anchor;
    const ForgePhysicsRigidBody *b = &state->bodies[body_idx];
    vec3 pos = vec3_lerp(b->prev_position, b->position, alpha);
    quat orient = quat_slerp(b->prev_orientation, b->orientation, alpha);
    return vec3_add(pos, quat_rotate_vec3(orient, local_anchor));
}

/* Collect joint anchor instances into a CPU array for instanced drawing.
 * Returns the number of instances written (2 per joint: A=green, B=red). */
static int collect_joint_anchor_instances(
    const app_state *state, float alpha,
    ForgeSceneColoredInstance *out, int max_count)
{
    int count = 0;
    for (int i = 0; i < state->num_joints && count + 1 < max_count; i++) {
        const ForgePhysicsJoint *j = &state->joints[i];

        vec3 wa = get_world_anchor(state, j->body_a, j->local_anchor_a, alpha);
        vec3 wb = get_world_anchor(state, j->body_b, j->local_anchor_b, alpha);

        out[count].transform = mat4_multiply(
            mat4_translate(wa), mat4_scale_uniform(ANCHOR_SPHERE_SCALE));
        SDL_memcpy(out[count].color, COLOR_ANCHOR_A, sizeof(out[count].color));
        count++;

        out[count].transform = mat4_multiply(
            mat4_translate(wb), mat4_scale_uniform(ANCHOR_SPHERE_SCALE));
        SDL_memcpy(out[count].color, COLOR_ANCHOR_B, sizeof(out[count].color));
        count++;
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SDL App Callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state *state = SDL_calloc(1, sizeof(*state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    /* Initialize scene renderer (Blinn-Phong, shadow map, grid, sky, UI) */
    ForgeSceneConfig cfg = forge_scene_default_config(
        "Physics 13 - Constraint Solver");
    cfg.cam_start_pos = vec3_create(0.0f, 4.0f, 10.0f);
    cfg.cam_start_pitch = -0.15f;
    cfg.font_path = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";

    if (!forge_scene_init(&state->scene, &cfg, argc, argv))
        return SDL_APP_FAILURE;

    /* Generate and upload sphere mesh */
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
    state->use_warm_start = true;
    state->speed_scale = 1.0f;
    state->manifold_cache = NULL;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    /* Set up first scene */
    set_scene(state, 0);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = appstate;
    ForgeScene *s = &state->scene;

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

        default:
            break;
        }
    }

    return SDL_APP_CONTINUE;
}

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

    /* ── Collect instanced draw data ────────────────────────────── */
    /* Interpolation factor for smooth rendering between physics steps.
     * alpha blends from prev_position (alpha=0) to position (alpha=1). */
    float alpha = state->accumulator / PHYSICS_DT;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    if (state->paused) alpha = 1.0f;

    ForgeSceneColoredInstance sphere_instances[MAX_BODIES];
    ForgeSceneColoredInstance cube_instances[MAX_BODIES];
    int sphere_count = 0;
    int cube_count   = 0;

    for (int i = 0; i < state->num_bodies; i++) {
        BodyRenderInfo *ri = &state->render_info[i];

        /* Interpolate position and orientation for smooth rendering */
        vec3 pos = vec3_lerp(state->bodies[i].prev_position,
                             state->bodies[i].position, alpha);
        quat ori = quat_slerp(state->bodies[i].prev_orientation,
                               state->bodies[i].orientation, alpha);

        mat4 rot_m   = quat_to_mat4(ori);
        mat4 scale_m = mat4_scale(vec3_create(ri->scale[0], ri->scale[1],
                                               ri->scale[2]));
        mat4 trans_m = mat4_translate(pos);
        mat4 model   = mat4_multiply(trans_m, mat4_multiply(rot_m, scale_m));

        if (ri->shape == SHAPE_SPHERE) {
            sphere_instances[sphere_count].transform = model;
            SDL_memcpy(sphere_instances[sphere_count].color,
                       ri->color, 4 * sizeof(float));
            sphere_count++;
        } else {
            cube_instances[cube_count].transform = model;
            SDL_memcpy(cube_instances[cube_count].color,
                       ri->color, 4 * sizeof(float));
            cube_count++;
        }
    }

    /* Collect joint anchor instances (2 per joint: green A + red B) */
    ForgeSceneColoredInstance anchor_instances[MAX_JOINTS * 2];
    int anchor_count = collect_joint_anchor_instances(
        state, alpha, anchor_instances, MAX_JOINTS * 2);

    /* Upload all instance buffers — batch into one copy pass */
    SDL_GPUBuffer *sphere_inst_buf = NULL;
    SDL_GPUBuffer *cube_inst_buf   = NULL;
    SDL_GPUBuffer *anchor_inst_buf = NULL;

    forge_scene_begin_deferred_uploads(s);
    if (sphere_count > 0) {
        sphere_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            sphere_instances,
            (Uint32)(sphere_count * sizeof(ForgeSceneColoredInstance)));
    }
    if (cube_count > 0) {
        cube_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            cube_instances,
            (Uint32)(cube_count * sizeof(ForgeSceneColoredInstance)));
    }
    if (anchor_count > 0) {
        anchor_inst_buf = forge_scene_upload_buffer_deferred(
            s, SDL_GPU_BUFFERUSAGE_VERTEX,
            anchor_instances,
            (Uint32)(anchor_count * sizeof(ForgeSceneColoredInstance)));
    }
    forge_scene_end_deferred_uploads(s);

    /* ── Render: Shadow pass ───────────────────────────────────── */
    forge_scene_begin_shadow_pass(s);

    if (sphere_inst_buf) {
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            (Uint32)state->sphere_index_count,
            sphere_inst_buf, (Uint32)sphere_count);
    }
    if (cube_inst_buf) {
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->cube_vb, state->cube_ib,
            (Uint32)state->cube_index_count,
            cube_inst_buf, (Uint32)cube_count);
    }
    if (anchor_inst_buf) {
        forge_scene_draw_shadow_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            (Uint32)state->sphere_index_count,
            anchor_inst_buf, (Uint32)anchor_count);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Render: Main pass ─────────────────────────────────────── */
    forge_scene_begin_main_pass(s);

    float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (sphere_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            (Uint32)state->sphere_index_count,
            sphere_inst_buf, (Uint32)sphere_count, white);
    }
    if (cube_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->cube_vb, state->cube_ib,
            (Uint32)state->cube_index_count,
            cube_inst_buf, (Uint32)cube_count, white);
    }
    if (anchor_inst_buf) {
        forge_scene_draw_mesh_instanced_colored(
            s, state->sphere_vb, state->sphere_ib,
            (Uint32)state->sphere_index_count,
            anchor_inst_buf, (Uint32)anchor_count, white);
    }
    /* Draw slider axis lines as debug lines (world-space, depth-tested) */
    for (int i = 0; i < state->num_joints; i++) {
        const ForgePhysicsJoint *j = &state->joints[i];
        if (j->type != FORGE_PHYSICS_JOINT_SLIDER) continue;

        /* Get the world-space anchor and slide axis.
         * For world-attached sliders (body_b == -1), anchor_b IS the world
         * position and local_axis_a is already in world space. For body-body
         * sliders, we'd need to transform — but all L13 sliders are world-attached. */
        vec3 anchor = j->local_anchor_b;
        vec3 axis   = j->local_axis_a;

        vec3 line_start = vec3_sub(anchor,
            vec3_scale(axis, SLIDER_LINE_HALF_LENGTH));
        vec3 line_end   = vec3_add(anchor,
            vec3_scale(axis, SLIDER_LINE_HALF_LENGTH));

        vec4 line_color = vec4_create(0.8f, 0.8f, 0.3f, 1.0f);
        forge_scene_debug_line(s, line_start, line_end, line_color, false);
    }
    forge_scene_draw_debug_lines(s);

    forge_scene_draw_grid(s);
    forge_scene_end_main_pass(s);

    /* Release per-frame instance buffers */
    SDL_GPUDevice *dev = forge_scene_device(s);
    if (sphere_inst_buf)  SDL_ReleaseGPUBuffer(dev, sphere_inst_buf);
    if (cube_inst_buf)    SDL_ReleaseGPUBuffer(dev, cube_inst_buf);
    if (anchor_inst_buf)  SDL_ReleaseGPUBuffer(dev, anchor_inst_buf);

    /* ── Render: UI pass ───────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = (buttons & SDL_BUTTON_LMASK) != 0;
    forge_scene_begin_ui(s, mx, my, mouse_down);

    ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
    char buf[128];

    if (wctx && forge_ui_wctx_window_begin(wctx, "Constraint Solver",
                                            &state->ui_window)) {
        ForgeUiContext *ui = wctx->ctx;

        /* Scene title */
        SDL_snprintf(buf, sizeof(buf), "Scene %d: %s",
                     state->scene_index + 1, SCENE_NAMES[state->scene_index]);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Joint count */
        SDL_snprintf(buf, sizeof(buf), "Joints: %d  Bodies: %d",
                     state->num_joints, state->num_bodies);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        /* Joint types in scene */
        int bs_count = 0, hinge_count = 0, slider_count = 0;
        for (int i = 0; i < state->num_joints; i++) {
            switch (state->joints[i].type) {
            case FORGE_PHYSICS_JOINT_BALL_SOCKET: bs_count++; break;
            case FORGE_PHYSICS_JOINT_HINGE:       hinge_count++; break;
            case FORGE_PHYSICS_JOINT_SLIDER:      slider_count++; break;
            }
        }
        SDL_snprintf(buf, sizeof(buf), "  B-S: %d  Hinge: %d  Slider: %d",
                     bs_count, hinge_count, slider_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Solver config */
        SDL_snprintf(buf, sizeof(buf), "Iterations: %d  [+/-]",
                     state->solver_iterations);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        SDL_snprintf(buf, sizeof(buf), "Warm-Start: %s",
                     state->use_warm_start ? "ON" : "OFF");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

        SDL_snprintf(buf, sizeof(buf), "Speed: %.0fx  %s",
                     (double)state->speed_scale,
                     state->paused ? "[PAUSED]" : "");
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Per-joint constraint error */
        forge_ui_ctx_label_layout(ui, "Constraint Error:", LABEL_HEIGHT);

        float total_error = 0.0f;
        for (int i = 0; i < state->num_joints && i < 8; i++) {
            const ForgePhysicsJoint *j = &state->joints[i];
            vec3 wa = get_world_anchor(state, j->body_a, j->local_anchor_a, 1.0f);
            vec3 wb = get_world_anchor(state, j->body_b, j->local_anchor_b, 1.0f);
            vec3 err_vec = vec3_sub(wa, wb);

            /* For sliders, only measure error perpendicular to the slide
             * axis — separation along the axis is allowed motion. */
            if (j->type == FORGE_PHYSICS_JOINT_SLIDER) {
                vec3 axis = (j->body_a >= 0 && j->body_a < state->num_bodies)
                    ? quat_rotate_vec3(
                          state->bodies[j->body_a].orientation,
                          j->local_axis_a)
                    : j->local_axis_a;
                float along = vec3_dot(err_vec, axis);
                err_vec = vec3_sub(err_vec, vec3_scale(axis, along));
            }
            float err = vec3_length(err_vec);

            /* Include axis tilt error for hinge/slider constraints.
             * NOTE: this measures axis misalignment only (|axis_a × axis_b|),
             * not twist around the axis — twist would require a full
             * quaternion-difference angle. Acceptable for a diagnostic. */
            if (j->type == FORGE_PHYSICS_JOINT_HINGE ||
                j->type == FORGE_PHYSICS_JOINT_SLIDER) {
                quat qa = (j->body_a >= 0 && j->body_a < state->num_bodies)
                    ? state->bodies[j->body_a].orientation : quat_identity();
                quat qb = (j->body_b >= 0 && j->body_b < state->num_bodies)
                    ? state->bodies[j->body_b].orientation : quat_identity();
                vec3 axis_a = quat_rotate_vec3(qa, j->local_axis_a);
                vec3 axis_b = quat_rotate_vec3(qb, j->local_axis_b);
                float tilt_err = vec3_length(vec3_cross(axis_a, axis_b));
                err += tilt_err;
            }
            total_error += err;

            const char *type_str = "B-S";
            if (j->type == FORGE_PHYSICS_JOINT_HINGE) type_str = "HNG";
            if (j->type == FORGE_PHYSICS_JOINT_SLIDER) type_str = "SLD";

            SDL_snprintf(buf, sizeof(buf), "  J%d [%s]: %.4f m",
                         i, type_str, (double)err);
            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        }

        SDL_snprintf(buf, sizeof(buf), "Total: %.4f m", (double)total_error);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Ground contacts */
        SDL_snprintf(buf, sizeof(buf), "Ground Contacts: %d",
                     state->manifold_count);
        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "", LABEL_SPACER);

        /* Controls */
        forge_ui_ctx_label_layout(ui, "Tab: next scene  R: reset",
                                  LABEL_HEIGHT);
        forge_ui_ctx_label_layout(ui, "P: pause  T: slow-mo",
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

    /* Free manifold cache */
    if (state->manifold_cache) {
        forge_physics_manifold_cache_free(&state->manifold_cache);
    }

    /* Release GPU buffers */
    SDL_GPUDevice *device = state->scene.device;
    if (device) {
        if (state->sphere_vb) SDL_ReleaseGPUBuffer(device, state->sphere_vb);
        if (state->sphere_ib) SDL_ReleaseGPUBuffer(device, state->sphere_ib);
        if (state->cube_vb)   SDL_ReleaseGPUBuffer(device, state->cube_vb);
        if (state->cube_ib)   SDL_ReleaseGPUBuffer(device, state->cube_ib);
    }

    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
