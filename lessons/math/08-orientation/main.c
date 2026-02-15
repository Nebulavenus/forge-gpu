/*
 * Math Lesson 08 — Orientation
 *
 * Four representations of 3D rotation and how to convert between them.
 * This is one of the most important topics in game and graphics math.
 *
 * Sections:
 *   1.  Euler angles — pitch, yaw, and roll
 *   2.  Gimbal lock — why Euler angles break at ±90° pitch
 *   3.  Rotation matrices — Rx, Ry, Rz
 *   4.  Rodrigues' rotation — rotating around an arbitrary axis
 *   5.  Axis-angle representation
 *   6.  Quaternion basics — identity, conjugate, inverse
 *   7.  Quaternion multiplication — composing rotations
 *   8.  Rotating a vector with a quaternion
 *   9.  Conversions — Euler, axis-angle, quaternion, and matrix round-trips
 *  10.  SLERP — smooth interpolation between orientations
 *  11.  Summary
 *
 * New math library additions in this lesson:
 *   quat type, quat_create, quat_identity, quat_dot, quat_length,
 *   quat_normalize, quat_conjugate, quat_inverse, quat_negate,
 *   quat_multiply, quat_rotate_vec3, quat_from_axis_angle,
 *   quat_to_axis_angle, quat_from_euler, quat_to_euler,
 *   quat_to_mat4, quat_from_mat4, quat_slerp, quat_nlerp,
 *   vec3_rotate_axis_angle
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <SDL3/SDL_main.h>
#include "math/forge_math.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Section 1: Euler angles */
#define SEC1_YAW_DEG       45.0f
#define SEC1_PITCH_DEG     30.0f
#define SEC1_ROLL_DEG       0.0f

/* Section 2: Gimbal lock */
#define SEC2_PITCH_LOCK    90.0f
#define SEC2_YAW_A         30.0f
#define SEC2_ROLL_A        20.0f
#define SEC2_YAW_B         50.0f

/* Section 3: Rotation matrices */
#define SEC3_ANGLE_DEG     90.0f

/* Section 4: Rodrigues */
#define SEC4_ANGLE_DEG     60.0f

/* Section 5: Axis-angle */
#define SEC5_ANGLE_DEG    120.0f

/* Section 7: Quaternion multiplication */
#define SEC7_YAW_DEG       90.0f
#define SEC7_PITCH_DEG     45.0f

/* Section 8: Rotating a vector */
#define SEC8_ANGLE_DEG     90.0f

/* Section 10: SLERP */
#define SEC10_NUM_STEPS     8
#define SEC10_START_DEG     0.0f
#define SEC10_END_DEG     120.0f

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void print_vec3(const char *label, vec3 v)
{
    SDL_Log("  %s = (%.4f, %.4f, %.4f)", label, v.x, v.y, v.z);
}

static void print_quat(const char *label, quat q)
{
    SDL_Log("  %s = (w=%.4f, x=%.4f, y=%.4f, z=%.4f)", label,
            q.w, q.x, q.y, q.z);
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
    printf("  Math Lesson 08 — Orientation\n");
    printf("  Four representations of 3D rotation\n");
    printf("=============================================================\n\n");

    /* ── Section 1: Euler angles ──────────────────────────────────────────
     *
     * The most intuitive rotation representation: three angles that
     * describe how to orient an object.
     *
     *   Yaw:   rotation around Y axis (look left/right)
     *   Pitch: rotation around X axis (look up/down)
     *   Roll:  rotation around Z axis (tilt head)
     *
     * The ORDER of application matters. We use intrinsic Y-X-Z:
     *   1. Yaw first (rotate around world Y)
     *   2. Pitch second (rotate around the new local X)
     *   3. Roll third (rotate around the new local Z)
     *
     *          +Y (yaw axis)
     *           |
     *           |  +Z (roll axis, toward camera)
     *           | /
     *           |/
     *   --------+--------> +X (pitch axis)
     *
     * This convention is standard for game cameras and aircraft:
     *   Yaw = heading, Pitch = elevation, Roll = bank
     */
    printf("-- 1. Euler angles -- pitch, yaw, and roll ----------------\n\n");

    {
        float yaw_deg   = SEC1_YAW_DEG;
        float pitch_deg = SEC1_PITCH_DEG;
        float roll_deg  = SEC1_ROLL_DEG;
        float yaw   = yaw_deg * FORGE_DEG2RAD;
        float pitch = pitch_deg * FORGE_DEG2RAD;
        float roll  = roll_deg * FORGE_DEG2RAD;

        SDL_Log("  Euler angles: yaw=%.0f, pitch=%.0f, roll=%.0f (degrees)",
                yaw_deg, pitch_deg, roll_deg);
        printf("\n");

        /* Build the rotation using individual axis matrices */
        mat4 r_yaw   = mat4_rotate_y(yaw);
        mat4 r_pitch = mat4_rotate_x(pitch);
        mat4 r_roll  = mat4_rotate_z(roll);

        /* Combined: R = R_y * R_x * R_z (apply roll, then pitch, then yaw) */
        mat4 r_combined = mat4_multiply(r_yaw,
                          mat4_multiply(r_pitch, r_roll));

        /* Apply to the forward direction (0, 0, -1) to see where we look */
        vec4 forward = vec4_create(0.0f, 0.0f, -1.0f, 0.0f);
        vec4 look_dir = mat4_multiply_vec4(r_combined, forward);

        SDL_Log("  Forward (0, 0, -1) after rotation:");
        SDL_Log("    -> (%.4f, %.4f, %.4f)", look_dir.x, look_dir.y, look_dir.z);
        printf("\n");
        printf("  Euler angles are easy to understand but have a fatal flaw...\n\n");
    }

    /* ── Section 2: Gimbal lock ───────────────────────────────────────────
     *
     * When pitch = ±90°, yaw and roll rotate around the same axis.
     * This means you lose one degree of freedom — you can't distinguish
     * yaw from roll. This is called "gimbal lock."
     *
     * Named after physical gimbals (nested rotating rings) on gyroscopes
     * and spacecraft. When two rings align, the device loses the ability
     * to measure rotation in one direction.
     *
     * Outer ring (yaw)
     *    +----[=]----+
     *    |  Middle ring (pitch = 90°)
     *    |    +--[=]--+       <- This ring is now aligned with outer!
     *    |    | Inner (roll)  <- Yaw and roll now do the same thing
     *    |    +--[=]--+
     *    +----[=]----+
     *
     * At pitch = 90°, the rotation matrix degenerates:
     *   R_y(a) * R_x(90°) * R_z(b) only depends on (a - b), not
     *   on a and b separately. Two different Euler triplets produce
     *   the same orientation.
     */
    printf("-- 2. Gimbal lock -- why Euler angles break ---------------\n\n");

    {
        float pitch = SEC2_PITCH_LOCK * FORGE_DEG2RAD;

        /* Two different Euler triplets at pitch = 90° */
        float yaw_a = SEC2_YAW_A * FORGE_DEG2RAD;
        float roll_a = SEC2_ROLL_A * FORGE_DEG2RAD;

        /* Different yaw and roll, but same (yaw - roll) */
        float yaw_b = SEC2_YAW_B * FORGE_DEG2RAD;
        float roll_b = yaw_b - (yaw_a - roll_a);  /* same difference */

        quat q_a = quat_from_euler(yaw_a, pitch, roll_a);
        quat q_b = quat_from_euler(yaw_b, pitch, roll_b);

        /* These should produce the same rotation */
        vec3 forward = vec3_create(0.0f, 0.0f, -1.0f);
        vec3 dir_a = quat_rotate_vec3(q_a, forward);
        vec3 dir_b = quat_rotate_vec3(q_b, forward);

        SDL_Log("  At pitch = 90 degrees (gimbal lock):");
        SDL_Log("    Euler A: yaw=%.0f, pitch=90, roll=%.0f",
                SEC2_YAW_A, SEC2_ROLL_A);
        SDL_Log("    Euler B: yaw=%.0f, pitch=90, roll=%.1f",
                SEC2_YAW_B, roll_b * FORGE_RAD2DEG);
        SDL_Log("    Both have yaw-roll = %.0f degrees", SEC2_YAW_A - SEC2_ROLL_A);
        printf("\n");
        print_vec3("Direction A", dir_a);
        print_vec3("Direction B", dir_b);

        int same = approx_eq(dir_a.x, dir_b.x, 0.001f) &&
                   approx_eq(dir_a.y, dir_b.y, 0.001f) &&
                   approx_eq(dir_a.z, dir_b.z, 0.001f);
        SDL_Log("  Same direction? %s", same ? "YES — gimbal lock confirmed" : "NO");
        printf("\n  Lesson: Euler angles lose a degree of freedom at pitch = +/-90.\n");
        printf("  This is why quaternions are preferred for runtime orientation.\n\n");
    }

    /* ── Section 3: Rotation matrices ─────────────────────────────────────
     *
     * Each basis-axis rotation matrix rotates in the plane perpendicular
     * to that axis. The columns of the matrix show where the basis vectors
     * end up after rotation.
     *
     * Rx(θ): rotates in YZ plane (Y toward Z)
     *   | 1    0      0   |
     *   | 0   cos θ  -sin θ |
     *   | 0   sin θ   cos θ |
     *
     * Ry(θ): rotates in XZ plane (Z toward X)
     *   |  cos θ  0  sin θ |
     *   |   0     1   0    |
     *   | -sin θ  0  cos θ |
     *
     * Rz(θ): rotates in XY plane (X toward Y)
     *   | cos θ  -sin θ  0 |
     *   | sin θ   cos θ  0 |
     *   |  0       0     1 |
     *
     * Positive angles rotate counter-clockwise when looking down the
     * positive axis toward the origin (right-hand rule).
     */
    printf("-- 3. Rotation matrices -- Rx, Ry, Rz --------------------\n\n");

    {
        float angle = SEC3_ANGLE_DEG * FORGE_DEG2RAD;

        SDL_Log("  90-degree rotations of the X axis (1, 0, 0):\n");

        vec4 x_axis = vec4_create(1.0f, 0.0f, 0.0f, 0.0f);

        /* Rotate (1,0,0) around each axis */
        vec4 rx_result = mat4_multiply_vec4(mat4_rotate_x(angle), x_axis);
        vec4 ry_result = mat4_multiply_vec4(mat4_rotate_y(angle), x_axis);
        vec4 rz_result = mat4_multiply_vec4(mat4_rotate_z(angle), x_axis);

        SDL_Log("    Rx(90) * (1,0,0) = (%.1f, %.1f, %.1f) — X stays (rotation is around X)",
                rx_result.x, rx_result.y, rx_result.z);
        SDL_Log("    Ry(90) * (1,0,0) = (%.1f, %.1f, %.1f) — X goes to -Z",
                ry_result.x, ry_result.y, ry_result.z);
        SDL_Log("    Rz(90) * (1,0,0) = (%.1f, %.1f, %.1f) — X goes to +Y",
                rz_result.x, rz_result.y, rz_result.z);
        printf("\n");

        /* Show that rotation matrices are orthonormal */
        mat4 ry = mat4_rotate_y(angle);
        float det = mat4_determinant(ry);
        mat4 ry_t = mat4_transpose(ry);
        mat4 product = mat4_multiply(ry, ry_t);
        int is_identity = approx_eq(product.m[0], 1.0f, 0.001f) &&
                          approx_eq(product.m[5], 1.0f, 0.001f) &&
                          approx_eq(product.m[10], 1.0f, 0.001f);

        SDL_Log("  Rotation matrix properties:");
        SDL_Log("    det(Ry) = %.4f (should be 1 — volume preserved)", det);
        SDL_Log("    Ry * Ry^T = I? %s (transpose = inverse for rotations)",
                is_identity ? "YES" : "NO");
        printf("\n");
    }

    /* ── Section 4: Rodrigues' rotation formula ───────────────────────────
     *
     * Rodrigues' formula rotates a vector around ANY axis, not just X/Y/Z.
     * It works by decomposing the vector into components parallel and
     * perpendicular to the axis:
     *
     *   v' = v*cos(θ) + (k x v)*sin(θ) + k*(k.v)*(1 - cos(θ))
     *
     * where k is the unit rotation axis and θ is the angle.
     *
     *   v_parallel = k * (k . v)          stays fixed
     *   v_perp = v - v_parallel            rotates in the plane
     *   v_perp_rotated = v_perp*cos(θ) + (k x v)*sin(θ)
     *   v' = v_parallel + v_perp_rotated
     */
    printf("-- 4. Rodrigues' rotation formula -------------------------\n\n");

    {
        /* Rotate (1, 0, 0) by 60° around the diagonal axis (1, 1, 1) */
        vec3 v = vec3_create(1.0f, 0.0f, 0.0f);
        vec3 axis = vec3_normalize(vec3_create(1.0f, 1.0f, 1.0f));
        float angle = SEC4_ANGLE_DEG * FORGE_DEG2RAD;

        vec3 rotated = vec3_rotate_axis_angle(v, axis, angle);

        print_vec3("Original vector", v);
        print_vec3("Rotation axis (normalized)", axis);
        SDL_Log("  Rotation angle: %.0f degrees", SEC4_ANGLE_DEG);
        print_vec3("Result", rotated);
        printf("\n");

        /* Verify: rotating 3 times by 120° around (1,1,1) is a cycle:
         * it maps X→Y→Z→X (a 3-fold symmetry of the cube) */
        vec3 v1 = vec3_rotate_axis_angle(v, axis, 120.0f * FORGE_DEG2RAD);
        vec3 v2 = vec3_rotate_axis_angle(v1, axis, 120.0f * FORGE_DEG2RAD);
        vec3 v3 = vec3_rotate_axis_angle(v2, axis, 120.0f * FORGE_DEG2RAD);

        SDL_Log("  Three 120-degree rotations around (1,1,1) form a cycle:");
        SDL_Log("    (1,0,0) -> (%.1f,%.1f,%.1f) -> (%.1f,%.1f,%.1f) -> (%.1f,%.1f,%.1f)",
                v1.x, v1.y, v1.z,  v2.x, v2.y, v2.z,  v3.x, v3.y, v3.z);
        int cycled = approx_eq(v3.x, 1.0f, 0.001f) &&
                     approx_eq(v3.y, 0.0f, 0.001f) &&
                     approx_eq(v3.z, 0.0f, 0.001f);
        SDL_Log("    Back to start? %s", cycled ? "YES" : "NO");
        printf("\n");
    }

    /* ── Section 5: Axis-angle representation ─────────────────────────────
     *
     * Axis-angle stores a rotation as:
     *   - A unit vector (the axis to rotate around)
     *   - A scalar (the angle to rotate by)
     *
     * This is the most natural "user-facing" representation:
     *   "Rotate 45° around the Y axis"
     *
     * Advantages:
     *   - Easy to understand and specify
     *   - Only 4 values (axis xyz + angle)
     *   - No gimbal lock
     *
     * Disadvantages:
     *   - Hard to compose (combining two rotations is complex)
     *   - Hard to interpolate smoothly
     *   - Not directly useful for transforms (need conversion)
     *
     * In practice, axis-angle is an input/interface format: humans
     * or game logic specify rotations this way, then convert to
     * quaternions for storage and computation.
     */
    printf("-- 5. Axis-angle representation ---------------------------\n\n");

    {
        /* Create a rotation from axis-angle */
        vec3 axis = vec3_create(0.0f, 1.0f, 0.0f);  /* Y axis */
        float angle = SEC5_ANGLE_DEG * FORGE_DEG2RAD;

        quat q = quat_from_axis_angle(axis, angle);

        SDL_Log("  Axis-angle: axis=(0, 1, 0), angle=%.0f degrees",
                SEC5_ANGLE_DEG);
        print_quat("Quaternion", q);
        printf("\n");

        /* Round-trip: quaternion back to axis-angle */
        vec3 recovered_axis;
        float recovered_angle;
        quat_to_axis_angle(q, &recovered_axis, &recovered_angle);

        SDL_Log("  Round-trip back to axis-angle:");
        print_vec3("Recovered axis", recovered_axis);
        SDL_Log("  Recovered angle: %.1f degrees",
                recovered_angle * FORGE_RAD2DEG);

        int axis_match = approx_eq(recovered_axis.x, axis.x, 0.001f) &&
                         approx_eq(recovered_axis.y, axis.y, 0.001f) &&
                         approx_eq(recovered_axis.z, axis.z, 0.001f);
        int angle_match = approx_eq(recovered_angle, angle, 0.001f);
        SDL_Log("  Match? %s",
                (axis_match && angle_match) ? "YES — round-trip preserved" : "NO");
        printf("\n");

        /* Show the half-angle relationship */
        SDL_Log("  Why half-angle?");
        SDL_Log("    angle = %.0f degrees -> half-angle = %.0f degrees",
                SEC5_ANGLE_DEG, SEC5_ANGLE_DEG * 0.5f);
        SDL_Log("    cos(half) = %.4f -> this is q.w (%.4f)",
                cosf(angle * 0.5f), q.w);
        SDL_Log("    sin(half) = %.4f -> this scales the axis",
                sinf(angle * 0.5f));
        printf("\n");
    }

    /* ── Section 6: Quaternion basics ─────────────────────────────────────
     *
     * A quaternion q = w + xi + yj + zk, where i, j, k are imaginary
     * units that satisfy:
     *
     *   i*i = j*j = k*k = i*j*k = -1
     *   i*j = k    j*k = i    k*i = j    (cyclic, like cross product)
     *   j*i = -k   k*j = -i   i*k = -j   (anti-commutative)
     *
     * For rotations, we use UNIT quaternions (|q| = 1).
     *
     * Key properties:
     *   Identity:    (1, 0, 0, 0)  — no rotation
     *   Conjugate:   q* = (w, -x, -y, -z)  — reverse rotation
     *   Inverse:     q^-1 = q* / |q|^2  (= q* for unit quaternions)
     *   Double cover: q and -q represent the SAME rotation
     */
    printf("-- 6. Quaternion basics -----------------------------------\n\n");

    {
        quat id = quat_identity();
        print_quat("Identity", id);
        SDL_Log("  Length: %.4f (should be 1.0)", quat_length(id));
        printf("\n");

        /* Create a 90° rotation around Y */
        quat q = quat_from_axis_angle(
            vec3_create(0.0f, 1.0f, 0.0f), FORGE_PI * 0.5f);
        quat q_conj = quat_conjugate(q);
        quat q_inv = quat_inverse(q);

        print_quat("q (90 deg around Y)", q);
        print_quat("q* (conjugate)", q_conj);
        print_quat("q^-1 (inverse)", q_inv);
        printf("\n");

        /* For unit quaternions, conjugate = inverse */
        int conj_eq_inv = approx_eq(q_conj.w, q_inv.w, 0.0001f) &&
                          approx_eq(q_conj.x, q_inv.x, 0.0001f) &&
                          approx_eq(q_conj.y, q_inv.y, 0.0001f) &&
                          approx_eq(q_conj.z, q_inv.z, 0.0001f);
        SDL_Log("  conjugate = inverse? %s (only for unit quaternions)",
                conj_eq_inv ? "YES" : "NO");
        printf("\n");

        /* q * q* = identity */
        quat product = quat_multiply(q, q_conj);
        print_quat("q * q*", product);
        SDL_Log("  Should be identity (1, 0, 0, 0)");
        printf("\n");

        /* Double cover: q and -q produce the same rotation */
        quat neg_q = quat_negate(q);
        vec3 test_v = vec3_create(1.0f, 0.0f, 0.0f);
        vec3 result_q = quat_rotate_vec3(q, test_v);
        vec3 result_neg = quat_rotate_vec3(neg_q, test_v);

        print_quat("q", q);
        print_quat("-q", neg_q);
        print_vec3("q rotates (1,0,0) to", result_q);
        print_vec3("-q rotates (1,0,0) to", result_neg);
        int same = approx_eq(result_q.x, result_neg.x, 0.001f) &&
                   approx_eq(result_q.y, result_neg.y, 0.001f) &&
                   approx_eq(result_q.z, result_neg.z, 0.001f);
        SDL_Log("  Same result? %s — this is the double cover property",
                same ? "YES" : "NO");
        printf("\n");
    }

    /* ── Section 7: Quaternion multiplication ─────────────────────────────
     *
     * Multiplying quaternions composes rotations, just like multiplying
     * matrices. The result of a * b is "apply b first, then a."
     *
     * Key difference from matrices: the formula is much simpler and
     * always produces a valid rotation (when inputs are unit quaternions,
     * the output is also unit).
     *
     * Quaternion multiplication is:
     *   - NOT commutative: a*b != b*a in general
     *   - Associative: (a*b)*c = a*(b*c)
     */
    printf("-- 7. Quaternion multiplication -- composing rotations ----\n\n");

    {
        /* Yaw 90° then pitch 45° */
        float yaw_angle = SEC7_YAW_DEG * FORGE_DEG2RAD;
        float pitch_angle = SEC7_PITCH_DEG * FORGE_DEG2RAD;

        quat q_yaw = quat_from_axis_angle(
            vec3_create(0.0f, 1.0f, 0.0f), yaw_angle);
        quat q_pitch = quat_from_axis_angle(
            vec3_create(1.0f, 0.0f, 0.0f), pitch_angle);

        /* Apply pitch first, then yaw: combined = q_yaw * q_pitch */
        quat combined = quat_multiply(q_yaw, q_pitch);

        print_quat("q_yaw (90 deg Y)", q_yaw);
        print_quat("q_pitch (45 deg X)", q_pitch);
        print_quat("combined (yaw * pitch)", combined);
        printf("\n");

        /* Compare with reverse order */
        quat reversed = quat_multiply(q_pitch, q_yaw);
        print_quat("reversed (pitch * yaw)", reversed);

        int different = !approx_eq(combined.w, reversed.w, 0.001f) ||
                        !approx_eq(combined.x, reversed.x, 0.001f) ||
                        !approx_eq(combined.y, reversed.y, 0.001f) ||
                        !approx_eq(combined.z, reversed.z, 0.001f);
        SDL_Log("  Order matters? %s — NOT commutative",
                different ? "YES (different results)" : "NO");
        printf("\n");

        /* Compare quaternion result with matrix result */
        mat4 m_yaw = mat4_rotate_y(yaw_angle);
        mat4 m_pitch = mat4_rotate_x(pitch_angle);
        mat4 m_combined = mat4_multiply(m_yaw, m_pitch);
        mat4 q_as_mat = quat_to_mat4(combined);

        vec4 test_v = vec4_create(0.0f, 0.0f, -1.0f, 0.0f);
        vec4 mat_result = mat4_multiply_vec4(m_combined, test_v);
        vec4 quat_result = mat4_multiply_vec4(q_as_mat, test_v);

        SDL_Log("  Quaternion vs matrix — same result?");
        SDL_Log("    Matrix:     (%.4f, %.4f, %.4f)",
                mat_result.x, mat_result.y, mat_result.z);
        SDL_Log("    Quaternion: (%.4f, %.4f, %.4f)",
                quat_result.x, quat_result.y, quat_result.z);
        int match = approx_eq(mat_result.x, quat_result.x, 0.001f) &&
                    approx_eq(mat_result.y, quat_result.y, 0.001f) &&
                    approx_eq(mat_result.z, quat_result.z, 0.001f);
        SDL_Log("    Match? %s", match ? "YES" : "NO");
        printf("\n");
    }

    /* ── Section 8: Rotating a vector with a quaternion ───────────────────
     *
     * The formula for rotating vector v by quaternion q is:
     *   v' = q * v * q*    (where v is treated as quaternion (0, v.x, v.y, v.z))
     *
     * This "sandwich product" is the fundamental quaternion rotation.
     * We use an optimized formula that avoids constructing intermediate
     * quaternions:
     *   v' = v + 2*w*(u x v) + 2*(u x (u x v))
     * where u = (q.x, q.y, q.z) is the vector part of q.
     */
    printf("-- 8. Rotating a vector with a quaternion -----------------\n\n");

    {
        vec3 v = vec3_create(1.0f, 0.0f, 0.0f);

        /* 90° rotation around each axis */
        float angle = SEC8_ANGLE_DEG * FORGE_DEG2RAD;
        quat q_y = quat_from_axis_angle(vec3_create(0, 1, 0), angle);
        quat q_x = quat_from_axis_angle(vec3_create(1, 0, 0), angle);
        quat q_z = quat_from_axis_angle(vec3_create(0, 0, 1), angle);

        vec3 ry = quat_rotate_vec3(q_y, v);
        vec3 rx = quat_rotate_vec3(q_x, v);
        vec3 rz = quat_rotate_vec3(q_z, v);

        SDL_Log("  Rotating (1, 0, 0) by 90 degrees:");
        SDL_Log("    Around Y: (%.1f, %.1f, %.1f)", ry.x, ry.y, ry.z);
        SDL_Log("    Around X: (%.1f, %.1f, %.1f)", rx.x, rx.y, rx.z);
        SDL_Log("    Around Z: (%.1f, %.1f, %.1f)", rz.x, rz.y, rz.z);
        printf("\n");

        /* Compare with Rodrigues' formula — should give same results */
        vec3 rodrigues_y = vec3_rotate_axis_angle(
            v, vec3_create(0, 1, 0), angle);
        vec3 rodrigues_x = vec3_rotate_axis_angle(
            v, vec3_create(1, 0, 0), angle);
        vec3 rodrigues_z = vec3_rotate_axis_angle(
            v, vec3_create(0, 0, 1), angle);

        int all_match =
            approx_eq(ry.x, rodrigues_y.x, 0.001f) &&
            approx_eq(ry.y, rodrigues_y.y, 0.001f) &&
            approx_eq(ry.z, rodrigues_y.z, 0.001f) &&
            approx_eq(rx.x, rodrigues_x.x, 0.001f) &&
            approx_eq(rx.y, rodrigues_x.y, 0.001f) &&
            approx_eq(rx.z, rodrigues_x.z, 0.001f) &&
            approx_eq(rz.x, rodrigues_z.x, 0.001f) &&
            approx_eq(rz.y, rodrigues_z.y, 0.001f) &&
            approx_eq(rz.z, rodrigues_z.z, 0.001f);

        SDL_Log("  Quaternion matches Rodrigues? %s",
                all_match ? "YES — same underlying math" : "NO");
        printf("\n");
    }

    /* ── Section 9: Conversions ───────────────────────────────────────────
     *
     * Conversion between the four representations:
     *
     *   Euler angles <-> Quaternion <-> Matrix
     *                ^                ^
     *                |                |
     *                +-- Axis-angle --+
     *
     * The most common conversions:
     *   - Euler -> Quaternion: for user input (camera controls)
     *   - Quaternion -> Matrix: for the GPU (MVP pipeline)
     *   - Matrix -> Quaternion: from imported animations
     *   - Axis-angle -> Quaternion: for specifying rotations in code
     */
    printf("-- 9. Conversions -- round-trips between representations --\n\n");

    {
        /* Start with Euler angles */
        float yaw_deg = 45.0f;
        float pitch_deg = 30.0f;
        float roll_deg = 15.0f;
        float yaw = yaw_deg * FORGE_DEG2RAD;
        float pitch = pitch_deg * FORGE_DEG2RAD;
        float roll = roll_deg * FORGE_DEG2RAD;

        SDL_Log("  Starting Euler: yaw=%.0f, pitch=%.0f, roll=%.0f (degrees)",
                yaw_deg, pitch_deg, roll_deg);
        printf("\n");

        /* Euler -> Quaternion */
        quat q = quat_from_euler(yaw, pitch, roll);
        print_quat("Euler -> Quaternion", q);

        /* Quaternion -> Matrix */
        mat4 m = quat_to_mat4(q);
        print_mat4("Quaternion -> Matrix", m);
        printf("\n");

        /* Matrix -> Quaternion */
        quat q2 = quat_from_mat4(m);
        print_quat("Matrix -> Quaternion", q2);

        /* Quaternion -> Euler (may differ from original due to
         * multiple valid representations) */
        vec3 euler2 = quat_to_euler(q2);
        SDL_Log("  Quaternion -> Euler: yaw=%.1f, pitch=%.1f, roll=%.1f (degrees)",
                euler2.x * FORGE_RAD2DEG,
                euler2.y * FORGE_RAD2DEG,
                euler2.z * FORGE_RAD2DEG);
        printf("\n");

        /* Verify: both quaternions rotate a vector the same way */
        vec3 test = vec3_create(1.0f, 2.0f, 3.0f);
        vec3 r1 = quat_rotate_vec3(q, test);
        vec3 r2 = quat_rotate_vec3(q2, test);

        print_vec3("Original q rotates (1,2,3) to", r1);
        print_vec3("Round-trip q rotates (1,2,3) to", r2);
        int match = approx_eq(r1.x, r2.x, 0.001f) &&
                    approx_eq(r1.y, r2.y, 0.001f) &&
                    approx_eq(r1.z, r2.z, 0.001f);
        SDL_Log("  Round-trip preserved rotation? %s", match ? "YES" : "NO");
        printf("\n");

        /* Compare with building the matrix from individual Euler rotations */
        mat4 m_euler = mat4_multiply(
            mat4_rotate_y(yaw),
            mat4_multiply(mat4_rotate_x(pitch), mat4_rotate_z(roll)));

        vec4 v4 = vec4_create(1.0f, 2.0f, 3.0f, 0.0f);
        vec4 from_euler_mat = mat4_multiply_vec4(m_euler, v4);
        vec4 from_quat_mat = mat4_multiply_vec4(m, v4);

        SDL_Log("  Matrix from Euler vs matrix from quaternion:");
        SDL_Log("    Euler matrix:      (%.4f, %.4f, %.4f)",
                from_euler_mat.x, from_euler_mat.y, from_euler_mat.z);
        SDL_Log("    Quaternion matrix: (%.4f, %.4f, %.4f)",
                from_quat_mat.x, from_quat_mat.y, from_quat_mat.z);
        match = approx_eq(from_euler_mat.x, from_quat_mat.x, 0.001f) &&
                approx_eq(from_euler_mat.y, from_quat_mat.y, 0.001f) &&
                approx_eq(from_euler_mat.z, from_quat_mat.z, 0.001f);
        SDL_Log("    Match? %s", match ? "YES" : "NO");
        printf("\n");
    }

    /* ── Section 10: SLERP ────────────────────────────────────────────────
     *
     * SLERP (Spherical Linear Interpolation) smoothly interpolates
     * between two orientations along the shortest arc on the unit sphere.
     *
     * Unlike linear interpolation of Euler angles (which can wobble and
     * hit gimbal lock), SLERP produces:
     *   - Constant angular velocity (uniform speed)
     *   - No gimbal lock
     *   - Shortest path between orientations
     *
     * NLERP (Normalized Linear Interpolation) is the cheaper alternative:
     *   - Linearly interpolate components, then normalize
     *   - Same path as SLERP but non-constant speed
     *   - Good enough for most games (faster, commutative)
     *
     *   SLERP:  arc on unit sphere (great circle)
     *   NLERP:  chord on unit sphere, then projected back
     *
     *       SLERP (arc)          NLERP (chord + normalize)
     *      ___.___.___           ___.___.___
     *     /    .    \           /    .    \
     *    /   . . .   \         /   . . .   \
     *   | .    |    . |       | .    |    . |
     *   A      |      B       A------+------B
     *   | .    |    . |       | .    |    . |
     *    \   . . .   /         \   . . .   /
     *     \___.___._/           \___.___._/
     *
     * The dots on the arc are evenly spaced (SLERP = constant speed).
     * The dots on the chord are evenly spaced but project to uneven
     * arc positions (NLERP = variable speed).
     */
    printf("-- 10. SLERP -- smooth rotation interpolation -------------\n\n");

    {
        /* Interpolate from 0° to 120° rotation around Y */
        float start_angle = SEC10_START_DEG * FORGE_DEG2RAD;
        float end_angle = SEC10_END_DEG * FORGE_DEG2RAD;
        vec3 axis = vec3_create(0.0f, 1.0f, 0.0f);

        quat q_start = quat_from_axis_angle(axis, start_angle);
        quat q_end   = quat_from_axis_angle(axis, end_angle);

        SDL_Log("  SLERP from %.0f to %.0f degrees around Y axis:",
                SEC10_START_DEG, SEC10_END_DEG);
        SDL_Log("    t   | SLERP angle | NLERP angle | difference");
        SDL_Log("    ----|-------------|-------------|----------");

        vec3 test_v = vec3_create(1.0f, 0.0f, 0.0f);

        for (int i = 0; i <= SEC10_NUM_STEPS; i++) {
            float t = (float)i / (float)SEC10_NUM_STEPS;

            quat q_slerp = quat_slerp(q_start, q_end, t);
            quat q_nlerp = quat_nlerp(q_start, q_end, t);

            /* Extract angle from each interpolated quaternion */
            vec3 slerp_axis;
            float slerp_angle;
            quat_to_axis_angle(q_slerp, &slerp_axis, &slerp_angle);

            vec3 nlerp_axis;
            float nlerp_angle;
            quat_to_axis_angle(q_nlerp, &nlerp_axis, &nlerp_angle);

            float slerp_deg = slerp_angle * FORGE_RAD2DEG;
            float nlerp_deg = nlerp_angle * FORGE_RAD2DEG;

            SDL_Log("    %.2f |   %7.2f   |   %7.2f   |   %6.2f",
                    t, slerp_deg, nlerp_deg, slerp_deg - nlerp_deg);
        }

        printf("\n  SLERP: perfectly uniform angle increments (%.1f degrees/step)\n",
               SEC10_END_DEG / SEC10_NUM_STEPS);
        printf("  NLERP: slightly non-uniform (faster in the middle)\n");
        printf("  For small angles, the difference is negligible.\n\n");

        /* Show vector interpolation */
        SDL_Log("  Where does (1, 0, 0) point at each step?");
        for (int i = 0; i <= 4; i++) {
            float t = (float)i / 4.0f;
            quat q = quat_slerp(q_start, q_end, t);
            vec3 v = quat_rotate_vec3(q, test_v);
            SDL_Log("    t=%.2f: (%.4f, %.4f, %.4f)", t, v.x, v.y, v.z);
        }
        printf("\n");
    }

    /* ── Section 11: Summary ──────────────────────────────────────────────── */
    printf("-- 11. Summary --------------------------------------------\n\n");
    printf("  Four ways to represent 3D rotation:\n\n");
    printf("    Representation  | Floats | Compose | Interpolate | Gimbal lock?\n");
    printf("    ----------------|--------|---------|-------------|------------\n");
    printf("    Euler angles    |   3    | Messy   | Broken      | YES\n");
    printf("    Rotation matrix |   9    | Multiply| Difficult   | No\n");
    printf("    Axis-angle      |   4    | Hard    | Hard        | No\n");
    printf("    Quaternion      |   4    | Multiply| SLERP       | No\n\n");

    printf("  When to use what:\n");
    printf("    * Euler angles  — User input/display only\n");
    printf("    * Rotation matrix — GPU transforms (MVP pipeline)\n");
    printf("    * Axis-angle    — Specifying rotations in code\n");
    printf("    * Quaternion    — Runtime storage, composition, interpolation\n\n");

    printf("  Typical pipeline:\n");
    printf("    User input -> Euler angles\n");
    printf("                  -> quat_from_euler(yaw, pitch, roll)\n");
    printf("                     -> quaternion (store and compose)\n");
    printf("                        -> quat_to_mat4(q)\n");
    printf("                           -> rotation matrix (send to GPU)\n\n");

    printf("  New math library functions:\n");
    printf("    * quat type (w, x, y, z)\n");
    printf("    * quat_from_axis_angle / quat_to_axis_angle\n");
    printf("    * quat_from_euler / quat_to_euler (intrinsic Y-X-Z)\n");
    printf("    * quat_to_mat4 / quat_from_mat4\n");
    printf("    * quat_multiply (compose), quat_rotate_vec3 (apply)\n");
    printf("    * quat_slerp / quat_nlerp (interpolation)\n");
    printf("    * vec3_rotate_axis_angle (Rodrigues' formula)\n\n");

    printf("  See: lessons/math/08-orientation/README.md\n");
    printf("  See: lessons/math/05-matrices/ (rotation matrix fundamentals)\n");
    printf("  See: lessons/math/01-vectors/ (cross product, normalize)\n\n");

    SDL_Quit();
    return 0;
}
