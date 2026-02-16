/*
 * Math Lesson 09 — View Matrix & Virtual Camera
 *
 * Building a view matrix from scratch and understanding the camera as
 * an inverse transform. This is how every 3D engine positions and
 * orients its camera.
 *
 * Sections:
 *   1. The camera as an inverse transform
 *   2. Extracting forward / right / up from a quaternion
 *   3. Building a view matrix from position + quaternion
 *   4. Look-at as a special case
 *   5. View matrix in the MVP pipeline
 *   6. Equivalence: look-at vs quaternion-based view
 *   7. Camera movement demo
 *   8. Summary
 *
 * New math library additions in this lesson:
 *   quat_forward, quat_right, quat_up, mat4_view_from_quat
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <SDL3/SDL_main.h>
#include "math/forge_math.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Section 2: Basis extraction */
#define SEC2_YAW_DEG     45.0f
#define SEC2_PITCH_DEG   30.0f

/* Section 3: View matrix from quaternion */
#define SEC3_CAM_X        3.0f
#define SEC3_CAM_Y        2.0f
#define SEC3_CAM_Z        5.0f
#define SEC3_YAW_DEG     30.0f
#define SEC3_PITCH_DEG  -15.0f

/* Section 4: Look-at */
#define SEC4_EYE_X        0.0f
#define SEC4_EYE_Y        2.0f
#define SEC4_EYE_Z        5.0f
#define SEC4_TARGET_X     0.0f
#define SEC4_TARGET_Y     0.0f
#define SEC4_TARGET_Z     0.0f

/* Section 5: MVP pipeline */
#define SEC5_FOV_DEG     60.0f
#define SEC5_ASPECT       1.5f
#define SEC5_NEAR         0.1f
#define SEC5_FAR        100.0f

/* Section 7: Camera movement */
#define SEC7_MOVE_SPEED   2.0f
#define SEC7_TURN_DEG    15.0f
#define SEC7_NUM_STEPS    4

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

static int mat4_approx_eq(mat4 a, mat4 b, float eps)
{
    for (int i = 0; i < 16; i++) {
        if (!approx_eq(a.m[i], b.m[i], eps)) return 0;
    }
    return 1;
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

    /* Disable stdout buffering so printf and SDL_Log (which writes to
     * stderr) interleave correctly when output is captured by a pipe. */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=============================================================\n");
    printf("  Math Lesson 09 - View Matrix & Virtual Camera\n");
    printf("  Building view matrices from scratch\n");
    printf("=============================================================\n\n");

    /* ── Section 1: The camera as an inverse transform ────────────────────
     *
     * A camera in 3D has a position and an orientation, just like any
     * other object. Its "world transform" would place it in the scene:
     *
     *   Camera world transform = T(position) * R(orientation)
     *
     * But the VIEW MATRIX is the INVERSE of this. Instead of placing
     * the camera in the world, we move the entire world so the camera
     * ends up at the origin, looking down -Z:
     *
     *   View = (T * R)^-1 = R^-1 * T^-1
     *
     * For rotation matrices, the inverse is the transpose (R^T = R^-1).
     * For unit quaternions, the inverse is the conjugate.
     * For translation, the inverse just negates the position.
     *
     *   Before (world space):          After (view space):
     *
     *   ^ Y                            ^ Y
     *   |                              |
     *   | cam -->                      | (cam at origin, looking -Z)
     *   |    /                         |
     *   +-------> X                    +-------> X
     *  /  objects                     / objects moved relative to cam
     * Z                              Z
     *
     * Everything in the scene is transformed relative to the camera.
     * Objects in front of the camera end up at negative Z values.
     */
    printf("-- 1. The camera as an inverse transform -------------------\n\n");

    {
        /* Place a camera at (0, 0, 5) with no rotation */
        vec3 cam_pos = vec3_create(0.0f, 0.0f, 5.0f);
        quat cam_rot = quat_identity();

        /* The camera's world transform: T(pos) * R(rot) */
        mat4 t = mat4_translate(cam_pos);
        mat4 r = quat_to_mat4(cam_rot);
        mat4 world_transform = mat4_multiply(t, r);

        /* The view matrix is the inverse */
        mat4 view = mat4_view_from_quat(cam_pos, cam_rot);

        /* Verify: world_transform * view should be identity */
        mat4 product = mat4_multiply(world_transform, view);

        print_mat4("Camera world transform", world_transform);
        printf("\n");
        print_mat4("View matrix (inverse)", view);
        printf("\n");

        int is_identity = mat4_approx_eq(product, mat4_identity(), 0.001f);
        SDL_Log("  World * View = Identity? %s",
                is_identity ? "YES -- view is the inverse" : "NO");
        printf("\n");

        /* Transform a world-space point through the view matrix */
        vec3 world_point = vec3_create(1.0f, 2.0f, 3.0f);
        vec4 wp4 = vec4_create(world_point.x, world_point.y, world_point.z, 1.0f);
        vec4 view_point = mat4_multiply_vec4(view, wp4);

        print_vec3("World point", world_point);
        print_vec4("In view space", view_point);
        SDL_Log("  Camera is at Z=5, point is at Z=3");
        SDL_Log("  -> In view space, point is at Z=%.1f (in front of camera)",
                view_point.z);
        printf("\n");
    }

    /* ── Section 2: Extracting forward / right / up from a quaternion ─────
     *
     * A quaternion orientation encodes three directions:
     *
     *   Forward: where the camera looks      (default: 0, 0, -1)
     *   Right:   to the camera's right side   (default: 1, 0, 0)
     *   Up:      above the camera's head      (default: 0, 1, 0)
     *
     * These are the camera's LOCAL basis vectors — the world-space
     * directions that correspond to the camera's X, Y, and -Z axes.
     *
     * We can extract them by rotating the default directions by the
     * quaternion, or more efficiently with direct formulas.
     *
     * These three vectors form an ORTHONORMAL BASIS: they are mutually
     * perpendicular (dot product = 0) and each has unit length.
     * Together they define a coordinate frame — the camera's local
     * frame of reference.
     *
     *        Up (Y)
     *         ^
     *         |
     *         |  Forward (-Z)
     *         | /
     *         |/
     *  -------+--------> Right (X)
     *
     * Why is forward -Z and not +Z? It follows from the right-hand rule.
     * Everyone agrees +X is right and +Y is up (matching screen layout).
     * In a right-handed system, curl your right hand's fingers from +X
     * toward +Y -- your thumb points in the +Z direction, which is OUT
     * of the screen, toward you. So the scene behind the screen is at
     * negative Z, and the camera looks INTO the screen: -Z.
     *
     * A left-handed system (traditional DirectX) flips this: +Z goes
     * into the screen, camera looks down +Z. Neither is better -- it's
     * a convention. We use right-handed to match Vulkan, OpenGL, and
     * math textbooks.
     */
    printf("-- 2. Extracting basis vectors from a quaternion -----------\n\n");

    {
        /* Start with identity — should give default directions */
        quat id = quat_identity();
        vec3 fwd_id   = quat_forward(id);
        vec3 right_id = quat_right(id);
        vec3 up_id    = quat_up(id);

        SDL_Log("  Identity quaternion (no rotation):");
        print_vec3("Forward", fwd_id);
        print_vec3("Right  ", right_id);
        print_vec3("Up     ", up_id);
        printf("\n");

        /* Now with a yaw of 45 degrees and pitch of 30 degrees */
        float yaw   = SEC2_YAW_DEG * FORGE_DEG2RAD;
        float pitch = SEC2_PITCH_DEG * FORGE_DEG2RAD;
        quat oriented = quat_from_euler(yaw, pitch, 0.0f);

        vec3 fwd   = quat_forward(oriented);
        vec3 right = quat_right(oriented);
        vec3 up    = quat_up(oriented);

        SDL_Log("  After yaw=%.0f, pitch=%.0f degrees:", SEC2_YAW_DEG, SEC2_PITCH_DEG);
        print_vec3("Forward", fwd);
        print_vec3("Right  ", right);
        print_vec3("Up     ", up);
        printf("\n");

        /* Verify they form an orthonormal basis */
        float dot_fr = vec3_dot(fwd, right);
        float dot_fu = vec3_dot(fwd, up);
        float dot_ru = vec3_dot(right, up);
        float len_f  = vec3_length(fwd);
        float len_r  = vec3_length(right);
        float len_u  = vec3_length(up);

        SDL_Log("  Orthonormal basis check:");
        SDL_Log("    dot(forward, right) = %.6f (should be ~0)", dot_fr);
        SDL_Log("    dot(forward, up)    = %.6f (should be ~0)", dot_fu);
        SDL_Log("    dot(right, up)      = %.6f (should be ~0)", dot_ru);
        SDL_Log("    |forward| = %.4f, |right| = %.4f, |up| = %.4f (all ~1)",
                len_f, len_r, len_u);

        int orthonormal =
            approx_eq(dot_fr, 0.0f, 0.001f) &&
            approx_eq(dot_fu, 0.0f, 0.001f) &&
            approx_eq(dot_ru, 0.0f, 0.001f) &&
            approx_eq(len_f, 1.0f, 0.001f) &&
            approx_eq(len_r, 1.0f, 0.001f) &&
            approx_eq(len_u, 1.0f, 0.001f);
        SDL_Log("    Orthonormal? %s", orthonormal ? "YES" : "NO");
        printf("\n");

        /* Verify against quat_rotate_vec3 (brute-force) */
        vec3 fwd_brute   = quat_rotate_vec3(oriented, vec3_create(0, 0, -1));
        vec3 right_brute = quat_rotate_vec3(oriented, vec3_create(1, 0, 0));
        vec3 up_brute    = quat_rotate_vec3(oriented, vec3_create(0, 1, 0));

        int fwd_match = approx_eq(fwd.x, fwd_brute.x, 0.0001f) &&
                        approx_eq(fwd.y, fwd_brute.y, 0.0001f) &&
                        approx_eq(fwd.z, fwd_brute.z, 0.0001f);
        int right_match = approx_eq(right.x, right_brute.x, 0.0001f) &&
                          approx_eq(right.y, right_brute.y, 0.0001f) &&
                          approx_eq(right.z, right_brute.z, 0.0001f);
        int up_match = approx_eq(up.x, up_brute.x, 0.0001f) &&
                       approx_eq(up.y, up_brute.y, 0.0001f) &&
                       approx_eq(up.z, up_brute.z, 0.0001f);

        SDL_Log("  Optimized matches quat_rotate_vec3?");
        SDL_Log("    Forward: %s  Right: %s  Up: %s",
                fwd_match ? "YES" : "NO",
                right_match ? "YES" : "NO",
                up_match ? "YES" : "NO");
        printf("\n");
    }

    /* ── Section 3: Building a view matrix from position + quaternion ──────
     *
     * The view matrix has two parts:
     *
     *   1. ROTATION: align the camera's axes with the view-space axes
     *      - Camera's right   -> view X
     *      - Camera's up      -> view Y
     *      - Camera's -forward -> view Z  (negate because camera looks down -Z)
     *
     *      The rotation part is the TRANSPOSE of the camera's rotation
     *      matrix. Since the camera basis vectors are the COLUMNS of the
     *      camera's rotation matrix, they become the ROWS of the view
     *      matrix rotation part.
     *
     *   2. TRANSLATION: move the world so the camera is at the origin
     *      - Dot each basis vector with -position
     *      - This is R^T * (-pos), not just -pos!
     *
     *   View = | right.x    right.y    right.z    -dot(right, pos)   |
     *          | up.x       up.y       up.z       -dot(up, pos)      |
     *          | -fwd.x     -fwd.y     -fwd.z      dot(fwd, pos)     |
     *          |  0          0          0           1                |
     *
     * Note: we store column-major, so right/up/-fwd appear as rows
     * when reading the matrix logically, but are stored transposed.
     */
    printf("-- 3. View matrix from position + quaternion ---------------\n\n");

    {
        vec3 cam_pos = vec3_create(SEC3_CAM_X, SEC3_CAM_Y, SEC3_CAM_Z);
        float yaw   = SEC3_YAW_DEG * FORGE_DEG2RAD;
        float pitch = SEC3_PITCH_DEG * FORGE_DEG2RAD;
        quat cam_rot = quat_from_euler(yaw, pitch, 0.0f);

        SDL_Log("  Camera position: (%.1f, %.1f, %.1f)", SEC3_CAM_X, SEC3_CAM_Y, SEC3_CAM_Z);
        SDL_Log("  Camera orientation: yaw=%.0f, pitch=%.0f degrees",
                SEC3_YAW_DEG, SEC3_PITCH_DEG);
        printf("\n");

        /* Show the basis vectors */
        vec3 fwd   = quat_forward(cam_rot);
        vec3 right = quat_right(cam_rot);
        vec3 up    = quat_up(cam_rot);

        print_vec3("Forward (where camera looks)", fwd);
        print_vec3("Right   (camera's right side)", right);
        print_vec3("Up      (above camera's head)", up);
        printf("\n");

        /* Build the view matrix */
        mat4 view = mat4_view_from_quat(cam_pos, cam_rot);
        print_mat4("View matrix", view);
        printf("\n");

        /* Verify: camera position should map to origin in view space */
        vec4 cam_in_view = mat4_multiply_vec4(view,
            vec4_create(cam_pos.x, cam_pos.y, cam_pos.z, 1.0f));
        print_vec4("Camera pos in view space", cam_in_view);
        int at_origin = approx_eq(cam_in_view.x, 0.0f, 0.001f) &&
                        approx_eq(cam_in_view.y, 0.0f, 0.001f) &&
                        approx_eq(cam_in_view.z, 0.0f, 0.001f);
        SDL_Log("  At origin? %s (camera maps to (0,0,0) in view space)",
                at_origin ? "YES" : "NO");
        printf("\n");

        /* Verify: a point along the forward direction is at negative Z */
        vec3 ahead = vec3_add(cam_pos, vec3_scale(fwd, 3.0f));
        vec4 ahead_view = mat4_multiply_vec4(view,
            vec4_create(ahead.x, ahead.y, ahead.z, 1.0f));
        SDL_Log("  Point 3 units ahead of camera:");
        print_vec3("World space", ahead);
        print_vec4("View space", ahead_view);
        SDL_Log("  View-space Z is negative? %s (objects ahead have Z < 0)",
                ahead_view.z < -0.001f ? "YES" : "NO");
        printf("\n");
    }

    /* ── Section 4: Look-at as a special case ─────────────────────────────
     *
     * mat4_look_at(eye, target, up) builds a view matrix by computing
     * the camera's orientation from two points: where the camera IS
     * and where it's LOOKING AT.
     *
     * This is a special case of the quaternion-based view matrix:
     *   1. Compute forward = normalize(target - eye)
     *   2. Compute right   = normalize(cross(forward, world_up))
     *   3. Compute up'     = cross(right, forward)
     *   4. Build the same rotation + translation matrix
     *
     * Look-at is convenient for:
     *   - Orbit cameras (always look at a target)
     *   - Cutscenes (look from A toward B)
     *   - Initial camera setup
     *
     * But it can't represent roll (tilting the camera sideways) because
     * it always derives "up" from the world up direction. For full
     * freedom, use the quaternion-based approach.
     */
    printf("-- 4. Look-at as a special case ----------------------------\n\n");

    {
        vec3 eye    = vec3_create(SEC4_EYE_X, SEC4_EYE_Y, SEC4_EYE_Z);
        vec3 target = vec3_create(SEC4_TARGET_X, SEC4_TARGET_Y, SEC4_TARGET_Z);
        vec3 world_up = vec3_create(0.0f, 1.0f, 0.0f);

        SDL_Log("  Eye:    (%.1f, %.1f, %.1f)", eye.x, eye.y, eye.z);
        SDL_Log("  Target: (%.1f, %.1f, %.1f)", target.x, target.y, target.z);
        printf("\n");

        mat4 view_lookat = mat4_look_at(eye, target, world_up);
        print_mat4("View matrix (look-at)", view_lookat);
        printf("\n");

        /* Show the implicit basis vectors from the look-at matrix */
        vec3 fwd = vec3_normalize(vec3_sub(target, eye));
        vec3 right = vec3_normalize(vec3_cross(fwd, world_up));
        vec3 up = vec3_cross(right, fwd);

        print_vec3("Implied forward", fwd);
        print_vec3("Implied right  ", right);
        print_vec3("Implied up     ", up);
        printf("\n");

        /* Same test: camera position maps to origin */
        vec4 eye_in_view = mat4_multiply_vec4(view_lookat,
            vec4_create(eye.x, eye.y, eye.z, 1.0f));
        print_vec4("Eye in view space", eye_in_view);
        int at_origin = approx_eq(eye_in_view.x, 0.0f, 0.001f) &&
                        approx_eq(eye_in_view.y, 0.0f, 0.001f) &&
                        approx_eq(eye_in_view.z, 0.0f, 0.001f);
        SDL_Log("  At origin? %s", at_origin ? "YES" : "NO");
        printf("\n");

        /* Target should be at negative Z in view space */
        vec4 target_in_view = mat4_multiply_vec4(view_lookat,
            vec4_create(target.x, target.y, target.z, 1.0f));
        print_vec4("Target in view space", target_in_view);
        SDL_Log("  Target at -Z? %s (on the camera's -Z axis)",
                (approx_eq(target_in_view.x, 0.0f, 0.01f) &&
                 target_in_view.z < -0.001f) ? "YES" : "NO");
        printf("\n");
    }

    /* ── Section 5: View matrix in the MVP pipeline ───────────────────────
     *
     * The view matrix is the "V" in MVP (Model-View-Projection):
     *
     *   Model space  --(Model)-->  World space
     *   World space  --(View)--->  View space    <-- THIS lesson
     *   View space   --(Proj)--->  Clip space
     *   Clip space   --(/w)----->  NDC
     *
     * On the GPU, the combined MVP matrix is typically computed as:
     *   MVP = Projection * View * Model
     *
     * The vertex shader multiplies each vertex by MVP:
     *   gl_Position = MVP * vec4(position, 1.0);
     *
     * The view matrix transforms EVERYTHING in the scene — every model's
     * world-space vertices pass through the same view matrix. It only
     * changes when the camera moves or rotates.
     */
    printf("-- 5. View matrix in the MVP pipeline ----------------------\n\n");

    {
        /* Set up a complete MVP pipeline */
        vec3 cam_pos = vec3_create(0.0f, 2.0f, 5.0f);
        quat cam_rot = quat_from_euler(0.0f, -20.0f * FORGE_DEG2RAD, 0.0f);

        mat4 model = mat4_translate(vec3_create(0.0f, 0.0f, 0.0f));
        mat4 view  = mat4_view_from_quat(cam_pos, cam_rot);
        mat4 proj  = mat4_perspective(
            SEC5_FOV_DEG * FORGE_DEG2RAD, SEC5_ASPECT, SEC5_NEAR, SEC5_FAR);

        mat4 mvp = mat4_multiply(proj, mat4_multiply(view, model));

        SDL_Log("  Pipeline: Model -> View -> Projection -> NDC");
        printf("\n");

        /* Transform a world-space vertex through the full pipeline */
        vec3 world_vertex = vec3_create(1.0f, 0.0f, 0.0f);
        vec4 v4 = vec4_create(world_vertex.x, world_vertex.y, world_vertex.z, 1.0f);

        vec4 in_view = mat4_multiply_vec4(view, v4);
        vec4 in_clip = mat4_multiply_vec4(proj, in_view);
        vec3 in_ndc  = vec3_perspective_divide(in_clip);

        print_vec3("World vertex     ", world_vertex);
        print_vec4("After View       ", in_view);
        print_vec4("After Projection ", in_clip);
        SDL_Log("  After /w (NDC)  = (%.4f, %.4f, %.4f)", in_ndc.x, in_ndc.y, in_ndc.z);
        printf("\n");

        /* Verify combined MVP gives same result */
        vec4 from_mvp = mat4_multiply_vec4(mvp, v4);
        vec3 ndc_mvp = vec3_perspective_divide(from_mvp);
        int match = approx_eq(in_ndc.x, ndc_mvp.x, 0.001f) &&
                    approx_eq(in_ndc.y, ndc_mvp.y, 0.001f) &&
                    approx_eq(in_ndc.z, ndc_mvp.z, 0.001f);
        SDL_Log("  Combined MVP gives same NDC? %s", match ? "YES" : "NO");

        SDL_Log("  NDC range: X,Y in [-1,1], Z in [0,1]");
        int in_range = (in_ndc.x >= -1.0f && in_ndc.x <= 1.0f) &&
                       (in_ndc.y >= -1.0f && in_ndc.y <= 1.0f) &&
                       (in_ndc.z >= 0.0f && in_ndc.z <= 1.0f);
        SDL_Log("  Vertex in valid NDC range? %s (visible on screen)",
                in_range ? "YES" : "NO");
        printf("\n");
    }

    /* ── Section 6: Equivalence — look-at vs quaternion-based view ────────
     *
     * mat4_look_at and mat4_view_from_quat produce the same view matrix
     * when given equivalent inputs. We can demonstrate this by:
     *
     *   1. Use look-at with eye and target
     *   2. Compute the forward direction: normalize(target - eye)
     *   3. Derive the equivalent quaternion from that direction
     *   4. Build the quaternion-based view matrix
     *   5. Compare — they should match (within floating-point tolerance)
     */
    printf("-- 6. Equivalence: look-at vs quaternion -------------------\n\n");

    {
        vec3 eye    = vec3_create(3.0f, 4.0f, 10.0f);
        vec3 target = vec3_create(0.0f, 0.0f, 0.0f);
        vec3 world_up = vec3_create(0.0f, 1.0f, 0.0f);

        /* Method 1: look-at */
        mat4 view_lookat = mat4_look_at(eye, target, world_up);

        /* Method 2: derive quaternion from the same direction */
        vec3 fwd = vec3_normalize(vec3_sub(target, eye));
        vec3 right = vec3_normalize(vec3_cross(fwd, world_up));
        vec3 up = vec3_cross(right, fwd);

        /* Build a rotation matrix from basis vectors, then extract quaternion.
         * The camera rotation matrix maps local axes to world directions.
         * Column 2 is -forward because the camera's local +Z points BEHIND
         * it (the camera looks down -Z in its own space): */
        mat4 cam_rot_mat = {
            right.x,   right.y,   right.z,   0.0f,
            up.x,      up.y,      up.z,      0.0f,
            -fwd.x,    -fwd.y,    -fwd.z,    0.0f,
            0.0f,      0.0f,      0.0f,      1.0f
        };
        quat cam_quat = quat_from_mat4(cam_rot_mat);

        mat4 view_quat = mat4_view_from_quat(eye, cam_quat);

        /* Compare */
        SDL_Log("  Eye: (%.1f, %.1f, %.1f), Target: (%.1f, %.1f, %.1f)",
                eye.x, eye.y, eye.z, target.x, target.y, target.z);
        printf("\n");

        /* Transform a test point through both matrices */
        vec4 test = vec4_create(2.0f, 1.0f, -3.0f, 1.0f);
        vec4 result_lookat = mat4_multiply_vec4(view_lookat, test);
        vec4 result_quat   = mat4_multiply_vec4(view_quat, test);

        print_vec4("Look-at result ", result_lookat);
        print_vec4("Quaternion result", result_quat);

        int matrices_match = mat4_approx_eq(view_lookat, view_quat, 0.001f);
        SDL_Log("  Matrices match? %s",
                matrices_match ? "YES -- both methods produce the same view"
                               : "NO");
        printf("\n");
    }

    /* ── Section 7: Camera movement demo ──────────────────────────────────
     *
     * In a game, the camera moves every frame. The view matrix is rebuilt
     * each frame from the updated position and orientation.
     *
     * For a first-person camera:
     *   - Mouse/stick yaw/pitch updates the quaternion orientation
     *   - WASD moves the camera along its local forward/right directions
     *   - The view matrix is rebuilt from the new position + quaternion
     *
     * Key pattern:
     *   forward = quat_forward(orientation)
     *   right   = quat_right(orientation)
     *   position += forward * speed * dt   (W/S keys)
     *   position += right * speed * dt     (A/D keys)
     *   view = mat4_view_from_quat(position, orientation)
     */
    printf("-- 7. Camera movement demo ---------------------------------\n\n");

    {
        /* Simulate a camera walking forward then turning right */
        vec3 pos = vec3_create(0.0f, 1.6f, 0.0f);
        float yaw = 0.0f;
        float pitch = 0.0f;
        float dt = 1.0f / 60.0f;  /* simulated 60 FPS */

        SDL_Log("  Simulating camera movement (4 steps):");
        SDL_Log("  Step | Action        | Position                  | Yaw");
        SDL_Log("  -----|---------------|---------------------------|-----");

        for (int step = 0; step < SEC7_NUM_STEPS; step++) {
            quat orientation = quat_from_euler(yaw, pitch, 0.0f);
            vec3 fwd   = quat_forward(orientation);
            vec3 right = quat_right(orientation);

            (void)right;  /* not used in this simple demo */

            /* Alternate: walk forward, then turn right */
            if (step % 2 == 0) {
                /* Walk forward — multiply by dt for frame-rate independence */
                pos = vec3_add(pos, vec3_scale(fwd, SEC7_MOVE_SPEED * dt));
                SDL_Log("  %4d | Walk forward  | (%6.2f, %5.2f, %6.2f) | %.0f deg",
                        step + 1, pos.x, pos.y, pos.z, yaw * FORGE_RAD2DEG);
            } else {
                /* Turn right */
                yaw -= SEC7_TURN_DEG * FORGE_DEG2RAD;
                SDL_Log("  %4d | Turn right    | (%6.2f, %5.2f, %6.2f) | %.0f deg",
                        step + 1, pos.x, pos.y, pos.z, yaw * FORGE_RAD2DEG);
            }

            /* Rebuild the view matrix every step */
            quat new_orientation = quat_from_euler(yaw, pitch, 0.0f);
            mat4 view = mat4_view_from_quat(pos, new_orientation);

            /* Verify camera is always at origin in view space */
            vec4 cam_in_view = mat4_multiply_vec4(view,
                vec4_create(pos.x, pos.y, pos.z, 1.0f));
            if (!approx_eq(cam_in_view.x, 0.0f, 0.001f) ||
                !approx_eq(cam_in_view.y, 0.0f, 0.001f) ||
                !approx_eq(cam_in_view.z, 0.0f, 0.001f)) {
                SDL_Log("  ERROR: camera not at origin in view space!");
            }
        }

        printf("\n");
        printf("  The view matrix is rebuilt every frame from:\n");
        printf("    position    (updated by movement input)\n");
        printf("    orientation (updated by mouse/stick input)\n");
        printf("  This is how first-person cameras work in every 3D game.\n\n");
    }

    /* ── Section 8: Summary ──────────────────────────────────────────────── */
    printf("-- 8. Summary ----------------------------------------------\n\n");

    printf("  The view matrix transforms world space into view (camera) space.\n");
    printf("  It is the INVERSE of the camera's world transform.\n\n");

    printf("  Two ways to build a view matrix:\n\n");
    printf("    Method            | Input                  | Best for\n");
    printf("    ------------------|------------------------|-------------------\n");
    printf("    mat4_look_at      | eye + target + up      | Orbit cameras\n");
    printf("    mat4_view_from_quat| position + quaternion | FPS cameras\n\n");

    printf("  Extracting camera basis vectors from a quaternion:\n");
    printf("    forward = quat_forward(q)   -> where the camera looks\n");
    printf("    right   = quat_right(q)     -> camera's right side\n");
    printf("    up      = quat_up(q)        -> above camera's head\n\n");

    printf("  Camera movement pattern (every frame):\n");
    printf("    1. Update orientation from mouse input  (yaw, pitch)\n");
    printf("    2. Extract forward/right from orientation\n");
    printf("    3. Move position along forward/right    (WASD)\n");
    printf("    4. Rebuild: view = mat4_view_from_quat(pos, orientation)\n");
    printf("    5. Upload MVP = proj * view * model to the GPU\n\n");

    printf("  New math library functions:\n");
    printf("    * quat_forward(q)                -> camera's look direction\n");
    printf("    * quat_right(q)                  -> camera's right direction\n");
    printf("    * quat_up(q)                     -> camera's up direction\n");
    printf("    * mat4_view_from_quat(pos, quat) -> view matrix\n\n");

    printf("  See: lessons/math/09-view-matrix/README.md\n");
    printf("  See: lessons/math/02-coordinate-spaces (view space in the pipeline)\n");
    printf("  See: lessons/math/06-projections (projection after view)\n");
    printf("  See: lessons/math/08-orientation (quaternion fundamentals)\n\n");

    SDL_Quit();
    return 0;
}
