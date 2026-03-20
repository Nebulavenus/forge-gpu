/*
 * Physics Library Tests — Collision Shapes and Support Functions
 *
 * Tests for shape creation, validation, capsule inertia, inertia-from-shape
 * dispatch, support functions (sphere, box, capsule), AABB computation,
 * AABB overlap, and AABB utilities.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Test constants for collision shapes ────────────────────────────────── */

#define CS_SPHERE_RADIUS      0.5f
#define CS_BOX_HALF           0.5f
#define CS_BOX_HALF_X         0.5f
#define CS_BOX_HALF_Y         0.25f
#define CS_BOX_HALF_Z         1.0f
#define CS_CAPSULE_RADIUS     0.3f
#define CS_CAPSULE_HALF_H     0.5f
#define CS_MASS               5.0f
#define CS_DAMPING            0.99f
#define CS_ANG_DAMPING        0.99f
#define CS_RESTIT             0.5f
#define CS_NEG_RADIUS        -0.5f
#define CS_NEG_HALF          -0.3f
#define CS_SUPPORT_EPS        0.01f
#define CS_AABB_EPS           0.01f
#define CS_INERTIA_EPS        0.001f
#define CS_DETERMINISM_ITERS  1000
#define CS_EXPAND_MARGIN      0.1f
#define CS_TRANSLATE_X        3.0f
#define CS_TRANSLATE_Y        2.0f
#define CS_TRANSLATE_Z       -1.0f
#define CS_ROT_45_DEG         (45.0f * FORGE_DEG2RAD)
#define CS_ROT_90_DEG         (90.0f * FORGE_DEG2RAD)
#define CS_SQRT2_INV          0.70710678f
#define CS_SQRT3_INV          0.57735027f
#define CS_INVALID_SHAPE_TYPE 99           /* out-of-range enum value for validation tests */
#define CS_UNIT_DIM           1.0f         /* unit radius/half-height for symmetry tests */
#define CS_SMALL_DIM          0.001f       /* small-but-valid dimension for degenerate tests */
#define CS_BELOW_MIN_DIM      1e-7f        /* below FORGE_PHYSICS_SHAPE_MIN_DIM for clamp tests */
#define CS_ZERO_DIR_POS_X     1.0f         /* position for zero-direction support tests */
#define CS_ZERO_DIR_POS_Y     2.0f
#define CS_ZERO_DIR_POS_Z     3.0f
#define CS_DET_POS_X          1.5f         /* determinism test reference position */
#define CS_DET_POS_Y         -0.3f
#define CS_DET_POS_Z          2.7f
#define CS_DET_YAW            0.5f         /* determinism test reference orientation */
#define CS_DET_PITCH          0.3f
#define CS_DET_ROLL           0.1f
#define CS_DET_DIR_X          0.6f         /* determinism test reference direction */
#define CS_DET_DIR_Y         -0.7f
#define CS_DET_DIR_Z          0.38f
#define CS_NONUNIT_QUAT_SCALE 2.5f         /* scale factor for non-unit quaternion tests */

/* Helper: create a dynamic body with given mass */
static ForgePhysicsRigidBody cs_make_body(vec3 pos)
{
    return forge_physics_rigid_body_create(
        pos, CS_MASS, CS_DAMPING, CS_ANG_DAMPING, CS_RESTIT);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Shape Creation Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_shape_create_sphere(void)
{
    TEST("CS_shape_create_sphere")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    ASSERT_TRUE(s.type == FORGE_PHYSICS_SHAPE_SPHERE);
    ASSERT_NEAR(s.data.sphere.radius, CS_SPHERE_RADIUS, EPSILON);
    END_TEST();
}

static void test_shape_create_box(void)
{
    TEST("CS_shape_create_box")
    vec3 he = vec3_create(CS_BOX_HALF_X, CS_BOX_HALF_Y, CS_BOX_HALF_Z);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    ASSERT_TRUE(s.type == FORGE_PHYSICS_SHAPE_BOX);
    ASSERT_NEAR(s.data.box.half_extents.x, CS_BOX_HALF_X, EPSILON);
    ASSERT_NEAR(s.data.box.half_extents.y, CS_BOX_HALF_Y, EPSILON);
    ASSERT_NEAR(s.data.box.half_extents.z, CS_BOX_HALF_Z, EPSILON);
    END_TEST();
}

static void test_shape_create_capsule(void)
{
    TEST("CS_shape_create_capsule")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    ASSERT_TRUE(s.type == FORGE_PHYSICS_SHAPE_CAPSULE);
    ASSERT_NEAR(s.data.capsule.radius, CS_CAPSULE_RADIUS, EPSILON);
    ASSERT_NEAR(s.data.capsule.half_height, CS_CAPSULE_HALF_H, EPSILON);
    END_TEST();
}

static void test_shape_create_box_negative_clamped(void)
{
    TEST("CS_shape_create_box_negative_clamped")
    vec3 he = vec3_create(CS_NEG_HALF, CS_NEG_HALF, CS_NEG_HALF);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    ASSERT_TRUE(s.data.box.half_extents.x > 0.0f);
    ASSERT_TRUE(s.data.box.half_extents.y > 0.0f);
    ASSERT_TRUE(s.data.box.half_extents.z > 0.0f);
    END_TEST();
}

static void test_shape_create_capsule_negative_clamped(void)
{
    TEST("CS_shape_create_capsule_negative_clamped")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_NEG_RADIUS, CS_NEG_HALF);
    ASSERT_TRUE(s.data.capsule.radius > 0.0f);
    ASSERT_TRUE(s.data.capsule.half_height > 0.0f);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Shape Validity Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_shape_valid_sphere(void)
{
    TEST("CS_shape_valid_sphere")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    ASSERT_TRUE(forge_physics_shape_is_valid(&s));
    END_TEST();
}

static void test_shape_valid_box(void)
{
    TEST("CS_shape_valid_box")
    vec3 he = vec3_create(CS_BOX_HALF, CS_BOX_HALF, CS_BOX_HALF);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    ASSERT_TRUE(forge_physics_shape_is_valid(&s));
    END_TEST();
}

static void test_shape_valid_capsule(void)
{
    TEST("CS_shape_valid_capsule")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    ASSERT_TRUE(forge_physics_shape_is_valid(&s));
    END_TEST();
}

static void test_shape_invalid_type(void)
{
    TEST("CS_shape_invalid_type")
    ForgePhysicsCollisionShape s;
    s.type = (ForgePhysicsShapeType)CS_INVALID_SHAPE_TYPE;
    ASSERT_TRUE(!forge_physics_shape_is_valid(&s));
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Capsule Inertia Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_capsule_inertia_basic(void)
{
    TEST("CS_capsule_inertia_basic")
    ForgePhysicsRigidBody rb = cs_make_body(vec3_create(0, 0, 0));
    forge_physics_rigid_body_set_inertia_capsule(&rb,
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    /* Capsule has Ixx = Izz (symmetric about Y) and Iyy < Ixx */
    float Ixx = rb.inertia_local.m[0];
    float Iyy = rb.inertia_local.m[4];
    float Izz = rb.inertia_local.m[8];
    ASSERT_TRUE(Ixx > 0.0f);
    ASSERT_TRUE(Iyy > 0.0f);
    ASSERT_NEAR(Ixx, Izz, CS_INERTIA_EPS);
    /* Easier to spin around symmetry axis (Y) than perpendicular axes */
    ASSERT_TRUE(Iyy < Ixx);
    END_TEST();
}

static void test_capsule_inertia_symmetry(void)
{
    TEST("CS_capsule_inertia_symmetry")
    ForgePhysicsRigidBody rb = cs_make_body(vec3_create(0, 0, 0));
    forge_physics_rigid_body_set_inertia_capsule(&rb, CS_UNIT_DIM, CS_UNIT_DIM);
    /* Off-diagonal elements should be zero for principal axes */
    ASSERT_NEAR(rb.inertia_local.m[1], 0.0f, CS_INERTIA_EPS);
    ASSERT_NEAR(rb.inertia_local.m[2], 0.0f, CS_INERTIA_EPS);
    ASSERT_NEAR(rb.inertia_local.m[3], 0.0f, CS_INERTIA_EPS);
    ASSERT_NEAR(rb.inertia_local.m[5], 0.0f, CS_INERTIA_EPS);
    ASSERT_NEAR(rb.inertia_local.m[6], 0.0f, CS_INERTIA_EPS);
    ASSERT_NEAR(rb.inertia_local.m[7], 0.0f, CS_INERTIA_EPS);
    END_TEST();
}

static void test_capsule_inertia_degenerate(void)
{
    TEST("CS_capsule_inertia_degenerate")
    /* Very small dimensions — should still produce valid non-zero inertia */
    ForgePhysicsRigidBody rb = cs_make_body(vec3_create(0, 0, 0));
    forge_physics_rigid_body_set_inertia_capsule(&rb, CS_SMALL_DIM, CS_SMALL_DIM);
    ASSERT_TRUE(rb.inertia_local.m[0] > 0.0f);
    ASSERT_TRUE(rb.inertia_local.m[4] > 0.0f);
    ASSERT_TRUE(rb.inertia_local.m[8] > 0.0f);
    END_TEST();
}

static void test_capsule_inertia_below_min_dim(void)
{
    TEST("CS_capsule_inertia_below_min_dim_clamped")
    /* Values below FORGE_PHYSICS_SHAPE_MIN_DIM exercise the clamp branch */
    ForgePhysicsRigidBody rb = cs_make_body(vec3_create(0, 0, 0));
    forge_physics_rigid_body_set_inertia_capsule(&rb, CS_BELOW_MIN_DIM, CS_BELOW_MIN_DIM);
    /* Should still produce valid positive inertia (clamped to MIN_DIM) */
    ASSERT_TRUE(rb.inertia_local.m[0] > 0.0f);
    ASSERT_TRUE(rb.inertia_local.m[4] > 0.0f);
    ASSERT_TRUE(rb.inertia_local.m[8] > 0.0f);
    ASSERT_TRUE(forge_isfinite(rb.inertia_local.m[0]));
    ASSERT_TRUE(forge_isfinite(rb.inertia_local.m[4]));
    END_TEST();
}

static void test_capsule_inertia_static_skipped(void)
{
    TEST("CS_capsule_inertia_static_skipped")
    /* Static body (mass=0) should not have inertia modified — remains identity */
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), 0.0f, CS_DAMPING, CS_ANG_DAMPING, CS_RESTIT);
    forge_physics_rigid_body_set_inertia_capsule(&rb,
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    /* Full identity check: diagonal = 1, off-diagonal = 0 */
    ASSERT_NEAR(rb.inertia_local.m[0], 1.0f, EPSILON);
    ASSERT_NEAR(rb.inertia_local.m[4], 1.0f, EPSILON);
    ASSERT_NEAR(rb.inertia_local.m[8], 1.0f, EPSILON);
    ASSERT_NEAR(rb.inertia_local.m[1], 0.0f, EPSILON);
    ASSERT_NEAR(rb.inertia_local.m[2], 0.0f, EPSILON);
    ASSERT_NEAR(rb.inertia_local.m[3], 0.0f, EPSILON);
    ASSERT_NEAR(rb.inertia_local.m[5], 0.0f, EPSILON);
    ASSERT_NEAR(rb.inertia_local.m[6], 0.0f, EPSILON);
    ASSERT_NEAR(rb.inertia_local.m[7], 0.0f, EPSILON);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Inertia from Shape Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_inertia_from_shape_sphere(void)
{
    TEST("CS_inertia_from_shape_sphere")
    ForgePhysicsRigidBody rb1 = cs_make_body(vec3_create(0, 0, 0));
    ForgePhysicsRigidBody rb2 = cs_make_body(vec3_create(0, 0, 0));
    forge_physics_rigid_body_set_inertia_sphere(&rb1, CS_SPHERE_RADIUS);
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    forge_physics_rigid_body_set_inertia_from_shape(&rb2, &s);
    ASSERT_NEAR(rb1.inertia_local.m[0], rb2.inertia_local.m[0], CS_INERTIA_EPS);
    ASSERT_NEAR(rb1.inertia_local.m[4], rb2.inertia_local.m[4], CS_INERTIA_EPS);
    END_TEST();
}

static void test_inertia_from_shape_box(void)
{
    TEST("CS_inertia_from_shape_box")
    vec3 he = vec3_create(CS_BOX_HALF_X, CS_BOX_HALF_Y, CS_BOX_HALF_Z);
    ForgePhysicsRigidBody rb1 = cs_make_body(vec3_create(0, 0, 0));
    ForgePhysicsRigidBody rb2 = cs_make_body(vec3_create(0, 0, 0));
    forge_physics_rigid_body_set_inertia_box(&rb1, he);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    forge_physics_rigid_body_set_inertia_from_shape(&rb2, &s);
    ASSERT_NEAR(rb1.inertia_local.m[0], rb2.inertia_local.m[0], CS_INERTIA_EPS);
    ASSERT_NEAR(rb1.inertia_local.m[4], rb2.inertia_local.m[4], CS_INERTIA_EPS);
    ASSERT_NEAR(rb1.inertia_local.m[8], rb2.inertia_local.m[8], CS_INERTIA_EPS);
    END_TEST();
}

static void test_inertia_from_shape_capsule(void)
{
    TEST("CS_inertia_from_shape_capsule")
    ForgePhysicsRigidBody rb1 = cs_make_body(vec3_create(0, 0, 0));
    ForgePhysicsRigidBody rb2 = cs_make_body(vec3_create(0, 0, 0));
    forge_physics_rigid_body_set_inertia_capsule(&rb1,
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    forge_physics_rigid_body_set_inertia_from_shape(&rb2, &s);
    ASSERT_NEAR(rb1.inertia_local.m[0], rb2.inertia_local.m[0], CS_INERTIA_EPS);
    ASSERT_NEAR(rb1.inertia_local.m[4], rb2.inertia_local.m[4], CS_INERTIA_EPS);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Support Function — Sphere Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_support_sphere_axis(void)
{
    TEST("CS_support_sphere_axis_aligned")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    vec3 pos = vec3_create(0, 0, 0);
    quat orient = quat_identity();

    /* Support along +X should be (radius, 0, 0) */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(1, 0, 0));
    ASSERT_NEAR(sp.x, CS_SPHERE_RADIUS, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, 0.0f, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, 0.0f, CS_SUPPORT_EPS);

    /* Support along -Y */
    sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(0, -1, 0));
    ASSERT_NEAR(sp.x, 0.0f, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, -CS_SPHERE_RADIUS, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_sphere_diagonal(void)
{
    TEST("CS_support_sphere_diagonal")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    vec3 pos = vec3_create(0, 0, 0);
    quat orient = quat_identity();

    /* Diagonal direction (1,1,1) normalized = (1/√3, 1/√3, 1/√3) */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(1, 1, 1));
    float expected = CS_SPHERE_RADIUS * CS_SQRT3_INV;
    ASSERT_NEAR(sp.x, expected, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, expected, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, expected, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_sphere_translated(void)
{
    TEST("CS_support_sphere_translated")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    vec3 pos = vec3_create(CS_TRANSLATE_X, CS_TRANSLATE_Y, CS_TRANSLATE_Z);
    quat orient = quat_identity();

    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(1, 0, 0));
    ASSERT_NEAR(sp.x, CS_TRANSLATE_X + CS_SPHERE_RADIUS, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, CS_TRANSLATE_Y, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, CS_TRANSLATE_Z, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_sphere_zero_dir(void)
{
    TEST("CS_support_sphere_zero_dir")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    vec3 pos = vec3_create(CS_ZERO_DIR_POS_X, CS_ZERO_DIR_POS_Y, CS_ZERO_DIR_POS_Z);
    quat orient = quat_identity();

    /* Zero direction returns center */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(0, 0, 0));
    ASSERT_NEAR(sp.x, CS_ZERO_DIR_POS_X, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, CS_ZERO_DIR_POS_Y, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, CS_ZERO_DIR_POS_Z, CS_SUPPORT_EPS);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Support Function — Box Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_support_box_axis(void)
{
    TEST("CS_support_box_axis_aligned")
    vec3 he = vec3_create(CS_BOX_HALF, CS_BOX_HALF, CS_BOX_HALF);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    vec3 pos = vec3_create(0, 0, 0);
    quat orient = quat_identity();

    /* Support along +X should pick corner (0.5, 0.5, 0.5) projected to +X
     * = corner with max X = (0.5, ?, ?) — but support picks the corner
     * that maximizes dot(corner, dir), so for +X: (0.5, 0.5, 0.5) has
     * dot = 0.5. Actually for axis (1,0,0) any corner with x=0.5 works.
     * The function picks signs per-axis, so (+half, +half, +half) */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(1, 0, 0));
    ASSERT_NEAR(sp.x, CS_BOX_HALF, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_box_diagonal(void)
{
    TEST("CS_support_box_diagonal")
    vec3 he = vec3_create(CS_BOX_HALF, CS_BOX_HALF, CS_BOX_HALF);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    vec3 pos = vec3_create(0, 0, 0);
    quat orient = quat_identity();

    /* Diagonal (1,1,1): picks corner (0.5, 0.5, 0.5) */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(1, 1, 1));
    ASSERT_NEAR(sp.x, CS_BOX_HALF, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, CS_BOX_HALF, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, CS_BOX_HALF, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_box_rotated(void)
{
    TEST("CS_support_box_rotated_45_degrees")
    vec3 he = vec3_create(CS_BOX_HALF, CS_BOX_HALF, CS_BOX_HALF);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    vec3 pos = vec3_create(0, 0, 0);
    /* Rotate 45 degrees around Y */
    quat orient = quat_from_euler(CS_ROT_45_DEG, 0.0f, 0.0f);

    /* After 45° rotation about Y, querying along +X should give a corner
     * at a distance of half * √2 along X (two corners project equally). */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(1, 0, 0));
    /* The support point x-coordinate should be half * √2 ≈ 0.707 */
    float expected_x = CS_BOX_HALF * SDL_sqrtf(2.0f);
    ASSERT_NEAR(sp.x, expected_x, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_box_non_cubic(void)
{
    TEST("CS_support_box_non_cubic")
    vec3 he = vec3_create(CS_BOX_HALF_X, CS_BOX_HALF_Y, CS_BOX_HALF_Z);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    vec3 pos = vec3_create(0, 0, 0);
    quat orient = quat_identity();

    /* Support along +Z: picks corner with max Z = (?, ?, 1.0) */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(0, 0, 1));
    ASSERT_NEAR(sp.z, CS_BOX_HALF_Z, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_box_zero_dir(void)
{
    TEST("CS_support_box_zero_dir")
    vec3 he = vec3_create(CS_BOX_HALF, CS_BOX_HALF, CS_BOX_HALF);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    vec3 pos = vec3_create(CS_ZERO_DIR_POS_X, CS_ZERO_DIR_POS_Y, CS_ZERO_DIR_POS_Z);
    quat orient = quat_identity();

    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(0, 0, 0));
    ASSERT_NEAR(sp.x, CS_ZERO_DIR_POS_X, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, CS_ZERO_DIR_POS_Y, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, CS_ZERO_DIR_POS_Z, CS_SUPPORT_EPS);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Support Function — Capsule Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_support_capsule_along_axis(void)
{
    TEST("CS_support_capsule_along_Y_axis")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    vec3 pos = vec3_create(0, 0, 0);
    quat orient = quat_identity();

    /* Along +Y: top cap center at (0, half_h, 0) + radius along +Y */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(0, 1, 0));
    ASSERT_NEAR(sp.x, 0.0f, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, CS_CAPSULE_HALF_H + CS_CAPSULE_RADIUS, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, 0.0f, CS_SUPPORT_EPS);

    /* Along -Y: bottom cap */
    sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(0, -1, 0));
    ASSERT_NEAR(sp.y, -(CS_CAPSULE_HALF_H + CS_CAPSULE_RADIUS), CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_capsule_perpendicular(void)
{
    TEST("CS_support_capsule_perpendicular")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    vec3 pos = vec3_create(0, 0, 0);
    quat orient = quat_identity();

    /* Along +X: perpendicular to axis.
     * dot_y = 0, picks either cap (top by convention when >= 0).
     * Support = cap_center + radius * normalize(dir_n)
     * = (0, half_h, 0) + (radius, 0, 0) = (radius, half_h, 0) */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(1, 0, 0));
    ASSERT_NEAR(sp.x, CS_CAPSULE_RADIUS, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_capsule_diagonal(void)
{
    TEST("CS_support_capsule_diagonal")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    vec3 pos = vec3_create(0, 0, 0);
    quat orient = quat_identity();

    /* Diagonal (1, 1, 0): dot_y > 0, picks top cap.
     * Cap center = (0, 0.5, 0). Support = center + radius * normalize(1,1,0)
     * normalize(1,1,0) = (0.707, 0.707, 0) */
    vec3 dir = vec3_create(1, 1, 0);
    vec3 sp = forge_physics_shape_support(&s, pos, orient, dir);
    ASSERT_NEAR(sp.x, CS_CAPSULE_RADIUS * CS_SQRT2_INV, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, CS_CAPSULE_HALF_H + CS_CAPSULE_RADIUS * CS_SQRT2_INV, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_capsule_rotated(void)
{
    TEST("CS_support_capsule_rotated_90_around_Z")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    vec3 pos = vec3_create(0, 0, 0);
    /* Rotate 90° around Z: local Y becomes world -X */
    quat orient = quat_from_euler(0.0f, 0.0f, CS_ROT_90_DEG);

    /* Along +X: local Y points to -X after 90° Z rotation.
     * dot(+X, -X) < 0 → picks bottom cap (which is now +X side).
     * Actually: local Y → rotate 90 around Z → becomes (-1, 0, 0) = -X.
     * dir = +X, dot_y = dot((1,0,0), (-1,0,0)) = -1 < 0.
     * Bottom cap center = pos - (-1,0,0) * half_h = (half_h, 0, 0).
     * Support = (half_h, 0, 0) + radius * (1, 0, 0) = (half_h + radius, 0, 0). */
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(1, 0, 0));
    ASSERT_NEAR(sp.x, CS_CAPSULE_HALF_H + CS_CAPSULE_RADIUS, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, 0.0f, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_support_capsule_zero_dir(void)
{
    TEST("CS_support_capsule_zero_dir")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    vec3 pos = vec3_create(CS_ZERO_DIR_POS_X, CS_ZERO_DIR_POS_Y, CS_ZERO_DIR_POS_Z);
    quat orient = quat_identity();

    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(0, 0, 0));
    ASSERT_NEAR(sp.x, CS_ZERO_DIR_POS_X, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, CS_ZERO_DIR_POS_Y, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, CS_ZERO_DIR_POS_Z, CS_SUPPORT_EPS);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * AABB Computation Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_aabb_sphere_origin(void)
{
    TEST("CS_aabb_sphere_at_origin")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s,
        vec3_create(0, 0, 0), quat_identity());
    ASSERT_NEAR(aabb.min.x, -CS_SPHERE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.y, -CS_SPHERE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.z, -CS_SPHERE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x,  CS_SPHERE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y,  CS_SPHERE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.z,  CS_SPHERE_RADIUS, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_sphere_translated(void)
{
    TEST("CS_aabb_sphere_translated")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    vec3 pos = vec3_create(CS_TRANSLATE_X, CS_TRANSLATE_Y, CS_TRANSLATE_Z);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s, pos,
        quat_identity());
    ASSERT_NEAR(aabb.min.x, CS_TRANSLATE_X - CS_SPHERE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x, CS_TRANSLATE_X + CS_SPHERE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.y, CS_TRANSLATE_Y - CS_SPHERE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y, CS_TRANSLATE_Y + CS_SPHERE_RADIUS, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_box_aligned(void)
{
    TEST("CS_aabb_box_axis_aligned")
    vec3 he = vec3_create(CS_BOX_HALF_X, CS_BOX_HALF_Y, CS_BOX_HALF_Z);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s,
        vec3_create(0, 0, 0), quat_identity());
    ASSERT_NEAR(aabb.min.x, -CS_BOX_HALF_X, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x,  CS_BOX_HALF_X, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.y, -CS_BOX_HALF_Y, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y,  CS_BOX_HALF_Y, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.z, -CS_BOX_HALF_Z, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.z,  CS_BOX_HALF_Z, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_box_rotated(void)
{
    TEST("CS_aabb_box_rotated_45_degrees")
    vec3 he = vec3_create(CS_BOX_HALF, CS_BOX_HALF, CS_BOX_HALF);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    /* Rotate 45° around Y */
    quat orient = quat_from_euler(CS_ROT_45_DEG, 0.0f, 0.0f);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s,
        vec3_create(0, 0, 0), orient);

    /* After 45° Y rotation, the AABB should be wider on X and Z.
     * A cube half=0.5 rotated 45° has AABB half-extent = 0.5*√2 ≈ 0.707
     * on X and Z, 0.5 on Y. */
    float expected_xz = CS_BOX_HALF * SDL_sqrtf(2.0f);
    ASSERT_NEAR(aabb.max.x, expected_xz, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y, CS_BOX_HALF, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.z, expected_xz, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_capsule_upright(void)
{
    TEST("CS_aabb_capsule_upright")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s,
        vec3_create(0, 0, 0), quat_identity());

    /* Upright capsule: Y extends ±(half_h + radius), X/Z extends ±radius */
    float total_half_y = CS_CAPSULE_HALF_H + CS_CAPSULE_RADIUS;
    ASSERT_NEAR(aabb.min.x, -CS_CAPSULE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x,  CS_CAPSULE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.y, -total_half_y, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y,  total_half_y, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.z, -CS_CAPSULE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.z,  CS_CAPSULE_RADIUS, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_capsule_tilted(void)
{
    TEST("CS_aabb_capsule_tilted_90_degrees")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    /* Rotate 90° around Z: capsule lies along X */
    quat orient = quat_from_euler(0.0f, 0.0f, CS_ROT_90_DEG);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s,
        vec3_create(0, 0, 0), orient);

    /* Tilted capsule: X extends ±(half_h + radius), Y/Z extends ±radius */
    float total_half_x = CS_CAPSULE_HALF_H + CS_CAPSULE_RADIUS;
    ASSERT_NEAR(aabb.min.x, -total_half_x, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x,  total_half_x, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.y, -CS_CAPSULE_RADIUS, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y,  CS_CAPSULE_RADIUS, CS_AABB_EPS);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * AABB Overlap Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_aabb_overlap_yes(void)
{
    TEST("CS_aabb_overlap_overlapping")
    ForgePhysicsAABB a = { vec3_create(-1, -1, -1), vec3_create(1, 1, 1) };
    ForgePhysicsAABB b = { vec3_create(0, 0, 0), vec3_create(2, 2, 2) };
    ASSERT_TRUE(forge_physics_aabb_overlap(a, b));
    END_TEST();
}

static void test_aabb_overlap_separated(void)
{
    TEST("CS_aabb_overlap_separated")
    ForgePhysicsAABB a = { vec3_create(-1, -1, -1), vec3_create(0, 0, 0) };
    ForgePhysicsAABB b = { vec3_create(1, 1, 1), vec3_create(2, 2, 2) };
    ASSERT_TRUE(!forge_physics_aabb_overlap(a, b));
    END_TEST();
}

static void test_aabb_overlap_touching(void)
{
    TEST("CS_aabb_overlap_touching")
    ForgePhysicsAABB a = { vec3_create(-1, -1, -1), vec3_create(0, 0, 0) };
    ForgePhysicsAABB b = { vec3_create(0, 0, 0), vec3_create(1, 1, 1) };
    /* Touching: max == min on all axes, should count as overlap */
    ASSERT_TRUE(forge_physics_aabb_overlap(a, b));
    END_TEST();
}

static void test_aabb_overlap_nested(void)
{
    TEST("CS_aabb_overlap_nested")
    ForgePhysicsAABB a = { vec3_create(-2, -2, -2), vec3_create(2, 2, 2) };
    ForgePhysicsAABB b = { vec3_create(-1, -1, -1), vec3_create(1, 1, 1) };
    ASSERT_TRUE(forge_physics_aabb_overlap(a, b));
    END_TEST();
}

static void test_aabb_overlap_single_axis_separated(void)
{
    TEST("CS_aabb_overlap_single_axis_separated")
    /* Overlap on X and Y, but separated on Z — should NOT overlap */
    ForgePhysicsAABB a = { vec3_create(-1, -1, -1), vec3_create(1, 1, 1) };
    ForgePhysicsAABB b = { vec3_create(0, 0, 3),    vec3_create(2, 2, 5) };
    ASSERT_TRUE(!forge_physics_aabb_overlap(a, b));
    END_TEST();
}

static void test_support_invalid_shape_returns_pos(void)
{
    TEST("CS_support_invalid_shape_returns_position")
    ForgePhysicsCollisionShape s;
    s.type = (ForgePhysicsShapeType)CS_INVALID_SHAPE_TYPE;
    vec3 pos = vec3_create(CS_ZERO_DIR_POS_X, CS_ZERO_DIR_POS_Y, CS_ZERO_DIR_POS_Z);
    quat orient = quat_identity();
    vec3 sp = forge_physics_shape_support(&s, pos, orient,
        vec3_create(1, 0, 0));
    ASSERT_NEAR(sp.x, CS_ZERO_DIR_POS_X, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, CS_ZERO_DIR_POS_Y, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, CS_ZERO_DIR_POS_Z, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_aabb_compute_invalid_shape(void)
{
    TEST("CS_aabb_compute_invalid_shape_zero_volume")
    ForgePhysicsCollisionShape s;
    s.type = (ForgePhysicsShapeType)CS_INVALID_SHAPE_TYPE;
    vec3 pos = vec3_create(CS_TRANSLATE_X, CS_TRANSLATE_Y, CS_TRANSLATE_Z);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s, pos,
        quat_identity());
    /* Invalid shape should produce a zero-volume AABB at position (all axes) */
    ASSERT_NEAR(aabb.min.x, CS_TRANSLATE_X, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x, CS_TRANSLATE_X, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.y, CS_TRANSLATE_Y, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y, CS_TRANSLATE_Y, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.z, CS_TRANSLATE_Z, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.z, CS_TRANSLATE_Z, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_expand_negative_margin(void)
{
    TEST("CS_aabb_expand_negative_margin_unchanged")
    ForgePhysicsAABB a = { vec3_create(-1, -1, -1), vec3_create(1, 1, 1) };
    ForgePhysicsAABB result = forge_physics_aabb_expand(a, -0.5f);
    /* Negative margin should return original AABB unchanged (all axes) */
    ASSERT_NEAR(result.min.x, -1.0f, EPSILON);
    ASSERT_NEAR(result.max.x,  1.0f, EPSILON);
    ASSERT_NEAR(result.min.y, -1.0f, EPSILON);
    ASSERT_NEAR(result.max.y,  1.0f, EPSILON);
    ASSERT_NEAR(result.min.z, -1.0f, EPSILON);
    ASSERT_NEAR(result.max.z,  1.0f, EPSILON);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * AABB Utility Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_aabb_expand(void)
{
    TEST("CS_aabb_expand")
    ForgePhysicsAABB a = { vec3_create(-1, -1, -1), vec3_create(1, 1, 1) };
    ForgePhysicsAABB expanded = forge_physics_aabb_expand(a, CS_EXPAND_MARGIN);
    ASSERT_NEAR(expanded.min.x, -1.0f - CS_EXPAND_MARGIN, EPSILON);
    ASSERT_NEAR(expanded.max.x,  1.0f + CS_EXPAND_MARGIN, EPSILON);
    ASSERT_NEAR(expanded.min.y, -1.0f - CS_EXPAND_MARGIN, EPSILON);
    ASSERT_NEAR(expanded.max.y,  1.0f + CS_EXPAND_MARGIN, EPSILON);
    END_TEST();
}

static void test_aabb_center(void)
{
    TEST("CS_aabb_center")
    ForgePhysicsAABB a = { vec3_create(-1, 0, 2), vec3_create(3, 4, 6) };
    vec3 center = forge_physics_aabb_center(a);
    ASSERT_NEAR(center.x, 1.0f, EPSILON);
    ASSERT_NEAR(center.y, 2.0f, EPSILON);
    ASSERT_NEAR(center.z, 4.0f, EPSILON);
    END_TEST();
}

static void test_aabb_extents(void)
{
    TEST("CS_aabb_extents")
    ForgePhysicsAABB a = { vec3_create(-1, 0, 2), vec3_create(3, 4, 6) };
    vec3 he = forge_physics_aabb_extents(a);
    ASSERT_NEAR(he.x, 2.0f, EPSILON);
    ASSERT_NEAR(he.y, 2.0f, EPSILON);
    ASSERT_NEAR(he.z, 2.0f, EPSILON);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Hemisphere Inertia Coefficient Test
 *
 * Verifies that capsule inertia uses the correct hemisphere transverse
 * coefficient (83/320 m r²) rather than the sphere coefficient (2/5 m r²).
 * The transverse inertia (Ixx) should be smaller with 83/320 than it would
 * be with 2/5, because the base hemisphere moment is lower.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_capsule_inertia_hemisphere_transverse(void)
{
    TEST("CS_capsule_inertia_hemisphere_transverse_coeff")
    /* Compute the expected Ixx explicitly using 83/320 for the transverse
     * hemisphere coefficient. This verifies the code uses the correct formula
     * rather than the incorrect 2/5 sphere coefficient for all axes. */
    float r = CS_UNIT_DIM;
    float h = CS_SMALL_DIM;
    float r2 = r * r;

    /* Volume-proportional mass split */
    float v_cyl  = FORGE_PI * r2 * (2.0f * h);
    float v_caps = (4.0f / 3.0f) * FORGE_PI * r2 * r;
    float v_total = v_cyl + v_caps;
    float m_cyl  = CS_MASS * (v_cyl / v_total);
    float m_hemi = 0.5f * CS_MASS * (v_caps / v_total);

    /* Cylinder transverse inertia */
    float cyl_Ixx = FORGE_PHYSICS_INERTIA_BOX_COEFF *
                    m_cyl * (3.0f * r2 + 4.0f * h * h);

    /* Hemisphere transverse inertia with parallel axis theorem.
     * Uses 83/320 (transverse), NOT 2/5 (symmetry axis only). */
    float offset = h + FORGE_PHYSICS_CAPSULE_HEMI_CENTROID_FRAC * r;
    float hemi_Ixx = FORGE_PHYSICS_HEMI_TRANSVERSE_INERTIA_COEFF *
                     m_hemi * r2 + m_hemi * offset * offset;

    float expected_Ixx = cyl_Ixx + 2.0f * hemi_Ixx;

    ForgePhysicsRigidBody rb = cs_make_body(vec3_create(0, 0, 0));
    forge_physics_rigid_body_set_inertia_capsule(&rb, r, h);
    float Ixx = rb.inertia_local.m[0];

    ASSERT_TRUE(Ixx > 0.0f);
    ASSERT_NEAR(Ixx, expected_Ixx, CS_INERTIA_EPS);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Non-Finite Input Guard Tests
 *
 * Verifies that forge_physics_shape_support and forge_physics_shape_compute_aabb
 * return safe finite fallbacks when given NaN or Inf inputs for position,
 * direction, or orientation. These guard paths were added during PR review.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Runtime NaN/Inf generators — MSVC rejects compile-time 0/0 (C2124).
 * The volatile qualifier prevents the compiler from evaluating at compile
 * time, producing NaN/Inf at runtime on all platforms. */
static float cs_make_nan(void) { volatile float z = 0.0f; return z / z; }
static float cs_make_inf(void) { volatile float z = 0.0f; return 1.0f / z; }
#define CS_NAN_VAL  cs_make_nan()
#define CS_INF_VAL  cs_make_inf()

/* NaN/Inf validation tests for forge_physics_shape_support() removed —
 * the function no longer guards against non-finite inputs for performance
 * (see forge_physics.h header comment). Callers are responsible for
 * ensuring finite inputs before entering the GJK/EPA inner loop. */

static void test_aabb_nan_position(void)
{
    TEST("CS_aabb_nan_position_returns_origin")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    vec3 nan_pos = vec3_create(0.0f, CS_NAN_VAL, 0.0f);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s, nan_pos,
        quat_identity());
    /* NaN pos → AABB centered at origin */
    ASSERT_TRUE(forge_isfinite(aabb.min.x));
    ASSERT_TRUE(forge_isfinite(aabb.min.y));
    ASSERT_NEAR(aabb.min.x, 0.0f, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x, 0.0f, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.y, 0.0f, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y, 0.0f, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_nan_orientation(void)
{
    TEST("CS_aabb_nan_orientation_returns_pos_aabb")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    vec3 pos = vec3_create(CS_TRANSLATE_X, CS_TRANSLATE_Y, CS_TRANSLATE_Z);
    quat nan_orient = quat_create(0.0f, CS_NAN_VAL, 0.0f, 0.0f);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s, pos, nan_orient);
    /* Finite pos + NaN orient → zero-volume AABB at pos */
    ASSERT_NEAR(aabb.min.x, CS_TRANSLATE_X, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x, CS_TRANSLATE_X, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.y, CS_TRANSLATE_Y, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y, CS_TRANSLATE_Y, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_inf_position(void)
{
    TEST("CS_aabb_inf_position_returns_origin")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    vec3 inf_pos = vec3_create(CS_INF_VAL, 0.0f, 0.0f);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(&s, inf_pos,
        quat_identity());
    /* Inf pos → AABB centered at origin */
    ASSERT_TRUE(forge_isfinite(aabb.min.x));
    ASSERT_NEAR(aabb.min.x, 0.0f, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x, 0.0f, CS_AABB_EPS);
    END_TEST();
}

static void test_support_null_shape(void)
{
    TEST("CS_support_null_shape_returns_pos")
    vec3 pos = vec3_create(CS_TRANSLATE_X, CS_TRANSLATE_Y, CS_TRANSLATE_Z);
    vec3 sp = forge_physics_shape_support(NULL, pos, quat_identity(),
        vec3_create(1, 0, 0));
    ASSERT_NEAR(sp.x, CS_TRANSLATE_X, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.y, CS_TRANSLATE_Y, CS_SUPPORT_EPS);
    ASSERT_NEAR(sp.z, CS_TRANSLATE_Z, CS_SUPPORT_EPS);
    END_TEST();
}

static void test_aabb_null_shape(void)
{
    TEST("CS_aabb_null_shape_returns_pos_aabb")
    vec3 pos = vec3_create(CS_TRANSLATE_X, CS_TRANSLATE_Y, CS_TRANSLATE_Z);
    ForgePhysicsAABB aabb = forge_physics_shape_compute_aabb(NULL, pos,
        quat_identity());
    ASSERT_NEAR(aabb.min.x, CS_TRANSLATE_X, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.x, CS_TRANSLATE_X, CS_AABB_EPS);
    ASSERT_NEAR(aabb.min.y, CS_TRANSLATE_Y, CS_AABB_EPS);
    ASSERT_NEAR(aabb.max.y, CS_TRANSLATE_Y, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_expand_nan_margin(void)
{
    TEST("CS_aabb_expand_nan_margin_unchanged")
    ForgePhysicsAABB a = { vec3_create(-1, -1, -1), vec3_create(1, 1, 1) };
    ForgePhysicsAABB result = forge_physics_aabb_expand(a, CS_NAN_VAL);
    ASSERT_NEAR(result.min.x, -1.0f, EPSILON);
    ASSERT_NEAR(result.max.x,  1.0f, EPSILON);
    ASSERT_NEAR(result.min.y, -1.0f, EPSILON);
    ASSERT_NEAR(result.max.y,  1.0f, EPSILON);
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Determinism Test
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 * Non-unit quaternion normalization tests
 *
 * Verifies that shape_compute_aabb internally normalizes non-unit
 * quaternions, producing the same results as a pre-normalized quat.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Non-unit quaternion normalization tests for forge_physics_shape_support()
 * removed — the function no longer normalizes quaternions internally.
 * Callers must pass unit quaternions. */

static void test_aabb_nonunit_quat_box(void)
{
    TEST("CS_aabb_nonunit_quat_box")
    vec3 he = vec3_create(CS_BOX_HALF_X, CS_BOX_HALF_Y, CS_BOX_HALF_Z);
    ForgePhysicsCollisionShape s = forge_physics_shape_box(he);
    vec3 pos = vec3_create(CS_TRANSLATE_X, CS_TRANSLATE_Y, CS_TRANSLATE_Z);

    quat unit_q = quat_from_euler(CS_ROT_45_DEG, CS_DET_PITCH, 0.0f);
    ForgePhysicsAABB ref = forge_physics_shape_compute_aabb(&s, pos, unit_q);

    quat scaled_q = (quat){ unit_q.w * CS_NONUNIT_QUAT_SCALE,
                            unit_q.x * CS_NONUNIT_QUAT_SCALE,
                            unit_q.y * CS_NONUNIT_QUAT_SCALE,
                            unit_q.z * CS_NONUNIT_QUAT_SCALE };
    ForgePhysicsAABB result = forge_physics_shape_compute_aabb(&s, pos, scaled_q);

    ASSERT_NEAR(result.min.x, ref.min.x, CS_AABB_EPS);
    ASSERT_NEAR(result.min.y, ref.min.y, CS_AABB_EPS);
    ASSERT_NEAR(result.min.z, ref.min.z, CS_AABB_EPS);
    ASSERT_NEAR(result.max.x, ref.max.x, CS_AABB_EPS);
    ASSERT_NEAR(result.max.y, ref.max.y, CS_AABB_EPS);
    ASSERT_NEAR(result.max.z, ref.max.z, CS_AABB_EPS);
    END_TEST();
}

static void test_aabb_nonunit_quat_capsule(void)
{
    TEST("CS_aabb_nonunit_quat_capsule")
    ForgePhysicsCollisionShape s = forge_physics_shape_capsule(
        CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);
    vec3 pos = vec3_create(CS_TRANSLATE_X, CS_TRANSLATE_Y, CS_TRANSLATE_Z);

    quat unit_q = quat_from_euler(CS_ROT_45_DEG, CS_DET_PITCH, 0.0f);
    ForgePhysicsAABB ref = forge_physics_shape_compute_aabb(&s, pos, unit_q);

    quat scaled_q = (quat){ unit_q.w * CS_NONUNIT_QUAT_SCALE,
                            unit_q.x * CS_NONUNIT_QUAT_SCALE,
                            unit_q.y * CS_NONUNIT_QUAT_SCALE,
                            unit_q.z * CS_NONUNIT_QUAT_SCALE };
    ForgePhysicsAABB result = forge_physics_shape_compute_aabb(&s, pos, scaled_q);

    ASSERT_NEAR(result.min.x, ref.min.x, CS_AABB_EPS);
    ASSERT_NEAR(result.min.y, ref.min.y, CS_AABB_EPS);
    ASSERT_NEAR(result.min.z, ref.min.z, CS_AABB_EPS);
    ASSERT_NEAR(result.max.x, ref.max.x, CS_AABB_EPS);
    ASSERT_NEAR(result.max.y, ref.max.y, CS_AABB_EPS);
    ASSERT_NEAR(result.max.z, ref.max.z, CS_AABB_EPS);
    END_TEST();
}

static void test_shape_create_sphere_negative_radius(void)
{
    TEST("CS_shape_create_sphere_negative_radius_clamped")
    ForgePhysicsCollisionShape s = forge_physics_shape_sphere(CS_NEG_RADIUS);
    ASSERT_TRUE(s.type == FORGE_PHYSICS_SHAPE_SPHERE);
    /* Negative radius should be clamped to FORGE_PHYSICS_SHAPE_MIN_DIM */
    ASSERT_NEAR(s.data.sphere.radius, FORGE_PHYSICS_SHAPE_MIN_DIM, EPSILON);
    ASSERT_TRUE(forge_physics_shape_is_valid(&s));
    END_TEST();
}

static void test_support_determinism(void)
{
    TEST("CS_support_determinism_1000_calls")
    ForgePhysicsCollisionShape shapes[3];
    shapes[0] = forge_physics_shape_sphere(CS_SPHERE_RADIUS);
    shapes[1] = forge_physics_shape_box(vec3_create(CS_BOX_HALF_X, CS_BOX_HALF_Y, CS_BOX_HALF_Z));
    shapes[2] = forge_physics_shape_capsule(CS_CAPSULE_RADIUS, CS_CAPSULE_HALF_H);

    vec3 pos = vec3_create(CS_DET_POS_X, CS_DET_POS_Y, CS_DET_POS_Z);
    quat orient = quat_from_euler(CS_DET_YAW, CS_DET_PITCH, CS_DET_ROLL);
    vec3 dir = vec3_create(CS_DET_DIR_X, CS_DET_DIR_Y, CS_DET_DIR_Z);

    /* First pass: compute reference results */
    vec3 ref[3];
    for (int s = 0; s < 3; s++) {
        ref[s] = forge_physics_shape_support(&shapes[s], pos, orient, dir);
    }

    /* Repeated passes: verify identical results */
    for (int i = 0; i < CS_DETERMINISM_ITERS; i++) {
        for (int s = 0; s < 3; s++) {
            vec3 result = forge_physics_shape_support(&shapes[s], pos, orient, dir);
            ASSERT_NEAR(result.x, ref[s].x, EPSILON);
            ASSERT_NEAR(result.y, ref[s].y, EPSILON);
            ASSERT_NEAR(result.z, ref[s].z, EPSILON);
        }
    }
    END_TEST();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test runner
 * ═══════════════════════════════════════════════════════════════════════════ */

void run_collision_shape_tests(void)
{
    SDL_Log("\n=== Collision Shape Tests ===\n");

    /* Shape creation (5) */
    test_shape_create_sphere();
    test_shape_create_box();
    test_shape_create_capsule();
    test_shape_create_box_negative_clamped();
    test_shape_create_capsule_negative_clamped();

    /* Shape validity (4) */
    test_shape_valid_sphere();
    test_shape_valid_box();
    test_shape_valid_capsule();
    test_shape_invalid_type();

    /* Capsule inertia (5) */
    test_capsule_inertia_basic();
    test_capsule_inertia_symmetry();
    test_capsule_inertia_degenerate();
    test_capsule_inertia_below_min_dim();
    test_capsule_inertia_static_skipped();

    /* Inertia from shape (3) */
    test_inertia_from_shape_sphere();
    test_inertia_from_shape_box();
    test_inertia_from_shape_capsule();

    /* Support — sphere (4) */
    test_support_sphere_axis();
    test_support_sphere_diagonal();
    test_support_sphere_translated();
    test_support_sphere_zero_dir();

    /* Support — box (5) */
    test_support_box_axis();
    test_support_box_diagonal();
    test_support_box_rotated();
    test_support_box_non_cubic();
    test_support_box_zero_dir();

    /* Support — capsule (5) */
    test_support_capsule_along_axis();
    test_support_capsule_perpendicular();
    test_support_capsule_diagonal();
    test_support_capsule_rotated();
    test_support_capsule_zero_dir();

    /* AABB computation (6) */
    test_aabb_sphere_origin();
    test_aabb_sphere_translated();
    test_aabb_box_aligned();
    test_aabb_box_rotated();
    test_aabb_capsule_upright();
    test_aabb_capsule_tilted();

    /* AABB overlap (5) */
    test_aabb_overlap_yes();
    test_aabb_overlap_separated();
    test_aabb_overlap_touching();
    test_aabb_overlap_nested();
    test_aabb_overlap_single_axis_separated();

    /* AABB utilities (4) */
    test_aabb_expand();
    test_aabb_expand_negative_margin();
    test_aabb_center();
    test_aabb_extents();

    /* Invalid shape fallback (2) */
    test_support_invalid_shape_returns_pos();
    test_aabb_compute_invalid_shape();

    /* Hemisphere inertia coefficient (1) */
    test_capsule_inertia_hemisphere_transverse();

    /* Non-finite input guards (6 — support NaN/Inf tests removed, see above) */
    test_aabb_nan_position();
    test_aabb_nan_orientation();
    test_aabb_inf_position();
    test_support_null_shape();
    test_aabb_null_shape();
    test_aabb_expand_nan_margin();

    /* Non-unit quaternion normalization (2 — support tests removed, see above) */
    test_aabb_nonunit_quat_box();
    test_aabb_nonunit_quat_capsule();

    /* Negative dimension clamping (1) */
    test_shape_create_sphere_negative_radius();

    /* Determinism (1) */
    test_support_determinism();
}
