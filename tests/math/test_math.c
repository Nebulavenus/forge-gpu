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
#define TEST_NEG_ONE    (-1.0f)
#define TEST_ZERO       0.0f
#define TEST_ONE        1.0f
#define TEST_TWO        2.0f
#define TEST_THREE      3.0f
#define TEST_FOUR       4.0f
#define TEST_FIVE       5.0f
#define TEST_EIGHT      8.0f
#define TEST_TEN        10.0f
#define TEST_FIFTEEN    15.0f
#define TEST_256        256.0f
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

/* Projection test constants */
#define TEST_PROJ_FOV_DEG       60.0f
#define TEST_PROJ_ASPECT_W      16.0f
#define TEST_PROJ_ASPECT_H       9.0f
#define TEST_PROJ_NEAR           0.1f
#define TEST_PROJ_FAR          100.0f

/* mat4_perspective_from_planes near-plane bounds */
#define TEST_PLANES_L           -2.0f
#define TEST_PLANES_R            2.0f
#define TEST_PLANES_B           -1.5f
#define TEST_PLANES_T            1.5f
#define TEST_PLANES_NEAR         1.0f
#define TEST_PLANES_FAR        100.0f

/* mat4_perspective_from_planes depth test */
#define TEST_PLANES_DEPTH_NEAR   0.5f
#define TEST_PLANES_DEPTH_FAR   50.0f

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

/* Check if two mat3s are approximately equal */
static bool mat3_eq(mat3 a, mat3 b)
{
    for (int i = 0; i < 9; i++) {
        if (!float_eq(a.m[i], b.m[i])) return false;
    }
    return true;
}

/* Check if two mat4s are approximately equal */
static bool mat4_eq(mat4 a, mat4 b)
{
    for (int i = 0; i < 16; i++) {
        if (!float_eq(a.m[i], b.m[i])) return false;
    }
    return true;
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

#define ASSERT_MAT3_EQ(a, b) \
    if (!mat3_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "    FAIL: mat3 mismatch"); \
        fail_count++; \
        return; \
    }

#define ASSERT_MAT4_EQ(a, b) \
    if (!mat4_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "    FAIL: mat4 mismatch"); \
        fail_count++; \
        return; \
    }

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

/* ══════════════════════════════════════════════════════════════════════════
 * Scalar Helper Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_forge_log2f(void)
{
    TEST("forge_log2f");
    ASSERT_FLOAT_EQ(forge_log2f(TEST_ONE), TEST_ZERO);     /* 2^0 = 1 */
    ASSERT_FLOAT_EQ(forge_log2f(TEST_TWO), TEST_ONE);      /* 2^1 = 2 */
    ASSERT_FLOAT_EQ(forge_log2f(TEST_FOUR), TEST_TWO);     /* 2^2 = 4 */
    ASSERT_FLOAT_EQ(forge_log2f(TEST_EIGHT), TEST_THREE);  /* 2^3 = 8 */
    ASSERT_FLOAT_EQ(forge_log2f(TEST_256), TEST_EIGHT);    /* 2^8 = 256 */
    END_TEST();
}

static void test_forge_clampf(void)
{
    TEST("forge_clampf");
    /* Value within range — returns unchanged */
    ASSERT_FLOAT_EQ(forge_clampf(TEST_FIVE, TEST_ZERO, TEST_TEN), TEST_FIVE);
    /* Value below range — returns lo */
    ASSERT_FLOAT_EQ(forge_clampf(TEST_NEG_ONE, TEST_ZERO, TEST_TEN), TEST_ZERO);
    /* Value above range — returns hi */
    ASSERT_FLOAT_EQ(forge_clampf(TEST_FIFTEEN, TEST_ZERO, TEST_TEN), TEST_TEN);
    /* Value at boundaries — returns boundary */
    ASSERT_FLOAT_EQ(forge_clampf(TEST_ZERO, TEST_ZERO, TEST_TEN), TEST_ZERO);
    ASSERT_FLOAT_EQ(forge_clampf(TEST_TEN, TEST_ZERO, TEST_TEN), TEST_TEN);
    END_TEST();
}

static void test_forge_trilerpf(void)
{
    TEST("forge_trilerpf");
    /* All corners same value — result equals that value */
    ASSERT_FLOAT_EQ(forge_trilerpf(5, 5, 5, 5, 5, 5, 5, 5,
                                    TEST_HALF, TEST_HALF, TEST_HALF), TEST_FIVE);

    /* At corner (0,0,0) — returns c000 */
    ASSERT_FLOAT_EQ(forge_trilerpf(1, 2, 3, 4, 5, 6, 7, 8,
                                    0.0f, 0.0f, 0.0f), TEST_ONE);
    /* At corner (1,1,1) — returns c111 */
    ASSERT_FLOAT_EQ(forge_trilerpf(1, 2, 3, 4, 5, 6, 7, 8,
                                    TEST_ONE, TEST_ONE, TEST_ONE), TEST_EIGHT);

    /* Center: average of all 8 values */
    /* (1+2+3+4+5+6+7+8)/8 = 4.5 */
    ASSERT_FLOAT_EQ(forge_trilerpf(1, 2, 3, 4, 5, 6, 7, 8,
                                    TEST_HALF, TEST_HALF, TEST_HALF), TEST_FOUR + TEST_HALF);

    /* tz=0 should equal bilerp of front face */
    float front = forge_bilerpf(1, 2, 3, 4, 0.3f, 0.7f);
    ASSERT_FLOAT_EQ(forge_trilerpf(1, 2, 3, 4, 5, 6, 7, 8,
                                    0.3f, 0.7f, 0.0f), front);
    END_TEST();
}

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

static void test_vec3_trilerp(void)
{
    TEST("vec3_trilerp");
    /* All corners same — result equals that value */
    vec3 same = vec3_create(TEST_ONE, TEST_TWO, TEST_THREE);
    vec3 result = vec3_trilerp(same, same, same, same, same, same, same, same,
                                TEST_HALF, TEST_HALF, TEST_HALF);
    ASSERT_VEC3_EQ(result, same);

    /* At corner (0,0,0) — returns c000 */
    vec3 c000 = vec3_create(TEST_ONE, TEST_ZERO, TEST_ZERO);
    vec3 c111 = vec3_create(TEST_ZERO, TEST_ZERO, TEST_ONE);
    vec3 zero3 = TEST_V3_ZERO;
    vec3 corner = vec3_trilerp(c000, zero3, zero3, zero3,
                                zero3, zero3, zero3, c111,
                                0.0f, 0.0f, 0.0f);
    ASSERT_VEC3_EQ(corner, c000);

    /* At corner (1,1,1) — returns c111 */
    corner = vec3_trilerp(c000, zero3, zero3, zero3,
                           zero3, zero3, zero3, c111,
                           1.0f, 1.0f, 1.0f);
    ASSERT_VEC3_EQ(corner, c111);
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

static void test_vec4_trilerp(void)
{
    TEST("vec4_trilerp");
    /* All corners same — result equals that value */
    vec4 same = vec4_create(TEST_ONE, TEST_TWO, TEST_THREE, TEST_FOUR);
    vec4 result = vec4_trilerp(same, same, same, same, same, same, same, same,
                                TEST_HALF, TEST_HALF, TEST_HALF);
    ASSERT_VEC4_EQ(result, same);

    /* At corner (0,0,0) — returns c000 */
    vec4 c000 = vec4_create(TEST_ONE, TEST_ZERO, TEST_ZERO, TEST_ONE);
    vec4 c111 = vec4_create(TEST_ZERO, TEST_ZERO, TEST_ONE, TEST_ONE);
    vec4 zero4 = vec4_create(TEST_ZERO, TEST_ZERO, TEST_ZERO, TEST_ZERO);
    vec4 corner = vec4_trilerp(c000, zero4, zero4, zero4,
                                zero4, zero4, zero4, c111,
                                0.0f, 0.0f, 0.0f);
    ASSERT_VEC4_EQ(corner, c000);
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

/* ══════════════════════════════════════════════════════════════════════════
 * Projection Tests (Lesson 06)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vec3_perspective_divide(void)
{
    TEST("vec3_perspective_divide");
    /* A clip-space point with w=2 should have its x,y,z halved */
    vec4 clip = vec4_create(TEST_FOUR, -6.0f, TEST_ONE, TEST_TWO);
    vec3 ndc = vec3_perspective_divide(clip);
    ASSERT_FLOAT_EQ(ndc.x, TEST_TWO);
    ASSERT_FLOAT_EQ(ndc.y, -TEST_THREE);
    ASSERT_FLOAT_EQ(ndc.z, TEST_HALF);
    END_TEST();
}

static void test_vec3_perspective_divide_w_one(void)
{
    TEST("vec3_perspective_divide with w=1");
    /* When w=1 (orthographic), NDC = clip.xyz unchanged */
    vec4 clip = vec4_create(TEST_HALF, -0.3f, 0.8f, TEST_ONE);
    vec3 ndc = vec3_perspective_divide(clip);
    ASSERT_FLOAT_EQ(ndc.x, TEST_HALF);
    ASSERT_FLOAT_EQ(ndc.y, -0.3f);
    ASSERT_FLOAT_EQ(ndc.z, 0.8f);
    END_TEST();
}

static void test_mat4_perspective_from_planes(void)
{
    TEST("mat4_perspective_from_planes near-plane corners");
    /* Near-plane corners should map to NDC corners (-1,-1,0) to (1,1,0) */
    float l = TEST_PLANES_L, r = TEST_PLANES_R;
    float b = TEST_PLANES_B, t = TEST_PLANES_T;
    float n = TEST_PLANES_NEAR, f = TEST_PLANES_FAR;
    mat4 proj = mat4_perspective_from_planes(l, r, b, t, n, f);

    /* Bottom-left of near plane: (l, b, -n, 1) -> NDC (-1, -1, 0) */
    vec4 bl_clip = mat4_multiply_vec4(proj, vec4_create(l, b, -n, TEST_ONE));
    vec3 bl_ndc = vec3_perspective_divide(bl_clip);
    ASSERT_FLOAT_EQ(bl_ndc.x, -TEST_ONE);
    ASSERT_FLOAT_EQ(bl_ndc.y, -TEST_ONE);
    ASSERT_FLOAT_EQ(bl_ndc.z, TEST_ZERO);

    /* Top-right of near plane: (r, t, -n, 1) -> NDC (1, 1, 0) */
    vec4 tr_clip = mat4_multiply_vec4(proj, vec4_create(r, t, -n, TEST_ONE));
    vec3 tr_ndc = vec3_perspective_divide(tr_clip);
    ASSERT_FLOAT_EQ(tr_ndc.x, TEST_ONE);
    ASSERT_FLOAT_EQ(tr_ndc.y, TEST_ONE);
    ASSERT_FLOAT_EQ(tr_ndc.z, TEST_ZERO);
    END_TEST();
}

static void test_mat4_perspective_from_planes_symmetric(void)
{
    TEST("mat4_perspective_from_planes symmetric matches mat4_perspective");
    /* Symmetric case should match mat4_perspective */
    float fov = TEST_PROJ_FOV_DEG * FORGE_DEG2RAD;
    float aspect = TEST_PROJ_ASPECT_W / TEST_PROJ_ASPECT_H;
    float n = TEST_PROJ_NEAR, f = TEST_PROJ_FAR;

    float half_h = n * tanf(fov * 0.5f);
    float half_w = half_h * aspect;

    mat4 from_fov = mat4_perspective(fov, aspect, n, f);
    mat4 from_planes = mat4_perspective_from_planes(-half_w, half_w,
                                                      -half_h, half_h, n, f);

    for (int i = 0; i < 16; i++) {
        ASSERT_FLOAT_EQ(from_fov.m[i], from_planes.m[i]);
    }
    END_TEST();
}

static void test_mat4_perspective_from_planes_depth(void)
{
    TEST("mat4_perspective_from_planes depth mapping");
    /* Near plane center -> z=0, far plane center -> z=1 */
    float n = TEST_PLANES_DEPTH_NEAR, f = TEST_PLANES_DEPTH_FAR;
    mat4 proj = mat4_perspective_from_planes(-TEST_ONE, TEST_ONE,
                                              -TEST_ONE, TEST_ONE, n, f);

    /* Center of near plane: (0, 0, -n) */
    vec4 near_clip = mat4_multiply_vec4(proj,
        vec4_create(TEST_ZERO, TEST_ZERO, -n, TEST_ONE));
    vec3 near_ndc = vec3_perspective_divide(near_clip);
    ASSERT_FLOAT_EQ(near_ndc.z, TEST_ZERO);

    /* Center of far plane: (0, 0, -f) */
    vec4 far_clip = mat4_multiply_vec4(proj,
        vec4_create(TEST_ZERO, TEST_ZERO, -f, TEST_ONE));
    vec3 far_ndc = vec3_perspective_divide(far_clip);
    ASSERT_FLOAT_EQ(far_ndc.z, TEST_ONE);
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
 * mat3 Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_mat3_identity(void)
{
    TEST("mat3_identity");
    mat3 m = mat3_identity();
    ASSERT_FLOAT_EQ(m.m[0], TEST_ONE);
    ASSERT_FLOAT_EQ(m.m[4], TEST_ONE);
    ASSERT_FLOAT_EQ(m.m[8], TEST_ONE);
    ASSERT_FLOAT_EQ(m.m[1], TEST_ZERO);
    ASSERT_FLOAT_EQ(m.m[3], TEST_ZERO);
    END_TEST();
}

static void test_mat3_create(void)
{
    TEST("mat3_create");
    /* Row-major input, column-major storage */
    mat3 m = mat3_create(
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    );
    /* Column 0 */
    ASSERT_FLOAT_EQ(m.m[0], TEST_ONE);
    ASSERT_FLOAT_EQ(m.m[1], TEST_FOUR);
    ASSERT_FLOAT_EQ(m.m[2], 7.0f);
    /* Column 1 */
    ASSERT_FLOAT_EQ(m.m[3], TEST_TWO);
    ASSERT_FLOAT_EQ(m.m[4], TEST_FIVE);
    ASSERT_FLOAT_EQ(m.m[5], TEST_EIGHT);
    /* Column 2 */
    ASSERT_FLOAT_EQ(m.m[6], TEST_THREE);
    ASSERT_FLOAT_EQ(m.m[7], 6.0f);
    ASSERT_FLOAT_EQ(m.m[8], 9.0f);
    END_TEST();
}

static void test_mat3_multiply_vec3(void)
{
    TEST("mat3_multiply_vec3");
    /* Identity * v = v */
    mat3 id = mat3_identity();
    vec3 v = TEST_V3_A;
    vec3 result = mat3_multiply_vec3(id, v);
    ASSERT_VEC3_EQ(result, v);

    /* Scale by (2, 3, 1) */
    mat3 scl = mat3_scale(vec2_create(TEST_TWO, TEST_THREE));
    vec3 scaled = mat3_multiply_vec3(scl, vec3_create(TEST_ONE, TEST_ONE, TEST_ONE));
    ASSERT_VEC3_EQ(scaled, vec3_create(TEST_TWO, TEST_THREE, TEST_ONE));
    END_TEST();
}

static void test_mat3_multiply(void)
{
    TEST("mat3_multiply");
    /* Identity * M = M */
    mat3 id = mat3_identity();
    mat3 m = mat3_create(1, 2, 3, 4, 5, 6, 7, 8, 9);
    mat3 result = mat3_multiply(id, m);
    ASSERT_MAT3_EQ(result, m);

    /* M * Identity = M */
    result = mat3_multiply(m, id);
    ASSERT_MAT3_EQ(result, m);
    END_TEST();
}

static void test_mat3_transpose(void)
{
    TEST("mat3_transpose");
    mat3 m = mat3_create(1, 2, 3, 4, 5, 6, 7, 8, 9);
    mat3 t = mat3_transpose(m);
    mat3 expected = mat3_create(1, 4, 7, 2, 5, 8, 3, 6, 9);
    ASSERT_MAT3_EQ(t, expected);

    /* Double transpose = original */
    mat3 tt = mat3_transpose(t);
    ASSERT_MAT3_EQ(tt, m);
    END_TEST();
}

static void test_mat3_determinant(void)
{
    TEST("mat3_determinant");
    /* Identity determinant = 1 */
    ASSERT_FLOAT_EQ(mat3_determinant(mat3_identity()), TEST_ONE);

    /* Rotation determinant = 1 */
    mat3 rot = mat3_rotate(FORGE_PI / TEST_FOUR);
    ASSERT_FLOAT_EQ(mat3_determinant(rot), TEST_ONE);

    /* Scale by 2 in all axes: det = 2*2*1 = 4 */
    mat3 scl = mat3_scale(vec2_create(TEST_TWO, TEST_TWO));
    ASSERT_FLOAT_EQ(mat3_determinant(scl), TEST_FOUR);

    /* Singular matrix (row 3 = row 1): det = 0 */
    mat3 singular = mat3_create(1, 2, 3, 4, 5, 6, 1, 2, 3);
    ASSERT_FLOAT_EQ(mat3_determinant(singular), TEST_ZERO);
    END_TEST();
}

static void test_mat3_inverse(void)
{
    TEST("mat3_inverse");
    /* Inverse of identity = identity */
    mat3 id = mat3_identity();
    mat3 inv_id = mat3_inverse(id);
    ASSERT_MAT3_EQ(inv_id, id);

    /* M * M^-1 = I for a general invertible matrix */
    mat3 m = mat3_create(2, 1, 0, 0, 3, 1, 0, 0, 1);
    mat3 inv = mat3_inverse(m);
    mat3 product = mat3_multiply(m, inv);
    ASSERT_MAT3_EQ(product, id);

    /* Rotation inverse = transpose */
    mat3 rot = mat3_rotate(FORGE_PI / TEST_THREE);
    mat3 rot_inv = mat3_inverse(rot);
    mat3 rot_t = mat3_transpose(rot);
    ASSERT_MAT3_EQ(rot_inv, rot_t);
    END_TEST();
}

static void test_mat3_rotate(void)
{
    TEST("mat3_rotate");
    /* 90° rotation: X axis -> Y axis */
    mat3 rot = mat3_rotate(FORGE_PI / TEST_TWO);
    vec3 x_axis = TEST_V3_X_AXIS;
    vec3 result = mat3_multiply_vec3(rot, x_axis);
    ASSERT_FLOAT_EQ(result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.y, TEST_ONE);
    ASSERT_FLOAT_EQ(result.z, TEST_ZERO);
    END_TEST();
}

static void test_mat3_scale(void)
{
    TEST("mat3_scale");
    mat3 scl = mat3_scale(vec2_create(TEST_TWO, TEST_THREE));
    vec3 v = vec3_create(TEST_FOUR, TEST_FIVE, TEST_ONE);
    vec3 result = mat3_multiply_vec3(scl, v);
    ASSERT_VEC3_EQ(result, vec3_create(TEST_EIGHT, TEST_FIFTEEN, TEST_ONE));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * mat4 Additional Tests (transpose, determinant, inverse)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_mat4_transpose(void)
{
    TEST("mat4_transpose");
    mat4 m = mat4_translate(vec3_create(TEST_FIVE, TEST_THREE, TEST_TWO));
    mat4 t = mat4_transpose(m);

    /* Translation (column 3) should become row 3 */
    ASSERT_FLOAT_EQ(t.m[12], m.m[3]);
    ASSERT_FLOAT_EQ(t.m[13], m.m[7]);
    ASSERT_FLOAT_EQ(t.m[14], m.m[11]);

    /* Double transpose = original */
    mat4 tt = mat4_transpose(t);
    ASSERT_MAT4_EQ(tt, m);
    END_TEST();
}

static void test_mat4_determinant(void)
{
    TEST("mat4_determinant");
    /* Identity: det = 1 */
    ASSERT_FLOAT_EQ(mat4_determinant(mat4_identity()), TEST_ONE);

    /* Rotation: det = 1 */
    mat4 rot = mat4_rotate_y(FORGE_PI / TEST_FOUR);
    ASSERT_FLOAT_EQ(mat4_determinant(rot), TEST_ONE);

    /* Uniform scale by 2: det = 2^3 * 1 = 8 (4x4 with w=1 row) */
    mat4 scl = mat4_scale_uniform(TEST_TWO);
    ASSERT_FLOAT_EQ(mat4_determinant(scl), TEST_EIGHT);
    END_TEST();
}

static void test_mat4_inverse(void)
{
    TEST("mat4_inverse");
    mat4 id = mat4_identity();

    /* Inverse of identity = identity */
    mat4 inv_id = mat4_inverse(id);
    ASSERT_MAT4_EQ(inv_id, id);

    /* Translation: inverse should negate the offset */
    mat4 t = mat4_translate(vec3_create(TEST_FIVE, TEST_THREE, TEST_TWO));
    mat4 t_inv = mat4_inverse(t);
    mat4 product = mat4_multiply(t, t_inv);
    ASSERT_MAT4_EQ(product, id);

    /* Rotation: inverse = transpose */
    mat4 rot = mat4_rotate_z(FORGE_PI / TEST_THREE);
    mat4 rot_inv = mat4_inverse(rot);
    mat4 rot_t = mat4_transpose(rot);
    ASSERT_MAT4_EQ(rot_inv, rot_t);
    END_TEST();
}

static void test_mat4_from_mat3(void)
{
    TEST("mat4_from_mat3");
    mat3 rot3 = mat3_rotate(FORGE_PI / TEST_FOUR);
    mat4 rot4 = mat4_from_mat3(rot3);

    /* Upper-left 3×3 should match */
    ASSERT_FLOAT_EQ(rot4.m[0], rot3.m[0]);
    ASSERT_FLOAT_EQ(rot4.m[1], rot3.m[1]);
    ASSERT_FLOAT_EQ(rot4.m[4], rot3.m[3]);
    ASSERT_FLOAT_EQ(rot4.m[5], rot3.m[4]);

    /* Last row/column should be identity */
    ASSERT_FLOAT_EQ(rot4.m[3], TEST_ZERO);
    ASSERT_FLOAT_EQ(rot4.m[7], TEST_ZERO);
    ASSERT_FLOAT_EQ(rot4.m[11], TEST_ZERO);
    ASSERT_FLOAT_EQ(rot4.m[12], TEST_ZERO);
    ASSERT_FLOAT_EQ(rot4.m[13], TEST_ZERO);
    ASSERT_FLOAT_EQ(rot4.m[14], TEST_ZERO);
    ASSERT_FLOAT_EQ(rot4.m[15], TEST_ONE);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Quaternion Tests (Lesson 08)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Check if two quats are approximately equal */
static bool quat_eq(quat a, quat b)
{
    return float_eq(a.w, b.w) && float_eq(a.x, b.x) &&
           float_eq(a.y, b.y) && float_eq(a.z, b.z);
}

#define ASSERT_QUAT_EQ(a, b) \
    if (!quat_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: quat (%.4f,%.4f,%.4f,%.4f) != (%.4f,%.4f,%.4f,%.4f)", \
                     a.w, a.x, a.y, a.z, b.w, b.x, b.y, b.z); \
        fail_count++; \
        return; \
    }

static void test_quat_identity(void)
{
    TEST("quat_identity");
    quat id = quat_identity();
    ASSERT_FLOAT_EQ(id.w, TEST_ONE);
    ASSERT_FLOAT_EQ(id.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(id.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(id.z, TEST_ZERO);
    ASSERT_FLOAT_EQ(quat_length(id), TEST_ONE);
    END_TEST();
}

static void test_quat_conjugate(void)
{
    TEST("quat_conjugate");
    quat q = quat_create(1.0f, 2.0f, 3.0f, 4.0f);
    quat c = quat_conjugate(q);
    ASSERT_FLOAT_EQ(c.w, 1.0f);
    ASSERT_FLOAT_EQ(c.x, -2.0f);
    ASSERT_FLOAT_EQ(c.y, -3.0f);
    ASSERT_FLOAT_EQ(c.z, -4.0f);
    END_TEST();
}

static void test_quat_normalize(void)
{
    TEST("quat_normalize");
    quat q = quat_create(1.0f, 1.0f, 1.0f, 1.0f);  /* length = 2 */
    quat n = quat_normalize(q);
    ASSERT_FLOAT_EQ(quat_length(n), TEST_ONE);
    ASSERT_FLOAT_EQ(n.w, TEST_HALF);
    ASSERT_FLOAT_EQ(n.x, TEST_HALF);
    ASSERT_FLOAT_EQ(n.y, TEST_HALF);
    ASSERT_FLOAT_EQ(n.z, TEST_HALF);
    END_TEST();
}

static void test_quat_multiply_identity(void)
{
    TEST("quat_multiply with identity");
    quat q = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 4.0f);
    quat id = quat_identity();

    /* q * identity = q */
    quat r1 = quat_multiply(q, id);
    ASSERT_QUAT_EQ(r1, q);

    /* identity * q = q */
    quat r2 = quat_multiply(id, q);
    ASSERT_QUAT_EQ(r2, q);
    END_TEST();
}

static void test_quat_multiply_inverse(void)
{
    TEST("quat_multiply with inverse");
    quat q = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 3.0f);
    quat q_inv = quat_conjugate(q);
    quat product = quat_multiply(q, q_inv);

    /* q * q* should be identity */
    ASSERT_FLOAT_EQ(product.w, TEST_ONE);
    ASSERT_FLOAT_EQ(product.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(product.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(product.z, TEST_ZERO);
    END_TEST();
}

static void test_quat_from_axis_angle(void)
{
    TEST("quat_from_axis_angle");
    /* 90° around Y axis: q = (cos(45°), 0, sin(45°), 0) */
    quat q = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 2.0f);
    float expected_w = cosf(FORGE_PI / 4.0f);
    float expected_y = sinf(FORGE_PI / 4.0f);
    ASSERT_FLOAT_EQ(q.w, expected_w);
    ASSERT_FLOAT_EQ(q.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(q.y, expected_y);
    ASSERT_FLOAT_EQ(q.z, TEST_ZERO);
    ASSERT_FLOAT_EQ(quat_length(q), TEST_ONE);
    END_TEST();
}

static void test_quat_to_axis_angle_roundtrip(void)
{
    TEST("quat_to_axis_angle round-trip");
    vec3 axis = vec3_create(0.0f, 1.0f, 0.0f);
    float angle = 1.5f;

    quat q = quat_from_axis_angle(axis, angle);
    vec3 out_axis;
    float out_angle;
    quat_to_axis_angle(q, &out_axis, &out_angle);

    ASSERT_FLOAT_EQ(out_axis.x, axis.x);
    ASSERT_FLOAT_EQ(out_axis.y, axis.y);
    ASSERT_FLOAT_EQ(out_axis.z, axis.z);
    ASSERT_FLOAT_EQ(out_angle, angle);
    END_TEST();
}

static void test_quat_rotate_vec3_y(void)
{
    TEST("quat_rotate_vec3 around Y");
    /* 90° around Y should turn (1,0,0) into (0,0,-1) */
    quat q = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 2.0f);
    vec3 v = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 result = quat_rotate_vec3(q, v);
    ASSERT_FLOAT_EQ(result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.z, -TEST_ONE);
    END_TEST();
}

static void test_quat_rotate_vec3_x(void)
{
    TEST("quat_rotate_vec3 around X");
    /* 90° around X should turn (0,1,0) into (0,0,1) */
    quat q = quat_from_axis_angle(vec3_create(1, 0, 0), FORGE_PI / 2.0f);
    vec3 v = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 result = quat_rotate_vec3(q, v);
    ASSERT_FLOAT_EQ(result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.z, TEST_ONE);
    END_TEST();
}

static void test_quat_rotate_vec3_z(void)
{
    TEST("quat_rotate_vec3 around Z");
    /* 90° around Z should turn (1,0,0) into (0,1,0) */
    quat q = quat_from_axis_angle(vec3_create(0, 0, 1), FORGE_PI / 2.0f);
    vec3 v = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 result = quat_rotate_vec3(q, v);
    ASSERT_FLOAT_EQ(result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.y, TEST_ONE);
    ASSERT_FLOAT_EQ(result.z, TEST_ZERO);
    END_TEST();
}

static void test_quat_double_cover(void)
{
    TEST("quat double cover (q and -q same rotation)");
    quat q = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 3.0f);
    quat neg_q = quat_negate(q);
    vec3 v = vec3_create(1.0f, 2.0f, 3.0f);

    vec3 r1 = quat_rotate_vec3(q, v);
    vec3 r2 = quat_rotate_vec3(neg_q, v);
    ASSERT_VEC3_EQ(r1, r2);
    END_TEST();
}

static void test_quat_to_mat4(void)
{
    TEST("quat_to_mat4 vs mat4_rotate_y");
    float angle = FORGE_PI / 3.0f;
    quat q = quat_from_axis_angle(vec3_create(0, 1, 0), angle);
    mat4 from_quat = quat_to_mat4(q);
    mat4 from_mat = mat4_rotate_y(angle);
    ASSERT_MAT4_EQ(from_quat, from_mat);
    END_TEST();
}

static void test_quat_to_mat4_x(void)
{
    TEST("quat_to_mat4 vs mat4_rotate_x");
    float angle = FORGE_PI / 4.0f;
    quat q = quat_from_axis_angle(vec3_create(1, 0, 0), angle);
    mat4 from_quat = quat_to_mat4(q);
    mat4 from_mat = mat4_rotate_x(angle);
    ASSERT_MAT4_EQ(from_quat, from_mat);
    END_TEST();
}

static void test_quat_from_mat4_roundtrip(void)
{
    TEST("quat_from_mat4 round-trip");
    quat q = quat_from_euler(0.5f, 0.3f, 0.1f);
    mat4 m = quat_to_mat4(q);
    quat q2 = quat_from_mat4(m);

    /* q2 might be -q (double cover), so check both */
    vec3 v = vec3_create(1.0f, 2.0f, 3.0f);
    vec3 r1 = quat_rotate_vec3(q, v);
    vec3 r2 = quat_rotate_vec3(q2, v);
    ASSERT_VEC3_EQ(r1, r2);
    END_TEST();
}

static void test_quat_from_euler_identity(void)
{
    TEST("quat_from_euler all zeros = identity");
    quat q = quat_from_euler(0.0f, 0.0f, 0.0f);
    ASSERT_FLOAT_EQ(q.w, TEST_ONE);
    ASSERT_FLOAT_EQ(q.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(q.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(q.z, TEST_ZERO);
    END_TEST();
}

static void test_quat_from_euler_yaw_only(void)
{
    TEST("quat_from_euler yaw only matches axis-angle Y");
    float yaw = FORGE_PI / 4.0f;
    quat from_euler = quat_from_euler(yaw, 0.0f, 0.0f);
    quat from_axis = quat_from_axis_angle(vec3_create(0, 1, 0), yaw);
    ASSERT_QUAT_EQ(from_euler, from_axis);
    END_TEST();
}

static void test_quat_from_euler_pitch_only(void)
{
    TEST("quat_from_euler pitch only matches axis-angle X");
    float pitch = FORGE_PI / 6.0f;
    quat from_euler = quat_from_euler(0.0f, pitch, 0.0f);
    quat from_axis = quat_from_axis_angle(vec3_create(1, 0, 0), pitch);
    ASSERT_QUAT_EQ(from_euler, from_axis);
    END_TEST();
}

static void test_quat_euler_roundtrip(void)
{
    TEST("quat_to_euler round-trip");
    float yaw = 0.5f;
    float pitch = 0.3f;
    float roll = 0.1f;
    quat q = quat_from_euler(yaw, pitch, roll);
    vec3 euler = quat_to_euler(q);
    ASSERT_FLOAT_EQ(euler.x, yaw);
    ASSERT_FLOAT_EQ(euler.y, pitch);
    ASSERT_FLOAT_EQ(euler.z, roll);
    END_TEST();
}

static void test_quat_euler_vs_matrix(void)
{
    TEST("quat_from_euler matches matrix Ry*Rx*Rz");
    float yaw = 0.7f;
    float pitch = 0.4f;
    float roll = 0.2f;
    quat q = quat_from_euler(yaw, pitch, roll);
    mat4 from_quat = quat_to_mat4(q);
    mat4 from_mat = mat4_multiply(mat4_rotate_y(yaw),
                    mat4_multiply(mat4_rotate_x(pitch),
                                  mat4_rotate_z(roll)));
    ASSERT_MAT4_EQ(from_quat, from_mat);
    END_TEST();
}

static void test_quat_slerp_endpoints(void)
{
    TEST("quat_slerp endpoints");
    quat a = quat_from_axis_angle(vec3_create(0, 1, 0), 0.0f);
    quat b = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 2.0f);

    /* t=0 should return a */
    quat r0 = quat_slerp(a, b, 0.0f);
    ASSERT_QUAT_EQ(r0, a);

    /* t=1 should return b */
    quat r1 = quat_slerp(a, b, 1.0f);
    ASSERT_QUAT_EQ(r1, b);
    END_TEST();
}

static void test_quat_slerp_midpoint(void)
{
    TEST("quat_slerp midpoint");
    quat a = quat_from_axis_angle(vec3_create(0, 1, 0), 0.0f);
    quat b = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 2.0f);

    /* t=0.5 should be halfway — 45° around Y */
    quat mid = quat_slerp(a, b, 0.5f);
    quat expected = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 4.0f);
    ASSERT_QUAT_EQ(mid, expected);
    END_TEST();
}

static void test_quat_nlerp_endpoints(void)
{
    TEST("quat_nlerp endpoints");
    quat a = quat_from_axis_angle(vec3_create(0, 1, 0), 0.0f);
    quat b = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 2.0f);

    quat r0 = quat_nlerp(a, b, 0.0f);
    ASSERT_QUAT_EQ(r0, a);

    quat r1 = quat_nlerp(a, b, 1.0f);
    ASSERT_QUAT_EQ(r1, b);
    END_TEST();
}

static void test_vec3_rotate_axis_angle(void)
{
    TEST("vec3_rotate_axis_angle");
    /* 90° around Y: (1,0,0) -> (0,0,-1) */
    vec3 v = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 axis = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 result = vec3_rotate_axis_angle(v, axis, FORGE_PI / 2.0f);
    ASSERT_FLOAT_EQ(result.x, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(result.z, -TEST_ONE);
    END_TEST();
}

static void test_vec3_rotate_axis_angle_120(void)
{
    TEST("vec3_rotate_axis_angle 3x120 around diagonal");
    /* Three 120° rotations around (1,1,1) cycle X->Y->Z->X */
    vec3 axis = vec3_normalize(vec3_create(1, 1, 1));
    float angle = 120.0f * FORGE_DEG2RAD;
    vec3 v = vec3_create(1, 0, 0);
    vec3 v1 = vec3_rotate_axis_angle(v, axis, angle);
    vec3 v2 = vec3_rotate_axis_angle(v1, axis, angle);
    vec3 v3 = vec3_rotate_axis_angle(v2, axis, angle);

    /* Should be back to (1, 0, 0) */
    ASSERT_FLOAT_EQ(v3.x, TEST_ONE);
    ASSERT_FLOAT_EQ(v3.y, TEST_ZERO);
    ASSERT_FLOAT_EQ(v3.z, TEST_ZERO);
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

    /* Scalar helper tests */
    SDL_Log("Scalar helper tests:");
    test_forge_log2f();
    test_forge_clampf();
    test_forge_trilerpf();

    /* vec2 tests */
    SDL_Log("\nvec2 tests:");
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
    test_vec3_trilerp();

    /* vec4 tests */
    SDL_Log("\nvec4 tests:");
    test_vec4_create();
    test_vec4_add();
    test_vec4_sub();
    test_vec4_scale();
    test_vec4_dot();
    test_vec4_trilerp();

    /* mat3 tests */
    SDL_Log("\nmat3 tests:");
    test_mat3_identity();
    test_mat3_create();
    test_mat3_multiply_vec3();
    test_mat3_multiply();
    test_mat3_transpose();
    test_mat3_determinant();
    test_mat3_inverse();
    test_mat3_rotate();
    test_mat3_scale();

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
    test_vec3_perspective_divide();
    test_vec3_perspective_divide_w_one();
    test_mat4_perspective_from_planes();
    test_mat4_perspective_from_planes_symmetric();
    test_mat4_perspective_from_planes_depth();
    test_mat4_multiply();
    test_mat4_multiply_identity();
    test_mat4_transpose();
    test_mat4_determinant();
    test_mat4_inverse();
    test_mat4_from_mat3();

    /* Quaternion tests */
    SDL_Log("\nquat tests:");
    test_quat_identity();
    test_quat_conjugate();
    test_quat_normalize();
    test_quat_multiply_identity();
    test_quat_multiply_inverse();
    test_quat_from_axis_angle();
    test_quat_to_axis_angle_roundtrip();
    test_quat_rotate_vec3_y();
    test_quat_rotate_vec3_x();
    test_quat_rotate_vec3_z();
    test_quat_double_cover();
    test_quat_to_mat4();
    test_quat_to_mat4_x();
    test_quat_from_mat4_roundtrip();
    test_quat_from_euler_identity();
    test_quat_from_euler_yaw_only();
    test_quat_from_euler_pitch_only();
    test_quat_euler_roundtrip();
    test_quat_euler_vs_matrix();
    test_quat_slerp_endpoints();
    test_quat_slerp_midpoint();
    test_quat_nlerp_endpoints();
    test_vec3_rotate_axis_angle();
    test_vec3_rotate_axis_angle_120();

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
