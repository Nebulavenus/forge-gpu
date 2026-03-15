/*
 * Physics Lesson 04 — Rigid Body State and Orientation
 *
 * Demonstrates: rigid body mass properties, inertia tensors, quaternion
 * orientation, angular velocity, torque, and the full rigid body
 * integration loop.
 *
 * Four selectable scenes:
 *   1. Spinning Cube — pure quaternion integration with adjustable spin
 *   2. Tumbling Shapes — cube, sphere, cylinder with different inertia
 *   3. Torque Demo — apply torques to a non-uniform box
 *   4. Gyroscopic Precession — spinning disc under gravity
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
 *   Arrow keys        — apply torque (Scene 3 only)
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Physics library — particles, rigid bodies */
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

/* Scene 1: Spinning Cube */
#define S1_NUM_BODIES        1
#define S1_MASS              5.0f
#define S1_HALF_EXTENT       0.8f
#define S1_START_Y           2.5f
#define S1_DEFAULT_SPIN      5.0f

/* Scene 2: Tumbling Shapes */
#define S2_NUM_BODIES        3
#define S2_MASS              5.0f
#define S2_LAUNCH_Y          6.0f
#define S2_SPACING           4.0f
#define S2_LAUNCH_VX         1.0f
#define S2_LAUNCH_VY         3.0f
#define S2_INITIAL_OMEGA     4.0f
#define S2_CUBE_HALF         0.6f
#define S2_SPHERE_RADIUS     0.5f
#define S2_CYL_RADIUS        0.4f
#define S2_CYL_HALF_H        0.6f
#define S2_BOUNCE_RESTIT     0.6f
/* Scene 3: Torque Demo */
#define S3_NUM_BODIES        1
#define S3_MASS              5.0f
#define S3_START_Y           4.0f
#define S3_HALF_W            1.0f
#define S3_HALF_H            0.5f
#define S3_HALF_D            0.25f
#define S3_DEFAULT_TORQUE    20.0f
#define S3_INIT_SPIN_Y       3.0f   /* initial angular velocity around Y */
#define S3_INIT_SPIN_Z       1.0f   /* initial angular velocity around Z */

/* Scene 4: Gyroscopic Precession */
#define S4_NUM_BODIES        1
#define S4_MASS              5.0f
#define S4_START_Y           3.0f
#define S4_DISC_RADIUS       1.0f
#define S4_DISC_HALF_H       0.1f
#define S4_SPIN_SPEED        30.0f

/* Common properties */
#define GROUND_Y             0.0f
#define DEFAULT_DAMPING      0.99f
#define DEFAULT_ANG_DAMPING  0.99f
#define DEFAULT_RESTIT       0.5f
#define DEFAULT_GRAVITY      9.81f

/* Ground collision tuning */
#define BOUNCE_KILL_VEL      0.5f   /* vertical speed below which bounce is killed */
#define GROUND_FRICTION      0.95f  /* per-step ground friction multiplier (timestep-dependent) */

/* Scene 2 angular velocity multipliers */
#define S2_OMEGA_RATIO_CUBE  0.5f   /* cube Y-axis omega = initial * this */
#define S2_OMEGA_RATIO_CYL   0.7f   /* cylinder Y-axis omega = initial * this */

/* Scene 4 precession */
#define S4_TILT_ANGLE        0.3f   /* initial disc tilt from vertical (radians) */
#define S4_PRECESSION_OFFSET 0.5f   /* lever arm from support to COM (m) */

/* Torque input */
#define TORQUE_MIN_LENGTH    0.001f /* minimum torque_input magnitude to apply */

/* Speed control */
#define SLOW_MOTION_SCALE    0.25f  /* speed multiplier when slow motion is on */
#define NORMAL_SPEED_SCALE   1.0f   /* speed multiplier at normal speed */
#define SLOW_MOTION_THRESH   0.5f   /* threshold to detect slow motion state */

/* Camera start position */
#define CAM_START_X          0.0f
#define CAM_START_Y          5.0f
#define CAM_START_Z         12.0f
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
#define PANEL_W            260.0f
#define PANEL_H            520.0f
#define LABEL_HEIGHT        24.0f
#define BUTTON_HEIGHT       30.0f
#define SLIDER_HEIGHT       28.0f

/* Slider ranges */
#define ANG_DAMPING_MIN      0.0f
#define ANG_DAMPING_MAX      1.0f
#define GRAVITY_MIN          0.0f
#define GRAVITY_MAX         20.0f
#define SPIN_SPEED_MIN       0.0f
#define SPIN_SPEED_MAX      50.0f
#define TORQUE_MIN           0.0f
#define TORQUE_MAX         100.0f

/* Number of scenes */
#define NUM_SCENES           4

/* Body shape type for rendering */
#define SHAPE_CUBE           0
#define SHAPE_SPHERE         1
#define SHAPE_CYLINDER       2

/* Colors (RGBA) */
#define COLOR_CUBE_R        0.2f
#define COLOR_CUBE_G        0.6f
#define COLOR_CUBE_B        1.0f
#define COLOR_CUBE_A        1.0f

#define COLOR_SPHERE_R      1.0f
#define COLOR_SPHERE_G      0.4f
#define COLOR_SPHERE_B      0.2f
#define COLOR_SPHERE_A      1.0f

#define COLOR_CYLINDER_R    0.3f
#define COLOR_CYLINDER_G    0.9f
#define COLOR_CYLINDER_B    0.3f
#define COLOR_CYLINDER_A    1.0f

#define COLOR_TORQUE_R      0.9f
#define COLOR_TORQUE_G      0.3f
#define COLOR_TORQUE_B      0.7f
#define COLOR_TORQUE_A      1.0f

/* ── Types ────────────────────────────────────────────────────────── */

/* Per-body rendering metadata */
typedef struct BodyRenderInfo {
    int   shape_type;     /* SHAPE_CUBE, SHAPE_SPHERE, or SHAPE_CYLINDER */
    vec3  render_scale;   /* scale factors for the unit mesh */
    float color[4];       /* RGBA color for this body */
} BodyRenderInfo;

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* GPU geometry — vertex/index buffers for each shape type */
    SDL_GPUBuffer *cube_vb;       /* cube vertex buffer (unit cube) */
    SDL_GPUBuffer *cube_ib;       /* cube index buffer */
    Uint32         cube_index_count; /* number of indices in cube IB */

    SDL_GPUBuffer *sphere_vb;     /* sphere vertex buffer (unit sphere) */
    SDL_GPUBuffer *sphere_ib;     /* sphere index buffer */
    Uint32         sphere_index_count; /* number of indices in sphere IB */

    SDL_GPUBuffer *cylinder_vb;   /* cylinder vertex buffer (unit cylinder) */
    SDL_GPUBuffer *cylinder_ib;   /* cylinder index buffer */
    Uint32         cylinder_index_count; /* number of indices in cylinder IB */

    /* Physics state */
    ForgePhysicsRigidBody bodies[MAX_BODIES];         /* active rigid bodies */
    ForgePhysicsRigidBody initial_bodies[MAX_BODIES];  /* snapshot for reset */
    BodyRenderInfo        body_info[MAX_BODIES];       /* per-body render metadata */
    int                   num_bodies;                  /* active body count */

    /* Simulation control */
    int   scene_index;   /* current scene (0-3) */
    float accumulator;   /* fixed-timestep accumulator (seconds) */
    bool  paused;        /* true = physics frozen, camera still works */
    float speed_scale;   /* time multiplier (1.0 = normal, 0.25 = slow) */

    /* UI-adjustable parameters */
    float ui_angular_damping;  /* angular damping slider value [0..1] */
    float ui_gravity;          /* gravity magnitude slider (m/s^2) */
    float ui_spin_speed;       /* Scene 1 spin speed (rad/s) */
    float ui_torque_strength;  /* Scene 3 torque magnitude (N*m) */

    /* Scene 3 torque direction (accumulated from arrow keys each frame) */
    vec3  torque_input;   /* unit direction of user-applied torque */

    /* UI window state */
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

/* ── Scene initialization ────────────────────────────────────────── */

/* Scene 1: Single spinning cube — demonstrates pure quaternion integration */
static void init_scene_1(app_state *state)
{
    state->num_bodies = S1_NUM_BODIES;

    state->bodies[0] = forge_physics_rigid_body_create(
        vec3_create(0.0f, S1_START_Y, 0.0f),
        S1_MASS, DEFAULT_DAMPING, state->ui_angular_damping, DEFAULT_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&state->bodies[0],
        vec3_create(S1_HALF_EXTENT, S1_HALF_EXTENT, S1_HALF_EXTENT));

    /* Initial angular velocity around Y (adjustable via UI) */
    state->bodies[0].angular_velocity =
        vec3_create(0.0f, state->ui_spin_speed, 0.0f);

    /* Render info */
    state->body_info[0].shape_type = SHAPE_CUBE;
    state->body_info[0].render_scale =
        vec3_create(S1_HALF_EXTENT, S1_HALF_EXTENT, S1_HALF_EXTENT);
    state->body_info[0].color[0] = COLOR_CUBE_R;
    state->body_info[0].color[1] = COLOR_CUBE_G;
    state->body_info[0].color[2] = COLOR_CUBE_B;
    state->body_info[0].color[3] = COLOR_CUBE_A;
}

/* Scene 2: Tumbling shapes — cube, sphere, cylinder with different inertia */
static void init_scene_2(app_state *state)
{
    state->num_bodies = S2_NUM_BODIES;

    /* Cube */
    state->bodies[0] = forge_physics_rigid_body_create(
        vec3_create(-S2_SPACING, S2_LAUNCH_Y, 0.0f),
        S2_MASS, DEFAULT_DAMPING, state->ui_angular_damping, S2_BOUNCE_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&state->bodies[0],
        vec3_create(S2_CUBE_HALF, S2_CUBE_HALF, S2_CUBE_HALF));
    state->bodies[0].velocity = vec3_create(S2_LAUNCH_VX, S2_LAUNCH_VY, 0.0f);
    state->bodies[0].angular_velocity =
        vec3_create(S2_INITIAL_OMEGA, S2_INITIAL_OMEGA * S2_OMEGA_RATIO_CUBE, 0.0f);
    state->body_info[0].shape_type = SHAPE_CUBE;
    state->body_info[0].render_scale =
        vec3_create(S2_CUBE_HALF, S2_CUBE_HALF, S2_CUBE_HALF);
    state->body_info[0].color[0] = COLOR_CUBE_R;
    state->body_info[0].color[1] = COLOR_CUBE_G;
    state->body_info[0].color[2] = COLOR_CUBE_B;
    state->body_info[0].color[3] = COLOR_CUBE_A;

    /* Sphere */
    state->bodies[1] = forge_physics_rigid_body_create(
        vec3_create(0.0f, S2_LAUNCH_Y, 0.0f),
        S2_MASS, DEFAULT_DAMPING, state->ui_angular_damping, S2_BOUNCE_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&state->bodies[1],
        S2_SPHERE_RADIUS);
    state->bodies[1].velocity = vec3_create(0.0f, S2_LAUNCH_VY, 0.0f);
    state->bodies[1].angular_velocity =
        vec3_create(S2_INITIAL_OMEGA, 0.0f, S2_INITIAL_OMEGA);
    state->body_info[1].shape_type = SHAPE_SPHERE;
    state->body_info[1].render_scale =
        vec3_create(S2_SPHERE_RADIUS, S2_SPHERE_RADIUS, S2_SPHERE_RADIUS);
    state->body_info[1].color[0] = COLOR_SPHERE_R;
    state->body_info[1].color[1] = COLOR_SPHERE_G;
    state->body_info[1].color[2] = COLOR_SPHERE_B;
    state->body_info[1].color[3] = COLOR_SPHERE_A;

    /* Cylinder */
    state->bodies[2] = forge_physics_rigid_body_create(
        vec3_create(S2_SPACING, S2_LAUNCH_Y, 0.0f),
        S2_MASS, DEFAULT_DAMPING, state->ui_angular_damping, S2_BOUNCE_RESTIT);
    forge_physics_rigid_body_set_inertia_cylinder(&state->bodies[2],
        S2_CYL_RADIUS, S2_CYL_HALF_H);
    state->bodies[2].velocity =
        vec3_create(-S2_LAUNCH_VX, S2_LAUNCH_VY, 0.0f);
    state->bodies[2].angular_velocity =
        vec3_create(0.0f, S2_INITIAL_OMEGA, S2_INITIAL_OMEGA * S2_OMEGA_RATIO_CYL);
    state->body_info[2].shape_type = SHAPE_CYLINDER;
    state->body_info[2].render_scale =
        vec3_create(S2_CYL_RADIUS, S2_CYL_HALF_H, S2_CYL_RADIUS);
    state->body_info[2].color[0] = COLOR_CYLINDER_R;
    state->body_info[2].color[1] = COLOR_CYLINDER_G;
    state->body_info[2].color[2] = COLOR_CYLINDER_B;
    state->body_info[2].color[3] = COLOR_CYLINDER_A;
}

/* Scene 3: Torque demo — non-uniform box, arrow keys apply torques */
static void init_scene_3(app_state *state)
{
    state->num_bodies = S3_NUM_BODIES;

    state->bodies[0] = forge_physics_rigid_body_create(
        vec3_create(0.0f, S3_START_Y, 0.0f),
        S3_MASS, DEFAULT_DAMPING, state->ui_angular_damping, DEFAULT_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&state->bodies[0],
        vec3_create(S3_HALF_W, S3_HALF_H, S3_HALF_D));

    /* Start with a gentle spin so the demo shows rotation immediately */
    state->bodies[0].angular_velocity = vec3_create(0.0f,
        S3_INIT_SPIN_Y, S3_INIT_SPIN_Z);

    state->body_info[0].shape_type = SHAPE_CUBE;
    state->body_info[0].render_scale =
        vec3_create(S3_HALF_W, S3_HALF_H, S3_HALF_D);
    state->body_info[0].color[0] = COLOR_TORQUE_R;
    state->body_info[0].color[1] = COLOR_TORQUE_G;
    state->body_info[0].color[2] = COLOR_TORQUE_B;
    state->body_info[0].color[3] = COLOR_TORQUE_A;
}

/* Scene 4: Gyroscopic precession — spinning disc under gravity */
static void init_scene_4(app_state *state)
{
    state->num_bodies = S4_NUM_BODIES;

    state->bodies[0] = forge_physics_rigid_body_create(
        vec3_create(0.0f, S4_START_Y, 0.0f),
        S4_MASS, DEFAULT_DAMPING, state->ui_angular_damping, DEFAULT_RESTIT);
    forge_physics_rigid_body_set_inertia_cylinder(&state->bodies[0],
        S4_DISC_RADIUS, S4_DISC_HALF_H);

    /* Tilt the disc slightly so gravity creates a torque */
    state->bodies[0].orientation = quat_from_axis_angle(
        vec3_create(0.0f, 0.0f, 1.0f), S4_TILT_ANGLE);

    /* Spin around the disc's local symmetry axis (body Y),
     * transformed to world space via the tilted orientation */
    vec3 local_spin = vec3_create(0.0f, S4_SPIN_SPEED, 0.0f);
    state->bodies[0].angular_velocity =
        quat_rotate_vec3(state->bodies[0].orientation, local_spin);
    forge_physics_rigid_body_update_derived(&state->bodies[0]);

    state->body_info[0].shape_type = SHAPE_CYLINDER;
    state->body_info[0].render_scale =
        vec3_create(S4_DISC_RADIUS, S4_DISC_HALF_H, S4_DISC_RADIUS);
    state->body_info[0].color[0] = COLOR_CYLINDER_R;
    state->body_info[0].color[1] = COLOR_CYLINDER_G;
    state->body_info[0].color[2] = COLOR_CYLINDER_B;
    state->body_info[0].color[3] = COLOR_CYLINDER_A;
}

/* Initialize current scene and save initial state for reset */
static void init_current_scene(app_state *state)
{
    switch (state->scene_index) {
    case 0: init_scene_1(state); break;
    case 1: init_scene_2(state); break;
    case 2: init_scene_3(state); break;
    case 3: init_scene_4(state); break;
    default: init_scene_1(state); break;
    }

    /* Save initial state for reset */
    for (int i = 0; i < state->num_bodies; i++) {
        state->initial_bodies[i] = state->bodies[i];
    }

    state->accumulator = 0.0f;

    state->torque_input = vec3_create(0.0f, 0.0f, 0.0f);
}

/* Reset simulation to initial state */
static void reset_simulation(app_state *state)
{
    for (int i = 0; i < state->num_bodies; i++) {
        state->bodies[i] = state->initial_bodies[i];
    }
    state->accumulator = 0.0f;

    state->torque_input = vec3_create(0.0f, 0.0f, 0.0f);
}

/* ── Simplified ground collision for rigid bodies ────────────────── */

static void rigid_body_ground_collision(ForgePhysicsRigidBody *rb,
                                         const BodyRenderInfo *info)
{
    if (rb->inv_mass == 0.0f) return;

    /* Approximate half-height for ground collision based on shape */
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

        /* Reflect vertical velocity with restitution */
        if (rb->velocity.y < 0.0f) {
            rb->velocity.y = -rb->velocity.y * rb->restitution;

            /* Kill tiny bounces */
            if (fabsf(rb->velocity.y) < BOUNCE_KILL_VEL) {
                rb->velocity.y = 0.0f;
            }
        }

        /* Friction: reduce horizontal velocity on ground contact */
        rb->velocity.x *= GROUND_FRICTION;
        rb->velocity.z *= GROUND_FRICTION;
    }
}

/* ── Physics step ────────────────────────────────────────────────── */

static void physics_step(app_state *state)
{
    /* Apply gravity to all dynamic bodies */
    for (int i = 0; i < state->num_bodies; i++) {
        if (state->bodies[i].inv_mass == 0.0f) continue;

        /* Scenes 1, 3, 4: no linear gravity
         * Scene 1: suspended cube, floating in place
         * Scene 3: torque demo, no gravity
         * Scene 4: precession — gravity torque applied separately below,
         *          support force cancels linear gravity at the pivot */
        if (state->scene_index == 1) {
            vec3 gravity = vec3_create(0.0f,
                -state->ui_gravity * state->bodies[i].mass, 0.0f);
            forge_physics_rigid_body_apply_force(&state->bodies[i], gravity);
        }
    }

    /* Scene 3: apply user-controlled torque */
    if (state->scene_index == 2 && state->num_bodies > 0) {
        float len = vec3_length(state->torque_input);
        if (len > TORQUE_MIN_LENGTH) {
            /* Normalize direction so diagonal input isn't stronger */
            vec3 torque = vec3_scale(vec3_normalize(state->torque_input),
                                      state->ui_torque_strength);
            forge_physics_rigid_body_apply_torque(&state->bodies[0], torque);
        }
    }

    /* Scene 4: apply gravity torque for precession
     * Gravity acts at the center of mass, but when the disc is tilted,
     * the gravitational force creates a torque around the support point.
     * We model this as torque = r × F where r is the tilt offset. */
    if (state->scene_index == 3 && state->num_bodies > 0) {
        ForgePhysicsRigidBody *rb = &state->bodies[0];
        vec3 up_body = quat_up(rb->orientation);
        /* Offset from support to COM along the tilted up direction */
        vec3 offset = vec3_scale(up_body, S4_PRECESSION_OFFSET);
        vec3 gravity_force = vec3_create(0.0f,
            -state->ui_gravity * rb->mass, 0.0f);
        vec3 precession_torque = vec3_cross(offset, gravity_force);
        forge_physics_rigid_body_apply_torque(rb, precession_torque);
    }

    /* Integrate all bodies */
    for (int i = 0; i < state->num_bodies; i++) {
        /* Update angular damping from UI */
        state->bodies[i].angular_damping = state->ui_angular_damping;

        forge_physics_rigid_body_integrate(&state->bodies[i], PHYSICS_DT);
    }

    /* Ground collision (Scene 2 only — tumbling shapes bounce) */
    if (state->scene_index == 1) {
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
    /* Interpolate position */
    vec3 pos = vec3_lerp(rb->prev_position, rb->position, alpha);

    /* Interpolate orientation */
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

        /* Linear KE = 0.5 * m * |v|² */
        float speed_sq = vec3_length_squared(rb->velocity);
        lin_ke += 0.5f * rb->mass * speed_sq;

        /* Rotational KE = 0.5 * ω^T * I * ω
         * Angular velocity is in world space but I_local is diagonal in body
         * space.  Transform ω to body space using R^T (= inverse rotation),
         * then compute KE_rot = 0.5 * Σ(Ii * ωi²) with body-space values. */
        mat3 R = quat_to_mat3(rb->orientation);
        /* R^T * ω  (transpose multiply — dot each column of R with ω) */
        vec3 w = rb->angular_velocity;
        float wb_x = R.m[0] * w.x + R.m[1] * w.y + R.m[2] * w.z;
        float wb_y = R.m[3] * w.x + R.m[4] * w.y + R.m[5] * w.z;
        float wb_z = R.m[6] * w.x + R.m[7] * w.y + R.m[8] * w.z;

        /* Use cached inertia_local directly (diagonal) */
        rot_ke += 0.5f * rb->inertia_local.m[0] * wb_x * wb_x;
        rot_ke += 0.5f * rb->inertia_local.m[4] * wb_y * wb_y;
        rot_ke += 0.5f * rb->inertia_local.m[8] * wb_z * wb_z;
    }

    *out_linear_ke    = lin_ke;
    *out_rotational_ke = rot_ke;
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
        "Physics Lesson 04 \xe2\x80\x94 Rigid Body State");
    cfg.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";
    cfg.font_size       = 16.0f;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("ERROR: forge_scene_init failed");
        goto init_fail;
    }

    /* Generate and upload cube geometry (with flat normals) */
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

    /* Default UI parameters */
    state->scene_index       = 0;
    state->accumulator       = 0.0f;
    state->paused            = false;
    state->speed_scale       = NORMAL_SPEED_SCALE;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
    state->ui_angular_damping = DEFAULT_ANG_DAMPING;
    state->ui_gravity         = DEFAULT_GRAVITY;
    state->ui_spin_speed      = S1_DEFAULT_SPIN;
    state->ui_torque_strength = S3_DEFAULT_TORQUE;
    state->torque_input       = vec3_create(0.0f, 0.0f, 0.0f);

    /* Initialize first scene */
    init_current_scene(state);

    return SDL_APP_CONTINUE;

init_fail:
    if (forge_scene_device(&state->scene)) {
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
                (state->speed_scale < SLOW_MOTION_THRESH) ? NORMAL_SPEED_SCALE : SLOW_MOTION_SCALE;
            SDL_Log("Speed: %.2fx", (double)state->speed_scale);
            break;
        case SDL_SCANCODE_1:
            state->scene_index = 0;
            init_current_scene(state);
            SDL_Log("Scene 1: Spinning Cube");
            break;
        case SDL_SCANCODE_2:
            state->scene_index = 1;
            init_current_scene(state);
            SDL_Log("Scene 2: Tumbling Shapes");
            break;
        case SDL_SCANCODE_3:
            state->scene_index = 2;
            init_current_scene(state);
            SDL_Log("Scene 3: Torque Demo");
            break;
        case SDL_SCANCODE_4:
            state->scene_index = 3;
            init_current_scene(state);
            SDL_Log("Scene 4: Gyroscopic Precession");
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

    /* ── Read arrow keys for Scene 3 torque input ──────────────── */

    if (state->scene_index == 2) {
        const bool *keys = SDL_GetKeyboardState(NULL);
        state->torque_input = vec3_create(0.0f, 0.0f, 0.0f);
        if (keys[SDL_SCANCODE_UP])    state->torque_input.x += 1.0f;
        if (keys[SDL_SCANCODE_DOWN])  state->torque_input.x -= 1.0f;
        if (keys[SDL_SCANCODE_LEFT])  state->torque_input.y += 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) state->torque_input.y -= 1.0f;
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
            forge_scene_draw_mesh_double_sided(s, state->cylinder_vb,
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
            if (forge_ui_wctx_window_begin(wctx, "Rigid Body State",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* Scene selection label */
                {
                    const char *scene_names[NUM_SCENES] = {
                        "1: Spinning Cube",
                        "2: Tumbling Shapes",
                        "3: Torque Demo",
                        "4: Precession"
                    };
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Scene: %s",
                                 scene_names[state->scene_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* Scene selection buttons */
                {
                    if (forge_ui_ctx_button_layout(ui, "1: Spinning Cube",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 0;
                        init_current_scene(state);
                    }
                    if (forge_ui_ctx_button_layout(ui, "2: Tumbling",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 1;
                        init_current_scene(state);
                    }
                    if (forge_ui_ctx_button_layout(ui, "3: Torque Demo",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 2;
                        init_current_scene(state);
                    }
                    if (forge_ui_ctx_button_layout(ui, "4: Precession",
                                                    BUTTON_HEIGHT)) {
                        state->scene_index = 3;
                        init_current_scene(state);
                    }
                }

                /* Separator */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Angular damping slider */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Ang. Damping: %.2f",
                                 (double)state->ui_angular_damping);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##angdamp",
                                           &state->ui_angular_damping,
                                           ANG_DAMPING_MIN, ANG_DAMPING_MAX,
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

                /* Scene-specific slider */
                if (state->scene_index == 0) {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Spin Speed: %.1f",
                                 (double)state->ui_spin_speed);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    float prev_spin = state->ui_spin_speed;
                    forge_ui_ctx_slider_layout(ui, "##spin",
                                               &state->ui_spin_speed,
                                               SPIN_SPEED_MIN, SPIN_SPEED_MAX,
                                               SLIDER_HEIGHT);
                    /* Apply only when slider changes — preserve current axis */
                    if (state->num_bodies > 0 &&
                        fabsf(state->ui_spin_speed - prev_spin) >
                            FORGE_PHYSICS_EPSILON) {
                        vec3 av = state->bodies[0].angular_velocity;
                        float len = vec3_length(av);
                        vec3 axis = (len > FORGE_PHYSICS_EPSILON)
                            ? vec3_scale(av, 1.0f / len)
                            : vec3_create(0.0f, 1.0f, 0.0f);
                        state->bodies[0].angular_velocity =
                            vec3_scale(axis, state->ui_spin_speed);
                    }
                }

                if (state->scene_index == 2) {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Torque: %.1f",
                                 (double)state->ui_torque_strength);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    forge_ui_ctx_slider_layout(ui, "##torque",
                                               &state->ui_torque_strength,
                                               TORQUE_MIN, TORQUE_MAX,
                                               SLIDER_HEIGHT);

                    /* Arrow key control hints */
                    forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.25f);
                    forge_ui_ctx_label_layout(ui, "Arrow keys: torque",
                                              LABEL_HEIGHT);
                    forge_ui_ctx_label_layout(ui,
                        "Left/Right: yaw  Up/Down: pitch",
                        LABEL_HEIGHT);
                }

                /* Separator */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Simulation info */
                {
                    char buf[64];

                    SDL_snprintf(buf, sizeof(buf), "Bodies: %d",
                                 state->num_bodies);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    /* Angular velocity magnitude of first body */
                    if (state->num_bodies > 0) {
                        float omega_mag = vec3_length(
                            state->bodies[0].angular_velocity);
                        SDL_snprintf(buf, sizeof(buf),
                                     "Ang. Vel: %.2f rad/s",
                                     (double)omega_mag);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
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

                /* Separator */
                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Pause checkbox */
                forge_ui_ctx_checkbox_layout(ui, "Paused",
                                             &state->paused, LABEL_HEIGHT);

                /* Slow motion checkbox */
                {
                    bool slow = (state->speed_scale < SLOW_MOTION_THRESH);
                    if (forge_ui_ctx_checkbox_layout(ui, "Slow Motion",
                                                      &slow, LABEL_HEIGHT)) {
                        state->speed_scale = slow ? SLOW_MOTION_SCALE : NORMAL_SPEED_SCALE;
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
