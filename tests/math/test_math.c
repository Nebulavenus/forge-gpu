/*
 * Math Library Tests
 *
 * Automated tests for common/math/forge_math.h
 * Verifies correctness of all vector and matrix operations.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

/* Epsilon for floating-point comparisons (account for rounding errors) */
#define EPSILON 0.0001f

/* ── Test Constants ──────────────────────────────────────────────────────── */

/* Common scalars */
#define TEST_ZERO       0.0f
#define TEST_ONE        1.0f
#define TEST_TWO        2.0f
#define TEST_THREE      3.0f
#define TEST_FOUR       4.0f
#define TEST_FIVE       5.0f
#define TEST_TEN        10.0f
#define TEST_HALF       0.5f
#define TEST_SCALE_2    2.0f

/* Common vec2 test vectors */
#define TEST_V2_A       vec2_create(1.0f, 2.0f)
#define TEST_V2_B       vec2_create(3.0f, 4.0f)
#define TEST_V2_ZERO    vec2_create(0.0f, 0.0f)
#define TEST_V2_X_AXIS  vec2_create(1.0f, 0.0f)
#define TEST_V2_Y_AXIS  vec2_create(0.0f, 1.0f)
#define TEST_V2_TEN     vec2_create(10.0f, 10.0f)
#define TEST_V2_345     vec2_create(3.0f, 4.0f)  /* 3-4-5 triangle */

/* Common vec3 test vectors */
#define TEST_V3_A       vec3_create(1.0f, 2.0f, 3.0f)
#define TEST_V3_B       vec3_create(4.0f, 5.0f, 6.0f)
#define TEST_V3_ZERO    vec3_create(0.0f, 0.0f, 0.0f)
#define TEST_V3_X_AXIS  vec3_create(1.0f, 0.0f, 0.0f)
#define TEST_V3_Y_AXIS  vec3_create(0.0f, 1.0f, 0.0f)
#define TEST_V3_Z_AXIS  vec3_create(0.0f, 0.0f, 1.0f)
#define TEST_V3_TEN     vec3_create(10.0f, 10.0f, 10.0f)
#define TEST_V3_345     vec3_create(3.0f, 4.0f, 0.0f)  /* 3-4-5 triangle */

/* Common vec4 test vectors */
#define TEST_V4_A       vec4_create(1.0f, 2.0f, 3.0f, 4.0f)
#define TEST_V4_B       vec4_create(5.0f, 6.0f, 7.0f, 8.0f)
#define TEST_V4_X_AXIS  vec4_create(1.0f, 0.0f, 0.0f, 0.0f)
#define TEST_V4_Y_AXIS  vec4_create(0.0f, 1.0f, 0.0f, 0.0f)
#define TEST_V4_POINT   vec4_create(0.0f, 0.0f, 0.0f, 1.0f)

/* Check if two floats are approximately equal */
static bool float_eq(float a, float b)
{
    return SDL_fabsf(a - b) < EPSILON;
}

/* Check if two vec2s are approximately equal */
static bool vec2_eq(vec2 a, vec2 b)
{
    return float_eq(a.x, b.x) && float_eq(a.y, b.y);
}

/* Check if two vec3s are approximately equal */
static bool vec3_eq(vec3 a, vec3 b)
{
    return float_eq(a.x, b.x) && float_eq(a.y, b.y) && float_eq(a.z, b.z);
}

/* Check if two vec4s are approximately equal */
static bool vec4_eq(vec4 a, vec4 b)
{
    return float_eq(a.x, b.x) && float_eq(a.y, b.y) &&
           float_eq(a.z, b.z) && float_eq(a.w, b.w);
}

/* Test assertion macros */
#define TEST(name) \
    do { \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_FLOAT_EQ(a, b) \
    if (!float_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected %.6f, got %.6f", b, a); \
        fail_count++; \
        return; \
    }

#define ASSERT_VEC2_EQ(a, b) \
    if (!vec2_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected (%.3f, %.3f), got (%.3f, %.3f)", \
                     b.x, b.y, a.x, a.y); \
        fail_count++; \
        return; \
    }

#define ASSERT_VEC3_EQ(a, b) \
    if (!vec3_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected (%.3f, %.3f, %.3f), got (%.3f, %.3f, %.3f)", \
                     b.x, b.y, b.z, a.x, a.y, a.z); \
        fail_count++; \
        return; \
    }

#define ASSERT_VEC4_EQ(a, b) \
    if (!vec4_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected (%.3f, %.3f, %.3f, %.3f), got (%.3f, %.3f, %.3f, %.3f)", \
                     b.x, b.y, b.z, b.w, a.x, a.y, a.z, a.w); \
        fail_count++; \
        return; \
    }

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

/* ══════════════════════════════════════════════════════════════════════════
 * vec2 Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vec2_create(void)
{
    TEST("vec2_create");
    vec2 v = TEST_V2_A;
    ASSERT_FLOAT_EQ(v.x, TEST_ONE);
    ASSERT_FLOAT_EQ(v.y, TEST_TWO);
    END_TEST();
}

static void test_vec2_add(void)
{
    TEST("vec2_add");
    vec2 a = TEST_V2_A;
    vec2 b = TEST_V2_B;
    vec2 result = vec2_add(a, b);
    ASSERT_VEC2_EQ(result, vec2_create(TEST_FOUR, 6.0f));
    END_TEST();
}

static void test_vec2_sub(void)
{
    TEST("vec2_sub");
    vec2 a = vec2_create(TEST_FIVE, TEST_THREE);
    vec2 b = vec2_create(TEST_TWO, TEST_ONE);
    vec2 result = vec2_sub(a, b);
    ASSERT_VEC2_EQ(result, vec2_create(TEST_THREE, TEST_TWO));
    END_TEST();
}

static void test_vec2_scale(void)
{
    TEST("vec2_scale");
    vec2 v = vec2_create(TEST_TWO, TEST_THREE);
    vec2 result = vec2_scale(v, TEST_SCALE_2);
    ASSERT_VEC2_EQ(result, vec2_create(TEST_FOUR, 6.0f));
    END_TEST();
}

static void test_vec2_dot(void)
{
    TEST("vec2_dot");
    vec2 a = TEST_V2_X_AXIS;
    vec2 b = TEST_V2_Y_AXIS;
    float dot = vec2_dot(a, b);
    ASSERT_FLOAT_EQ(dot, TEST_ZERO);  /* Perpendicular */

    vec2 c = vec2_create(TEST_TWO, TEST_ZERO);
    float dot2 = vec2_dot(a, c);
    ASSERT_FLOAT_EQ(dot2, TEST_TWO);  /* Parallel */
    END_TEST();
}

static void test_vec2_length(void)
{
    TEST("vec2_length");
    vec2 v = TEST_V2_345;
    float len = vec2_length(v);
    ASSERT_FLOAT_EQ(len, TEST_FIVE);  /* 3-4-5 triangle */
    END_TEST();
}

static void test_vec2_normalize(void)
{
    TEST("vec2_normalize");
    vec2 v = TEST_V2_345;
    vec2 normalized = vec2_normalize(v);
    ASSERT_FLOAT_EQ(vec2_length(normalized), TEST_ONE);  /* Unit length */
    ASSERT_VEC2_EQ(normalized, vec2_create(0.6f, 0.8f));
    END_TEST();
}

static void test_vec2_lerp(void)
{
    TEST("vec2_lerp");
    vec2 a = TEST_V2_ZERO;
    vec2 b = TEST_V2_TEN;
    vec2 mid = vec2_lerp(a, b, TEST_HALF);
    ASSERT_VEC2_EQ(mid, vec2_create(TEST_FIVE, TEST_FIVE));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * vec3 Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vec3_create(void)
{
    TEST("vec3_create");
    vec3 v = TEST_V3_A;
    ASSERT_FLOAT_EQ(v.x, TEST_ONE);
    ASSERT_FLOAT_EQ(v.y, TEST_TWO);
    ASSERT_FLOAT_EQ(v.z, TEST_THREE);
    END_TEST();
}

static void test_vec3_add(void)
{
    TEST("vec3_add");
    vec3 a = TEST_V3_A;
    vec3 b = TEST_V3_B;
    vec3 result = vec3_add(a, b);
    ASSERT_VEC3_EQ(result, vec3_create(TEST_FIVE, 7.0f, 9.0f));
    END_TEST();
}

static void test_vec3_sub(void)
{
    TEST("vec3_sub");
    vec3 a = vec3_create(TEST_FIVE, TEST_THREE, TEST_TWO);
    vec3 b = vec3_create(TEST_TWO, TEST_ONE, TEST_ONE);
    vec3 result = vec3_sub(a, b);
    ASSERT_VEC3_EQ(result, vec3_create(TEST_THREE, TEST_TWO, TEST_ONE));
    END_TEST();
}

static void test_vec3_scale(void)
{
    TEST("vec3_scale");
    vec3 v = TEST_V3_A;
    vec3 result = vec3_scale(v, TEST_SCALE_2);
    ASSERT_VEC3_EQ(result, vec3_create(TEST_TWO, TEST_FOUR, 6.0f));
    END_TEST();
}

static void test_vec3_dot(void)
{
    TEST("vec3_dot");
    vec3 a = TEST_V3_X_AXIS;
    vec3 b = TEST_V3_Y_AXIS;
    float dot = vec3_dot(a, b);
    ASSERT_FLOAT_EQ(dot, TEST_ZERO);  /* Perpendicular */
    END_TEST();
}

static void test_vec3_cross(void)
{
    TEST("vec3_cross");
    vec3 x = TEST_V3_X_AXIS;
    vec3 y = TEST_V3_Y_AXIS;
    vec3 z = vec3_cross(x, y);
    ASSERT_VEC3_EQ(z, TEST_V3_Z_AXIS);  /* X × Y = Z */

    /* Verify perpendicularity */
    ASSERT_FLOAT_EQ(vec3_dot(z, x), TEST_ZERO);
    ASSERT_FLOAT_EQ(vec3_dot(z, y), TEST_ZERO);
    END_TEST();
}

static void test_vec3_length(void)
{
    TEST("vec3_length");
    vec3 v = TEST_V3_345;
    float len = vec3_length(v);
    ASSERT_FLOAT_EQ(len, TEST_FIVE);
    END_TEST();
}

static void test_vec3_normalize(void)
{
    TEST("vec3_normalize");
    vec3 v = TEST_V3_345;
    vec3 normalized = vec3_normalize(v);
    ASSERT_FLOAT_EQ(vec3_length(normalized), TEST_ONE);
    END_TEST();
}

static void test_vec3_lerp(void)
{
    TEST("vec3_lerp");
    vec3 a = TEST_V3_ZERO;
    vec3 b = TEST_V3_TEN;
    vec3 mid = vec3_lerp(a, b, TEST_HALF);
    ASSERT_VEC3_EQ(mid, vec3_create(TEST_FIVE, TEST_FIVE, TEST_FIVE));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * vec4 Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vec4_create(void)
{
    TEST("vec4_create");
    vec4 v = TEST_V4_A;
    ASSERT_FLOAT_EQ(v.x, TEST_ONE);
    ASSERT_FLOAT_EQ(v.y, TEST_TWO);
    ASSERT_FLOAT_EQ(v.z, TEST_THREE);
    ASSERT_FLOAT_EQ(v.w, TEST_FOUR);
    END_TEST();
}

static void test_vec4_add(void)
{
    TEST("vec4_add");
    vec4 a = TEST_V4_A;
    vec4 b = TEST_V4_B;
    vec4 result = vec4_add(a, b);
    ASSERT_VEC4_EQ(result, vec4_create(6.0f, 8.0f, TEST_TEN, 12.0f));
    END_TEST();
}

static void test_vec4_sub(void)
{
    TEST("vec4_sub");
    vec4 a = TEST_V4_B;
    vec4 b = TEST_V4_A;
    vec4 result = vec4_sub(a, b);
    ASSERT_VEC4_EQ(result, vec4_create(TEST_FOUR, TEST_FOUR, TEST_FOUR, TEST_FOUR));
    END_TEST();
}

static void test_vec4_scale(void)
{
    TEST("vec4_scale");
    vec4 v = TEST_V4_A;
    vec4 result = vec4_scale(v, TEST_SCALE_2);
    ASSERT_VEC4_EQ(result, vec4_create(TEST_TWO, TEST_FOUR, 6.0f, 8.0f));
    END_TEST();
}

static void test_vec4_dot(void)
{
    TEST("vec4_dot");
    vec4 a = TEST_V4_X_AXIS;
    vec4 b = TEST_V4_Y_AXIS;
    float dot = vec4_dot(a, b);
    ASSERT_FLOAT_EQ(dot, TEST_ZERO);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * mat4 Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_mat4_identity(void)
{
    TEST("mat4_identity");
    mat4 m = mat4_identity();

    /* Diagonal should be 1.0 */
    ASSERT_FLOAT_EQ(m.m[0], TEST_ONE);
    ASSERT_FLOAT_EQ(m.m[5], TEST_ONE);
    ASSERT_FLOAT_EQ(m.m[10], TEST_ONE);
    ASSERT_FLOAT_EQ(m.m[15], TEST_ONE);

    /* Off-diagonal should be 0.0 */
    ASSERT_FLOAT_EQ(m.m[1], TEST_ZERO);
    ASSERT_FLOAT_EQ(m.m[4], TEST_ZERO);
    END_TEST();
}

static void test_mat4_translate(void)
{
    TEST("mat4_translate");
    mat4 m = mat4_translate(vec3_create(TEST_FIVE, TEST_THREE, TEST_TWO));

    /* Translation is in column 3 (indices 12, 13, 14) */
    ASSERT_FLOAT_EQ(m.m[12], TEST_FIVE);
    ASSERT_FLOAT_EQ(m.m[13], TEST_THREE);
    ASSERT_FLOAT_EQ(m.m[14], TEST_TWO);

    /* Transform a point */
    vec4 point = TEST_V4_POINT;
    vec4 result = mat4_multiply_vec4(m, point);
    ASSERT_VEC4_EQ(result, vec4_create(TEST_FIVE, TEST_THREE, TEST_TWO, TEST_ONE));
    END_TEST();
}

static void test_mat4_scale(void)
{
    TEST("mat4_scale");
    mat4 m = mat4_scale(vec3_create(TEST_TWO, TEST_THREE, TEST_FOUR));

    vec4 v = vec4_create(TEST_ONE, TEST_ONE, TEST_ONE, TEST_ONE);
    vec4 result = mat4_multiply_vec4(m, v);
    ASSERT_VEC4_EQ(result, vec4_create(TEST_TWO, TEST_THREE, TEST_FOUR, TEST_ONE));
    END_TEST();
}

static void test_mat4_rotate_z(void)
{
    TEST("mat4_rotate_z");
    /* 90-degree rotation around Z should turn X-axis into Y-axis */
    mat4 m = mat4_rotate_z(FORGE_PI / TEST_SCALE_2);
    vec4 x_axis = TEST_V4_X_AXIS;
    vec4 result = mat4_multiply_vec4(m, x_axis);

    /* Should be approximately (0, 1, 0, 0) */
    ASSERT_FLOAT_EQ(result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.y, TEST_ONE);
    ASSERT_FLOAT_EQ(result.z, TEST_ZERO);
    END_TEST();
}

static void test_mat4_multiply(void)
{
    TEST("mat4_multiply");
    /* Translate then scale should scale first, then translate */
    mat4 translate = mat4_translate(vec3_create(TEST_TEN, TEST_ZERO, TEST_ZERO));
    mat4 scale = mat4_scale_uniform(TEST_SCALE_2);
    mat4 combined = mat4_multiply(translate, scale);

    vec4 point = vec4_create(TEST_ONE, TEST_ZERO, TEST_ZERO, TEST_ONE);
    vec4 result = mat4_multiply_vec4(combined, point);

    /* Scale first (1 * 2 = 2), then translate (2 + 10 = 12) */
    ASSERT_FLOAT_EQ(result.x, 12.0f);
    END_TEST();
}

static void test_mat4_rotate_x(void)
{
    TEST("mat4_rotate_x");
    /* 90-degree rotation around X should turn Y-axis into Z-axis */
    mat4 m = mat4_rotate_x(FORGE_PI / TEST_SCALE_2);
    vec4 y_axis = TEST_V4_Y_AXIS;
    vec4 result = mat4_multiply_vec4(m, y_axis);

    /* Should be approximately (0, 0, 1, 0) */
    ASSERT_FLOAT_EQ(result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.z, TEST_ONE);
    END_TEST();
}

static void test_mat4_rotate_y(void)
{
    TEST("mat4_rotate_y");
    /* 90-degree rotation around Y should turn X-axis into -Z */
    mat4 m = mat4_rotate_y(FORGE_PI / TEST_SCALE_2);
    vec4 x_axis = TEST_V4_X_AXIS;
    vec4 result = mat4_multiply_vec4(m, x_axis);

    /* Should be approximately (0, 0, -1, 0) */
    ASSERT_FLOAT_EQ(result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.z, -TEST_ONE);
    END_TEST();
}

static void test_mat4_look_at(void)
{
    TEST("mat4_look_at");
    /* Camera at (0, 0, 5) looking at origin — standard setup */
    vec3 eye = vec3_create(TEST_ZERO, TEST_ZERO, TEST_FIVE);
    vec3 target = vec3_create(TEST_ZERO, TEST_ZERO, TEST_ZERO);
    vec3 up = TEST_V3_Y_AXIS;
    mat4 view = mat4_look_at(eye, target, up);

    /* Origin should map to (0, 0, -5) in view space (5 units in front) */
    vec4 origin = vec4_create(TEST_ZERO, TEST_ZERO, TEST_ZERO, TEST_ONE);
    vec4 result = mat4_multiply_vec4(view, origin);
    ASSERT_FLOAT_EQ(result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.z, -TEST_FIVE);

    /* Camera position should map to origin in view space */
    vec4 eye_point = vec4_create(TEST_ZERO, TEST_ZERO, TEST_FIVE, TEST_ONE);
    vec4 eye_result = mat4_multiply_vec4(view, eye_point);
    ASSERT_FLOAT_EQ(eye_result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(eye_result.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(eye_result.z, TEST_ZERO);

    /* Point to the right in world (+X) should be +X in view space */
    vec4 right_point = vec4_create(TEST_ONE, TEST_ZERO, TEST_ZERO, TEST_ONE);
    vec4 right_result = mat4_multiply_vec4(view, right_point);
    ASSERT_FLOAT_EQ(right_result.x, TEST_ONE);
    ASSERT_FLOAT_EQ(right_result.y, TEST_ZERO);
    END_TEST();
}

static void test_mat4_perspective(void)
{
    TEST("mat4_perspective");
    float fov = FORGE_PI / TEST_THREE;  /* 60 degrees */
    float aspect = 16.0f / 9.0f;
    float near = 0.1f;
    float far = 100.0f;
    mat4 proj = mat4_perspective(fov, aspect, near, far);

    /* Point on the near plane (z = -near) should map to NDC z = 0 */
    vec4 near_point = vec4_create(TEST_ZERO, TEST_ZERO, -near, TEST_ONE);
    vec4 near_clip = mat4_multiply_vec4(proj, near_point);
    float near_ndc_z = near_clip.z / near_clip.w;
    ASSERT_FLOAT_EQ(near_ndc_z, TEST_ZERO);

    /* Point on the far plane (z = -far) should map to NDC z = 1 */
    vec4 far_point = vec4_create(TEST_ZERO, TEST_ZERO, -far, TEST_ONE);
    vec4 far_clip = mat4_multiply_vec4(proj, far_point);
    float far_ndc_z = far_clip.z / far_clip.w;
    ASSERT_FLOAT_EQ(far_ndc_z, TEST_ONE);

    /* w should equal -z (positive depth) */
    ASSERT_FLOAT_EQ(near_clip.w, near);
    ASSERT_FLOAT_EQ(far_clip.w, far);

    /* Center point should stay centered after perspective divide */
    float near_ndc_x = near_clip.x / near_clip.w;
    float near_ndc_y = near_clip.y / near_clip.w;
    ASSERT_FLOAT_EQ(near_ndc_x, TEST_ZERO);
    ASSERT_FLOAT_EQ(near_ndc_y, TEST_ZERO);
    END_TEST();
}

static void test_mat4_orthographic(void)
{
    TEST("mat4_orthographic");
    mat4 ortho = mat4_orthographic(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 20.0f);

    /* Near plane (z = -0.1 in view space) should map to NDC z = 0 */
    vec4 near_point = vec4_create(TEST_ZERO, TEST_ZERO, -0.1f, TEST_ONE);
    vec4 near_clip = mat4_multiply_vec4(ortho, near_point);
    ASSERT_FLOAT_EQ(near_clip.z, TEST_ZERO);

    /* Far plane (z = -20 in view space) should map to NDC z = 1 */
    vec4 far_point = vec4_create(TEST_ZERO, TEST_ZERO, -20.0f, TEST_ONE);
    vec4 far_clip = mat4_multiply_vec4(ortho, far_point);
    ASSERT_FLOAT_EQ(far_clip.z, TEST_ONE);

    /* w should always be 1 (no perspective divide) */
    ASSERT_FLOAT_EQ(near_clip.w, TEST_ONE);
    ASSERT_FLOAT_EQ(far_clip.w, TEST_ONE);

    /* Center of the box should map to NDC origin */
    vec4 center = vec4_create(TEST_ZERO, TEST_ZERO, -10.05f, TEST_ONE);
    vec4 center_clip = mat4_multiply_vec4(ortho, center);
    ASSERT_FLOAT_EQ(center_clip.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(center_clip.y, TEST_ZERO);
    END_TEST();
}

static void test_mat4_orthographic_corners(void)
{
    TEST("mat4_orthographic corners");
    mat4 ortho = mat4_orthographic(-10.0f, 10.0f, -5.0f, 5.0f, 1.0f, 100.0f);

    /* Left edge maps to NDC x = -1 */
    vec4 left = vec4_create(-10.0f, TEST_ZERO, -1.0f, TEST_ONE);
    vec4 left_clip = mat4_multiply_vec4(ortho, left);
    ASSERT_FLOAT_EQ(left_clip.x, -TEST_ONE);

    /* Right edge maps to NDC x = +1 */
    vec4 right = vec4_create(10.0f, TEST_ZERO, -1.0f, TEST_ONE);
    vec4 right_clip = mat4_multiply_vec4(ortho, right);
    ASSERT_FLOAT_EQ(right_clip.x, TEST_ONE);

    /* Bottom edge maps to NDC y = -1 */
    vec4 bottom = vec4_create(TEST_ZERO, -5.0f, -1.0f, TEST_ONE);
    vec4 bottom_clip = mat4_multiply_vec4(ortho, bottom);
    ASSERT_FLOAT_EQ(bottom_clip.y, -TEST_ONE);

    /* Top edge maps to NDC y = +1 */
    vec4 top_pt = vec4_create(TEST_ZERO, 5.0f, -1.0f, TEST_ONE);
    vec4 top_clip = mat4_multiply_vec4(ortho, top_pt);
    ASSERT_FLOAT_EQ(top_clip.y, TEST_ONE);
    END_TEST();
}

static void test_mat4_orthographic_2d(void)
{
    TEST("mat4_orthographic 2D screen");
    /* Common 2D setup: pixel coordinates to NDC */
    mat4 ortho = mat4_orthographic(0.0f, 800.0f, 0.0f, 600.0f, -1.0f, 1.0f);

    /* Bottom-left corner (0, 0) -> NDC (-1, -1) */
    vec4 bl = mat4_multiply_vec4(ortho, vec4_create(TEST_ZERO, TEST_ZERO, TEST_ZERO, TEST_ONE));
    ASSERT_FLOAT_EQ(bl.x, -TEST_ONE);
    ASSERT_FLOAT_EQ(bl.y, -TEST_ONE);

    /* Top-right corner (800, 600) -> NDC (1, 1) */
    vec4 tr = mat4_multiply_vec4(ortho, vec4_create(800.0f, 600.0f, TEST_ZERO, TEST_ONE));
    ASSERT_FLOAT_EQ(tr.x, TEST_ONE);
    ASSERT_FLOAT_EQ(tr.y, TEST_ONE);

    /* Center (400, 300) -> NDC (0, 0) */
    vec4 ctr = mat4_multiply_vec4(ortho, vec4_create(400.0f, 300.0f, TEST_ZERO, TEST_ONE));
    ASSERT_FLOAT_EQ(ctr.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(ctr.y, TEST_ZERO);
    END_TEST();
}

static void test_mat4_multiply_identity(void)
{
    TEST("mat4_multiply with identity");
    mat4 m = mat4_translate(vec3_create(TEST_FIVE, TEST_THREE, TEST_TWO));
    mat4 identity = mat4_identity();
    mat4 result = mat4_multiply(m, identity);

    /* Should equal the original matrix */
    for (int i = 0; i < 16; i++) {
        ASSERT_FLOAT_EQ(result.m[i], m.m[i]);
    }
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("\n=== forge-gpu Math Library Tests ===\n");

    /* vec2 tests */
    SDL_Log("vec2 tests:");
    test_vec2_create();
    test_vec2_add();
    test_vec2_sub();
    test_vec2_scale();
    test_vec2_dot();
    test_vec2_length();
    test_vec2_normalize();
    test_vec2_lerp();

    /* vec3 tests */
    SDL_Log("\nvec3 tests:");
    test_vec3_create();
    test_vec3_add();
    test_vec3_sub();
    test_vec3_scale();
    test_vec3_dot();
    test_vec3_cross();
    test_vec3_length();
    test_vec3_normalize();
    test_vec3_lerp();

    /* vec4 tests */
    SDL_Log("\nvec4 tests:");
    test_vec4_create();
    test_vec4_add();
    test_vec4_sub();
    test_vec4_scale();
    test_vec4_dot();

    /* mat4 tests */
    SDL_Log("\nmat4 tests:");
    test_mat4_identity();
    test_mat4_translate();
    test_mat4_scale();
    test_mat4_rotate_x();
    test_mat4_rotate_y();
    test_mat4_rotate_z();
    test_mat4_look_at();
    test_mat4_perspective();
    test_mat4_orthographic();
    test_mat4_orthographic_corners();
    test_mat4_orthographic_2d();
    test_mat4_multiply();
    test_mat4_multiply_identity();

    /* Summary */
    SDL_Log("\n=== Test Summary ===");
    SDL_Log("Total:  %d", test_count);
    SDL_Log("Passed: %d", pass_count);
    SDL_Log("Failed: %d", fail_count);

    if (fail_count > 0) {
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "\nSome tests FAILED!");
        SDL_Quit();
        return 1;
    }

    SDL_Log("\nAll tests PASSED!");
    SDL_Quit();
    return 0;
}
