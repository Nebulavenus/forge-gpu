/*
 * Joint Constraint Solver Tests
 *
 * Tests for forge_physics_joint_* functions added in Lesson 13.
 * Validates joint creation, anchor coincidence, axis constraints,
 * warm-starting convergence, determinism, and numerical stability.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Test constants ───────────────────────────────────────────────────────── */

#define JOINT_TEST_ANCHOR_TOL     0.05f     /* anchor coincidence tolerance (m) */
#define JOINT_TEST_ANGULAR_TOL    0.1f      /* angular constraint tolerance (rad/s) */
#define JOINT_TEST_STABILITY_STEPS 10000    /* long-run stability test steps */
#define JOINT_TEST_DT              PHYSICS_DT
#define JOINT_TEST_SOLVER_ITERS   20        /* iteration count for solver */
#define JOINT_TEST_BAUMGARTE      0.2f      /* position correction fraction */
#define JOINT_TEST_DEFAULT_MASS   2.0f      /* standard test body mass (kg) */
#define JOINT_TEST_HEAVY_MASS     1000.0f   /* extreme mass ratio test (kg) */
#define JOINT_TEST_LIGHT_MASS     1.0f      /* light body mass (kg) */
#define JOINT_TEST_PENDULUM_MASS  5.0f      /* pendulum body mass (kg) */
#define JOINT_TEST_DRIFT_TOL      0.5f      /* slider/extreme drift tolerance (m) */
#define JOINT_TEST_POS_BOUND      100.0f    /* stability test position bound (m) */
#define JOINT_TEST_WARM_ITERS     5         /* warm-start comparison iterations */
#define JOINT_TEST_WARM_STEPS     30        /* warm-start comparison frame count */
#define JOINT_TEST_WARM_TOL       0.01f     /* warm-start error tolerance (m) */
#define JOINT_TEST_SHORT_STEPS    500       /* standard short simulation steps */
#define JOINT_TEST_MEDIUM_STEPS   1000      /* standard medium simulation steps */
#define JOINT_TEST_EXTREME_STEPS  2000      /* extreme mass ratio steps */
#define JOINT_TEST_VERY_SHORT_STEPS 10      /* very short run for velocity checks */
#define JOINT_TEST_RESTITUTION    0.2f      /* default restitution coefficient */
#define JOINT_TEST_GRAVITY        9.81f     /* gravitational acceleration (m/s^2) */
#define JOINT_TEST_HINGE_ANG_VEL  2.0f      /* initial angular vel for hinge tests (rad/s) */
#define JOINT_TEST_FREE_ROT_VEL   3.0f      /* initial angular vel for free-rotation (rad/s) */
#define JOINT_TEST_FREE_ROT_MIN   1.0f      /* min retained angular vel (rad/s) */
#define JOINT_TEST_SLIDER_VEL     2.0f      /* initial velocity for slider tests (m/s) */
#define JOINT_TEST_EXACT_TOL      1e-6f     /* bit-exact determinism tolerance */
#define JOINT_TEST_CONV_TOL       1e-4f     /* convenience wrapper tolerance */

/* ── Helpers ──────────────────────────────────────────────────────────────── */

/* Create a dynamic rigid body at position with given mass and box inertia. */
static ForgePhysicsRigidBody make_joint_body(vec3 pos, float mass)
{
    ForgePhysicsRigidBody b;
    SDL_memset(&b, 0, sizeof(b));
    b.position    = pos;
    b.orientation = quat_identity();
    b.mass        = mass;
    b.inv_mass    = (mass > FORGE_PHYSICS_EPSILON) ? (1.0f / mass) : 0.0f;
    b.restitution = JOINT_TEST_RESTITUTION;
    b.damping     = 1.0f;
    b.angular_damping = 1.0f;

    /* Box inertia (1×1×1 box): I = (1/6) * m * s^2 */
    if (mass > FORGE_PHYSICS_EPSILON) {
        float I = mass / 6.0f;
        float inv_I = 1.0f / I;
        b.inertia_local     = mat3_from_diagonal(I, I, I);
        b.inv_inertia_local = mat3_from_diagonal(inv_I, inv_I, inv_I);
        b.inv_inertia_world = mat3_from_diagonal(inv_I, inv_I, inv_I);
    }

    return b;
}

/* Run the joint solver pipeline for N steps (no contacts). */
static void simulate_joints(
    ForgePhysicsRigidBody *bodies, int nb,
    ForgePhysicsJoint *joints, int nj,
    int steps, float dt)
{
    SDL_assert(nj <= 16 && "workspace buffer exceeded");
    ForgePhysicsJointSolverData workspace[16];
    vec3 gravity = vec3_create(0, -JOINT_TEST_GRAVITY, 0);

    for (int step = 0; step < steps; step++) {
        /* Apply gravity and integrate velocities */
        for (int i = 0; i < nb; i++) {
            if (bodies[i].inv_mass > 0.0f) {
                vec3 gforce = vec3_scale(gravity, bodies[i].mass);
                forge_physics_rigid_body_apply_force(&bodies[i], gforce);
            }
            forge_physics_rigid_body_integrate_velocities(&bodies[i], dt);
        }

        /* Solve joint constraints */
        forge_physics_joint_prepare(joints, nj, bodies, nb, dt, workspace);
        forge_physics_joint_warm_start(joints, workspace, nj, bodies, nb);
        for (int iter = 0; iter < JOINT_TEST_SOLVER_ITERS; iter++) {
            forge_physics_joint_solve_velocities(
                joints, workspace, nj, bodies, nb);
        }
        forge_physics_joint_store_impulses(joints, workspace, nj);

        /* Position correction */
        forge_physics_joint_correct_positions(
            joints, nj, bodies, nb, JOINT_TEST_BAUMGARTE,
            FORGE_PHYSICS_JOINT_SLOP);

        /* Integrate positions */
        for (int i = 0; i < nb; i++) {
            forge_physics_rigid_body_integrate_positions(&bodies[i], dt);
        }
    }
}

/* Compute world-space anchor position for a joint endpoint. */
static vec3 world_anchor(
    const ForgePhysicsRigidBody *bodies, int nb,
    int body_idx, vec3 local_anchor)
{
    if (body_idx < 0 || body_idx >= nb)
        return local_anchor; /* world anchor */
    const ForgePhysicsRigidBody *b = &bodies[body_idx];
    return vec3_add(b->position, quat_rotate_vec3(b->orientation, local_anchor));
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_JOINT_ball_socket_creation(void)
{
    TEST("JOINT_ball_socket_creation");

    vec3 anchor_a = vec3_create(0, 0.5f, 0);
    vec3 anchor_b = vec3_create(0, 5, 0);
    ForgePhysicsJoint j = forge_physics_joint_ball_socket(
        0, -1, anchor_a, anchor_b);

    ASSERT_TRUE(j.type == FORGE_PHYSICS_JOINT_BALL_SOCKET);
    ASSERT_TRUE(j.body_a == 0);
    ASSERT_TRUE(j.body_b == -1);
    ASSERT_NEAR(j.local_anchor_a.y, 0.5f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(j.local_anchor_b.y, 5.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(j.j_point.x, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(j.j_point.y, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(j.j_point.z, 0.0f, JOINT_TEST_EXACT_TOL);

    END_TEST();
}

static void test_JOINT_ball_socket_holds_anchor(void)
{
    TEST("JOINT_ball_socket_holds_anchor");

    /* Two bodies connected by a ball-socket joint. Body A at (0,4,0),
     * body B at (0,2,0). Joint at the midpoint (body A bottom, body B top).
     * Under gravity, both fall but must stay connected. */
    ForgePhysicsRigidBody bodies[2];
    bodies[0] = make_joint_body(vec3_create(0, 4, 0), JOINT_TEST_LIGHT_MASS);
    bodies[1] = make_joint_body(vec3_create(0, 2, 0), JOINT_TEST_LIGHT_MASS);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_ball_socket(
        0, 1,
        vec3_create(0, -0.5f, 0),  /* bottom of body A */
        vec3_create(0,  0.5f, 0)); /* top of body B */

    simulate_joints(bodies, 2, joints, 1, JOINT_TEST_MEDIUM_STEPS, JOINT_TEST_DT);

    /* Check anchors coincide */
    vec3 wa = world_anchor(bodies, 2, 0, joints[0].local_anchor_a);
    vec3 wb = world_anchor(bodies, 2, 1, joints[0].local_anchor_b);
    float dist = vec3_length(vec3_sub(wa, wb));

    ASSERT_TRUE(dist < JOINT_TEST_ANCHOR_TOL);

    /* Check no NaN */
    ASSERT_TRUE(forge_isfinite(bodies[0].position.x));
    ASSERT_TRUE(forge_isfinite(bodies[0].position.y));
    ASSERT_TRUE(forge_isfinite(bodies[1].position.x));
    ASSERT_TRUE(forge_isfinite(bodies[1].position.y));

    END_TEST();
}

static void test_JOINT_ball_socket_world_anchor(void)
{
    TEST("JOINT_ball_socket_world_anchor");

    /* Single body anchored to world at (0, 5, 0) — pendulum. */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(1, 5, 0), JOINT_TEST_DEFAULT_MASS);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_ball_socket(
        0, -1,
        vec3_create(0, 0.5f, 0),   /* top of body */
        vec3_create(0, 5.5f, 0));  /* world anchor */

    simulate_joints(bodies, 1, joints, 1, JOINT_TEST_SHORT_STEPS, JOINT_TEST_DT);

    /* Body should swing but anchor should stay near world point */
    vec3 wa = world_anchor(bodies, 1, 0, joints[0].local_anchor_a);
    vec3 target = vec3_create(0, 5.5f, 0);
    float dist = vec3_length(vec3_sub(wa, target));

    ASSERT_TRUE(dist < JOINT_TEST_ANCHOR_TOL);
    ASSERT_TRUE(forge_isfinite(bodies[0].velocity.y));

    END_TEST();
}

static void test_JOINT_hinge_axis_constraint(void)
{
    TEST("JOINT_hinge_axis_constraint");

    /* Body hinged to world along Y axis. Give it angular velocity
     * around X — the hinge should suppress that rotation. */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(0, 3, 0), JOINT_TEST_DEFAULT_MASS);
    bodies[0].angular_velocity = vec3_create(JOINT_TEST_HINGE_ANG_VEL, 0, 0);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_hinge(
        0, -1,
        vec3_create(0, 0.5f, 0),
        vec3_create(0, 3.5f, 0),
        vec3_create(0, 1, 0));  /* Y axis */

    simulate_joints(bodies, 1, joints, 1, JOINT_TEST_SHORT_STEPS, JOINT_TEST_DT);

    /* Angular velocity perpendicular to hinge axis should be near zero */
    float wx = SDL_fabsf(bodies[0].angular_velocity.x);
    float wz = SDL_fabsf(bodies[0].angular_velocity.z);

    /* Allow some tolerance — constraint may not fully damp in finite iters */
    ASSERT_TRUE(wx < JOINT_TEST_ANGULAR_TOL);
    ASSERT_TRUE(wz < JOINT_TEST_ANGULAR_TOL);

    END_TEST();
}

static void test_JOINT_hinge_free_rotation(void)
{
    TEST("JOINT_hinge_free_rotation");

    /* Body hinged along Y axis with angular velocity around Y.
     * The hinge should allow this rotation freely. */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(0, 3, 0), JOINT_TEST_DEFAULT_MASS);
    bodies[0].angular_velocity = vec3_create(0, JOINT_TEST_FREE_ROT_VEL, 0);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_hinge(
        0, -1,
        vec3_create(0, 0.5f, 0),
        vec3_create(0, 3.5f, 0),
        vec3_create(0, 1, 0));

    /* Only a few steps — check angular velocity around Y is preserved */
    simulate_joints(bodies, 1, joints, 1, JOINT_TEST_VERY_SHORT_STEPS,
                    JOINT_TEST_DT);

    /* Y angular velocity should still be significant (not clamped to zero) */
    float wy = SDL_fabsf(bodies[0].angular_velocity.y);
    ASSERT_TRUE(wy > JOINT_TEST_FREE_ROT_MIN);

    END_TEST();
}

static void test_JOINT_slider_axis_translation(void)
{
    TEST("JOINT_slider_axis_translation");

    /* Body on slider along X axis. Give it velocity in Y — slider
     * should suppress perpendicular translation. */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(0, 3, 0), JOINT_TEST_DEFAULT_MASS);
    bodies[0].velocity = vec3_create(0, JOINT_TEST_SLIDER_VEL, 0);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_slider(
        0, -1,
        vec3_create(0, 0, 0),
        vec3_create(0, 3, 0),
        vec3_create(1, 0, 0));  /* slide along X */

    simulate_joints(bodies, 1, joints, 1, JOINT_TEST_SHORT_STEPS, JOINT_TEST_DT);

    /* Position should be near Y=3 (initial), not drifting away */
    float y_drift = SDL_fabsf(bodies[0].position.y - 3.0f);
    float z_drift = SDL_fabsf(bodies[0].position.z);

    /* Allow some tolerance for Baumgarte correction lag */
    ASSERT_TRUE(y_drift < JOINT_TEST_DRIFT_TOL);
    ASSERT_TRUE(z_drift < JOINT_TEST_DRIFT_TOL);

    END_TEST();
}

static void test_JOINT_slider_rotation_locked(void)
{
    TEST("JOINT_slider_rotation_locked");

    /* Body on slider — give it angular velocity. Slider should lock
     * all rotation. */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(0, 3, 0), JOINT_TEST_DEFAULT_MASS);
    bodies[0].angular_velocity = vec3_create(JOINT_TEST_HINGE_ANG_VEL,
                                             JOINT_TEST_FREE_ROT_MIN,
                                             JOINT_TEST_FREE_ROT_VEL);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_slider(
        0, -1,
        vec3_create(0, 0, 0),
        vec3_create(0, 3, 0),
        vec3_create(1, 0, 0));

    simulate_joints(bodies, 1, joints, 1, JOINT_TEST_SHORT_STEPS, JOINT_TEST_DT);

    /* All angular velocity components should be near zero */
    float wx = SDL_fabsf(bodies[0].angular_velocity.x);
    float wy = SDL_fabsf(bodies[0].angular_velocity.y);
    float wz = SDL_fabsf(bodies[0].angular_velocity.z);

    ASSERT_TRUE(wx < JOINT_TEST_ANGULAR_TOL);
    ASSERT_TRUE(wy < JOINT_TEST_ANGULAR_TOL);
    ASSERT_TRUE(wz < JOINT_TEST_ANGULAR_TOL);

    END_TEST();
}

static void test_JOINT_slider_free_axis(void)
{
    TEST("JOINT_slider_free_axis");

    /* Body on slider along X axis with X velocity — should slide freely.
     * The point constraint must NOT lock the slide axis. */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(0, 3, 0), JOINT_TEST_DEFAULT_MASS);
    bodies[0].velocity = vec3_create(JOINT_TEST_SLIDER_VEL, 0, 0);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_slider(
        0, -1,
        vec3_create(0, 0, 0),
        vec3_create(0, 3, 0),
        vec3_create(1, 0, 0));  /* slide along X */

    simulate_joints(bodies, 1, joints, 1, JOINT_TEST_VERY_SHORT_STEPS,
                    JOINT_TEST_DT);

    /* X position should have changed — body is free to slide along X */
    ASSERT_TRUE(SDL_fabsf(bodies[0].position.x) > 0.1f);

    /* Y and Z should still be constrained near initial position */
    float y_drift = SDL_fabsf(bodies[0].position.y - 3.0f);
    float z_drift = SDL_fabsf(bodies[0].position.z);
    ASSERT_TRUE(y_drift < JOINT_TEST_DRIFT_TOL);
    ASSERT_TRUE(z_drift < JOINT_TEST_DRIFT_TOL);

    END_TEST();
}

static void test_JOINT_warm_start_convergence(void)
{
    TEST("JOINT_warm_start_convergence");

    /* Compare constraint error with and without warm-starting.
     * With warm-start, error should be significantly lower after
     * the same number of iterations. */

    /* Setup: pendulum body */
    ForgePhysicsRigidBody bodies_warm[1];
    ForgePhysicsRigidBody bodies_cold[1];
    bodies_warm[0] = make_joint_body(vec3_create(2, 4, 0), JOINT_TEST_PENDULUM_MASS);
    bodies_cold[0] = make_joint_body(vec3_create(2, 4, 0), JOINT_TEST_PENDULUM_MASS);

    ForgePhysicsJoint joints_warm[1];
    ForgePhysicsJoint joints_cold[1];
    joints_warm[0] = forge_physics_joint_ball_socket(
        0, -1, vec3_create(0, 0.5f, 0), vec3_create(0, 5, 0));
    joints_cold[0] = forge_physics_joint_ball_socket(
        0, -1, vec3_create(0, 0.5f, 0), vec3_create(0, 5, 0));

    vec3 gravity = vec3_create(0, -JOINT_TEST_GRAVITY, 0);
    ForgePhysicsJointSolverData ws[1];
    int solver_iters = JOINT_TEST_WARM_ITERS; /* deliberately low */

    for (int step = 0; step < JOINT_TEST_WARM_STEPS; step++) {
        /* Warm-start run */
        forge_physics_rigid_body_apply_force(&bodies_warm[0],
            vec3_scale(gravity, bodies_warm[0].mass));
        forge_physics_rigid_body_integrate_velocities(
            &bodies_warm[0], JOINT_TEST_DT);

        forge_physics_joint_prepare(joints_warm, 1, bodies_warm, 1,
                                    JOINT_TEST_DT, ws);
        forge_physics_joint_warm_start(joints_warm, ws, 1, bodies_warm, 1);
        for (int it = 0; it < solver_iters; it++)
            forge_physics_joint_solve_velocities(
                joints_warm, ws, 1, bodies_warm, 1);
        forge_physics_joint_store_impulses(joints_warm, ws, 1);
        forge_physics_rigid_body_integrate_positions(
            &bodies_warm[0], JOINT_TEST_DT);

        /* Cold run (reset impulses each frame) */
        forge_physics_rigid_body_apply_force(&bodies_cold[0],
            vec3_scale(gravity, bodies_cold[0].mass));
        forge_physics_rigid_body_integrate_velocities(
            &bodies_cold[0], JOINT_TEST_DT);

        joints_cold[0].j_point = vec3_create(0, 0, 0); /* clear warm-start */
        forge_physics_joint_prepare(joints_cold, 1, bodies_cold, 1,
                                    JOINT_TEST_DT, ws);
        /* No warm-start call */
        for (int it = 0; it < solver_iters; it++)
            forge_physics_joint_solve_velocities(
                joints_cold, ws, 1, bodies_cold, 1);
        forge_physics_joint_store_impulses(joints_cold, ws, 1);
        forge_physics_rigid_body_integrate_positions(
            &bodies_cold[0], JOINT_TEST_DT);
    }

    /* Measure anchor error for both */
    vec3 wa_warm = world_anchor(bodies_warm, 1, 0, joints_warm[0].local_anchor_a);
    vec3 tgt = vec3_create(0, 5, 0);
    float err_warm = vec3_length(vec3_sub(wa_warm, tgt));

    vec3 wa_cold = world_anchor(bodies_cold, 1, 0, joints_cold[0].local_anchor_a);
    float err_cold = vec3_length(vec3_sub(wa_cold, tgt));

    /* Warm-start error should be better (or equal) */
    ASSERT_TRUE(err_warm <= err_cold + JOINT_TEST_WARM_TOL);

    END_TEST();
}

static void test_JOINT_determinism(void)
{
    TEST("JOINT_determinism");

    /* Two identical simulations must produce identical results. */
    ForgePhysicsRigidBody bodies_a[2], bodies_b[2];
    bodies_a[0] = make_joint_body(vec3_create(0, 4, 0), JOINT_TEST_LIGHT_MASS);
    bodies_a[1] = make_joint_body(vec3_create(0, 2, 0), JOINT_TEST_DEFAULT_MASS);
    SDL_memcpy(bodies_b, bodies_a, sizeof(bodies_a));

    ForgePhysicsJoint joints_a[1], joints_b[1];
    joints_a[0] = forge_physics_joint_ball_socket(
        0, 1,
        vec3_create(0, -0.5f, 0),
        vec3_create(0,  0.5f, 0));
    SDL_memcpy(joints_b, joints_a, sizeof(joints_a));

    simulate_joints(bodies_a, 2, joints_a, 1, JOINT_TEST_SHORT_STEPS, JOINT_TEST_DT);
    simulate_joints(bodies_b, 2, joints_b, 1, JOINT_TEST_SHORT_STEPS, JOINT_TEST_DT);

    ASSERT_NEAR(bodies_a[0].position.x, bodies_b[0].position.x, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies_a[0].position.y, bodies_b[0].position.y, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies_a[0].position.z, bodies_b[0].position.z, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies_a[1].position.x, bodies_b[1].position.x, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies_a[1].position.y, bodies_b[1].position.y, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies_a[1].position.z, bodies_b[1].position.z, JOINT_TEST_EXACT_TOL);

    END_TEST();
}

static void test_JOINT_numerical_stability(void)
{
    TEST("JOINT_numerical_stability");

    /* Run a pendulum for 10000 steps — no NaN or Inf. */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(2, 4, 0), JOINT_TEST_PENDULUM_MASS);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_ball_socket(
        0, -1,
        vec3_create(0, 0.5f, 0),
        vec3_create(0, 5, 0));

    simulate_joints(bodies, 1, joints, 1, JOINT_TEST_STABILITY_STEPS,
                    JOINT_TEST_DT);

    ASSERT_TRUE(forge_isfinite(bodies[0].position.x));
    ASSERT_TRUE(forge_isfinite(bodies[0].position.y));
    ASSERT_TRUE(forge_isfinite(bodies[0].position.z));
    ASSERT_TRUE(forge_isfinite(bodies[0].velocity.x));
    ASSERT_TRUE(forge_isfinite(bodies[0].velocity.y));
    ASSERT_TRUE(forge_isfinite(bodies[0].velocity.z));
    ASSERT_TRUE(forge_isfinite(bodies[0].angular_velocity.x));
    ASSERT_TRUE(forge_isfinite(bodies[0].angular_velocity.y));
    ASSERT_TRUE(forge_isfinite(bodies[0].angular_velocity.z));

    /* Position should be bounded (not exploding) */
    ASSERT_TRUE(vec3_length(bodies[0].position) < JOINT_TEST_POS_BOUND);

    END_TEST();
}

static void test_JOINT_static_static_guard(void)
{
    TEST("JOINT_static_static_guard");

    /* Both bodies are world anchors (-1). Should not crash or produce NaN. */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(0, 0, 0), 0.0f); /* static */

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_ball_socket(
        -1, -1,
        vec3_create(0, 1, 0),
        vec3_create(0, 2, 0));

    /* Should not crash */
    ForgePhysicsJointSolverData ws[1];
    forge_physics_joint_prepare(joints, 1, bodies, 1, PHYSICS_DT, ws);
    forge_physics_joint_solve_velocities(joints, ws, 1, bodies, 1);

    /* No body was modified (both are world anchors) — just check no crash */
    ASSERT_TRUE(1);

    END_TEST();
}

static void test_JOINT_extreme_mass_ratio(void)
{
    TEST("JOINT_extreme_mass_ratio");

    /* 1000:1 mass ratio — light body connected to heavy body.
     * Should remain stable (no NaN, no explosion). */
    ForgePhysicsRigidBody bodies[2];
    bodies[0] = make_joint_body(vec3_create(0, 4, 0), JOINT_TEST_HEAVY_MASS);
    bodies[1] = make_joint_body(vec3_create(0, 2, 0), JOINT_TEST_LIGHT_MASS);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_ball_socket(
        0, 1,
        vec3_create(0, -0.5f, 0),
        vec3_create(0,  0.5f, 0));

    simulate_joints(bodies, 2, joints, 1, JOINT_TEST_EXTREME_STEPS, JOINT_TEST_DT);

    ASSERT_TRUE(forge_isfinite(bodies[0].position.y));
    ASSERT_TRUE(forge_isfinite(bodies[1].position.y));
    ASSERT_TRUE(forge_isfinite(bodies[0].velocity.y));
    ASSERT_TRUE(forge_isfinite(bodies[1].velocity.y));

    /* Bodies fall under gravity — check anchor coincidence, not absolute position.
     * With a 1000:1 mass ratio the joint should still hold both anchors together. */
    vec3 wa = world_anchor(bodies, 2, 0, joints[0].local_anchor_a);
    vec3 wb = world_anchor(bodies, 2, 1, joints[0].local_anchor_b);
    float dist = vec3_length(vec3_sub(wa, wb));
    ASSERT_TRUE(dist < JOINT_TEST_DRIFT_TOL);

    END_TEST();
}

static void test_JOINT_solve_convenience_wrapper(void)
{
    TEST("JOINT_solve_convenience_wrapper");

    /* Verify forge_physics_joint_solve() produces the same result as
     * calling the individual pipeline functions manually. */
    ForgePhysicsRigidBody bodies_manual[1];
    ForgePhysicsRigidBody bodies_conv[1];
    bodies_manual[0] = make_joint_body(vec3_create(2, 4, 0),
                                       JOINT_TEST_PENDULUM_MASS);
    bodies_conv[0] = make_joint_body(vec3_create(2, 4, 0),
                                     JOINT_TEST_PENDULUM_MASS);

    ForgePhysicsJoint joints_manual[1];
    ForgePhysicsJoint joints_conv[1];
    joints_manual[0] = forge_physics_joint_ball_socket(
        0, -1,
        vec3_create(0, 0.5f, 0),
        vec3_create(0, 5, 0));
    joints_conv[0] = forge_physics_joint_ball_socket(
        0, -1,
        vec3_create(0, 0.5f, 0),
        vec3_create(0, 5, 0));

    vec3 gravity = vec3_create(0, -JOINT_TEST_GRAVITY, 0);
    ForgePhysicsJointSolverData ws[1];

    for (int step = 0; step < JOINT_TEST_WARM_STEPS; step++) {
        /* Apply gravity to both */
        forge_physics_rigid_body_apply_force(&bodies_manual[0],
            vec3_scale(gravity, bodies_manual[0].mass));
        forge_physics_rigid_body_apply_force(&bodies_conv[0],
            vec3_scale(gravity, bodies_conv[0].mass));
        forge_physics_rigid_body_integrate_velocities(
            &bodies_manual[0], JOINT_TEST_DT);
        forge_physics_rigid_body_integrate_velocities(
            &bodies_conv[0], JOINT_TEST_DT);

        /* Manual pipeline */
        forge_physics_joint_prepare(joints_manual, 1, bodies_manual, 1,
                                    JOINT_TEST_DT, ws);
        forge_physics_joint_warm_start(joints_manual, ws, 1,
                                       bodies_manual, 1);
        for (int it = 0; it < JOINT_TEST_SOLVER_ITERS; it++)
            forge_physics_joint_solve_velocities(
                joints_manual, ws, 1, bodies_manual, 1);
        forge_physics_joint_store_impulses(joints_manual, ws, 1);
        forge_physics_joint_correct_positions(
            joints_manual, 1, bodies_manual, 1,
            FORGE_PHYSICS_JOINT_BAUMGARTE, FORGE_PHYSICS_JOINT_SLOP);

        /* Convenience wrapper */
        forge_physics_joint_solve(joints_conv, 1, bodies_conv, 1,
                                  JOINT_TEST_SOLVER_ITERS, JOINT_TEST_DT,
                                  true, ws);

        /* Integrate positions */
        forge_physics_rigid_body_integrate_positions(
            &bodies_manual[0], JOINT_TEST_DT);
        forge_physics_rigid_body_integrate_positions(
            &bodies_conv[0], JOINT_TEST_DT);
    }

    /* Results should be identical (both paths execute the same code) */
    ASSERT_NEAR(bodies_manual[0].position.x,
                bodies_conv[0].position.x, JOINT_TEST_CONV_TOL);
    ASSERT_NEAR(bodies_manual[0].position.y,
                bodies_conv[0].position.y, JOINT_TEST_CONV_TOL);
    ASSERT_NEAR(bodies_manual[0].position.z,
                bodies_conv[0].position.z, JOINT_TEST_CONV_TOL);

    END_TEST();
}

/* ── Tests for PR feedback fixes ──────────────────────────────────────────── */

static void test_JOINT_local_axis_b_stored(void)
{
    TEST("JOINT_local_axis_b_stored");

    /* Verify constructors store local_axis_b matching local_axis_a */
    vec3 axis_y = vec3_create(0, 1, 0);
    vec3 axis_x = vec3_create(1, 0, 0);
    vec3 zero   = vec3_create(0, 0, 0);

    ForgePhysicsJoint bs = forge_physics_joint_ball_socket(0, 1, zero, zero);
    ASSERT_NEAR(bs.local_axis_b.y, 1.0f, JOINT_TEST_EXACT_TOL);

    ForgePhysicsJoint h = forge_physics_joint_hinge(0, 1, zero, zero, axis_y);
    ASSERT_NEAR(h.local_axis_b.x, h.local_axis_a.x, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(h.local_axis_b.y, h.local_axis_a.y, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(h.local_axis_b.z, h.local_axis_a.z, JOINT_TEST_EXACT_TOL);

    ForgePhysicsJoint s = forge_physics_joint_slider(0, 1, zero, zero, axis_x);
    ASSERT_NEAR(s.local_axis_b.x, s.local_axis_a.x, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(s.local_axis_b.y, s.local_axis_a.y, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(s.local_axis_b.z, s.local_axis_a.z, JOINT_TEST_EXACT_TOL);

    END_TEST();
}

static void test_JOINT_invalid_dt_rejected(void)
{
    TEST("JOINT_invalid_dt_rejected");

    /* Prepare should be a no-op for invalid dt values. We check that
     * the solver data stays zeroed (no crash, no NaN). */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(0, 5, 0), JOINT_TEST_DEFAULT_MASS);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_ball_socket(
        0, -1, vec3_create(0, 0.5f, 0), vec3_create(0, 5.5f, 0));

    ForgePhysicsJointSolverData ws[1];

    /* dt = 0 */
    SDL_memset(ws, 0, sizeof(ws));
    forge_physics_joint_prepare(joints, 1, bodies, 1, 0.0f, ws);
    ASSERT_NEAR(ws[0].r_a.x, 0.0f, JOINT_TEST_EXACT_TOL);

    /* dt = -1 */
    SDL_memset(ws, 0, sizeof(ws));
    forge_physics_joint_prepare(joints, 1, bodies, 1, -1.0f, ws);
    ASSERT_NEAR(ws[0].r_a.x, 0.0f, JOINT_TEST_EXACT_TOL);

    /* dt = NaN (use SDL_sqrt(-1) to avoid MSVC compile-time error) */
    SDL_memset(ws, 0, sizeof(ws));
    float nan_val = (float)SDL_sqrt(-1.0);
    forge_physics_joint_prepare(joints, 1, bodies, 1, nan_val, ws);
    ASSERT_NEAR(ws[0].r_a.x, 0.0f, JOINT_TEST_EXACT_TOL);

    /* dt = +inf (use runtime zero to avoid MSVC compile-time error) */
    SDL_memset(ws, 0, sizeof(ws));
    volatile float zero = 0.0f;
    float inf_val = 1.0f / zero;
    forge_physics_joint_prepare(joints, 1, bodies, 1, inf_val, ws);
    ASSERT_NEAR(ws[0].r_a.x, 0.0f, JOINT_TEST_EXACT_TOL);

    /* Also test forge_physics_joint_solve with invalid dt */
    vec3 orig_pos = bodies[0].position;
    forge_physics_joint_solve(joints, 1, bodies, 1,
                              JOINT_TEST_SOLVER_ITERS, 0.0f, true, ws);
    /* Body should not have moved */
    ASSERT_NEAR(bodies[0].position.x, orig_pos.x, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies[0].position.y, orig_pos.y, JOINT_TEST_EXACT_TOL);

    END_TEST();
}

static void test_JOINT_invalid_body_index_skipped(void)
{
    TEST("JOINT_invalid_body_index_skipped");

    /* A joint with a stale body index (not -1, but out of range)
     * should be skipped entirely — no crash, no effect on bodies. */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_joint_body(vec3_create(0, 5, 0), JOINT_TEST_DEFAULT_MASS);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_ball_socket(
        0, 99,  /* body_b = 99, which is out of range */
        vec3_create(0, 0, 0), vec3_create(0, 0, 0));

    ForgePhysicsJointSolverData ws[1];
    SDL_memset(ws, 0, sizeof(ws));

    /* prepare should skip the joint, leaving ws zeroed */
    forge_physics_joint_prepare(joints, 1, bodies, 1, JOINT_TEST_DT, ws);
    ASSERT_NEAR(ws[0].r_a.x, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(ws[0].r_a.y, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(ws[0].r_b.x, 0.0f, JOINT_TEST_EXACT_TOL);

    /* correct_positions should also skip — body stays put */
    vec3 orig = bodies[0].position;
    forge_physics_joint_correct_positions(
        joints, 1, bodies, 1,
        FORGE_PHYSICS_JOINT_BAUMGARTE, FORGE_PHYSICS_JOINT_SLOP);
    ASSERT_NEAR(bodies[0].position.x, orig.x, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies[0].position.y, orig.y, JOINT_TEST_EXACT_TOL);

    END_TEST();
}

static void test_JOINT_static_body_no_angular_impulse(void)
{
    TEST("JOINT_static_body_no_angular_impulse");

    /* A static body (inv_mass = 0) connected to a dynamic body via hinge.
     * After warm-start and solve, the static body's angular velocity must
     * remain zero regardless of joint impulses. */
    ForgePhysicsRigidBody bodies[2];
    bodies[0] = make_joint_body(vec3_create(0, 5, 0), 0.0f);  /* static */
    bodies[1] = make_joint_body(vec3_create(1, 5, 0), JOINT_TEST_DEFAULT_MASS);

    /* Give body 1 angular velocity to generate joint impulses */
    bodies[1].angular_velocity = vec3_create(0, 0, JOINT_TEST_HINGE_ANG_VEL);

    ForgePhysicsJoint joints[1];
    joints[0] = forge_physics_joint_hinge(
        0, 1,
        vec3_create(0.5f, 0, 0),
        vec3_create(-0.5f, 0, 0),
        vec3_create(0, 1, 0));

    /* Manually set cached impulses to test warm-start guarding */
    joints[0].j_point = vec3_create(10, 10, 10);
    joints[0].j_angular = vec3_create(5, 5, 0);

    ForgePhysicsJointSolverData ws[1];
    forge_physics_joint_prepare(joints, 1, bodies, 2, JOINT_TEST_DT, ws);
    forge_physics_joint_warm_start(joints, ws, 1, bodies, 2);

    /* Static body must not have gained angular velocity from warm-start */
    ASSERT_NEAR(bodies[0].angular_velocity.x, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies[0].angular_velocity.y, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies[0].angular_velocity.z, 0.0f, JOINT_TEST_EXACT_TOL);

    /* Run a few solver iterations */
    for (int iter = 0; iter < JOINT_TEST_SOLVER_ITERS; iter++) {
        forge_physics_joint_solve_velocities(
            joints, ws, 1, bodies, 2);
    }

    /* Static body must still have zero angular velocity after solving */
    ASSERT_NEAR(bodies[0].angular_velocity.x, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies[0].angular_velocity.y, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(bodies[0].angular_velocity.z, 0.0f, JOINT_TEST_EXACT_TOL);

    END_TEST();
}

static void test_JOINT_reset_cached_impulses(void)
{
    TEST("JOINT_reset_cached_impulses");

    /* Verify the public reset helper zeroes all cached impulse fields */
    ForgePhysicsJoint joints[2];
    SDL_memset(joints, 0, sizeof(joints));
    joints[0].type = FORGE_PHYSICS_JOINT_BALL_SOCKET;
    joints[1].type = FORGE_PHYSICS_JOINT_SLIDER;

    /* Set non-zero impulses */
    joints[0].j_point   = vec3_create(1, 2, 3);
    joints[0].j_angular = vec3_create(4, 5, 6);
    joints[1].j_slide[0] = 7.0f;
    joints[1].j_slide[1] = 8.0f;
    joints[1].j_angular = vec3_create(9, 10, 11);

    forge_physics_joint_reset_cached_impulses(joints, 2);

    ASSERT_NEAR(joints[0].j_point.x,   0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(joints[0].j_point.y,   0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(joints[0].j_point.z,   0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(joints[0].j_angular.x, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(joints[0].j_angular.y, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(joints[0].j_angular.z, 0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(joints[1].j_slide[0],  0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(joints[1].j_slide[1],  0.0f, JOINT_TEST_EXACT_TOL);
    ASSERT_NEAR(joints[1].j_angular.x, 0.0f, JOINT_TEST_EXACT_TOL);

    /* Edge case: NULL and zero count should not crash */
    forge_physics_joint_reset_cached_impulses(NULL, 0);
    forge_physics_joint_reset_cached_impulses(joints, 0);
    forge_physics_joint_reset_cached_impulses(NULL, 5);

    END_TEST();
}

static void test_JOINT_cold_start_low_level_api(void)
{
    TEST("JOINT_cold_start_low_level_api");

    /* Verify that using the low-level API with the reset helper produces
     * the same result as the convenience wrapper with warm_start=false. */
    ForgePhysicsRigidBody bodies_ll[1], bodies_conv[1];
    bodies_ll[0] = make_joint_body(vec3_create(1, 5, 0), JOINT_TEST_DEFAULT_MASS);
    bodies_conv[0] = bodies_ll[0];

    ForgePhysicsJoint joints_ll[1], joints_conv[1];
    joints_ll[0] = forge_physics_joint_ball_socket(
        0, -1, vec3_create(0, 0.5f, 0), vec3_create(0, 5.5f, 0));
    joints_conv[0] = joints_ll[0];

    /* Set stale impulses to test that reset works */
    joints_ll[0].j_point   = vec3_create(100, 100, 100);
    joints_conv[0].j_point = vec3_create(100, 100, 100);

    ForgePhysicsJointSolverData ws_ll[1], ws_conv[1];

    /* Low-level path: reset → prepare → solve (no warm-start) */
    forge_physics_joint_reset_cached_impulses(joints_ll, 1);
    forge_physics_joint_prepare(joints_ll, 1, bodies_ll, 1,
                                JOINT_TEST_DT, ws_ll);
    for (int iter = 0; iter < JOINT_TEST_SOLVER_ITERS; iter++) {
        forge_physics_joint_solve_velocities(
            joints_ll, ws_ll, 1, bodies_ll, 1);
    }
    forge_physics_joint_store_impulses(joints_ll, ws_ll, 1);
    forge_physics_joint_correct_positions(
        joints_ll, 1, bodies_ll, 1,
        FORGE_PHYSICS_JOINT_BAUMGARTE, FORGE_PHYSICS_JOINT_SLOP);

    /* Convenience wrapper with warm_start=false */
    forge_physics_joint_solve(joints_conv, 1, bodies_conv, 1,
                              JOINT_TEST_SOLVER_ITERS, JOINT_TEST_DT,
                              false, ws_conv);

    /* Both paths should produce the same result */
    ASSERT_NEAR(bodies_ll[0].position.x,
                bodies_conv[0].position.x, JOINT_TEST_CONV_TOL);
    ASSERT_NEAR(bodies_ll[0].position.y,
                bodies_conv[0].position.y, JOINT_TEST_CONV_TOL);
    ASSERT_NEAR(bodies_ll[0].velocity.x,
                bodies_conv[0].velocity.x, JOINT_TEST_CONV_TOL);
    ASSERT_NEAR(bodies_ll[0].velocity.y,
                bodies_conv[0].velocity.y, JOINT_TEST_CONV_TOL);

    END_TEST();
}

/* ── Test runner ──────────────────────────────────────────────────────────── */

void run_joint_tests(void)
{
    SDL_Log("\n--- Joint Constraint Solver (Lesson 13) ---");
    test_JOINT_ball_socket_creation();
    test_JOINT_ball_socket_holds_anchor();
    test_JOINT_ball_socket_world_anchor();
    test_JOINT_hinge_axis_constraint();
    test_JOINT_hinge_free_rotation();
    test_JOINT_slider_axis_translation();
    test_JOINT_slider_rotation_locked();
    test_JOINT_slider_free_axis();
    test_JOINT_warm_start_convergence();
    test_JOINT_determinism();
    test_JOINT_numerical_stability();
    test_JOINT_static_static_guard();
    test_JOINT_extreme_mass_ratio();
    test_JOINT_solve_convenience_wrapper();
    test_JOINT_local_axis_b_stored();
    test_JOINT_invalid_dt_rejected();
    test_JOINT_invalid_body_index_skipped();
    test_JOINT_static_body_no_angular_impulse();
    test_JOINT_reset_cached_impulses();
    test_JOINT_cold_start_low_level_api();
}
