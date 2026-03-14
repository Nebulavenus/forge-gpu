/*
 * Physics Library Tests — Rigid Body Contacts
 *
 * Tests for contact detection (sphere-plane, box-plane, sphere-sphere),
 * contact resolution with Coulomb friction, iterative solver stability,
 * determinism, and numerical safety.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Test constants for rigid body contacts ────────────────────────────── */

#define RBC_SPHERE_RADIUS    0.5f
#define RBC_BOX_HALF         0.5f
#define RBC_MASS             5.0f
#define RBC_DAMPING          0.99f
#define RBC_ANG_DAMPING      0.99f
#define RBC_RESTIT           0.5f
#define RBC_MU_S             0.6f
#define RBC_MU_D             0.4f
#define RBC_DT               (1.0f / 60.0f)
#define RBC_SPHERE_PEN_BASIC 0.2f       /* 0.5 radius sphere at y=0.3       */
#define RBC_SPHERE_Y_PEN     0.3f       /* RBC_SPHERE_RADIUS - RBC_SPHERE_PEN_BASIC */
#define RBC_INIT_VEL_X       2.0f       /* horizontal velocity for friction  */
#define RBC_INIT_VEL_Y_NEG  -1.0f       /* downward velocity for contact     */
#define RBC_FALL_VEL        -5.0f       /* fast downward for bounce tests    */
#define RBC_BOUNCE_VEL      -3.0f       /* moderate downward for bounce      */
#define RBC_RESTING_VEL     -0.1f       /* very slow — triggers resting      */
#define RBC_CONTACT_PEN      0.05f      /* small penetration for resting     */
#define RBC_CONTACT_PEN_MED  0.2f       /* medium penetration for friction   */
#define RBC_BOX_ROT_Z_45     45.0f      /* edge contact rotation (degrees)   */
#define RBC_BOX_ROT_X_35_26  35.26f     /* corner contact rotation (degrees) */
#define RBC_BOX_ROT_X_90     90.0f      /* full quarter-turn rotation        */
#define RBC_STACK_STEPS      300        /* simulation steps for stability    */
#define RBC_DETERMINISM_STEPS 200       /* steps for determinism tests       */
#define RBC_NAN_STEPS        1000       /* steps for NaN safety tests        */
#define RBC_SOLVER_ITERS     10         /* iteration count for solver tests  */
#define RBC_RESTING_EPS      0.5f       /* velocity epsilon for resting (includes Baumgarte bias) */
#define RBC_GROUND_PEN_TOL   0.05f      /* max allowed ground penetration (accounts for Baumgarte slop) */
#define RBC_BOX_Y_PEN        0.4f       /* box center Y for face-down penetration tests */
#define RBC_NORMAL_SCALE     10.0f      /* non-unit normal scale factor for normalization tests */
#define RBC_SOLVER_SPHERE_Y  0.4f       /* sphere center Y for solver iteration tests */
#define RBC_SOLVER_INIT_VY  -2.0f       /* initial downward velocity for solver tests */
#define RBC_SLIDING_VEL      10.0f      /* tangential sliding velocity for friction tests */
#define RBC_BOX_Y_EDGE       0.5f       /* box center Y for edge contact test */
#define RBC_BOX_Y_CORNER     0.6f       /* box center Y for corner contact test */
#define RBC_BOX_Y_ROTATED    0.9f       /* box center Y for rotated contact test */
#define RBC_NONCUBE_HX       0.5f       /* non-cubic box: half-extent X */
#define RBC_NONCUBE_HY       0.25f      /* non-cubic box: half-extent Y (short) */
#define RBC_NONCUBE_HZ       1.0f       /* non-cubic box: half-extent Z (tall) */
#define RBC_SPHERE_Y_BOUNCE  0.3f       /* sphere center Y for bounce tests */
#define RBC_SPHERE_Y_FRIC    0.45f      /* sphere center Y for friction/resting tests */
#define RBC_PAIR_OFFSET_X    0.8f       /* X offset for overlapping sphere pair */
#define RBC_PAIR_PENETRATION 0.2f       /* expected penetration for sphere pair */
#define RBC_SS_TOL           0.01f      /* tolerance for sphere-sphere geometry checks */
#define RBC_NEG_MU_S        -0.5f       /* negative static friction (clamp test) */
#define RBC_NEG_MU_D        -0.3f       /* negative dynamic friction (clamp test) */
#define RBC_CLAMP_MU_S       0.3f       /* static friction for mu_d>mu_s clamp test */
#define RBC_CLAMP_MU_D       0.8f       /* dynamic friction that exceeds mu_s */
#define RBC_TEST_MU_S        0.7f       /* test friction for stored-value check */
#define RBC_TEST_MU_D        0.3f       /* test friction for stored-value check */
#define RBC_SOLVER_CONTACT_Y 0.9f       /* Y position of second solver contact point */
#define RBC_STATIC_PEN_FRAC  0.2f       /* fraction of box half for synthetic penetration */
#define RBC_NAN_SOLVER_ITERS 8          /* solver iterations for NaN safety test */

/* Helper: create a dynamic sphere body at given position */
static ForgePhysicsRigidBody rbc_make_sphere(vec3 pos, float radius)
{
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        pos, RBC_MASS, RBC_DAMPING, RBC_ANG_DAMPING, RBC_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, radius);
    return rb;
}

/* Helper: create a dynamic box body at given position */
static ForgePhysicsRigidBody rbc_make_box(vec3 pos, vec3 half_ext)
{
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        pos, RBC_MASS, RBC_DAMPING, RBC_ANG_DAMPING, RBC_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb, half_ext);
    return rb;
}

/* Helper: create a default ground contact for resolve tests */
static ForgePhysicsRBContact rbc_make_ground_contact(int body_a,
                                                      float penetration)
{
    ForgePhysicsRBContact c;
    c.point             = vec3_create(0, 0, 0);
    c.normal            = vec3_create(0, 1, 0);
    c.penetration       = penetration;
    c.body_a            = body_a;
    c.body_b            = -1;
    c.static_friction   = RBC_MU_S;
    c.dynamic_friction  = RBC_MU_D;
    return c;
}

/* ── Sphere-plane contact detection ───────────────────────────────────── */

static void test_rb_sphere_plane_basic(void)
{
    TEST("rb_sphere_plane: basic penetration");
    float sphere_y = RBC_SPHERE_RADIUS - RBC_SPHERE_PEN_BASIC;
    ForgePhysicsRigidBody rb = rbc_make_sphere(
        vec3_create(0, sphere_y, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    vec3 plane_pt = vec3_create(0, 0, 0);
    vec3 plane_n  = vec3_create(0, 1, 0);
    bool hit = forge_physics_rb_collide_sphere_plane(
        &rb, 0, RBC_SPHERE_RADIUS, plane_pt, plane_n, RBC_MU_S, RBC_MU_D, &c);
    ASSERT_TRUE(hit);
    ASSERT_NEAR(c.penetration, RBC_SPHERE_PEN_BASIC, EPSILON);
    ASSERT_NEAR(c.normal.y, 1.0f, EPSILON);
    ASSERT_TRUE(c.body_a == 0);
    ASSERT_TRUE(c.body_b == -1);
    ASSERT_NEAR(c.static_friction, RBC_MU_S, EPSILON);
    ASSERT_NEAR(c.dynamic_friction, RBC_MU_D, EPSILON);
    END_TEST();
}

static void test_rb_sphere_plane_no_contact(void)
{
    TEST("rb_sphere_plane: no contact when above");
    ForgePhysicsRigidBody rb = rbc_make_sphere(
        vec3_create(0, 5, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    bool hit = forge_physics_rb_collide_sphere_plane(
        &rb, 0, RBC_SPHERE_RADIUS,
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, &c);
    ASSERT_TRUE(!hit);
    END_TEST();
}

static void test_rb_sphere_plane_resting(void)
{
    TEST("rb_sphere_plane: resting contact (exactly touching)");
    ForgePhysicsRigidBody rb = rbc_make_sphere(
        vec3_create(0, RBC_SPHERE_RADIUS, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    bool hit = forge_physics_rb_collide_sphere_plane(
        &rb, 0, RBC_SPHERE_RADIUS,
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, &c);
    ASSERT_TRUE(!hit);
    END_TEST();
}

static void test_rb_sphere_plane_static_body(void)
{
    TEST("rb_sphere_plane: works with static body too");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, RBC_SPHERE_Y_PEN, 0), 0.0f,
        RBC_DAMPING, RBC_ANG_DAMPING, RBC_RESTIT);
    ForgePhysicsRBContact c;
    bool hit = forge_physics_rb_collide_sphere_plane(
        &rb, 0, RBC_SPHERE_RADIUS,
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, &c);
    ASSERT_TRUE(hit);
    END_TEST();
}

static void test_rb_sphere_plane_zero_radius(void)
{
    TEST("rb_sphere_plane: zero radius returns false");
    ForgePhysicsRigidBody rb = rbc_make_sphere(
        vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    bool hit = forge_physics_rb_collide_sphere_plane(
        &rb, 0, 0.0f,
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, &c);
    ASSERT_TRUE(!hit);
    END_TEST();
}

static void test_rb_sphere_plane_null(void)
{
    TEST("rb_sphere_plane: NULL body returns false");
    ForgePhysicsRBContact c;
    bool hit = forge_physics_rb_collide_sphere_plane(
        NULL, 0, RBC_SPHERE_RADIUS,
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, &c);
    ASSERT_TRUE(!hit);
    END_TEST();
}

static void test_rb_sphere_plane_non_unit_normal(void)
{
    TEST("rb_sphere_plane: non-unit normal produces same result");
    float sphere_y = RBC_SPHERE_RADIUS - RBC_SPHERE_PEN_BASIC;
    ForgePhysicsRigidBody rb = rbc_make_sphere(
        vec3_create(0, sphere_y, 0), RBC_SPHERE_RADIUS);
    vec3 plane_pt = vec3_create(0, 0, 0);

    ForgePhysicsRBContact c_unit;
    bool hit_unit = forge_physics_rb_collide_sphere_plane(
        &rb, 0, RBC_SPHERE_RADIUS, plane_pt, vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, &c_unit);
    ASSERT_TRUE(hit_unit);

    ForgePhysicsRBContact c_scaled;
    bool hit_scaled = forge_physics_rb_collide_sphere_plane(
        &rb, 0, RBC_SPHERE_RADIUS, plane_pt, vec3_create(0, RBC_NORMAL_SCALE, 0),
        RBC_MU_S, RBC_MU_D, &c_scaled);
    ASSERT_TRUE(hit_scaled);
    ASSERT_NEAR(c_scaled.penetration, c_unit.penetration, EPSILON);
    ASSERT_NEAR(c_scaled.normal.y, c_unit.normal.y, EPSILON);
    END_TEST();
}

/* ── Box-plane contact detection ──────────────────────────────────────── */

static void test_rb_box_plane_face_down(void)
{
    TEST("rb_box_plane: face-down generates 4 contacts");
    ForgePhysicsRigidBody rb = rbc_make_box(
        vec3_create(0, RBC_BOX_Y_PEN, 0),
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF));
    ForgePhysicsRBContact contacts[8];
    int n = forge_physics_rb_collide_box_plane(
        &rb, 0,
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF),
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, contacts, 8);
    ASSERT_TRUE(n == 4);
    for (int i = 0; i < n; i++) {
        ASSERT_NEAR(contacts[i].normal.y, 1.0f, EPSILON);
        ASSERT_NEAR(contacts[i].penetration,
                     RBC_BOX_HALF - RBC_BOX_Y_PEN, EPSILON);
        ASSERT_TRUE(contacts[i].body_a == 0);
        ASSERT_TRUE(contacts[i].body_b == -1);
    }
    END_TEST();
}

static void test_rb_box_plane_edge_contact(void)
{
    TEST("rb_box_plane: tilted box produces 2 contacts");
    ForgePhysicsRigidBody rb = rbc_make_box(
        vec3_create(0, RBC_BOX_Y_EDGE, 0),
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF));
    rb.orientation = quat_from_axis_angle(
        vec3_create(0, 0, 1), RBC_BOX_ROT_Z_45 * FORGE_DEG2RAD);
    forge_physics_rigid_body_update_derived(&rb);
    ForgePhysicsRBContact contacts[8];
    int n = forge_physics_rb_collide_box_plane(
        &rb, 0,
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF),
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, contacts, 8);
    ASSERT_TRUE(n == 2);
    END_TEST();
}

static void test_rb_box_plane_corner_contact(void)
{
    TEST("rb_box_plane: tilted box on corner produces 1 contact");
    ForgePhysicsRigidBody rb = rbc_make_box(
        vec3_create(0, RBC_BOX_Y_CORNER, 0),
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF));
    quat q1 = quat_from_axis_angle(
        vec3_create(0, 0, 1), RBC_BOX_ROT_Z_45 * FORGE_DEG2RAD);
    quat q2 = quat_from_axis_angle(
        vec3_create(1, 0, 0), RBC_BOX_ROT_X_35_26 * FORGE_DEG2RAD);
    rb.orientation = quat_multiply(q2, q1);
    forge_physics_rigid_body_update_derived(&rb);
    ForgePhysicsRBContact contacts[8];
    int n = forge_physics_rb_collide_box_plane(
        &rb, 0,
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF),
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, contacts, 8);
    ASSERT_TRUE(n >= 1 && n <= 2);
    END_TEST();
}

static void test_rb_box_plane_no_contact(void)
{
    TEST("rb_box_plane: no contact when above");
    ForgePhysicsRigidBody rb = rbc_make_box(
        vec3_create(0, 5, 0),
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF));
    ForgePhysicsRBContact contacts[8];
    int n = forge_physics_rb_collide_box_plane(
        &rb, 0,
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF),
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, contacts, 8);
    ASSERT_TRUE(n == 0);
    END_TEST();
}

static void test_rb_box_plane_rotated(void)
{
    TEST("rb_box_plane: 90-degree rotation changes contact face");
    vec3 he = vec3_create(RBC_NONCUBE_HX, RBC_NONCUBE_HY, RBC_NONCUBE_HZ);
    ForgePhysicsRigidBody rb = rbc_make_box(
        vec3_create(0, RBC_BOX_Y_ROTATED, 0), he);
    rb.orientation = quat_from_axis_angle(
        vec3_create(1, 0, 0), RBC_BOX_ROT_X_90 * FORGE_DEG2RAD);
    forge_physics_rigid_body_update_derived(&rb);
    ForgePhysicsRBContact contacts[8];
    int n = forge_physics_rb_collide_box_plane(
        &rb, 0, he,
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, contacts, 8);
    ASSERT_TRUE(n == 4);
    END_TEST();
}

static void test_rb_box_plane_null(void)
{
    TEST("rb_box_plane: NULL body returns 0");
    ForgePhysicsRBContact contacts[8];
    int n = forge_physics_rb_collide_box_plane(
        NULL, 0,
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF),
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, contacts, 8);
    ASSERT_TRUE(n == 0);
    END_TEST();
}

static void test_rb_box_plane_non_unit_normal(void)
{
    TEST("rb_box_plane: non-unit normal produces same result");
    ForgePhysicsRigidBody rb = rbc_make_box(
        vec3_create(0, RBC_BOX_Y_PEN, 0),
        vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF));
    vec3 he = vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF);
    vec3 plane_pt = vec3_create(0, 0, 0);

    ForgePhysicsRBContact c_unit[8];
    int n_unit = forge_physics_rb_collide_box_plane(
        &rb, 0, he, plane_pt, vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, c_unit, 8);

    ForgePhysicsRBContact c_scaled[8];
    int n_scaled = forge_physics_rb_collide_box_plane(
        &rb, 0, he, plane_pt, vec3_create(0, RBC_NORMAL_SCALE, 0),
        RBC_MU_S, RBC_MU_D, c_scaled, 8);

    ASSERT_TRUE(n_unit == n_scaled);
    ASSERT_TRUE(n_unit == 4);
    ASSERT_NEAR(c_scaled[0].penetration, c_unit[0].penetration, EPSILON);
    ASSERT_NEAR(c_scaled[0].normal.y, c_unit[0].normal.y, EPSILON);
    END_TEST();
}

/* ── RB contact resolution ────────────────────────────────────────────── */

static void test_rb_contact_resolve_normal_impulse(void)
{
    TEST("rb_contact_resolve: falling sphere bounces off ground");
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = rbc_make_sphere(vec3_create(0, RBC_SPHERE_Y_BOUNCE, 0), RBC_SPHERE_RADIUS);
    bodies[0].velocity = vec3_create(0, RBC_FALL_VEL, 0);

    ForgePhysicsRBContact c = rbc_make_ground_contact(0, RBC_CONTACT_PEN_MED);

    forge_physics_rb_resolve_contact(&c, bodies, 1, RBC_DT);

    ASSERT_TRUE(bodies[0].velocity.y > 0.0f);
    ASSERT_NEAR(bodies[0].position.y, RBC_SPHERE_Y_BOUNCE, 1e-6f);
    END_TEST();
}

static void test_rb_contact_resolve_friction(void)
{
    TEST("rb_contact_resolve: sliding sphere loses tangential velocity");
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = rbc_make_sphere(vec3_create(0, RBC_SPHERE_Y_FRIC, 0), RBC_SPHERE_RADIUS);
    bodies[0].velocity = vec3_create(RBC_SLIDING_VEL, 0, 0);

    ForgePhysicsRBContact c = rbc_make_ground_contact(0, RBC_CONTACT_PEN);

    float speed_before = fabsf(bodies[0].velocity.x);
    forge_physics_rb_resolve_contact(&c, bodies, 1, RBC_DT);
    float speed_after = fabsf(bodies[0].velocity.x);

    /* Friction should reduce tangential speed */
    ASSERT_TRUE(speed_after < speed_before);
    /* Friction at the ground contact should spin the sphere backward
     * around -Z (sliding in +X on ground with +Y normal) without
     * introducing off-axis rotation. */
    ASSERT_TRUE(bodies[0].angular_velocity.z < -EPSILON);
    ASSERT_NEAR(bodies[0].angular_velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[0].angular_velocity.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_contact_resolve_resting(void)
{
    TEST("rb_contact_resolve: slow contact has zero restitution");
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = rbc_make_sphere(vec3_create(0, RBC_SPHERE_Y_FRIC, 0), RBC_SPHERE_RADIUS);
    bodies[0].velocity = vec3_create(0, RBC_RESTING_VEL, 0);

    ForgePhysicsRBContact c = rbc_make_ground_contact(0, RBC_CONTACT_PEN);

    forge_physics_rb_resolve_contact(&c, bodies, 1, RBC_DT);

    /* Resting contact: velocity should be near zero or slightly positive
     * (Baumgarte bias can push it positive) — not a full bounce */
    ASSERT_TRUE(fabsf(bodies[0].velocity.y) < RBC_RESTING_EPS);
    END_TEST();
}

static void test_rb_contact_resolve_plane_contact(void)
{
    TEST("rb_contact_resolve: sphere-plane contact produces valid response");
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = rbc_make_sphere(vec3_create(0, RBC_SPHERE_Y_PEN, 0), RBC_SPHERE_RADIUS);
    bodies[0].velocity = vec3_create(RBC_INIT_VEL_X, RBC_INIT_VEL_Y_NEG, 0);

    ForgePhysicsRBContact c;
    bool hit = forge_physics_rb_collide_sphere_plane(
        &bodies[0], 0, RBC_SPHERE_RADIUS,
        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        RBC_MU_S, RBC_MU_D, &c);
    ASSERT_TRUE(hit);

    forge_physics_rb_resolve_contact(&c, bodies, 1, RBC_DT);

    /* After resolve, vertical velocity should be non-negative (bounced) */
    ASSERT_TRUE(bodies[0].velocity.y >= 0.0f);
    ASSERT_TRUE(isfinite(bodies[0].velocity.x));
    ASSERT_TRUE(isfinite(bodies[0].velocity.y));
    END_TEST();
}

static void test_rb_contact_resolve_both_static(void)
{
    TEST("rb_contact_resolve: both-static bodies unchanged");
    ForgePhysicsRigidBody bodies[2];
    bodies[0] = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    bodies[0].mass = 0.0f; bodies[0].inv_mass = 0.0f;
    bodies[0].velocity = vec3_create(0, 0, 0);
    bodies[1] = rbc_make_sphere(vec3_create(RBC_PAIR_OFFSET_X, 0, 0), RBC_SPHERE_RADIUS);
    bodies[1].mass = 0.0f; bodies[1].inv_mass = 0.0f;
    bodies[1].velocity = vec3_create(0, 0, 0);

    ForgePhysicsRBContact c;
    c.point = vec3_create(RBC_PAIR_OFFSET_X / 2.0f, 0, 0);
    c.normal = vec3_create(-1, 0, 0);
    c.penetration = RBC_PAIR_PENETRATION;
    c.body_a = 0;
    c.body_b = 1;
    c.static_friction = RBC_MU_S;
    c.dynamic_friction = RBC_MU_D;

    forge_physics_rb_resolve_contact(&c, bodies, 2, RBC_DT);

    /* Both static — velocity and angular_velocity must remain zero */
    ASSERT_NEAR(bodies[0].velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[0].velocity.y, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[0].velocity.z, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[1].velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[1].velocity.y, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[1].velocity.z, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[0].angular_velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[0].angular_velocity.y, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[0].angular_velocity.z, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[1].angular_velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[1].angular_velocity.y, 0.0f, EPSILON);
    ASSERT_NEAR(bodies[1].angular_velocity.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_contact_resolve_null(void)
{
    TEST("rb_contact_resolve: NULL args do not crash");
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c = rbc_make_ground_contact(0, RBC_CONTACT_PEN);

    forge_physics_rb_resolve_contact(NULL, bodies, 1, RBC_DT);
    forge_physics_rb_resolve_contact(&c, NULL, 1, RBC_DT);
    forge_physics_rb_resolve_contact(&c, bodies, 0, RBC_DT);
    /* No crash means pass */
    END_TEST();
}

static void test_rb_contact_resolve_negative_friction(void)
{
    TEST("rb_contact_resolve: negative friction treated as zero");
    ForgePhysicsRigidBody bodies_neg[1];
    bodies_neg[0] = rbc_make_sphere(vec3_create(0, RBC_SPHERE_Y_PEN, 0), RBC_SPHERE_RADIUS);
    bodies_neg[0].velocity = vec3_create(RBC_INIT_VEL_X, RBC_INIT_VEL_Y_NEG, 0);
    ForgePhysicsRBContact c = rbc_make_ground_contact(0, RBC_CONTACT_PEN_MED);
    c.static_friction = RBC_NEG_MU_S;
    c.dynamic_friction = RBC_NEG_MU_D;
    forge_physics_rb_resolve_contact(&c, bodies_neg, 1, RBC_DT);
    ASSERT_TRUE(isfinite(bodies_neg[0].velocity.x));
    ASSERT_TRUE(isfinite(bodies_neg[0].velocity.y));

    ForgePhysicsRigidBody bodies_zero[1];
    bodies_zero[0] = rbc_make_sphere(vec3_create(0, RBC_SPHERE_Y_PEN, 0), RBC_SPHERE_RADIUS);
    bodies_zero[0].velocity = vec3_create(RBC_INIT_VEL_X, RBC_INIT_VEL_Y_NEG, 0);
    ForgePhysicsRBContact c0 = c;
    c0.static_friction = 0.0f;
    c0.dynamic_friction = 0.0f;
    forge_physics_rb_resolve_contact(&c0, bodies_zero, 1, RBC_DT);

    /* Negative friction must clamp to zero — same state as zero friction */
    ASSERT_NEAR(bodies_neg[0].velocity.x, bodies_zero[0].velocity.x, EPSILON);
    ASSERT_NEAR(bodies_neg[0].velocity.y, bodies_zero[0].velocity.y, EPSILON);
    ASSERT_NEAR(bodies_neg[0].angular_velocity.x,
                bodies_zero[0].angular_velocity.x, EPSILON);
    ASSERT_NEAR(bodies_neg[0].angular_velocity.y,
                bodies_zero[0].angular_velocity.y, EPSILON);
    ASSERT_NEAR(bodies_neg[0].angular_velocity.z,
                bodies_zero[0].angular_velocity.z, EPSILON);
    END_TEST();
}

static void test_rb_contact_resolve_mud_exceeds_mus(void)
{
    TEST("rb_contact_resolve: mu_d > mu_s clamped to mu_s");
    ForgePhysicsRigidBody bodies_bad[1];
    bodies_bad[0] = rbc_make_sphere(vec3_create(0, RBC_SPHERE_Y_PEN, 0), RBC_SPHERE_RADIUS);
    bodies_bad[0].velocity = vec3_create(RBC_INIT_VEL_X, RBC_INIT_VEL_Y_NEG, 0);
    ForgePhysicsRBContact c = rbc_make_ground_contact(0, RBC_CONTACT_PEN_MED);
    c.static_friction = RBC_CLAMP_MU_S;
    c.dynamic_friction = RBC_CLAMP_MU_D;
    forge_physics_rb_resolve_contact(&c, bodies_bad, 1, RBC_DT);
    ASSERT_TRUE(isfinite(bodies_bad[0].velocity.x));
    ASSERT_TRUE(isfinite(bodies_bad[0].velocity.y));

    ForgePhysicsRigidBody bodies_clamped[1];
    bodies_clamped[0] = rbc_make_sphere(vec3_create(0, RBC_SPHERE_Y_PEN, 0), RBC_SPHERE_RADIUS);
    bodies_clamped[0].velocity = vec3_create(RBC_INIT_VEL_X, RBC_INIT_VEL_Y_NEG, 0);
    ForgePhysicsRBContact c_clamped = c;
    c_clamped.dynamic_friction = RBC_CLAMP_MU_S;
    forge_physics_rb_resolve_contact(&c_clamped, bodies_clamped, 1, RBC_DT);

    /* mu_d must be clamped to mu_s — same state as clamped case */
    ASSERT_NEAR(bodies_bad[0].velocity.x, bodies_clamped[0].velocity.x, EPSILON);
    ASSERT_NEAR(bodies_bad[0].velocity.y, bodies_clamped[0].velocity.y, EPSILON);
    ASSERT_NEAR(bodies_bad[0].angular_velocity.x,
                bodies_clamped[0].angular_velocity.x, EPSILON);
    ASSERT_NEAR(bodies_bad[0].angular_velocity.y,
                bodies_clamped[0].angular_velocity.y, EPSILON);
    ASSERT_NEAR(bodies_bad[0].angular_velocity.z,
                bodies_clamped[0].angular_velocity.z, EPSILON);
    END_TEST();
}

/* ── Sphere-sphere rigid body collision ──────────────────────────────── */

static void test_rb_collide_sphere_sphere_basic(void)
{
    TEST("rb_sphere_sphere: overlapping spheres produce contact");
    ForgePhysicsRigidBody a = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRigidBody b = rbc_make_sphere(vec3_create(RBC_PAIR_OFFSET_X, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    ASSERT_TRUE(forge_physics_rb_collide_sphere_sphere(
        &a, 0, RBC_SPHERE_RADIUS, &b, 1, RBC_SPHERE_RADIUS,
        RBC_MU_S, RBC_MU_D, &c));
    ASSERT_NEAR(c.normal.x, -1.0f, RBC_SS_TOL);
    ASSERT_NEAR(c.normal.y, 0.0f, RBC_SS_TOL);
    ASSERT_NEAR(c.normal.z, 0.0f, RBC_SS_TOL);
    ASSERT_NEAR(c.penetration, RBC_PAIR_PENETRATION, RBC_SS_TOL);
    ASSERT_TRUE(c.body_a == 0);
    ASSERT_TRUE(c.body_b == 1);
    END_TEST();
}

static void test_rb_collide_sphere_sphere_separated(void)
{
    TEST("rb_sphere_sphere: separated spheres return false");
    ForgePhysicsRigidBody a = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRigidBody b = rbc_make_sphere(vec3_create(5, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    ASSERT_TRUE(!forge_physics_rb_collide_sphere_sphere(
        &a, 0, RBC_SPHERE_RADIUS, &b, 1, RBC_SPHERE_RADIUS,
        RBC_MU_S, RBC_MU_D, &c));
    END_TEST();
}

static void test_rb_collide_sphere_sphere_touching(void)
{
    TEST("rb_sphere_sphere: exactly touching spheres return false");
    ForgePhysicsRigidBody a = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRigidBody b = rbc_make_sphere(vec3_create(1.0f, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    ASSERT_TRUE(!forge_physics_rb_collide_sphere_sphere(
        &a, 0, RBC_SPHERE_RADIUS, &b, 1, RBC_SPHERE_RADIUS,
        RBC_MU_S, RBC_MU_D, &c));
    END_TEST();
}

static void test_rb_collide_sphere_sphere_coincident(void)
{
    TEST("rb_sphere_sphere: coincident centers use upward normal");
    ForgePhysicsRigidBody a = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRigidBody b = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    ASSERT_TRUE(forge_physics_rb_collide_sphere_sphere(
        &a, 0, RBC_SPHERE_RADIUS, &b, 1, RBC_SPHERE_RADIUS,
        RBC_MU_S, RBC_MU_D, &c));
    ASSERT_NEAR(c.normal.y, 1.0f, RBC_SS_TOL);
    END_TEST();
}

static void test_rb_collide_sphere_sphere_zero_radius(void)
{
    TEST("rb_sphere_sphere: zero radius returns false");
    ForgePhysicsRigidBody a = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRigidBody b = rbc_make_sphere(vec3_create(RBC_SPHERE_RADIUS, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    ASSERT_TRUE(!forge_physics_rb_collide_sphere_sphere(
        &a, 0, 0.0f, &b, 1, RBC_SPHERE_RADIUS,
        RBC_MU_S, RBC_MU_D, &c));
    END_TEST();
}

static void test_rb_collide_sphere_sphere_both_static(void)
{
    TEST("rb_sphere_sphere: two static bodies return false");
    ForgePhysicsRigidBody a = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRigidBody b = rbc_make_sphere(vec3_create(RBC_SPHERE_RADIUS, 0, 0), RBC_SPHERE_RADIUS);
    a.mass = 0.0f; a.inv_mass = 0.0f;
    b.mass = 0.0f; b.inv_mass = 0.0f;
    ForgePhysicsRBContact c;
    ASSERT_TRUE(!forge_physics_rb_collide_sphere_sphere(
        &a, 0, RBC_SPHERE_RADIUS, &b, 1, RBC_SPHERE_RADIUS,
        RBC_MU_S, RBC_MU_D, &c));
    END_TEST();
}

static void test_rb_collide_sphere_sphere_null(void)
{
    TEST("rb_sphere_sphere: NULL pointers return false");
    ForgePhysicsRigidBody a = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    ASSERT_TRUE(!forge_physics_rb_collide_sphere_sphere(
        NULL, 0, RBC_SPHERE_RADIUS, &a, 1, RBC_SPHERE_RADIUS,
        RBC_MU_S, RBC_MU_D, &c));
    ASSERT_TRUE(!forge_physics_rb_collide_sphere_sphere(
        &a, 0, RBC_SPHERE_RADIUS, NULL, 1, RBC_SPHERE_RADIUS,
        RBC_MU_S, RBC_MU_D, &c));
    ASSERT_TRUE(!forge_physics_rb_collide_sphere_sphere(
        &a, 0, RBC_SPHERE_RADIUS, &a, 1, RBC_SPHERE_RADIUS,
        RBC_MU_S, RBC_MU_D, NULL));
    END_TEST();
}

static void test_rb_collide_sphere_sphere_friction_stored(void)
{
    TEST("rb_sphere_sphere: friction coefficients stored in contact");
    ForgePhysicsRigidBody a = rbc_make_sphere(vec3_create(0, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRigidBody b = rbc_make_sphere(vec3_create(RBC_PAIR_OFFSET_X, 0, 0), RBC_SPHERE_RADIUS);
    ForgePhysicsRBContact c;
    float test_mu_s = RBC_TEST_MU_S;
    float test_mu_d = RBC_TEST_MU_D;
    ASSERT_TRUE(forge_physics_rb_collide_sphere_sphere(
        &a, 0, RBC_SPHERE_RADIUS, &b, 1, RBC_SPHERE_RADIUS,
        test_mu_s, test_mu_d, &c));
    ASSERT_NEAR(c.static_friction, test_mu_s, 1e-6f);
    ASSERT_NEAR(c.dynamic_friction, test_mu_d, 1e-6f);
    END_TEST();
}

/* ── Iterative solver ─────────────────────────────────────────────────── */

static void test_rb_solver_iterations_stable(void)
{
    TEST("rb_solver: iterative solve remains bounded and finite");
    ForgePhysicsRigidBody bodies[2];
    bodies[0] = rbc_make_sphere(vec3_create(0, RBC_SOLVER_SPHERE_Y, 0), RBC_SPHERE_RADIUS);
    bodies[0].velocity = vec3_create(0, RBC_SOLVER_INIT_VY, 0);
    bodies[1] = rbc_make_sphere(vec3_create(0, RBC_SOLVER_SPHERE_Y + 1.0f, 0), RBC_SPHERE_RADIUS);
    bodies[1].velocity = vec3_create(0, RBC_SOLVER_INIT_VY, 0);

    ForgePhysicsRBContact contacts[2];
    contacts[0].point = vec3_create(0, 0, 0);
    contacts[0].normal = vec3_create(0, 1, 0);
    contacts[0].penetration = RBC_BOX_HALF * RBC_STATIC_PEN_FRAC;
    contacts[0].body_a = 0;
    contacts[0].body_b = -1;
    contacts[0].static_friction = RBC_MU_S;
    contacts[0].dynamic_friction = RBC_MU_D;

    contacts[1].point = vec3_create(0, RBC_SOLVER_CONTACT_Y, 0);
    contacts[1].normal = vec3_create(0, 1, 0);
    contacts[1].penetration = RBC_BOX_HALF * RBC_STATIC_PEN_FRAC;
    contacts[1].body_a = 1;
    contacts[1].body_b = 0;
    contacts[1].static_friction = RBC_MU_S;
    contacts[1].dynamic_friction = RBC_MU_D;

    ForgePhysicsRigidBody bodies1[2], bodies10[2];
    bodies1[0] = bodies[0]; bodies1[1] = bodies[1];
    bodies10[0] = bodies[0]; bodies10[1] = bodies[1];

    forge_physics_rb_resolve_contacts(contacts, 2, bodies1, 2, 1, RBC_DT);
    forge_physics_rb_resolve_contacts(
        contacts, 2, bodies10, 2, RBC_SOLVER_ITERS, RBC_DT);

    float residual_10 = fabsf(bodies10[0].velocity.y) + fabsf(bodies10[1].velocity.y);
    float residual_1  = fabsf(bodies1[0].velocity.y) + fabsf(bodies1[1].velocity.y);
    ASSERT_TRUE(residual_1 < 50.0f);
    ASSERT_TRUE(residual_10 < 50.0f);
    ASSERT_TRUE(!isnan(bodies10[0].velocity.y));
    ASSERT_TRUE(!isnan(bodies10[1].velocity.y));
    /* Solver should have corrected downward velocity */
    ASSERT_TRUE(bodies10[0].velocity.y > RBC_SOLVER_INIT_VY);
    ASSERT_TRUE(bodies10[1].velocity.y > RBC_SOLVER_INIT_VY);
    END_TEST();
}

static void test_rb_solver_ground_contact_stable(void)
{
    TEST("rb_solver: boxes with ground contacts stay above ground after many steps");
    ForgePhysicsRigidBody bodies[3];
    vec3 he = vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF);

    bodies[0] = rbc_make_box(vec3_create(0, RBC_BOX_HALF, 0), he);
    bodies[1] = rbc_make_box(vec3_create(0, RBC_BOX_HALF * 3.0f, 0), he);
    bodies[2] = rbc_make_box(vec3_create(0, RBC_BOX_HALF * 5.0f, 0), he);

    vec3 gravity = vec3_create(0, -9.81f, 0);
    vec3 plane_pt = vec3_create(0, 0, 0);
    vec3 plane_n  = vec3_create(0, 1, 0);

    for (int step = 0; step < RBC_STACK_STEPS; step++) {
        for (int i = 0; i < 3; i++)
            forge_physics_rigid_body_apply_gravity(&bodies[i], gravity);
        for (int i = 0; i < 3; i++)
            forge_physics_rigid_body_integrate(&bodies[i], RBC_DT);

        ForgePhysicsRBContact contacts[FORGE_PHYSICS_MAX_RB_CONTACTS];
        int num_contacts = 0;
        for (int i = 0; i < 3 && num_contacts < FORGE_PHYSICS_MAX_RB_CONTACTS - 8; i++) {
            num_contacts += forge_physics_rb_collide_box_plane(
                &bodies[i], i, he, plane_pt, plane_n,
                RBC_MU_S, RBC_MU_D,
                &contacts[num_contacts],
                FORGE_PHYSICS_MAX_RB_CONTACTS - num_contacts);
        }

        forge_physics_rb_resolve_contacts(contacts, num_contacts,
                                          bodies, 3, RBC_SOLVER_ITERS, RBC_DT);
    }

    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE((bodies[i].position.y - RBC_BOX_HALF) >= -RBC_GROUND_PEN_TOL);
        ASSERT_TRUE(!isnan(bodies[i].position.y));
        ASSERT_TRUE(!isinf(bodies[i].position.y));
    }
    END_TEST();
}

static void test_rb_solver_empty_contacts(void)
{
    TEST("rb_solver: empty contacts does not crash");
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = rbc_make_sphere(vec3_create(0, 5, 0), RBC_SPHERE_RADIUS);
    forge_physics_rb_resolve_contacts(NULL, 0, bodies, 1, RBC_SOLVER_ITERS, RBC_DT);
    END_TEST();
}

/* ── Determinism ──────────────────────────────────────────────────────── */

static void test_rb_contact_determinism(void)
{
    TEST("rb_contact: two identical runs produce identical results");
    vec3 he = vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF);
    vec3 gravity = vec3_create(0, -9.81f, 0);
    vec3 plane_pt = vec3_create(0, 0, 0);
    vec3 plane_n  = vec3_create(0, 1, 0);

    /* Run 1 */
    ForgePhysicsRigidBody a = rbc_make_box(vec3_create(0, 3, 0), he);
    a.velocity = vec3_create(RBC_INIT_VEL_X, RBC_INIT_VEL_Y_NEG, 0);
    for (int step = 0; step < RBC_DETERMINISM_STEPS; step++) {
        forge_physics_rigid_body_apply_gravity(&a, gravity);
        forge_physics_rigid_body_integrate(&a, RBC_DT);
        ForgePhysicsRBContact contacts[8];
        int nc = forge_physics_rb_collide_box_plane(
            &a, 0, he, plane_pt, plane_n, RBC_MU_S, RBC_MU_D, contacts, 8);
        if (nc > 0) {
            forge_physics_rb_resolve_contacts(
                contacts, nc, &a, 1, RBC_SOLVER_ITERS, RBC_DT);
        }
    }

    /* Run 2 — identical setup */
    ForgePhysicsRigidBody b = rbc_make_box(vec3_create(0, 3, 0), he);
    b.velocity = vec3_create(RBC_INIT_VEL_X, RBC_INIT_VEL_Y_NEG, 0);
    for (int step = 0; step < RBC_DETERMINISM_STEPS; step++) {
        forge_physics_rigid_body_apply_gravity(&b, gravity);
        forge_physics_rigid_body_integrate(&b, RBC_DT);
        ForgePhysicsRBContact contacts[8];
        int nc = forge_physics_rb_collide_box_plane(
            &b, 0, he, plane_pt, plane_n, RBC_MU_S, RBC_MU_D, contacts, 8);
        if (nc > 0) {
            forge_physics_rb_resolve_contacts(
                contacts, nc, &b, 1, RBC_SOLVER_ITERS, RBC_DT);
        }
    }

    /* Same process, same setup — must match exactly (epsilon = 0) */
    ASSERT_NEAR(a.position.x, b.position.x, 0.0f);
    ASSERT_NEAR(a.position.y, b.position.y, 0.0f);
    ASSERT_NEAR(a.position.z, b.position.z, 0.0f);
    ASSERT_NEAR(a.velocity.x, b.velocity.x, 0.0f);
    ASSERT_NEAR(a.velocity.y, b.velocity.y, 0.0f);
    ASSERT_NEAR(a.velocity.z, b.velocity.z, 0.0f);
    ASSERT_NEAR(a.angular_velocity.x, b.angular_velocity.x, 0.0f);
    ASSERT_NEAR(a.angular_velocity.y, b.angular_velocity.y, 0.0f);
    ASSERT_NEAR(a.angular_velocity.z, b.angular_velocity.z, 0.0f);
    ASSERT_NEAR(a.orientation.w, b.orientation.w, 0.0f);
    ASSERT_NEAR(a.orientation.x, b.orientation.x, 0.0f);
    ASSERT_NEAR(a.orientation.y, b.orientation.y, 0.0f);
    ASSERT_NEAR(a.orientation.z, b.orientation.z, 0.0f);
    END_TEST();
}

/* ── Stability — no NaN ───────────────────────────────────────────────── */

static void test_rb_contact_no_nan(void)
{
    TEST("rb_contact: no NaN after 1000 steps with contacts");
    ForgePhysicsRigidBody bodies[2];
    vec3 he = vec3_create(RBC_BOX_HALF, RBC_BOX_HALF, RBC_BOX_HALF);
    bodies[0] = rbc_make_box(vec3_create(0, 2, 0), he);
    bodies[0].velocity = vec3_create(3, -5, 1);
    bodies[0].angular_velocity = vec3_create(1, 2, -1);
    bodies[1] = rbc_make_sphere(vec3_create(1, 3, 0), RBC_SPHERE_RADIUS);
    bodies[1].velocity = vec3_create(-1, -3, 0);

    vec3 gravity = vec3_create(0, -9.81f, 0);
    vec3 plane_pt = vec3_create(0, 0, 0);
    vec3 plane_n  = vec3_create(0, 1, 0);

    for (int step = 0; step < RBC_NAN_STEPS; step++) {
        for (int i = 0; i < 2; i++)
            forge_physics_rigid_body_apply_gravity(&bodies[i], gravity);
        for (int i = 0; i < 2; i++)
            forge_physics_rigid_body_integrate(&bodies[i], RBC_DT);

        ForgePhysicsRBContact contacts[FORGE_PHYSICS_MAX_RB_CONTACTS];
        int nc = 0;
        nc += forge_physics_rb_collide_box_plane(
            &bodies[0], 0, he, plane_pt, plane_n,
            RBC_MU_S, RBC_MU_D, &contacts[nc],
            FORGE_PHYSICS_MAX_RB_CONTACTS - nc);
        ForgePhysicsRBContact sc;
        if (forge_physics_rb_collide_sphere_plane(
                &bodies[1], 1, RBC_SPHERE_RADIUS, plane_pt, plane_n,
                RBC_MU_S, RBC_MU_D, &sc)) {
            if (nc < FORGE_PHYSICS_MAX_RB_CONTACTS) {
                contacts[nc++] = sc;
            }
        }

        if (nc > 0) {
            forge_physics_rb_resolve_contacts(contacts, nc, bodies, 2, RBC_NAN_SOLVER_ITERS, RBC_DT);
        }
    }

    for (int i = 0; i < 2; i++) {
        ASSERT_TRUE(!isnan(bodies[i].position.x));
        ASSERT_TRUE(!isnan(bodies[i].position.y));
        ASSERT_TRUE(!isnan(bodies[i].position.z));
        ASSERT_TRUE(!isinf(bodies[i].position.x));
        ASSERT_TRUE(!isinf(bodies[i].position.y));
        ASSERT_TRUE(!isinf(bodies[i].position.z));
        ASSERT_TRUE(!isnan(bodies[i].velocity.x));
        ASSERT_TRUE(!isnan(bodies[i].velocity.y));
        ASSERT_TRUE(!isnan(bodies[i].velocity.z));
        ASSERT_TRUE(!isinf(bodies[i].velocity.x));
        ASSERT_TRUE(!isinf(bodies[i].velocity.y));
        ASSERT_TRUE(!isinf(bodies[i].velocity.z));
        ASSERT_TRUE(!isnan(bodies[i].angular_velocity.x));
        ASSERT_TRUE(!isnan(bodies[i].angular_velocity.y));
        ASSERT_TRUE(!isnan(bodies[i].angular_velocity.z));
        ASSERT_TRUE(!isinf(bodies[i].angular_velocity.x));
        ASSERT_TRUE(!isinf(bodies[i].angular_velocity.y));
        ASSERT_TRUE(!isinf(bodies[i].angular_velocity.z));
        ASSERT_TRUE(!isnan(bodies[i].orientation.w));
        ASSERT_TRUE(!isnan(bodies[i].orientation.x));
        ASSERT_TRUE(!isnan(bodies[i].orientation.y));
        ASSERT_TRUE(!isnan(bodies[i].orientation.z));
        ASSERT_TRUE(!isinf(bodies[i].orientation.w));
        ASSERT_TRUE(!isinf(bodies[i].orientation.x));
        ASSERT_TRUE(!isinf(bodies[i].orientation.y));
        ASSERT_TRUE(!isinf(bodies[i].orientation.z));
    }
    END_TEST();
}

/* ── Runner — called from test_physics.c main() ──────────────────────── */

void run_rbc_tests(void)
{
    SDL_Log("--- rb sphere-plane detection ---");
    test_rb_sphere_plane_basic();
    test_rb_sphere_plane_no_contact();
    test_rb_sphere_plane_resting();
    test_rb_sphere_plane_static_body();
    test_rb_sphere_plane_zero_radius();
    test_rb_sphere_plane_null();
    test_rb_sphere_plane_non_unit_normal();

    SDL_Log("--- rb box-plane detection ---");
    test_rb_box_plane_face_down();
    test_rb_box_plane_edge_contact();
    test_rb_box_plane_corner_contact();
    test_rb_box_plane_no_contact();
    test_rb_box_plane_rotated();
    test_rb_box_plane_null();
    test_rb_box_plane_non_unit_normal();

    SDL_Log("--- rb contact resolution ---");
    test_rb_contact_resolve_normal_impulse();
    test_rb_contact_resolve_friction();
    test_rb_contact_resolve_resting();
    test_rb_contact_resolve_plane_contact();
    test_rb_contact_resolve_both_static();
    test_rb_contact_resolve_null();
    test_rb_contact_resolve_negative_friction();
    test_rb_contact_resolve_mud_exceeds_mus();

    SDL_Log("--- rb sphere-sphere detection ---");
    test_rb_collide_sphere_sphere_basic();
    test_rb_collide_sphere_sphere_separated();
    test_rb_collide_sphere_sphere_touching();
    test_rb_collide_sphere_sphere_coincident();
    test_rb_collide_sphere_sphere_zero_radius();
    test_rb_collide_sphere_sphere_both_static();
    test_rb_collide_sphere_sphere_null();
    test_rb_collide_sphere_sphere_friction_stored();

    SDL_Log("--- rb iterative solver ---");
    test_rb_solver_iterations_stable();
    test_rb_solver_ground_contact_stable();
    test_rb_solver_empty_contacts();

    SDL_Log("--- rb contact determinism ---");
    test_rb_contact_determinism();

    SDL_Log("--- rb contact stability ---");
    test_rb_contact_no_nan();
}
