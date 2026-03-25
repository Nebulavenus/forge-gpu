/*
 * Physics World / Island / Sleep Tests
 *
 * Tests for ForgePhysicsWorld and related functions added in Physics Lesson 15.
 * Validates world lifecycle, body/joint addition, gravity integration,
 * ground collision, sleep/wake, island detection, determinism, and stats.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Test constants ───────────────────────────────────────────────────────── */

#define WORLD_TEST_BOX_HALF        0.5f    /* box half-extent on all axes (m) */
#define WORLD_TEST_BOX_MASS        2.0f    /* standard body mass (kg) */
#define WORLD_TEST_DAMPING         0.99f   /* linear damping coefficient */
#define WORLD_TEST_ANG_DAMPING     0.98f   /* angular damping coefficient */
#define WORLD_TEST_RESTIT          0.2f    /* coefficient of restitution */
#define WORLD_TEST_HIGH_Y          5.0f    /* initial height for gravity tests (m) */
#define WORLD_TEST_REST_Y          0.501f  /* just-above-ground resting height (m) */
#define WORLD_TEST_SETTLE_STEPS    300     /* steps to settle + sleep (5 s at 60 Hz) */
#define WORLD_TEST_EXTRA_STEPS     10      /* extra steps after sleeping */
#define WORLD_TEST_CONTACT_STEPS    5      /* steps to establish body-body contact */
#define WORLD_TEST_GROUND_STEPS    60      /* steps for ground-collision test */
#define WORLD_TEST_STATS_STEPS     120     /* steps for stats test */
#define WORLD_TEST_DET_STEPS       60      /* steps for determinism test */
#define WORLD_TEST_GRAVITY_Y       (-9.81f)
#define WORLD_TEST_FIXED_DT        (1.0f / 60.0f)
#define WORLD_TEST_SOLVER_ITERS    40
#define WORLD_TEST_VEL_REST_EPS    0.05f   /* max |vy| for resting body (m/s) */
#define WORLD_TEST_POS_REST_EPS    0.05f   /* max position drift for resting body (m) */
#define WORLD_TEST_FAR_LEFT_X      (-10.0f) /* far-left body x for island split */
#define WORLD_TEST_FAR_RIGHT_X     10.0f   /* far-right body x for island split */
#define WORLD_TEST_STACK_LOWER_Y   0.5f    /* lower box centre y for stacking (m) */
#define WORLD_TEST_STACK_UPPER_Y   1.5f    /* upper box centre y — flush with lower top */
#define WORLD_TEST_WAKE_FORCE_Y    200.0f  /* upward force to wake a sleeping body (N) */
#define WORLD_TEST_WAKE_IMPULSE_Y  5.0f    /* upward impulse to wake a sleeping body (N·s) */

/* ── Helper ───────────────────────────────────────────────────────────────── */

/* Return a world config tuned for fast settling in tests. */
static ForgePhysicsWorldConfig world_test_config(void)
{
    ForgePhysicsWorldConfig cfg = forge_physics_world_config_default();
    cfg.solver_iterations = WORLD_TEST_SOLVER_ITERS;
    return cfg;
}

/* Create a standard dynamic box body at pos. */
static ForgePhysicsRigidBody world_make_body(vec3 pos)
{
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        pos,
        WORLD_TEST_BOX_MASS,
        WORLD_TEST_DAMPING,
        WORLD_TEST_ANG_DAMPING,
        WORLD_TEST_RESTIT);
    vec3 he = vec3_create(WORLD_TEST_BOX_HALF, WORLD_TEST_BOX_HALF, WORLD_TEST_BOX_HALF);
    forge_physics_rigid_body_set_inertia_box(&rb, he);
    return rb;
}

/* Create a unit-cube collision shape. */
static ForgePhysicsCollisionShape world_make_shape(void)
{
    return forge_physics_shape_box(
        vec3_create(WORLD_TEST_BOX_HALF, WORLD_TEST_BOX_HALF, WORLD_TEST_BOX_HALF));
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_WORLD_config_default(void)
{
    TEST("WORLD_config_default");

    ForgePhysicsWorldConfig cfg = forge_physics_world_config_default();

    ASSERT_NEAR(cfg.gravity.y,      WORLD_TEST_GRAVITY_Y,  EPSILON);
    ASSERT_NEAR(cfg.fixed_dt,       WORLD_TEST_FIXED_DT,   EPSILON);
    ASSERT_TRUE(cfg.solver_iterations == FORGE_PHYSICS_WORLD_DEFAULT_ITERATIONS);
    ASSERT_TRUE(cfg.warm_start      == true);
    ASSERT_TRUE(cfg.enable_sleeping == true);

    END_TEST();
}

static void test_WORLD_init_destroy(void)
{
    TEST("WORLD_init_destroy");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ASSERT_TRUE(forge_physics_world_body_count(&world) == 0);

    forge_physics_world_destroy(&world);
    /* Verify the struct is zeroed — body count must be 0 after destroy. */
    ASSERT_TRUE(forge_physics_world_body_count(&world) == 0);

    END_TEST();
}

static void test_WORLD_add_body(void)
{
    TEST("WORLD_add_body");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ForgePhysicsRigidBody rb;
    ForgePhysicsCollisionShape sh = world_make_shape();

    rb = world_make_body(vec3_create(0.0f, 1.0f, 0.0f));
    int idx0 = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx0 >= 0);

    rb = world_make_body(vec3_create(2.0f, 1.0f, 0.0f));
    int idx1 = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx1 >= 0);

    rb = world_make_body(vec3_create(4.0f, 1.0f, 0.0f));
    int idx2 = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx2 >= 0);

    ASSERT_TRUE(forge_physics_world_body_count(&world) == 3);
    ASSERT_TRUE(idx0 == 0);
    ASSERT_TRUE(idx1 == 1);
    ASSERT_TRUE(idx2 == 2);

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_step_gravity(void)
{
    TEST("WORLD_step_gravity");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ForgePhysicsRigidBody rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_HIGH_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    int idx = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx >= 0);

    forge_physics_world_step(&world);

    /* Body must have moved downward after one gravity step. */
    ASSERT_TRUE(world.bodies[idx].position.y < WORLD_TEST_HIGH_Y);
    /* prev_position must record the pre-integration position. */
    ASSERT_NEAR(world.bodies[idx].prev_position.y, WORLD_TEST_HIGH_Y, EPSILON);

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_step_ground_collision(void)
{
    TEST("WORLD_step_ground_collision");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    /* Place a box at y=0.5 so its bottom face just touches the ground plane. */
    ForgePhysicsRigidBody rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_BOX_HALF, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    int idx = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx >= 0);

    for (int i = 0; i < WORLD_TEST_GROUND_STEPS; i++) {
        forge_physics_world_step(&world);
    }

    /* Box should remain at approximately y=0.5 — it must not fall through. */
    ASSERT_NEAR(world.bodies[idx].position.y, WORLD_TEST_BOX_HALF, WORLD_TEST_POS_REST_EPS);
    /* Vertical velocity must be near zero for a body at rest on the ground. */
    ASSERT_NEAR(world.bodies[idx].velocity.y, 0.0f, WORLD_TEST_VEL_REST_EPS);

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_sleep_stationary(void)
{
    TEST("WORLD_sleep_stationary");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ForgePhysicsRigidBody rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_REST_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    int idx = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx >= 0);

    for (int i = 0; i < WORLD_TEST_SETTLE_STEPS; i++) {
        forge_physics_world_step(&world);
    }

    /* After settling, the body must have transitioned to sleep. */
    ASSERT_TRUE(forge_physics_world_is_sleeping(&world, idx));

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_wake_on_force(void)
{
    TEST("WORLD_wake_on_force");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ForgePhysicsRigidBody rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_REST_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    int idx = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx >= 0);

    /* Let the body settle and sleep. */
    for (int i = 0; i < WORLD_TEST_SETTLE_STEPS; i++) {
        forge_physics_world_step(&world);
    }
    ASSERT_TRUE(forge_physics_world_is_sleeping(&world, idx));

    /* Apply a large upward force and step once — body must wake. */
    forge_physics_world_apply_force(
        &world, idx, vec3_create(0.0f, WORLD_TEST_WAKE_FORCE_Y, 0.0f));
    forge_physics_world_step(&world);

    ASSERT_TRUE(!forge_physics_world_is_sleeping(&world, idx));

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_wake_on_impulse(void)
{
    TEST("WORLD_wake_on_impulse");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ForgePhysicsRigidBody rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_REST_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    int idx = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx >= 0);

    /* Let the body settle and sleep. */
    for (int i = 0; i < WORLD_TEST_SETTLE_STEPS; i++) {
        forge_physics_world_step(&world);
    }
    ASSERT_TRUE(forge_physics_world_is_sleeping(&world, idx));

    /* Apply an upward impulse and step once — body must wake. */
    forge_physics_world_apply_impulse(
        &world, idx, vec3_create(0.0f, WORLD_TEST_WAKE_IMPULSE_Y, 0.0f));
    forge_physics_world_step(&world);

    ASSERT_TRUE(!forge_physics_world_is_sleeping(&world, idx));

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_island_two_groups(void)
{
    TEST("WORLD_island_two_groups");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    /* Two boxes placed far apart so they cannot be in contact. */
    ForgePhysicsRigidBody rb_left = world_make_body(
        vec3_create(WORLD_TEST_FAR_LEFT_X, WORLD_TEST_HIGH_Y, 0.0f));
    ForgePhysicsRigidBody rb_right = world_make_body(
        vec3_create(WORLD_TEST_FAR_RIGHT_X, WORLD_TEST_HIGH_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();

    int idx_left  = forge_physics_world_add_body(&world, &rb_left,  &sh);
    ASSERT_TRUE(idx_left >= 0);
    int idx_right = forge_physics_world_add_body(&world, &rb_right, &sh);
    ASSERT_TRUE(idx_right >= 0);

    forge_physics_world_step(&world);

    int island_left  = forge_physics_world_island_id(&world, idx_left);
    int island_right = forge_physics_world_island_id(&world, idx_right);

    /* Isolated bodies must belong to distinct islands. */
    ASSERT_TRUE(island_left  != FORGE_PHYSICS_ISLAND_NONE);
    ASSERT_TRUE(island_right != FORGE_PHYSICS_ISLAND_NONE);
    ASSERT_TRUE(island_left  != island_right);

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_island_merge_on_contact(void)
{
    TEST("WORLD_island_merge_on_contact");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    /* Two boxes stacked so they are touching (lower at 0.5, upper at 1.501). */
    ForgePhysicsRigidBody rb_lower = world_make_body(
        vec3_create(0.0f, WORLD_TEST_STACK_LOWER_Y, 0.0f));
    ForgePhysicsRigidBody rb_upper = world_make_body(
        vec3_create(0.0f, WORLD_TEST_STACK_UPPER_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();

    int idx_lower = forge_physics_world_add_body(&world, &rb_lower, &sh);
    ASSERT_TRUE(idx_lower >= 0);
    int idx_upper = forge_physics_world_add_body(&world, &rb_upper, &sh);
    ASSERT_TRUE(idx_upper >= 0);

    /* Step a few times so the upper box falls onto the lower and contacts form. */
    for (int i = 0; i < WORLD_TEST_CONTACT_STEPS; i++) {
        forge_physics_world_step(&world);
    }

    int island_lower = forge_physics_world_island_id(&world, idx_lower);
    int island_upper = forge_physics_world_island_id(&world, idx_upper);

    /* Contacting bodies must be merged into the same island. */
    ASSERT_TRUE(island_lower != FORGE_PHYSICS_ISLAND_NONE);
    ASSERT_TRUE(island_lower == island_upper);

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_sleeping_skips_integrate(void)
{
    TEST("WORLD_sleeping_skips_integrate");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ForgePhysicsRigidBody rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_REST_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    int idx = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx >= 0);

    /* Settle until sleeping. */
    for (int i = 0; i < WORLD_TEST_SETTLE_STEPS; i++) {
        forge_physics_world_step(&world);
    }
    ASSERT_TRUE(forge_physics_world_is_sleeping(&world, idx));

    /* Record position after sleep is confirmed. */
    float y_before = world.bodies[idx].position.y;
    float x_before = world.bodies[idx].position.x;
    float z_before = world.bodies[idx].position.z;

    /* Step several more times — position must not change. */
    for (int i = 0; i < WORLD_TEST_EXTRA_STEPS; i++) {
        forge_physics_world_step(&world);
    }

    ASSERT_NEAR(world.bodies[idx].position.x, x_before, EPSILON);
    ASSERT_NEAR(world.bodies[idx].position.y, y_before, EPSILON);
    ASSERT_NEAR(world.bodies[idx].position.z, z_before, EPSILON);

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_stats(void)
{
    TEST("WORLD_stats");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ForgePhysicsCollisionShape sh = world_make_shape();
    ForgePhysicsRigidBody rb;

    /* Add 3 dynamic boxes at ground-rest height. */
    rb = world_make_body(vec3_create(-2.0f, WORLD_TEST_REST_Y, 0.0f));
    ASSERT_TRUE(forge_physics_world_add_body(&world, &rb, &sh) >= 0);
    rb = world_make_body(vec3_create( 0.0f, WORLD_TEST_REST_Y, 0.0f));
    ASSERT_TRUE(forge_physics_world_add_body(&world, &rb, &sh) >= 0);
    rb = world_make_body(vec3_create( 2.0f, WORLD_TEST_REST_Y, 0.0f));
    ASSERT_TRUE(forge_physics_world_add_body(&world, &rb, &sh) >= 0);

    for (int i = 0; i < WORLD_TEST_STATS_STEPS; i++) {
        forge_physics_world_step(&world);
    }

    /* All 3 bodies must be accounted for across active + sleeping counts. */
    ASSERT_TRUE(world.active_body_count + world.sleeping_body_count == 3);
    /* No static bodies were added. */
    ASSERT_TRUE(world.static_body_count == 0);

    forge_physics_world_destroy(&world);
    END_TEST();
}

static void test_WORLD_determinism(void)
{
    TEST("WORLD_determinism");

    ForgePhysicsWorldConfig cfg = forge_physics_world_config_default();
    ForgePhysicsCollisionShape sh = world_make_shape();

    /* Build two identical worlds and run them in lockstep. */
    ForgePhysicsWorld world_a;
    ForgePhysicsWorld world_b;
    forge_physics_world_init(&world_a, cfg);
    forge_physics_world_init(&world_b, cfg);

    /* Positions chosen to produce non-trivial motion and contacts. */
    vec3 positions[3] = {
        vec3_create(-1.0f, WORLD_TEST_HIGH_Y, 0.0f),
        vec3_create( 0.0f, WORLD_TEST_HIGH_Y + 2.0f, 0.0f),
        vec3_create( 1.0f, WORLD_TEST_HIGH_Y + 4.0f, 0.0f),
    };

    for (int i = 0; i < 3; i++) {
        ForgePhysicsRigidBody rb = world_make_body(positions[i]);
        int ia = forge_physics_world_add_body(&world_a, &rb, &sh);
        int ib = forge_physics_world_add_body(&world_b, &rb, &sh);
        ASSERT_TRUE(ia >= 0);
        ASSERT_TRUE(ib >= 0);
    }

    for (int step = 0; step < WORLD_TEST_DET_STEPS; step++) {
        forge_physics_world_step(&world_a);
        forge_physics_world_step(&world_b);
    }

    /* Every body position must match within epsilon across both worlds. */
    int nb = forge_physics_world_body_count(&world_a);
    ASSERT_TRUE(nb == forge_physics_world_body_count(&world_b));
    for (int i = 0; i < nb; i++) {
        ASSERT_NEAR(world_a.bodies[i].position.x, world_b.bodies[i].position.x, EPSILON);
        ASSERT_NEAR(world_a.bodies[i].position.y, world_b.bodies[i].position.y, EPSILON);
        ASSERT_NEAR(world_a.bodies[i].position.z, world_b.bodies[i].position.z, EPSILON);
    }

    forge_physics_world_destroy(&world_a);
    forge_physics_world_destroy(&world_b);
    END_TEST();
}

static void test_WORLD_sleep_disabled(void)
{
    TEST("WORLD_sleep_disabled");

    ForgePhysicsWorldConfig cfg = forge_physics_world_config_default();
    cfg.enable_sleeping = false;

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, cfg);

    ForgePhysicsRigidBody rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_REST_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    int idx = forge_physics_world_add_body(&world, &rb, &sh);
    ASSERT_TRUE(idx >= 0);

    /* Run long enough that sleeping would normally trigger. */
    for (int i = 0; i < WORLD_TEST_SETTLE_STEPS; i++) {
        forge_physics_world_step(&world);
    }

    /* With sleeping disabled the body must never sleep. */
    ASSERT_TRUE(!forge_physics_world_is_sleeping(&world, idx));

    forge_physics_world_destroy(&world);
    END_TEST();
}

/* ── Joint wake propagation ─────────────────────────────────────────────── */

#define WORLD_TEST_JOINT_SETTLE_STEPS  300  /* steps for jointed bodies to settle */
#define WORLD_TEST_JOINT_WAKE_FORCE_Y  500.0f /* force to wake one jointed body */

static void test_WORLD_joint_wake_propagation(void)
{
    TEST("WORLD_joint_wake_propagation");

    ForgePhysicsWorld world;
    ForgePhysicsWorldConfig cfg = world_test_config();
    forge_physics_world_init(&world, cfg);

    /* Two boxes side by side connected by a ball-socket joint */
    ForgePhysicsRigidBody rb_a = world_make_body(
        vec3_create(-1.0f, WORLD_TEST_REST_Y, 0.0f));
    ForgePhysicsRigidBody rb_b = world_make_body(
        vec3_create( 1.0f, WORLD_TEST_REST_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    int idx_a = forge_physics_world_add_body(&world, &rb_a, &sh);
    ASSERT_TRUE(idx_a >= 0);
    int idx_b = forge_physics_world_add_body(&world, &rb_b, &sh);
    ASSERT_TRUE(idx_b >= 0);

    /* Joint connecting them — anchors map to the same world-space point
     * (midpoint at x=0) so the test isolates wake propagation without
     * mixing in joint-error correction forces. */
    ForgePhysicsJoint j = forge_physics_joint_ball_socket(
        idx_a, idx_b,
        vec3_create(WORLD_TEST_BOX_HALF * 2, 0.0f, 0.0f),
        vec3_create(-WORLD_TEST_BOX_HALF * 2, 0.0f, 0.0f));
    forge_physics_world_add_joint(&world, &j);

    /* Let both settle and sleep */
    for (int i = 0; i < WORLD_TEST_JOINT_SETTLE_STEPS; i++) {
        forge_physics_world_step(&world);
    }
    ASSERT_TRUE(forge_physics_world_is_sleeping(&world, idx_a));
    ASSERT_TRUE(forge_physics_world_is_sleeping(&world, idx_b));

    /* Wake only body A — body B should also wake via joint propagation */
    forge_physics_world_apply_force(
        &world, idx_a, vec3_create(0.0f, WORLD_TEST_JOINT_WAKE_FORCE_Y, 0.0f));

    /* apply_force calls wake_body which propagates through joints */
    ASSERT_TRUE(!forge_physics_world_is_sleeping(&world, idx_a));
    ASSERT_TRUE(!forge_physics_world_is_sleeping(&world, idx_b));

    forge_physics_world_destroy(&world);
    END_TEST();
}

/* ── Add joint returns -1 on NULL ────────────────────────────────────────── */

static void test_WORLD_add_joint_null(void)
{
    TEST("WORLD_add_joint_null");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    /* NULL joint pointer should return -1 */
    int idx = forge_physics_world_add_joint(&world, NULL);
    ASSERT_TRUE(idx == -1);

    forge_physics_world_destroy(&world);
    END_TEST();
}

/* ── Init on zeroed struct is safe ───────────────────────────────────────── */

static void test_WORLD_double_destroy(void)
{
    TEST("WORLD_double_destroy");

    /* Calling destroy twice should not crash (all arrays NULL after first) */
    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ForgePhysicsRigidBody rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_HIGH_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    ASSERT_TRUE(forge_physics_world_add_body(&world, &rb, &sh) >= 0);

    forge_physics_world_destroy(&world);
    forge_physics_world_destroy(&world); /* second destroy — must not crash */

    ASSERT_TRUE(forge_physics_world_body_count(&world) == 0);

    END_TEST();
}

static void test_WORLD_reinit_no_leak(void)
{
    TEST("WORLD_reinit_no_leak");

    /* Callers must destroy before re-initializing to avoid leaks.
     * This test verifies the destroy → init → use cycle works. */
    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    ForgePhysicsRigidBody rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_HIGH_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    ASSERT_TRUE(forge_physics_world_add_body(&world, &rb, &sh) >= 0);
    ASSERT_TRUE(forge_physics_world_body_count(&world) == 1);

    /* Destroy then re-init — must not crash or leak */
    forge_physics_world_destroy(&world);
    forge_physics_world_init(&world, world_test_config());
    ASSERT_TRUE(forge_physics_world_body_count(&world) == 0);

    /* World is usable after re-init */
    rb = world_make_body(vec3_create(1.0f, 2.0f, 0.0f));
    ASSERT_TRUE(forge_physics_world_add_body(&world, &rb, &sh) >= 0);
    ASSERT_TRUE(forge_physics_world_body_count(&world) == 1);

    forge_physics_world_destroy(&world);

    END_TEST();
}

/* A sleeping body must wake when a falling body collides with it. */
static void test_WORLD_wake_on_contact(void)
{
    TEST("WORLD_wake_on_contact");

    ForgePhysicsWorld world;
    forge_physics_world_init(&world, world_test_config());

    /* Create a target body resting just above the ground and let it sleep */
    ForgePhysicsRigidBody target_rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_REST_Y, 0.0f));
    ForgePhysicsCollisionShape sh = world_make_shape();
    int idx_target = forge_physics_world_add_body(&world, &target_rb, &sh);
    ASSERT_TRUE(idx_target >= 0);

    for (int i = 0; i < WORLD_TEST_SETTLE_STEPS; i++) {
        forge_physics_world_step(&world);
    }
    ASSERT_TRUE(forge_physics_world_is_sleeping(&world, idx_target));

    /* Drop a dynamic body from above — it will collide and wake the target */
    ForgePhysicsRigidBody falling_rb = world_make_body(
        vec3_create(0.0f, WORLD_TEST_REST_Y + 2.0f, 0.0f));
    int idx_falling = forge_physics_world_add_body(&world, &falling_rb, &sh);
    ASSERT_TRUE(idx_falling >= 0);

    /* Step until collision (bounded to prevent infinite loop) */
    for (int i = 0; i < WORLD_TEST_SETTLE_STEPS; i++) {
        forge_physics_world_step(&world);
        if (!forge_physics_world_is_sleeping(&world, idx_target)) break;
    }

    ASSERT_TRUE(!forge_physics_world_is_sleeping(&world, idx_target));

    forge_physics_world_destroy(&world);

    END_TEST();
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

void run_world_tests(void)
{
    SDL_Log("--- World / Island / Sleep Tests ---");
    test_WORLD_config_default();
    test_WORLD_init_destroy();
    test_WORLD_add_body();
    test_WORLD_step_gravity();
    test_WORLD_step_ground_collision();
    test_WORLD_sleep_stationary();
    test_WORLD_wake_on_force();
    test_WORLD_wake_on_impulse();
    test_WORLD_wake_on_contact();
    test_WORLD_island_two_groups();
    test_WORLD_island_merge_on_contact();
    test_WORLD_sleeping_skips_integrate();
    test_WORLD_stats();
    test_WORLD_determinism();
    test_WORLD_sleep_disabled();
    test_WORLD_joint_wake_propagation();
    test_WORLD_add_joint_null();
    test_WORLD_double_destroy();
    test_WORLD_reinit_no_leak();
}
