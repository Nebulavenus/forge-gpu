/*
 * Math Lesson 06 — Projections
 *
 * How 3D scenes map to 2D screens. Covers the full projection pipeline
 * from the core insight (similar triangles) through the matrix math and
 * into the GPU's clip-space / NDC machinery.
 *
 * Sections:
 *   1.  Perspective without a matrix — similar triangles: x' = x·n/(-z)
 *   2.  The perspective projection matrix — mat4_perspective
 *   3.  Clip space to NDC — vec3_perspective_divide
 *   4.  Frustum dimensions from FOV
 *   5.  Perspective-correct interpolation — why naive lerp fails
 *   6.  Orthographic projection — mat4_orthographic
 *   7.  Asymmetric perspective — mat4_perspective_from_planes
 *   8.  Comparing projections — side by side
 *   9.  Summary
 *
 * New math library additions in this lesson:
 *   vec3_perspective_divide, mat4_perspective_from_planes
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <SDL3/SDL_main.h>
#include "math/forge_math.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Section 1: Perspective without a matrix */
#define SEC1_NEAR              1.0f
#define SEC1_POINT_X           2.0f
#define SEC1_POINT_Y           1.0f
#define SEC1_DEPTH_A          -2.0f
#define SEC1_DEPTH_B          -4.0f
#define SEC1_DEPTH_C          -8.0f

/* Section 2: Perspective projection matrix */
#define SEC2_FOV_DEG           90.0f
#define SEC2_ASPECT_W          16.0f
#define SEC2_ASPECT_H           9.0f
#define SEC2_NEAR               0.1f
#define SEC2_FAR              100.0f
#define SEC2_TEST_X             3.0f
#define SEC2_TEST_Y             2.0f
#define SEC2_TEST_Z            -5.0f

/* Section 3: Clip space to NDC */
#define SEC3_FOV_DEG           60.0f
#define SEC3_NEAR               0.1f
#define SEC3_FAR              100.0f
#define SEC3_MID_Z            -50.0f
#define SEC3_OFF_X              5.0f
#define SEC3_OFF_Y              3.0f
#define SEC3_OFF_Z            -10.0f

/* Section 4: Frustum dimensions */
#define SEC4_FOV_DEG           60.0f

/* Section 5: Perspective-correct interpolation */
#define SEC5_Z_NEAR            -1.0f
#define SEC5_Z_FAR             -4.0f
#define SEC5_U_NEAR             0.0f
#define SEC5_U_FAR              1.0f
#define SEC5_SCREEN_MID         0.5f
#define SEC5_NUM_SAMPLES       10

/* Section 6: Orthographic projection */
#define SEC6_EXTENT            10.0f
#define SEC6_NEAR               0.1f
#define SEC6_FAR              100.0f

/* Section 7: Asymmetric perspective */
#define SEC7_ASYM_LEFT         -0.06f
#define SEC7_ASYM_RIGHT         0.04f
#define SEC7_ASYM_BOTTOM       -0.05f
#define SEC7_ASYM_TOP           0.05f

/* Section 8: Comparing projections */
#define SEC8_ASPECT             1.0f
#define SEC8_NEAR               1.0f
#define SEC8_FAR              100.0f
#define SEC8_REF_DEPTH         10.0f
#define SEC8_TEST_X             2.0f

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void print_vec3(const char *label, vec3 v)
{
    SDL_Log("  %s = (%.4f, %.4f, %.4f)", label, v.x, v.y, v.z);
}

static void print_vec4(const char *label, vec4 v)
{
    SDL_Log("  %s = (%.4f, %.4f, %.4f, %.4f)", label, v.x, v.y, v.z, v.w);
}

static void print_mat4(const char *label, mat4 m)
{
    SDL_Log("  %s:", label);
    SDL_Log("    | %8.4f %8.4f %8.4f %8.4f |",
            m.m[0], m.m[4], m.m[8],  m.m[12]);
    SDL_Log("    | %8.4f %8.4f %8.4f %8.4f |",
            m.m[1], m.m[5], m.m[9],  m.m[13]);
    SDL_Log("    | %8.4f %8.4f %8.4f %8.4f |",
            m.m[2], m.m[6], m.m[10], m.m[14]);
    SDL_Log("    | %8.4f %8.4f %8.4f %8.4f |",
            m.m[3], m.m[7], m.m[11], m.m[15]);
}

static int approx_eq(float a, float b, float eps)
{
    float diff = a - b;
    return (diff < eps) && (diff > -eps);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("=============================================================\n");
    printf("  Math Lesson 06 — Projections\n");
    printf("  How 3D scenes map to 2D screens\n");
    printf("=============================================================\n\n");

    /* ── Section 1: Perspective without a matrix ─────────────────────────
     *
     * The fundamental insight: perspective is just similar triangles.
     *
     * Two triangles are "similar" when they have the same angles. Their
     * shapes are identical — one is a scaled version of the other. The
     * key property: the ratios of corresponding sides are equal.
     *
     * Imagine a camera at the origin, looking down -Z. A point P sits
     * at (x, y, z) in view space, and the near plane is at distance n.
     * A line from the eye through P forms two right triangles:
     *
     *        n             -z
     *   ◄─────────►◄────────────────►
     *   :    P'     :       P        :
     *   +----*------+- - - -*- - - - -    (the horizontal axis)
     *   |   /       :     / |
     *   |  / small  :    /  |
     *   | /  tri    :   / big tri
     *   |/ θ        :  /   |
     *   *- - - - - -+*- - - - - - - -
     *  eye         near   x (or y)
     *
     * Both triangles share angle θ at the eye, and both have a right
     * angle where the point meets the axis. Same angles → similar
     * triangles → equal side ratios:
     *
     *     x_screen / n = x / (-z)
     *     x_screen = x * near / (-z)
     *     y_screen = y * near / (-z)
     *
     * That's it. No matrices needed. Objects farther from the camera
     * (larger -z) get divided by a bigger number → appear smaller.
     */
    printf("── 1. Perspective without a matrix ────────────────────────\n\n");
    printf("  Similar triangles: two triangles with the same angles.\n");
    printf("  Their side ratios are equal — one is a scaled copy of the other.\n\n");
    printf("  A line from the eye through a point P creates two right triangles\n");
    printf("  (one to the near plane, one to P). Same angles, so:\n\n");
    printf("      x_screen / n  =  x / (-z)\n\n");
    printf("  Solving: x_screen = x * near / (-z)\n");
    printf("           y_screen = y * near / (-z)\n\n");

    {
        float near = SEC1_NEAR;

        /* Test points at increasing depth (z is negative in view space) */
        vec3 points[] = {
            vec3_create(SEC1_POINT_X, SEC1_POINT_Y, SEC1_DEPTH_A),
            vec3_create(SEC1_POINT_X, SEC1_POINT_Y, SEC1_DEPTH_B),
            vec3_create(SEC1_POINT_X, SEC1_POINT_Y, SEC1_DEPTH_C),
        };
        int num_points = sizeof(points) / sizeof(points[0]);

        SDL_Log("  Near plane distance: %.1f", near);
        SDL_Log("  Projecting points with same (x,y) at different depths:\n");

        for (int i = 0; i < num_points; i++) {
            float x = points[i].x;
            float y = points[i].y;
            float z = points[i].z;
            float x_screen = x * near / (-z);
            float y_screen = y * near / (-z);
            SDL_Log("    (%5.1f, %5.1f, %5.1f) -> screen (%.4f, %.4f)"
                    "  [scale = n/(-z) = %.4f]",
                    x, y, z, x_screen, y_screen, near / (-z));
        }
        printf("\n  Notice: same (x,y), but farther z → smaller on screen.\n");
        printf("  That's perspective foreshortening.\n\n");
    }

    /* ── Section 2: The perspective projection matrix ────────────────────
     *
     * mat4_perspective wraps this similar-triangles idea into a matrix that
     * also handles:
     *   - FOV (field of view) → how wide the camera sees
     *   - Aspect ratio → non-square screens
     *   - Depth mapping → z mapped to [0, 1] for the depth buffer
     *
     * The matrix output is in CLIP SPACE — not yet divided by w.
     * The w component is set to -z, so the GPU can divide later.
     */
    printf("── 2. The perspective projection matrix ───────────────────\n\n");

    {
        float fov = SEC2_FOV_DEG * FORGE_DEG2RAD;  /* 90° vertical FOV */
        float aspect = SEC2_ASPECT_W / SEC2_ASPECT_H;
        float near = SEC2_NEAR;
        float far = SEC2_FAR;
        mat4 proj = mat4_perspective(fov, aspect, near, far);

        print_mat4("Perspective matrix (90° FOV, 16:9, near=0.1, far=100)", proj);
        printf("\n");

        /* Transform a point and show clip-space output */
        vec4 view_point = vec4_create(SEC2_TEST_X, SEC2_TEST_Y, SEC2_TEST_Z, 1.0f);
        vec4 clip = mat4_multiply_vec4(proj, view_point);

        print_vec4("View-space point", view_point);
        print_vec4("Clip-space result", clip);
        SDL_Log("  Note: w = %.4f = -z = -(%.4f)", clip.w, view_point.z);
        printf("\n");
    }

    /* ── Section 3: Clip space → NDC (perspective divide) ────────────────
     *
     * Clip space is what the vertex shader outputs.
     * NDC (Normalized Device Coordinates) is what the rasterizer uses.
     * The step between them: divide x, y, z by w.
     *
     *   NDC.x = clip.x / clip.w   ∈ [-1, 1]
     *   NDC.y = clip.y / clip.w   ∈ [-1, 1]
     *   NDC.z = clip.z / clip.w   ∈ [0, 1]
     *
     * Pipeline: Vertex shader → Clip space → Clipping → Perspective divide → NDC → Rasterizer
     *
     * We provide vec3_perspective_divide() to do this on the CPU.
     */
    printf("── 3. Clip space to NDC (perspective divide) ──────────────\n\n");

    {
        float fov = SEC3_FOV_DEG * FORGE_DEG2RAD;
        float aspect = SEC2_ASPECT_W / SEC2_ASPECT_H;
        float near = SEC3_NEAR;
        float far = SEC3_FAR;
        mat4 proj = mat4_perspective(fov, aspect, near, far);

        /* Test several points */
        vec4 test_points[] = {
            vec4_create(0.0f, 0.0f, -near, 1.0f),       /* on near plane */
            vec4_create(0.0f, 0.0f, -far, 1.0f),         /* on far plane */
            vec4_create(0.0f, 0.0f, SEC3_MID_Z, 1.0f),   /* halfway */
            vec4_create(SEC3_OFF_X, SEC3_OFF_Y, SEC3_OFF_Z, 1.0f), /* off-center */
        };
        const char *names[] = {
            "On near plane   ", "On far plane    ",
            "Halfway (z=-50) ", "Off-center      "
        };
        int n = sizeof(test_points) / sizeof(test_points[0]);

        SDL_Log("  Projection: 60° FOV, 16:9, near=0.1, far=100");
        printf("\n");

        for (int i = 0; i < n; i++) {
            vec4 clip = mat4_multiply_vec4(proj, test_points[i]);
            vec3 ndc = vec3_perspective_divide(clip);
            SDL_Log("  %s  view z=%7.1f  →  NDC (%.4f, %.4f, %.4f)",
                    names[i], test_points[i].z, ndc.x, ndc.y, ndc.z);
        }

        printf("\n  Near plane → NDC z ≈ 0, far plane → NDC z ≈ 1\n");
        printf("  Center of screen → NDC x,y ≈ 0\n\n");
    }

    /* ── Section 4: Frustum dimensions from FOV ──────────────────────────
     *
     * FOV and aspect ratio determine the size of the near-plane rectangle.
     * This is how mat4_perspective computes its scaling factors:
     *
     *   half_height = near * tan(fov_y / 2)
     *   half_width  = half_height * aspect
     *
     * These define the frustum — the truncated pyramid of visible space.
     */
    printf("── 4. Frustum dimensions from FOV ─────────────────────────\n\n");

    {
        float fov_deg = SEC4_FOV_DEG;
        float fov = fov_deg * FORGE_DEG2RAD;
        float aspect = SEC2_ASPECT_W / SEC2_ASPECT_H;
        float near = SEC3_NEAR;
        float far = SEC3_FAR;

        float half_h = near * tanf(fov * 0.5f);
        float half_w = half_h * aspect;

        SDL_Log("  FOV: %.0f°  Aspect: %.4f  Near: %.1f  Far: %.1f",
                fov_deg, aspect, near, far);
        SDL_Log("  Near plane half-height: %.6f", half_h);
        SDL_Log("  Near plane half-width:  %.6f", half_w);
        SDL_Log("  Near plane rectangle: [%.6f, %.6f] x [%.6f, %.6f]",
                -half_w, half_w, -half_h, half_h);

        /* The perspective matrix scaling factors */
        float sx = 1.0f / (aspect * tanf(fov * 0.5f));
        float sy = 1.0f / tanf(fov * 0.5f);
        SDL_Log("  Matrix scale X (m[0]):  %.6f = 1 / (aspect * tan(fov/2))", sx);
        SDL_Log("  Matrix scale Y (m[5]):  %.6f = 1 / tan(fov/2)", sy);
        printf("\n");

        /*
         * ASCII frustum diagram (top-down view, looking from above)
         *
         *              near plane
         *            ┌───────────┐
         *           /             \
         *          /               \
         *         /                 \
         *        /                   \
         *       /                     \
         *      /        frustum        \
         *     /                         \
         *    ┌───────────────────────────┐
         *              far plane
         *
         *    eye ◄──── near ────►◄─────────── far ──────────────►
         */
        printf("  The frustum is a truncated pyramid:\n\n");
        printf("                near plane\n");
        printf("              +-----+-----+\n");
        printf("             /      |      \\\n");
        printf("            /       |       \\\n");
        printf("           /        |        \\\n");
        printf("     eye *          |         \\\n");
        printf("           \\        |        /\n");
        printf("            \\       |       /\n");
        printf("             \\      |      /\n");
        printf("              +-----+-----+\n");
        printf("                far plane\n\n");
    }

    /* ── Section 5: Perspective-correct interpolation ────────────────────
     *
     * When the GPU rasterizes a triangle, it interpolates vertex attributes
     * (UVs, colors, etc.) across the surface. But in perspective, a naive
     * screen-space lerp gives wrong results — textures appear to "swim."
     *
     * The fix: interpolate (attribute / w) and (1 / w) in screen space,
     * then divide: attribute = (attribute/w) / (1/w).
     *
     * This is called "perspective-correct interpolation" and the GPU does
     * it automatically. But understanding why is important.
     */
    printf("── 5. Perspective-correct interpolation ───────────────────\n\n");

    {
        /* Two triangle vertices at different depths */
        float z_near = SEC5_Z_NEAR;   /* near vertex */
        float z_far  = SEC5_Z_FAR;    /* far vertex */
        float u_near = SEC5_U_NEAR;   /* UV at near vertex */
        float u_far  = SEC5_U_FAR;    /* UV at far vertex */

        float w_near = -z_near;  /* w = -z in perspective */
        float w_far  = -z_far;

        SDL_Log("  Two vertices at z=%.1f (u=%.1f) and z=%.1f (u=%.1f)",
                z_near, u_near, z_far, u_far);
        printf("\n");

        /* Compare at screen-space midpoint (t = 0.5) */
        float t = SEC5_SCREEN_MID;

        /* Wrong: naive screen-space lerp */
        float u_wrong = forge_lerpf(u_near, u_far, t);

        /* Correct: perspective-correct interpolation
         * Interpolate u/w and 1/w in screen space, then divide */
        float inv_w_near = 1.0f / w_near;
        float inv_w_far  = 1.0f / w_far;
        float u_over_w_near = u_near / w_near;
        float u_over_w_far  = u_far / w_far;

        float inv_w_interp    = forge_lerpf(inv_w_near, inv_w_far, t);
        float u_over_w_interp = forge_lerpf(u_over_w_near, u_over_w_far, t);
        float u_correct       = u_over_w_interp / inv_w_interp;

        SDL_Log("  At screen midpoint (t = %.1f):", t);
        SDL_Log("    Naive lerp (wrong):     u = %.4f", u_wrong);
        SDL_Log("    Perspective-correct:     u = %.4f", u_correct);
        SDL_Log("    Difference:              %.4f", u_correct - u_wrong);
        printf("\n");
        printf("  The naive lerp gives 0.5 (screen midpoint = texture midpoint).\n");
        printf("  But the correct value is %.4f — biased toward the near vertex,\n",
               u_correct);
        printf("  because the near vertex covers more screen space.\n\n");

        /* Show several sample points */
        SDL_Log("  Full interpolation comparison:");
        SDL_Log("    screen_t | naive_u | correct_u | error");
        SDL_Log("    ---------|---------|-----------|------");
        for (int i = 0; i <= SEC5_NUM_SAMPLES; i++) {
            float st = (float)i / (float)SEC5_NUM_SAMPLES;
            float naive = forge_lerpf(u_near, u_far, st);
            float iw = forge_lerpf(inv_w_near, inv_w_far, st);
            float uw = forge_lerpf(u_over_w_near, u_over_w_far, st);
            float correct = uw / iw;
            SDL_Log("      %.1f    |  %.4f |   %.4f  |  %.4f",
                    st, naive, correct, correct - naive);
        }
        printf("\n");
    }

    /* ── Section 6: Orthographic projection ──────────────────────────────
     *
     * Orthographic projection maps an axis-aligned box to NDC without
     * perspective foreshortening. Parallel lines stay parallel.
     *
     *   - No perspective divide (w stays 1)
     *   - Objects don't get smaller with distance
     *   - Used for: 2D rendering, shadow maps, CAD, architectural views
     *
     * The matrix just scales and translates each axis to fit [-1,1] for
     * x and y, and [0,1] for z.
     */
    printf("── 6. Orthographic projection ─────────────────────────────\n\n");

    {
        float left = -SEC6_EXTENT, right = SEC6_EXTENT;
        float bottom = -SEC6_EXTENT, top = SEC6_EXTENT;
        float near = SEC6_NEAR, far = SEC6_FAR;
        mat4 ortho = mat4_orthographic(left, right, bottom, top, near, far);

        print_mat4("Orthographic matrix ([-10,10] x [-10,10], near=0.1, far=100)",
                   ortho);
        printf("\n");

        /* Transform corners of the ortho box — they should map to NDC corners */
        vec4 corners[] = {
            vec4_create(left, bottom, -near, 1.0f),   /* near-bottom-left */
            vec4_create(right, top, -near, 1.0f),     /* near-top-right */
            vec4_create(0.0f, 0.0f, -near, 1.0f),    /* near-center */
            vec4_create(0.0f, 0.0f, -far, 1.0f),     /* far-center */
            vec4_create(left, bottom, -far, 1.0f),    /* far-bottom-left */
        };
        const char *corner_names[] = {
            "Near-bottom-left", "Near-top-right  ",
            "Near-center     ", "Far-center      ",
            "Far-bottom-left "
        };
        int nc = sizeof(corners) / sizeof(corners[0]);

        for (int i = 0; i < nc; i++) {
            vec4 clip = mat4_multiply_vec4(ortho, corners[i]);
            /* Ortho: w=1, so NDC = clip.xyz */
            SDL_Log("  %s  (%6.1f, %6.1f, %6.1f)  →  NDC (%.4f, %.4f, %.4f)"
                    "  w=%.1f",
                    corner_names[i],
                    corners[i].x, corners[i].y, corners[i].z,
                    clip.x, clip.y, clip.z, clip.w);
        }

        printf("\n  Note: w stays 1.0 — no perspective divide needed.\n");
        printf("  The box maps linearly to NDC.\n\n");
    }

    /* ── Section 7: Asymmetric perspective ───────────────────────────────
     *
     * mat4_perspective_from_planes lets you specify the near-plane rectangle
     * directly (left, right, bottom, top) instead of using FOV + aspect.
     *
     * The symmetric case should produce the same result as mat4_perspective.
     * Asymmetric frustums are used for VR (each eye is off-center) and
     * multi-monitor setups.
     */
    printf("── 7. Asymmetric perspective ──────────────────────────────\n\n");

    {
        /* First: verify symmetric case matches mat4_perspective */
        float fov = SEC3_FOV_DEG * FORGE_DEG2RAD;
        float aspect = SEC2_ASPECT_W / SEC2_ASPECT_H;
        float near = SEC3_NEAR;
        float far = SEC3_FAR;

        float half_h = near * tanf(fov * 0.5f);
        float half_w = half_h * aspect;

        mat4 sym_fov   = mat4_perspective(fov, aspect, near, far);
        mat4 sym_planes = mat4_perspective_from_planes(
            -half_w, half_w, -half_h, half_h, near, far);

        /* Compare all 16 elements */
        int match = 1;
        for (int i = 0; i < 16; i++) {
            if (!approx_eq(sym_fov.m[i], sym_planes.m[i], 1e-5f)) {
                match = 0;
                SDL_Log("  MISMATCH at element %d: %.6f vs %.6f",
                        i, sym_fov.m[i], sym_planes.m[i]);
            }
        }
        SDL_Log("  Symmetric case: mat4_perspective vs mat4_perspective_from_planes");
        SDL_Log("  Match: %s", match ? "YES (all 16 elements equal)" : "NO");
        printf("\n");

        /* Now show an asymmetric frustum (like a VR left eye) */
        float asym_left = SEC7_ASYM_LEFT;
        float asym_right = SEC7_ASYM_RIGHT;
        float asym_bottom = SEC7_ASYM_BOTTOM;
        float asym_top = SEC7_ASYM_TOP;

        mat4 asym = mat4_perspective_from_planes(
            asym_left, asym_right, asym_bottom, asym_top, near, far);

        SDL_Log("  Asymmetric frustum (VR-style left eye):");
        SDL_Log("    near plane: [%.2f, %.2f] x [%.2f, %.2f]",
                asym_left, asym_right, asym_bottom, asym_top);
        print_mat4("Asymmetric perspective", asym);
        printf("\n");

        /* Note the off-diagonal elements in row 3 (m[8] and m[9]) — these
         * shift the frustum center away from the view axis */
        SDL_Log("  m[8]  = %.4f  (X center shift — nonzero for asymmetric)",
                asym.m[8]);
        SDL_Log("  m[9]  = %.4f  (Y center shift — zero because bottom=-top)",
                asym.m[9]);
        printf("\n");

        /* Transform center of near plane — should map to shifted NDC */
        float cx = (asym_left + asym_right) * 0.5f;
        float cy = (asym_bottom + asym_top) * 0.5f;
        vec4 center_near = vec4_create(cx, cy, -near, 1.0f);
        vec4 clip = mat4_multiply_vec4(asym, center_near);
        vec3 ndc = vec3_perspective_divide(clip);
        SDL_Log("  Near plane center (%.3f, %.3f, %.3f) → NDC (%.4f, %.4f, %.4f)",
                cx, cy, -near, ndc.x, ndc.y, ndc.z);
        printf("  (Asymmetric center maps to NDC origin)\n\n");
    }

    /* ── Section 8: Comparing projections ────────────────────────────────
     *
     * Side-by-side comparison: same points through perspective vs ortho.
     * Key difference: perspective makes far objects smaller, ortho doesn't.
     */
    printf("── 8. Comparing projections ───────────────────────────────\n\n");

    {
        float fov = SEC3_FOV_DEG * FORGE_DEG2RAD;
        float aspect = SEC8_ASPECT;  /* Square for simple comparison */
        float near = SEC8_NEAR;
        float far = SEC8_FAR;

        mat4 persp = mat4_perspective(fov, aspect, near, far);

        /* For fair comparison, set ortho bounds to match the perspective
         * frustum at a reference depth */
        float ref_depth = SEC8_REF_DEPTH;
        float half_h = ref_depth * tanf(fov * 0.5f);
        mat4 ortho = mat4_orthographic(-half_h, half_h, -half_h, half_h,
                                        near, far);

        vec4 test_pts[] = {
            vec4_create(SEC8_TEST_X, 0.0f,  -5.0f, 1.0f),
            vec4_create(SEC8_TEST_X, 0.0f, -10.0f, 1.0f),
            vec4_create(SEC8_TEST_X, 0.0f, -20.0f, 1.0f),
            vec4_create(SEC8_TEST_X, 0.0f, -50.0f, 1.0f),
        };
        int np = sizeof(test_pts) / sizeof(test_pts[0]);

        SDL_Log("  Same x=2.0 at increasing depth:");
        SDL_Log("    depth |  persp NDC.x  |  ortho NDC.x  |  difference");
        SDL_Log("    ------|---------------|---------------|------------");

        for (int i = 0; i < np; i++) {
            vec4 clip_p = mat4_multiply_vec4(persp, test_pts[i]);
            vec3 ndc_p = vec3_perspective_divide(clip_p);

            vec4 clip_o = mat4_multiply_vec4(ortho, test_pts[i]);
            vec3 ndc_o = vec3_perspective_divide(clip_o);

            SDL_Log("    %5.0f |    %8.4f   |    %8.4f   |    %8.4f",
                    -test_pts[i].z, ndc_p.x, ndc_o.x, ndc_p.x - ndc_o.x);
        }

        printf("\n  Perspective: NDC.x shrinks with depth (objects get smaller)\n");
        printf("  Orthographic: NDC.x stays constant (no foreshortening)\n\n");
    }

    /* ── Section 9: Summary ──────────────────────────────────────────────── */
    printf("── 9. Summary ─────────────────────────────────────────────\n\n");
    printf("  Projection maps 3D to 2D:\n");
    printf("    * The core idea is similar triangles: x' = x * near / (-z)\n");
    printf("    * mat4_perspective wraps this + FOV + depth mapping\n");
    printf("    * The GPU divides by w (= -z) to get NDC\n\n");

    printf("  Two projection types:\n");
    printf("    * Perspective — distant objects shrink (realistic 3D)\n");
    printf("    * Orthographic — no size change with depth (2D, shadow maps)\n\n");

    printf("  Perspective-correct interpolation:\n");
    printf("    * Naive screen-space lerp is wrong in perspective\n");
    printf("    * Interpolate attr/w and 1/w, then divide → correct\n");
    printf("    * The GPU does this automatically\n\n");

    printf("  New math library functions:\n");
    printf("    * vec3_perspective_divide(clip) — explicit w-divide\n");
    printf("    * mat4_perspective_from_planes  — asymmetric frustum\n");
    printf("    (mat4_perspective is the symmetric special case)\n\n");

    printf("  See: lessons/math/06-projections/README.md\n");
    printf("  See: lessons/math/05-matrices/ (matrix fundamentals)\n");
    printf("  See: lessons/math/02-coordinate-spaces/ (the full transform pipeline)\n");
    printf("  See: lessons/gpu/06-depth-and-3d/ (using projections in practice)\n\n");

    SDL_Quit();
    return 0;
}
