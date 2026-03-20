/*
 * GJK Intersection Tests
 *
 * Tests for forge_physics_gjk_* functions in common/physics/forge_physics.h
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Shared test constants ─────────────────────────────────────────────── */

/* Shape dimensions */
/* Portable INF without <math.h> — float overflow produces +INF per IEEE 754 */
static const float GJK_TEST_INF = 1e38f * 1e38f;

#define GJK_SPHERE_RADIUS       1.0f
#define GJK_BOX_HALF            1.0f    /* unit cube half-extent per axis */
#define GJK_CAPSULE_RADIUS      0.5f
#define GJK_CAPSULE_HALF_H      1.0f

/* Separation distances */
#define GJK_OVERLAP_DIST        0.5f    /* close enough to overlap unit shapes */
#define GJK_SEPARATED_DIST      5.0f    /* clearly separated for unit shapes */
#define GJK_FAR_DIST            8.0f    /* well beyond any shape pair sum */
#define GJK_LARGE_DIST       1000.0f    /* extreme separation test */
#define GJK_TOUCH_DIST          2.0f    /* sum of two GJK_SPHERE_RADIUS */
#define GJK_NEAR_MISS_DIST      2.01f   /* just past GJK_TOUCH_DIST */
#define GJK_SUPPORT_SEP         1.5f    /* support function sphere test */
#define GJK_SUPPORT_BOX_SEP     3.0f    /* support function box test */
#define GJK_CONVERGE_SEP        1.2f    /* convergence test separation */

/* Deep penetration test */
#define GJK_LARGE_RADIUS        5.0f
#define GJK_SMALL_RADIUS        0.5f

/* Rotated OBB test */
#define GJK_OBB_ROTATION_DEG   45.0f
#define GJK_OBB_BOUNDARY_SEP    2.1f    /* just past AA sum (2.0), within 45° rotated reach */

/* Determinism test */
#define GJK_DETERM_BOX_HALF     0.8f
#define GJK_DETERM_PB_X         1.3f
#define GJK_DETERM_PB_Y         0.2f
#define GJK_DETERM_PB_Z         0.1f

/* Convergence bounds */
#define GJK_FAST_ITER_LIMIT    10       /* simple overlap converges below this */

/* Invalid shape test */
#define GJK_INVALID_SHAPE_TYPE  999     /* sentinel for invalid shape type */

/* Non-unit quaternion test */
#define GJK_QUAT_SCALE          2.0f    /* non-unit quaternion scaling factor */

/* Rigid body test parameters */
#define GJK_BODY_MASS           1.0f
#define GJK_BODY_DRAG           0.01f
#define GJK_BODY_ANG_DRAG       0.01f
#define GJK_BODY_RESTIT         0.5f

/* Support function validation tests */
#define GJK_SUPPORT_ZERO_DIR_EPS  1e-7f  /* below GJK_EPSILON — zero-length direction */

/* Coplanar / degenerate tetrahedron tests (division-by-zero guards) */
#define GJK_FLAT_HALF_THIN      1e-6f   /* near-zero half-extent for flat boxes */
#define GJK_FLAT_HALF_WIDE      1.0f    /* normal half-extent for wide axes */
#define GJK_FLAT_OFFSET_SMALL   0.5f    /* overlapping offset for flat pairs */
#define GJK_FLAT_OFFSET_EXACT   2.0f    /* exact touching distance (2 * WIDE) */
#define GJK_FLAT_SEP_DIST       5.0f    /* clearly separated flat boxes */
#define GJK_FLAT_MICRO_HALF     1e-5f   /* at SHAPE_MIN_DIM boundary */
#define GJK_DEGEN_ITER_COUNT    200     /* iteration count for stability loop */
#define GJK_DEGEN_STEP_OFFSET   0.017f  /* small step offset per iteration */
#define GJK_FLAT_DIAG_SEP       1.5f    /* diagonal separation for multi-axis test */

/* ── Helpers ────────────────────────────────────────────────────────────── */

/* Run GJK intersection test with identity orientations */
static ForgePhysicsGJKResult gjk_test(
    ForgePhysicsCollisionShape *sa, vec3 pa,
    ForgePhysicsCollisionShape *sb, vec3 pb)
{
    quat identity = quat_identity();
    return forge_physics_gjk_intersect(sa, pa, identity, sb, pb, identity);
}

/* Assert that a GJK vertex is the NaN failure sentinel (all components NaN).
 * forge_physics_gjk_support returns NaN-sentinel on invalid input so callers
 * can distinguish failure from a legitimate zero-point (coincident shapes). */
#define ASSERT_VERTEX_INVALID(v) do { \
    ASSERT_TRUE(SDL_isnan((v).point.x)); \
    ASSERT_TRUE(SDL_isnan((v).point.y)); \
    ASSERT_TRUE(SDL_isnan((v).point.z)); \
    ASSERT_TRUE(SDL_isnan((v).sup_a.x)); \
    ASSERT_TRUE(SDL_isnan((v).sup_a.y)); \
    ASSERT_TRUE(SDL_isnan((v).sup_a.z)); \
    ASSERT_TRUE(SDL_isnan((v).sup_b.x)); \
    ASSERT_TRUE(SDL_isnan((v).sup_b.y)); \
    ASSERT_TRUE(SDL_isnan((v).sup_b.z)); \
} while (0)

/* Check that a vec3 has no NaN or Inf components */
static bool vec3_is_finite(vec3 v)
{
    return forge_isfinite((double)v.x) &&
           forge_isfinite((double)v.y) &&
           forge_isfinite((double)v.z);
}

/* Check that every vertex in the simplex has finite coordinates */
static bool simplex_is_finite(const ForgePhysicsGJKSimplex *s)
{
    if (!s) return false;
    if (s->count < 0 || s->count > 4) return false;
    for (int i = 0; i < s->count; i++) {
        if (!vec3_is_finite(s->verts[i].point)) return false;
        if (!vec3_is_finite(s->verts[i].sup_a))  return false;
        if (!vec3_is_finite(s->verts[i].sup_b))  return false;
    }
    return true;
}

/* ── Tests: Minkowski support ───────────────────────────────────────────── */

static void test_gjk_support_sphere_pair(void)
{
    TEST("GJK_support_sphere_pair — exact support vertices for +X direction");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    vec3 pa       = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb       = vec3_create(GJK_SUPPORT_SEP, 0.0f, 0.0f);
    quat identity = quat_identity();
    vec3 dir      = vec3_create(1.0f, 0.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, pa, identity, &sb, pb, identity, dir);

    /* sup_a = center_a + radius * normalize(dir) = (1.0, 0, 0) */
    ASSERT_NEAR(v.sup_a.x, GJK_SPHERE_RADIUS, EPSILON);
    ASSERT_NEAR(v.sup_a.y, 0.0f, EPSILON);
    /* sup_b = center_b + radius * normalize(-dir) = (1.5 - 1.0, 0, 0) = (0.5, 0, 0) */
    ASSERT_NEAR(v.sup_b.x, GJK_SUPPORT_SEP - GJK_SPHERE_RADIUS, EPSILON);
    ASSERT_NEAR(v.sup_b.y, 0.0f, EPSILON);
    /* point = sup_a - sup_b */
    ASSERT_NEAR(v.point.x, v.sup_a.x - v.sup_b.x, EPSILON);
    ASSERT_NEAR(v.point.y, v.sup_a.y - v.sup_b.y, EPSILON);
    ASSERT_NEAR(v.point.z, v.sup_a.z - v.sup_b.z, EPSILON);
    END_TEST();
}

static void test_gjk_support_box_pair(void)
{
    TEST("GJK_support_box_pair — exact support vertices for +Y direction");
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    vec3 pa       = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb       = vec3_create(GJK_SUPPORT_BOX_SEP, 0.0f, 0.0f);
    quat identity = quat_identity();
    vec3 dir      = vec3_create(0.3f, 0.8f, 0.5f);  /* non-axis-aligned */

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, pa, identity, &sb, pb, identity, dir);

    /* Box support in direction d picks the corner sign-matching d.
     * sup_a for (0.3,0.8,0.5) = (+1,+1,+1) = (1,1,1) */
    ASSERT_NEAR(v.sup_a.x, GJK_BOX_HALF, EPSILON);
    ASSERT_NEAR(v.sup_a.y, GJK_BOX_HALF, EPSILON);
    ASSERT_NEAR(v.sup_a.z, GJK_BOX_HALF, EPSILON);
    /* sup_b for -dir = (-0.3,-0.8,-0.5) picks corner (-1,-1,-1) + center */
    ASSERT_NEAR(v.sup_b.x, GJK_SUPPORT_BOX_SEP - GJK_BOX_HALF, EPSILON);
    ASSERT_NEAR(v.sup_b.y, -GJK_BOX_HALF, EPSILON);
    ASSERT_NEAR(v.sup_b.z, -GJK_BOX_HALF, EPSILON);
    /* point = sup_a - sup_b */
    ASSERT_NEAR(v.point.x, v.sup_a.x - v.sup_b.x, EPSILON);
    ASSERT_NEAR(v.point.y, v.sup_a.y - v.sup_b.y, EPSILON);
    ASSERT_NEAR(v.point.z, v.sup_a.z - v.sup_b.z, EPSILON);
    END_TEST();
}

/* ── Tests: Basic intersection / separation ─────────────────────────────── */

static void test_gjk_overlapping_spheres(void)
{
    TEST("GJK_overlapping_spheres — clearly overlapping → intersecting = true");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    /* distance GJK_OVERLAP_DIST < sum of radii GJK_TOUCH_DIST */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_separated_spheres(void)
{
    TEST("GJK_separated_spheres — clearly separated → intersecting = false");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    /* distance >> sum of radii */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_FAR_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

static void test_gjk_touching_spheres(void)
{
    TEST("GJK_touching_spheres — distance == sum of radii → deterministic hit");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    /* distance = GJK_TOUCH_DIST = sum of radii — first support point is at origin */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_TOUCH_DIST, 0.0f, 0.0f));
    /* Exact touch is a deterministic hit for this fixture. */
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    END_TEST();
}

/* ── Tests: All 9 shape-pair combos ─────────────────────────────────────── */

/* sphere – sphere */

static void test_gjk_sphere_sphere_overlap(void)
{
    TEST("GJK_sphere_sphere_overlap — sphere vs sphere, overlapping → true");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SPHERE_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_sphere_sphere_separated(void)
{
    TEST("GJK_sphere_sphere_separated — sphere vs sphere, far apart → false");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SEPARATED_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* sphere – box */

static void test_gjk_sphere_box_overlap(void)
{
    TEST("GJK_sphere_box_overlap — sphere vs box, overlapping → true");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SPHERE_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_sphere_box_separated(void)
{
    TEST("GJK_sphere_box_separated — sphere vs box, far apart → false");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f,  0.0f, 0.0f),
        &sb, vec3_create(GJK_FAR_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* sphere – capsule */

static void test_gjk_sphere_capsule_overlap(void)
{
    TEST("GJK_sphere_capsule_overlap — sphere vs capsule, overlapping → true");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SPHERE_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_sphere_capsule_separated(void)
{
    TEST("GJK_sphere_capsule_separated — sphere vs capsule, far apart → false");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f,  0.0f, 0.0f),
        &sb, vec3_create(GJK_FAR_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* box – box */

static void test_gjk_box_box_overlap(void)
{
    TEST("GJK_box_box_overlap — box vs box, overlapping → true");
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SPHERE_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_box_box_separated(void)
{
    TEST("GJK_box_box_separated — box vs box, far apart → false");
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f,  0.0f, 0.0f),
        &sb, vec3_create(GJK_FAR_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* box – sphere */

static void test_gjk_box_sphere_overlap(void)
{
    TEST("GJK_box_sphere_overlap — box vs sphere, overlapping → true");
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SPHERE_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_box_sphere_separated(void)
{
    TEST("GJK_box_sphere_separated — box vs sphere, far apart → false");
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f,  0.0f, 0.0f),
        &sb, vec3_create(GJK_FAR_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* box – capsule */

static void test_gjk_box_capsule_overlap(void)
{
    TEST("GJK_box_capsule_overlap — box vs capsule, overlapping → true");
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SPHERE_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_box_capsule_separated(void)
{
    TEST("GJK_box_capsule_separated — box vs capsule, far apart → false");
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f,  0.0f, 0.0f),
        &sb, vec3_create(GJK_FAR_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* capsule – capsule */

static void test_gjk_capsule_capsule_overlap(void)
{
    TEST("GJK_capsule_capsule_overlap — capsule vs capsule, overlapping → true");
    ForgePhysicsCollisionShape sa = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsCollisionShape sb = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_capsule_capsule_separated(void)
{
    TEST("GJK_capsule_capsule_separated — capsule vs capsule, far apart → false");
    ForgePhysicsCollisionShape sa = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsCollisionShape sb = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f,  0.0f, 0.0f),
        &sb, vec3_create(GJK_FAR_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* capsule – sphere */

static void test_gjk_capsule_sphere_overlap(void)
{
    TEST("GJK_capsule_sphere_overlap — capsule vs sphere, overlapping → true");
    ForgePhysicsCollisionShape sa = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SPHERE_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_capsule_sphere_separated(void)
{
    TEST("GJK_capsule_sphere_separated — capsule vs sphere, far apart → false");
    ForgePhysicsCollisionShape sa = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f,  0.0f, 0.0f),
        &sb, vec3_create(GJK_FAR_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* capsule – box */

static void test_gjk_capsule_box_overlap(void)
{
    TEST("GJK_capsule_box_overlap — capsule vs box, overlapping → true");
    ForgePhysicsCollisionShape sa = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SPHERE_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_capsule_box_separated(void)
{
    TEST("GJK_capsule_box_separated — capsule vs box, far apart → false");
    ForgePhysicsCollisionShape sa = forge_physics_shape_capsule(GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f,  0.0f, 0.0f),
        &sb, vec3_create(GJK_FAR_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* ── Tests: Edge cases ───────────────────────────────────────────────────── */

static void test_gjk_coincident_centers(void)
{
    TEST("GJK_coincident_centers — both shapes at origin → intersecting = true");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_rotated_obb_overlap(void)
{
    TEST("GJK_rotated_obb_overlap — rotation changes hit/miss at boundary distance");
    /* Unit box half-extent = 1.0. Two axis-aligned boxes separated by 2.1
     * along X are just barely separated (sum of half-extents = 2.0 < 2.1).
     * Rotating box B 45° around Y extends its X-projected half-extent to
     * sqrt(2) ≈ 1.414, so combined reach = 1.0 + 1.414 = 2.414 > 2.1.
     * The rotation should flip the result from miss to hit. */
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    vec3 pa       = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb       = vec3_create(GJK_OBB_BOUNDARY_SEP, 0.0f, 0.0f);
    quat identity = quat_identity();
    quat rot_b    = quat_from_euler(GJK_OBB_ROTATION_DEG * FORGE_DEG2RAD, 0.0f, 0.0f);

    /* Axis-aligned: separated */
    ForgePhysicsGJKResult r_aa = forge_physics_gjk_intersect(
        &sa, pa, identity, &sb, pb, identity);
    ASSERT_TRUE(!r_aa.intersecting);

    /* Rotated 45°: overlapping (sqrt(2) reach exceeds gap) */
    ForgePhysicsGJKResult r_rot = forge_physics_gjk_intersect(
        &sa, pa, identity, &sb, pb, rot_b);
    ASSERT_TRUE(r_rot.intersecting);
    END_TEST();
}

static void test_gjk_large_separation(void)
{
    TEST("GJK_large_separation — spheres 1000 units apart → intersecting = false");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(   0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_LARGE_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

static void test_gjk_near_miss(void)
{
    TEST("GJK_near_miss — spheres just barely not touching → intersecting = false");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    /* Sum of radii = GJK_TOUCH_DIST; distance = GJK_NEAR_MISS_DIST → separated */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_NEAR_MISS_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

static void test_gjk_deep_penetration(void)
{
    TEST("GJK_deep_penetration — small sphere fully inside large sphere → intersecting = true");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_LARGE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SMALL_RADIUS);
    /* sb is completely contained within sa */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SMALL_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    END_TEST();
}

static void test_gjk_collinear_aligned_overlap(void)
{
    TEST("GJK_collinear_aligned — spheres on same axis, overlapping → intersecting = true");
    /* Two spheres on the X axis, overlapping. The support points are
     * collinear, exercising the gjk_do_line_ collinear projection path. */
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    END_TEST();
}

static void test_gjk_collinear_large_scale(void)
{
    TEST("GJK_collinear_large_scale — collinear overlap at 1e4 scale");
    /* Verify the scale-aware collinearity check works at large world
     * coordinates where a fixed-epsilon check would fail. */
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(50.0f);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(50.0f);
    /* Both on the X axis at large offset — collinear support points */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(1e4f, 0.0f, 0.0f),
        &sb, vec3_create(1e4f + 30.0f, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    END_TEST();
}

static void test_gjk_coplanar_touching_triangle(void)
{
    TEST("GJK_coplanar_touching — origin on triangle face → intersecting = true");
    /* Two boxes positioned so the Minkowski difference simplex produces a
     * triangle that contains the origin on its face (plane_dot ≈ 0). The
     * boxes share a face plane at x = 0.5. */
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(
        vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF));
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(
        vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF));
    /* Exactly touching: distance = 2 * half = 2 * 1.0 */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_BOX_HALF * 2.0f, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    END_TEST();
}

static void test_gjk_non_unit_quaternion(void)
{
    TEST("GJK_non_unit_quaternion — scaled quaternion normalized internally → correct result");
    /* Use boxes (orientation-sensitive) to actually exercise the quat path */
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    /* Non-unit quaternion: 45° Y rotation scaled by 2 */
    quat unit_rot = quat_from_euler(GJK_OBB_ROTATION_DEG * FORGE_DEG2RAD, 0.0f, 0.0f);
    quat scaled = { unit_rot.w * GJK_QUAT_SCALE, unit_rot.x * GJK_QUAT_SCALE,
                    unit_rot.y * GJK_QUAT_SCALE, unit_rot.z * GJK_QUAT_SCALE };
    quat identity = quat_identity();

    /* Use boundary distance where rotation changes the result.
     * At GJK_OBB_BOUNDARY_SEP: identity → miss, 45° rotation → hit.
     * If normalization fails, the scaled quat acts like identity → wrong result. */
    ForgePhysicsGJKResult r_identity = forge_physics_gjk_intersect(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sb, vec3_create(GJK_OBB_BOUNDARY_SEP, 0.0f, 0.0f), identity);
    ASSERT_TRUE(!r_identity.intersecting);  /* axis-aligned: miss */

    ForgePhysicsGJKResult r_unit_rot = forge_physics_gjk_intersect(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), unit_rot,
        &sb, vec3_create(GJK_OBB_BOUNDARY_SEP, 0.0f, 0.0f), identity);
    ASSERT_TRUE(r_unit_rot.intersecting);  /* 45° rotation: hit */

    ForgePhysicsGJKResult r_scaled_rot = forge_physics_gjk_intersect(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), scaled,
        &sb, vec3_create(GJK_OBB_BOUNDARY_SEP, 0.0f, 0.0f), identity);
    /* Scaled quat must produce same result as unit — proves normalization */
    ASSERT_TRUE(r_scaled_rot.intersecting == r_unit_rot.intersecting);

    /* Zero quaternion — should return empty result */
    quat zero_q = { 0.0f, 0.0f, 0.0f, 0.0f };
    ForgePhysicsGJKResult r3 = forge_physics_gjk_intersect(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), zero_q,
        &sb, vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f), identity);
    ASSERT_TRUE(!r3.intersecting);
    END_TEST();
}

/* ── Tests: Convergence ──────────────────────────────────────────────────── */

static void test_gjk_converges_within_max(void)
{
    TEST("GJK_converges_within_max — iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_CONVERGE_SEP, 0.0f, 0.0f));
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    END_TEST();
}

static void test_gjk_simple_converges_fast(void)
{
    TEST("GJK_simple_converges_fast — overlapping spheres converge in < 10 iterations");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.iterations < GJK_FAST_ITER_LIMIT);
    END_TEST();
}

/* ── Tests: Determinism ──────────────────────────────────────────────────── */

static void test_gjk_determinism(void)
{
    TEST("GJK_determinism — same inputs produce identical results on two calls");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    vec3 half = vec3_create(GJK_DETERM_BOX_HALF, GJK_DETERM_BOX_HALF, GJK_DETERM_BOX_HALF);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(GJK_DETERM_PB_X, GJK_DETERM_PB_Y, GJK_DETERM_PB_Z);

    ForgePhysicsGJKResult r1 = gjk_test(&sa, pa, &sb, pb);
    ForgePhysicsGJKResult r2 = gjk_test(&sa, pa, &sb, pb);

    ASSERT_TRUE(r1.intersecting == r2.intersecting);
    ASSERT_TRUE(r1.iterations   == r2.iterations);
    /* Simplex vertex count must also match */
    ASSERT_TRUE(r1.simplex.count == r2.simplex.count);
    END_TEST();
}

/* ── Tests: Simplex validity for EPA ────────────────────────────────────── */

static void test_gjk_simplex_valid_on_hit(void)
{
    TEST("GJK_simplex_valid_on_hit — intersecting result has count >= 1 and finite coords");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.simplex.count >= 1);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_simplex_valid_on_miss(void)
{
    TEST("GJK_simplex_valid_on_miss — separated result has finite simplex coords");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f,  0.0f, 0.0f),
        &sb, vec3_create(GJK_SEPARATED_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

/* ── Tests: Degenerate inputs ────────────────────────────────────────────── */

static void test_gjk_null_shape_a(void)
{
    TEST("GJK_null_shape_a — NULL shape_a → intersecting = false, no crash");
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    ForgePhysicsGJKResult r = forge_physics_gjk_intersect(
        NULL,
        vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sb,
        vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f), identity);
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

static void test_gjk_null_shape_b(void)
{
    TEST("GJK_null_shape_b — NULL shape_b → intersecting = false, no crash");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    ForgePhysicsGJKResult r = forge_physics_gjk_intersect(
        &sa,
        vec3_create(0.0f, 0.0f, 0.0f), identity,
        NULL,
        vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f), identity);
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

static void test_gjk_nan_position(void)
{
    TEST("GJK_nan_position — NaN position → intersecting = false, no crash");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    ForgePhysicsGJKResult r = forge_physics_gjk_intersect(
        &sa,
        vec3_create(SDL_sqrtf(-1.0f), SDL_sqrtf(-1.0f), SDL_sqrtf(-1.0f)), identity,
        &sb,
        vec3_create(0.0f, 0.0f, 0.0f), identity);
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

static void test_gjk_invalid_shape(void)
{
    TEST("GJK_invalid_shape — invalid shape type → intersecting = false, no crash");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb;
    SDL_memset(&sb, 0, sizeof(sb));
    sb.type = (ForgePhysicsShapeType)GJK_INVALID_SHAPE_TYPE;
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

static void test_gjk_zero_volume_shape(void)
{
    TEST("GJK_zero_volume_shape — sphere with zero radius clamped to minimum, intersects");
    /* Radius 0.0 should be clamped internally to FORGE_PHYSICS_SHAPE_MIN_DIM */
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(0.0f);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ASSERT_NEAR(sa.data.sphere.radius, FORGE_PHYSICS_SHAPE_MIN_DIM, EPSILON);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    END_TEST();
}

/* ── Tests: Convenience function ─────────────────────────────────────────── */

static void test_gjk_test_bodies_matches(void)
{
    TEST("GJK_test_bodies_matches — forge_physics_gjk_test_bodies matches forge_physics_gjk_intersect");
    /* Use boxes with non-identity orientation to exercise pose forwarding */
    vec3 half = vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(GJK_OBB_BOUNDARY_SEP, 0.0f, 0.0f);
    quat rot = quat_from_euler(GJK_OBB_ROTATION_DEG * FORGE_DEG2RAD, 0.0f, 0.0f);

    /* Create rigid bodies with the rotation applied */
    ForgePhysicsRigidBody ba = forge_physics_rigid_body_create(
        pa, GJK_BODY_MASS, GJK_BODY_DRAG, GJK_BODY_ANG_DRAG, GJK_BODY_RESTIT);
    ba.orientation = rot;
    ForgePhysicsRigidBody bb = forge_physics_rigid_body_create(
        pb, GJK_BODY_MASS, GJK_BODY_DRAG, GJK_BODY_ANG_DRAG, GJK_BODY_RESTIT);

    ForgePhysicsGJKResult r_direct = forge_physics_gjk_intersect(
        &sa, pa, rot, &sb, pb, quat_identity());
    ForgePhysicsGJKResult r_bodies = forge_physics_gjk_test_bodies(
        &ba, &sa, &bb, &sb);

    ASSERT_TRUE(r_direct.intersecting == r_bodies.intersecting);
    ASSERT_TRUE(r_direct.iterations == r_bodies.iterations);
    END_TEST();
}

static void test_gjk_test_bodies_null_body(void)
{
    TEST("GJK_test_bodies_null_body — NULL body pointer → intersecting = false, no crash");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsRigidBody ba = forge_physics_rigid_body_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        GJK_BODY_MASS, GJK_BODY_DRAG, GJK_BODY_ANG_DRAG, GJK_BODY_RESTIT);

    /* NULL body_a */
    ForgePhysicsGJKResult r1 = forge_physics_gjk_test_bodies(NULL, &sa, &ba, &sb);
    ASSERT_TRUE(!r1.intersecting);

    /* NULL body_b */
    ForgePhysicsGJKResult r2 = forge_physics_gjk_test_bodies(&ba, &sa, NULL, &sb);
    ASSERT_TRUE(!r2.intersecting);

    /* NULL shape_a */
    ForgePhysicsGJKResult r3 = forge_physics_gjk_test_bodies(&ba, NULL, &ba, &sb);
    ASSERT_TRUE(!r3.intersecting);

    /* NULL shape_b */
    ForgePhysicsGJKResult r4 = forge_physics_gjk_test_bodies(&ba, &sa, &ba, NULL);
    ASSERT_TRUE(!r4.intersecting);
    END_TEST();
}

/* ── Tests: Coplanar tetrahedron / division-by-zero guards ───────────────── */
/* These tests exercise the degenerate coplanar fallback path in
 * gjk_do_tetrahedron_() where the Voronoi closest-point algorithm runs.
 * Flat boxes (near-zero extent in one axis) force the Minkowski difference
 * simplex to have near-zero volume, triggering the coplanar guard.
 * The four guarded divisions are:
 *   Edge AB:   d1 / (d1 - d3)       → fallback t = 0 (vertex A)
 *   Edge AC:   d2 / (d2 - d6)       → fallback t = 0 (vertex A)
 *   Edge BC:   (d4-d3)/((d4-d3)+(d5-d6)) → fallback t = 0 (vertex B)
 *   Face:      1 / (va + vb + vc)    → fallback to vertex A distance
 * All tests verify: correct boolean result, finite simplex, bounded iters. */

static void test_gjk_flat_box_overlap_y(void)
{
    TEST("GJK_flat_box_overlap_y — Y-flat boxes overlapping → finite result, no NaN");
    /* Two boxes that are paper-thin in Y.  The Minkowski difference of
     * two flat boxes is itself flat, so the GJK simplex becomes coplanar. */
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_FLAT_OFFSET_SMALL, 0.0f, 0.0f));

    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_flat_box_overlap_x(void)
{
    TEST("GJK_flat_box_overlap_x — X-flat boxes overlapping → finite result");
    vec3 half = vec3_create(GJK_FLAT_HALF_THIN, GJK_FLAT_HALF_WIDE,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    /* Offset along Y so flat dimension doesn't dominate separation */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(0.0f, GJK_FLAT_OFFSET_SMALL, 0.0f));

    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_flat_box_overlap_z(void)
{
    TEST("GJK_flat_box_overlap_z — Z-flat boxes overlapping on wide axis → finite result");
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_WIDE,
                            GJK_FLAT_HALF_THIN);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    /* Offset along X (wide axis) so the boxes overlap despite thin Z */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_FLAT_OFFSET_SMALL, 0.0f, 0.0f));

    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_flat_box_separated(void)
{
    TEST("GJK_flat_box_separated — Y-flat boxes far apart → no intersection, finite");
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_FLAT_SEP_DIST, 0.0f, 0.0f));

    ASSERT_TRUE(!r.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_flat_box_coincident(void)
{
    TEST("GJK_flat_box_coincident — Y-flat boxes at same position → intersecting, finite");
    /* Coincident centers with a flat shape.  The initial search direction
     * falls back to the X axis, and the simplex vertices will be nearly
     * coplanar.  This stresses the vertex-region fallback where d1 ≈ 0
     * and d3 ≈ 0 (edge AB guard) and the face-region denom ≈ 0 guard. */
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(0.0f, 0.0f, 0.0f));

    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_flat_box_exact_touch(void)
{
    TEST("GJK_flat_box_exact_touch — flat boxes touching on face → hit, finite simplex");
    /* Distance = 2 * GJK_FLAT_HALF_WIDE = exact sum of half-extents along X.
     * The Minkowski difference boundary passes through the origin, and the
     * simplex is coplanar.  This stresses the face-region barycentric
     * denominator guard (va + vb + vc ≈ 0). */
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_FLAT_OFFSET_EXACT, 0.0f, 0.0f));

    /* Exact touch is a deterministic hit (same as coplanar_touching_triangle) */
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_flat_box_rotated(void)
{
    TEST("GJK_flat_box_rotated — rotated flat box overlap → correct despite coplanar simplex");
    /* Rotate a flat box 45° around Y.  The Minkowski difference is still
     * flat (both boxes thin in Y) but the support directions change,
     * exercising different Voronoi regions in the coplanar fallback. */
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    quat rot = quat_from_euler(GJK_OBB_ROTATION_DEG * FORGE_DEG2RAD, 0.0f, 0.0f);
    quat identity = quat_identity();

    ForgePhysicsGJKResult r = forge_physics_gjk_intersect(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sb, vec3_create(GJK_FLAT_OFFSET_SMALL, 0.0f, 0.0f), rot);

    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_flat_box_diagonal_overlap(void)
{
    TEST("GJK_flat_box_diagonal_overlap — flat boxes offset on diagonal → overlap, finite");
    /* Offset on both X and Z to exercise different Voronoi edge regions
     * in the coplanar closest-point algorithm.  At GJK_FLAT_DIAG_SEP the
     * box corners still overlap. */
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_FLAT_DIAG_SEP, 0.0f, GJK_FLAT_DIAG_SEP));

    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_flat_sphere_vs_flat_box(void)
{
    TEST("GJK_flat_sphere_vs_flat_box — sphere vs Y-flat box → coplanar path, finite");
    /* Mixed shape pair: sphere + flat box.  The sphere's support is smooth
     * but the flat box constrains the Minkowski difference to a thin slab. */
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_FLAT_OFFSET_SMALL, 0.0f, 0.0f));

    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_flat_capsule_vs_flat_box(void)
{
    TEST("GJK_flat_capsule_vs_flat_box — capsule vs Y-flat box → coplanar path, finite");
    ForgePhysicsCollisionShape sa = forge_physics_shape_capsule(
        GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_FLAT_OFFSET_SMALL, 0.0f, 0.0f));

    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_micro_box_overlap(void)
{
    TEST("GJK_micro_box_overlap — boxes at SHAPE_MIN_DIM boundary → finite, no crash");
    /* All extents at the minimum clamped dimension.  The Minkowski
     * difference is a tiny cube — all simplex vertices cluster near
     * the same point, maximising degeneracy.  The critical check is
     * that no NaN/Inf is produced, regardless of the boolean result. */
    vec3 half = vec3_create(GJK_FLAT_MICRO_HALF, GJK_FLAT_MICRO_HALF,
                            GJK_FLAT_MICRO_HALF);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);

    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(0.0f, 0.0f, 0.0f));

    /* Result may or may not be intersecting at this scale — the important
     * thing is that no NaN/Inf was produced by degenerate divisions. */
    ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_micro_sphere_overlap(void)
{
    TEST("GJK_micro_sphere_overlap — tiny spheres overlap correctly with squared epsilon");
    /* Spheres with radius just above the duplicate-vertex squared-epsilon
     * threshold. Tests that the coplanar face scoring does not falsely
     * classify distinct vertices as duplicates. */
    float r = 0.001f;  /* well above GJK_EPSILON² ≈ 1e-12, but below GJK_EPSILON ≈ 1e-6 */
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(r);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(r);

    /* Overlapping at half-radius */
    ForgePhysicsGJKResult r_hit = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(r, 0.0f, 0.0f));
    ASSERT_TRUE(r_hit.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r_hit.simplex));

    /* Separated */
    ForgePhysicsGJKResult r_miss = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(3.0f * r, 0.0f, 0.0f));
    ASSERT_TRUE(!r_miss.intersecting);
    ASSERT_TRUE(simplex_is_finite(&r_miss.simplex));
    END_TEST();
}

static void test_gjk_flat_box_stability_sweep(void)
{
    TEST("GJK_flat_box_stability_sweep — 200 queries sliding flat boxes, no NaN/Inf");
    /* Slide shape B across shape A in small increments.  Many positions will
     * produce boundary / coplanar simplices.  Verifying that all 200 results
     * have a finite simplex exercises the division guards across a range of
     * Voronoi regions.  This is the most thorough stress test because
     * different offsets activate different edge/face regions. */
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < GJK_DEGEN_ITER_COUNT; i++) {
        float offset = -GJK_FLAT_HALF_WIDE
            + (float)i * GJK_DEGEN_STEP_OFFSET;
        vec3 pb = vec3_create(offset, 0.0f, 0.0f);

        ForgePhysicsGJKResult r = gjk_test(&sa, pa, &sb, pb);

        ASSERT_TRUE(r.iterations <= FORGE_PHYSICS_GJK_MAX_ITERATIONS);
        ASSERT_TRUE(simplex_is_finite(&r.simplex));
    }
    END_TEST();
}

static void test_gjk_flat_box_determinism(void)
{
    TEST("GJK_flat_box_determinism — coplanar queries produce identical results twice");
    /* Two runs of the same coplanar configuration must produce bit-identical
     * results.  The division guards must not introduce non-determinism. */
    vec3 half = vec3_create(GJK_FLAT_HALF_WIDE, GJK_FLAT_HALF_THIN,
                            GJK_FLAT_HALF_WIDE);
    ForgePhysicsCollisionShape sa = forge_physics_shape_box(half);
    ForgePhysicsCollisionShape sb = forge_physics_shape_box(half);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(GJK_FLAT_OFFSET_SMALL, 0.0f, 0.0f);

    ForgePhysicsGJKResult r1 = gjk_test(&sa, pa, &sb, pb);
    ForgePhysicsGJKResult r2 = gjk_test(&sa, pa, &sb, pb);

    ASSERT_TRUE(r1.intersecting == r2.intersecting);
    ASSERT_TRUE(r1.iterations   == r2.iterations);
    ASSERT_TRUE(r1.simplex.count == r2.simplex.count);
    ASSERT_TRUE(r1.simplex.count >= 0);
    ASSERT_TRUE(r1.simplex.count <= 4);
    ASSERT_TRUE(simplex_is_finite(&r1.simplex));
    ASSERT_TRUE(simplex_is_finite(&r2.simplex));
    for (int i = 0; i < r1.simplex.count; i++) {
        ASSERT_NEAR(r1.simplex.verts[i].point.x,
                     r2.simplex.verts[i].point.x, EPSILON);
        ASSERT_NEAR(r1.simplex.verts[i].point.y,
                     r2.simplex.verts[i].point.y, EPSILON);
        ASSERT_NEAR(r1.simplex.verts[i].point.z,
                     r2.simplex.verts[i].point.z, EPSILON);
        /* Also compare support provenance — EPA needs these */
        ASSERT_NEAR(r1.simplex.verts[i].sup_a.x,
                     r2.simplex.verts[i].sup_a.x, EPSILON);
        ASSERT_NEAR(r1.simplex.verts[i].sup_a.y,
                     r2.simplex.verts[i].sup_a.y, EPSILON);
        ASSERT_NEAR(r1.simplex.verts[i].sup_a.z,
                     r2.simplex.verts[i].sup_a.z, EPSILON);
        ASSERT_NEAR(r1.simplex.verts[i].sup_b.x,
                     r2.simplex.verts[i].sup_b.x, EPSILON);
        ASSERT_NEAR(r1.simplex.verts[i].sup_b.y,
                     r2.simplex.verts[i].sup_b.y, EPSILON);
        ASSERT_NEAR(r1.simplex.verts[i].sup_b.z,
                     r2.simplex.verts[i].sup_b.z, EPSILON);
    }
    END_TEST();
}

/* ── Tests: Support function input validation ────────────────────────────── */

static void test_gjk_support_null_shape_a(void)
{
    TEST("GJK_support_null_shape_a — NULL shape_a → zeroed vertex");
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    vec3 dir = vec3_create(1.0f, 0.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        NULL, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sb,  vec3_create(0.0f, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_support_null_shape_b(void)
{
    TEST("GJK_support_null_shape_b — NULL shape_b → zeroed vertex");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    vec3 dir = vec3_create(1.0f, 0.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa,  vec3_create(0.0f, 0.0f, 0.0f), identity,
        NULL, vec3_create(0.0f, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_support_invalid_shape(void)
{
    TEST("GJK_support_invalid_shape — invalid shape type → zeroed vertex");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb;
    SDL_memset(&sb, 0, sizeof(sb));
    sb.type = (ForgePhysicsShapeType)GJK_INVALID_SHAPE_TYPE;
    quat identity = quat_identity();
    vec3 dir = vec3_create(1.0f, 0.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sb, vec3_create(1.0f, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_support_nan_position(void)
{
    TEST("GJK_support_nan_position — NaN position → zeroed vertex");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    vec3 dir = vec3_create(1.0f, 0.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, vec3_create(SDL_sqrtf(-1.0f), 0.0f, 0.0f), identity,
        &sb, vec3_create(0.0f, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_support_inf_position(void)
{
    TEST("GJK_support_inf_position — infinite position → zeroed vertex");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    vec3 dir = vec3_create(1.0f, 0.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, vec3_create(GJK_TEST_INF, 0.0f, 0.0f), identity,
        &sb, vec3_create(0.0f, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_support_zero_direction(void)
{
    TEST("GJK_support_zero_direction — zero-length dir → zeroed vertex");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    vec3 dir = vec3_create(0.0f, 0.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sb, vec3_create(1.0f, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_support_near_zero_direction(void)
{
    TEST("GJK_support_near_zero_dir — sub-epsilon dir → zeroed vertex");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    vec3 dir = vec3_create(GJK_SUPPORT_ZERO_DIR_EPS, 0.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sb, vec3_create(1.0f, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_support_nan_direction(void)
{
    TEST("GJK_support_nan_direction — NaN direction → zeroed vertex");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    vec3 dir = vec3_create(SDL_sqrtf(-1.0f), 1.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sb, vec3_create(0.0f, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_support_zero_quat(void)
{
    TEST("GJK_support_zero_quat — zero quaternion → zeroed vertex");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat zero_q = { 0.0f, 0.0f, 0.0f, 0.0f };
    vec3 dir = vec3_create(1.0f, 0.0f, 0.0f);

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), zero_q,
        &sb, vec3_create(0.0f, 0.0f, 0.0f), quat_identity(), dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

/* ── Tests: simplex inflation — intersecting results always have count == 4 ─ */

static void test_gjk_coincident_spheres_simplex_count(void)
{
    TEST("GJK_coincident_simplex — coincident spheres hit, inflated to count == 4");
    /* Coincident spheres trigger the zero-direction early exit (dir_len < eps).
     * GJK inflates the sub-dimensional simplex to a full tetrahedron so EPA
     * can use it directly as a starting polytope. */
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.simplex.count == FORGE_PHYSICS_GJK_MAX_SIMPLEX);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_touching_simplex_count(void)
{
    TEST("GJK_touching_simplex — touching spheres hit, inflated to count == 4");
    /* Exact touch triggers the zero-direction early exit with count == 1,
     * then simplex inflation expands to a full tetrahedron for EPA. */
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_TOUCH_DIST, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.simplex.count == FORGE_PHYSICS_GJK_MAX_SIMPLEX);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_deep_overlap_simplex_valid(void)
{
    TEST("GJK_deep_overlap_simplex — deeply overlapping → inflated simplex for EPA");
    /* Deep overlap with offset centers. Even if an early exit triggers (e.g.
     * collinear or coplanar path), GJK inflates to a full tetrahedron. */
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_LARGE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(0.0f, 0.0f, 0.0f),
        &sb, vec3_create(GJK_SPHERE_RADIUS, 0.0f, 0.0f));
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.simplex.count == FORGE_PHYSICS_GJK_MAX_SIMPLEX);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

/* ── Tests: INF overflow guards ───────────────────────────────────────── */

#define GJK_HUGE_POS  2e38f  /* large enough that pos_b - pos_a overflows float */

static void test_gjk_support_overflow_zeroed(void)
{
    TEST("GJK_support_overflow — huge positions overflow v.point → zeroed vertex");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    vec3 dir = vec3_create(1.0f, 0.0f, 0.0f);

    /* Place shapes at positions whose difference overflows float */
    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, vec3_create(-GJK_HUGE_POS, 0.0f, 0.0f), identity,
        &sb, vec3_create( GJK_HUGE_POS, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_intersect_overflow_no_false_positive(void)
{
    TEST("GJK_intersect_overflow — huge positions → no false-positive intersection");
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);

    /* Positions far enough apart that direction computation overflows */
    ForgePhysicsGJKResult r = gjk_test(
        &sa, vec3_create(-GJK_HUGE_POS, 0.0f, 0.0f),
        &sb, vec3_create( GJK_HUGE_POS, 0.0f, 0.0f));

    /* Must not report intersection — should bail out on INF direction */
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

static void test_gjk_support_direction_overflow_zeroed(void)
{
    TEST("GJK_support_dir_overflow — finite components whose squares overflow → zeroed");
    /* Components are individually finite but large enough that their squares
     * overflow float, producing INF in vec3_length_squared(dir). */
    ForgePhysicsCollisionShape sa = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    quat identity = quat_identity();
    vec3 dir = vec3_create(2e19f, 2e19f, 2e19f);  /* each² ≈ 4e38 > FLT_MAX */

    ForgePhysicsGJKVertex v = forge_physics_gjk_support(
        &sa, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sb, vec3_create(1.0f, 0.0f, 0.0f), identity, dir);

    ASSERT_VERTEX_INVALID(v);
    END_TEST();
}

static void test_gjk_intersection_always_tetrahedron(void)
{
    TEST("GJK_intersection_tetrahedron — intersecting result always has count == 4");
    /* Test various configurations that produce early-exit hits (collinear,
     * coplanar, direction-collapse) and verify the simplex is inflated to
     * a full tetrahedron for EPA. */
    ForgePhysicsCollisionShape sphere = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsCollisionShape box = forge_physics_shape_box(
        vec3_create(GJK_BOX_HALF, GJK_BOX_HALF, GJK_BOX_HALF));
    ForgePhysicsCollisionShape capsule = forge_physics_shape_capsule(
        GJK_CAPSULE_RADIUS, GJK_CAPSULE_HALF_H);
    quat identity = quat_identity();

    /* Coincident spheres — triggers direction-collapse (count was 1) */
    ForgePhysicsGJKResult r = forge_physics_gjk_intersect(
        &sphere, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sphere, vec3_create(0.0f, 0.0f, 0.0f), identity);
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.simplex.count == FORGE_PHYSICS_GJK_MAX_SIMPLEX);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));

    /* Overlapping spheres along X — may trigger collinear hit (count was 2) */
    r = forge_physics_gjk_intersect(
        &sphere, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sphere, vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f), identity);
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.simplex.count == FORGE_PHYSICS_GJK_MAX_SIMPLEX);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));

    /* Touching spheres — may trigger coplanar hit (count was 3) */
    r = forge_physics_gjk_intersect(
        &sphere, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sphere, vec3_create(GJK_TOUCH_DIST, 0.0f, 0.0f), identity);
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.simplex.count == FORGE_PHYSICS_GJK_MAX_SIMPLEX);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));

    /* Box-sphere overlap */
    r = forge_physics_gjk_intersect(
        &box, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &sphere, vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f), identity);
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.simplex.count == FORGE_PHYSICS_GJK_MAX_SIMPLEX);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));

    /* Capsule-capsule overlap */
    r = forge_physics_gjk_intersect(
        &capsule, vec3_create(0.0f, 0.0f, 0.0f), identity,
        &capsule, vec3_create(GJK_OVERLAP_DIST, 0.0f, 0.0f), identity);
    ASSERT_TRUE(r.intersecting);
    ASSERT_TRUE(r.simplex.count == FORGE_PHYSICS_GJK_MAX_SIMPLEX);
    ASSERT_TRUE(simplex_is_finite(&r.simplex));
    END_TEST();
}

static void test_gjk_invalid_input_no_false_positive(void)
{
    TEST("GJK_invalid_no_false_positive — invalid shapes never report intersection");
    /* Regression: zeroed support sentinel previously caused false-positive
     * intersection because zero-length direction was misinterpreted as
     * "shapes touching". NaN sentinel prevents this. */
    ForgePhysicsCollisionShape valid = forge_physics_shape_sphere(GJK_SPHERE_RADIUS);
    ForgePhysicsGJKResult r;

    /* NULL shape */
    r = forge_physics_gjk_intersect(
        NULL, vec3_create(0.0f, 0.0f, 0.0f), quat_identity(),
        &valid, vec3_create(0.0f, 0.0f, 0.0f), quat_identity());
    ASSERT_TRUE(!r.intersecting);

    /* NaN position */
    r = forge_physics_gjk_intersect(
        &valid, vec3_create(SDL_sqrtf(-1.0f), 0.0f, 0.0f), quat_identity(),
        &valid, vec3_create(0.0f, 0.0f, 0.0f), quat_identity());
    ASSERT_TRUE(!r.intersecting);

    /* Zero quaternion */
    quat zero_q = { 0.0f, 0.0f, 0.0f, 0.0f };
    r = forge_physics_gjk_intersect(
        &valid, vec3_create(0.0f, 0.0f, 0.0f), zero_q,
        &valid, vec3_create(0.0f, 0.0f, 0.0f), quat_identity());
    ASSERT_TRUE(!r.intersecting);
    END_TEST();
}

/* ── Public runner ───────────────────────────────────────────────────────── */

void run_gjk_tests(void)
{
    SDL_Log("\n=== GJK Intersection Tests ===\n");

    /* Minkowski support */
    test_gjk_support_sphere_pair();
    test_gjk_support_box_pair();

    /* Basic intersection / separation */
    test_gjk_overlapping_spheres();
    test_gjk_separated_spheres();
    test_gjk_touching_spheres();

    /* All 9 shape-pair combos */
    test_gjk_sphere_sphere_overlap();
    test_gjk_sphere_sphere_separated();
    test_gjk_sphere_box_overlap();
    test_gjk_sphere_box_separated();
    test_gjk_sphere_capsule_overlap();
    test_gjk_sphere_capsule_separated();
    test_gjk_box_box_overlap();
    test_gjk_box_box_separated();
    test_gjk_box_sphere_overlap();
    test_gjk_box_sphere_separated();
    test_gjk_box_capsule_overlap();
    test_gjk_box_capsule_separated();
    test_gjk_capsule_capsule_overlap();
    test_gjk_capsule_capsule_separated();
    test_gjk_capsule_sphere_overlap();
    test_gjk_capsule_sphere_separated();
    test_gjk_capsule_box_overlap();
    test_gjk_capsule_box_separated();

    /* Edge cases */
    test_gjk_coincident_centers();
    test_gjk_rotated_obb_overlap();
    test_gjk_large_separation();
    test_gjk_near_miss();
    test_gjk_deep_penetration();
    test_gjk_collinear_aligned_overlap();
    test_gjk_collinear_large_scale();
    test_gjk_coplanar_touching_triangle();
    test_gjk_non_unit_quaternion();

    /* Convergence */
    test_gjk_converges_within_max();
    test_gjk_simple_converges_fast();

    /* Determinism */
    test_gjk_determinism();

    /* Simplex validity for EPA */
    test_gjk_simplex_valid_on_hit();
    test_gjk_simplex_valid_on_miss();

    /* Degenerate inputs */
    test_gjk_null_shape_a();
    test_gjk_null_shape_b();
    test_gjk_nan_position();
    test_gjk_invalid_shape();
    test_gjk_zero_volume_shape();

    /* Convenience function */
    test_gjk_test_bodies_matches();
    test_gjk_test_bodies_null_body();

    /* Support function input validation */
    test_gjk_support_null_shape_a();
    test_gjk_support_null_shape_b();
    test_gjk_support_invalid_shape();
    test_gjk_support_nan_position();
    test_gjk_support_inf_position();
    test_gjk_support_zero_direction();
    test_gjk_support_near_zero_direction();
    test_gjk_support_nan_direction();
    test_gjk_support_zero_quat();

    /* EPA simplex contract */
    test_gjk_coincident_spheres_simplex_count();
    test_gjk_touching_simplex_count();
    test_gjk_deep_overlap_simplex_valid();

    /* Coplanar tetrahedron / division-by-zero guards */
    test_gjk_flat_box_overlap_y();
    test_gjk_flat_box_overlap_x();
    test_gjk_flat_box_overlap_z();
    test_gjk_flat_box_separated();
    test_gjk_flat_box_coincident();
    test_gjk_flat_box_exact_touch();
    test_gjk_flat_box_rotated();
    test_gjk_flat_box_diagonal_overlap();
    test_gjk_flat_sphere_vs_flat_box();
    test_gjk_flat_capsule_vs_flat_box();
    test_gjk_micro_box_overlap();
    test_gjk_micro_sphere_overlap();
    test_gjk_flat_box_stability_sweep();
    test_gjk_flat_box_determinism();

    /* INF overflow guards */
    test_gjk_support_overflow_zeroed();
    test_gjk_support_direction_overflow_zeroed();
    test_gjk_intersect_overflow_no_false_positive();
    test_gjk_invalid_input_no_false_positive();

    /* Simplex inflation for EPA */
    test_gjk_intersection_always_tetrahedron();
}
