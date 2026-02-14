/*
 * Math Lesson 02 - Coordinate Spaces
 *
 * Demonstrates the transformation pipeline from model space to screen space.
 * Shows how a single point transforms through each coordinate space.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include "math/forge_math.h"

/* Helper to print a vec4 with a label */
static void print_vec4(const char *label, vec4 v)
{
    printf("  %-20s (%.2f, %.2f, %.2f, %.2f)\n", label, v.x, v.y, v.z, v.w);
}

/* Helper to print a coordinate space section */
static void print_space_header(const char *name, const char *description)
{
    printf("\n%s\n", name);
    printf("%s\n", description);
    printf("---------------------------------------------------------\n");
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
    printf("====================================================================\n");
    printf("  Coordinate Spaces - The Transformation Pipeline\n");
    printf("====================================================================\n");
    printf("\n");
    printf("Watch how a single point transforms through each coordinate space:\n");
    printf("  Model -> World -> View -> Clip -> NDC -> Screen\n");
    printf("\n");

    /* ── Define our point in local/model space ──────────────────────── */

    print_space_header(
        "1. LOCAL / MODEL SPACE",
        "The object's own coordinate system. Origin is at the object's center."
    );

    vec4 local_point = vec4_create(1.0f, 0.5f, 0.0f, 1.0f);
    print_vec4("Local point:", local_point);

    printf("\n");
    printf("  This is where you define your mesh. A cube's vertices are\n");
    printf("  centered at (0,0,0), with coordinates like (+/-1, +/-1, +/-1).\n");

    /* ── Transform to world space ───────────────────────────────────── */

    print_space_header(
        "2. WORLD SPACE",
        "The scene's coordinate system. Multiple objects positioned relative to each other."
    );

    /* Let's say our object is at position (5, 2, 0), rotated 45° around Z */
    vec3 world_position = vec3_create(5.0f, 2.0f, 0.0f);
    float rotation_angle = 45.0f * FORGE_DEG2RAD;

    mat4 model_matrix = mat4_multiply(
        mat4_translate(world_position),
        mat4_rotate_z(rotation_angle)
    );

    vec4 world_point = mat4_multiply_vec4(model_matrix, local_point);
    print_vec4("World point:", world_point);

    printf("\n");
    printf("  Applied model matrix (translate + rotate):\n");
    printf("    Position: (%.1f, %.1f, %.1f)\n",
           world_position.x, world_position.y, world_position.z);
    printf("    Rotation: %.0f degrees around Z\n", rotation_angle * FORGE_RAD2DEG);

    /* ── Transform to view/camera space ─────────────────────────────── */

    print_space_header(
        "3. VIEW / CAMERA SPACE",
        "Coordinates relative to the camera. Camera is at origin looking down -Z."
    );

    /* Camera at (3, 3, 10), looking at origin */
    vec3 camera_pos = vec3_create(3.0f, 3.0f, 10.0f);
    vec3 look_at = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 up = vec3_create(0.0f, 1.0f, 0.0f);

    mat4 view_matrix = mat4_look_at(camera_pos, look_at, up);
    vec4 view_point = mat4_multiply_vec4(view_matrix, world_point);

    print_vec4("View point:", view_point);

    printf("\n");
    printf("  Camera position: (%.1f, %.1f, %.1f)\n",
           camera_pos.x, camera_pos.y, camera_pos.z);
    printf("  Looking at: (%.1f, %.1f, %.1f)\n",
           look_at.x, look_at.y, look_at.z);
    printf("  In view space, +X is right, +Y is up, -Z is forward (into screen)\n");

    /* ── Transform to clip space ────────────────────────────────────── */

    print_space_header(
        "4. CLIP SPACE",
        "After projection. Perspective makes distant objects smaller."
    );

    /* Perspective projection: 60° FOV, 16:9 aspect, near=0.1, far=100 */
    float fov = 60.0f * FORGE_DEG2RAD;
    float aspect = 16.0f / 9.0f;
    float near = 0.1f;
    float far = 100.0f;

    mat4 projection_matrix = mat4_perspective(fov, aspect, near, far);
    vec4 clip_point = mat4_multiply_vec4(projection_matrix, view_point);

    print_vec4("Clip point:", clip_point);

    printf("\n");
    printf("  Projection: FOV=%.0f degrees, Aspect=%.2f, Near=%.1f, Far=%.1f\n",
           fov * FORGE_RAD2DEG, aspect, near, far);
    printf("  Note the w component! It's used for perspective division.\n");

    /* ── Normalize to NDC ───────────────────────────────────────────── */

    print_space_header(
        "5. NDC (Normalized Device Coordinates)",
        "After perspective division (x/w, y/w, z/w). Visible range is [-1, 1]."
    );

    vec4 ndc_point = vec4_create(
        clip_point.x / clip_point.w,
        clip_point.y / clip_point.w,
        clip_point.z / clip_point.w,
        1.0f
    );

    print_vec4("NDC point:", ndc_point);

    printf("\n");
    printf("  NDC is the GPU's canonical view volume:\n");
    printf("    X in [-1, 1]: left to right\n");
    printf("    Y in [-1, 1]: bottom to top\n");
    printf("    Z in [0, 1]: near to far (Vulkan/Metal) or [-1,1] (OpenGL)\n");
    printf("\n");

    if (ndc_point.x < -1.0f || ndc_point.x > 1.0f ||
        ndc_point.y < -1.0f || ndc_point.y > 1.0f ||
        ndc_point.z < 0.0f || ndc_point.z > 1.0f) {
        printf("  [!] Point is OUTSIDE the visible range - would be clipped!\n");
    } else {
        printf("  [OK] Point is INSIDE the visible range - would be rendered!\n");
    }

    /* ── Transform to screen space ──────────────────────────────────── */

    print_space_header(
        "6. SCREEN SPACE",
        "Final pixel coordinates. Origin at top-left (or bottom-left, API-dependent)."
    );

    int screen_width = 1920;
    int screen_height = 1080;

    /* NDC [-1,1] -> Screen [0, width/height] */
    float screen_x = (ndc_point.x + 1.0f) * 0.5f * screen_width;
    float screen_y = (1.0f - ndc_point.y) * 0.5f * screen_height;  /* Flip Y */

    printf("  Screen pixel:        (%.1f, %.1f)\n", screen_x, screen_y);
    printf("\n");
    printf("  Screen resolution: %dx%d\n", screen_width, screen_height);
    printf("  (Y-axis flipped: NDC +Y is up, but screen +Y is down)\n");

    /* ── Summary ─────────────────────────────────────────────────────── */

    printf("\n");
    printf("====================================================================\n");
    printf("  Summary: The Complete Pipeline\n");
    printf("====================================================================\n");
    printf("\n");
    printf("  Local (%.2f, %.2f)  ->  (object coordinates)\n",
           local_point.x, local_point.y);
    printf("    | Model matrix (translate + rotate)\n");
    printf("  World (%.2f, %.2f)  ->  (scene coordinates)\n",
           world_point.x, world_point.y);
    printf("    | View matrix (camera transform)\n");
    printf("  View  (%.2f, %.2f)  ->  (relative to camera)\n",
           view_point.x, view_point.y);
    printf("    | Projection matrix (perspective)\n");
    printf("  Clip  (%.2f, %.2f, w=%.2f)  ->  (homogeneous coordinates)\n",
           clip_point.x, clip_point.y, clip_point.w);
    printf("    | Perspective divide (x/w, y/w, z/w)\n");
    printf("  NDC   (%.2f, %.2f)  ->  (normalized [-1,1])\n",
           ndc_point.x, ndc_point.y);
    printf("    | Viewport transform\n");
    printf("  Screen (%.1f, %.1f px)  ->  (final pixel position)\n",
           screen_x, screen_y);
    printf("\n");
    printf("  Each space has a purpose. Understanding them helps you:\n");
    printf("    * Position objects (model -> world)\n");
    printf("    * Move the camera (world -> view)\n");
    printf("    * Create perspective (view -> clip)\n");
    printf("    * Render to pixels (NDC -> screen)\n");
    printf("\n");
    printf("  See lessons/math/02-coordinate-spaces/README.md for details.\n");
    printf("\n");

    SDL_Quit();
    return 0;
}
