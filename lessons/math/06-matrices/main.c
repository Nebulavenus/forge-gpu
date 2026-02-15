/*
 * Math Lesson 06 — Matrices
 *
 * Demonstrates the mat4 transform functions from forge_math.h with concrete
 * numerical examples.  This lesson explains the transforms you'll use in
 * GPU Lesson 06 to render a spinning 3D cube.
 *
 * Sections:
 *   1. Identity matrix — the "do nothing" transform
 *   2. Translation — moving points by an offset
 *   3. Scaling — stretching or shrinking
 *   4. Rotation — spinning around each axis
 *   5. Composition — combining transforms (order matters!)
 *   6. The MVP pipeline — model, view, projection end-to-end
 *
 * No new math library functions — all mat4 functions already exist in
 * forge_math.h.  This lesson demonstrates and explains them.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include "math/forge_math.h"

/* ── Constants for the MVP pipeline demo (section 6) ──────────────────── */
#define FOV_DEG      60.0f
#define ASPECT_RATIO (16.0f / 9.0f)
#define NEAR_PLANE   0.1f
#define FAR_PLANE    100.0f

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

static void print_vec4(const char *label, vec4 v)
{
    printf("  %-40s (%.3f, %.3f, %.3f, %.3f)\n", label, v.x, v.y, v.z, v.w);
}

static void print_mat4(const char *label, mat4 m)
{
    printf("  %s\n", label);
    printf("    | %8.3f  %8.3f  %8.3f  %8.3f |\n",
           m.m[0], m.m[4], m.m[8],  m.m[12]);
    printf("    | %8.3f  %8.3f  %8.3f  %8.3f |\n",
           m.m[1], m.m[5], m.m[9],  m.m[13]);
    printf("    | %8.3f  %8.3f  %8.3f  %8.3f |\n",
           m.m[2], m.m[6], m.m[10], m.m[14]);
    printf("    | %8.3f  %8.3f  %8.3f  %8.3f |\n",
           m.m[3], m.m[7], m.m[11], m.m[15]);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("\n");
    printf("==============================================================\n");
    printf("  Matrices — Transforms for 3D Graphics\n");
    printf("==============================================================\n");
    printf("\n");
    printf("A 4x4 matrix (mat4) encodes a transformation: translation,\n");
    printf("rotation, scaling, or any combination.  Multiplying a point\n");
    printf("by a matrix applies that transformation.\n");
    printf("\n");
    printf("forge_math.h stores matrices in column-major order, matching\n");
    printf("HLSL's default layout.  This means:\n");
    printf("  - m[0..3]   = column 0 (X axis)\n");
    printf("  - m[4..7]   = column 1 (Y axis)\n");
    printf("  - m[8..11]  = column 2 (Z axis)\n");
    printf("  - m[12..15] = column 3 (translation)\n");

    /* ── 1. Identity ─────────────────────────────────────────────────── */

    print_header("1. IDENTITY MATRIX -- THE 'DO NOTHING' TRANSFORM");

    printf("  The identity matrix leaves every point unchanged:\n");
    printf("  I * v = v\n\n");

    mat4 identity = mat4_identity();
    print_mat4("mat4_identity():", identity);

    vec4 point = vec4_create(3.0f, 5.0f, -2.0f, 1.0f);
    vec4 result = mat4_multiply_vec4(identity, point);

    printf("\n");
    print_vec4("Original point:", point);
    print_vec4("Identity * point:", result);
    printf("\n  No change, as expected!\n");

    /* ── 2. Translation ──────────────────────────────────────────────── */

    print_header("2. TRANSLATION -- MOVING POINTS");

    printf("  Translation adds an offset to every point.\n");
    printf("  Only affects points (w=1), not directions (w=0).\n\n");

    vec3 offset = vec3_create(10.0f, 0.0f, -5.0f);
    mat4 translate = mat4_translate(offset);
    print_mat4("mat4_translate(10, 0, -5):", translate);

    printf("\n  Notice: the offset appears in column 3 (m[12..14]).\n\n");

    /* Translate a point (w=1) */
    vec4 p = vec4_create(1.0f, 2.0f, 3.0f, 1.0f);
    vec4 moved = mat4_multiply_vec4(translate, p);

    print_vec4("Point (1, 2, 3, w=1):", p);
    print_vec4("After translate:", moved);

    /* Translate a direction (w=0) — should be unaffected */
    vec4 dir = vec4_create(1.0f, 0.0f, 0.0f, 0.0f);
    vec4 dir_result = mat4_multiply_vec4(translate, dir);

    printf("\n");
    print_vec4("Direction (1, 0, 0, w=0):", dir);
    print_vec4("After translate:", dir_result);
    printf("\n  Direction unchanged — w=0 ignores translation.\n");
    printf("  This is why w=1 means 'position' and w=0 means 'direction'.\n");

    /* ── 3. Scaling ──────────────────────────────────────────────────── */

    print_header("3. SCALING -- STRETCHING AND SHRINKING");

    printf("  Uniform scaling multiplies all axes by the same factor.\n");
    printf("  Non-uniform scaling uses different factors per axis.\n\n");

    /* Uniform scale */
    mat4 scale2 = mat4_scale_uniform(2.0f);
    print_mat4("mat4_scale_uniform(2):", scale2);

    vec4 sp = vec4_create(3.0f, 4.0f, 5.0f, 1.0f);
    vec4 scaled = mat4_multiply_vec4(scale2, sp);

    printf("\n");
    print_vec4("Point (3, 4, 5):", sp);
    print_vec4("After scale by 2:", scaled);

    /* Non-uniform scale */
    vec3 non_uniform = vec3_create(1.0f, 2.0f, 0.5f);
    mat4 scale_nu = mat4_scale(non_uniform);
    vec4 scaled_nu = mat4_multiply_vec4(scale_nu, sp);

    printf("\n");
    print_mat4("mat4_scale(1, 2, 0.5):", scale_nu);
    printf("\n");
    print_vec4("Point (3, 4, 5):", sp);
    print_vec4("After non-uniform scale:", scaled_nu);
    printf("  X unchanged, Y doubled, Z halved.\n");

    /* ── 4. Rotation ─────────────────────────────────────────────────── */

    print_header("4. ROTATION -- SPINNING AROUND AN AXIS");

    printf("  Rotation matrices spin points around one of the three axes.\n");
    printf("  We use radians — 90 degrees = PI/2 = %.4f radians.\n\n",
           FORGE_PI / 2.0f);

    /* Rotate 90 degrees around Z — X axis becomes Y axis */
    float angle_90 = FORGE_PI / 2.0f;
    mat4 rot_z = mat4_rotate_z(angle_90);
    print_mat4("mat4_rotate_z(PI/2)  [90 deg]:", rot_z);

    vec4 x_axis = vec4_create(1.0f, 0.0f, 0.0f, 0.0f);
    vec4 rotated_x = mat4_multiply_vec4(rot_z, x_axis);

    printf("\n");
    print_vec4("X axis (1, 0, 0):", x_axis);
    print_vec4("After 90-deg Z rotation:", rotated_x);
    printf("  X axis became Y axis — correct for CCW rotation!\n");

    /* Rotate around X */
    mat4 rot_x = mat4_rotate_x(angle_90);
    vec4 y_axis = vec4_create(0.0f, 1.0f, 0.0f, 0.0f);
    vec4 rotated_y = mat4_multiply_vec4(rot_x, y_axis);

    printf("\n");
    print_vec4("Y axis (0, 1, 0):", y_axis);
    print_vec4("After 90-deg X rotation:", rotated_y);
    printf("  Y axis became Z axis.\n");

    /* Rotate around Y */
    mat4 rot_y = mat4_rotate_y(angle_90);
    vec4 z_axis = vec4_create(0.0f, 0.0f, 1.0f, 0.0f);
    vec4 rotated_z = mat4_multiply_vec4(rot_y, z_axis);

    printf("\n");
    print_vec4("Z axis (0, 0, 1):", z_axis);
    print_vec4("After 90-deg Y rotation:", rotated_z);
    printf("  Z axis became +X axis (right-hand rule).\n");

    /* ── 5. Composition ──────────────────────────────────────────────── */

    print_header("5. COMPOSITION -- COMBINING TRANSFORMS");

    printf("  mat4_multiply(A, B) means 'apply B first, then A'.\n");
    printf("  Order matters!  Translate-then-rotate is different from\n");
    printf("  rotate-then-translate.\n\n");

    /* Set up: translate by (5, 0, 0), rotate 90 around Z */
    mat4 translate_x = mat4_translate(vec3_create(5.0f, 0.0f, 0.0f));
    mat4 rotate_z = mat4_rotate_z(angle_90);

    /* Order 1: rotate first, then translate (T * R) */
    mat4 translate_rotate = mat4_multiply(translate_x, rotate_z);
    vec4 test_point = vec4_create(1.0f, 0.0f, 0.0f, 1.0f);
    vec4 result_translate_rotate = mat4_multiply_vec4(translate_rotate, test_point);

    printf("  Starting point: (1, 0, 0)\n\n");

    printf("  Order 1: Rotate 90 around Z, THEN translate by (5, 0, 0)\n");
    printf("    Combined = Translate * Rotate\n");
    print_vec4("    Result:", result_translate_rotate);
    printf("    (1,0,0) -> rotate -> (0,1,0) -> translate -> (5,1,0)\n\n");

    /* Order 2: translate first, then rotate (R * T) */
    mat4 rotate_translate = mat4_multiply(rotate_z, translate_x);
    vec4 result_rotate_translate = mat4_multiply_vec4(rotate_translate, test_point);

    printf("  Order 2: Translate by (5, 0, 0), THEN rotate 90 around Z\n");
    printf("    Combined = Rotate * Translate\n");
    print_vec4("    Result:", result_rotate_translate);
    printf("    (1,0,0) -> translate -> (6,0,0) -> rotate -> (0,6,0)\n\n");

    printf("  DIFFERENT results!  Transform order is critical.\n");
    printf("  In 3D graphics, the standard order is:\n");
    printf("    MVP = Projection * View * Model\n");
    printf("  Which applies: Model first, then View, then Projection.\n");

    /* ── 6. The MVP pipeline ─────────────────────────────────────────── */

    print_header("6. THE MVP PIPELINE -- MODEL, VIEW, PROJECTION");

    printf("  Every 3D vertex goes through three transforms:\n\n");
    printf("    Model      — object space to world space\n");
    printf("    View       — world space to camera space\n");
    printf("    Projection — camera space to clip space (perspective)\n\n");
    printf("  Let's trace a cube corner through each stage.\n\n");

    /* A corner of a unit cube, before any transform */
    vec4 cube_corner = vec4_create(0.5f, 0.5f, 0.5f, 1.0f);
    print_vec4("Cube corner (object space):", cube_corner);

    /* Model: rotate 45 degrees around Y, then translate up by 1 */
    float angle_45 = FORGE_PI / 4.0f;
    mat4 model_rotate = mat4_rotate_y(angle_45);
    mat4 model_translate = mat4_translate(vec3_create(0.0f, 1.0f, 0.0f));
    mat4 model = mat4_multiply(model_translate, model_rotate);

    vec4 world_pos = mat4_multiply_vec4(model, cube_corner);
    printf("\n  Model transform: rotate 45 around Y, translate up by 1\n");
    print_vec4("After Model (world space):", world_pos);

    /* View: camera at (0, 1.5, 3) looking at origin */
    vec3 eye    = vec3_create(0.0f, 1.5f, 3.0f);
    vec3 target = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 up     = vec3_create(0.0f, 1.0f, 0.0f);
    mat4 view   = mat4_look_at(eye, target, up);

    vec4 view_pos = mat4_multiply_vec4(view, world_pos);
    printf("\n  View: camera at (0, 1.5, 3) looking at origin\n");
    print_vec4("After View (camera space):", view_pos);
    printf("  Negative Z means the point is in front of the camera.\n");

    /* Projection: 60-degree FOV, 16:9 aspect */
    float fov = FOV_DEG * FORGE_DEG2RAD;
    float aspect = ASPECT_RATIO;
    mat4 projection = mat4_perspective(fov, aspect, NEAR_PLANE, FAR_PLANE);

    vec4 clip_pos = mat4_multiply_vec4(projection, view_pos);
    printf("\n  Projection: 60-degree FOV, 16:9 aspect\n");
    print_vec4("After Projection (clip space):", clip_pos);

    /* Perspective divide */
    vec4 ndc;
    if (clip_pos.w != 0.0f) {
        ndc = vec4_create(
            clip_pos.x / clip_pos.w,
            clip_pos.y / clip_pos.w,
            clip_pos.z / clip_pos.w,
            1.0f);
    } else {
        ndc = vec4_create(0.0f, 0.0f, 0.0f, 1.0f);
    }

    printf("\n  Perspective divide: divide xyz by w\n");
    print_vec4("NDC (screen mapping):", ndc);
    printf("\n  X in [-1, 1]: left to right on screen\n");
    printf("  Y in [-1, 1]: bottom to top on screen\n");
    printf("  Z in [0, 1]:  near to far (for depth testing)\n");

    /* Show the combined MVP */
    printf("\n  In practice, we combine all three into one matrix:\n");
    printf("    MVP = Projection * View * Model\n");
    printf("  Then in the vertex shader: clip_pos = MVP * vertex\n");

    mat4 vp  = mat4_multiply(projection, view);
    mat4 mvp = mat4_multiply(vp, model);

    vec4 combined_result = mat4_multiply_vec4(mvp, cube_corner);
    printf("\n");
    print_vec4("MVP * cube_corner (clip space):", combined_result);

    /* Verify it matches */
    printf("\n  Matches the step-by-step clip-space result above!\n");
    printf("  The GPU does this for every vertex, every frame.\n");

    /* ── Summary ─────────────────────────────────────────────────────── */

    printf("\n");
    printf("==============================================================\n");
    printf("  Summary\n");
    printf("==============================================================\n");
    printf("\n");
    printf("  Transforms:\n");
    printf("    * mat4_identity()       -- leaves points unchanged\n");
    printf("    * mat4_translate(v)     -- moves points by offset v\n");
    printf("    * mat4_scale(v)         -- scales each axis independently\n");
    printf("    * mat4_scale_uniform(s) -- scales all axes equally\n");
    printf("    * mat4_rotate_x/y/z(a) -- rotates around one axis\n");
    printf("\n");
    printf("  Composition:\n");
    printf("    * mat4_multiply(A, B)       -- apply B first, then A\n");
    printf("    * mat4_multiply_vec4(M, v)  -- transform a point/direction\n");
    printf("    * Order matters: T*R != R*T\n");
    printf("\n");
    printf("  Camera:\n");
    printf("    * mat4_look_at(eye, target, up) -- view matrix\n");
    printf("    * mat4_perspective(fov, asp, n, f) -- projection matrix\n");
    printf("\n");
    printf("  The MVP pipeline:\n");
    printf("    * MVP = Projection * View * Model\n");
    printf("    * Applied per-vertex in the vertex shader\n");
    printf("    * Transforms: object -> world -> camera -> clip -> NDC\n");
    printf("\n");
    printf("  See: lessons/math/06-matrices/README.md\n");
    printf("  See: lessons/gpu/06-depth-and-3d/ (using MVP in practice)\n");
    printf("  See: lessons/math/02-coordinate-spaces/ (the theory)\n");
    printf("\n");

    SDL_Quit();
    return 0;
}
