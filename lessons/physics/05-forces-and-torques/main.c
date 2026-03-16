/*
 * Physics Lesson 05 — Forces and Torques
 *
 * Demonstrates: force generators (gravity, drag, angular drag, friction),
 * forces at arbitrary points producing torque, the force accumulator pattern,
 * combining linear and angular effects, and gyroscopic stability.
 *
 * Four selectable scenes:
 *   1. Force at a Point — off-center forces produce torque (tau = r x F)
 *   2. Force Generators — gravity + drag → terminal velocity
 *   3. Friction and Rolling — contact friction slows sliding objects
 *   4. Gyroscopic Stability — fast spin resists tipping
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
 *   1-4               — select scene
 *   Arrow keys        — apply force direction (Scene 1)
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Physics library — particles, rigid bodies, force generators */
#include "physics/forge_physics.h"

/* Procedural geometry — sphere, cube, cylinder meshes */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Physics simulation */
#define PHYSICS_DT          (1.0f / 60.0f)
#define MAX_DELTA_TIME       0.1f

/* Rigid body limits */
#define MAX_BODIES           8

/* Scene 1: Force at a Point */
#define S1_NUM_BODIES        1
#define S1_MASS              5.0f
#define S1_HALF_W            1.2f
#define S1_HALF_H            0.6f
#define S1_HALF_D            0.4f
#define S1_START_Y           3.0f
#define S1_FORCE_MAG         50.0f
#define S1_NUM_CORNERS       4
/* Corner offsets from COM (local space, front face corners) */
#define S1_CORNER_TL_X      (-S1_HALF_W)
#define S1_CORNER_TL_Y      ( S1_HALF_H)
#define S1_CORNER_TR_X      ( S1_HALF_W)
#define S1_CORNER_TR_Y      ( S1_HALF_H)
#define S1_CORNER_BL_X      (-S1_HALF_W)
#define S1_CORNER_BL_Y      (-S1_HALF_H)
#define S1_CORNER_BR_X      ( S1_HALF_W)
#define S1_CORNER_BR_Y      (-S1_HALF_H)
#define S1_LINEAR_DRAG       1.0f
#define S1_ANGULAR_DRAG      0.5f

/* Scene 2: Force Generators (gravity + drag) */
#define S2_NUM_BODIES        3
#define S2_MASS              5.0f
#define S2_SPHERE_RADIUS     0.4f
#define S2_SPACING           3.0f
#define S2_START_Y          12.0f
#define S2_DRAG_LOW          0.5f
#define S2_DRAG_MED          2.0f
#define S2_DRAG_HIGH         5.0f

/* Scene 3: Friction and Rolling */
#define S3_NUM_BODIES        2
#define S3_MASS              5.0f
#define S3_BOX_HALF          0.5f
#define S3_SPHERE_RADIUS     0.5f
#define S3_START_Y           0.6f
#define S3_INIT_VEL          8.0f
#define S3_SPACING           3.0f
#define S3_DEFAULT_FRICTION  3.0f
#define S3_DEFAULT_ANG_DRAG  1.0f

/* Scene 4: Gyroscopic Stability */
#define S4_NUM_BODIES        2
#define S4_MASS              5.0f
#define S4_DISC_RADIUS       0.8f
#define S4_DISC_HALF_H       0.1f
#define S4_SPACING           4.0f
#define S4_START_Y           4.0f
#define S4_TILT_ANGLE        0.3f
#define S4_FAST_SPIN        30.0f
#define S4_SLOW_SPIN         2.0f
#define S4_SUPPORT_OFFSET    0.5f
#define S4_ANGULAR_DRAG      0.1f

/* Common properties */
#define GROUND_Y             0.0f
#define GROUND_CONTACT_MARGIN 0.05f       /* proximity margin for ground contact detection */
#define FORCE_INPUT_EPSILON   0.001f      /* minimum force input magnitude to act on */
#define TERMINAL_VEL_MAX      999.0f      /* display cap when drag is near zero */
#define DEFAULT_DAMPING      0.99f
#define DEFAULT_ANG_DAMPING  0.99f
#define DEFAULT_RESTIT       0.5f
#define DEFAULT_GRAVITY      9.81f

/* Ground collision tuning */
#define BOUNCE_KILL_VEL      0.5f
/* Ground friction is handled by forge_physics_rigid_body_apply_friction() */

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
#define CYLINDER_SLICES     24
#define CYLINDER_STACKS      1

/* UI panel layout */
#define PANEL_X             10.0f
#define PANEL_Y             10.0f
#define PANEL_W            280.0f
#define PANEL_H            560.0f
#define LABEL_HEIGHT        24.0f
#define BUTTON_HEIGHT       30.0f
#define SLIDER_HEIGHT       28.0f

/* Slider ranges */
#define GRAVITY_MIN          0.0f
#define GRAVITY_MAX         20.0f
#define FORCE_MAG_MIN        0.0f
#define FORCE_MAG_MAX      200.0f
#define DRAG_MIN             0.0f
#define DRAG_MAX            10.0f
#define FRICTION_MIN         0.0f
#define FRICTION_MAX        20.0f
#define ANG_DRAG_MIN         0.0f
#define ANG_DRAG_MAX        10.0f
#define INIT_VEL_MIN         0.0f
#define INIT_VEL_MAX        20.0f
#define SPIN_SPEED_MIN       0.0f
#define SPIN_SPEED_MAX      60.0f
#define TILT_MIN             0.0f
#define TILT_MAX             1.5f

/* Number of scenes */
#define NUM_SCENES           4

/* Body shape type for rendering */
#define SHAPE_CUBE           0
#define SHAPE_SPHERE         1
#define SHAPE_CYLINDER       2

/* Colors (RGBA) */
#define COLOR_BOX_R         0.2f
#define COLOR_BOX_G         0.6f
#define COLOR_BOX_B         1.0f
#define COLOR_BOX_A         1.0f

#define COLOR_SPHERE1_R     1.0f
#define COLOR_SPHERE1_G     0.4f
#define COLOR_SPHERE1_B     0.2f
#define COLOR_SPHERE1_A     1.0f

#define COLOR_SPHERE2_R     0.3f
#define COLOR_SPHERE2_G     0.9f
#define COLOR_SPHERE2_B     0.3f
#define COLOR_SPHERE2_A     1.0f

#define COLOR_SPHERE3_R     0.9f
#define COLOR_SPHERE3_G     0.7f
#define COLOR_SPHERE3_B     0.1f
#define COLOR_SPHERE3_A     1.0f

#define COLOR_DISC1_R       0.9f
#define COLOR_DISC1_G       0.3f
#define COLOR_DISC1_B       0.3f
#define COLOR_DISC1_A       1.0f

#define COLOR_DISC2_R       0.3f
#define COLOR_DISC2_G       0.3f
#define COLOR_DISC2_B       0.9f
#define COLOR_DISC2_A       1.0f

#define COLOR_FRIC_BOX_R    0.8f
#define COLOR_FRIC_BOX_G    0.5f
#define COLOR_FRIC_BOX_B    0.2f
#define COLOR_FRIC_BOX_A    1.0f

#define COLOR_FRIC_SPH_R    0.2f
#define COLOR_FRIC_SPH_G    0.7f
#define COLOR_FRIC_SPH_B    0.9f
#define COLOR_FRIC_SPH_A    1.0f

/* ── Types ────────────────────────────────────────────────────────── */

/* Per-body rendering metadata */
typedef struct BodyRenderInfo {
    int   shape_type;     /* SHAPE_CUBE, SHAPE_SPHERE, or SHAPE_CYLINDER */
    vec3  render_scale;   /* scale factors for the unit mesh */
    float color[4];       /* RGBA color for this body */
} BodyRenderInfo;

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Double-sided pipeline for open geometry (uncapped cylinders) */
    SDL_GPUGraphicsPipeline *double_sided_pipeline;

    /* GPU geometry — vertex/index buffers for each shape type */
    SDL_GPUBuffer *cube_vb;
    SDL_GPUBuffer *cube_ib;
    Uint32         cube_index_count;

    SDL_GPUBuffer *sphere_vb;
    SDL_GPUBuffer *sphere_ib;
    Uint32         sphere_index_count;

    SDL_GPUBuffer *cylinder_vb;
    SDL_GPUBuffer *cylinder_ib;
    Uint32         cylinder_index_count;

    /* Physics state */
    ForgePhysicsRigidBody bodies[MAX_BODIES];
    ForgePhysicsRigidBody initial_bodies[MAX_BODIES];
    BodyRenderInfo        body_info[MAX_BODIES];
    int                   num_bodies;

    /* Simulation control */
    int   scene_index;
    float accumulator;
    bool  paused;
    float speed_scale;

    /* UI-adjustable parameters */
    float ui_gravity;
    float ui_force_magnitude;     /* Scene 1: force strength */
    int   ui_corner_index;        /* Scene 1: which corner (0-3) */
    float ui_drag_coeff;          /* Scene 2: drag coefficient */
    float ui_friction;            /* Scene 3: friction coefficient */
    float ui_angular_drag;        /* Scene 3: angular drag coefficient */
    float ui_init_velocity;       /* Scene 3: initial sliding velocity */
    float ui_fast_spin;           /* Scene 4: fast top spin speed */
    float ui_slow_spin;           /* Scene 4: slow top spin speed */
    float ui_tilt_angle;          /* Scene 4: initial tilt */

    /* Scene 1: force direction (from arrow keys) */
    vec3  force_input;

    /* UI window */
    ForgeUiWindowState ui_window;
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

/* ── Simplified ground collision for rigid bodies ────────────────── */

static void rigid_body_ground_collision(ForgePhysicsRigidBody *rb,
                                         const BodyRenderInfo *info)
{
    if (rb->inv_mass == 0.0f) return;

    float half_h;
    switch (info->shape_type) {
    case SHAPE_CUBE:    half_h = info->render_scale.y; break;
    case SHAPE_SPHERE:  half_h = info->render_scale.x; break;
    case SHAPE_CYLINDER: half_h = info->render_scale.y; break;
    default:            half_h = 0.5f; break;
    }

    float ground = GROUND_Y + half_h;
    if (rb->position.y < ground) {
        rb->position.y = ground;

        if (rb->velocity.y < 0.0f) {
            rb->velocity.y = -rb->velocity.y * rb->restitution;

            if (SDL_fabsf(rb->velocity.y) < BOUNCE_KILL_VEL) {
                rb->velocity.y = 0.0f;
            }
        }

    }
}

/* ── Scene initialization ────────────────────────────────────────── */

/* Scene 1: Force at a Point — a floating box, apply force at corners */
static void init_scene_1(app_state *state)
{
    state->num_bodies = S1_NUM_BODIES;

    state->bodies[0] = forge_physics_rigid_body_create(
        vec3_create(0.0f, S1_START_Y, 0.0f),
        S1_MASS, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, DEFAULT_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&state->bodies[0],
        vec3_create(S1_HALF_W, S1_HALF_H, S1_HALF_D));

    state->body_info[0].shape_type = SHAPE_CUBE;
    state->body_info[0].render_scale =
        vec3_create(S1_HALF_W, S1_HALF_H, S1_HALF_D);
    state->body_info[0].color[0] = COLOR_BOX_R;
    state->body_info[0].color[1] = COLOR_BOX_G;
    state->body_info[0].color[2] = COLOR_BOX_B;
    state->body_info[0].color[3] = COLOR_BOX_A;
}

/* Scene 2: Force Generators — 3 spheres with different drag */
static void init_scene_2(app_state *state)
{
    state->num_bodies = S2_NUM_BODIES;

    float colors[S2_NUM_BODIES][4] = {
        {COLOR_SPHERE1_R, COLOR_SPHERE1_G, COLOR_SPHERE1_B, COLOR_SPHERE1_A},
        {COLOR_SPHERE2_R, COLOR_SPHERE2_G, COLOR_SPHERE2_B, COLOR_SPHERE2_A},
        {COLOR_SPHERE3_R, COLOR_SPHERE3_G, COLOR_SPHERE3_B, COLOR_SPHERE3_A}
    };

    for (int i = 0; i < S2_NUM_BODIES; i++) {
        float x = ((float)i - 1.0f) * S2_SPACING;
        state->bodies[i] = forge_physics_rigid_body_create(
            vec3_create(x, S2_START_Y, 0.0f),
            S2_MASS, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, DEFAULT_RESTIT);
        forge_physics_rigid_body_set_inertia_sphere(&state->bodies[i],
            S2_SPHERE_RADIUS);

        state->body_info[i].shape_type = SHAPE_SPHERE;
        state->body_info[i].render_scale =
            vec3_create(S2_SPHERE_RADIUS, S2_SPHERE_RADIUS, S2_SPHERE_RADIUS);
        state->body_info[i].color[0] = colors[i][0];
        state->body_info[i].color[1] = colors[i][1];
        state->body_info[i].color[2] = colors[i][2];
        state->body_info[i].color[3] = colors[i][3];
    }
}

/* Scene 3: Friction and Rolling — box + sphere sliding on ground */
static void init_scene_3(app_state *state)
{
    state->num_bodies = S3_NUM_BODIES;

    /* Box */
    state->bodies[0] = forge_physics_rigid_body_create(
        vec3_create(-S3_SPACING * 0.5f, S3_START_Y, 0.0f),
        S3_MASS, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, DEFAULT_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&state->bodies[0],
        vec3_create(S3_BOX_HALF, S3_BOX_HALF, S3_BOX_HALF));
    state->bodies[0].velocity =
        vec3_create(state->ui_init_velocity, 0.0f, 0.0f);

    state->body_info[0].shape_type = SHAPE_CUBE;
    state->body_info[0].render_scale =
        vec3_create(S3_BOX_HALF, S3_BOX_HALF, S3_BOX_HALF);
    state->body_info[0].color[0] = COLOR_FRIC_BOX_R;
    state->body_info[0].color[1] = COLOR_FRIC_BOX_G;
    state->body_info[0].color[2] = COLOR_FRIC_BOX_B;
    state->body_info[0].color[3] = COLOR_FRIC_BOX_A;

    /* Sphere */
    state->bodies[1] = forge_physics_rigid_body_create(
        vec3_create(S3_SPACING * 0.5f, S3_START_Y, 0.0f),
        S3_MASS, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, DEFAULT_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&state->bodies[1],
        S3_SPHERE_RADIUS);
    state->bodies[1].velocity =
        vec3_create(state->ui_init_velocity, 0.0f, 0.0f);

    state->body_info[1].shape_type = SHAPE_SPHERE;
    state->body_info[1].render_scale =
        vec3_create(S3_SPHERE_RADIUS, S3_SPHERE_RADIUS, S3_SPHERE_RADIUS);
    state->body_info[1].color[0] = COLOR_FRIC_SPH_R;
    state->body_info[1].color[1] = COLOR_FRIC_SPH_G;
    state->body_info[1].color[2] = COLOR_FRIC_SPH_B;
    state->body_info[1].color[3] = COLOR_FRIC_SPH_A;
}

/* Scene 4: Gyroscopic Stability — two discs, fast vs slow spin */
static void init_scene_4(app_state *state)
{
    state->num_bodies = S4_NUM_BODIES;

    float spin_speeds[2] = {state->ui_fast_spin, state->ui_slow_spin};
    float disc_colors[2][4] = {
        {COLOR_DISC1_R, COLOR_DISC1_G, COLOR_DISC1_B, COLOR_DISC1_A},
        {COLOR_DISC2_R, COLOR_DISC2_G, COLOR_DISC2_B, COLOR_DISC2_A}
    };

    for (int i = 0; i < S4_NUM_BODIES; i++) {
        float x = ((float)i - 0.5f) * S4_SPACING;
        state->bodies[i] = forge_physics_rigid_body_create(
            vec3_create(x, S4_START_Y, 0.0f),
            S4_MASS, DEFAULT_DAMPING, DEFAULT_ANG_DAMPING, DEFAULT_RESTIT);
        forge_physics_rigid_body_set_inertia_cylinder(&state->bodies[i],
            S4_DISC_RADIUS, S4_DISC_HALF_H);

        /* Tilt the disc off vertical */
        state->bodies[i].orientation = quat_from_axis_angle(
            vec3_create(0, 0, 1), state->ui_tilt_angle);

        /* Spin around local Y axis (the disc's symmetry axis) */
        vec3 up_body = quat_up(state->bodies[i].orientation);
        state->bodies[i].angular_velocity =
            vec3_scale(up_body, spin_speeds[i]);

        /* Update derived data after manual orientation change */
        forge_physics_rigid_body_update_derived(&state->bodies[i]);

        /* Sync prev_* so render interpolation starts from the seeded pose */
        state->bodies[i].prev_orientation = state->bodies[i].orientation;
        state->bodies[i].prev_position    = state->bodies[i].position;

        state->body_info[i].shape_type = SHAPE_CYLINDER;
        state->body_info[i].render_scale =
            vec3_create(S4_DISC_RADIUS, S4_DISC_HALF_H, S4_DISC_RADIUS);
        state->body_info[i].color[0] = disc_colors[i][0];
        state->body_info[i].color[1] = disc_colors[i][1];
        state->body_info[i].color[2] = disc_colors[i][2];
        state->body_info[i].color[3] = disc_colors[i][3];
    }
}

/* Initialize current scene and save initial state for reset */
static void init_current_scene(app_state *state)
{
    switch (state->scene_index) {
    case 0: init_scene_1(state); break;
    case 1: init_scene_2(state); break;
    case 2: init_scene_3(state); break;
    case 3: init_scene_4(state); break;
    default:
        state->scene_index = 0;
        init_scene_1(state);
        break;
    }

    for (int i = 0; i < state->num_bodies; i++) {
        state->initial_bodies[i] = state->bodies[i];
    }

    state->accumulator = 0.0f;
    state->force_input = vec3_create(0.0f, 0.0f, 0.0f);
}

/* Reset simulation — re-initialize from current UI controls so slider
 * changes take effect immediately on reset (R key). */
static void reset_simulation(app_state *state)
{
    init_current_scene(state);
}

/* ── Physics step ────────────────────────────────────────────────── */

static void physics_step(app_state *state)
{
    /* Scene 1: Force at a Point — no gravity, user-applied force */
    if (state->scene_index == 0 && state->num_bodies > 0) {
        ForgePhysicsRigidBody *rb = &state->bodies[0];

        /* Light linear and angular drag to slow the body naturally */
        forge_physics_rigid_body_apply_linear_drag(rb, S1_LINEAR_DRAG);
        forge_physics_rigid_body_apply_angular_drag(rb, S1_ANGULAR_DRAG);

        /* Apply user force at the selected corner */
        float len = vec3_length(state->force_input);
        if (len > FORCE_INPUT_EPSILON) {
            /* Local-space corner offsets */
            vec3 corners[S1_NUM_CORNERS] = {
                vec3_create(S1_CORNER_TL_X, S1_CORNER_TL_Y, S1_HALF_D),
                vec3_create(S1_CORNER_TR_X, S1_CORNER_TR_Y, S1_HALF_D),
                vec3_create(S1_CORNER_BL_X, S1_CORNER_BL_Y, S1_HALF_D),
                vec3_create(S1_CORNER_BR_X, S1_CORNER_BR_Y, S1_HALF_D)
            };

            /* Transform corner to world space using current orientation */
            mat3 R = quat_to_mat3(rb->orientation);
            vec3 local_corner = corners[state->ui_corner_index % S1_NUM_CORNERS];
            vec3 world_offset = mat3_multiply_vec3(R, local_corner);
            vec3 world_point = vec3_add(rb->position, world_offset);

            /* Force direction from input, scaled by magnitude */
            vec3 force_dir = vec3_normalize(state->force_input);
            vec3 force = vec3_scale(force_dir, state->ui_force_magnitude);

            forge_physics_rigid_body_apply_force_at_point(rb, force,
                                                           world_point);
        }
    }

    /* Scene 2: Force Generators — gravity + variable drag */
    if (state->scene_index == 1) {
        float drag_values[S2_NUM_BODIES] = {
            S2_DRAG_LOW, state->ui_drag_coeff, S2_DRAG_HIGH
        };
        for (int i = 0; i < state->num_bodies; i++) {
            forge_physics_rigid_body_apply_gravity(&state->bodies[i],
                vec3_create(0.0f, -state->ui_gravity, 0.0f));
            forge_physics_rigid_body_apply_linear_drag(&state->bodies[i],
                drag_values[i]);
        }
    }

    /* Scene 3: Friction and Rolling */
    if (state->scene_index == 2) {
        vec3 ground_normal = vec3_create(0.0f, 1.0f, 0.0f);
        for (int i = 0; i < state->num_bodies; i++) {
            ForgePhysicsRigidBody *rb = &state->bodies[i];

            /* Gravity */
            forge_physics_rigid_body_apply_gravity(rb,
                vec3_create(0.0f, -state->ui_gravity, 0.0f));

            /* Angular drag — slows rotation over time */
            forge_physics_rigid_body_apply_angular_drag(rb,
                state->ui_angular_drag);

            /* Contact friction when on or near the ground */
            float half_h = (state->body_info[i].shape_type == SHAPE_SPHERE)
                ? state->body_info[i].render_scale.x
                : state->body_info[i].render_scale.y;
            float ground_threshold = GROUND_Y + half_h + GROUND_CONTACT_MARGIN;
            if (rb->position.y <= ground_threshold) {
                vec3 contact_pt = rb->position;
                contact_pt.y = GROUND_Y;
                forge_physics_rigid_body_apply_friction(rb, ground_normal,
                    contact_pt, state->ui_friction);
            }
        }
    }

    /* Scene 4: Gyroscopic Stability — gravity torque for precession */
    if (state->scene_index == 3) {
        for (int i = 0; i < state->num_bodies; i++) {
            ForgePhysicsRigidBody *rb = &state->bodies[i];

            /* Apply gravity torque for precession (support force cancels
             * linear gravity — disc is on a pivot at the bottom) */
            vec3 up_body = quat_up(rb->orientation);
            vec3 offset = vec3_scale(up_body, S4_SUPPORT_OFFSET);
            vec3 gravity_force = vec3_create(0.0f,
                -state->ui_gravity * rb->mass, 0.0f);
            vec3 precession_torque = vec3_cross(offset, gravity_force);
            forge_physics_rigid_body_apply_torque(rb, precession_torque);

            /* Light angular drag */
            forge_physics_rigid_body_apply_angular_drag(rb, S4_ANGULAR_DRAG);
        }
    }

    /* Integrate all bodies */
    for (int i = 0; i < state->num_bodies; i++) {
        forge_physics_rigid_body_integrate(&state->bodies[i], PHYSICS_DT);
    }

    /* Ground collision for scenes that need it */
    if (state->scene_index == 1 || state->scene_index == 2) {
        for (int i = 0; i < state->num_bodies; i++) {
            rigid_body_ground_collision(&state->bodies[i],
                                         &state->body_info[i]);
        }
    }
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

/* ── Helper: compute kinetic energies ────────────────────────────── */

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

        /* KE_rot = 0.5 * omega . (I_world * omega) */
        vec3 Iw = mat3_multiply_vec3(rb->inertia_world, rb->angular_velocity);
        rot_ke += 0.5f * vec3_dot(rb->angular_velocity, Iw);
    }

    *out_linear_ke    = lin_ke;
    *out_rotational_ke = rot_ke;
}

/* ── Helper: compute angular momentum magnitude ──────────────────── */

static float compute_angular_momentum_mag(const ForgePhysicsRigidBody *rb)
{
    vec3 L = mat3_multiply_vec3(rb->inertia_world, rb->angular_velocity);
    return vec3_length(L);
}

/* ── Helper: compute tilt angle from vertical ────────────────────── */

static float compute_tilt_from_vertical(const ForgePhysicsRigidBody *rb)
{
    vec3 up_body = quat_up(rb->orientation);
    vec3 world_up = vec3_create(0, 1, 0);
    float dot = vec3_dot(up_body, world_up);
    if (dot > 1.0f) dot = 1.0f;
    if (dot < -1.0f) dot = -1.0f;
    return SDL_acosf(dot);
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
        "Physics Lesson 05 \xe2\x80\x94 Forces and Torques");
    cfg.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";
    cfg.font_size       = 16.0f;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("ERROR: forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* Double-sided pipeline for uncapped cylinder geometry */
    state->double_sided_pipeline = forge_scene_create_pipeline(
        &state->scene, SDL_GPU_CULLMODE_NONE, SDL_GPU_FILLMODE_FILL);
    if (!state->double_sided_pipeline) {
        SDL_Log("ERROR: Failed to create double-sided pipeline: %s",
                SDL_GetError());
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

    /* Default UI parameters */
    state->scene_index       = 0;
    state->accumulator       = 0.0f;
    state->paused            = false;
    state->speed_scale       = NORMAL_SPEED_SCALE;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
    state->ui_gravity         = DEFAULT_GRAVITY;
    state->ui_force_magnitude = S1_FORCE_MAG;
    state->ui_corner_index    = 0;
    state->ui_drag_coeff      = S2_DRAG_MED;
    state->ui_friction        = S3_DEFAULT_FRICTION;
    state->ui_angular_drag    = S3_DEFAULT_ANG_DRAG;
    state->ui_init_velocity   = S3_INIT_VEL;
    state->ui_fast_spin       = S4_FAST_SPIN;
    state->ui_slow_spin       = S4_SLOW_SPIN;
    state->ui_tilt_angle      = S4_TILT_ANGLE;
    state->force_input        = vec3_create(0.0f, 0.0f, 0.0f);

    init_current_scene(state);

    /* No init_fail cleanup block — SDL guarantees SDL_AppQuit runs even when
     * SDL_AppInit returns failure, and *appstate is assigned before init work,
     * so SDL_AppQuit handles all resource release with proper NULL checks. */
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
        case SDL_SCANCODE_4: set_scene(state, 3); break;
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

    /* ── Read arrow keys for Scene 1 force direction ──────────── */

    if (state->scene_index == 0) {
        const bool *keys = SDL_GetKeyboardState(NULL);
        state->force_input = vec3_create(0.0f, 0.0f, 0.0f);
        if (keys[SDL_SCANCODE_UP])    state->force_input.y += 1.0f;
        if (keys[SDL_SCANCODE_DOWN])  state->force_input.y -= 1.0f;
        if (keys[SDL_SCANCODE_LEFT])  state->force_input.x -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) state->force_input.x += 1.0f;
    }

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
        case SHAPE_CYLINDER:
            forge_scene_draw_shadow_mesh(s, state->cylinder_vb,
                                         state->cylinder_ib,
                                         state->cylinder_index_count, model);
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
        case SHAPE_CYLINDER:
            forge_scene_draw_mesh_ex(s, state->double_sided_pipeline,
                                  state->cylinder_vb,
                                  state->cylinder_ib,
                                  state->cylinder_index_count, model,
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
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "Forces & Torques",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* Scene selection label */
                {
                    const char *scene_names[NUM_SCENES] = {
                        "1: Force at Point",
                        "2: Force Generators",
                        "3: Friction",
                        "4: Gyroscopic"
                    };
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Scene: %s",
                                 scene_names[state->scene_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* Scene selection buttons */
                {
                    if (forge_ui_ctx_button_layout(ui, "1: Force at Point",
                                                    BUTTON_HEIGHT))
                        set_scene(state, 0);
                    if (forge_ui_ctx_button_layout(ui, "2: Generators",
                                                    BUTTON_HEIGHT))
                        set_scene(state, 1);
                    if (forge_ui_ctx_button_layout(ui, "3: Friction",
                                                    BUTTON_HEIGHT))
                        set_scene(state, 2);
                    if (forge_ui_ctx_button_layout(ui, "4: Gyroscopic",
                                                    BUTTON_HEIGHT))
                        set_scene(state, 3);
                }

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Gravity slider (all scenes) */
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

                /* Scene-specific controls */
                if (state->scene_index == 0) {
                    /* Force magnitude */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Force: %.0f N",
                                     (double)state->ui_force_magnitude);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##forcemag",
                                               &state->ui_force_magnitude,
                                               FORCE_MAG_MIN, FORCE_MAG_MAX,
                                               SLIDER_HEIGHT);

                    /* Corner selector buttons */
                    forge_ui_ctx_label_layout(ui, "Apply at corner:",
                                              LABEL_HEIGHT);
                    {
                        const char *corner_names[S1_NUM_CORNERS] = {
                            "Top-Left", "Top-Right",
                            "Bot-Left", "Bot-Right"
                        };
                        for (int c = 0; c < S1_NUM_CORNERS; c++) {
                            if (forge_ui_ctx_button_layout(ui,
                                    corner_names[c], BUTTON_HEIGHT)) {
                                state->ui_corner_index = c;
                            }
                        }
                    }

                    forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.25f);
                    forge_ui_ctx_label_layout(ui, "Arrow keys: force dir",
                                              LABEL_HEIGHT);
                }

                if (state->scene_index == 1) {
                    /* Drag coefficient (applied to middle sphere) */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Mid Drag: %.1f",
                                     (double)state->ui_drag_coeff);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##drag",
                                               &state->ui_drag_coeff,
                                               DRAG_MIN, DRAG_MAX,
                                               SLIDER_HEIGHT);

                    /* Display drag values for all 3 spheres */
                    {
                        char buf[80];
                        SDL_snprintf(buf, sizeof(buf),
                            "Drag: %.1f / %.1f / %.1f",
                            (double)S2_DRAG_LOW,
                            (double)state->ui_drag_coeff,
                            (double)S2_DRAG_HIGH);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }

                    /* Terminal velocities */
                    {
                        char buf[80];
                        float g = state->ui_gravity;
                        float vt1 = (S2_DRAG_LOW > FORCE_INPUT_EPSILON)
                            ? S2_MASS * g / S2_DRAG_LOW : TERMINAL_VEL_MAX;
                        float vt2 = (state->ui_drag_coeff > FORCE_INPUT_EPSILON)
                            ? S2_MASS * g / state->ui_drag_coeff : TERMINAL_VEL_MAX;
                        float vt3 = (S2_DRAG_HIGH > FORCE_INPUT_EPSILON)
                            ? S2_MASS * g / S2_DRAG_HIGH : TERMINAL_VEL_MAX;
                        SDL_snprintf(buf, sizeof(buf),
                            "v_term: %.1f / %.1f / %.1f",
                            (double)vt1, (double)vt2, (double)vt3);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                }

                if (state->scene_index == 2) {
                    /* Friction coefficient */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Friction: %.1f",
                                     (double)state->ui_friction);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##friction",
                                               &state->ui_friction,
                                               FRICTION_MIN, FRICTION_MAX,
                                               SLIDER_HEIGHT);

                    /* Angular drag */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Ang. Drag: %.1f",
                                     (double)state->ui_angular_drag);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##angdrag",
                                               &state->ui_angular_drag,
                                               ANG_DRAG_MIN, ANG_DRAG_MAX,
                                               SLIDER_HEIGHT);

                    /* Initial velocity */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Init Vel: %.1f m/s",
                                     (double)state->ui_init_velocity);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##initvel",
                                               &state->ui_init_velocity,
                                               INIT_VEL_MIN, INIT_VEL_MAX,
                                               SLIDER_HEIGHT);
                }

                if (state->scene_index == 3) {
                    /* Fast spin speed */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Fast Spin: %.1f",
                                     (double)state->ui_fast_spin);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##fastspin",
                                               &state->ui_fast_spin,
                                               SPIN_SPEED_MIN, SPIN_SPEED_MAX,
                                               SLIDER_HEIGHT);

                    /* Slow spin speed */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Slow Spin: %.1f",
                                     (double)state->ui_slow_spin);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##slowspin",
                                               &state->ui_slow_spin,
                                               SPIN_SPEED_MIN, SPIN_SPEED_MAX,
                                               SLIDER_HEIGHT);

                    /* Tilt angle */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "Tilt: %.2f rad",
                                     (double)state->ui_tilt_angle);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }
                    forge_ui_ctx_slider_layout(ui, "##tilt",
                                               &state->ui_tilt_angle,
                                               TILT_MIN, TILT_MAX,
                                               SLIDER_HEIGHT);
                }

                /* Separator */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Simulation info */
                {
                    char buf[80];

                    SDL_snprintf(buf, sizeof(buf), "Bodies: %d",
                                 state->num_bodies);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    /* Per-body velocity info */
                    for (int i = 0; i < state->num_bodies && i < 3; i++) {
                        float speed = vec3_length(state->bodies[i].velocity);
                        SDL_snprintf(buf, sizeof(buf),
                                     "Body %d vel: %.1f m/s", i, (double)speed);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }

                    /* Angular velocity for relevant scenes */
                    if (state->scene_index == 0 || state->scene_index == 3) {
                        for (int i = 0; i < state->num_bodies && i < 2; i++) {
                            float omega = vec3_length(
                                state->bodies[i].angular_velocity);
                            SDL_snprintf(buf, sizeof(buf),
                                "Body %d omega: %.1f rad/s", i, (double)omega);
                            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                        }
                    }

                    /* Scene 4: tilt angles */
                    if (state->scene_index == 3) {
                        for (int i = 0; i < state->num_bodies; i++) {
                            float tilt = compute_tilt_from_vertical(
                                &state->bodies[i]);
                            float L_mag = compute_angular_momentum_mag(
                                &state->bodies[i]);
                            SDL_snprintf(buf, sizeof(buf),
                                "Disc %d tilt: %.1f deg |L|: %.1f",
                                i, (double)(tilt * FORGE_RAD2DEG),
                                (double)L_mag);
                            forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                        }
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

    if (forge_scene_device(&state->scene)) {
        if (!SDL_WaitForGPUIdle(forge_scene_device(&state->scene))) {
            SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }
        if (state->double_sided_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(forge_scene_device(&state->scene),
                                           state->double_sided_pipeline);
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
