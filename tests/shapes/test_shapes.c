/*
 * Shapes Library Tests
 *
 * Automated tests for common/shapes/forge_shapes.h
 * Verifies correctness of all procedural geometry generators.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <math.h>

#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

/* Epsilon for floating-point comparisons (account for rounding errors) */
#define EPSILON 0.0001f

/* Relaxed epsilon for radius checks where tessellation introduces error */
#define RELAXED_EPSILON 0.01f

/* Check if two floats are approximately equal */
static bool float_eq(float a, float b)
{
    return SDL_fabsf(a - b) < EPSILON;
}

/* Check if two floats are approximately equal with a custom tolerance */
static bool float_near(float a, float b, float tolerance)
{
    return SDL_fabsf(a - b) < tolerance;
}

/* Check if two vec3s are approximately equal */
static bool vec3_eq(vec3 a, vec3 b)
{
    return float_eq(a.x, b.x) && float_eq(a.y, b.y) && float_eq(a.z, b.z);
}

/* Test assertion macros */
#define TEST(name) \
    do { \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_FLOAT_EQ(a, b) \
    if (!float_eq(a, b)) { \
        SDL_Log( \
                     "    FAIL: Expected %.6f, got %.6f", b, a); \
        fail_count++; \
        return; \
    }

#define ASSERT_INT_EQ(a, b) \
    if ((a) != (b)) { \
        SDL_Log( \
                     "    FAIL: Expected %d, got %d", b, a); \
        fail_count++; \
        return; \
    }

#define ASSERT_TRUE(expr) \
    if (!(expr)) { \
        SDL_Log( \
                     "    FAIL: Assertion failed: %s", #expr); \
        fail_count++; \
        return; \
    }

#define ASSERT_VEC3_EQ(a, b) \
    if (!vec3_eq(a, b)) { \
        SDL_Log( \
                     "    FAIL: Expected (%.3f, %.3f, %.3f), got (%.3f, %.3f, %.3f)", \
                     b.x, b.y, b.z, a.x, a.y, a.z); \
        fail_count++; \
        return; \
    }

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

/* ══════════════════════════════════════════════════════════════════════════
 * Helper functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* Compute the length of a vec3 */
static float v3_length(vec3 v)
{
    return SDL_sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

/* Compute the XZ distance from the origin (ignoring Y) */
static float xz_distance(vec3 v)
{
    return SDL_sqrtf(v.x * v.x + v.z * v.z);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Sphere Tests (1-8)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_sphere_vertex_count(void)
{
    TEST("sphere_vertex_count");
    ForgeShape s = forge_shapes_sphere(32, 16);
    /* (slices+1) * (stacks+1) = 33 * 17 = 561 */
    ASSERT_INT_EQ(s.vertex_count, 561);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_sphere_index_count(void)
{
    TEST("sphere_index_count");
    ForgeShape s = forge_shapes_sphere(32, 16);
    /* Pole rows emit 1 triangle each (slices*3), middle rows emit 2 (slices*6)
     * = 2*slices*3 + (stacks-2)*slices*6 = 2*32*3 + 14*32*6 = 192+2688 = 2880 */
    ASSERT_INT_EQ(s.index_count, 2880);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_sphere_north_pole(void)
{
    TEST("sphere_north_pole");
    ForgeShape s = forge_shapes_sphere(32, 16);
    /* Search for a vertex at (0, 1, 0) — the north pole */
    bool found = false;
    vec3 north = { 0.0f, 1.0f, 0.0f };
    for (int i = 0; i < s.vertex_count; i++) {
        if (vec3_eq(s.positions[i], north)) {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_sphere_south_pole(void)
{
    TEST("sphere_south_pole");
    ForgeShape s = forge_shapes_sphere(32, 16);
    /* Search for a vertex at (0, -1, 0) — the south pole */
    bool found = false;
    vec3 south = { 0.0f, -1.0f, 0.0f };
    for (int i = 0; i < s.vertex_count; i++) {
        if (vec3_eq(s.positions[i], south)) {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_sphere_normals_unit_length(void)
{
    TEST("sphere_normals_unit_length");
    ForgeShape s = forge_shapes_sphere(32, 16);
    for (int i = 0; i < s.vertex_count; i++) {
        float len = v3_length(s.normals[i]);
        if (!float_eq(len, 1.0f)) {
            SDL_Log("    FAIL: Normal %d has length %.6f, expected 1.0", i, len);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_sphere_positions_on_unit_sphere(void)
{
    TEST("sphere_positions_on_unit_sphere");
    ForgeShape s = forge_shapes_sphere(32, 16);
    for (int i = 0; i < s.vertex_count; i++) {
        float len = v3_length(s.positions[i]);
        if (!float_eq(len, 1.0f)) {
            SDL_Log("    FAIL: Position %d has length %.6f, expected 1.0", i, len);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_sphere_uv_range(void)
{
    TEST("sphere_uv_range");
    ForgeShape s = forge_shapes_sphere(32, 16);
    for (int i = 0; i < s.vertex_count; i++) {
        float u = s.uvs[i].x;
        float v = s.uvs[i].y;
        if (u < -EPSILON || u > 1.0f + EPSILON ||
            v < -EPSILON || v > 1.0f + EPSILON) {
            SDL_Log("    FAIL: UV %d = (%.6f, %.6f) out of [0,1] range", i, u, v);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_sphere_winding_order(void)
{
    TEST("sphere_winding_order");
    ForgeShape s = forge_shapes_sphere(32, 16);
    int checked = 0;
    int triangle_count = s.index_count / 3;
    for (int t = 0; t < triangle_count; t++) {
        uint32_t i0 = s.indices[t * 3 + 0];
        uint32_t i1 = s.indices[t * 3 + 1];
        uint32_t i2 = s.indices[t * 3 + 2];
        vec3 v0 = s.positions[i0];
        vec3 v1 = s.positions[i1];
        vec3 v2 = s.positions[i2];

        /* Compute centroid */
        vec3 centroid = {
            (v0.x + v1.x + v2.x) / 3.0f,
            (v0.y + v1.y + v2.y) / 3.0f,
            (v0.z + v1.z + v2.z) / 3.0f
        };

        /* Compute edge vectors */
        vec3 e1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
        vec3 e2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };

        /* Cross product: face normal */
        vec3 face_normal = {
            e1.y * e2.z - e1.z * e2.y,
            e1.z * e2.x - e1.x * e2.z,
            e1.x * e2.y - e1.y * e2.x
        };

        /* Skip degenerate triangles — near the poles, two or more vertices
         * share the same 3D position, producing a near-zero cross product
         * whose direction is unreliable due to floating-point error */
        float face_len = v3_length(face_normal);
        if (face_len < 1e-4f) continue;

        float centroid_len = v3_length(centroid);
        if (centroid_len < 1e-4f) continue;

        /* Normalize face normal before dot test — unnormalized cross products
         * from thin triangles near the poles have magnitudes that can make
         * a tiny directional error look significant */
        float inv_fl = 1.0f / face_len;
        face_normal.x *= inv_fl;
        face_normal.y *= inv_fl;
        face_normal.z *= inv_fl;

        float inv_cl = 1.0f / centroid_len;
        vec3 centroid_dir = {centroid.x * inv_cl, centroid.y * inv_cl, centroid.z * inv_cl};

        /* Dot face normal with centroid direction — should be positive
         * for CCW winding (outward-facing on a sphere).  Use a small
         * negative tolerance for near-degenerate pole triangles. */
        float dot = face_normal.x * centroid_dir.x +
                    face_normal.y * centroid_dir.y +
                    face_normal.z * centroid_dir.z;

        if (dot < -0.1f) {
            SDL_Log("    FAIL: Triangle %d has negative winding (dot=%.6f)", t, dot);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
        checked++;
    }
    /* Ensure we actually checked a meaningful number of triangles */
    ASSERT_TRUE(checked > triangle_count / 2);
    forge_shapes_free(&s);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Icosphere Tests (9-12)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_icosphere_sub0_counts(void)
{
    TEST("icosphere_sub0_counts");
    ForgeShape s = forge_shapes_icosphere(0);
    /* Base icosahedron has 12 vertices; UV seam duplication adds extra
     * vertices at the anti-meridian to prevent texture wrapping artifacts. */
    ASSERT_INT_EQ(s.vertex_count, 20);
    ASSERT_INT_EQ(s.index_count, 60);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_icosphere_sub1_counts(void)
{
    TEST("icosphere_sub1_counts");
    ForgeShape s = forge_shapes_icosphere(1);
    /* 42 base vertices + UV seam duplicates */
    ASSERT_INT_EQ(s.vertex_count, 56);
    ASSERT_INT_EQ(s.index_count, 240);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_icosphere_positions_on_unit_sphere(void)
{
    TEST("icosphere_positions_on_unit_sphere");
    ForgeShape s = forge_shapes_icosphere(2);
    for (int i = 0; i < s.vertex_count; i++) {
        float len = v3_length(s.positions[i]);
        if (!float_eq(len, 1.0f)) {
            SDL_Log("    FAIL: Position %d has length %.6f, expected 1.0", i, len);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_icosphere_normals_match_positions(void)
{
    TEST("icosphere_normals_match_positions");
    ForgeShape s = forge_shapes_icosphere(2);
    for (int i = 0; i < s.vertex_count; i++) {
        /* Normal should equal normalize(position) — for a unit sphere,
         * that is just the position itself */
        float len = v3_length(s.positions[i]);
        vec3 expected = {
            s.positions[i].x / len,
            s.positions[i].y / len,
            s.positions[i].z / len
        };
        if (!vec3_eq(s.normals[i], expected)) {
            SDL_Log("    FAIL: Normal %d = (%.3f, %.3f, %.3f), expected (%.3f, %.3f, %.3f)",
                    i, s.normals[i].x, s.normals[i].y, s.normals[i].z,
                    expected.x, expected.y, expected.z);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Torus Tests (13-17)
 * ══════════════════════════════════════════════════════════════════════════ */

#define TORUS_SLICES     16
#define TORUS_STACKS      8
#define TORUS_MAJOR       1.0f
#define TORUS_TUBE        0.4f

static void test_torus_vertex_count(void)
{
    TEST("torus_vertex_count");
    ForgeShape s = forge_shapes_torus(TORUS_SLICES, TORUS_STACKS,
                                      TORUS_MAJOR, TORUS_TUBE);
    /* (slices+1) * (stacks+1) = 17 * 9 = 153 */
    int expected = (TORUS_SLICES + 1) * (TORUS_STACKS + 1);
    ASSERT_INT_EQ(s.vertex_count, expected);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_torus_inner_radius(void)
{
    TEST("torus_inner_radius");
    ForgeShape s = forge_shapes_torus(TORUS_SLICES, TORUS_STACKS,
                                      TORUS_MAJOR, TORUS_TUBE);
    /* Minimum XZ distance should approximate major - tube */
    float min_dist = 1e9f;
    for (int i = 0; i < s.vertex_count; i++) {
        float d = xz_distance(s.positions[i]);
        if (d < min_dist) min_dist = d;
    }
    float expected = TORUS_MAJOR - TORUS_TUBE;
    if (!float_near(min_dist, expected, RELAXED_EPSILON)) {
        SDL_Log("    FAIL: Min XZ distance %.6f, expected %.6f", min_dist, expected);
        fail_count++;
        forge_shapes_free(&s);
        return;
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_torus_outer_radius(void)
{
    TEST("torus_outer_radius");
    ForgeShape s = forge_shapes_torus(TORUS_SLICES, TORUS_STACKS,
                                      TORUS_MAJOR, TORUS_TUBE);
    /* Maximum XZ distance should approximate major + tube */
    float max_dist = 0.0f;
    for (int i = 0; i < s.vertex_count; i++) {
        float d = xz_distance(s.positions[i]);
        if (d > max_dist) max_dist = d;
    }
    float expected = TORUS_MAJOR + TORUS_TUBE;
    if (!float_near(max_dist, expected, RELAXED_EPSILON)) {
        SDL_Log("    FAIL: Max XZ distance %.6f, expected %.6f", max_dist, expected);
        fail_count++;
        forge_shapes_free(&s);
        return;
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_torus_normals_unit_length(void)
{
    TEST("torus_normals_unit_length");
    ForgeShape s = forge_shapes_torus(TORUS_SLICES, TORUS_STACKS,
                                      TORUS_MAJOR, TORUS_TUBE);
    for (int i = 0; i < s.vertex_count; i++) {
        float len = v3_length(s.normals[i]);
        if (!float_eq(len, 1.0f)) {
            SDL_Log("    FAIL: Normal %d has length %.6f, expected 1.0", i, len);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_torus_y_extent(void)
{
    TEST("torus_y_extent");
    ForgeShape s = forge_shapes_torus(TORUS_SLICES, TORUS_STACKS,
                                      TORUS_MAJOR, TORUS_TUBE);
    for (int i = 0; i < s.vertex_count; i++) {
        float y = s.positions[i].y;
        if (y < -(TORUS_TUBE + EPSILON) || y > (TORUS_TUBE + EPSILON)) {
            SDL_Log("    FAIL: Position %d Y=%.6f outside [-%.4f, %.4f]",
                    i, y, TORUS_TUBE, TORUS_TUBE);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Plane Tests (18-22)
 * ══════════════════════════════════════════════════════════════════════════ */

#define PLANE_SLICES 4
#define PLANE_STACKS 4

static void test_plane_vertex_count(void)
{
    TEST("plane_vertex_count");
    ForgeShape s = forge_shapes_plane(PLANE_SLICES, PLANE_STACKS);
    /* (4+1) * (4+1) = 25 */
    ASSERT_INT_EQ(s.vertex_count, 25);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_plane_index_count(void)
{
    TEST("plane_index_count");
    ForgeShape s = forge_shapes_plane(PLANE_SLICES, PLANE_STACKS);
    /* 4 * 4 * 6 = 96 */
    ASSERT_INT_EQ(s.index_count, 96);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_plane_all_normals_up(void)
{
    TEST("plane_all_normals_up");
    ForgeShape s = forge_shapes_plane(PLANE_SLICES, PLANE_STACKS);
    vec3 up = { 0.0f, 1.0f, 0.0f };
    for (int i = 0; i < s.vertex_count; i++) {
        if (!vec3_eq(s.normals[i], up)) {
            SDL_Log("    FAIL: Normal %d = (%.3f, %.3f, %.3f), expected (0, 1, 0)",
                    i, s.normals[i].x, s.normals[i].y, s.normals[i].z);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_plane_y_is_zero(void)
{
    TEST("plane_y_is_zero");
    ForgeShape s = forge_shapes_plane(PLANE_SLICES, PLANE_STACKS);
    for (int i = 0; i < s.vertex_count; i++) {
        if (!float_eq(s.positions[i].y, 0.0f)) {
            SDL_Log("    FAIL: Position %d Y=%.6f, expected 0.0", i, s.positions[i].y);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_plane_uv_corners(void)
{
    TEST("plane_uv_corners");
    ForgeShape s = forge_shapes_plane(PLANE_SLICES, PLANE_STACKS);
    /* Check that UV values span the full [0,1] range */
    float min_u = 1.0f, max_u = 0.0f;
    float min_v = 1.0f, max_v = 0.0f;
    for (int i = 0; i < s.vertex_count; i++) {
        float u = s.uvs[i].x;
        float v = s.uvs[i].y;
        if (u < min_u) min_u = u;
        if (u > max_u) max_u = u;
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
    }
    if (!float_eq(min_u, 0.0f)) {
        SDL_Log("    FAIL: Min U = %.6f, expected 0.0", min_u);
        fail_count++;
        forge_shapes_free(&s);
        return;
    }
    if (!float_eq(max_u, 1.0f)) {
        SDL_Log("    FAIL: Max U = %.6f, expected 1.0", max_u);
        fail_count++;
        forge_shapes_free(&s);
        return;
    }
    if (!float_eq(min_v, 0.0f)) {
        SDL_Log("    FAIL: Min V = %.6f, expected 0.0", min_v);
        fail_count++;
        forge_shapes_free(&s);
        return;
    }
    if (!float_eq(max_v, 1.0f)) {
        SDL_Log("    FAIL: Max V = %.6f, expected 1.0", max_v);
        fail_count++;
        forge_shapes_free(&s);
        return;
    }
    forge_shapes_free(&s);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Cube Tests (23-25)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_cube_vertex_count(void)
{
    TEST("cube_vertex_count");
    ForgeShape s = forge_shapes_cube(1, 1);
    /* 6 faces * (1+1) * (1+1) = 6 * 4 = 24 */
    ASSERT_INT_EQ(s.vertex_count, 24);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_cube_face_normals_axis_aligned(void)
{
    TEST("cube_face_normals_axis_aligned");
    ForgeShape s = forge_shapes_cube(1, 1);
    for (int i = 0; i < s.vertex_count; i++) {
        vec3 n = s.normals[i];
        /* Each component must be -1, 0, or 1 */
        bool x_ok = float_eq(n.x, -1.0f) || float_eq(n.x, 0.0f) || float_eq(n.x, 1.0f);
        bool y_ok = float_eq(n.y, -1.0f) || float_eq(n.y, 0.0f) || float_eq(n.y, 1.0f);
        bool z_ok = float_eq(n.z, -1.0f) || float_eq(n.z, 0.0f) || float_eq(n.z, 1.0f);
        if (!x_ok || !y_ok || !z_ok) {
            SDL_Log("    FAIL: Normal %d = (%.3f, %.3f, %.3f) is not axis-aligned",
                    i, n.x, n.y, n.z);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
        /* Also verify it is a unit normal */
        float len = v3_length(n);
        if (!float_eq(len, 1.0f)) {
            SDL_Log("    FAIL: Normal %d has length %.6f, expected 1.0", i, len);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_cube_positions_in_unit_box(void)
{
    TEST("cube_positions_in_unit_box");
    ForgeShape s = forge_shapes_cube(1, 1);
    for (int i = 0; i < s.vertex_count; i++) {
        vec3 p = s.positions[i];
        if (p.x < -(1.0f + EPSILON) || p.x > (1.0f + EPSILON) ||
            p.y < -(1.0f + EPSILON) || p.y > (1.0f + EPSILON) ||
            p.z < -(1.0f + EPSILON) || p.z > (1.0f + EPSILON)) {
            SDL_Log("    FAIL: Position %d = (%.3f, %.3f, %.3f) outside [-1, 1]",
                    i, p.x, p.y, p.z);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Flat Normals Test (26)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_flat_normals_constant_per_triangle(void)
{
    TEST("flat_normals_constant_per_triangle");
    /* Generate a sphere and then compute flat normals — every triangle's
     * three vertices should share an identical normal afterward */
    ForgeShape s = forge_shapes_sphere(8, 4);
    forge_shapes_compute_flat_normals(&s);

    int triangle_count = s.index_count / 3;
    for (int t = 0; t < triangle_count; t++) {
        uint32_t i0 = s.indices[t * 3 + 0];
        uint32_t i1 = s.indices[t * 3 + 1];
        uint32_t i2 = s.indices[t * 3 + 2];
        vec3 n0 = s.normals[i0];
        vec3 n1 = s.normals[i1];
        vec3 n2 = s.normals[i2];
        if (!vec3_eq(n0, n1) || !vec3_eq(n0, n2)) {
            SDL_Log("    FAIL: Triangle %d normals differ: "
                    "(%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f)",
                    t, n0.x, n0.y, n0.z, n1.x, n1.y, n1.z, n2.x, n2.y, n2.z);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Memory Tests (27-28)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_free_zeroes_pointers(void)
{
    TEST("free_zeroes_pointers");
    ForgeShape s = forge_shapes_sphere(8, 4);
    /* Verify the shape has data before freeing */
    ASSERT_TRUE(s.positions != NULL);
    ASSERT_TRUE(s.normals != NULL);
    ASSERT_TRUE(s.uvs != NULL);
    ASSERT_TRUE(s.indices != NULL);
    ASSERT_TRUE(s.vertex_count > 0);
    ASSERT_TRUE(s.index_count > 0);

    forge_shapes_free(&s);

    /* After freeing, everything should be zeroed */
    ASSERT_TRUE(s.positions == NULL);
    ASSERT_TRUE(s.normals == NULL);
    ASSERT_TRUE(s.uvs == NULL);
    ASSERT_TRUE(s.indices == NULL);
    ASSERT_INT_EQ(s.vertex_count, 0);
    ASSERT_INT_EQ(s.index_count, 0);
    END_TEST();
}

static void test_free_safe_on_zeroed_shape(void)
{
    TEST("free_safe_on_zeroed_shape");
    /* A zero-initialized ForgeShape should be safe to free without crashing */
    ForgeShape s;
    SDL_memset(&s, 0, sizeof(s));
    forge_shapes_free(&s);
    /* If we get here without crashing, the test passes */
    ASSERT_TRUE(s.positions == NULL);
    ASSERT_TRUE(s.vertex_count == 0);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Cylinder Tests (29-31)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_cylinder_vertex_count(void)
{
    TEST("cylinder_vertex_count");
    ForgeShape s = forge_shapes_cylinder(16, 4);
    /* (16+1) * (4+1) = 17 * 5 = 85 */
    ASSERT_INT_EQ(s.vertex_count, 85);
    ASSERT_INT_EQ(s.index_count, 384);  /* 16 * 4 * 6 = 384 */
    forge_shapes_free(&s);
    END_TEST();
}

static void test_cylinder_normals_unit_length(void)
{
    TEST("cylinder_normals_unit_length");
    ForgeShape s = forge_shapes_cylinder(16, 4);
    for (int i = 0; i < s.vertex_count; i++) {
        float len = v3_length(s.normals[i]);
        if (!float_eq(len, 1.0f)) {
            SDL_Log("    FAIL: Normal %d has length %.6f, expected 1.0", i, len);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

static void test_cylinder_uv_range(void)
{
    TEST("cylinder_uv_range");
    ForgeShape s = forge_shapes_cylinder(16, 4);
    for (int i = 0; i < s.vertex_count; i++) {
        float u = s.uvs[i].x;
        float v = s.uvs[i].y;
        if (u < -EPSILON || u > 1.0f + EPSILON ||
            v < -EPSILON || v > 1.0f + EPSILON) {
            SDL_Log("    FAIL: UV %d = (%.6f, %.6f) out of [0,1] range", i, u, v);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Cone Tests (32-34)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_cone_vertex_count(void)
{
    TEST("cone_vertex_count");
    ForgeShape s = forge_shapes_cone(12, 3);
    /* (12+1) * (3+1) = 13 * 4 = 52 */
    ASSERT_INT_EQ(s.vertex_count, 52);
    ASSERT_INT_EQ(s.index_count, 216);  /* 12 * 3 * 6 = 216 */
    forge_shapes_free(&s);
    END_TEST();
}

static void test_cone_apex_radius_zero(void)
{
    TEST("cone_apex_radius_zero");
    ForgeShape s = forge_shapes_cone(12, 3);
    /* The top row (last row, stack == stacks) should have Y near +1
     * and XZ radius near 0 */
    int stacks = 3;
    int slices = 12;
    bool found_apex = false;
    for (int i = 0; i < s.vertex_count; i++) {
        /* Apex vertices are at Y ~= +1 */
        if (float_near(s.positions[i].y, 1.0f, RELAXED_EPSILON)) {
            float r = xz_distance(s.positions[i]);
            if (!float_near(r, 0.0f, RELAXED_EPSILON)) {
                SDL_Log("    FAIL: Apex vertex %d has XZ radius %.6f, expected ~0",
                        i, r);
                fail_count++;
                forge_shapes_free(&s);
                return;
            }
            found_apex = true;
        }
    }
    (void)stacks;
    (void)slices;
    ASSERT_TRUE(found_apex);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_cone_uv_range(void)
{
    TEST("cone_uv_range");
    ForgeShape s = forge_shapes_cone(12, 3);
    for (int i = 0; i < s.vertex_count; i++) {
        float u = s.uvs[i].x;
        float v = s.uvs[i].y;
        if (u < -EPSILON || u > 1.0f + EPSILON ||
            v < -EPSILON || v > 1.0f + EPSILON) {
            SDL_Log("    FAIL: UV %d = (%.6f, %.6f) out of [0,1] range", i, u, v);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Capsule Tests (35-36)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_capsule_vertex_count(void)
{
    TEST("capsule_vertex_count");
    ForgeShape s = forge_shapes_capsule(12, 2, 4, 1.0f);
    /* total_rows = cap_stacks + stacks + cap_stacks + 1 = 4 + 2 + 4 + 1 = 11
     * vertex_count = (slices+1) * total_rows = 13 * 11 = 143 */
    ASSERT_INT_EQ(s.vertex_count, 143);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_capsule_normals_unit_length(void)
{
    TEST("capsule_normals_unit_length");
    ForgeShape s = forge_shapes_capsule(12, 2, 4, 1.0f);
    for (int i = 0; i < s.vertex_count; i++) {
        float len = v3_length(s.normals[i]);
        if (!float_eq(len, 1.0f)) {
            SDL_Log("    FAIL: Normal %d has length %.6f, expected 1.0", i, len);
            fail_count++;
            forge_shapes_free(&s);
            return;
        }
    }
    forge_shapes_free(&s);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Merge Tests (37)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_merge_counts(void)
{
    TEST("merge_counts");
    ForgeShape plane = forge_shapes_plane(1, 1);
    ForgeShape cube  = forge_shapes_cube(1, 1);
    int expected_verts   = plane.vertex_count + cube.vertex_count;
    int expected_indices = plane.index_count + cube.index_count;

    ForgeShape shapes[2] = { plane, cube };
    ForgeShape merged = forge_shapes_merge(shapes, 2);

    ASSERT_INT_EQ(merged.vertex_count, expected_verts);
    ASSERT_INT_EQ(merged.index_count, expected_indices);

    forge_shapes_free(&merged);
    forge_shapes_free(&plane);
    forge_shapes_free(&cube);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Invalid Parameter Tests (38-41)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_invalid_sphere_returns_empty(void)
{
    TEST("invalid_sphere_returns_empty");
    ForgeShape s = forge_shapes_sphere(0, 0);
    ASSERT_INT_EQ(s.vertex_count, 0);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_invalid_plane_returns_empty(void)
{
    TEST("invalid_plane_returns_empty");
    ForgeShape s = forge_shapes_plane(0, 0);
    ASSERT_INT_EQ(s.vertex_count, 0);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_invalid_icosphere_returns_empty(void)
{
    TEST("invalid_icosphere_returns_empty");
    ForgeShape s = forge_shapes_icosphere(-1);
    ASSERT_INT_EQ(s.vertex_count, 0);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_free_empty_shape_safe(void)
{
    TEST("free_empty_shape_safe");
    /* Free the shape returned by an invalid call — should not crash */
    ForgeShape s = forge_shapes_sphere(0, 0);
    forge_shapes_free(&s);
    /* If we get here without crashing, the test passes */
    ASSERT_TRUE(s.positions == NULL);
    ASSERT_INT_EQ(s.vertex_count, 0);
    END_TEST();
}

static void test_invalid_cylinder_returns_empty(void)
{
    TEST("invalid_cylinder_returns_empty");
    ForgeShape s = forge_shapes_cylinder(2, 1);  /* slices < 3 */
    ASSERT_INT_EQ(s.vertex_count, 0);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_invalid_cone_returns_empty(void)
{
    TEST("invalid_cone_returns_empty");
    ForgeShape s = forge_shapes_cone(2, 0);  /* slices < 3 and stacks < 1 */
    ASSERT_INT_EQ(s.vertex_count, 0);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_invalid_torus_returns_empty(void)
{
    TEST("invalid_torus_returns_empty");
    ForgeShape s = forge_shapes_torus(2, 2, 1.0f, 0.3f);  /* slices/stacks < 3 */
    ASSERT_INT_EQ(s.vertex_count, 0);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_invalid_capsule_returns_empty(void)
{
    TEST("invalid_capsule_returns_empty");
    ForgeShape s = forge_shapes_capsule(2, 1, 1, 1.0f);  /* slices < 3 */
    ASSERT_INT_EQ(s.vertex_count, 0);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_invalid_cube_returns_empty(void)
{
    TEST("invalid_cube_returns_empty");
    ForgeShape s = forge_shapes_cube(0, 0);  /* slices/stacks < 1 */
    ASSERT_INT_EQ(s.vertex_count, 0);
    forge_shapes_free(&s);
    END_TEST();
}

static void test_invalid_merge_returns_empty(void)
{
    TEST("invalid_merge_returns_empty");
    ForgeShape s = forge_shapes_merge(NULL, 0);
    ASSERT_INT_EQ(s.vertex_count, 0);
    ASSERT_INT_EQ(s.index_count, 0);
    forge_shapes_free(&s);
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

    SDL_Log("\n=== forge-gpu Shapes Library Tests ===\n");

    /* ── Sphere (8 tests) ── */
    SDL_Log("Sphere tests:");
    test_sphere_vertex_count();
    test_sphere_index_count();
    test_sphere_north_pole();
    test_sphere_south_pole();
    test_sphere_normals_unit_length();
    test_sphere_positions_on_unit_sphere();
    test_sphere_uv_range();
    test_sphere_winding_order();

    /* ── Icosphere (4 tests) ── */
    SDL_Log("\nIcosphere tests:");
    test_icosphere_sub0_counts();
    test_icosphere_sub1_counts();
    test_icosphere_positions_on_unit_sphere();
    test_icosphere_normals_match_positions();

    /* ── Torus (5 tests) ── */
    SDL_Log("\nTorus tests:");
    test_torus_vertex_count();
    test_torus_inner_radius();
    test_torus_outer_radius();
    test_torus_normals_unit_length();
    test_torus_y_extent();

    /* ── Plane (5 tests) ── */
    SDL_Log("\nPlane tests:");
    test_plane_vertex_count();
    test_plane_index_count();
    test_plane_all_normals_up();
    test_plane_y_is_zero();
    test_plane_uv_corners();

    /* ── Cube (3 tests) ── */
    SDL_Log("\nCube tests:");
    test_cube_vertex_count();
    test_cube_face_normals_axis_aligned();
    test_cube_positions_in_unit_box();

    /* ── Flat normals (1 test) ── */
    SDL_Log("\nFlat normals tests:");
    test_flat_normals_constant_per_triangle();

    /* ── Memory (2 tests) ── */
    SDL_Log("\nMemory tests:");
    test_free_zeroes_pointers();
    test_free_safe_on_zeroed_shape();

    /* ── Cylinder (3 tests) ── */
    SDL_Log("\nCylinder tests:");
    test_cylinder_vertex_count();
    test_cylinder_normals_unit_length();
    test_cylinder_uv_range();

    /* ── Cone (3 tests) ── */
    SDL_Log("\nCone tests:");
    test_cone_vertex_count();
    test_cone_apex_radius_zero();
    test_cone_uv_range();

    /* ── Capsule (2 tests) ── */
    SDL_Log("\nCapsule tests:");
    test_capsule_vertex_count();
    test_capsule_normals_unit_length();

    /* ── Merge (1 test) ── */
    SDL_Log("\nMerge tests:");
    test_merge_counts();

    /* ── Invalid parameters (10 tests) ── */
    SDL_Log("\nInvalid parameter tests:");
    test_invalid_sphere_returns_empty();
    test_invalid_plane_returns_empty();
    test_invalid_icosphere_returns_empty();
    test_free_empty_shape_safe();
    test_invalid_cylinder_returns_empty();
    test_invalid_cone_returns_empty();
    test_invalid_torus_returns_empty();
    test_invalid_capsule_returns_empty();
    test_invalid_cube_returns_empty();
    test_invalid_merge_returns_empty();

    /* ── Summary ── */
    SDL_Log("\n=== Test Summary ===");
    SDL_Log("Total:  %d", test_count);
    SDL_Log("Passed: %d", pass_count);
    SDL_Log("Failed: %d", fail_count);

    if (fail_count > 0) {
        SDL_Log("\nSome tests FAILED!");
        SDL_Quit();
        return 1;
    }

    SDL_Log("\nAll tests PASSED!");
    SDL_Quit();
    return 0;
}
