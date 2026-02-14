/*
 * Math Lesson 03 - Orthographic Projection
 *
 * Demonstrates orthographic projection and compares it to perspective.
 * Shows how an axis-aligned box maps to NDC without foreshortening.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include "math/forge_math.h"

/* Helper to print a vec4 with a label */
static void print_vec4(const char *label, vec4 v)
{
    printf("  %-28s (%7.3f, %7.3f, %7.3f, %7.3f)\n",
           label, v.x, v.y, v.z, v.w);
}

/* Helper to print NDC coordinates after perspective divide */
static void print_ndc(const char *label, vec4 clip)
{
    float x = clip.x / clip.w;
    float y = clip.y / clip.w;
    float z = clip.z / clip.w;
    printf("  %-28s (%7.3f, %7.3f, %7.3f)\n", label, x, y, z);
}

/* Helper to print a section header */
static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("\n");
    printf("==============================================================\n");
    printf("  Orthographic Projection\n");
    printf("==============================================================\n");
    printf("\n");
    printf("Orthographic projection maps a rectangular box in view space\n");
    printf("to the NDC cube. Unlike perspective, distant objects stay the\n");
    printf("same size -- parallel lines remain parallel.\n");

    /* ── Set up a common view matrix ──────────────────────────────────── */

    vec3 eye    = vec3_create(0.0f, 0.0f, 10.0f);
    vec3 target = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 up     = vec3_create(0.0f, 1.0f, 0.0f);
    mat4 view   = mat4_look_at(eye, target, up);

    /* ── Define test points at different depths ──────────────────────── */
    /* All in world space, then transformed to view space */

    print_header("1. TEST POINTS IN VIEW SPACE");
    printf("  Camera at (0, 0, 10) looking at origin.\n");
    printf("  Three points at the same XY but different depths:\n\n");

    vec4 world_near   = vec4_create(3.0f, 2.0f,  9.0f, 1.0f);
    vec4 world_mid    = vec4_create(3.0f, 2.0f,  0.0f, 1.0f);
    vec4 world_far    = vec4_create(3.0f, 2.0f, -8.0f, 1.0f);

    vec4 view_near = mat4_multiply_vec4(view, world_near);
    vec4 view_mid  = mat4_multiply_vec4(view, world_mid);
    vec4 view_far  = mat4_multiply_vec4(view, world_far);

    print_vec4("Near  (z_world =  9):", view_near);
    print_vec4("Mid   (z_world =  0):", view_mid);
    print_vec4("Far   (z_world = -8):", view_far);

    /* ── Orthographic projection ─────────────────────────────────────── */

    print_header("2. ORTHOGRAPHIC PROJECTION");
    printf("  Box: X=[-5, 5], Y=[-5, 5], near=0.1, far=20\n\n");

    mat4 ortho = mat4_orthographic(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 20.0f);

    vec4 ortho_near = mat4_multiply_vec4(ortho, view_near);
    vec4 ortho_mid  = mat4_multiply_vec4(ortho, view_mid);
    vec4 ortho_far  = mat4_multiply_vec4(ortho, view_far);

    printf("  Clip space (w is always 1 -- no perspective divide!):\n");
    print_vec4("Near clip:", ortho_near);
    print_vec4("Mid  clip:", ortho_mid);
    print_vec4("Far  clip:", ortho_far);

    printf("\n  NDC (same as clip since w=1):\n");
    print_ndc("Near NDC:", ortho_near);
    print_ndc("Mid  NDC:", ortho_mid);
    print_ndc("Far  NDC:", ortho_far);

    printf("\n  Key observation: X and Y are IDENTICAL for all three points.\n");
    printf("  Depth does not affect apparent size. This is the defining\n");
    printf("  property of orthographic projection.\n");

    /* ── Perspective projection for comparison ────────────────────────── */

    print_header("3. PERSPECTIVE PROJECTION (for comparison)");
    printf("  FOV=60 degrees, aspect=1.0, near=0.1, far=20\n\n");

    float fov = 60.0f * FORGE_DEG2RAD;
    mat4 persp = mat4_perspective(fov, 1.0f, 0.1f, 20.0f);

    vec4 persp_near = mat4_multiply_vec4(persp, view_near);
    vec4 persp_mid  = mat4_multiply_vec4(persp, view_mid);
    vec4 persp_far  = mat4_multiply_vec4(persp, view_far);

    printf("  Clip space (note w varies -- this is what causes foreshortening):\n");
    print_vec4("Near clip:", persp_near);
    print_vec4("Mid  clip:", persp_mid);
    print_vec4("Far  clip:", persp_far);

    printf("\n  NDC (after dividing by w):\n");
    print_ndc("Near NDC:", persp_near);
    print_ndc("Mid  NDC:", persp_mid);
    print_ndc("Far  NDC:", persp_far);

    printf("\n  Key observation: X and Y CHANGE with depth. Farther points\n");
    printf("  appear closer to the center. This is perspective foreshortening.\n");

    /* ── Side-by-side comparison ──────────────────────────────────────── */

    print_header("4. SIDE-BY-SIDE COMPARISON");
    printf("  Same three points, X coordinate in NDC:\n\n");

    printf("  %-12s %-18s %-18s\n", "Point", "Orthographic X", "Perspective X");
    printf("  %-12s %-18s %-18s\n", "-----", "--------------", "-------------");
    printf("  %-12s %-18.3f %-18.3f\n", "Near (z= 9)",
           ortho_near.x / ortho_near.w, persp_near.x / persp_near.w);
    printf("  %-12s %-18.3f %-18.3f\n", "Mid  (z= 0)",
           ortho_mid.x / ortho_mid.w, persp_mid.x / persp_mid.w);
    printf("  %-12s %-18.3f %-18.3f\n", "Far  (z=-8)",
           ortho_far.x / ortho_far.w, persp_far.x / persp_far.w);

    printf("\n  Orthographic: same X at every depth (no foreshortening)\n");
    printf("  Perspective:  X shrinks with distance (foreshortening)\n");

    /* ── 2D rendering example ─────────────────────────────────────────── */

    print_header("5. COMMON USE CASE: 2D RENDERING");
    printf("  Orthographic projection for a 1920x1080 screen.\n");
    printf("  Maps pixel coordinates directly to NDC.\n\n");

    mat4 ortho_2d = mat4_orthographic(0.0f, 1920.0f, 0.0f, 1080.0f, -1.0f, 1.0f);

    vec4 corner_bl = mat4_multiply_vec4(ortho_2d, vec4_create(0.0f, 0.0f, 0.0f, 1.0f));
    vec4 corner_tr = mat4_multiply_vec4(ortho_2d, vec4_create(1920.0f, 1080.0f, 0.0f, 1.0f));
    vec4 center    = mat4_multiply_vec4(ortho_2d, vec4_create(960.0f, 540.0f, 0.0f, 1.0f));

    printf("  Pixel -> NDC:\n");
    printf("    (0, 0)         -> (%.1f, %.1f)    bottom-left\n",
           corner_bl.x, corner_bl.y);
    printf("    (1920, 1080)   -> (%.1f, %.1f)     top-right\n",
           corner_tr.x, corner_tr.y);
    printf("    (960, 540)     -> (%.1f, %.1f)     center\n",
           center.x, center.y);

    printf("\n  This is how 2D games, UIs, and text rendering work:\n");
    printf("  specify positions in pixels, let the orthographic matrix\n");
    printf("  handle the mapping to GPU coordinates.\n");

    /* ── Summary ─────────────────────────────────────────────────────── */

    printf("\n");
    printf("==============================================================\n");
    printf("  Summary\n");
    printf("==============================================================\n");
    printf("\n");
    printf("  Orthographic projection:\n");
    printf("    * No foreshortening -- size is independent of depth\n");
    printf("    * w stays 1 (no perspective divide needed)\n");
    printf("    * Parallel lines in the scene remain parallel on screen\n");
    printf("    * Maps an axis-aligned box to NDC\n");
    printf("\n");
    printf("  Use orthographic for:\n");
    printf("    * 2D games and UI rendering\n");
    printf("    * Shadow map generation\n");
    printf("    * CAD and architectural visualization\n");
    printf("    * Isometric/top-down views\n");
    printf("\n");
    printf("  Use perspective for:\n");
    printf("    * 3D scenes with realistic depth perception\n");
    printf("    * First-person / third-person cameras\n");
    printf("\n");
    printf("  See lessons/math/03-orthographic-projection/README.md\n");
    printf("\n");

    SDL_Quit();
    return 0;
}
