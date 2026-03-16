/*
 * Physics Lesson 07 — Collision Shapes and Support Functions
 *
 * Demonstrates: ForgePhysicsCollisionShape tagged union, support functions
 * (geometric core of GJK), AABB computation, and AABB overlap testing.
 * Replaces the ad-hoc BodyRenderInfo shape metadata from L06 with a proper
 * collision shape abstraction.
 *
 * Three selectable scenes:
 *   1. Shape Gallery — sphere, box, capsule drop onto ground with AABB wireframes
 *   2. Support Point Sweep — visualize the support function for each shape
 *   3. Mixed Collisions — multiple shapes using ForgePhysicsCollisionShape dispatch
 *
 * Controls:
 *   WASD              — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   1-3               — select scene
 *   4/5/6             — Scene 2: select sphere/box/capsule
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

/* Physics library — rigid bodies, contacts, collision shapes */
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
#define MAX_BODIES           16

/* Scene 1: Shape Gallery — one of each shape */
#define S1_NUM_BODIES        3
#define S1_MASS              5.0f
#define S1_SPHERE_RADIUS     0.5f
#define S1_BOX_HALF          0.4f
#define S1_CAPSULE_RADIUS    0.3f
#define S1_CAPSULE_HALF_H    0.4f
#define S1_DROP_HEIGHT       3.0f
#define S1_SPACING           3.0f

/* Scene 2: Support Point Sweep — single shape with sweeping direction */
#define S2_SHAPE_Y           2.0f
#define S2_SWEEP_SPEED       1.5f
#define S2_ARROW_LENGTH      2.5f
#define S2_SPHERE_RADIUS     0.7f
#define S2_BOX_HALF          0.6f
#define S2_CAPSULE_RADIUS    0.35f
#define S2_CAPSULE_HALF_H    0.5f
#define S2_MARKER_RADIUS     0.06f
#define S2_ARROW_THICKNESS   0.02f

/* Scene 1: per-shape drop height offsets (stagger for visual clarity) */
#define S1_BOX_DROP_OFFSET     1.0f
#define S1_CAPSULE_DROP_OFFSET 0.5f

/* Scene 3: Mixed Collisions — several shapes dropping */
#define S3_NUM_BODIES        9
#define S3_MASS              5.0f
#define S3_SPHERE_RADIUS     0.4f
#define S3_BOX_HALF          0.35f
#define S3_CAPSULE_RADIUS    0.25f
#define S3_CAPSULE_HALF_H    0.35f
#define S3_DROP_BASE         3.0f
#define S3_DROP_STEP         1.5f
#define S3_SPACING_X         2.5f
#define S3_SPACING_Z         2.5f
#define S3_BOX_DROP_OFFSET   0.5f
#define S3_CAPSULE_DROP_OFFSET 1.0f

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
#define CAM_START_Y          5.0f
#define CAM_START_Z         14.0f
#define CAM_START_PITCH     -0.15f

/* Mesh resolution */
#define SPHERE_SLICES       16
#define SPHERE_STACKS        8
#define CUBE_SLICES          1
#define CUBE_STACKS          1
#define CAPSULE_SLICES       12
#define CAPSULE_STACKS        4
#define CAPSULE_CAP_STACKS    4
#define MARKER_SLICES         8
#define MARKER_STACKS         4

/* UI panel layout */
#define PANEL_X             10.0f
#define PANEL_Y             10.0f
#define PANEL_W            280.0f
#define PANEL_H            580.0f
#define LABEL_HEIGHT        24.0f
#define BUTTON_HEIGHT       30.0f

/* Number of scenes */
#define NUM_SCENES           3

/* AABB wireframe color (semi-transparent green) */
#define AABB_COLOR_R         0.2f
#define AABB_COLOR_G         0.9f
#define AABB_COLOR_B         0.2f
#define AABB_COLOR_A         0.25f

/* Support point marker color (bright yellow) */
#define SUPPORT_COLOR_R      1.0f
#define SUPPORT_COLOR_G      1.0f
#define SUPPORT_COLOR_B      0.0f
#define SUPPORT_COLOR_A      1.0f

/* Direction arrow color (cyan) */
#define ARROW_COLOR_R        0.0f
#define ARROW_COLOR_G        1.0f
#define ARROW_COLOR_B        1.0f
#define ARROW_COLOR_A        1.0f

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

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Custom pipelines for non-default rasterizer state */
    SDL_GPUGraphicsPipeline *wireframe_pipeline;  /* AABB wireframe overlay */

    /* GPU geometry — vertex/index buffers for each shape type */
    SDL_GPUBuffer *cube_vb;
    SDL_GPUBuffer *cube_ib;
    Uint32         cube_index_count;

    SDL_GPUBuffer *sphere_vb;
    SDL_GPUBuffer *sphere_ib;
    Uint32         sphere_index_count;

    SDL_GPUBuffer *capsule_vb;
    SDL_GPUBuffer *capsule_ib;
    Uint32         capsule_index_count;
    float          capsule_mesh_half_h;  /* half-height used for the mesh */

    /* Small sphere for support point marker */
    SDL_GPUBuffer *marker_vb;
    SDL_GPUBuffer *marker_ib;
    Uint32         marker_index_count;

    /* Physics state */
    ForgePhysicsRigidBody      bodies[MAX_BODIES];
    ForgePhysicsCollisionShape shapes[MAX_BODIES];
    float                      body_colors[MAX_BODIES][4];
    int                        num_bodies;

    /* Cached AABBs (updated each physics step) */
    ForgePhysicsAABB           cached_aabbs[MAX_BODIES];

    /* Contact state (for UI readout) */
    int last_contact_count;

    /* Simulation control */
    int   scene_index;
    float accumulator;
    bool  paused;
    float speed_scale;

    /* AABB visualization toggle */
    bool show_aabbs;

    /* Scene 2: support sweep state */
    float sweep_angle;
    int   s2_shape_type;  /* 0=sphere, 1=box, 2=capsule */

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
        /* The capsule mesh was generated with unit radius and a specific
         * half-height. Scale radius uniformly on X/Z, scale Y to match
         * the desired half_height + radius proportions. */
        float r = shape->data.capsule.radius;
        float h = shape->data.capsule.half_height;
        float mesh_total_half = capsule_mesh_half_h + 1.0f;  /* unit radius */
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

/* ── Scene initialization ────────────────────────────────────────── */

/* Scene 1: Shape Gallery — one sphere, one box, one capsule */
static void init_scene_1(app_state *state)
{
    state->num_bodies = S1_NUM_BODIES;

    /* Sphere (center) */
    init_body(state, 0,
        vec3_create(0.0f, S1_DROP_HEIGHT, 0.0f),
        forge_physics_shape_sphere(S1_SPHERE_RADIUS),
        S1_MASS, DEFAULT_RESTIT,
        COLOR_RED_R, COLOR_RED_G, COLOR_RED_B);

    /* Box (left) */
    init_body(state, 1,
        vec3_create(-S1_SPACING, S1_DROP_HEIGHT + S1_BOX_DROP_OFFSET, 0.0f),
        forge_physics_shape_box(vec3_create(S1_BOX_HALF, S1_BOX_HALF, S1_BOX_HALF)),
        S1_MASS, DEFAULT_RESTIT,
        COLOR_BLUE_R, COLOR_BLUE_G, COLOR_BLUE_B);

    /* Capsule (right) */
    init_body(state, 2,
        vec3_create(S1_SPACING, S1_DROP_HEIGHT + S1_CAPSULE_DROP_OFFSET, 0.0f),
        forge_physics_shape_capsule(S1_CAPSULE_RADIUS, S1_CAPSULE_HALF_H),
        S1_MASS, DEFAULT_RESTIT,
        COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B);
}

/* Scene 2: Support Point Sweep — single shape, direction sweeps */
static void init_scene_2(app_state *state)
{
    state->num_bodies = 1;
    state->sweep_angle = 0.0f;

    ForgePhysicsCollisionShape shape;
    float cr, cg, cb;

    switch (state->s2_shape_type) {
    case 1:
        shape = forge_physics_shape_box(
            vec3_create(S2_BOX_HALF, S2_BOX_HALF, S2_BOX_HALF));
        cr = COLOR_BLUE_R; cg = COLOR_BLUE_G; cb = COLOR_BLUE_B;
        break;
    case 2:
        shape = forge_physics_shape_capsule(S2_CAPSULE_RADIUS, S2_CAPSULE_HALF_H);
        cr = COLOR_GREEN_R; cg = COLOR_GREEN_G; cb = COLOR_GREEN_B;
        break;
    default:
        shape = forge_physics_shape_sphere(S2_SPHERE_RADIUS);
        cr = COLOR_RED_R; cg = COLOR_RED_G; cb = COLOR_RED_B;
        break;
    }

    init_body(state, 0,
        vec3_create(0.0f, S2_SHAPE_Y, 0.0f),
        shape, 0.0f, DEFAULT_RESTIT,  /* static body for visualization */
        cr, cg, cb);
}

/* Scene 3: Mixed Collisions — spheres, boxes, capsules dropping */
static void init_scene_3(app_state *state)
{
    state->num_bodies = S3_NUM_BODIES;

    /* Row 1: 3 spheres */
    for (int i = 0; i < 3; i++) {
        float x = ((float)i - 1.0f) * S3_SPACING_X;
        float y = S3_DROP_BASE + (float)i * S3_DROP_STEP;
        float colors[3][3] = {
            {COLOR_RED_R, COLOR_RED_G, COLOR_RED_B},
            {COLOR_ORANGE_R, COLOR_ORANGE_G, COLOR_ORANGE_B},
            {COLOR_PINK_R, COLOR_PINK_G, COLOR_PINK_B}
        };
        init_body(state, i,
            vec3_create(x, y, -S3_SPACING_Z * 0.5f),
            forge_physics_shape_sphere(S3_SPHERE_RADIUS),
            S3_MASS, DEFAULT_RESTIT,
            colors[i][0], colors[i][1], colors[i][2]);
    }

    /* Row 2: 3 boxes */
    for (int i = 0; i < 3; i++) {
        float x = ((float)i - 1.0f) * S3_SPACING_X;
        float y = S3_DROP_BASE + S3_BOX_DROP_OFFSET + (float)i * S3_DROP_STEP;
        float colors[3][3] = {
            {COLOR_BLUE_R, COLOR_BLUE_G, COLOR_BLUE_B},
            {COLOR_PURPLE_R, COLOR_PURPLE_G, COLOR_PURPLE_B},
            {COLOR_GOLD_R, COLOR_GOLD_G, COLOR_GOLD_B}
        };
        vec3 he = vec3_create(S3_BOX_HALF, S3_BOX_HALF, S3_BOX_HALF);
        init_body(state, 3 + i,
            vec3_create(x, y, S3_SPACING_Z * 0.5f),
            forge_physics_shape_box(he),
            S3_MASS, DEFAULT_RESTIT,
            colors[i][0], colors[i][1], colors[i][2]);
    }

    /* Row 3: 3 capsules */
    for (int i = 0; i < 3; i++) {
        float x = ((float)i - 1.0f) * S3_SPACING_X;
        float y = S3_DROP_BASE + S3_CAPSULE_DROP_OFFSET + (float)i * S3_DROP_STEP;
        float colors[3][3] = {
            {COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B},
            {COLOR_TEAL_R, COLOR_TEAL_G, COLOR_TEAL_B},
            {COLOR_LIME_R, COLOR_LIME_G, COLOR_LIME_B}
        };
        init_body(state, 6 + i,
            vec3_create(x, y, 0.0f),
            forge_physics_shape_capsule(S3_CAPSULE_RADIUS, S3_CAPSULE_HALF_H),
            S3_MASS, DEFAULT_RESTIT,
            colors[i][0], colors[i][1], colors[i][2]);
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

    state->accumulator       = 0.0f;
    state->last_contact_count = 0;

    /* Compute initial AABBs */
    for (int i = 0; i < state->num_bodies; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i], state->bodies[i].position,
            state->bodies[i].orientation);
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

    /* Detect contacts with ground plane.
     * Dispatch based on ForgePhysicsCollisionShape.type instead of ad-hoc
     * BodyRenderInfo fields — the core improvement of this lesson. */
    ForgePhysicsRBContact contacts[FORGE_PHYSICS_MAX_RB_CONTACTS];
    int num_contacts = 0;

    for (int i = 0; i < state->num_bodies; i++) {
        const ForgePhysicsCollisionShape *shape = &state->shapes[i];

        switch (shape->type) {
        case FORGE_PHYSICS_SHAPE_SPHERE: {
            ForgePhysicsRBContact c;
            if (forge_physics_rb_collide_sphere_plane(
                    &state->bodies[i], i, shape->data.sphere.radius,
                    plane_pt, plane_n, DEFAULT_MU_S, DEFAULT_MU_D, &c)) {
                if (num_contacts < FORGE_PHYSICS_MAX_RB_CONTACTS) {
                    contacts[num_contacts++] = c;
                }
            }
            break;
        }

        case FORGE_PHYSICS_SHAPE_BOX: {
            int n = forge_physics_rb_collide_box_plane(
                &state->bodies[i], i, shape->data.box.half_extents,
                plane_pt, plane_n, DEFAULT_MU_S, DEFAULT_MU_D,
                &contacts[num_contacts],
                FORGE_PHYSICS_MAX_RB_CONTACTS - num_contacts);
            num_contacts += n;
            break;
        }

        case FORGE_PHYSICS_SHAPE_CAPSULE: {
            /* Temporary two-sphere approximation for capsule-plane contact.
             * This treats the capsule as two spheres at the hemisphere cap
             * centers. GJK-based capsule-plane will replace this in Lesson 09.
             *
             * The approximation is exact for upright capsules and reasonable
             * for moderate tilts, but underestimates contact for capsules
             * lying on their side. */
            float r = shape->data.capsule.radius;
            float h = shape->data.capsule.half_height;
            vec3 local_y = quat_rotate_vec3(
                state->bodies[i].orientation, vec3_create(0, 1, 0));

            /* Test top cap sphere */
            vec3 top_center = vec3_add(state->bodies[i].position,
                vec3_scale(local_y, h));
            ForgePhysicsRigidBody temp_top = state->bodies[i];
            temp_top.position = top_center;
            ForgePhysicsRBContact c_top;
            if (forge_physics_rb_collide_sphere_plane(
                    &temp_top, i, r, plane_pt, plane_n,
                    DEFAULT_MU_S, DEFAULT_MU_D, &c_top)) {
                /* Fix contact point relative to actual body */
                if (num_contacts < FORGE_PHYSICS_MAX_RB_CONTACTS) {
                    contacts[num_contacts++] = c_top;
                }
            }

            /* Test bottom cap sphere */
            vec3 bot_center = vec3_sub(state->bodies[i].position,
                vec3_scale(local_y, h));
            ForgePhysicsRigidBody temp_bot = state->bodies[i];
            temp_bot.position = bot_center;
            ForgePhysicsRBContact c_bot;
            if (forge_physics_rb_collide_sphere_plane(
                    &temp_bot, i, r, plane_pt, plane_n,
                    DEFAULT_MU_S, DEFAULT_MU_D, &c_bot)) {
                if (num_contacts < FORGE_PHYSICS_MAX_RB_CONTACTS) {
                    contacts[num_contacts++] = c_bot;
                }
            }
            break;
        }

        default:
            break;
        }
    }

    /* Detect pairwise sphere-sphere contacts (body-body collisions).
     * Only between sphere-typed shapes for now. */
    for (int i = 0; i < state->num_bodies &&
         num_contacts < FORGE_PHYSICS_MAX_RB_CONTACTS; i++) {
        if (state->shapes[i].type != FORGE_PHYSICS_SHAPE_SPHERE) continue;
        for (int j = i + 1; j < state->num_bodies &&
             num_contacts < FORGE_PHYSICS_MAX_RB_CONTACTS; j++) {
            if (state->shapes[j].type != FORGE_PHYSICS_SHAPE_SPHERE) continue;
            ForgePhysicsRBContact c;
            if (forge_physics_rb_collide_sphere_sphere(
                    &state->bodies[i], i, state->shapes[i].data.sphere.radius,
                    &state->bodies[j], j, state->shapes[j].data.sphere.radius,
                    DEFAULT_MU_S, DEFAULT_MU_D, &c)) {
                contacts[num_contacts++] = c;
            }
        }
    }

    /* Resolve contacts with iterative solver */
    if (num_contacts > 0) {
        forge_physics_rb_resolve_contacts(contacts, num_contacts,
                                           state->bodies, state->num_bodies,
                                           DEFAULT_SOLVER_ITERS, PHYSICS_DT);
    }

    state->last_contact_count = num_contacts;

    /* Update cached AABBs */
    for (int i = 0; i < state->num_bodies; i++) {
        state->cached_aabbs[i] = forge_physics_shape_compute_aabb(
            &state->shapes[i], state->bodies[i].position,
            state->bodies[i].orientation);
    }
}

/* ── Helper: draw AABB wireframe ──────────────────────────────────── */

static void draw_aabb_wireframe(app_state *state, ForgePhysicsAABB aabb)
{
    vec3 center = forge_physics_aabb_center(aabb);
    vec3 he = forge_physics_aabb_extents(aabb);

    mat4 model = mat4_multiply(
        mat4_translate(center),
        mat4_scale(he));

    float color[4] = {AABB_COLOR_R, AABB_COLOR_G, AABB_COLOR_B, AABB_COLOR_A};
    forge_scene_draw_mesh_ex(&state->scene, state->wireframe_pipeline,
        state->cube_vb, state->cube_ib,
        state->cube_index_count, model, color);
}

/* ── Helper: draw a shape body ────────────────────────────────────── */

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
        case SDL_SCANCODE_3: set_scene(state, 2); break;
        case SDL_SCANCODE_4:
            if (state->scene_index == 1) {
                state->s2_shape_type = 0;
                init_current_scene(state);
            }
            break;
        case SDL_SCANCODE_5:
            if (state->scene_index == 1) {
                state->s2_shape_type = 1;
                init_current_scene(state);
            }
            break;
        case SDL_SCANCODE_6:
            if (state->scene_index == 1) {
                state->s2_shape_type = 2;
                init_current_scene(state);
            }
            break;
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
        "Physics Lesson 07 \xe2\x80\x94 Collision Shapes and Support Functions");
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
        return SDL_APP_FAILURE;
    }

    /* Generate and upload cube geometry */
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

    /* Generate and upload capsule geometry */
    state->capsule_mesh_half_h = 1.0f;  /* unit proportions */
    ForgeShape capsule = forge_shapes_capsule(
        CAPSULE_SLICES, CAPSULE_STACKS, CAPSULE_CAP_STACKS,
        state->capsule_mesh_half_h);
    if (capsule.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_capsule failed");
        return SDL_APP_FAILURE;
    }
    state->capsule_vb = upload_shape_vb(&state->scene, &capsule);
    state->capsule_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, capsule.indices,
        (Uint32)capsule.index_count * (Uint32)sizeof(uint32_t));
    state->capsule_index_count = (Uint32)capsule.index_count;
    forge_shapes_free(&capsule);
    if (!state->capsule_vb || !state->capsule_ib) {
        SDL_Log("ERROR: Failed to upload capsule geometry");
        return SDL_APP_FAILURE;
    }

    /* Small sphere for support point marker */
    ForgeShape marker = forge_shapes_sphere(MARKER_SLICES, MARKER_STACKS);
    if (marker.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_sphere (marker) failed");
        return SDL_APP_FAILURE;
    }
    state->marker_vb = upload_shape_vb(&state->scene, &marker);
    state->marker_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, marker.indices,
        (Uint32)marker.index_count * (Uint32)sizeof(uint32_t));
    state->marker_index_count = (Uint32)marker.index_count;
    forge_shapes_free(&marker);
    if (!state->marker_vb || !state->marker_ib) {
        SDL_Log("ERROR: Failed to upload marker geometry");
        return SDL_APP_FAILURE;
    }

    /* Default state */
    state->scene_index   = 0;
    state->accumulator   = 0.0f;
    state->paused        = false;
    state->speed_scale   = NORMAL_SPEED_SCALE;
    state->show_aabbs    = true;
    state->s2_shape_type = 0;
    state->sweep_angle   = 0.0f;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    init_current_scene(state);

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

    /* ── Fixed-timestep physics ──────────────────────────────────── */

    if (!state->paused) {
        float sim_dt = dt * state->speed_scale;

        if (state->scene_index == 1) {
            /* Scene 2: update sweep angle (runs outside physics step) */
            state->sweep_angle += S2_SWEEP_SPEED * sim_dt;
            if (state->sweep_angle > 2.0f * FORGE_PI)
                state->sweep_angle -= 2.0f * FORGE_PI;
        }

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

    /* Draw AABB wireframes if enabled.
     * Recompute from interpolated pose so the wireframe tracks the visible
     * mesh, not the last physics step position. */
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
            draw_aabb_wireframe(state, aabb);
        }
    }

    /* Scene 2: draw support point and direction arrow */
    if (state->scene_index == 1 && state->num_bodies > 0) {
        float angle = state->sweep_angle;
        /* Sweep in the XY plane */
        vec3 dir = vec3_create(
            SDL_cosf(angle), SDL_sinf(angle), 0.0f);
        vec3 body_pos = state->bodies[0].position;
        quat body_orient = state->bodies[0].orientation;

        /* Compute support point */
        vec3 support_pt = forge_physics_shape_support(
            &state->shapes[0], body_pos, body_orient, dir);

        /* Draw support point as small sphere */
        mat4 marker_model = mat4_multiply(
            mat4_translate(support_pt),
            mat4_scale(vec3_create(S2_MARKER_RADIUS, S2_MARKER_RADIUS, S2_MARKER_RADIUS)));
        float support_color[4] = {
            SUPPORT_COLOR_R, SUPPORT_COLOR_G,
            SUPPORT_COLOR_B, SUPPORT_COLOR_A};
        forge_scene_draw_mesh(s,
            state->marker_vb, state->marker_ib,
            state->marker_index_count, marker_model, support_color);

        /* Draw direction arrow as elongated cube */
        vec3 arrow_end = vec3_add(body_pos,
            vec3_scale(dir, S2_ARROW_LENGTH));
        vec3 arrow_mid = vec3_scale(
            vec3_add(body_pos, arrow_end), 0.5f);
        float arrow_len = S2_ARROW_LENGTH * 0.5f;
        /* Rotate the arrow to point in dir direction */
        mat4 arrow_model = mat4_multiply(
            mat4_translate(arrow_mid),
            mat4_multiply(
                quat_to_mat4(quat_from_euler(0.0f, 0.0f, angle)),
                mat4_scale(vec3_create(arrow_len, S2_ARROW_THICKNESS, S2_ARROW_THICKNESS))));
        float arrow_color[4] = {
            ARROW_COLOR_R, ARROW_COLOR_G,
            ARROW_COLOR_B, ARROW_COLOR_A};
        forge_scene_draw_mesh(s,
            state->cube_vb, state->cube_ib,
            state->cube_index_count, arrow_model, arrow_color);
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
            if (forge_ui_wctx_window_begin(wctx, "Collision Shapes",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* Scene selection */
                {
                    const char *scene_names[NUM_SCENES] = {
                        "1: Gallery", "2: Support", "3: Mixed"
                    };
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Scene: %s",
                                 scene_names[state->scene_index]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                if (forge_ui_ctx_button_layout(ui, "1: Shape Gallery",
                                                BUTTON_HEIGHT))
                    set_scene(state, 0);
                if (forge_ui_ctx_button_layout(ui, "2: Support Sweep",
                                                BUTTON_HEIGHT))
                    set_scene(state, 1);
                if (forge_ui_ctx_button_layout(ui, "3: Mixed Collisions",
                                                BUTTON_HEIGHT))
                    set_scene(state, 2);

                forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT * 0.5f);

                /* Scene 2: shape selection */
                if (state->scene_index == 1) {
                    const char *shape_names[] = {
                        "Sphere", "Box", "Capsule" };
                    int si = state->s2_shape_type;
                    if (si < 0 || si > 2) si = 0;
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Shape: %s (4/5/6)",
                                 shape_names[si]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* Info readouts */
                {
                    char buf[80];
                    SDL_snprintf(buf, sizeof(buf), "Bodies: %d",
                                 state->num_bodies);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Contacts: %d",
                                 state->last_contact_count);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    /* AABB status */
                    SDL_snprintf(buf, sizeof(buf), "AABBs: %s (V)",
                                 state->show_aabbs ? "ON" : "OFF");
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* AABB overlap count (Scene 1 & 3) */
                if (state->scene_index != 1) {
                    int overlap_count = 0;
                    for (int i = 0; i < state->num_bodies; i++) {
                        for (int j = i + 1; j < state->num_bodies; j++) {
                            if (forge_physics_aabb_overlap(
                                    state->cached_aabbs[i],
                                    state->cached_aabbs[j])) {
                                overlap_count++;
                            }
                        }
                    }
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "AABB overlaps: %d",
                                 overlap_count);
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

                /* AABB toggle */
                forge_ui_ctx_checkbox_layout(ui, "Show AABBs",
                                             &state->show_aabbs, LABEL_HEIGHT);

                /* Reset button */
                if (forge_ui_ctx_button_layout(ui, "Reset", BUTTON_HEIGHT)) {
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
        if (state->marker_vb)  SDL_ReleaseGPUBuffer(dev, state->marker_vb);
        if (state->marker_ib)  SDL_ReleaseGPUBuffer(dev, state->marker_ib);
    }

    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
