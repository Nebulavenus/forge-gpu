/*
 * EPA Penetration Depth Tests
 *
 * Tests for forge_physics_epa_* functions in common/physics/forge_physics.h
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Shared test constants ─────────────────────────────────────────────── */

#define EPA_SPHERE_RADIUS       1.0f
#define EPA_BOX_HALF            1.0f
#define EPA_CAPSULE_RADIUS      0.5f
#define EPA_CAPSULE_HALF_H      1.0f

/* Overlap distances — shapes placed this far apart overlap */
#define EPA_OVERLAP_DIST              1.5f  /* sphere-sphere: 2*r - 1.5 = 0.5 */
#define EPA_BOX_OVERLAP_DIST          1.5f  /* box-box: 2*half - 1.5 = 0.5 */
#define EPA_CAPSULE_OVERLAP_DIST      0.7f  /* capsule-capsule: 2*0.5 - 0.7 = 0.3 */
#define EPA_SPHERE_BOX_OVERLAP_DIST   1.5f  /* sphere(r=1)+box(h=1) on X axis */
#define EPA_MIXED_OVERLAP_DIST        1.2f  /* sphere/box vs capsule on X axis */
#define EPA_ROTATED_OVERLAP_DIST      1.0f  /* overlap distance for rotated shape tests */
#define EPA_SEPARATED_DIST            5.0f  /* well-separated (no overlap) */
#define EPA_COINCIDENT_TOL            0.15f /* wider tolerance for coincident shapes */

/* Rigid body test parameters (for combined pipeline tests) */
#define EPA_BODY_MASS           1.0f
#define EPA_BODY_DAMPING        0.01f
#define EPA_BODY_RESTIT         0.5f
#define EPA_BODY_MU_S           0.6f
#define EPA_BODY_MU_D           0.4f

/* Tolerances */
#define EPA_DEPTH_TOL           0.05f   /* depth accuracy tolerance */
#define EPA_NORMAL_TOL          0.02f   /* normal unit-length tolerance */
#define EPA_POINT_TOL           0.1f    /* contact point tolerance */
#define EPA_MIDPOINT_TOL        1e-4f   /* midpoint identity tolerance (tight) */

/* Convergence */
/* Spheres are the hardest shape for EPA — the Minkowski difference
 * boundary is curved, requiring many iterations to approximate.
 * Flat-faced shapes (boxes) converge much faster. This limit just
 * verifies EPA terminates within its maximum iteration budget. */
#define EPA_FAST_ITER_LIMIT     FORGE_PHYSICS_EPA_MAX_ITERATIONS

/* ── Helpers ────────────────────────────────────────────────────────────── */

/* Create shapes using the library constructors */
static ForgePhysicsCollisionShape epa_sphere(float radius)
{
    return forge_physics_shape_sphere(radius);
}

static ForgePhysicsCollisionShape epa_box(float hx, float hy, float hz)
{
    return forge_physics_shape_box(vec3_create(hx, hy, hz));
}

static ForgePhysicsCollisionShape epa_capsule(float radius, float half_h)
{
    return forge_physics_shape_capsule(radius, half_h);
}

/* Run GJK then EPA for two shapes at given positions (identity orientation) */
static ForgePhysicsEPAResult epa_test(
    ForgePhysicsCollisionShape *sa, vec3 pa,
    ForgePhysicsCollisionShape *sb, vec3 pb)
{
    quat identity = quat_identity();
    ForgePhysicsGJKResult gjk = forge_physics_gjk_intersect(
        sa, pa, identity, sb, pb, identity);
    if (!gjk.intersecting) {
        ForgePhysicsEPAResult empty;
        SDL_memset(&empty, 0, sizeof(empty));
        return empty;
    }
    return forge_physics_epa(&gjk, sa, pa, identity, sb, pb, identity);
}

/* ── Basic convergence tests ───────────────────────────────────────────── */

static void test_epa_sphere_sphere_overlap(void)
{
    TEST("EPA_sphere_sphere_overlap")
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.depth > 0.0f);
    ASSERT_TRUE(r.iterations > 0);
    END_TEST();
}

static void test_epa_box_box_overlap(void)
{
    TEST("EPA_box_box_overlap")
    ForgePhysicsCollisionShape sa = epa_box(EPA_BOX_HALF, EPA_BOX_HALF, EPA_BOX_HALF);
    ForgePhysicsCollisionShape sb = epa_box(EPA_BOX_HALF, EPA_BOX_HALF, EPA_BOX_HALF);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_BOX_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.depth > 0.0f);
    END_TEST();
}

static void test_epa_capsule_capsule_overlap(void)
{
    TEST("EPA_capsule_capsule_overlap")
    ForgePhysicsCollisionShape sa = epa_capsule(EPA_CAPSULE_RADIUS, EPA_CAPSULE_HALF_H);
    ForgePhysicsCollisionShape sb = epa_capsule(EPA_CAPSULE_RADIUS, EPA_CAPSULE_HALF_H);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_CAPSULE_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.depth > 0.0f);
    END_TEST();
}

static void test_epa_sphere_box_overlap(void)
{
    TEST("EPA_sphere_box_overlap")
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_box(EPA_BOX_HALF, EPA_BOX_HALF, EPA_BOX_HALF);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_SPHERE_BOX_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.depth > 0.0f);
    END_TEST();
}

static void test_epa_sphere_capsule_overlap(void)
{
    TEST("EPA_sphere_capsule_overlap")
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_capsule(EPA_CAPSULE_RADIUS, EPA_CAPSULE_HALF_H);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_MIXED_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.depth > 0.0f);
    END_TEST();
}

static void test_epa_box_capsule_overlap(void)
{
    TEST("EPA_box_capsule_overlap")
    ForgePhysicsCollisionShape sa = epa_box(EPA_BOX_HALF, EPA_BOX_HALF, EPA_BOX_HALF);
    ForgePhysicsCollisionShape sb = epa_capsule(EPA_CAPSULE_RADIUS, EPA_CAPSULE_HALF_H);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_MIXED_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.depth > 0.0f);
    END_TEST();
}

/* ── Depth accuracy tests ──────────────────────────────────────────────── */

static void test_epa_sphere_sphere_depth_accuracy(void)
{
    TEST("EPA_sphere_sphere_depth_accuracy")
    /* Analytical depth = (r1 + r2) - distance */
    float r = EPA_SPHERE_RADIUS;
    float distances[] = { 1.8f, 1.5f, 1.0f, 0.5f };
    int count = (int)(sizeof(distances) / sizeof(distances[0]));

    for (int i = 0; i < count; i++) {
        float dist = distances[i];
        float expected_depth = 2.0f * r - dist;
        if (expected_depth <= 0.0f) continue;

        ForgePhysicsCollisionShape sa = epa_sphere(r);
        ForgePhysicsCollisionShape sb = epa_sphere(r);
        vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
        vec3 pb = vec3_create(dist, 0.0f, 0.0f);

        ForgePhysicsEPAResult res = epa_test(&sa, pa, &sb, pb);
        ASSERT_TRUE(res.valid);
        ASSERT_NEAR(res.depth, expected_depth, EPA_DEPTH_TOL);
    }
    END_TEST();
}

static void test_epa_box_box_axis_aligned_depth(void)
{
    TEST("EPA_box_box_axis_aligned_depth")
    /* Axis-aligned boxes: depth on X axis = 2*half - separation */
    float half = EPA_BOX_HALF;
    float sep = EPA_BOX_OVERLAP_DIST;
    float expected_depth = 2.0f * half - sep; /* 0.5 */

    ForgePhysicsCollisionShape sa = epa_box(half, half, half);
    ForgePhysicsCollisionShape sb = epa_box(half, half, half);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(sep, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    ASSERT_NEAR(r.depth, expected_depth, EPA_DEPTH_TOL);
    END_TEST();
}

/* ── Normal direction tests ────────────────────────────────────────────── */

static void test_epa_normal_points_b_to_a(void)
{
    TEST("EPA_normal_points_B_to_A")
    /* Shape A at origin, shape B at +X. Normal should point in -X
     * direction (from B toward A). */
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    /* Normal should have a negative X component (pointing from B toward A) */
    ASSERT_TRUE(r.normal.x < -0.5f);
    END_TEST();
}

static void test_epa_normal_is_unit(void)
{
    TEST("EPA_normal_is_unit")
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    float len = vec3_length(r.normal);
    ASSERT_NEAR(len, 1.0f, EPA_NORMAL_TOL);
    END_TEST();
}

/* ── Contact point tests ───────────────────────────────────────────────── */

static void test_epa_contact_midpoint(void)
{
    TEST("EPA_contact_midpoint")
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);

    /* Midpoint should be average of point_a and point_b */
    vec3 expected_mid = vec3_scale(vec3_add(r.point_a, r.point_b), 0.5f);
    ASSERT_NEAR(r.point.x, expected_mid.x, EPA_MIDPOINT_TOL);
    ASSERT_NEAR(r.point.y, expected_mid.y, EPA_MIDPOINT_TOL);
    ASSERT_NEAR(r.point.z, expected_mid.z, EPA_MIDPOINT_TOL);
    END_TEST();
}

static void test_epa_contact_points_reasonable(void)
{
    TEST("EPA_contact_points_reasonable")
    /* Contact points should be between the two shape centers */
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);

    /* point_a should be near shape A's surface in the direction of B */
    ASSERT_TRUE(r.point_a.x > -EPA_SPHERE_RADIUS - EPA_POINT_TOL);
    ASSERT_TRUE(r.point_a.x <  EPA_SPHERE_RADIUS + EPA_POINT_TOL);

    /* point_b should be near shape B's surface in the direction of A */
    ASSERT_TRUE(r.point_b.x > EPA_OVERLAP_DIST - EPA_SPHERE_RADIUS - EPA_POINT_TOL);
    ASSERT_TRUE(r.point_b.x < EPA_OVERLAP_DIST + EPA_SPHERE_RADIUS + EPA_POINT_TOL);
    END_TEST();
}

/* ── Rotated shape tests ───────────────────────────────────────────────── */

static void test_epa_rotated_box_box(void)
{
    TEST("EPA_rotated_box_box")
    ForgePhysicsCollisionShape sa = epa_box(EPA_BOX_HALF, EPA_BOX_HALF, EPA_BOX_HALF);
    ForgePhysicsCollisionShape sb = epa_box(EPA_BOX_HALF, EPA_BOX_HALF, EPA_BOX_HALF);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_ROTATED_OVERLAP_DIST, 0.0f, 0.0f);
    quat orient_a = quat_identity();
    quat orient_b = quat_from_euler(0.0f, 45.0f * FORGE_DEG2RAD, 0.0f);

    ForgePhysicsGJKResult gjk = forge_physics_gjk_intersect(
        &sa, pa, orient_a, &sb, pb, orient_b);
    ASSERT_TRUE(gjk.intersecting);

    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, pa, orient_a, &sb, pb, orient_b);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.depth > 0.0f);
    /* Normal should be unit length */
    ASSERT_NEAR(vec3_length(r.normal), 1.0f, EPA_NORMAL_TOL);
    END_TEST();
}

static void test_epa_rotated_capsule_sphere(void)
{
    TEST("EPA_rotated_capsule_sphere")
    ForgePhysicsCollisionShape sa = epa_capsule(EPA_CAPSULE_RADIUS, EPA_CAPSULE_HALF_H);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_ROTATED_OVERLAP_DIST, 0.0f, 0.0f);
    quat orient_a = quat_from_euler(0.0f, 0.0f, 45.0f * FORGE_DEG2RAD);
    quat orient_b = quat_identity();

    ForgePhysicsGJKResult gjk = forge_physics_gjk_intersect(
        &sa, pa, orient_a, &sb, pb, orient_b);
    ASSERT_TRUE(gjk.intersecting);

    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, pa, orient_a, &sb, pb, orient_b);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.depth > 0.0f);
    END_TEST();
}

/* ── Edge case tests ───────────────────────────────────────────────────── */

static void test_epa_coincident_shapes(void)
{
    TEST("EPA_coincident_shapes")
    /* Same position — deep penetration */
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(0.0f, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    /* Depth should be approximately 2*radius. Coincident shapes are the
     * hardest case for EPA — the initial GJK tetrahedron may be poorly
     * shaped, and convergence is slower. Use a wider tolerance. */
    ASSERT_NEAR(r.depth, 2.0f * EPA_SPHERE_RADIUS, EPA_COINCIDENT_TOL);
    END_TEST();
}

static void test_epa_non_intersecting_returns_invalid(void)
{
    TEST("EPA_non_intersecting_returns_invalid")
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_SEPARATED_DIST, 0.0f, 0.0f);

    quat identity = quat_identity();
    ForgePhysicsGJKResult gjk = forge_physics_gjk_intersect(
        &sa, pa, identity, &sb, pb, identity);
    ASSERT_TRUE(!gjk.intersecting);

    /* EPA should return invalid when given a non-intersecting result */
    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, pa, identity, &sb, pb, identity);
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_null_inputs(void)
{
    TEST("EPA_null_inputs")
    ForgePhysicsEPAResult r;

    /* NULL gjk_result */
    ForgePhysicsCollisionShape sa = epa_sphere(1.0f);
    ForgePhysicsCollisionShape sb = epa_sphere(1.0f);
    vec3 p = vec3_create(0.0f, 0.0f, 0.0f);
    quat q = quat_identity();
    r = forge_physics_epa(NULL, &sa, p, q, &sb, p, q);
    ASSERT_TRUE(!r.valid);

    /* NULL shape_a */
    ForgePhysicsGJKResult gjk;
    SDL_memset(&gjk, 0, sizeof(gjk));
    gjk.intersecting = true;
    gjk.simplex.count = 4;
    r = forge_physics_epa(&gjk, NULL, p, q, &sb, p, q);
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_simplex_count_not_4(void)
{
    TEST("EPA_simplex_count_not_4")
    ForgePhysicsGJKResult gjk;
    SDL_memset(&gjk, 0, sizeof(gjk));
    gjk.intersecting = true;
    gjk.simplex.count = 3; /* not 4 */

    ForgePhysicsCollisionShape sa = epa_sphere(1.0f);
    ForgePhysicsCollisionShape sb = epa_sphere(1.0f);
    vec3 p = vec3_create(0.0f, 0.0f, 0.0f);
    quat q = quat_identity();
    ForgePhysicsEPAResult r = forge_physics_epa(&gjk, &sa, p, q, &sb, p, q);
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

/* ── Combined GJK+EPA pipeline tests ──────────────────────────────────── */

static void test_epa_gjk_epa_contact_sphere_sphere(void)
{
    TEST("EPA_gjk_epa_contact_sphere_sphere")
    ForgePhysicsRigidBody body_a = forge_physics_rigid_body_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    ForgePhysicsRigidBody body_b = forge_physics_rigid_body_create(
        vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);

    ForgePhysicsRBContact contact;
    bool got_contact = forge_physics_gjk_epa_contact(
        &body_a, &sa, &body_b, &sb, 0, 1,
        EPA_BODY_MU_S, EPA_BODY_MU_D, &contact);

    ASSERT_TRUE(got_contact);
    ASSERT_TRUE(contact.penetration > 0.0f);
    ASSERT_TRUE(contact.body_a == 0);
    ASSERT_TRUE(contact.body_b == 1);
    ASSERT_NEAR(contact.static_friction, EPA_BODY_MU_S, EPSILON);
    ASSERT_NEAR(contact.dynamic_friction, EPA_BODY_MU_D, EPSILON);
    END_TEST();
}

static void test_epa_gjk_epa_no_contact_separated(void)
{
    TEST("EPA_gjk_epa_no_contact_separated")
    ForgePhysicsRigidBody body_a = forge_physics_rigid_body_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    ForgePhysicsRigidBody body_b = forge_physics_rigid_body_create(
        vec3_create(EPA_SEPARATED_DIST, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);

    ForgePhysicsRBContact contact;
    bool got_contact = forge_physics_gjk_epa_contact(
        &body_a, &sa, &body_b, &sb, 0, 1,
        EPA_BODY_MU_S, EPA_BODY_MU_D, &contact);
    ASSERT_TRUE(!got_contact);
    END_TEST();
}

/* ── Determinism test ──────────────────────────────────────────────────── */

static void test_epa_deterministic(void)
{
    TEST("EPA_deterministic")
    ForgePhysicsCollisionShape sa = epa_box(0.8f, 0.8f, 0.8f);
    ForgePhysicsCollisionShape sb = epa_sphere(1.2f);
    vec3 pa = vec3_create(0.3f, 0.1f, -0.2f);
    vec3 pb = vec3_create(1.0f, 0.0f, 0.0f);

    ForgePhysicsEPAResult first = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(first.valid);

    /* Run 100 more times — all must produce identical results */
    for (int i = 0; i < 100; i++) {
        ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
        ASSERT_TRUE(r.valid);
        ASSERT_TRUE(r.depth == first.depth);
        ASSERT_TRUE(r.normal.x == first.normal.x);
        ASSERT_TRUE(r.normal.y == first.normal.y);
        ASSERT_TRUE(r.normal.z == first.normal.z);
        ASSERT_TRUE(r.point.x == first.point.x);
        ASSERT_TRUE(r.point.y == first.point.y);
        ASSERT_TRUE(r.point.z == first.point.z);
    }
    END_TEST();
}

/* ── Convergence speed test ────────────────────────────────────────────── */

static void test_epa_terminates_within_budget(void)
{
    TEST("EPA_terminates_within_budget")
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.iterations <= EPA_FAST_ITER_LIMIT);
    END_TEST();
}

/* ── NaN / Infinity safety tests ───────────────────────────────────────── */

/* Helper: build a valid GJK result for overlapping spheres so EPA can be
 * called directly with controlled parameters. Returns an intersecting
 * result with simplex.count == 4 (the full-tetrahedron contract that EPA
 * requires). */
static ForgePhysicsGJKResult epa_make_valid_gjk(void)
{
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);
    quat identity = quat_identity();
    return forge_physics_gjk_intersect(
        &sa, pa, identity, &sb, pb, identity);
}

static void test_epa_nan_pos_a(void)
{
    TEST("EPA_nan_pos_a")
    ForgePhysicsGJKResult gjk = epa_make_valid_gjk();
    ASSERT_TRUE(gjk.intersecting);
    ASSERT_TRUE(gjk.simplex.count == 4);

    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 nan_pos = vec3_create(NAN, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);
    quat identity = quat_identity();

    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, nan_pos, identity, &sb, pb, identity);
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_nan_pos_b(void)
{
    TEST("EPA_nan_pos_b")
    ForgePhysicsGJKResult gjk = epa_make_valid_gjk();
    ASSERT_TRUE(gjk.intersecting);

    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 nan_pos = vec3_create(0.0f, NAN, 0.0f);
    quat identity = quat_identity();

    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, pa, identity, &sb, nan_pos, identity);
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_inf_pos(void)
{
    TEST("EPA_inf_pos")
    ForgePhysicsGJKResult gjk = epa_make_valid_gjk();
    ASSERT_TRUE(gjk.intersecting);

    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 inf_pos = vec3_create(INFINITY, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);
    quat identity = quat_identity();

    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, inf_pos, identity, &sb, pb, identity);
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_nan_orient_a(void)
{
    TEST("EPA_nan_orient_a")
    ForgePhysicsGJKResult gjk = epa_make_valid_gjk();
    ASSERT_TRUE(gjk.intersecting);

    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);
    quat nan_orient = { NAN, 0.0f, 0.0f, 0.0f };

    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, pa, nan_orient, &sb, pb, quat_identity());
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_zero_orient(void)
{
    TEST("EPA_zero_orient")
    ForgePhysicsGJKResult gjk = epa_make_valid_gjk();
    ASSERT_TRUE(gjk.intersecting);

    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);
    quat zero_orient = { 0.0f, 0.0f, 0.0f, 0.0f };

    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, pa, zero_orient, &sb, pb, quat_identity());
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_nonunit_orient_normalized(void)
{
    TEST("EPA_nonunit_orient_normalized")
    /* EPA normalizes quaternions via gjk_validate_quat_ (same as GJK).
     * A scaled-but-valid quaternion should be normalized and produce
     * a valid result. Uses a box (not sphere) so orientation actually
     * affects the support function — a sphere is rotationally symmetric
     * and would pass even without normalization. */
    ForgePhysicsCollisionShape sa = epa_box(EPA_BOX_HALF, EPA_BOX_HALF, EPA_BOX_HALF);
    ForgePhysicsCollisionShape sb = epa_box(EPA_BOX_HALF, EPA_BOX_HALF, EPA_BOX_HALF);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_BOX_OVERLAP_DIST, 0.0f, 0.0f);
    /* Quaternion with length-squared ~4.0 — EPA normalizes to identity */
    quat scaled_orient = { 2.0f, 0.0f, 0.0f, 0.0f };

    /* Run GJK with the scaled orient — GJK also normalizes */
    ForgePhysicsGJKResult gjk = forge_physics_gjk_intersect(
        &sa, pa, scaled_orient, &sb, pb, quat_identity());
    ASSERT_TRUE(gjk.intersecting);

    /* EPA should normalize and produce a valid result */
    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, pa, scaled_orient, &sb, pb, quat_identity());
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(r.depth > 0.0f);
    END_TEST();
}

static void test_epa_nan_simplex_vertex(void)
{
    TEST("EPA_nan_simplex_vertex")
    ForgePhysicsGJKResult gjk = epa_make_valid_gjk();
    ASSERT_TRUE(gjk.intersecting);

    /* Corrupt one simplex vertex */
    gjk.simplex.verts[2].point.x = NAN;

    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);
    quat identity = quat_identity();

    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, pa, identity, &sb, pb, identity);
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_inf_simplex_vertex(void)
{
    TEST("EPA_inf_simplex_vertex")
    ForgePhysicsGJKResult gjk = epa_make_valid_gjk();
    ASSERT_TRUE(gjk.intersecting);

    /* Corrupt one simplex vertex */
    gjk.simplex.verts[0].point.y = INFINITY;

    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);
    quat identity = quat_identity();

    ForgePhysicsEPAResult r = forge_physics_epa(
        &gjk, &sa, pa, identity, &sb, pb, identity);
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_bodies_nan_position(void)
{
    TEST("EPA_bodies_nan_position")
    ForgePhysicsGJKResult gjk = epa_make_valid_gjk();
    ASSERT_TRUE(gjk.intersecting);

    /* Create valid bodies, then inject NaN directly into position
     * (rigid_body_create sanitizes NaN to zero, so we must corrupt after) */
    ForgePhysicsRigidBody body_a = forge_physics_rigid_body_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    body_a.position.x = NAN;
    ForgePhysicsRigidBody body_b = forge_physics_rigid_body_create(
        vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);

    ForgePhysicsEPAResult r = forge_physics_epa_bodies(
        &gjk, &body_a, &sa, &body_b, &sb);
    ASSERT_TRUE(!r.valid);
    END_TEST();
}

static void test_epa_gjk_epa_contact_nan_friction(void)
{
    TEST("EPA_gjk_epa_contact_nan_friction")
    ForgePhysicsRigidBody body_a = forge_physics_rigid_body_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    ForgePhysicsRigidBody body_b = forge_physics_rigid_body_create(
        vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);

    ForgePhysicsRBContact contact;
    bool got = forge_physics_gjk_epa_contact(
        &body_a, &sa, &body_b, &sb, 0, 1, NAN, INFINITY, &contact);
    /* Contact should still be generated — NaN/inf friction clamped to 0 */
    ASSERT_TRUE(got);
    ASSERT_NEAR(contact.static_friction, 0.0f, EPSILON);
    ASSERT_NEAR(contact.dynamic_friction, 0.0f, EPSILON);
    END_TEST();
}

static void test_epa_gjk_epa_contact_negative_friction(void)
{
    TEST("EPA_gjk_epa_contact_negative_friction")
    ForgePhysicsRigidBody body_a = forge_physics_rigid_body_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    ForgePhysicsRigidBody body_b = forge_physics_rigid_body_create(
        vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f),
        EPA_BODY_MASS, EPA_BODY_DAMPING, EPA_BODY_DAMPING, EPA_BODY_RESTIT);
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);

    ForgePhysicsRBContact contact;
    bool got = forge_physics_gjk_epa_contact(
        &body_a, &sa, &body_b, &sb, 0, 1, -1.0f, -0.5f, &contact);
    ASSERT_TRUE(got);
    ASSERT_NEAR(contact.static_friction, 0.0f, EPSILON);
    ASSERT_NEAR(contact.dynamic_friction, 0.0f, EPSILON);
    END_TEST();
}

static void test_epa_result_outputs_are_finite(void)
{
    TEST("EPA_result_outputs_are_finite")
    /* Verify that a valid EPA result always has finite outputs */
    ForgePhysicsCollisionShape sa = epa_sphere(EPA_SPHERE_RADIUS);
    ForgePhysicsCollisionShape sb = epa_sphere(EPA_SPHERE_RADIUS);
    vec3 pa = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 pb = vec3_create(EPA_OVERLAP_DIST, 0.0f, 0.0f);

    ForgePhysicsEPAResult r = epa_test(&sa, pa, &sb, pb);
    ASSERT_TRUE(r.valid);
    ASSERT_TRUE(forge_isfinite(r.depth));
    ASSERT_TRUE(forge_isfinite(r.normal.x));
    ASSERT_TRUE(forge_isfinite(r.normal.y));
    ASSERT_TRUE(forge_isfinite(r.normal.z));
    ASSERT_TRUE(forge_isfinite(r.point_a.x));
    ASSERT_TRUE(forge_isfinite(r.point_a.y));
    ASSERT_TRUE(forge_isfinite(r.point_a.z));
    ASSERT_TRUE(forge_isfinite(r.point_b.x));
    ASSERT_TRUE(forge_isfinite(r.point_b.y));
    ASSERT_TRUE(forge_isfinite(r.point_b.z));
    ASSERT_TRUE(forge_isfinite(r.point.x));
    ASSERT_TRUE(forge_isfinite(r.point.y));
    ASSERT_TRUE(forge_isfinite(r.point.z));
    END_TEST();
}

/* ── Test runner ───────────────────────────────────────────────────────── */

void run_epa_tests(void)
{
    SDL_Log("\n── EPA Penetration Depth Tests ──────────────────────────");

    /* Basic convergence */
    test_epa_sphere_sphere_overlap();
    test_epa_box_box_overlap();
    test_epa_capsule_capsule_overlap();
    test_epa_sphere_box_overlap();
    test_epa_sphere_capsule_overlap();
    test_epa_box_capsule_overlap();

    /* Depth accuracy */
    test_epa_sphere_sphere_depth_accuracy();
    test_epa_box_box_axis_aligned_depth();

    /* Normal direction */
    test_epa_normal_points_b_to_a();
    test_epa_normal_is_unit();

    /* Contact points */
    test_epa_contact_midpoint();
    test_epa_contact_points_reasonable();

    /* Rotated shapes */
    test_epa_rotated_box_box();
    test_epa_rotated_capsule_sphere();

    /* Edge cases */
    test_epa_coincident_shapes();
    test_epa_non_intersecting_returns_invalid();
    test_epa_null_inputs();
    test_epa_simplex_count_not_4();

    /* Combined pipeline */
    test_epa_gjk_epa_contact_sphere_sphere();
    test_epa_gjk_epa_no_contact_separated();

    /* NaN / Infinity safety */
    test_epa_nan_pos_a();
    test_epa_nan_pos_b();
    test_epa_inf_pos();
    test_epa_nan_orient_a();
    test_epa_zero_orient();
    test_epa_nonunit_orient_normalized();
    test_epa_nan_simplex_vertex();
    test_epa_inf_simplex_vertex();
    test_epa_bodies_nan_position();
    test_epa_gjk_epa_contact_nan_friction();
    test_epa_gjk_epa_contact_negative_friction();
    test_epa_result_outputs_are_finite();

    /* Determinism */
    test_epa_deterministic();

    /* Termination */
    test_epa_terminates_within_budget();
}
