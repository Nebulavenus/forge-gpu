/*
 * Physics Library Tests
 *
 * Automated tests for common/physics/forge_physics.h
 * Verifies correctness of particle creation, force application,
 * integration, collision detection, determinism, and energy stability.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

int test_count = 0;
int pass_count = 0;
int fail_count = 0;

/* Gravity */
#define GRAVITY_Y         (-9.81f)

/* Common particle parameters */
#define DEFAULT_RADIUS    0.5f
#define DEFAULT_RESTIT    1.0f

/* Particle create — normal test */
#define PC_POS_X          1.0f
#define PC_POS_Y          2.0f
#define PC_POS_Z          3.0f
#define PC_MASS           2.0f
#define PC_DAMPING        0.1f
#define PC_RESTIT         0.8f
#define PC_INV_MASS       0.5f   /* 1 / PC_MASS */

/* Particle create — static test */
#define PC_STATIC_POS_X   5.0f

/* Particle create — clamping test */
#define PC_OVER_DAMPING   5.0f
#define PC_NEG_RESTIT     (-2.0f)

/* Particle create — all-fields test */
#define PC_ALL_RADIUS     0.3f

/* Apply gravity — normal test */
#define AG_HEIGHT         10.0f
#define AG_MASS           2.0f
#define AG_EXPECTED_FY    (-19.62f)  /* AG_MASS * GRAVITY_Y */

/* Apply gravity — custom direction test */
#define AG_CUSTOM_MASS    3.0f
#define AG_CUSTOM_GX      5.0f
#define AG_CUSTOM_GZ      (-2.0f)
#define AG_CUSTOM_FX      15.0f   /* AG_CUSTOM_MASS * AG_CUSTOM_GX */
#define AG_CUSTOM_FZ      (-6.0f) /* AG_CUSTOM_MASS * AG_CUSTOM_GZ */

/* Apply drag tests */
#define DRAG_VELOCITY     10.0f
#define DRAG_COEFF        0.5f
#define DRAG_EXPECTED_FX  (-5.0f) /* -DRAG_COEFF * DRAG_VELOCITY */

/* Integration — gravity 1s test */
#define INT_HEIGHT        100.0f
#define INT_MASS          2.0f
#define INT_TIME_1SEC     1.0f
#define INT_EXPECTED_VY   (-9.81f)   /* GRAVITY_Y * INT_TIME_1SEC */
#define INT_EXPECTED_PY   90.19f     /* INT_HEIGHT + INT_EXPECTED_VY * INT_TIME_1SEC */
#define INT_TOLERANCE     0.01f

/* Integration — zero dt test */
#define INT_ZERO_POS_X    5.0f
#define INT_ZERO_POS_Y    10.0f

/* Integration — static test */
#define INT_STATIC_POS_X  5.0f

/* Integration — velocity clamping test */
#define INT_HUGE_FORCE    1e8f

/* Collision — no-collision test */
#define COL_ABOVE_HEIGHT  5.0f
#define COL_DOWN_VEL      (-1.0f)

/* Collision — penetrating test */
#define COL_PEN_HEIGHT    0.1f
#define COL_PEN_VEL       (-5.0f)

/* Collision — restitution test */
#define COL_REST_RESTIT   0.5f
#define COL_REST_VEL      (-10.0f)
#define COL_REST_EXPECTED 5.0f    /* |COL_REST_VEL| * COL_REST_RESTIT */
#define COL_REST_TOL      0.1f

/* Collision — static particle test */
#define COL_STATIC_DEPTH  (-1.0f)
#define COL_STATIC_VEL    (-5.0f)

/* Negative drag coefficient test */
#define NEG_DRAG_COEFF    (-5.0f)
#define NEG_DRAG_VEL      10.0f

/* Zero-length plane normal test */
#define ZERO_NORMAL_HEIGHT 0.1f

/* Static particle collision test (updated — should NOT collide) */
#define COL_STATIC_INV_MASS_HEIGHT  0.1f

/* Determinism test */
#define DET_MASS          2.0f
#define DET_DAMPING       0.05f
#define DET_RESTIT        0.8f
#define DET_DRAG          0.01f
#define DET_ITERATIONS    1000

/* Energy stability tests */
#define NRG_DAMPING       0.01f
#define NRG_RESTIT        0.9f
#define NRG_DRAG          0.01f
#define NRG_ITERATIONS    10000
#define NRG_POS_BOUND     1e6f
#define NRG_HEADROOM      1.5f

/* Test macros (TEST, ASSERT_NEAR, ASSERT_TRUE, END_TEST) and
 * run_rbc_tests() prototype are in test_physics_common.h */

/* ══════════════════════════════════════════════════════════════════════════
 * 1. forge_physics_particle_create
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_particle_create_normal(void)
{
    TEST("particle_create — normal mass particle");
    vec3 pos = vec3_create(PC_POS_X, PC_POS_Y, PC_POS_Z);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, PC_MASS, PC_DAMPING, PC_RESTIT, DEFAULT_RADIUS);

    ASSERT_NEAR(p.position.x, PC_POS_X, EPSILON);
    ASSERT_NEAR(p.position.y, PC_POS_Y, EPSILON);
    ASSERT_NEAR(p.position.z, PC_POS_Z, EPSILON);
    ASSERT_NEAR(p.mass, PC_MASS, EPSILON);
    ASSERT_NEAR(p.inv_mass, PC_INV_MASS, EPSILON);
    ASSERT_NEAR(p.damping, PC_DAMPING, EPSILON);
    ASSERT_NEAR(p.restitution, PC_RESTIT, EPSILON);
    ASSERT_NEAR(p.radius, DEFAULT_RADIUS, EPSILON);
    ASSERT_NEAR(p.velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(p.velocity.y, 0.0f, EPSILON);
    ASSERT_NEAR(p.velocity.z, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_particle_create_static(void)
{
    TEST("particle_create — zero mass (static particle)");
    vec3 pos = vec3_create(PC_STATIC_POS_X, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 0.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    ASSERT_NEAR(p.mass, 0.0f, EPSILON);
    ASSERT_NEAR(p.inv_mass, 0.0f, EPSILON);
    END_TEST();
}

static void test_particle_create_clamping(void)
{
    TEST("particle_create — clamps damping and restitution");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);

    /* Damping above 1 should be clamped to 1 */
    ForgePhysicsParticle p1 = forge_physics_particle_create(
        pos, 1.0f, PC_OVER_DAMPING, DRAG_COEFF, DEFAULT_RADIUS);
    ASSERT_TRUE(p1.damping <= 1.0f);

    /* Negative restitution should be clamped to 0 */
    ForgePhysicsParticle p2 = forge_physics_particle_create(
        pos, 1.0f, DRAG_COEFF, PC_NEG_RESTIT, DEFAULT_RADIUS);
    ASSERT_TRUE(p2.restitution >= 0.0f);
    END_TEST();
}

static void test_particle_create_all_fields_initialized(void)
{
    TEST("particle_create — all fields have explicit values");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, DRAG_COEFF, DRAG_COEFF, PC_ALL_RADIUS);

    /* prev_position should equal position at creation */
    ASSERT_NEAR(p.prev_position.x, p.position.x, EPSILON);
    ASSERT_NEAR(p.prev_position.y, p.position.y, EPSILON);
    ASSERT_NEAR(p.prev_position.z, p.position.z, EPSILON);

    /* force accumulator should be zero */
    ASSERT_NEAR(p.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.z, 0.0f, EPSILON);

    /* radius should be set */
    ASSERT_NEAR(p.radius, PC_ALL_RADIUS, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 2. forge_physics_apply_gravity
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_apply_gravity_normal(void)
{
    TEST("apply_gravity — adds mass * gravity to force accumulator");
    vec3 pos = vec3_create(0.0f, AG_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, AG_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
    forge_physics_apply_gravity(&p, gravity);

    /* F = m * g = AG_MASS * GRAVITY_Y */
    ASSERT_NEAR(p.force_accum.y, AG_EXPECTED_FY, EPSILON);
    ASSERT_NEAR(p.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_apply_gravity_static(void)
{
    TEST("apply_gravity — static particle unaffected");
    vec3 pos = vec3_create(0.0f, AG_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 0.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
    forge_physics_apply_gravity(&p, gravity);

    ASSERT_NEAR(p.force_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_apply_gravity_custom_direction(void)
{
    TEST("apply_gravity — arbitrary direction");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, AG_CUSTOM_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    /* Sideways gravity */
    vec3 gravity = vec3_create(AG_CUSTOM_GX, 0.0f, AG_CUSTOM_GZ);
    forge_physics_apply_gravity(&p, gravity);

    ASSERT_NEAR(p.force_accum.x, AG_CUSTOM_FX, EPSILON);
    ASSERT_NEAR(p.force_accum.z, AG_CUSTOM_FZ, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. forge_physics_apply_drag
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_apply_drag_normal(void)
{
    TEST("apply_drag — opposes velocity");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(DRAG_VELOCITY, 0.0f, 0.0f);

    forge_physics_apply_drag(&p, DRAG_COEFF);

    /* F_drag = -DRAG_COEFF * DRAG_VELOCITY */
    ASSERT_NEAR(p.force_accum.x, DRAG_EXPECTED_FX, EPSILON);
    END_TEST();
}

static void test_apply_drag_static(void)
{
    TEST("apply_drag — static particle unaffected");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 0.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(DRAG_VELOCITY, 0.0f, 0.0f);

    forge_physics_apply_drag(&p, DRAG_COEFF);

    ASSERT_NEAR(p.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_apply_drag_zero_velocity(void)
{
    TEST("apply_drag — zero velocity produces zero drag");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    forge_physics_apply_drag(&p, 1.0f);

    ASSERT_NEAR(p.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_apply_drag_negative_coefficient(void)
{
    TEST("apply_drag — negative coefficient clamped to zero");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(NEG_DRAG_VEL, 0.0f, 0.0f);

    forge_physics_apply_drag(&p, NEG_DRAG_COEFF);

    /* Negative drag should be clamped to 0 — no force applied */
    ASSERT_NEAR(p.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.z, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. forge_physics_integrate
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_integrate_gravity_one_second(void)
{
    TEST("integrate — gravity for 1 second");
    vec3 pos = vec3_create(0.0f, INT_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, INT_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
    forge_physics_apply_gravity(&p, gravity);
    forge_physics_integrate(&p, INT_TIME_1SEC);

    /* v = 0 + GRAVITY_Y * 1 = GRAVITY_Y m/s */
    ASSERT_NEAR(p.velocity.y, INT_EXPECTED_VY, INT_TOLERANCE);
    /* x = INT_HEIGHT + GRAVITY_Y * 1 = INT_EXPECTED_PY (symplectic: uses new velocity) */
    ASSERT_NEAR(p.position.y, INT_EXPECTED_PY, INT_TOLERANCE);
    END_TEST();
}

static void test_integrate_zero_dt(void)
{
    TEST("integrate — zero dt is a no-op");
    vec3 pos = vec3_create(INT_ZERO_POS_X, INT_ZERO_POS_Y, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
    forge_physics_apply_gravity(&p, gravity);
    forge_physics_integrate(&p, 0.0f);

    /* Position unchanged */
    ASSERT_NEAR(p.position.x, INT_ZERO_POS_X, EPSILON);
    ASSERT_NEAR(p.position.y, INT_ZERO_POS_Y, EPSILON);
    /* Velocity unchanged */
    ASSERT_NEAR(p.velocity.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_integrate_static_particle(void)
{
    TEST("integrate — static particle does not move");
    vec3 pos = vec3_create(INT_STATIC_POS_X, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 0.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
    forge_physics_apply_gravity(&p, gravity);
    forge_physics_integrate(&p, INT_TIME_1SEC);

    ASSERT_NEAR(p.position.x, INT_STATIC_POS_X, EPSILON);
    ASSERT_NEAR(p.position.y, 0.0f, EPSILON);
    ASSERT_NEAR(p.velocity.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_integrate_velocity_clamping(void)
{
    TEST("integrate — velocity clamped to MAX_VELOCITY");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    /* Apply an enormous force to produce velocity > MAX_VELOCITY */
    vec3 huge_force = vec3_create(INT_HUGE_FORCE, 0.0f, 0.0f);
    forge_physics_apply_force(&p, huge_force);
    forge_physics_integrate(&p, INT_TIME_1SEC);

    float speed = vec3_length(p.velocity);
    ASSERT_TRUE(speed <= FORGE_PHYSICS_MAX_VELOCITY + EPSILON);
    ASSERT_TRUE(!isnan(p.position.x));
    ASSERT_TRUE(!isinf(p.position.x));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 5. forge_physics_collide_plane
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_collide_plane_no_collision(void)
{
    TEST("collide_plane — no collision when above plane");
    vec3 pos = vec3_create(0.0f, COL_ABOVE_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(0.0f, COL_DOWN_VEL, 0.0f);

    vec3 normal = vec3_create(0.0f, 1.0f, 0.0f);
    bool collided = forge_physics_collide_plane(&p, normal, 0.0f);

    ASSERT_TRUE(!collided);
    /* Position and velocity unchanged */
    ASSERT_NEAR(p.position.y, COL_ABOVE_HEIGHT, EPSILON);
    ASSERT_NEAR(p.velocity.y, COL_DOWN_VEL, EPSILON);
    END_TEST();
}

static void test_collide_plane_penetrating(void)
{
    TEST("collide_plane — resolves penetration");
    vec3 pos = vec3_create(0.0f, COL_PEN_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(0.0f, COL_PEN_VEL, 0.0f);

    vec3 normal = vec3_create(0.0f, 1.0f, 0.0f);
    bool collided = forge_physics_collide_plane(&p, normal, 0.0f);

    ASSERT_TRUE(collided);
    /* Pushed out: position.y >= radius */
    ASSERT_TRUE(p.position.y >= p.radius - EPSILON);
    /* Velocity reflected upward */
    ASSERT_TRUE(p.velocity.y > 0.0f);
    END_TEST();
}

static void test_collide_plane_restitution(void)
{
    TEST("collide_plane — restitution scales reflected velocity");
    vec3 pos = vec3_create(0.0f, COL_PEN_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, COL_REST_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(0.0f, COL_REST_VEL, 0.0f);

    vec3 normal = vec3_create(0.0f, 1.0f, 0.0f);
    ASSERT_TRUE(forge_physics_collide_plane(&p, normal, 0.0f));

    /* With restitution COL_REST_RESTIT, reflected velocity should be COL_REST_EXPECTED */
    ASSERT_NEAR(p.velocity.y, COL_REST_EXPECTED, COL_REST_TOL);
    END_TEST();
}

static void test_collide_plane_static_particle(void)
{
    TEST("collide_plane — static particle skipped");
    vec3 pos = vec3_create(0.0f, COL_STATIC_DEPTH, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 0.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(0.0f, COL_STATIC_VEL, 0.0f);

    vec3 normal = vec3_create(0.0f, 1.0f, 0.0f);
    bool collided = forge_physics_collide_plane(&p, normal, 0.0f);

    /* Static particles (inv_mass == 0) are skipped by collide_plane */
    ASSERT_TRUE(!collided);
    END_TEST();
}

static void test_collide_plane_zero_normal(void)
{
    TEST("collide_plane — zero-length normal returns false");
    vec3 pos = vec3_create(0.0f, ZERO_NORMAL_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(0.0f, COL_PEN_VEL, 0.0f);

    vec3 zero_normal = vec3_create(0.0f, 0.0f, 0.0f);
    bool collided = forge_physics_collide_plane(&p, zero_normal, 0.0f);

    ASSERT_TRUE(!collided);
    /* Position and velocity unchanged */
    ASSERT_NEAR(p.position.y, ZERO_NORMAL_HEIGHT, EPSILON);
    END_TEST();
}

static void test_collide_plane_static_unchanged(void)
{
    TEST("collide_plane — static particle skipped (inv_mass == 0)");
    vec3 pos = vec3_create(0.0f, COL_STATIC_INV_MASS_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 0.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(0.0f, COL_STATIC_VEL, 0.0f);

    vec3 normal = vec3_create(0.0f, 1.0f, 0.0f);
    bool collided = forge_physics_collide_plane(&p, normal, 0.0f);

    /* Static particle should be skipped entirely */
    ASSERT_TRUE(!collided);
    ASSERT_NEAR(p.position.y, COL_STATIC_INV_MASS_HEIGHT, EPSILON);
    ASSERT_NEAR(p.velocity.y, COL_STATIC_VEL, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 6. Determinism
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_determinism(void)
{
    TEST("determinism — identical inputs produce identical outputs");
    vec3 pos = vec3_create(0.0f, AG_HEIGHT, 0.0f);
    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
    vec3 ground_normal = vec3_create(0.0f, 1.0f, 0.0f);

    ForgePhysicsParticle a = forge_physics_particle_create(
        pos, DET_MASS, DET_DAMPING, DET_RESTIT, DEFAULT_RADIUS);
    ForgePhysicsParticle b = forge_physics_particle_create(
        pos, DET_MASS, DET_DAMPING, DET_RESTIT, DEFAULT_RADIUS);

    for (int i = 0; i < DET_ITERATIONS; i++) {
        forge_physics_apply_gravity(&a, gravity);
        forge_physics_apply_drag(&a, DET_DRAG);
        forge_physics_integrate(&a, PHYSICS_DT);
        forge_physics_collide_plane(&a, ground_normal, 0.0f);

        forge_physics_apply_gravity(&b, gravity);
        forge_physics_apply_drag(&b, DET_DRAG);
        forge_physics_integrate(&b, PHYSICS_DT);
        forge_physics_collide_plane(&b, ground_normal, 0.0f);
    }

    ASSERT_NEAR(a.position.x, b.position.x, EPSILON);
    ASSERT_NEAR(a.position.y, b.position.y, EPSILON);
    ASSERT_NEAR(a.position.z, b.position.z, EPSILON);
    ASSERT_NEAR(a.velocity.x, b.velocity.x, EPSILON);
    ASSERT_NEAR(a.velocity.y, b.velocity.y, EPSILON);
    ASSERT_NEAR(a.velocity.z, b.velocity.z, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 7. Energy stability
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_energy_stability_no_nan(void)
{
    TEST("energy stability — no NaN or Inf after 10000 steps");
    vec3 pos = vec3_create(0.0f, AG_HEIGHT, 0.0f);
    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
    vec3 ground_normal = vec3_create(0.0f, 1.0f, 0.0f);

    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, NRG_DAMPING, NRG_RESTIT, DEFAULT_RADIUS);

    for (int i = 0; i < NRG_ITERATIONS; i++) {
        forge_physics_apply_gravity(&p, gravity);
        forge_physics_apply_drag(&p, NRG_DRAG);
        forge_physics_integrate(&p, PHYSICS_DT);
        forge_physics_collide_plane(&p, ground_normal, 0.0f);
    }

    ASSERT_TRUE(!isnan(p.position.x) && !isnan(p.position.y) && !isnan(p.position.z));
    ASSERT_TRUE(!isinf(p.position.x) && !isinf(p.position.y) && !isinf(p.position.z));
    ASSERT_TRUE(!isnan(p.velocity.x) && !isnan(p.velocity.y) && !isnan(p.velocity.z));
    ASSERT_TRUE(!isinf(p.velocity.x) && !isinf(p.velocity.y) && !isinf(p.velocity.z));
    /* Position should remain reasonable */
    ASSERT_TRUE(SDL_fabsf(p.position.x) < NRG_POS_BOUND);
    ASSERT_TRUE(SDL_fabsf(p.position.y) < NRG_POS_BOUND);
    ASSERT_TRUE(SDL_fabsf(p.position.z) < NRG_POS_BOUND);
    END_TEST();
}

static void test_energy_stability_bounded(void)
{
    TEST("energy stability — energy does not grow unboundedly");
    vec3 pos = vec3_create(0.0f, AG_HEIGHT, 0.0f);
    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
    vec3 plane_normal = vec3_create(0.0f, 1.0f, 0.0f);
    float mass = 1.0f;

    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, mass, NRG_DAMPING, NRG_RESTIT, DEFAULT_RADIUS);

    /* Initial energy: E = 0.5 * m * v^2 + m * g * h
     * At rest at height AG_HEIGHT: E = 0 + mass * |GRAVITY_Y| * AG_HEIGHT */
    float gravity_mag = -GRAVITY_Y;  /* positive magnitude */
    float initial_energy = mass * gravity_mag * pos.y;

    for (int i = 0; i < NRG_ITERATIONS; i++) {
        forge_physics_apply_gravity(&p, gravity);
        forge_physics_apply_drag(&p, NRG_DRAG);
        forge_physics_integrate(&p, PHYSICS_DT);
        forge_physics_collide_plane(&p, plane_normal, 0.0f);
    }

    /* Kinetic energy: 0.5 * m * |v|^2 */
    float speed_sq = vec3_dot(p.velocity, p.velocity);
    float kinetic = 0.5f * mass * speed_sq;

    /* Potential energy: m * g * h (height above plane) */
    float potential = mass * gravity_mag * p.position.y;

    float total_energy = kinetic + potential;

    /* With restitution < 1 and drag, energy should dissipate, not grow.
     * Allow generous headroom (NRG_HEADROOM * initial) to catch explosions. */
    ASSERT_TRUE(total_energy < initial_energy * NRG_HEADROOM);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 8. forge_physics_clear_forces
 * ══════════════════════════════════════════════════════════════════════════ */

#define CF_FORCE_X   3.0f
#define CF_FORCE_Y   (-9.81f)
#define CF_FORCE_Z   1.5f

static void test_clear_forces(void)
{
    TEST("clear_forces — resets force accumulator to zero");
    vec3 pos = vec3_create(0.0f, AG_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    /* Apply a force so the accumulator is non-zero */
    vec3 force = vec3_create(CF_FORCE_X, CF_FORCE_Y, CF_FORCE_Z);
    forge_physics_apply_force(&p, force);

    /* Verify force_accum is non-zero before clearing */
    ASSERT_NEAR(p.force_accum.x, CF_FORCE_X, EPSILON);
    ASSERT_NEAR(p.force_accum.y, CF_FORCE_Y, EPSILON);
    ASSERT_NEAR(p.force_accum.z, CF_FORCE_Z, EPSILON);

    forge_physics_clear_forces(&p);

    ASSERT_NEAR(p.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(p.force_accum.z, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 9. forge_physics_apply_force (direct)
 * ══════════════════════════════════════════════════════════════════════════ */

#define AF_FORCE1_X    5.0f
#define AF_FORCE1_Y    0.0f
#define AF_FORCE1_Z    (-3.0f)
#define AF_FORCE2_X    (-2.0f)
#define AF_FORCE2_Y    7.0f
#define AF_FORCE2_Z    1.0f
#define AF_SUM_X       3.0f   /* AF_FORCE1_X + AF_FORCE2_X */
#define AF_SUM_Y       7.0f   /* AF_FORCE1_Y + AF_FORCE2_Y */
#define AF_SUM_Z       (-2.0f) /* AF_FORCE1_Z + AF_FORCE2_Z */

static void test_apply_force_direct(void)
{
    TEST("apply_force — accumulates forces additively");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    /* Apply first force */
    vec3 f1 = vec3_create(AF_FORCE1_X, AF_FORCE1_Y, AF_FORCE1_Z);
    forge_physics_apply_force(&p, f1);

    ASSERT_NEAR(p.force_accum.x, AF_FORCE1_X, EPSILON);
    ASSERT_NEAR(p.force_accum.y, AF_FORCE1_Y, EPSILON);
    ASSERT_NEAR(p.force_accum.z, AF_FORCE1_Z, EPSILON);

    /* Apply second force — should accumulate */
    vec3 f2 = vec3_create(AF_FORCE2_X, AF_FORCE2_Y, AF_FORCE2_Z);
    forge_physics_apply_force(&p, f2);

    ASSERT_NEAR(p.force_accum.x, AF_SUM_X, EPSILON);
    ASSERT_NEAR(p.force_accum.y, AF_SUM_Y, EPSILON);
    ASSERT_NEAR(p.force_accum.z, AF_SUM_Z, EPSILON);

    /* Static particle — force should have no effect */
    ForgePhysicsParticle s = forge_physics_particle_create(
        pos, 0.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    forge_physics_apply_force(&s, f1);

    ASSERT_NEAR(s.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(s.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(s.force_accum.z, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 10. forge_physics_integrate — negative dt
 * ══════════════════════════════════════════════════════════════════════════ */

#define INT_NEG_POS_X   3.0f
#define INT_NEG_POS_Y   7.0f
#define INT_NEG_POS_Z   (-2.0f)
#define INT_NEG_VEL_X   10.0f
#define INT_NEG_VEL_Y   (-5.0f)
#define INT_NEG_VEL_Z   2.0f
#define INT_NEG_DT      (-1.0f)

static void test_integrate_negative_dt(void)
{
    TEST("integrate — negative dt is rejected (no-op)");
    vec3 pos = vec3_create(INT_NEG_POS_X, INT_NEG_POS_Y, INT_NEG_POS_Z);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(INT_NEG_VEL_X, INT_NEG_VEL_Y, INT_NEG_VEL_Z);

    forge_physics_integrate(&p, INT_NEG_DT);

    /* Position must be unchanged */
    ASSERT_NEAR(p.position.x, INT_NEG_POS_X, EPSILON);
    ASSERT_NEAR(p.position.y, INT_NEG_POS_Y, EPSILON);
    ASSERT_NEAR(p.position.z, INT_NEG_POS_Z, EPSILON);

    /* Velocity must be unchanged */
    ASSERT_NEAR(p.velocity.x, INT_NEG_VEL_X, EPSILON);
    ASSERT_NEAR(p.velocity.y, INT_NEG_VEL_Y, EPSILON);
    ASSERT_NEAR(p.velocity.z, INT_NEG_VEL_Z, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 11. forge_physics_collide_plane — zero restitution (fully inelastic)
 * ══════════════════════════════════════════════════════════════════════════ */

#define COL_INELASTIC_RESTIT   0.0f
#define COL_INELASTIC_VEL_Y    (-10.0f)
#define COL_INELASTIC_VEL_X    4.0f   /* tangential component */
#define COL_INELASTIC_HEIGHT   0.1f

static void test_collide_plane_zero_restitution(void)
{
    TEST("collide_plane — zero restitution kills normal velocity");
    vec3 pos = vec3_create(0.0f, COL_INELASTIC_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, 1.0f, 0.0f, COL_INELASTIC_RESTIT, DEFAULT_RADIUS);
    p.velocity = vec3_create(COL_INELASTIC_VEL_X, COL_INELASTIC_VEL_Y, 0.0f);

    vec3 normal = vec3_create(0.0f, 1.0f, 0.0f);
    bool collided = forge_physics_collide_plane(&p, normal, 0.0f);

    ASSERT_TRUE(collided);

    /* Normal velocity component should be zero (fully inelastic) */
    ASSERT_NEAR(p.velocity.y, 0.0f, EPSILON);

    /* Tangential component should be preserved */
    ASSERT_NEAR(p.velocity.x, COL_INELASTIC_VEL_X, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 12. forge_physics_particle_create — negative mass
 * ══════════════════════════════════════════════════════════════════════════ */

#define PC_NEG_MASS   (-5.0f)

static void test_particle_create_negative_mass(void)
{
    TEST("particle_create — negative mass treated as static");
    vec3 pos = vec3_create(0.0f, 0.0f, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, PC_NEG_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    /* Negative mass should be treated as static (mass=0, inv_mass=0) */
    ASSERT_NEAR(p.mass, 0.0f, EPSILON);
    ASSERT_NEAR(p.inv_mass, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 13. Very heavy particle must NOT be treated as static
 * ══════════════════════════════════════════════════════════════════════════ */

#define VH_MASS           2000000.0f
#define VH_INV_MASS       (1.0f / VH_MASS)   /* ~5e-7, below EPSILON */
#define VH_PEN_HEIGHT     0.1f
#define VH_PEN_VEL        (-5.0f)

static void test_very_heavy_particle_not_static(void)
{
    TEST("very heavy particle (mass=2M) is NOT treated as static");

    /* Create a particle with mass = 2,000,000. Its inv_mass (~5e-7) is
     * less than FORGE_PHYSICS_EPSILON. Under the old SDL_fabsf(inv_mass) < EPSILON
     * guard, this particle would have been incorrectly treated as static. */
    vec3 pos = vec3_create(0.0f, AG_HEIGHT, 0.0f);
    ForgePhysicsParticle p = forge_physics_particle_create(
        pos, VH_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    /* Sanity: inv_mass is tiny but non-zero */
    ASSERT_TRUE(p.inv_mass > 0.0f);
    ASSERT_TRUE(p.inv_mass < FORGE_PHYSICS_EPSILON);

    /* Apply gravity — must NOT be skipped */
    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
    forge_physics_apply_gravity(&p, gravity);

    /* Force accumulator must be non-zero (gravity was applied) */
    float fy = VH_MASS * GRAVITY_Y;
    ASSERT_NEAR(p.force_accum.y, fy, SDL_fabsf(fy) * 0.001f);

    /* Integrate with dt = 1/60 — particle must move */
    float orig_y = p.position.y;
    forge_physics_integrate(&p, PHYSICS_DT);
    ASSERT_TRUE(SDL_fabsf(p.position.y - orig_y) > EPSILON);

    /* Collide with ground plane while penetrating */
    p.position.y = VH_PEN_HEIGHT;  /* below radius — penetrating */
    p.velocity.y = VH_PEN_VEL;
    vec3 normal = vec3_create(0.0f, 1.0f, 0.0f);
    bool collided = forge_physics_collide_plane(&p, normal, 0.0f);

    /* Collision must be detected — not skipped as static */
    ASSERT_TRUE(collided);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 14. forge_physics_spring_create
 * ══════════════════════════════════════════════════════════════════════════ */

#define SP_REST_LEN   2.0f
#define SP_STIFFNESS  50.0f
#define SP_DAMPING    1.0f

static void test_spring_create_normal(void)
{
    TEST("spring_create — valid parameters");
    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_LEN, SP_STIFFNESS, SP_DAMPING);

    ASSERT_NEAR(s.rest_length, SP_REST_LEN, EPSILON);
    ASSERT_NEAR(s.stiffness, SP_STIFFNESS, EPSILON);
    ASSERT_NEAR(s.damping, SP_DAMPING, EPSILON);
    ASSERT_TRUE(s.a == 0);
    ASSERT_TRUE(s.b == 1);
    END_TEST();
}

static void test_spring_create_zero_stiffness(void)
{
    TEST("spring_create — zero stiffness");
    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_LEN, 0.0f, SP_DAMPING);

    ASSERT_NEAR(s.stiffness, 0.0f, EPSILON);
    END_TEST();
}

static void test_spring_create_zero_damping(void)
{
    TEST("spring_create — zero damping");
    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_LEN, SP_STIFFNESS, 0.0f);

    ASSERT_NEAR(s.damping, 0.0f, EPSILON);
    END_TEST();
}

static void test_spring_create_zero_rest_length(void)
{
    TEST("spring_create — zero rest length (bungee-style)");
    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, 0.0f, SP_STIFFNESS, SP_DAMPING);

    ASSERT_NEAR(s.rest_length, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 15. forge_physics_spring_apply — force correctness
 * ══════════════════════════════════════════════════════════════════════════ */

/* Two particles at (0,0,0) and (3,0,0) with rest_length=2, k=10, b=0.
 * Displacement = 3-2 = 1. F = k * displacement = 10 * 1 = 10 N along +x.
 * Particle a gets +10x, particle b gets -10x. */
#define SP_K_TEST      10.0f
#define SP_SEP_TEST    3.0f
#define SP_REST_TEST   2.0f
#define SP_F_EXPECTED  10.0f  /* k * (3 - 2) = 10 */

static void test_spring_force_extension(void)
{
    TEST("spring_apply — extension produces correct force (Hooke's law)");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(SP_SEP_TEST, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_TEST, SP_K_TEST, 0.0f);
    forge_physics_spring_apply(&s, particles, 2);

    /* Particle 0: force toward particle 1 (+x) */
    ASSERT_NEAR(particles[0].force_accum.x, SP_F_EXPECTED, EPSILON);
    ASSERT_NEAR(particles[0].force_accum.y, 0.0f, EPSILON);
    /* Particle 1: force toward particle 0 (-x) */
    ASSERT_NEAR(particles[1].force_accum.x, -SP_F_EXPECTED, EPSILON);
    END_TEST();
}

/* Compression: particles at (0,0,0) and (1,0,0), rest_length=2, k=10.
 * Displacement = 1-2 = -1. F = 10 * (-1) = -10 along +x direction.
 * Particle a gets -10x (pushed away), particle b gets +10x. */
#define SP_COMP_SEP    1.0f
#define SP_COMP_F      (-10.0f)  /* k * (1 - 2) = -10 */

static void test_spring_force_compression(void)
{
    TEST("spring_apply — compression produces opposite force");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(SP_COMP_SEP, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_TEST, SP_K_TEST, 0.0f);
    forge_physics_spring_apply(&s, particles, 2);

    /* Particle 0: pushed away from 1 (-x direction) */
    ASSERT_NEAR(particles[0].force_accum.x, SP_COMP_F, EPSILON);
    /* Particle 1: pushed away from 0 (+x direction) */
    ASSERT_NEAR(particles[1].force_accum.x, -SP_COMP_F, EPSILON);
    END_TEST();
}

/* Diagonal spring: particles at (0,0,0) and (3,4,0), distance=5.
 * With rest_length=3, k=20: displacement=5-3=2, F=40 along (3/5, 4/5, 0). */
#define SP_DIAG_POS_X  3.0f
#define SP_DIAG_POS_Y  4.0f
#define SP_DIAG_REST   3.0f
#define SP_DIAG_K      20.0f
#define SP_DIAG_DIST   5.0f  /* sqrt(9+16) */
#define SP_DIAG_DISP   2.0f  /* 5 - 3 */
#define SP_DIAG_F      40.0f /* 20 * 2 */
#define SP_DIAG_FX     24.0f /* 40 * 3/5 */
#define SP_DIAG_FY     32.0f /* 40 * 4/5 */
/* Slightly looser tolerance for diagonal due to float imprecision in sqrt */
#define SP_DIAG_TOL    0.01f

static void test_spring_force_diagonal(void)
{
    TEST("spring_apply — diagonal spring with known geometry");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(SP_DIAG_POS_X, SP_DIAG_POS_Y, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_DIAG_REST, SP_DIAG_K, 0.0f);
    forge_physics_spring_apply(&s, particles, 2);

    ASSERT_NEAR(particles[0].force_accum.x, SP_DIAG_FX, SP_DIAG_TOL);
    ASSERT_NEAR(particles[0].force_accum.y, SP_DIAG_FY, SP_DIAG_TOL);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 16. Spring force symmetry (Newton's third law)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_spring_force_symmetry(void)
{
    TEST("spring_apply — Newton's third law: F_a = -F_b");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(SP_SEP_TEST, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_TEST, SP_K_TEST, SP_DAMPING);
    forge_physics_spring_apply(&s, particles, 2);

    /* F_a + F_b = 0 (Newton's third law) */
    ASSERT_NEAR(particles[0].force_accum.x + particles[1].force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[0].force_accum.y + particles[1].force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(particles[0].force_accum.z + particles[1].force_accum.z, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 17. Damped spring behavior
 * ══════════════════════════════════════════════════════════════════════════ */

#define SP_DAMP_K       50.0f
#define SP_DAMP_B       2.0f
#define SP_DAMP_STEPS   500
#define SP_DAMP_DT      (1.0f / 60.0f)

static void test_spring_damped_amplitude_decreases(void)
{
    TEST("spring — damped oscillation: amplitude decreases over time");
    ForgePhysicsParticle particles[2];
    /* Fixed anchor at origin */
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f); /* static */
    /* Dynamic particle displaced from rest */
    particles[1] = forge_physics_particle_create(
        vec3_create(4.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_TEST, SP_DAMP_K, SP_DAMP_B);

    /* Track peak displacement in first and second halves */
    float max_disp_first_half = 0.0f;
    float max_disp_second_half = 0.0f;

    for (int i = 0; i < SP_DAMP_STEPS; i++) {
        forge_physics_clear_forces(&particles[1]);
        forge_physics_spring_apply(&s, particles, 2);
        forge_physics_integrate(&particles[1], SP_DAMP_DT);

        float disp = SDL_fabsf(particles[1].position.x - SP_REST_TEST);
        if (i < SP_DAMP_STEPS / 2) {
            if (disp > max_disp_first_half) max_disp_first_half = disp;
        } else {
            if (disp > max_disp_second_half) max_disp_second_half = disp;
        }
    }

    /* Amplitude in second half must be less than first half */
    ASSERT_TRUE(max_disp_second_half < max_disp_first_half);
    END_TEST();
}

static void test_spring_damped_energy_decreases(void)
{
    TEST("spring — damped oscillation: energy decreases overall");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f); /* static */
    particles[1] = forge_physics_particle_create(
        vec3_create(4.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_TEST, SP_DAMP_K, SP_DAMP_B);

    /* Compute initial energy */
    float disp0 = 4.0f - SP_REST_TEST;
    float initial_pe = 0.5f * SP_DAMP_K * disp0 * disp0;

    for (int i = 0; i < SP_DAMP_STEPS; i++) {
        forge_physics_clear_forces(&particles[1]);
        forge_physics_spring_apply(&s, particles, 2);
        forge_physics_integrate(&particles[1], SP_DAMP_DT);
    }

    /* Final energy */
    float d = particles[1].position.x - SP_REST_TEST;
    float final_pe = 0.5f * SP_DAMP_K * d * d;
    float final_ke = 0.5f * 1.0f * vec3_length_squared(particles[1].velocity);
    float final_energy = final_pe + final_ke;

    /* Energy must have decreased */
    ASSERT_TRUE(final_energy < initial_pe);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 18. Spring + static particle
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_spring_static_particle_no_force(void)
{
    TEST("spring_apply — static particle accumulates no force");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f); /* static */
    particles[1] = forge_physics_particle_create(
        vec3_create(SP_SEP_TEST, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_TEST, SP_K_TEST, 0.0f);
    forge_physics_spring_apply(&s, particles, 2);

    /* Static particle should have zero force */
    ASSERT_NEAR(particles[0].force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[0].force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(particles[0].force_accum.z, 0.0f, EPSILON);

    /* Dynamic particle should still get force */
    ASSERT_TRUE(SDL_fabsf(particles[1].force_accum.x) > EPSILON);
    END_TEST();
}

static void test_spring_static_position_unchanged(void)
{
    TEST("spring_apply + integrate — static particle position unchanged");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f); /* static */
    particles[1] = forge_physics_particle_create(
        vec3_create(SP_SEP_TEST, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_TEST, SP_K_TEST, SP_DAMPING);

    for (int i = 0; i < 100; i++) {
        forge_physics_clear_forces(&particles[0]);
        forge_physics_clear_forces(&particles[1]);
        forge_physics_spring_apply(&s, particles, 2);
        forge_physics_integrate(&particles[0], PHYSICS_DT);
        forge_physics_integrate(&particles[1], PHYSICS_DT);
    }

    /* Static particle must not have moved */
    ASSERT_NEAR(particles[0].position.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[0].position.y, 0.0f, EPSILON);
    ASSERT_NEAR(particles[0].position.z, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 19. Spring degenerate cases
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_spring_coincident_particles(void)
{
    TEST("spring_apply — coincident particles: no crash, no force");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(5.0f, 5.0f, 5.0f), 1.0f, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(5.0f, 5.0f, 5.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, SP_REST_TEST, SP_K_TEST, SP_DAMPING);
    forge_physics_spring_apply(&s, particles, 2);

    /* No NaN, no crash, forces remain zero */
    ASSERT_TRUE(!isnan(particles[0].force_accum.x));
    ASSERT_TRUE(!isnan(particles[1].force_accum.x));
    ASSERT_NEAR(particles[0].force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_spring_out_of_bounds_indices(void)
{
    TEST("spring_apply — out-of-bounds indices: no crash");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(SP_SEP_TEST, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    /* Index 5 is out of bounds for array of 2 */
    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 5, SP_REST_TEST, SP_K_TEST, SP_DAMPING);
    forge_physics_spring_apply(&s, particles, 2);

    /* No force applied, no crash */
    ASSERT_NEAR(particles[0].force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].force_accum.x, 0.0f, EPSILON);

    /* Negative index */
    s = forge_physics_spring_create(-1, 1, SP_REST_TEST, SP_K_TEST, SP_DAMPING);
    forge_physics_spring_apply(&s, particles, 2);
    ASSERT_NEAR(particles[0].force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 20. forge_physics_constraint_distance_create
 * ══════════════════════════════════════════════════════════════════════════ */

#define DC_DIST       2.5f
#define DC_STIFF      0.8f

static void test_constraint_create_normal(void)
{
    TEST("constraint_distance_create — valid parameters");
    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(0, 1, DC_DIST, DC_STIFF);

    ASSERT_TRUE(c.a == 0);
    ASSERT_TRUE(c.b == 1);
    ASSERT_NEAR(c.distance, DC_DIST, EPSILON);
    ASSERT_NEAR(c.stiffness, DC_STIFF, EPSILON);
    END_TEST();
}

static void test_constraint_create_zero_distance(void)
{
    TEST("constraint_distance_create — zero distance");
    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(0, 1, 0.0f, 1.0f);

    ASSERT_NEAR(c.distance, 0.0f, EPSILON);
    END_TEST();
}

static void test_constraint_create_stiffness_clamped(void)
{
    TEST("constraint_distance_create — stiffness clamped to [0, 1]");
    ForgePhysicsDistanceConstraint c1 =
        forge_physics_constraint_distance_create(0, 1, DC_DIST, 5.0f);
    ASSERT_TRUE(c1.stiffness <= 1.0f);

    ForgePhysicsDistanceConstraint c2 =
        forge_physics_constraint_distance_create(0, 1, DC_DIST, -3.0f);
    ASSERT_TRUE(c2.stiffness >= 0.0f);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 21. forge_physics_constraint_solve_distance — projection
 * ══════════════════════════════════════════════════════════════════════════ */

/* Two particles at (0,0,0) and (4,0,0), constraint distance=2, stiffness=1.
 * They are 4 apart but want to be 2 apart → each should move 1 unit inward
 * (equal mass). After solve: p0=(1,0,0), p1=(3,0,0). */
#define DC_PROJ_SEP     4.0f
#define DC_PROJ_TARGET  2.0f
#define DC_PROJ_A_X     1.0f   /* 0 + (4-2)/2 = 1 */
#define DC_PROJ_B_X     3.0f   /* 4 - (4-2)/2 = 3 */

static void test_constraint_projection_pull_in(void)
{
    TEST("constraint_solve — particles too far apart are pulled in");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(DC_PROJ_SEP, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(0, 1, DC_PROJ_TARGET, 1.0f);
    forge_physics_constraint_solve_distance(&c, particles, 2);

    ASSERT_NEAR(particles[0].position.x, DC_PROJ_A_X, EPSILON);
    ASSERT_NEAR(particles[1].position.x, DC_PROJ_B_X, EPSILON);
    END_TEST();
}

/* Two particles at (0,0,0) and (1,0,0), constraint distance=3, stiffness=1.
 * They are 1 apart but want to be 3 apart → each pushed 1 unit outward.
 * After solve: p0=(-1,0,0), p1=(2,0,0). */
#define DC_PUSH_SEP     1.0f
#define DC_PUSH_TARGET  3.0f
#define DC_PUSH_A_X     (-1.0f) /* 0 - (3-1)/2 = -1 */
#define DC_PUSH_B_X     2.0f    /* 1 + (3-1)/2 = 2 */

static void test_constraint_projection_push_apart(void)
{
    TEST("constraint_solve — particles too close are pushed apart");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(DC_PUSH_SEP, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(0, 1, DC_PUSH_TARGET, 1.0f);
    forge_physics_constraint_solve_distance(&c, particles, 2);

    ASSERT_NEAR(particles[0].position.x, DC_PUSH_A_X, EPSILON);
    ASSERT_NEAR(particles[1].position.x, DC_PUSH_B_X, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 22. Constraint mass weighting
 * ══════════════════════════════════════════════════════════════════════════ */

/* Particle a: mass=1 (inv_mass=1), particle b: mass=3 (inv_mass=1/3).
 * At (0,0,0) and (4,0,0), target distance=2, stiffness=1.
 * w_total = 1 + 1/3 = 4/3. Error = 4-2 = 2.
 * w_a = 1/(4/3) = 3/4. w_b = (1/3)/(4/3) = 1/4.
 * p0 moves: 3/4 * 2 = 1.5. p1 moves: 1/4 * 2 = 0.5.
 * Result: p0=(1.5, 0, 0), p1=(3.5, 0, 0). */
#define DC_MW_MASS_A    1.0f
#define DC_MW_MASS_B    3.0f
#define DC_MW_A_X       1.5f
#define DC_MW_B_X       3.5f

static void test_constraint_mass_weighting(void)
{
    TEST("constraint_solve — mass weighting: lighter particle moves more");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), DC_MW_MASS_A, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(DC_PROJ_SEP, 0.0f, 0.0f), DC_MW_MASS_B, 0.0f, 0.0f, 0.0f);

    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(0, 1, DC_PROJ_TARGET, 1.0f);
    forge_physics_constraint_solve_distance(&c, particles, 2);

    ASSERT_NEAR(particles[0].position.x, DC_MW_A_X, EPSILON);
    ASSERT_NEAR(particles[1].position.x, DC_MW_B_X, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 23. Constraint + static particle
 * ══════════════════════════════════════════════════════════════════════════ */

/* Static particle at (0,0,0), dynamic at (4,0,0), target=2.
 * Static stays at 0; dynamic moves to (2,0,0). */
#define DC_STATIC_DYN_X  2.0f

static void test_constraint_static_particle(void)
{
    TEST("constraint_solve — static particle stays, other moves full correction");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f); /* static */
    particles[1] = forge_physics_particle_create(
        vec3_create(DC_PROJ_SEP, 0.0f, 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);

    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(0, 1, DC_PROJ_TARGET, 1.0f);
    forge_physics_constraint_solve_distance(&c, particles, 2);

    /* Static particle unmoved */
    ASSERT_NEAR(particles[0].position.x, 0.0f, EPSILON);
    /* Dynamic particle moves to distance from static */
    ASSERT_NEAR(particles[1].position.x, DC_STATIC_DYN_X, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 24. Multi-constraint solver (Gauss-Seidel)
 * ══════════════════════════════════════════════════════════════════════════ */

#define CHAIN_LEN            5
#define CHAIN_SPACING        2.0f
#define CHAIN_TARGET         1.0f
#define CHAIN_CONVERGE_TOL   0.05f  /* allowable error after 50 iterations */
#define CHAIN_FEW_ITERS      2
#define CHAIN_MANY_ITERS     50

static void test_constraints_solve_chain_converges(void)
{
    TEST("constraints_solve — chain of 5 converges toward target distances");
    ForgePhysicsParticle particles[CHAIN_LEN];
    ForgePhysicsDistanceConstraint constraints[CHAIN_LEN - 1];

    /* Place particles in a line with spacing 2, target distance 1. */
    for (int i = 0; i < CHAIN_LEN; i++) {
        float mass = (i == 0) ? 0.0f : 1.0f; /* first particle is static */
        particles[i] = forge_physics_particle_create(
            vec3_create((float)i * CHAIN_SPACING, 0.0f, 0.0f),
            mass, 0.0f, 0.0f, 0.0f);
    }
    for (int i = 0; i < CHAIN_LEN - 1; i++) {
        constraints[i] = forge_physics_constraint_distance_create(
            i, i + 1, CHAIN_TARGET, 1.0f);
    }

    /* Solve with many iterations */
    forge_physics_constraints_solve(constraints, CHAIN_LEN - 1,
                                    particles, CHAIN_LEN, CHAIN_MANY_ITERS);

    /* Check that each pair is approximately at the target distance */
    for (int i = 0; i < CHAIN_LEN - 1; i++) {
        float dist = vec3_length(
            vec3_sub(particles[i + 1].position, particles[i].position));
        ASSERT_NEAR(dist, CHAIN_TARGET, CHAIN_CONVERGE_TOL);
    }
    END_TEST();
}

static void test_constraints_solve_more_iterations_better(void)
{
    TEST("constraints_solve — more iterations = closer to target");
    ForgePhysicsParticle p_few[CHAIN_LEN], p_many[CHAIN_LEN];
    ForgePhysicsDistanceConstraint constraints[CHAIN_LEN - 1];

    for (int i = 0; i < CHAIN_LEN; i++) {
        float mass = (i == 0) ? 0.0f : 1.0f;
        vec3 pos = vec3_create((float)i * CHAIN_SPACING, 0.0f, 0.0f);
        p_few[i] = forge_physics_particle_create(pos, mass, 0.0f, 0.0f, 0.0f);
        p_many[i] = forge_physics_particle_create(pos, mass, 0.0f, 0.0f, 0.0f);
    }
    for (int i = 0; i < CHAIN_LEN - 1; i++) {
        constraints[i] = forge_physics_constraint_distance_create(
            i, i + 1, CHAIN_TARGET, 1.0f);
    }

    /* Solve with few and many iterations */
    forge_physics_constraints_solve(constraints, CHAIN_LEN - 1,
                                    p_few, CHAIN_LEN, CHAIN_FEW_ITERS);
    forge_physics_constraints_solve(constraints, CHAIN_LEN - 1,
                                    p_many, CHAIN_LEN, CHAIN_MANY_ITERS);

    /* Compute total error for each */
    float error_few = 0.0f, error_many = 0.0f;
    for (int i = 0; i < CHAIN_LEN - 1; i++) {
        float d_few = SDL_fabsf(vec3_length(
            vec3_sub(p_few[i + 1].position, p_few[i].position)) - CHAIN_TARGET);
        float d_many = SDL_fabsf(vec3_length(
            vec3_sub(p_many[i + 1].position, p_many[i].position)) - CHAIN_TARGET);
        error_few += d_few;
        error_many += d_many;
    }

    /* More iterations should produce less total error */
    ASSERT_TRUE(error_many < error_few);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 25. Constraint stability
 * ══════════════════════════════════════════════════════════════════════════ */

#define STAB_STEPS       10000
#define STAB_HEIGHT      5.0f
#define STAB_SPACING     1.0f
#define STAB_DAMPING     0.01f
#define STAB_ITERS       5

static void test_constraint_stability_no_nan(void)
{
    TEST("constraint — 10000 steps with gravity, no NaN/Inf");
    ForgePhysicsParticle particles[3];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, STAB_HEIGHT, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f); /* static */
    particles[1] = forge_physics_particle_create(
        vec3_create(STAB_SPACING, STAB_HEIGHT, 0.0f), 1.0f, STAB_DAMPING, 0.0f, 0.0f);
    particles[2] = forge_physics_particle_create(
        vec3_create(STAB_SPACING * 2.0f, STAB_HEIGHT, 0.0f), 1.0f, STAB_DAMPING, 0.0f, 0.0f);

    ForgePhysicsDistanceConstraint constraints[2];
    constraints[0] = forge_physics_constraint_distance_create(0, 1, STAB_SPACING, 1.0f);
    constraints[1] = forge_physics_constraint_distance_create(1, 2, STAB_SPACING, 1.0f);

    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);

    for (int i = 0; i < STAB_STEPS; i++) {
        for (int j = 1; j < 3; j++) {
            forge_physics_clear_forces(&particles[j]);
            forge_physics_apply_gravity(&particles[j], gravity);
            forge_physics_integrate(&particles[j], PHYSICS_DT);
        }
        forge_physics_constraints_solve(constraints, 2, particles, 3, STAB_ITERS);
    }

    for (int j = 0; j < 3; j++) {
        ASSERT_TRUE(!isnan(particles[j].position.x));
        ASSERT_TRUE(!isnan(particles[j].position.y));
        ASSERT_TRUE(!isnan(particles[j].position.z));
        ASSERT_TRUE(!isinf(particles[j].position.x));
        ASSERT_TRUE(!isinf(particles[j].position.y));
        ASSERT_TRUE(!isinf(particles[j].position.z));
        ASSERT_TRUE(!isnan(particles[j].velocity.x));
        ASSERT_TRUE(!isnan(particles[j].velocity.y));
        ASSERT_TRUE(!isnan(particles[j].velocity.z));
        ASSERT_TRUE(!isinf(particles[j].velocity.x));
        ASSERT_TRUE(!isinf(particles[j].velocity.y));
        ASSERT_TRUE(!isinf(particles[j].velocity.z));
    }
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 26. Spring-constraint comparison
 * ══════════════════════════════════════════════════════════════════════════ */

#define CMP_STEPS          2000
#define CMP_STIFF_K        200.0f
#define CMP_DAMP_B         5.0f
#define CMP_REST_LEN       2.0f
#define CMP_ANCHOR_Y       5.0f
#define CMP_INIT_Y         2.0f
#define CMP_PARTICLE_DAMP  0.01f
#define CMP_CONSTRAINT_TOL 0.1f   /* constraint maintains distance tightly */
#define CMP_SPRING_TOL     1.0f   /* spring sags under gravity */

static void test_spring_constraint_comparison(void)
{
    TEST("spring vs constraint — both reach similar equilibrium");

    /* Spring system */
    ForgePhysicsParticle sp[2];
    sp[0] = forge_physics_particle_create(
        vec3_create(0.0f, CMP_ANCHOR_Y, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f); /* static */
    sp[1] = forge_physics_particle_create(
        vec3_create(0.0f, CMP_INIT_Y, 0.0f), 1.0f, CMP_PARTICLE_DAMP, 0.0f, 0.0f);
    ForgePhysicsSpring spring = forge_physics_spring_create(
        0, 1, CMP_REST_LEN, CMP_STIFF_K, CMP_DAMP_B);

    /* Constraint system */
    ForgePhysicsParticle cp[2];
    cp[0] = forge_physics_particle_create(
        vec3_create(0.0f, CMP_ANCHOR_Y, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f); /* static */
    cp[1] = forge_physics_particle_create(
        vec3_create(0.0f, CMP_INIT_Y, 0.0f), 1.0f, CMP_PARTICLE_DAMP, 0.0f, 0.0f);
    ForgePhysicsDistanceConstraint dc =
        forge_physics_constraint_distance_create(0, 1, CMP_REST_LEN, 1.0f);

    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);

    for (int i = 0; i < CMP_STEPS; i++) {
        /* Spring system */
        forge_physics_clear_forces(&sp[1]);
        forge_physics_apply_gravity(&sp[1], gravity);
        forge_physics_spring_apply(&spring, sp, 2);
        forge_physics_integrate(&sp[1], PHYSICS_DT);

        /* Constraint system */
        forge_physics_clear_forces(&cp[1]);
        forge_physics_apply_gravity(&cp[1], gravity);
        forge_physics_integrate(&cp[1], PHYSICS_DT);
        forge_physics_constraint_solve_distance(&dc, cp, 2);
    }

    /* Both should have particle 1 approximately 2 units from static anchor.
     * Spring will oscillate but with damping should settle near rest. */
    float sp_dist = vec3_length(vec3_sub(sp[1].position, sp[0].position));
    float cp_dist = vec3_length(vec3_sub(cp[1].position, cp[0].position));

    /* Both should be near the target distance, within reasonable tolerance.
     * Spring may settle at a gravity-displaced equilibrium, so allow more tolerance. */
    ASSERT_TRUE(SDL_fabsf(cp_dist - CMP_REST_LEN) < CMP_CONSTRAINT_TOL);
    ASSERT_TRUE(SDL_fabsf(sp_dist - CMP_REST_LEN) < CMP_SPRING_TOL); /* spring has gravity sag */
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 27. Spring + constraint determinism
 * ══════════════════════════════════════════════════════════════════════════ */

#define DET2_STEPS      1000
#define DET2_HEIGHT     5.0f
#define DET2_SPACING    1.0f
#define DET2_DAMPING    0.01f
#define DET2_SPRING_K   50.0f
#define DET2_SPRING_B   1.0f

static void test_spring_constraint_determinism(void)
{
    TEST("determinism — spring+constraint: two identical systems match");
    vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);

    /* System A */
    ForgePhysicsParticle a[3];
    a[0] = forge_physics_particle_create(
        vec3_create(0.0f, DET2_HEIGHT, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f);
    a[1] = forge_physics_particle_create(
        vec3_create(DET2_SPACING, DET2_HEIGHT, 0.0f), 1.0f, DET2_DAMPING, 0.0f, 0.0f);
    a[2] = forge_physics_particle_create(
        vec3_create(DET2_SPACING * 2.0f, DET2_HEIGHT, 0.0f), 1.0f, DET2_DAMPING, 0.0f, 0.0f);
    ForgePhysicsSpring sp = forge_physics_spring_create(
        0, 1, DET2_SPACING, DET2_SPRING_K, DET2_SPRING_B);
    ForgePhysicsDistanceConstraint dc =
        forge_physics_constraint_distance_create(1, 2, DET2_SPACING, 1.0f);

    /* System B — identical */
    ForgePhysicsParticle b[3];
    b[0] = a[0]; b[1] = a[1]; b[2] = a[2];

    for (int i = 0; i < DET2_STEPS; i++) {
        for (int j = 1; j < 3; j++) {
            forge_physics_clear_forces(&a[j]);
            forge_physics_apply_gravity(&a[j], gravity);
            forge_physics_clear_forces(&b[j]);
            forge_physics_apply_gravity(&b[j], gravity);
        }
        forge_physics_spring_apply(&sp, a, 3);
        forge_physics_spring_apply(&sp, b, 3);
        for (int j = 1; j < 3; j++) {
            forge_physics_integrate(&a[j], PHYSICS_DT);
            forge_physics_integrate(&b[j], PHYSICS_DT);
        }
        forge_physics_constraint_solve_distance(&dc, a, 3);
        forge_physics_constraint_solve_distance(&dc, b, 3);
    }

    for (int j = 0; j < 3; j++) {
        ASSERT_NEAR(a[j].position.x, b[j].position.x, EPSILON);
        ASSERT_NEAR(a[j].position.y, b[j].position.y, EPSILON);
        ASSERT_NEAR(a[j].position.z, b[j].position.z, EPSILON);
    }
    END_TEST();
}

/* Shared constants for guard/edge-case tests (sections 28–31) */
#define GUARD_MASS        1.0f    /* unit mass for dynamic particles */
#define GUARD_POS_NEAR    2.0f    /* nearby particle x position */
#define GUARD_POS_FAR     4.0f    /* far particle x position */
#define GUARD_POS_ANCHOR  5.0f    /* static anchor x position */
#define GUARD_REST        1.0f    /* spring rest length */
#define GUARD_K           10.0f   /* spring stiffness */
#define GUARD_DC_DIST     2.0f    /* constraint target distance */
#define GUARD_DC_STIFF    1.0f    /* constraint stiffness (full) */
#define GUARD_DC_HALF     0.5f    /* constraint stiffness (half) */
#define GUARD_STATIC_K    50.0f   /* stiff spring for static test */
#define GUARD_STATIC_SEP  3.0f    /* separation for static test */
#define GUARD_ITERS       5       /* solver iterations */

/* ══════════════════════════════════════════════════════════════════════════
 * 28. NULL pointer safety
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_spring_apply_null_spring(void)
{
    TEST("spring_apply — NULL spring: no crash");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(GUARD_POS_NEAR, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);

    forge_physics_spring_apply(NULL, particles, 2);

    /* Should return silently — no force applied */
    ASSERT_NEAR(particles[0].force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_spring_apply_null_particles(void)
{
    TEST("spring_apply — NULL particles: no crash");
    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, GUARD_REST, GUARD_K, 0.0f);
    forge_physics_spring_apply(&s, NULL, 2);
    /* Reached: no crash from NULL particles. */
    END_TEST();
}

static void test_spring_apply_zero_count(void)
{
    TEST("spring_apply — zero count: no crash");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(GUARD_POS_NEAR, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, GUARD_REST, GUARD_K, 0.0f);
    forge_physics_spring_apply(&s, particles, 0);

    /* Should return silently — no force applied */
    ASSERT_NEAR(particles[0].force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_constraint_solve_null_constraint(void)
{
    TEST("constraint_solve_distance — NULL constraint: no crash");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(GUARD_POS_FAR, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);

    forge_physics_constraint_solve_distance(NULL, particles, 2);

    /* Positions unchanged */
    ASSERT_NEAR(particles[0].position.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].position.x, GUARD_POS_FAR, EPSILON);
    END_TEST();
}

static void test_constraint_solve_null_particles(void)
{
    TEST("constraint_solve_distance — NULL particles: no crash");
    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(
            0, 1, GUARD_DC_DIST, GUARD_DC_STIFF);
    forge_physics_constraint_solve_distance(&c, NULL, 2);
    /* Reached: no crash from NULL particles. */
    END_TEST();
}

static void test_constraints_solve_null_constraints(void)
{
    TEST("constraints_solve — NULL constraints: no crash");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(GUARD_POS_FAR, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);

    forge_physics_constraints_solve(NULL, 2, particles, 2, GUARD_ITERS);

    /* Positions unchanged */
    ASSERT_NEAR(particles[0].position.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].position.x, GUARD_POS_FAR, EPSILON);
    END_TEST();
}

static void test_constraints_solve_null_particles(void)
{
    TEST("constraints_solve — NULL particles: no crash");
    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(
            0, 1, GUARD_DC_DIST, GUARD_DC_STIFF);
    forge_physics_constraints_solve(&c, 1, NULL, 2, GUARD_ITERS);
    /* Reached: no crash from NULL particles. */
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 29. Both-static early returns
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_spring_both_static_no_force(void)
{
    TEST("spring_apply — both particles static: no force");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f);  /* static */
    particles[1] = forge_physics_particle_create(
        vec3_create(GUARD_STATIC_SEP, 0.0f, 0.0f),
        0.0f, 0.0f, 0.0f, 0.0f);  /* static */

    ForgePhysicsSpring s = forge_physics_spring_create(
        0, 1, GUARD_REST, GUARD_STATIC_K, GUARD_MASS);
    forge_physics_spring_apply(&s, particles, 2);

    /* Both static — forces must remain zero */
    ASSERT_NEAR(particles[0].force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[0].force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].force_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_constraint_both_static_no_movement(void)
{
    TEST("constraint_solve_distance — both static: positions unchanged");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f);  /* static */
    particles[1] = forge_physics_particle_create(
        vec3_create(GUARD_POS_ANCHOR, 0.0f, 0.0f),
        0.0f, 0.0f, 0.0f, 0.0f);  /* static */

    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(
            0, 1, GUARD_DC_DIST, GUARD_DC_STIFF);
    forge_physics_constraint_solve_distance(&c, particles, 2);

    /* Both static — positions must not move */
    ASSERT_NEAR(particles[0].position.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].position.x, GUARD_POS_ANCHOR, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 30. NaN guard — constraint_distance_create
 * ══════════════════════════════════════════════════════════════════════════ */

#define NAN_VALUE NAN  /* from <math.h> — portable NaN constant */

static void test_constraint_create_nan_stiffness(void)
{
    TEST("constraint_distance_create — NaN stiffness clamped to 0");
    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(
            0, 1, GUARD_DC_DIST, NAN_VALUE);

    /* NaN stiffness should be clamped to 0.0f */
    ASSERT_NEAR(c.stiffness, 0.0f, EPSILON);
    /* Must not be NaN */
    ASSERT_TRUE(c.stiffness == c.stiffness);  /* NaN != NaN */
    END_TEST();
}

static void test_constraint_create_nan_distance(void)
{
    TEST("constraint_distance_create — NaN distance clamped to 0");
    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(
            0, 1, NAN_VALUE, GUARD_DC_HALF);

    /* NaN distance: !(NaN > 0.0f) is true, so distance = 0.0f */
    ASSERT_NEAR(c.distance, 0.0f, EPSILON);
    ASSERT_TRUE(c.distance == c.distance);  /* Not NaN */
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 31. Solver iteration clamping
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_constraints_solve_zero_constraints(void)
{
    TEST("constraints_solve — zero constraints: no crash");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(GUARD_POS_FAR, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);

    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(
            0, 1, GUARD_DC_DIST, GUARD_DC_STIFF);
    forge_physics_constraints_solve(&c, 0, particles, 2, GUARD_ITERS);

    /* Zero constraints — positions unchanged */
    ASSERT_NEAR(particles[0].position.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].position.x, GUARD_POS_FAR, EPSILON);
    END_TEST();
}

static void test_constraints_solve_zero_particles(void)
{
    TEST("constraints_solve — zero particles: no crash");
    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(
            0, 1, GUARD_DC_DIST, GUARD_DC_STIFF);
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(GUARD_POS_FAR, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);

    forge_physics_constraints_solve(&c, 1, particles, 0, GUARD_ITERS);

    /* Zero particles — positions unchanged (bounds check fails) */
    ASSERT_NEAR(particles[0].position.x, 0.0f, EPSILON);
    END_TEST();
}

#define GUARD_NEG_ITERS  (-5)     /* negative iteration count for clamping test */

static void test_constraints_solve_negative_iterations(void)
{
    TEST("constraints_solve — negative iterations clamped to minimum");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);
    particles[1] = forge_physics_particle_create(
        vec3_create(GUARD_POS_FAR, 0.0f, 0.0f), GUARD_MASS, 0.0f, 0.0f, 0.0f);

    ForgePhysicsDistanceConstraint c =
        forge_physics_constraint_distance_create(
            0, 1, GUARD_DC_DIST, GUARD_DC_STIFF);

    /* Negative iterations should be clamped to 1, still converge somewhat */
    forge_physics_constraints_solve(&c, 1, particles, 2, GUARD_NEG_ITERS);

    /* With 1 iteration and stiffness=1, full correction:
     * particles move toward target distance from GUARD_POS_FAR apart */
    float dist = SDL_fabsf(particles[1].position.x - particles[0].position.x);
    ASSERT_NEAR(dist, GUARD_DC_DIST, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 32. Sphere-sphere detection
 * ══════════════════════════════════════════════════════════════════════════ */

/* Overlapping spheres test */
#define SS_OVERLAP_A_X        0.0f
#define SS_OVERLAP_B_X        0.8f
#define SS_OVERLAP_RADIUS     0.5f
#define SS_OVERLAP_PEN        0.2f       /* sum_radii(1.0) - dist(0.8) */
#define SS_OVERLAP_CONTACT_X  0.4f       /* midpoint along contact axis */
#define SS_OVERLAP_NORMAL_X   (-1.0f)    /* from B toward A */
#define SS_OVERLAP_MASS       1.0f

/* Touching spheres test */
#define SS_TOUCH_A_X          0.0f
#define SS_TOUCH_B_X          1.0f       /* exactly sum of radii */
#define SS_TOUCH_RADIUS       0.5f
#define SS_TOUCH_MASS         1.0f

/* Separated spheres test */
#define SS_SEP_A_X            0.0f
#define SS_SEP_B_X            5.0f
#define SS_SEP_RADIUS         0.5f
#define SS_SEP_MASS           1.0f

/* Coincident spheres test */
#define SS_COIN_RADIUS        0.5f
#define SS_COIN_MASS          1.0f
#define SS_COIN_PEN           1.0f       /* sum_radii when dist=0 */
#define SS_COIN_NORMAL_Y      1.0f       /* arbitrary fallback normal */

/* Zero radius test */
#define SS_ZERO_R_A_X         0.0f
#define SS_ZERO_R_B_X         0.3f
#define SS_ZERO_R_RADIUS_A    0.0f
#define SS_ZERO_R_RADIUS_B    0.5f
#define SS_ZERO_R_MASS        1.0f

/* Both static test */
#define SS_BSTAT_A_X          0.0f
#define SS_BSTAT_B_X          0.5f
#define SS_BSTAT_RADIUS       0.5f

/* One static test */
#define SS_OSTAT_A_X          0.0f
#define SS_OSTAT_B_X          0.8f
#define SS_OSTAT_RADIUS       0.5f
#define SS_OSTAT_MASS         1.0f

/* Asymmetric radii test */
#define SS_ASYM_A_X           0.0f
#define SS_ASYM_B_X           0.8f
#define SS_ASYM_RADIUS_A      0.3f
#define SS_ASYM_RADIUS_B      0.7f
#define SS_ASYM_MASS          1.0f
#define SS_ASYM_PEN           0.2f       /* sum_radii(1.0) - dist(0.8) */
/* Contact point: b_pos + normal * (r_b - pen/2) = 0.8 + (-1)(0.7 - 0.1) = 0.2 */
#define SS_ASYM_CONTACT_X     0.2f       /* b_x + normal * (r_b - pen/2) */

static void test_collide_sphere_overlapping(void)
{
    TEST("collide_sphere_sphere — overlapping spheres");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(SS_OVERLAP_A_X, 0.0f, 0.0f),
        SS_OVERLAP_MASS, 0.0f, DEFAULT_RESTIT, SS_OVERLAP_RADIUS);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(SS_OVERLAP_B_X, 0.0f, 0.0f),
        SS_OVERLAP_MASS, 0.0f, DEFAULT_RESTIT, SS_OVERLAP_RADIUS);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(hit);
    ASSERT_NEAR(contact.penetration, SS_OVERLAP_PEN, EPSILON);
    ASSERT_NEAR(contact.normal.x, SS_OVERLAP_NORMAL_X, EPSILON);
    ASSERT_NEAR(contact.normal.y, 0.0f, EPSILON);
    ASSERT_NEAR(contact.normal.z, 0.0f, EPSILON);
    ASSERT_NEAR(contact.point.x, SS_OVERLAP_CONTACT_X, EPSILON);
    ASSERT_NEAR(contact.point.y, 0.0f, EPSILON);
    ASSERT_NEAR(contact.point.z, 0.0f, EPSILON);
    ASSERT_TRUE(contact.particle_a == 0);
    ASSERT_TRUE(contact.particle_b == 1);
    END_TEST();
}

static void test_collide_sphere_touching(void)
{
    TEST("collide_sphere_sphere — exactly touching (no overlap)");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(SS_TOUCH_A_X, 0.0f, 0.0f),
        SS_TOUCH_MASS, 0.0f, DEFAULT_RESTIT, SS_TOUCH_RADIUS);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(SS_TOUCH_B_X, 0.0f, 0.0f),
        SS_TOUCH_MASS, 0.0f, DEFAULT_RESTIT, SS_TOUCH_RADIUS);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(!hit);
    END_TEST();
}

static void test_collide_sphere_separated(void)
{
    TEST("collide_sphere_sphere — separated spheres");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(SS_SEP_A_X, 0.0f, 0.0f),
        SS_SEP_MASS, 0.0f, DEFAULT_RESTIT, SS_SEP_RADIUS);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(SS_SEP_B_X, 0.0f, 0.0f),
        SS_SEP_MASS, 0.0f, DEFAULT_RESTIT, SS_SEP_RADIUS);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(!hit);
    END_TEST();
}

static void test_collide_sphere_coincident(void)
{
    TEST("collide_sphere_sphere — coincident (same position)");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        SS_COIN_MASS, 0.0f, DEFAULT_RESTIT, SS_COIN_RADIUS);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        SS_COIN_MASS, 0.0f, DEFAULT_RESTIT, SS_COIN_RADIUS);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(hit);
    ASSERT_NEAR(contact.penetration, SS_COIN_PEN, EPSILON);
    /* Arbitrary fallback normal — expect (0,1,0) */
    ASSERT_NEAR(contact.normal.y, SS_COIN_NORMAL_Y, EPSILON);
    END_TEST();
}

static void test_collide_sphere_zero_radius(void)
{
    TEST("collide_sphere_sphere — one sphere has zero radius");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(SS_ZERO_R_A_X, 0.0f, 0.0f),
        SS_ZERO_R_MASS, 0.0f, DEFAULT_RESTIT, SS_ZERO_R_RADIUS_A);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(SS_ZERO_R_B_X, 0.0f, 0.0f),
        SS_ZERO_R_MASS, 0.0f, DEFAULT_RESTIT, SS_ZERO_R_RADIUS_B);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(!hit);
    END_TEST();
}

static void test_collide_sphere_both_static(void)
{
    TEST("collide_sphere_sphere — both static (mass=0)");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(SS_BSTAT_A_X, 0.0f, 0.0f),
        0.0f, 0.0f, DEFAULT_RESTIT, SS_BSTAT_RADIUS);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(SS_BSTAT_B_X, 0.0f, 0.0f),
        0.0f, 0.0f, DEFAULT_RESTIT, SS_BSTAT_RADIUS);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(!hit);
    END_TEST();
}

static void test_collide_sphere_one_static(void)
{
    TEST("collide_sphere_sphere — one static, one dynamic");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(SS_OSTAT_A_X, 0.0f, 0.0f),
        0.0f, 0.0f, DEFAULT_RESTIT, SS_OSTAT_RADIUS);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(SS_OSTAT_B_X, 0.0f, 0.0f),
        SS_OSTAT_MASS, 0.0f, DEFAULT_RESTIT, SS_OSTAT_RADIUS);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(hit);
    END_TEST();
}

static void test_collide_sphere_asymmetric_radii(void)
{
    TEST("collide_sphere_sphere — asymmetric radii");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(SS_ASYM_A_X, 0.0f, 0.0f),
        SS_ASYM_MASS, 0.0f, DEFAULT_RESTIT, SS_ASYM_RADIUS_A);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(SS_ASYM_B_X, 0.0f, 0.0f),
        SS_ASYM_MASS, 0.0f, DEFAULT_RESTIT, SS_ASYM_RADIUS_B);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(hit);
    ASSERT_NEAR(contact.penetration, SS_ASYM_PEN, EPSILON);
    /* Contact point lies on the line between centers */
    ASSERT_NEAR(contact.point.x, SS_ASYM_CONTACT_X, EPSILON);
    ASSERT_NEAR(contact.point.y, 0.0f, EPSILON);
    ASSERT_NEAR(contact.point.z, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 33. Impulse response
 * ══════════════════════════════════════════════════════════════════════════ */

/* Equal mass elastic test */
#define IR_EQUAL_MASS         1.0f
#define IR_EQUAL_VEL          1.0f       /* ±1 m/s head-on */
#define IR_EQUAL_RESTIT       1.0f
#define IR_EQUAL_RADIUS       0.5f
#define IR_EQUAL_OFFSET       0.8f       /* < sum_radii so they overlap */

/* Unequal mass test */
#define IR_UNEQ_MASS_A        1.0f
#define IR_UNEQ_MASS_B        3.0f
#define IR_UNEQ_VEL_A         2.0f
#define IR_UNEQ_RESTIT        1.0f
#define IR_UNEQ_RADIUS        0.5f
#define IR_UNEQ_OFFSET        0.8f
/* Momentum: 1*2 + 3*0 = 2. Post: v_a = (m_a-m_b)/(m_a+m_b)*v_a = -1, v_b = 2*m_a/(m_a+m_b)*v_a = 1 */
#define IR_UNEQ_EXPECTED_VA   (-1.0f)
#define IR_UNEQ_EXPECTED_VB   1.0f

/* One static test */
#define IR_STAT_MASS          1.0f
#define IR_STAT_VEL           (-2.0f)
#define IR_STAT_RESTIT        1.0f
#define IR_STAT_RADIUS        0.5f
#define IR_STAT_OFFSET        0.8f

/* Zero restitution test */
#define IR_INELASTIC_MASS     1.0f
#define IR_INELASTIC_VEL      2.0f
#define IR_INELASTIC_RESTIT   0.0f
#define IR_INELASTIC_RADIUS   0.5f
#define IR_INELASTIC_OFFSET   0.8f

/* Separating pair test */
#define IR_SEP_MASS           1.0f
#define IR_SEP_VEL            1.0f       /* moving apart */
#define IR_SEP_RESTIT         1.0f
#define IR_SEP_RADIUS         0.5f
#define IR_SEP_OFFSET         0.8f

static void test_resolve_equal_mass_elastic(void)
{
    TEST("resolve_contact — equal mass elastic, velocities swap");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        IR_EQUAL_MASS, 0.0f, IR_EQUAL_RESTIT, IR_EQUAL_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(IR_EQUAL_OFFSET, 0.0f, 0.0f),
        IR_EQUAL_MASS, 0.0f, IR_EQUAL_RESTIT, IR_EQUAL_RADIUS);

    particles[0].velocity = vec3_create(IR_EQUAL_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(-IR_EQUAL_VEL, 0.0f, 0.0f);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    forge_physics_resolve_contact(&contact, particles, 2);

    /* Velocities should swap after elastic collision */
    ASSERT_NEAR(particles[0].velocity.x, -IR_EQUAL_VEL, EPSILON);
    ASSERT_NEAR(particles[1].velocity.x, IR_EQUAL_VEL, EPSILON);
    END_TEST();
}

static void test_resolve_unequal_mass(void)
{
    TEST("resolve_contact — unequal mass, momentum conserved");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        IR_UNEQ_MASS_A, 0.0f, IR_UNEQ_RESTIT, IR_UNEQ_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(IR_UNEQ_OFFSET, 0.0f, 0.0f),
        IR_UNEQ_MASS_B, 0.0f, IR_UNEQ_RESTIT, IR_UNEQ_RADIUS);

    particles[0].velocity = vec3_create(IR_UNEQ_VEL_A, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(0.0f, 0.0f, 0.0f);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    float mom_before = IR_UNEQ_MASS_A * particles[0].velocity.x
                     + IR_UNEQ_MASS_B * particles[1].velocity.x;

    forge_physics_resolve_contact(&contact, particles, 2);

    float mom_after = IR_UNEQ_MASS_A * particles[0].velocity.x
                    + IR_UNEQ_MASS_B * particles[1].velocity.x;

    ASSERT_NEAR(mom_after, mom_before, EPSILON);
    ASSERT_NEAR(particles[0].velocity.x, IR_UNEQ_EXPECTED_VA, EPSILON);
    ASSERT_NEAR(particles[1].velocity.x, IR_UNEQ_EXPECTED_VB, EPSILON);
    END_TEST();
}

static void test_resolve_one_static(void)
{
    TEST("resolve_contact — static particle, dynamic bounces");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        0.0f, 0.0f, IR_STAT_RESTIT, IR_STAT_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(IR_STAT_OFFSET, 0.0f, 0.0f),
        IR_STAT_MASS, 0.0f, IR_STAT_RESTIT, IR_STAT_RADIUS);

    particles[1].velocity = vec3_create(IR_STAT_VEL, 0.0f, 0.0f);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    forge_physics_resolve_contact(&contact, particles, 2);

    /* Dynamic particle should bounce away (positive x after hitting static at origin) */
    ASSERT_TRUE(particles[1].velocity.x > 0.0f);
    /* Static particle velocity unchanged */
    ASSERT_NEAR(particles[0].velocity.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_resolve_zero_restitution(void)
{
    TEST("resolve_contact — zero restitution, no relative velocity after");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        IR_INELASTIC_MASS, 0.0f, IR_INELASTIC_RESTIT, IR_INELASTIC_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(IR_INELASTIC_OFFSET, 0.0f, 0.0f),
        IR_INELASTIC_MASS, 0.0f, IR_INELASTIC_RESTIT, IR_INELASTIC_RADIUS);

    particles[0].velocity = vec3_create(IR_INELASTIC_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(-IR_INELASTIC_VEL, 0.0f, 0.0f);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    forge_physics_resolve_contact(&contact, particles, 2);

    /* No relative velocity along normal after perfectly inelastic collision */
    float rel_vel = particles[0].velocity.x - particles[1].velocity.x;
    ASSERT_NEAR(rel_vel, 0.0f, EPSILON);
    END_TEST();
}

static void test_resolve_separating_pair(void)
{
    TEST("resolve_contact — already separating, velocities unchanged");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        IR_SEP_MASS, 0.0f, IR_SEP_RESTIT, IR_SEP_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(IR_SEP_OFFSET, 0.0f, 0.0f),
        IR_SEP_MASS, 0.0f, IR_SEP_RESTIT, IR_SEP_RADIUS);

    /* Moving apart: a goes left, b goes right */
    particles[0].velocity = vec3_create(-IR_SEP_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(IR_SEP_VEL, 0.0f, 0.0f);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    float va_before = particles[0].velocity.x;
    float vb_before = particles[1].velocity.x;

    forge_physics_resolve_contact(&contact, particles, 2);

    /* Already separating — no impulse applied */
    ASSERT_NEAR(particles[0].velocity.x, va_before, EPSILON);
    ASSERT_NEAR(particles[1].velocity.x, vb_before, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 34. Conservation laws
 * ══════════════════════════════════════════════════════════════════════════ */

#define CL_MASS               1.0f
#define CL_VEL                3.0f
#define CL_RESTIT_ELASTIC     1.0f
#define CL_RESTIT_PARTIAL     0.5f
#define CL_RADIUS             0.5f
#define CL_OFFSET             0.8f

static void test_collision_momentum_conservation(void)
{
    TEST("conservation — total momentum preserved");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        CL_MASS, 0.0f, CL_RESTIT_ELASTIC, CL_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(CL_OFFSET, 0.0f, 0.0f),
        CL_MASS, 0.0f, CL_RESTIT_ELASTIC, CL_RADIUS);

    particles[0].velocity = vec3_create(CL_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(-CL_VEL, 0.0f, 0.0f);

    float mom_before = CL_MASS * particles[0].velocity.x
                     + CL_MASS * particles[1].velocity.x;

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    forge_physics_resolve_contact(&contact, particles, 2);

    float mom_after = CL_MASS * particles[0].velocity.x
                    + CL_MASS * particles[1].velocity.x;

    ASSERT_NEAR(mom_after, mom_before, EPSILON);
    END_TEST();
}

static void test_collision_energy_elastic(void)
{
    TEST("conservation — kinetic energy preserved (e=1.0)");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        CL_MASS, 0.0f, CL_RESTIT_ELASTIC, CL_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(CL_OFFSET, 0.0f, 0.0f),
        CL_MASS, 0.0f, CL_RESTIT_ELASTIC, CL_RADIUS);

    particles[0].velocity = vec3_create(CL_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(0.0f, 0.0f, 0.0f);

    float ke_before = 0.5f * CL_MASS * CL_VEL * CL_VEL;

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    forge_physics_resolve_contact(&contact, particles, 2);

    float v0 = vec3_length(particles[0].velocity);
    float v1 = vec3_length(particles[1].velocity);
    float ke_after = 0.5f * CL_MASS * v0 * v0
                   + 0.5f * CL_MASS * v1 * v1;

    ASSERT_NEAR(ke_after, ke_before, EPSILON);
    END_TEST();
}

static void test_collision_energy_inelastic(void)
{
    TEST("conservation — kinetic energy decreases (e=0.5)");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        CL_MASS, 0.0f, CL_RESTIT_PARTIAL, CL_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(CL_OFFSET, 0.0f, 0.0f),
        CL_MASS, 0.0f, CL_RESTIT_PARTIAL, CL_RADIUS);

    particles[0].velocity = vec3_create(CL_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(0.0f, 0.0f, 0.0f);

    float v0_pre = vec3_length(particles[0].velocity);
    float v1_pre = vec3_length(particles[1].velocity);
    float ke_before = 0.5f * CL_MASS * v0_pre * v0_pre
                    + 0.5f * CL_MASS * v1_pre * v1_pre;

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    forge_physics_resolve_contact(&contact, particles, 2);

    float v0_post = vec3_length(particles[0].velocity);
    float v1_post = vec3_length(particles[1].velocity);
    float ke_after = 0.5f * CL_MASS * v0_post * v0_post
                   + 0.5f * CL_MASS * v1_post * v1_post;

    ASSERT_TRUE(ke_after < ke_before);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 35. All-pairs detection
 * ══════════════════════════════════════════════════════════════════════════ */

#define AP_MASS               1.0f
#define AP_RADIUS             0.5f
#define AP_NUM_PARTICLES      4
#define AP_EXPECTED_PAIRS     2
/* Particle positions: (0,0,0), (0.8,0,0), (5,0,0), (5.8,0,0)
 * Pairs 0-1 and 2-3 overlap; all others are separated */
#define AP_POS_0_X            0.0f
#define AP_POS_1_X            0.8f
#define AP_POS_2_X            5.0f
#define AP_POS_3_X            5.8f

static void test_all_pairs_correct_count(void)
{
    TEST("collide_particles_all — correct contact count");
    ForgePhysicsParticle particles[AP_NUM_PARTICLES];
    particles[0] = forge_physics_particle_create(
        vec3_create(AP_POS_0_X, 0.0f, 0.0f),
        AP_MASS, 0.0f, DEFAULT_RESTIT, AP_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(AP_POS_1_X, 0.0f, 0.0f),
        AP_MASS, 0.0f, DEFAULT_RESTIT, AP_RADIUS);
    particles[2] = forge_physics_particle_create(
        vec3_create(AP_POS_2_X, 0.0f, 0.0f),
        AP_MASS, 0.0f, DEFAULT_RESTIT, AP_RADIUS);
    particles[3] = forge_physics_particle_create(
        vec3_create(AP_POS_3_X, 0.0f, 0.0f),
        AP_MASS, 0.0f, DEFAULT_RESTIT, AP_RADIUS);

    ForgePhysicsContact contacts[FORGE_PHYSICS_MAX_CONTACTS];
    int count = forge_physics_collide_particles_all(
        particles, AP_NUM_PARTICLES,
        contacts, FORGE_PHYSICS_MAX_CONTACTS);

    ASSERT_TRUE(count == AP_EXPECTED_PAIRS);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 36. Determinism
 * ══════════════════════════════════════════════════════════════════════════ */

#define CD_MASS               1.0f
#define CD_VEL                2.0f
#define CD_RESTIT             0.8f
#define CD_RADIUS             0.5f
#define CD_OFFSET             0.8f

static void test_collision_determinism(void)
{
    TEST("collision determinism — identical results on repeat");

    /* First run */
    ForgePhysicsParticle p1[2];
    p1[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        CD_MASS, 0.0f, CD_RESTIT, CD_RADIUS);
    p1[1] = forge_physics_particle_create(
        vec3_create(CD_OFFSET, 0.0f, 0.0f),
        CD_MASS, 0.0f, CD_RESTIT, CD_RADIUS);
    p1[0].velocity = vec3_create(CD_VEL, 0.0f, 0.0f);
    p1[1].velocity = vec3_create(-CD_VEL, 0.0f, 0.0f);

    ForgePhysicsContact c1;
    bool hit1 = forge_physics_collide_sphere_sphere(&p1[0], &p1[1], 0, 1, &c1);
    ASSERT_TRUE(hit1);
    forge_physics_resolve_contact(&c1, p1, 2);

    /* Second run — identical setup */
    ForgePhysicsParticle p2[2];
    p2[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        CD_MASS, 0.0f, CD_RESTIT, CD_RADIUS);
    p2[1] = forge_physics_particle_create(
        vec3_create(CD_OFFSET, 0.0f, 0.0f),
        CD_MASS, 0.0f, CD_RESTIT, CD_RADIUS);
    p2[0].velocity = vec3_create(CD_VEL, 0.0f, 0.0f);
    p2[1].velocity = vec3_create(-CD_VEL, 0.0f, 0.0f);

    ForgePhysicsContact c2;
    bool hit2 = forge_physics_collide_sphere_sphere(&p2[0], &p2[1], 0, 1, &c2);
    ASSERT_TRUE(hit2);
    forge_physics_resolve_contact(&c2, p2, 2);

    /* Results must match exactly */
    ASSERT_NEAR(p1[0].velocity.x, p2[0].velocity.x, 0.0f);
    ASSERT_NEAR(p1[0].velocity.y, p2[0].velocity.y, 0.0f);
    ASSERT_NEAR(p1[0].velocity.z, p2[0].velocity.z, 0.0f);
    ASSERT_NEAR(p1[1].velocity.x, p2[1].velocity.x, 0.0f);
    ASSERT_NEAR(p1[1].velocity.y, p2[1].velocity.y, 0.0f);
    ASSERT_NEAR(p1[1].velocity.z, p2[1].velocity.z, 0.0f);
    ASSERT_NEAR(c1.penetration, c2.penetration, 0.0f);
    ASSERT_NEAR(c1.normal.x, c2.normal.x, 0.0f);
    ASSERT_NEAR(c1.normal.y, c2.normal.y, 0.0f);
    ASSERT_NEAR(c1.normal.z, c2.normal.z, 0.0f);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 37. All-pairs boundary conditions
 * ══════════════════════════════════════════════════════════════════════════ */

/* Cluster of 3 particles all within overlap distance */
#define AP_CLUSTER_RADIUS       0.5f
#define AP_CLUSTER_MASS         1.0f
#define AP_CLUSTER_OFFSET       0.4f   /* < 2 * radius, so all pairs overlap */
#define AP_CLUSTER_NUM          3
#define AP_CLUSTER_PAIRS        3      /* C(3,2) = 3 */

/* Well-separated positions for no-overlap test */
#define AP_NOSEP_RADIUS         0.5f
#define AP_NOSEP_MASS           1.0f
#define AP_NOSEP_NUM            4
#define AP_NOSEP_SPACING        5.0f   /* >> 2 * radius */

/* Max contacts clamping test — 5 particles in a cluster yields C(5,2)=10 pairs */
#define AP_CLAMP_NUM            5
#define AP_CLAMP_OFFSET         0.1f   /* all overlapping */
#define AP_CLAMP_RADIUS         0.5f
#define AP_CLAMP_MASS           1.0f
#define AP_CLAMP_MAX            2      /* artificially low limit */

static void test_all_pairs_zero_particles(void)
{
    TEST("collide_particles_all — 0 particles returns 0");
    ForgePhysicsContact contacts[1];
    int count = forge_physics_collide_particles_all(NULL, 0, contacts, 1);
    ASSERT_TRUE(count == 0);
    END_TEST();
}

static void test_all_pairs_single_particle(void)
{
    TEST("collide_particles_all — 1 particle returns 0 (no pairs)");
    ForgePhysicsParticle p = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        AP_CLUSTER_MASS, 0.0f, DEFAULT_RESTIT, AP_CLUSTER_RADIUS);
    ForgePhysicsContact contacts[1];
    int count = forge_physics_collide_particles_all(&p, 1, contacts, 1);
    ASSERT_TRUE(count == 0);
    END_TEST();
}

static void test_all_pairs_no_overlaps(void)
{
    TEST("collide_particles_all — 4 separated particles returns 0");
    ForgePhysicsParticle particles[AP_NOSEP_NUM];
    for (int i = 0; i < AP_NOSEP_NUM; i++) {
        particles[i] = forge_physics_particle_create(
            vec3_create((float)i * AP_NOSEP_SPACING, 0.0f, 0.0f),
            AP_NOSEP_MASS, 0.0f, DEFAULT_RESTIT, AP_NOSEP_RADIUS);
    }
    ForgePhysicsContact contacts[FORGE_PHYSICS_MAX_CONTACTS];
    int count = forge_physics_collide_particles_all(
        particles, AP_NOSEP_NUM, contacts, FORGE_PHYSICS_MAX_CONTACTS);
    ASSERT_TRUE(count == 0);
    END_TEST();
}

static void test_all_pairs_all_overlap(void)
{
    TEST("collide_particles_all — 3 close particles returns 3 contacts");
    ForgePhysicsParticle particles[AP_CLUSTER_NUM];
    for (int i = 0; i < AP_CLUSTER_NUM; i++) {
        particles[i] = forge_physics_particle_create(
            vec3_create((float)i * AP_CLUSTER_OFFSET, 0.0f, 0.0f),
            AP_CLUSTER_MASS, 0.0f, DEFAULT_RESTIT, AP_CLUSTER_RADIUS);
    }
    ForgePhysicsContact contacts[FORGE_PHYSICS_MAX_CONTACTS];
    int count = forge_physics_collide_particles_all(
        particles, AP_CLUSTER_NUM, contacts, FORGE_PHYSICS_MAX_CONTACTS);
    ASSERT_TRUE(count == AP_CLUSTER_PAIRS);
    END_TEST();
}

static void test_all_pairs_exceeds_max_contacts(void)
{
    TEST("collide_particles_all — max_contacts caps output");
    ForgePhysicsParticle particles[AP_CLAMP_NUM];
    for (int i = 0; i < AP_CLAMP_NUM; i++) {
        particles[i] = forge_physics_particle_create(
            vec3_create((float)i * AP_CLAMP_OFFSET, 0.0f, 0.0f),
            AP_CLAMP_MASS, 0.0f, DEFAULT_RESTIT, AP_CLAMP_RADIUS);
    }
    ForgePhysicsContact contacts[AP_CLAMP_MAX];
    int count = forge_physics_collide_particles_all(
        particles, AP_CLAMP_NUM, contacts, AP_CLAMP_MAX);
    ASSERT_TRUE(count == AP_CLAMP_MAX);
    END_TEST();
}

static void test_all_pairs_contact_ordering(void)
{
    TEST("collide_particles_all — contacts in pair order (0,1),(0,2),(1,2)");
    ForgePhysicsParticle particles[AP_CLUSTER_NUM];
    for (int i = 0; i < AP_CLUSTER_NUM; i++) {
        particles[i] = forge_physics_particle_create(
            vec3_create((float)i * AP_CLUSTER_OFFSET, 0.0f, 0.0f),
            AP_CLUSTER_MASS, 0.0f, DEFAULT_RESTIT, AP_CLUSTER_RADIUS);
    }
    ForgePhysicsContact contacts[FORGE_PHYSICS_MAX_CONTACTS];
    int count = forge_physics_collide_particles_all(
        particles, AP_CLUSTER_NUM, contacts, FORGE_PHYSICS_MAX_CONTACTS);
    ASSERT_TRUE(count == AP_CLUSTER_PAIRS);
    /* Order must follow the nested i,j loop: (0,1), (0,2), (1,2) */
    ASSERT_TRUE(contacts[0].particle_a == 0 && contacts[0].particle_b == 1);
    ASSERT_TRUE(contacts[1].particle_a == 0 && contacts[1].particle_b == 2);
    ASSERT_TRUE(contacts[2].particle_a == 1 && contacts[2].particle_b == 2);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 38. 3D geometry (non-axis-aligned)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Diagonal overlap: (0,0,0) and (0.5,0.5,0), radii 0.5 each.
 * Distance = sqrt(0.25+0.25) ≈ 0.7071. Sum radii = 1.0.
 * Penetration = 1.0 - 0.7071 ≈ 0.2929.
 * Normal from B toward A = (-0.5, -0.5, 0) / 0.7071 ≈ (-0.7071, -0.7071, 0). */
#define SS3D_DIAG_B_X          0.5f
#define SS3D_DIAG_B_Y          0.5f
#define SS3D_DIAG_RADIUS       0.5f
#define SS3D_DIAG_MASS         1.0f
#define SS3D_DIAG_DIST         0.70710678f  /* sqrt(0.5) */
#define SS3D_DIAG_PEN          0.29289322f  /* 1.0 - sqrt(0.5) */
#define SS3D_DIAG_NORM         (-0.70710678f) /* each component of normal */
#define SS3D_DIAG_TOL          0.01f

/* Z-axis overlap: (0,0,0) and (0,0,0.8), radii 0.5 each.
 * Distance = 0.8. Penetration = 0.2. Normal = (0,0,-1). */
#define SS3D_Z_B_Z             0.8f
#define SS3D_Z_RADIUS          0.5f
#define SS3D_Z_MASS            1.0f
#define SS3D_Z_PEN             0.2f
#define SS3D_Z_NORM_Z          (-1.0f)

/* 3D momentum: two particles approaching along the XY diagonal */
#define SS3D_MOM_MASS          1.0f
#define SS3D_MOM_RADIUS        0.5f
#define SS3D_MOM_RESTIT        1.0f
#define SS3D_MOM_VEL           2.0f  /* each component */

/* 3D contact point midpoint test */
#define SS3D_MID_RADIUS_A      0.5f
#define SS3D_MID_RADIUS_B      0.5f
#define SS3D_MID_MASS          1.0f
#define SS3D_MID_B_X           0.5f
#define SS3D_MID_B_Y           0.5f

static void test_collide_sphere_diagonal(void)
{
    TEST("collide_sphere_sphere — diagonal overlap, 3D normal correct");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        SS3D_DIAG_MASS, 0.0f, DEFAULT_RESTIT, SS3D_DIAG_RADIUS);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(SS3D_DIAG_B_X, SS3D_DIAG_B_Y, 0.0f),
        SS3D_DIAG_MASS, 0.0f, DEFAULT_RESTIT, SS3D_DIAG_RADIUS);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(hit);
    ASSERT_NEAR(contact.penetration, SS3D_DIAG_PEN, SS3D_DIAG_TOL);
    /* Normal from B toward A: (-0.7071, -0.7071, 0) */
    ASSERT_NEAR(contact.normal.x, SS3D_DIAG_NORM, SS3D_DIAG_TOL);
    ASSERT_NEAR(contact.normal.y, SS3D_DIAG_NORM, SS3D_DIAG_TOL);
    ASSERT_NEAR(contact.normal.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_collide_sphere_z_axis(void)
{
    TEST("collide_sphere_sphere — Z-axis overlap, normal.z correct");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        SS3D_Z_MASS, 0.0f, DEFAULT_RESTIT, SS3D_Z_RADIUS);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, SS3D_Z_B_Z),
        SS3D_Z_MASS, 0.0f, DEFAULT_RESTIT, SS3D_Z_RADIUS);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(hit);
    ASSERT_NEAR(contact.penetration, SS3D_Z_PEN, EPSILON);
    ASSERT_NEAR(contact.normal.x, 0.0f, EPSILON);
    ASSERT_NEAR(contact.normal.y, 0.0f, EPSILON);
    ASSERT_NEAR(contact.normal.z, SS3D_Z_NORM_Z, EPSILON);
    END_TEST();
}

static void test_resolve_contact_3d(void)
{
    TEST("resolve_contact — 3D diagonal, momentum conserved in all components");
    ForgePhysicsParticle particles[2];
    /* Overlapping along the XY diagonal */
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        SS3D_MOM_MASS, 0.0f, SS3D_MOM_RESTIT, SS3D_MOM_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(SS3D_DIAG_B_X, SS3D_DIAG_B_Y, 0.0f),
        SS3D_MOM_MASS, 0.0f, SS3D_MOM_RESTIT, SS3D_MOM_RADIUS);

    /* Approaching head-on along the diagonal */
    particles[0].velocity = vec3_create(SS3D_MOM_VEL, SS3D_MOM_VEL, 0.0f);
    particles[1].velocity = vec3_create(-SS3D_MOM_VEL, -SS3D_MOM_VEL, 0.0f);

    float mom_x_before = SS3D_MOM_MASS * particles[0].velocity.x
                       + SS3D_MOM_MASS * particles[1].velocity.x;
    float mom_y_before = SS3D_MOM_MASS * particles[0].velocity.y
                       + SS3D_MOM_MASS * particles[1].velocity.y;
    float mom_z_before = SS3D_MOM_MASS * particles[0].velocity.z
                       + SS3D_MOM_MASS * particles[1].velocity.z;

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    forge_physics_resolve_contact(&contact, particles, 2);

    float mom_x_after = SS3D_MOM_MASS * particles[0].velocity.x
                      + SS3D_MOM_MASS * particles[1].velocity.x;
    float mom_y_after = SS3D_MOM_MASS * particles[0].velocity.y
                      + SS3D_MOM_MASS * particles[1].velocity.y;
    float mom_z_after = SS3D_MOM_MASS * particles[0].velocity.z
                      + SS3D_MOM_MASS * particles[1].velocity.z;

    ASSERT_NEAR(mom_x_after, mom_x_before, EPSILON);
    ASSERT_NEAR(mom_y_after, mom_y_before, EPSILON);
    ASSERT_NEAR(mom_z_after, mom_z_before, EPSILON);
    END_TEST();
}

static void test_contact_point_midpoint_3d(void)
{
    TEST("collide_sphere_sphere — 3D contact point geometrically correct");
    ForgePhysicsParticle a = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        SS3D_MID_MASS, 0.0f, DEFAULT_RESTIT, SS3D_MID_RADIUS_A);
    ForgePhysicsParticle b = forge_physics_particle_create(
        vec3_create(SS3D_MID_B_X, SS3D_MID_B_Y, 0.0f),
        SS3D_MID_MASS, 0.0f, DEFAULT_RESTIT, SS3D_MID_RADIUS_B);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(&a, &b, 0, 1, &contact);
    ASSERT_TRUE(hit);

    /* Contact point should lie on the line between centers.
     * Point = b.pos + normal * (r_b - pen/2).
     * Verify it lies between the two centers. */
    float cp_dist_from_a = vec3_length(vec3_sub(contact.point, a.position));
    float cp_dist_from_b = vec3_length(vec3_sub(contact.point, b.position));
    float center_dist = vec3_length(vec3_sub(a.position, b.position));

    /* Contact point must be between the two centers */
    ASSERT_TRUE(cp_dist_from_a <= center_dist + EPSILON);
    ASSERT_TRUE(cp_dist_from_b <= center_dist + EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 39. Positional correction
 * ══════════════════════════════════════════════════════════════════════════ */

#define PC_CORR_RADIUS         0.5f
#define PC_CORR_MASS           1.0f
#define PC_CORR_OFFSET         0.8f    /* overlap = 1.0 - 0.8 = 0.2 */
#define PC_CORR_PEN            0.2f    /* sum_radii - dist */
#define PC_CORR_HALF_PEN       0.1f    /* pen / 2 for equal mass */
#define PC_CORR_RESTIT         1.0f

/* Unequal mass: 1kg vs 3kg. inv_mass_sum = 1 + 1/3 = 4/3.
 * ratio_a = (1) / (4/3) = 3/4. ratio_b = (1/3) / (4/3) = 1/4.
 * Correction A = pen * 3/4 = 0.15. Correction B = pen * 1/4 = 0.05. */
#define PC_CORR_MASS_A         1.0f
#define PC_CORR_MASS_B         3.0f
#define PC_CORR_A_MOVE         0.15f   /* pen * 3/4 */
#define PC_CORR_B_MOVE         0.05f   /* pen * 1/4 */
#define PC_CORR_APPROACH_VEL   1.0f    /* approach speed to ensure collision */

static void test_resolve_position_correction_equal_mass(void)
{
    TEST("resolve_contact — equal mass: both move by penetration/2");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        PC_CORR_MASS, 0.0f, PC_CORR_RESTIT, PC_CORR_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(PC_CORR_OFFSET, 0.0f, 0.0f),
        PC_CORR_MASS, 0.0f, PC_CORR_RESTIT, PC_CORR_RADIUS);

    /* Head-on approach so impulse is applied (not separating) */
    particles[0].velocity = vec3_create(PC_CORR_APPROACH_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(-PC_CORR_APPROACH_VEL, 0.0f, 0.0f);

    float pos_a_before = particles[0].position.x;
    float pos_b_before = particles[1].position.x;

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);
    forge_physics_resolve_contact(&contact, particles, 2);

    /* A moves away from B (negative x direction) by pen/2 */
    float a_moved = SDL_fabsf(particles[0].position.x - pos_a_before);
    float b_moved = SDL_fabsf(particles[1].position.x - pos_b_before);
    ASSERT_NEAR(a_moved, PC_CORR_HALF_PEN, EPSILON);
    ASSERT_NEAR(b_moved, PC_CORR_HALF_PEN, EPSILON);
    END_TEST();
}

static void test_resolve_position_correction_one_static(void)
{
    TEST("resolve_contact — static + dynamic: only dynamic moves");
    ForgePhysicsParticle particles[2];
    /* Static particle at origin */
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        0.0f, 0.0f, PC_CORR_RESTIT, PC_CORR_RADIUS);
    /* Dynamic particle overlapping */
    particles[1] = forge_physics_particle_create(
        vec3_create(PC_CORR_OFFSET, 0.0f, 0.0f),
        PC_CORR_MASS, 0.0f, PC_CORR_RESTIT, PC_CORR_RADIUS);

    particles[1].velocity = vec3_create(-PC_CORR_APPROACH_VEL, 0.0f, 0.0f);

    float pos_static_before = particles[0].position.x;

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);
    forge_physics_resolve_contact(&contact, particles, 2);

    /* Static particle must not move */
    ASSERT_NEAR(particles[0].position.x, pos_static_before, EPSILON);
    /* Dynamic particle moves by the full penetration */
    float b_moved = SDL_fabsf(particles[1].position.x - PC_CORR_OFFSET);
    ASSERT_NEAR(b_moved, PC_CORR_PEN, EPSILON);
    END_TEST();
}

static void test_resolve_position_correction_unequal_mass(void)
{
    TEST("resolve_contact — unequal mass: mass-proportional correction");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        PC_CORR_MASS_A, 0.0f, PC_CORR_RESTIT, PC_CORR_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(PC_CORR_OFFSET, 0.0f, 0.0f),
        PC_CORR_MASS_B, 0.0f, PC_CORR_RESTIT, PC_CORR_RADIUS);

    particles[0].velocity = vec3_create(PC_CORR_APPROACH_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(-PC_CORR_APPROACH_VEL, 0.0f, 0.0f);

    float pos_a_before = particles[0].position.x;
    float pos_b_before = particles[1].position.x;

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);
    forge_physics_resolve_contact(&contact, particles, 2);

    /* Lighter (A, 1kg) should move more, heavier (B, 3kg) moves less */
    float a_moved = SDL_fabsf(particles[0].position.x - pos_a_before);
    float b_moved = SDL_fabsf(particles[1].position.x - pos_b_before);
    ASSERT_NEAR(a_moved, PC_CORR_A_MOVE, EPSILON);
    ASSERT_NEAR(b_moved, PC_CORR_B_MOVE, EPSILON);
    END_TEST();
}

#define PC_ZERO_OFFSET         1.0f    /* exactly touching, no overlap */
#define PC_ZERO_MIDPOINT       0.5f    /* midpoint between 0 and 1 */

static void test_resolve_no_overlap_no_correction(void)
{
    TEST("resolve_contact — zero penetration: no position change");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        PC_CORR_MASS, 0.0f, PC_CORR_RESTIT, PC_CORR_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(PC_ZERO_OFFSET, 0.0f, 0.0f),
        PC_CORR_MASS, 0.0f, PC_CORR_RESTIT, PC_CORR_RADIUS);

    /* Manually create a contact with zero penetration */
    ForgePhysicsContact contact;
    contact.particle_a  = 0;
    contact.particle_b  = 1;
    contact.normal      = vec3_create(-1.0f, 0.0f, 0.0f);
    contact.penetration = 0.0f;
    contact.point       = vec3_create(PC_ZERO_MIDPOINT, 0.0f, 0.0f);

    /* Approaching so impulse fires, but no positional correction */
    particles[0].velocity = vec3_create(PC_CORR_APPROACH_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(-PC_CORR_APPROACH_VEL, 0.0f, 0.0f);

    float pos_a_before = particles[0].position.x;
    float pos_b_before = particles[1].position.x;

    forge_physics_resolve_contact(&contact, particles, 2);

    /* Position must be unchanged (only velocities change) */
    ASSERT_NEAR(particles[0].position.x, pos_a_before, EPSILON);
    ASSERT_NEAR(particles[1].position.x, pos_b_before, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 40. Resting threshold
 * ══════════════════════════════════════════════════════════════════════════ */

/* Low velocity: v_closing must be below FORGE_PHYSICS_RESTING_THRESHOLD (0.5) */
#define RT_LOW_VEL             0.2f    /* well below 0.5 threshold */
#define RT_LOW_MASS            1.0f
#define RT_LOW_RADIUS          0.5f
#define RT_LOW_RESTIT          1.0f    /* would bounce if threshold not applied */
#define RT_LOW_OFFSET          0.8f

/* High velocity: v_closing well above threshold */
#define RT_HIGH_VEL            5.0f
#define RT_HIGH_MASS           1.0f
#define RT_HIGH_RADIUS         0.5f
#define RT_HIGH_RESTIT         1.0f
#define RT_HIGH_OFFSET         0.8f

static void test_resolve_low_velocity_no_bounce(void)
{
    TEST("resolve_contact — low velocity: restitution killed, no bounce");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        RT_LOW_MASS, 0.0f, RT_LOW_RESTIT, RT_LOW_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(RT_LOW_OFFSET, 0.0f, 0.0f),
        RT_LOW_MASS, 0.0f, RT_LOW_RESTIT, RT_LOW_RADIUS);

    /* Slowly approaching */
    particles[0].velocity = vec3_create(RT_LOW_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(-RT_LOW_VEL, 0.0f, 0.0f);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    forge_physics_resolve_contact(&contact, particles, 2);

    /* With e killed to 0, relative normal velocity after should be ~0
     * (perfectly inelastic). Particles should not bounce apart. */
    float v_rel_after = particles[0].velocity.x - particles[1].velocity.x;
    float v_normal_after = v_rel_after * contact.normal.x;
    ASSERT_NEAR(v_normal_after, 0.0f, EPSILON);
    END_TEST();
}

static void test_resolve_above_threshold_bounces(void)
{
    TEST("resolve_contact — above threshold: e=1.0 causes velocity reversal");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        RT_HIGH_MASS, 0.0f, RT_HIGH_RESTIT, RT_HIGH_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(RT_HIGH_OFFSET, 0.0f, 0.0f),
        RT_HIGH_MASS, 0.0f, RT_HIGH_RESTIT, RT_HIGH_RADIUS);

    /* Fast head-on approach */
    particles[0].velocity = vec3_create(RT_HIGH_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(-RT_HIGH_VEL, 0.0f, 0.0f);

    ForgePhysicsContact contact;
    bool hit = forge_physics_collide_sphere_sphere(
        &particles[0], &particles[1], 0, 1, &contact);
    ASSERT_TRUE(hit);

    forge_physics_resolve_contact(&contact, particles, 2);

    /* With elastic collision (e=1), equal mass: velocities swap.
     * Particle 0 was moving +x, should now move -x. */
    ASSERT_NEAR(particles[0].velocity.x, -RT_HIGH_VEL, EPSILON);
    ASSERT_NEAR(particles[1].velocity.x, RT_HIGH_VEL, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 41. NULL safety for collision functions
 * ══════════════════════════════════════════════════════════════════════════ */

#define NS_MASS                1.0f    /* mass for NULL safety tests */
#define NS_OFFSET              1.0f    /* separation between particles */
#define NS_PEN                 0.1f    /* penetration for manual contact */
#define NS_VEL                 1.0f    /* velocity for unchanged checks */

static void test_resolve_contact_out_of_bounds(void)
{
    TEST("resolve_contact — out-of-bounds indices: no crash, no change");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(NS_OFFSET, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    particles[0].velocity = vec3_create(NS_VEL, 0.0f, 0.0f);

    /* Contact with particle_a out of range */
    ForgePhysicsContact contact;
    contact.particle_a  = 5;  /* out of bounds */
    contact.particle_b  = 0;
    contact.normal      = vec3_create(NS_OFFSET, 0.0f, 0.0f);
    contact.penetration = NS_PEN;
    contact.point       = vec3_create(0.0f, 0.0f, 0.0f);

    forge_physics_resolve_contact(&contact, particles, 2);

    /* Velocity unchanged — bounds check prevented resolution */
    ASSERT_NEAR(particles[0].velocity.x, NS_VEL, EPSILON);
    ASSERT_NEAR(particles[1].velocity.x, 0.0f, EPSILON);

    /* Also test negative index */
    contact.particle_a = -1;
    contact.particle_b = 0;

    forge_physics_resolve_contact(&contact, particles, 2);

    ASSERT_NEAR(particles[0].velocity.x, NS_VEL, EPSILON);
    END_TEST();
}

static void test_resolve_contact_null_contact(void)
{
    TEST("resolve_contact — NULL contact: no crash");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(NS_OFFSET, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    forge_physics_resolve_contact(NULL, particles, 2);

    /* Velocities unchanged */
    ASSERT_NEAR(particles[0].velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(particles[1].velocity.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_resolve_contact_null_particles(void)
{
    TEST("resolve_contact — NULL particles: no crash");
    ForgePhysicsContact contact;
    contact.particle_a  = 0;
    contact.particle_b  = 1;
    contact.normal      = vec3_create(NS_OFFSET, 0.0f, 0.0f);
    contact.penetration = NS_PEN;
    contact.point       = vec3_create(0.0f, 0.0f, 0.0f);

    forge_physics_resolve_contact(&contact, NULL, 2);
    /* Reached — no crash */
    END_TEST();
}

static void test_resolve_contacts_null_contacts(void)
{
    TEST("resolve_contacts — NULL contacts array: no crash");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(NS_OFFSET, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    forge_physics_resolve_contacts(NULL, 5, particles, 2);

    /* No crash, no change */
    ASSERT_NEAR(particles[0].velocity.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_resolve_contacts_zero_count(void)
{
    TEST("resolve_contacts — 0 contacts: no crash, no change");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(NS_OFFSET, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);

    particles[0].velocity = vec3_create(NS_VEL, 0.0f, 0.0f);

    ForgePhysicsContact contacts[1];
    forge_physics_resolve_contacts(contacts, 0, particles, 2);

    /* Velocity unchanged */
    ASSERT_NEAR(particles[0].velocity.x, NS_VEL, EPSILON);
    END_TEST();
}

static void test_all_pairs_null_particles(void)
{
    TEST("collide_particles_all — NULL particles: returns 0");
    ForgePhysicsContact contacts[1];
    int count = forge_physics_collide_particles_all(NULL, 5, contacts, 1);
    ASSERT_TRUE(count == 0);
    END_TEST();
}

static void test_all_pairs_null_contacts(void)
{
    TEST("collide_particles_all — NULL contacts buffer: returns 0");
    ForgePhysicsParticle p = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    int count = forge_physics_collide_particles_all(&p, 1, NULL, 10);
    ASSERT_TRUE(count == 0);
    END_TEST();
}

static void test_step_null_particles(void)
{
    TEST("collide_particles_step — NULL particles: returns 0");
    ForgePhysicsContact contacts[1];
    int count = forge_physics_collide_particles_step(NULL, 5, contacts, 1);
    ASSERT_TRUE(count == 0);
    END_TEST();
}

static void test_step_null_contacts(void)
{
    TEST("collide_particles_step — NULL contacts: returns 0");
    ForgePhysicsParticle p = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        NS_MASS, 0.0f, DEFAULT_RESTIT, DEFAULT_RADIUS);
    int count = forge_physics_collide_particles_step(&p, 1, NULL, 10);
    ASSERT_TRUE(count == 0);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 42. Convenience function (collide_particles_step)
 * ══════════════════════════════════════════════════════════════════════════ */

#define STEP_RADIUS            0.5f
#define STEP_MASS              1.0f
#define STEP_RESTIT            1.0f
#define STEP_OVERLAP_OFFSET    0.8f    /* < 2 * radius */
#define STEP_SEP_OFFSET        5.0f    /* >> 2 * radius */
#define STEP_VEL               2.0f
#define STEP_CLUSTER_OFFSET    0.4f    /* all 3 overlap */
#define STEP_CLUSTER_PAIRS     3       /* C(3,2) = 3 */

static void test_step_detects_and_resolves(void)
{
    TEST("collide_particles_step — 2 overlapping: returns 1, velocities changed");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        STEP_MASS, 0.0f, STEP_RESTIT, STEP_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(STEP_OVERLAP_OFFSET, 0.0f, 0.0f),
        STEP_MASS, 0.0f, STEP_RESTIT, STEP_RADIUS);

    /* Head-on approach */
    particles[0].velocity = vec3_create(STEP_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(-STEP_VEL, 0.0f, 0.0f);

    ForgePhysicsContact contacts[FORGE_PHYSICS_MAX_CONTACTS];
    int count = forge_physics_collide_particles_step(
        particles, 2, contacts, FORGE_PHYSICS_MAX_CONTACTS);

    ASSERT_TRUE(count == 1);
    /* Velocities should have changed (elastic swap for equal mass) */
    ASSERT_NEAR(particles[0].velocity.x, -STEP_VEL, EPSILON);
    ASSERT_NEAR(particles[1].velocity.x, STEP_VEL, EPSILON);
    END_TEST();
}

static void test_step_no_collisions(void)
{
    TEST("collide_particles_step — 2 separated: returns 0, velocities unchanged");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        STEP_MASS, 0.0f, STEP_RESTIT, STEP_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(STEP_SEP_OFFSET, 0.0f, 0.0f),
        STEP_MASS, 0.0f, STEP_RESTIT, STEP_RADIUS);

    particles[0].velocity = vec3_create(STEP_VEL, 0.0f, 0.0f);
    particles[1].velocity = vec3_create(0.0f, 0.0f, 0.0f);

    ForgePhysicsContact contacts[FORGE_PHYSICS_MAX_CONTACTS];
    int count = forge_physics_collide_particles_step(
        particles, 2, contacts, FORGE_PHYSICS_MAX_CONTACTS);

    ASSERT_TRUE(count == 0);
    /* Velocities unchanged */
    ASSERT_NEAR(particles[0].velocity.x, STEP_VEL, EPSILON);
    ASSERT_NEAR(particles[1].velocity.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_step_multiple_contacts(void)
{
    TEST("collide_particles_step — 3 overlapping: returns 3, all resolved");
    ForgePhysicsParticle particles[3];
    for (int i = 0; i < 3; i++) {
        particles[i] = forge_physics_particle_create(
            vec3_create((float)i * STEP_CLUSTER_OFFSET, 0.0f, 0.0f),
            STEP_MASS, 0.0f, STEP_RESTIT, STEP_RADIUS);
    }

    /* Give them approaching velocities */
    particles[0].velocity = vec3_create(STEP_VEL, 0.0f, 0.0f);
    particles[2].velocity = vec3_create(-STEP_VEL, 0.0f, 0.0f);

    ForgePhysicsContact contacts[FORGE_PHYSICS_MAX_CONTACTS];
    int count = forge_physics_collide_particles_step(
        particles, 3, contacts, FORGE_PHYSICS_MAX_CONTACTS);

    ASSERT_TRUE(count == STEP_CLUSTER_PAIRS);

    /* All velocities should have been modified by resolution */
    /* At minimum, outer particles should no longer be approaching */
    ASSERT_TRUE(particles[0].velocity.x < STEP_VEL);
    ASSERT_TRUE(particles[2].velocity.x > -STEP_VEL);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Lesson 04 — Rigid Body State and Orientation
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Rigid body test constants ─────────────────────────────────────────── */

#define RB_MASS            5.0f
#define RB_INV_MASS        0.2f    /* 1 / RB_MASS */
#define RB_DAMPING         0.99f
#define RB_ANG_DAMPING     0.99f
#define RB_RESTIT          0.5f

/* Box inertia: unit cube (half=1,1,1), mass=12
 * I = (1/12)*12*((2)²+(2)²) = 8 for each axis */
#define BOX_MASS           12.0f
#define BOX_HALF           1.0f
#define BOX_I              8.0f
#define BOX_INV_I          0.125f  /* 1/8 */

/* Sphere inertia: radius=1, mass=10
 * I = (2/5)*10*1² = 4 */
#define SPHERE_MASS        10.0f
#define SPHERE_RADIUS      1.0f
#define SPHERE_I           4.0f
#define SPHERE_INV_I       0.25f   /* 1/4 */

/* Cylinder inertia: radius=1, half_h=1, mass=12
 * Iyy = (1/2)*12*1 = 6
 * Ixx = Izz = (1/12)*12*(3+4) = 7 */
#define CYL_MASS           12.0f
#define CYL_RADIUS         1.0f
#define CYL_HALF_H         1.0f
#define CYL_IYY            6.0f
#define CYL_IXX            7.0f

/* Creation test inputs */
#define RB_POS_X            1.0f
#define RB_POS_Y            2.0f
#define RB_POS_Z            3.0f
#define RB_PREV_POS_X       5.0f
#define RB_PREV_POS_Y       10.0f
#define RB_PREV_POS_Z       15.0f
#define RB_UNIT_MASS        1.0f
#define RB_NEG_MASS         -5.0f
#define RB_LARGE_MASS       1e6f
#define RB_LARGE_INV_MASS   1e-6f

/* Clamping test inputs */
#define RB_OVER_DAMP        5.0f
#define RB_NEG_ANG_DAMP     -2.0f
#define RB_OVER_RESTIT      3.0f

/* Non-uniform box (used in inertia, symmetry, world inertia tests) */
#define RB_BOX2_MASS        6.0f
#define RB_BOX2_HX          2.0f
#define RB_BOX2_HY          1.0f
#define RB_BOX2_HZ          0.5f
#define RB_BOX2_IXX         2.5f
#define RB_BOX2_IYY         8.5f
#define RB_BOX2_IZZ         10.0f

/* Sphere radius=2 test */
#define RB_SPHERE2_RADIUS   2.0f
#define RB_SPHERE2_INV_I    0.125f

/* Force / torque test values */
#define RB_FORCE_X          10.0f
#define RB_FORCE2_X         5.0f
#define RB_FORCE2_Y         3.0f
#define RB_FORCE_ACCUM_X    15.0f
#define RB_POINT_FORCE      10.0f
#define RB_TORQUE_Y         5.0f
#define RB_LARGE_FORCE      100.0f
#define RB_HUGE_FORCE       1e10f

/* Clear-forces test vectors */
#define RB_CF_FORCE_X       10.0f
#define RB_CF_FORCE_Y       20.0f
#define RB_CF_FORCE_Z       30.0f
#define RB_CF_TORQUE_X      5.0f
#define RB_CF_TORQUE_Y      10.0f
#define RB_CF_TORQUE_Z      15.0f

/* Integrate-clears torque */
#define RB_IC_TORQUE_X      1.0f
#define RB_IC_TORQUE_Y      2.0f
#define RB_IC_TORQUE_Z      3.0f

/* Integration tests */
#define RB_INT_MASS         2.0f
#define RB_INT_ANG_VEL      5.0f
#define RB_INT_ROT_TOL      0.01f
#define RB_STATIC_POS       5.0f

/* Quaternion stability */
#define RB_QUAT_AV_X        3.0f
#define RB_QUAT_AV_Y        5.0f
#define RB_QUAT_AV_Z        2.0f
#define RB_QUAT_STEPS       10000
#define RB_QUAT_TOL         0.001f

/* Damping tests */
#define RB_HALF_DAMP        0.5f
#define RB_DAMP_VEL         10.0f
#define RB_DAMP_DT          1.0f
#define RB_DAMP_VEL_AFTER   5.0f
#define RB_DAMP_TOL         0.1f

/* Transform tests */
#define RB_XFORM_POS_X      3.0f
#define RB_XFORM_POS_Y      4.0f
#define RB_XFORM_POS_Z      5.0f

/* Stability / determinism tests */
#define RB_STAB_POS_Y       10.0f
#define RB_STAB_HX          1.0f
#define RB_STAB_HY          0.5f
#define RB_STAB_HZ          0.25f
#define RB_DET_STEPS        1000

/* ── 43. Rigid body create ─────────────────────────────────────────────── */

static void test_rb_create_defaults(void)
{
    TEST("rigid_body_create — default values");
    vec3 pos = vec3_create(RB_POS_X, RB_POS_Y, RB_POS_Z);
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        pos, RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    ASSERT_NEAR(rb.mass, RB_MASS, EPSILON);
    ASSERT_NEAR(rb.inv_mass, RB_INV_MASS, EPSILON);
    ASSERT_NEAR(rb.damping, RB_DAMPING, EPSILON);
    ASSERT_NEAR(rb.angular_damping, RB_ANG_DAMPING, EPSILON);
    ASSERT_NEAR(rb.restitution, RB_RESTIT, EPSILON);
    ASSERT_NEAR(rb.position.x, RB_POS_X, EPSILON);
    ASSERT_NEAR(rb.position.y, RB_POS_Y, EPSILON);
    ASSERT_NEAR(rb.position.z, RB_POS_Z, EPSILON);
    /* Identity orientation */
    ASSERT_NEAR(rb.orientation.w, 1.0f, EPSILON);
    ASSERT_NEAR(rb.orientation.x, 0.0f, EPSILON);
    /* Zero velocities */
    ASSERT_NEAR(rb.velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.angular_velocity.x, 0.0f, EPSILON);
    /* Zero accumulators */
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_create_static(void)
{
    TEST("rigid_body_create — static body (mass=0)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), 0.0f, RB_HALF_DAMP, RB_HALF_DAMP, RB_RESTIT);
    ASSERT_NEAR(rb.mass, 0.0f, EPSILON);
    ASSERT_NEAR(rb.inv_mass, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_create_negative_mass(void)
{
    TEST("rigid_body_create — negative mass treated as static");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_NEG_MASS, RB_HALF_DAMP, RB_HALF_DAMP, RB_RESTIT);
    ASSERT_NEAR(rb.mass, 0.0f, EPSILON);
    ASSERT_NEAR(rb.inv_mass, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_create_clamping(void)
{
    TEST("rigid_body_create — damping/restitution clamped to [0..1]");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_UNIT_MASS, RB_OVER_DAMP, RB_NEG_ANG_DAMP, RB_OVER_RESTIT);
    ASSERT_NEAR(rb.damping, 1.0f, EPSILON);
    ASSERT_NEAR(rb.angular_damping, 0.0f, EPSILON);
    ASSERT_NEAR(rb.restitution, 1.0f, EPSILON);
    END_TEST();
}

static void test_rb_create_prev_fields(void)
{
    TEST("rigid_body_create — prev fields match initial");
    vec3 pos = vec3_create(RB_PREV_POS_X, RB_PREV_POS_Y, RB_PREV_POS_Z);
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        pos, RB_UNIT_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    ASSERT_NEAR(rb.prev_position.x, pos.x, EPSILON);
    ASSERT_NEAR(rb.prev_position.y, pos.y, EPSILON);
    ASSERT_NEAR(rb.prev_position.z, pos.z, EPSILON);
    ASSERT_NEAR(rb.prev_orientation.w, 1.0f, EPSILON);
    ASSERT_NEAR(rb.prev_orientation.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_create_large_mass(void)
{
    TEST("rigid_body_create — very large mass");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_LARGE_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    ASSERT_NEAR(rb.mass, RB_LARGE_MASS, 1.0f);
    ASSERT_TRUE(rb.inv_mass > 0.0f);
    ASSERT_NEAR(rb.inv_mass, RB_LARGE_INV_MASS, 1e-8f);
    END_TEST();
}

/* ── 44. Inertia tensor ────────────────────────────────────────────────── */

static void test_rb_inertia_box_unit_cube(void)
{
    TEST("inertia_box — unit cube (half=1,1,1), mass=12");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), BOX_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(BOX_HALF, BOX_HALF, BOX_HALF));
    /* All three moments equal for a cube */
    ASSERT_NEAR(rb.inv_inertia_local.m[0], BOX_INV_I, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_local.m[4], BOX_INV_I, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_local.m[8], BOX_INV_I, EPSILON);
    /* Off-diagonals zero */
    ASSERT_NEAR(rb.inv_inertia_local.m[1], 0.0f, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_local.m[3], 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_inertia_box_nonuniform(void)
{
    TEST("inertia_box — non-uniform box");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_BOX2_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(RB_BOX2_HX, RB_BOX2_HY, RB_BOX2_HZ));
    /* Full extents: 4, 2, 1. Squared: 16, 4, 1
     * Ixx = (1/12)*6*(4+1) = 2.5
     * Iyy = (1/12)*6*(16+1) = 8.5
     * Izz = (1/12)*6*(16+4) = 10.0 */
    ASSERT_NEAR(1.0f / rb.inv_inertia_local.m[0], RB_BOX2_IXX, EPSILON);
    ASSERT_NEAR(1.0f / rb.inv_inertia_local.m[4], RB_BOX2_IYY, EPSILON);
    ASSERT_NEAR(1.0f / rb.inv_inertia_local.m[8], RB_BOX2_IZZ, EPSILON);
    END_TEST();
}

static void test_rb_inertia_sphere(void)
{
    TEST("inertia_sphere — radius=1, mass=10");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), SPHERE_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    /* I = (2/5)*10*1 = 4, I_inv = 0.25 */
    ASSERT_NEAR(rb.inv_inertia_local.m[0], SPHERE_INV_I, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_local.m[4], SPHERE_INV_I, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_local.m[8], SPHERE_INV_I, EPSILON);
    END_TEST();
}

static void test_rb_inertia_sphere_r2(void)
{
    TEST("inertia_sphere — radius=2, mass=5");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, RB_SPHERE2_RADIUS);
    /* I = (2/5)*5*4 = 8, I_inv = 0.125 */
    ASSERT_NEAR(rb.inv_inertia_local.m[0], RB_SPHERE2_INV_I, EPSILON);
    END_TEST();
}

static void test_rb_inertia_cylinder(void)
{
    TEST("inertia_cylinder — r=1, half_h=1, mass=12");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), CYL_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_cylinder(&rb, CYL_RADIUS, CYL_HALF_H);
    /* Iyy = 6, Ixx = Izz = 7 */
    ASSERT_NEAR(1.0f / rb.inv_inertia_local.m[0], CYL_IXX, EPSILON);
    ASSERT_NEAR(1.0f / rb.inv_inertia_local.m[4], CYL_IYY, EPSILON);
    ASSERT_NEAR(1.0f / rb.inv_inertia_local.m[8], CYL_IXX, EPSILON);
    END_TEST();
}

static void test_rb_inertia_static_body(void)
{
    TEST("inertia — static body remains identity");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), 0.0f, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(1.0f, 1.0f, 1.0f));
    /* Static body: inertia unchanged (still identity) */
    ASSERT_NEAR(rb.inv_inertia_local.m[0], 1.0f, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_local.m[4], 1.0f, EPSILON);
    END_TEST();
}

static void test_rb_inertia_symmetry(void)
{
    TEST("inertia — diagonal tensor is symmetric");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(RB_BOX2_HX, RB_BOX2_HY, RB_BOX2_HZ));
    mat3 t = mat3_transpose(rb.inv_inertia_local);
    for (int i = 0; i < 9; i++) {
        ASSERT_NEAR(rb.inv_inertia_local.m[i], t.m[i], EPSILON);
    }
    END_TEST();
}

/* ── 45. Force / torque application ────────────────────────────────────── */

static void test_rb_force_at_center(void)
{
    TEST("apply_force — accumulates at center (no torque)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_apply_force(&rb, vec3_create(RB_FORCE_X, 0.0f, 0.0f));
    ASSERT_NEAR(rb.force_accum.x, RB_FORCE_X, EPSILON);
    ASSERT_NEAR(rb.torque_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_force_accumulates(void)
{
    TEST("apply_force — multiple calls accumulate");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_apply_force(&rb, vec3_create(RB_FORCE_X, 0.0f, 0.0f));
    forge_physics_rigid_body_apply_force(&rb, vec3_create(RB_FORCE2_X, RB_FORCE2_Y, 0.0f));
    ASSERT_NEAR(rb.force_accum.x, RB_FORCE_ACCUM_X, EPSILON);
    ASSERT_NEAR(rb.force_accum.y, RB_FORCE2_Y, EPSILON);
    END_TEST();
}

static void test_rb_force_at_point_torque(void)
{
    TEST("apply_force_at_point — cross(+X, +Y) = +Z torque");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    /* Force +Y at point +X from COM: torque = cross(+X, +Y) = +Z */
    forge_physics_rigid_body_apply_force_at_point(&rb,
        vec3_create(0, RB_POINT_FORCE, 0),   /* force */
        vec3_create(1, 0, 0));               /* world point */
    ASSERT_NEAR(rb.force_accum.y, RB_POINT_FORCE, EPSILON);
    ASSERT_NEAR(rb.torque_accum.z, RB_POINT_FORCE, EPSILON);
    ASSERT_NEAR(rb.torque_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_force_at_point_nonorigin_com(void)
{
    TEST("apply_force_at_point — non-origin COM produces same torque");
    /* Body at (1,0,0) with force +Y at world point (2,0,0):
     * offset = (2,0,0) - (1,0,0) = (1,0,0)
     * torque = cross((1,0,0), (0,F,0)) = (0,0,F) — same as origin case */
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(RB_POS_X, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING,
        RB_RESTIT);
    forge_physics_rigid_body_apply_force_at_point(&rb,
        vec3_create(0, RB_POINT_FORCE, 0),         /* force */
        vec3_create(RB_POS_X + 1.0f, 0, 0));       /* world point */
    ASSERT_NEAR(rb.force_accum.y, RB_POINT_FORCE, EPSILON);
    ASSERT_NEAR(rb.torque_accum.z, RB_POINT_FORCE, EPSILON);
    ASSERT_NEAR(rb.torque_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_torque_angular_acceleration_magnitude(void)
{
    TEST("torque produces correct angular acceleration magnitude");
    /* Sphere: I = (2/5)*m*r^2 = (2/5)*5.0*1.0 = 2.0
     * I_inv = 0.5.  Torque = (0, 10, 0).
     * alpha = I_inv * torque = (0, 5, 0).
     * After one step with dt and damping=1.0:
     * omega = alpha * dt = (0, 5*dt, 0) */
#define TAA_MASS    5.0f
#define TAA_RADIUS  1.0f
#define TAA_TORQUE  10.0f
#define TAA_DT      0.01f
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), TAA_MASS, 1.0f, 1.0f, 0.0f);
    forge_physics_rigid_body_set_inertia_sphere(&rb, TAA_RADIUS);
    forge_physics_rigid_body_apply_torque(&rb,
        vec3_create(0, TAA_TORQUE, 0));
    forge_physics_rigid_body_integrate(&rb, TAA_DT);

    /* I_inv = 1 / ((2/5)*5*1) = 0.5, alpha_y = 0.5 * 10 = 5.0 */
    float expected_omega_y = 5.0f * TAA_DT;
    ASSERT_NEAR(rb.angular_velocity.y, expected_omega_y, EPSILON);
    ASSERT_NEAR(rb.angular_velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.angular_velocity.z, 0.0f, EPSILON);
#undef TAA_MASS
#undef TAA_RADIUS
#undef TAA_TORQUE
#undef TAA_DT
    END_TEST();
}

static void test_rb_apply_torque_direct(void)
{
    TEST("apply_torque — only torque changes");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_apply_torque(&rb, vec3_create(0, RB_TORQUE_Y, 0));
    ASSERT_NEAR(rb.torque_accum.y, RB_TORQUE_Y, EPSILON);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.force_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_force_static_noop(void)
{
    TEST("apply_force — static body is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), 0.0f, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_apply_force(&rb, vec3_create(RB_LARGE_FORCE, 0, 0));
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_force_zero(void)
{
    TEST("apply_force — zero force no change");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_apply_force(&rb, vec3_create(0, 0, 0));
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_clear_forces(void)
{
    TEST("clear_forces — zeroes both accumulators");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_apply_force(&rb,
        vec3_create(RB_CF_FORCE_X, RB_CF_FORCE_Y, RB_CF_FORCE_Z));
    forge_physics_rigid_body_apply_torque(&rb,
        vec3_create(RB_CF_TORQUE_X, RB_CF_TORQUE_Y, RB_CF_TORQUE_Z));
    forge_physics_rigid_body_clear_forces(&rb);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.y, 0.0f, EPSILON);
    END_TEST();
}

/* ── 46. Integration ───────────────────────────────────────────────────── */

static void test_rb_integrate_pure_linear(void)
{
    TEST("integrate — pure linear (gravity, no rotation)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, RB_STAB_POS_Y, 0), RB_INT_MASS, 1.0f, 1.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    /* Apply gravity: F = m*g = 2 * -9.81 */
    forge_physics_rigid_body_apply_force(&rb,
        vec3_create(0.0f, -9.81f * RB_INT_MASS, 0.0f));
    forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    /* v = g * dt = -9.81 * (1/60) = -0.1635 */
    ASSERT_NEAR(rb.velocity.y, -9.81f * PHYSICS_DT, RB_INT_ROT_TOL);
    /* No angular velocity */
    ASSERT_NEAR(rb.angular_velocity.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_integrate_pure_rotation(void)
{
    TEST("integrate — pure rotation (no forces)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, 1.0f, 1.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    /* Set initial angular velocity around Y */
    rb.angular_velocity = vec3_create(0.0f, RB_INT_ANG_VEL, 0.0f);
    forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    /* With damping=1.0, velocity preserved */
    ASSERT_NEAR(rb.angular_velocity.y, RB_INT_ANG_VEL, RB_INT_ROT_TOL);
    /* Quaternion should have changed */
    ASSERT_TRUE(SDL_fabsf(rb.orientation.w - 1.0f) > EPSILON ||
                SDL_fabsf(rb.orientation.y) > EPSILON);
    /* Position unchanged (no forces) */
    ASSERT_NEAR(rb.position.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_integrate_quat_stays_unit(void)
{
    TEST("integrate — quaternion stays unit after many steps");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, 1.0f, 1.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    rb.angular_velocity = vec3_create(RB_QUAT_AV_X, RB_QUAT_AV_Y, RB_QUAT_AV_Z);
    for (int i = 0; i < RB_QUAT_STEPS; i++) {
        forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    }
    float q_len = quat_length(rb.orientation);
    ASSERT_NEAR(q_len, 1.0f, RB_QUAT_TOL);
    END_TEST();
}

static void test_rb_integrate_static_noop(void)
{
    TEST("integrate — static body unchanged");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(RB_STATIC_POS, RB_STATIC_POS, RB_STATIC_POS),
        0.0f, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    rb.force_accum = vec3_create(RB_LARGE_FORCE, RB_LARGE_FORCE, RB_LARGE_FORCE);
    forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    ASSERT_NEAR(rb.position.x, RB_STATIC_POS, EPSILON);
    ASSERT_NEAR(rb.position.y, RB_STATIC_POS, EPSILON);
    END_TEST();
}

static void test_rb_integrate_zero_dt(void)
{
    TEST("integrate — zero dt no change");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(RB_POS_X, RB_POS_Y, RB_POS_Z),
        RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_apply_force(&rb, vec3_create(RB_LARGE_FORCE, 0, 0));
    forge_physics_rigid_body_integrate(&rb, 0.0f);
    ASSERT_NEAR(rb.position.x, RB_POS_X, EPSILON);
    ASSERT_NEAR(rb.velocity.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_integrate_velocity_clamp(void)
{
    TEST("integrate — large force clamps velocity");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_UNIT_MASS, 1.0f, 1.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    forge_physics_rigid_body_apply_force(&rb, vec3_create(RB_HUGE_FORCE, 0, 0));
    forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    float v_len = vec3_length(rb.velocity);
    ASSERT_TRUE(v_len <= FORGE_PHYSICS_MAX_VELOCITY + EPSILON);
    END_TEST();
}

static void test_rb_integrate_angular_velocity_clamp(void)
{
    TEST("integrate — large torque clamps angular velocity");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_UNIT_MASS, 1.0f, 1.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    forge_physics_rigid_body_apply_torque(&rb, vec3_create(RB_HUGE_FORCE, 0, 0));
    forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    float w_len = vec3_length(rb.angular_velocity);
    ASSERT_TRUE(w_len <= FORGE_PHYSICS_MAX_ANGULAR_VELOCITY + EPSILON);
    END_TEST();
}

static void test_rb_integrate_prev_saved(void)
{
    TEST("integrate — prev state saved");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, 1.0f, 1.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    forge_physics_rigid_body_apply_force(&rb, vec3_create(RB_LARGE_FORCE, 0, 0));
    rb.angular_velocity = vec3_create(0, RB_INT_ANG_VEL, 0);
    forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    /* prev_position should be original (0,0,0) */
    ASSERT_NEAR(rb.prev_position.x, 0.0f, EPSILON);
    /* current position should have moved */
    ASSERT_TRUE(SDL_fabsf(rb.position.x) > EPSILON);
    END_TEST();
}

static void test_rb_integrate_clears_accumulators(void)
{
    TEST("integrate — clears accumulators");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    forge_physics_rigid_body_apply_force(&rb,
        vec3_create(RB_CF_FORCE_X, RB_CF_FORCE_Y, RB_CF_FORCE_Z));
    forge_physics_rigid_body_apply_torque(&rb,
        vec3_create(RB_IC_TORQUE_X, RB_IC_TORQUE_Y, RB_IC_TORQUE_Z));
    forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.x, 0.0f, EPSILON);
    END_TEST();
}

/* ── 47. Damping ───────────────────────────────────────────────────────── */

static void test_rb_damping_linear(void)
{
    TEST("damping — linear velocity reduced");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_HALF_DAMP, 1.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    rb.velocity = vec3_create(RB_DAMP_VEL, 0.0f, 0.0f);
    forge_physics_rigid_body_integrate(&rb, RB_DAMP_DT);
    /* v *= pow(0.5, 1.0) = 0.5, so v ≈ 5.0 */
    ASSERT_NEAR(vec3_length(rb.velocity), RB_DAMP_VEL_AFTER, RB_DAMP_TOL);
    END_TEST();
}

static void test_rb_damping_angular(void)
{
    TEST("damping — angular velocity reduced");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, 1.0f, RB_HALF_DAMP, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    rb.angular_velocity = vec3_create(0.0f, RB_DAMP_VEL, 0.0f);
    forge_physics_rigid_body_integrate(&rb, RB_DAMP_DT);
    /* omega *= pow(0.5, 1.0) = 0.5, so omega ≈ 5.0 */
    ASSERT_NEAR(vec3_length(rb.angular_velocity), RB_DAMP_VEL_AFTER, RB_DAMP_TOL);
    END_TEST();
}

static void test_rb_damping_one_preserves(void)
{
    TEST("damping — damping=1.0 preserves velocity");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, 1.0f, 1.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    rb.velocity = vec3_create(RB_DAMP_VEL, 0.0f, 0.0f);
    forge_physics_rigid_body_integrate(&rb, RB_DAMP_DT);
    ASSERT_NEAR(vec3_length(rb.velocity), RB_DAMP_VEL, RB_DAMP_TOL);
    END_TEST();
}

static void test_rb_damping_zero_zeroes(void)
{
    TEST("damping — damping=0.0 zeroes velocity");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, 0.0f, 0.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    rb.velocity = vec3_create(RB_DAMP_VEL, 0.0f, 0.0f);
    rb.angular_velocity = vec3_create(0.0f, RB_DAMP_VEL, 0.0f);
    forge_physics_rigid_body_integrate(&rb, RB_DAMP_DT);
    ASSERT_NEAR(vec3_length(rb.velocity), 0.0f, EPSILON);
    ASSERT_NEAR(vec3_length(rb.angular_velocity), 0.0f, EPSILON);
    END_TEST();
}

/* ── 48. World-space inertia ───────────────────────────────────────────── */

static void test_rb_world_inertia_identity_orient(void)
{
    TEST("world inertia — identity orientation matches local");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(RB_BOX2_HX, RB_BOX2_HY, RB_BOX2_HZ));
    forge_physics_rigid_body_update_derived(&rb);
    for (int i = 0; i < 9; i++) {
        ASSERT_NEAR(rb.inv_inertia_world.m[i],
                     rb.inv_inertia_local.m[i], EPSILON);
    }
    END_TEST();
}

static void test_rb_world_inertia_sphere_invariant(void)
{
    TEST("world inertia — sphere invariant under rotation");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), SPHERE_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    rb.orientation = quat_from_axis_angle(
        vec3_normalize(vec3_create(1, 1, 1)), 1.23f);
    forge_physics_rigid_body_update_derived(&rb);
    /* Sphere inertia is I_inv = 0.25 * I₃, invariant under rotation */
    ASSERT_NEAR(rb.inv_inertia_world.m[0], SPHERE_INV_I, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_world.m[4], SPHERE_INV_I, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_world.m[8], SPHERE_INV_I, EPSILON);
    END_TEST();
}

static void test_rb_world_inertia_90_rotation(void)
{
    TEST("world inertia — 90° Y rotation swaps X/Z axes");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(RB_BOX2_HX, RB_BOX2_HY, RB_BOX2_HZ));
    float local_xx = rb.inv_inertia_local.m[0];
    float local_yy = rb.inv_inertia_local.m[4];
    float local_zz = rb.inv_inertia_local.m[8];
    /* 90° rotation around Y: X axis → -Z, Z axis → X */
    rb.orientation = quat_from_axis_angle(vec3_create(0, 1, 0),
                                           FORGE_PI * 0.5f);
    forge_physics_rigid_body_update_derived(&rb);
    /* After 90° Y: world_xx should be local_zz, world_zz should be local_xx */
    ASSERT_NEAR(rb.inv_inertia_world.m[0], local_zz, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_world.m[4], local_yy, EPSILON);
    ASSERT_NEAR(rb.inv_inertia_world.m[8], local_xx, EPSILON);
    END_TEST();
}

static void test_rb_world_inertia_after_integration(void)
{
    TEST("world inertia — updated after integration");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, 1.0f, 1.0f, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(RB_BOX2_HX, RB_BOX2_HY, RB_BOX2_HZ));
    rb.angular_velocity = vec3_create(0, RB_INT_ANG_VEL, 0);
    mat3 before = rb.inv_inertia_world;
    forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    /* Non-uniform inertia + rotation → world inertia should change */
    /* (Except if rotation is exactly around a principal axis with
     *  equal perpendicular moments, which is not the case here) */
    /* At least one element should differ */
    bool changed = false;
    for (int i = 0; i < 9; i++) {
        if (SDL_fabsf(rb.inv_inertia_world.m[i] - before.m[i]) > EPSILON) {
            changed = true;
            break;
        }
    }
    ASSERT_TRUE(changed);
    END_TEST();
}

/* ── 49. Transform ─────────────────────────────────────────────────────── */

static void test_rb_transform_identity(void)
{
    TEST("get_transform — identity at origin");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_UNIT_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    mat4 m = forge_physics_rigid_body_get_transform(&rb);
    mat4 id = mat4_identity();
    for (int i = 0; i < 16; i++) {
        ASSERT_NEAR(m.m[i], id.m[i], EPSILON);
    }
    END_TEST();
}

static void test_rb_transform_translation_only(void)
{
    TEST("get_transform — translation only");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(RB_XFORM_POS_X, RB_XFORM_POS_Y, RB_XFORM_POS_Z),
        RB_UNIT_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    mat4 m = forge_physics_rigid_body_get_transform(&rb);
    ASSERT_NEAR(m.m[12], RB_XFORM_POS_X, EPSILON);
    ASSERT_NEAR(m.m[13], RB_XFORM_POS_Y, EPSILON);
    ASSERT_NEAR(m.m[14], RB_XFORM_POS_Z, EPSILON);
    /* Rotation part should be identity */
    ASSERT_NEAR(m.m[0], 1.0f, EPSILON);
    ASSERT_NEAR(m.m[5], 1.0f, EPSILON);
    ASSERT_NEAR(m.m[10], 1.0f, EPSILON);
    END_TEST();
}

static void test_rb_transform_combined(void)
{
    TEST("get_transform — translation + rotation");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(RB_XFORM_POS_X, RB_XFORM_POS_Y, RB_XFORM_POS_Z),
        RB_UNIT_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    rb.orientation = quat_from_axis_angle(vec3_create(0, 1, 0),
                                           FORGE_PI * 0.5f);
    mat4 m = forge_physics_rigid_body_get_transform(&rb);
    /* Compare with manual: translate(3,4,5) * rotate_y(90°) */
    mat4 expected = mat4_multiply(
        mat4_translate(vec3_create(RB_XFORM_POS_X, RB_XFORM_POS_Y, RB_XFORM_POS_Z)),
        quat_to_mat4(rb.orientation));
    for (int i = 0; i < 16; i++) {
        ASSERT_NEAR(m.m[i], expected.m[i], EPSILON);
    }
    END_TEST();
}

/* ── 50. Conservation and stability ────────────────────────────────────── */

static void test_rb_no_nan_after_many_steps(void)
{
    TEST("stability — no NaN/inf after 10000 steps");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, RB_STAB_POS_Y, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(RB_STAB_HX, RB_STAB_HY, RB_STAB_HZ));
    rb.angular_velocity = vec3_create(RB_QUAT_AV_X, RB_QUAT_AV_Y, RB_QUAT_AV_Z);
    for (int i = 0; i < RB_QUAT_STEPS; i++) {
        forge_physics_rigid_body_apply_force(&rb,
            vec3_create(0, -9.81f * rb.mass, 0));
        forge_physics_rigid_body_integrate(&rb, PHYSICS_DT);
    }
    ASSERT_TRUE(isfinite(rb.position.x));
    ASSERT_TRUE(isfinite(rb.position.y));
    ASSERT_TRUE(isfinite(rb.position.z));
    ASSERT_TRUE(isfinite(rb.velocity.x));
    ASSERT_TRUE(isfinite(rb.angular_velocity.x));
    ASSERT_TRUE(isfinite(rb.orientation.w));
    END_TEST();
}

static void test_rb_determinism(void)
{
    TEST("stability — deterministic integration");
    ForgePhysicsRigidBody rb1 = forge_physics_rigid_body_create(
        vec3_create(0, RB_STAB_POS_Y, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb1,
        vec3_create(RB_STAB_HX, RB_STAB_HY, RB_STAB_HZ));
    rb1.angular_velocity = vec3_create(RB_QUAT_AV_X, RB_QUAT_AV_Y, RB_QUAT_AV_Z);
    ForgePhysicsRigidBody rb2 = rb1;  /* copy after full setup */
    for (int i = 0; i < RB_DET_STEPS; i++) {
        forge_physics_rigid_body_apply_force(&rb1,
            vec3_create(0, -9.81f * rb1.mass, 0));
        forge_physics_rigid_body_apply_force(&rb2,
            vec3_create(0, -9.81f * rb2.mass, 0));
        forge_physics_rigid_body_integrate(&rb1, PHYSICS_DT);
        forge_physics_rigid_body_integrate(&rb2, PHYSICS_DT);
    }
    ASSERT_NEAR(rb1.position.x, rb2.position.x, EPSILON);
    ASSERT_NEAR(rb1.position.y, rb2.position.y, EPSILON);
    ASSERT_NEAR(rb1.position.z, rb2.position.z, EPSILON);
    ASSERT_NEAR(rb1.orientation.w, rb2.orientation.w, EPSILON);
    END_TEST();
}

/* ── 51. Gyroscopic term ───────────────────────────────────────────────── */

/* Test constants for gyroscopic tests */
#define GYRO_MASS      5.0f
#define GYRO_DT        (1.0f / 60.0f)
#define GYRO_OMEGA     10.0f
#define GYRO_STEPS     60
#define GYRO_TOL       0.05f

static void test_rb_gyro_sphere_no_effect(void)
{
    TEST("gyroscopic — sphere: no gyroscopic coupling (I is scalar)");
    /* For a sphere, I is a scalar multiple of identity, so ω × (Iω) = 0.
     * Integration with and without the gyroscopic term should be identical. */
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), GYRO_MASS, 1.0f, 1.0f, 0.5f);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 1.0f);
    rb.angular_velocity = vec3_create(GYRO_OMEGA, 0.0f, 0.0f);
    /* With no torque, sphere should maintain angular velocity direction */
    for (int i = 0; i < GYRO_STEPS; i++) {
        forge_physics_rigid_body_integrate(&rb, GYRO_DT);
    }
    /* Direction should be unchanged (still along X) */
    float total = vec3_length(rb.angular_velocity);
    ASSERT_TRUE(total > 0.1f);
    ASSERT_NEAR(rb.angular_velocity.y / total, 0.0f, GYRO_TOL);
    ASSERT_NEAR(rb.angular_velocity.z / total, 0.0f, GYRO_TOL);
    END_TEST();
}

static void test_rb_gyro_box_coupling(void)
{
    TEST("gyroscopic — non-spherical box: off-axis spin produces coupling");
    /* A box with asymmetric inertia spinning around a non-principal axis
     * will experience gyroscopic torque that changes the angular velocity
     * direction. This is the "intermediate axis theorem" (tennis racket). */
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), GYRO_MASS, 1.0f, 1.0f, 0.5f);
    /* Box with well-separated principal moments (half-extents 2, 1, 0.2):
     * Ixx ≈ 1.73, Iyy ≈ 6.73, Izz ≈ 8.33
     * Y is the intermediate axis — rotation around it is unstable. */
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(2.0f, 1.0f, 0.2f));
    /* Spin around intermediate axis (Y) with perturbation along X.
     * The intermediate axis theorem predicts unstable rotation here. */
    rb.angular_velocity = vec3_create(3.0f, GYRO_OMEGA, 0.0f);
    vec3 initial_omega = rb.angular_velocity;
    /* Run for 5 seconds to let the instability develop */
    for (int i = 0; i < GYRO_STEPS * 5; i++) {
        forge_physics_rigid_body_integrate(&rb, GYRO_DT);
    }
    /* The gyroscopic effect should cause angular velocity to change direction.
     * Without the gyroscopic term, ω would be constant (no torque applied). */
    float dot = vec3_dot(vec3_normalize(rb.angular_velocity),
                         vec3_normalize(initial_omega));
    /* Direction should have changed noticeably (dot < 1) */
    ASSERT_TRUE(dot < 0.98f);
    END_TEST();
}

static void test_rb_gyro_angular_momentum_conservation(void)
{
    TEST("gyroscopic — angular momentum magnitude conserved (no torque)");
    /* With no external torque and damping = 1.0, the angular momentum
     * magnitude |L| = |I·ω| should be approximately conserved. */
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), GYRO_MASS, 1.0f, 1.0f, 0.5f);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(2.0f, 0.5f, 1.0f));
    rb.angular_velocity = vec3_create(3.0f, 5.0f, 2.0f);
    forge_physics_rigid_body_update_derived(&rb);
    vec3 L0 = mat3_multiply_vec3(rb.inertia_world, rb.angular_velocity);
    float L0_mag = vec3_length(L0);
    for (int i = 0; i < GYRO_STEPS; i++) {
        forge_physics_rigid_body_integrate(&rb, GYRO_DT);
    }
    vec3 L1 = mat3_multiply_vec3(rb.inertia_world, rb.angular_velocity);
    float L1_mag = vec3_length(L1);
    /* Allow ~5% drift from integration error over 60 steps */
    ASSERT_NEAR(L1_mag, L0_mag, L0_mag * 0.05f);
    END_TEST();
}

static void test_rb_gyro_inertia_world_field(void)
{
    TEST("gyroscopic — inertia_world field computed correctly");
    /* Verify that inertia_world = R * I_local * R^T and that it equals
     * the inverse of inv_inertia_world (within tolerance). */
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), GYRO_MASS, 1.0f, 1.0f, 0.5f);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(2.0f, 0.5f, 1.0f));
    rb.orientation = quat_from_axis_angle(
        vec3_normalize(vec3_create(1, 1, 0)), 0.7f);
    forge_physics_rigid_body_update_derived(&rb);
    /* I_world * I_world_inv should approximate identity */
    mat3 product = mat3_multiply(rb.inertia_world, rb.inv_inertia_world);
    mat3 id = mat3_identity();
    for (int i = 0; i < 9; i++) {
        ASSERT_NEAR(product.m[i], id.m[i], 1e-4f);
    }
    END_TEST();
}

static void test_rb_gyro_stability_asymmetric(void)
{
    TEST("gyroscopic — stability with asymmetric inertia over 600 steps");
    /* A long-running test: asymmetric body spinning freely should not
     * produce NaN or explode. */
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), GYRO_MASS, 1.0f, 0.999f, 0.5f);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(3.0f, 0.2f, 1.0f));
    rb.angular_velocity = vec3_create(2.0f, 8.0f, 1.0f);
    for (int i = 0; i < 600; i++) {
        forge_physics_rigid_body_integrate(&rb, GYRO_DT);
    }
    ASSERT_TRUE(isfinite(rb.angular_velocity.x));
    ASSERT_TRUE(isfinite(rb.angular_velocity.y));
    ASSERT_TRUE(isfinite(rb.angular_velocity.z));
    ASSERT_TRUE(isfinite(rb.orientation.w));
    float q_len = quat_length(rb.orientation);
    ASSERT_NEAR(q_len, 1.0f, 0.01f);
    END_TEST();
}

/* ── 52. NULL safety for rigid body functions ──────────────────────────── */

static void test_rb_get_transform_null(void)
{
    TEST("get_transform — NULL returns identity");
    mat4 m = forge_physics_rigid_body_get_transform(NULL);
    mat4 id = mat4_identity();
    for (int i = 0; i < 16; i++) {
        ASSERT_NEAR(m.m[i], id.m[i], EPSILON);
    }
    END_TEST();
}

static void test_rb_clear_forces_null(void)
{
    TEST("clear_forces — NULL does not crash");
    forge_physics_rigid_body_clear_forces(NULL);
    ASSERT_TRUE(1);  /* survived */
    END_TEST();
}

/* ── 53. Inertia setter edge cases ─────────────────────────────────────── */

static void test_rb_inertia_box_nan_rejected(void)
{
    TEST("inertia box — NaN half-extents rejected (no-op)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    /* Set valid inertia first */
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(1.0f, 1.0f, 1.0f));
    mat3 before = rb.inv_inertia_local;
    /* Attempt NaN — should be rejected */
    float nan_val = nanf("");
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(nan_val, 1.0f, 1.0f));
    /* Inertia should be unchanged */
    for (int i = 0; i < 9; i++) {
        ASSERT_NEAR(rb.inv_inertia_local.m[i], before.m[i], EPSILON);
    }
    END_TEST();
}

static void test_rb_inertia_sphere_inf_rejected(void)
{
    TEST("inertia sphere — Inf radius rejected (no-op)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 1.0f);
    mat3 before = rb.inv_inertia_local;
    float inf_val = INFINITY;
    forge_physics_rigid_body_set_inertia_sphere(&rb, inf_val);
    for (int i = 0; i < 9; i++) {
        ASSERT_NEAR(rb.inv_inertia_local.m[i], before.m[i], EPSILON);
    }
    END_TEST();
}

static void test_rb_inertia_cylinder_nan_rejected(void)
{
    TEST("inertia cylinder — NaN dimensions rejected (no-op)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_cylinder(&rb, 1.0f, 2.0f);
    mat3 before = rb.inv_inertia_local;
    float nan_val = nanf("");
    forge_physics_rigid_body_set_inertia_cylinder(&rb, nan_val, 2.0f);
    for (int i = 0; i < 9; i++) {
        ASSERT_NEAR(rb.inv_inertia_local.m[i], before.m[i], EPSILON);
    }
    END_TEST();
}

/* ── 54. Rigid body integrator edge cases ──────────────────────────────── */

static void test_rb_integrate_negative_dt(void)
{
    TEST("integrate — negative dt is rejected (no-op)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(RB_POS_X, RB_POS_Y, RB_POS_Z),
        RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    forge_physics_rigid_body_apply_force(&rb,
        vec3_create(RB_LARGE_FORCE, 0, 0));
    rb.angular_velocity = vec3_create(5.0f, 0.0f, 0.0f);
    forge_physics_rigid_body_integrate(&rb, -0.016f);
    ASSERT_NEAR(rb.position.x, RB_POS_X, EPSILON);
    ASSERT_NEAR(rb.velocity.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_integrate_nan_dt(void)
{
    TEST("integrate — NaN dt is rejected (no-op)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(RB_POS_X, RB_POS_Y, RB_POS_Z),
        RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    forge_physics_rigid_body_apply_force(&rb,
        vec3_create(RB_LARGE_FORCE, 0, 0));
    float nan_val = nanf("");
    forge_physics_rigid_body_integrate(&rb, nan_val);
    ASSERT_NEAR(rb.position.x, RB_POS_X, EPSILON);
    ASSERT_NEAR(rb.velocity.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_integrate_inf_dt(void)
{
    TEST("integrate — +Inf dt is rejected (no-op)");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(RB_POS_X, RB_POS_Y, RB_POS_Z),
        RB_MASS, RB_DAMPING, RB_ANG_DAMPING, RB_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, SPHERE_RADIUS);
    forge_physics_rigid_body_apply_force(&rb,
        vec3_create(RB_LARGE_FORCE, 0, 0));
    float inf_val = INFINITY;
    forge_physics_rigid_body_integrate(&rb, inf_val);
    ASSERT_NEAR(rb.position.x, RB_POS_X, EPSILON);
    ASSERT_NEAR(rb.velocity.x, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Lesson 05: Force Generators
 * ══════════════════════════════════════════════════════════════════════════ */

/* Test constants for force generator tests */
#define FG_MASS             5.0f
#define FG_MASS_ALT         2.0f    /* alternate mass for direction tests */
#define FG_DT               (1.0f / 60.0f)
#define FG_GRAVITY_Y        (-9.81f)
#define FG_DRAG_COEFF       2.0f
#define FG_ADRAG_COEFF      1.5f
#define FG_FRIC_COEFF       3.0f
#define FG_VELOCITY         10.0f
#define FG_ANG_VEL          8.0f
#define FG_TOL              0.01f
#define FG_RESTIT           0.5f
#define FG_LIN_DAMPING      1.0f
#define FG_ANG_DAMPING      1.0f
#define FG_SPHERE_RADIUS    0.5f
#define FG_INIT_Y           10.0f
#define FG_DROP_HEIGHT      100.0f
#define FG_COM_HEIGHT       1.0f
#define FG_FALL_SPEED       5.0f
#define FG_GRAV_DIR_X       3.0f
#define FG_GRAV_DIR_Z       (-2.0f)
/* Combined test constants (group 59) */
#define FG_COMB_LDRAG       0.5f
#define FG_COMB_ADRAG       0.3f
#define FG_COMB_FRICTION    2.0f
#define FG_COMB_DAMPING     0.99f
#define FG_BOX_EXT_X        1.0f
#define FG_BOX_EXT_Y        0.5f
#define FG_BOX_EXT_Z        0.3f
#define FG_COMB_VEL_X       5.0f
#define FG_COMB_VEL_Z       3.0f
#define FG_COMB_ANGVEL_X    2.0f
#define FG_COMB_ANGVEL_Y    5.0f
#define FG_COMB_ANGVEL_Z    1.0f
#define FG_GROUND_PROX      1.0f
#define FG_GROUND_LEVEL     0.5f
#define FG_TERMINAL_STEPS   3000
#define FG_STABILITY_STEPS  10000
#define FG_DETERM_STEPS     1000
#define FG_TERMINAL_TOL     0.05f   /* 5% relative tolerance */

/* ── 55. Gravity on rigid body ─────────────────────────────────────────── */

static void test_rb_gravity_basic(void)
{
    TEST("rb_gravity — basic gravity accumulates F = m * g");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_INIT_Y, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 1.0f);
    forge_physics_rigid_body_apply_gravity(&rb,
        vec3_create(0, FG_GRAVITY_Y, 0));
    /* F = m * g = 5 * -9.81 = -49.05 */
    ASSERT_NEAR(rb.force_accum.y, FG_MASS * FG_GRAVITY_Y, EPSILON);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.force_accum.z, 0.0f, EPSILON);
    /* Gravity at COM produces no torque */
    ASSERT_NEAR(rb.torque_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_gravity_static_noop(void)
{
    TEST("rb_gravity — static body unaffected");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), 0.0f, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_apply_gravity(&rb,
        vec3_create(0, FG_GRAVITY_Y, 0));
    ASSERT_NEAR(rb.force_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_gravity_direction(void)
{
    TEST("rb_gravity — arbitrary direction");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), FG_MASS_ALT, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 1.0f);
    forge_physics_rigid_body_apply_gravity(&rb,
        vec3_create(FG_GRAV_DIR_X, 0, FG_GRAV_DIR_Z));
    /* F = m * g = 2 * (3, 0, -2) = (6, 0, -4) */
    ASSERT_NEAR(rb.force_accum.x, FG_MASS_ALT * FG_GRAV_DIR_X, EPSILON);
    ASSERT_NEAR(rb.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(rb.force_accum.z, FG_MASS_ALT * FG_GRAV_DIR_Z, EPSILON);
    END_TEST();
}

static void test_rb_gravity_null_noop(void)
{
    TEST("rb_gravity — NULL does not crash");
    forge_physics_rigid_body_apply_gravity(NULL,
        vec3_create(0, FG_GRAVITY_Y, 0));
    ASSERT_TRUE(1);
    END_TEST();
}

/* ── 56. Linear drag ───────────────────────────────────────────────────── */

static void test_rb_linear_drag_basic(void)
{
    TEST("rb_linear_drag — F = -k * v");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 1.0f);
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    forge_physics_rigid_body_apply_linear_drag(&rb, FG_DRAG_COEFF);
    /* F = -2.0 * 10.0 = -20.0 */
    ASSERT_NEAR(rb.force_accum.x, -FG_DRAG_COEFF * FG_VELOCITY, EPSILON);
    ASSERT_NEAR(rb.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(rb.force_accum.z, 0.0f, EPSILON);
    /* No torque from linear drag */
    ASSERT_NEAR(rb.torque_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_linear_drag_zero_coeff_noop(void)
{
    TEST("rb_linear_drag — zero coefficient is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    forge_physics_rigid_body_apply_linear_drag(&rb, 0.0f);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_linear_drag_static_noop(void)
{
    TEST("rb_linear_drag — static body unaffected");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), 0.0f, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    forge_physics_rigid_body_apply_linear_drag(&rb, FG_DRAG_COEFF);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_linear_drag_negative_coeff_noop(void)
{
    TEST("rb_linear_drag — negative coefficient is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    forge_physics_rigid_body_apply_linear_drag(&rb, -1.0f);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_linear_drag_nan_coeff_noop(void)
{
    TEST("rb_linear_drag — NaN coefficient is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    forge_physics_rigid_body_apply_linear_drag(&rb, NAN);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_linear_drag_null_noop(void)
{
    TEST("rb_linear_drag — NULL does not crash");
    forge_physics_rigid_body_apply_linear_drag(NULL, FG_DRAG_COEFF);
    ASSERT_TRUE(1);
    END_TEST();
}

/* ── 57. Angular drag ──────────────────────────────────────────────────── */

static void test_rb_angular_drag_basic(void)
{
    TEST("rb_angular_drag — tau = -k * omega");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 1.0f);
    rb.angular_velocity = vec3_create(0.0f, FG_ANG_VEL, 0.0f);
    forge_physics_rigid_body_apply_angular_drag(&rb, FG_ADRAG_COEFF);
    /* tau = -1.5 * 8.0 = -12.0 */
    ASSERT_NEAR(rb.torque_accum.y, -FG_ADRAG_COEFF * FG_ANG_VEL, EPSILON);
    ASSERT_NEAR(rb.torque_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.torque_accum.z, 0.0f, EPSILON);
    /* No force from angular drag */
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.force_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_angular_drag_zero_coeff_noop(void)
{
    TEST("rb_angular_drag — zero coefficient is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.angular_velocity = vec3_create(0.0f, FG_ANG_VEL, 0.0f);
    forge_physics_rigid_body_apply_angular_drag(&rb, 0.0f);
    ASSERT_NEAR(rb.torque_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_angular_drag_static_noop(void)
{
    TEST("rb_angular_drag — static body unaffected");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), 0.0f, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.angular_velocity = vec3_create(0.0f, FG_ANG_VEL, 0.0f);
    forge_physics_rigid_body_apply_angular_drag(&rb, FG_ADRAG_COEFF);
    ASSERT_NEAR(rb.torque_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_angular_drag_negative_coeff_noop(void)
{
    TEST("rb_angular_drag — negative coefficient is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.angular_velocity = vec3_create(0.0f, FG_ANG_VEL, 0.0f);
    forge_physics_rigid_body_apply_angular_drag(&rb, -FG_ADRAG_COEFF);
    ASSERT_NEAR(rb.torque_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_angular_drag_nan_coeff_noop(void)
{
    TEST("rb_angular_drag — NaN coefficient is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.angular_velocity = vec3_create(0.0f, FG_ANG_VEL, 0.0f);
    forge_physics_rigid_body_apply_angular_drag(&rb, NAN);
    ASSERT_NEAR(rb.torque_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_angular_drag_null_noop(void)
{
    TEST("rb_angular_drag — NULL does not crash");
    forge_physics_rigid_body_apply_angular_drag(NULL, FG_ADRAG_COEFF);
    ASSERT_TRUE(1);
    END_TEST();
}

/* ── 58. Friction ──────────────────────────────────────────────────────── */

static void test_rb_friction_opposes_sliding(void)
{
    TEST("rb_friction — friction opposes tangential velocity");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_GROUND_LEVEL, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 0.5f);
    /* Sliding along +X on a ground plane (normal = +Y) */
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    vec3 normal = vec3_create(0, 1, 0);
    vec3 contact = vec3_create(0, 0, 0);
    forge_physics_rigid_body_apply_friction(&rb, normal, contact,
                                            FG_FRIC_COEFF);
    /* Friction should oppose +X velocity → force in -X direction */
    ASSERT_TRUE(rb.force_accum.x < -EPSILON);
    /* F = -coeff * |v_tangent| = -3 * 10 = -30 in X direction */
    ASSERT_NEAR(rb.force_accum.x, -FG_FRIC_COEFF * FG_VELOCITY, EPSILON);
    /* No force along normal (Y) from friction */
    ASSERT_NEAR(rb.force_accum.y, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_friction_normal_aligned_noop(void)
{
    TEST("rb_friction — velocity along normal produces no friction");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_GROUND_LEVEL, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 0.5f);
    /* Velocity straight down (along the normal) — no tangential component */
    rb.velocity = vec3_create(0.0f, -FG_FALL_SPEED, 0.0f);
    vec3 normal = vec3_create(0, 1, 0);
    vec3 contact = rb.position;
    forge_physics_rigid_body_apply_friction(&rb, normal, contact,
                                            FG_FRIC_COEFF);
    /* No tangential velocity → no friction force */
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    ASSERT_NEAR(rb.force_accum.y, 0.0f, EPSILON);
    ASSERT_NEAR(rb.force_accum.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_friction_zero_coeff_noop(void)
{
    TEST("rb_friction — zero coefficient is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_GROUND_LEVEL, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    forge_physics_rigid_body_apply_friction(&rb,
        vec3_create(0, 1, 0), vec3_create(0, 0, 0), 0.0f);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_friction_static_noop(void)
{
    TEST("rb_friction — static body unaffected");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, 0, 0), 0.0f, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    forge_physics_rigid_body_apply_friction(&rb,
        vec3_create(0, 1, 0), vec3_create(0, 0, 0), FG_FRIC_COEFF);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_friction_generates_torque(void)
{
    TEST("rb_friction — off-center contact generates torque");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_COM_HEIGHT, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 1.0f);
    /* Sliding along +X, contact at bottom of sphere (0, 0, 0) */
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    vec3 normal = vec3_create(0, 1, 0);
    vec3 contact = vec3_create(0, 0, 0);  /* below COM */
    forge_physics_rigid_body_apply_friction(&rb, normal, contact,
                                            FG_FRIC_COEFF);
    /* Friction force is in -X direction at contact point (0,0,0).
     * Offset from COM (0,1,0) to contact (0,0,0) = (0,-1,0).
     * Torque = offset × force = (0,-1,0) × (-30,0,0)
     *        = ((-1)*0 - 0*0, 0*(-30) - 0*0, 0*0 - (-1)*(-30))
     *        = (0, 0, -30)
     * Wait — that's cross((0,-1,0), (-30,0,0)):
     *   x = (-1)*0 - 0*0     = 0
     *   y = 0*(-30) - 0*0    = 0
     *   z = 0*0 - (-1)*(-30) = -30
     * So torque should be in -Z direction. */
    ASSERT_TRUE(SDL_fabsf(rb.torque_accum.z) > EPSILON);
    /* Torque z should be negative (friction causes backward spin) */
    ASSERT_TRUE(rb.torque_accum.z < 0.0f);
    END_TEST();
}

static void test_rb_friction_spinning_body_gets_friction(void)
{
    TEST("rb_friction — spinning body with zero linear velocity gets friction");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_COM_HEIGHT, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, FG_SPHERE_RADIUS);
    /* Zero linear velocity, spinning around Y axis */
    rb.velocity = vec3_create(0.0f, 0.0f, 0.0f);
    rb.angular_velocity = vec3_create(0.0f, FG_ANG_VEL, 0.0f);
    vec3 normal = vec3_create(0, 1, 0);
    /* Contact point at bottom of sphere (on the ground) */
    vec3 contact = vec3_create(0, 0, 0);
    /* r = contact - position = (0,-1,0)
     * v_contact = v + omega x r = 0 + (0,8,0) x (0,-1,0)
     *           = (8*0 - 0*(-1), 0*0 - 0*0, 0*(-1) - 8*0)
     *           = (0, 0, 0)  — spinning around Y with r along Y produces no tangent
     * Actually: cross((0,8,0), (0,-1,0)):
     *   x = 8*0 - 0*(-1) = 0
     *   y = 0*0 - 0*0    = 0
     *   z = 0*(-1) - 8*0 = 0
     * So no friction — angular velocity parallel to r. Try spinning around Z: */
    rb.angular_velocity = vec3_create(0.0f, 0.0f, FG_ANG_VEL);
    /* r = (0,-1,0), omega = (0,0,8)
     * omega x r = (0,0,8) x (0,-1,0)
     *   x = 0*0 - 8*(-1)  = 8
     *   y = 8*0 - 0*0     = 0
     *   z = 0*(-1) - 0*0  = 0
     * v_contact = (8, 0, 0) — tangent to ground plane! */
    forge_physics_rigid_body_apply_friction(&rb, normal, contact,
                                            FG_FRIC_COEFF);
    /* Friction should oppose the +X contact-point velocity */
    ASSERT_TRUE(rb.force_accum.x < -EPSILON);
    /* v_contact tangent speed = 8, so F = -coeff * 8 = -3 * 8 = -24 */
    float expected_force = -FG_FRIC_COEFF * FG_ANG_VEL;
    ASSERT_NEAR(rb.force_accum.x, expected_force, FG_TOL);
    END_TEST();
}

static void test_rb_friction_nan_coeff_noop(void)
{
    TEST("rb_friction — NaN coefficient is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_GROUND_LEVEL, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    forge_physics_rigid_body_apply_friction(&rb,
        vec3_create(0, 1, 0), vec3_create(0, 0, 0), NAN);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_friction_negative_coeff_noop(void)
{
    TEST("rb_friction — negative coefficient is no-op");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_GROUND_LEVEL, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    rb.velocity = vec3_create(FG_VELOCITY, 0.0f, 0.0f);
    forge_physics_rigid_body_apply_friction(&rb,
        vec3_create(0, 1, 0), vec3_create(0, 0, 0), -FG_FRIC_COEFF);
    ASSERT_NEAR(rb.force_accum.x, 0.0f, EPSILON);
    END_TEST();
}

static void test_rb_friction_null_noop(void)
{
    TEST("rb_friction — NULL does not crash");
    forge_physics_rigid_body_apply_friction(NULL,
        vec3_create(0, 1, 0), vec3_create(0, 0, 0), FG_FRIC_COEFF);
    ASSERT_TRUE(1);
    END_TEST();
}

/* ── 59. Combined force generator effects ──────────────────────────────── */

static void test_rb_gravity_drag_terminal_velocity(void)
{
    TEST("combined — gravity + drag approaches terminal velocity");
    /* Terminal velocity: v_term = m * g / k
     * m=5, g=9.81, k=2 → v_term = 5*9.81/2 = 24.525 m/s */
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_DROP_HEIGHT, 0), FG_MASS, FG_LIN_DAMPING, FG_ANG_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_sphere(&rb, 1.0f);
    float v_term = FG_MASS * SDL_fabsf(FG_GRAVITY_Y) / FG_DRAG_COEFF;
    /* Run for many steps to approach terminal velocity */
    for (int i = 0; i < FG_TERMINAL_STEPS; i++) {
        forge_physics_rigid_body_apply_gravity(&rb,
            vec3_create(0, FG_GRAVITY_Y, 0));
        forge_physics_rigid_body_apply_linear_drag(&rb, FG_DRAG_COEFF);
        forge_physics_rigid_body_integrate(&rb, FG_DT);
    }
    /* Vertical speed should be near terminal velocity */
    float vy = SDL_fabsf(rb.velocity.y);
    ASSERT_NEAR(vy, v_term, v_term * FG_TERMINAL_TOL);  /* within 5% */
    END_TEST();
}

static void test_rb_all_generators_no_nan(void)
{
    TEST("combined — all generators for 10000 steps, no NaN/Inf");
    ForgePhysicsRigidBody rb = forge_physics_rigid_body_create(
        vec3_create(0, FG_INIT_Y, 0), FG_MASS, FG_COMB_DAMPING, FG_COMB_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb,
        vec3_create(FG_BOX_EXT_X, FG_BOX_EXT_Y, FG_BOX_EXT_Z));
    rb.velocity = vec3_create(FG_COMB_VEL_X, 0.0f, FG_COMB_VEL_Z);
    rb.angular_velocity = vec3_create(FG_COMB_ANGVEL_X, FG_COMB_ANGVEL_Y, FG_COMB_ANGVEL_Z);
    vec3 ground_normal = vec3_create(0, 1, 0);
    for (int i = 0; i < FG_STABILITY_STEPS; i++) {
        forge_physics_rigid_body_apply_gravity(&rb,
            vec3_create(0, FG_GRAVITY_Y, 0));
        forge_physics_rigid_body_apply_linear_drag(&rb, FG_COMB_LDRAG);
        forge_physics_rigid_body_apply_angular_drag(&rb, FG_COMB_ADRAG);
        /* Apply friction when near ground */
        if (rb.position.y < FG_GROUND_PROX) {
            vec3 contact = rb.position;
            contact.y = 0.0f;
            forge_physics_rigid_body_apply_friction(&rb, ground_normal,
                                                    contact, FG_COMB_FRICTION);
        }
        forge_physics_rigid_body_integrate(&rb, FG_DT);
        /* Simple ground collision */
        if (rb.position.y < FG_GROUND_LEVEL) {
            rb.position.y = FG_GROUND_LEVEL;
            if (rb.velocity.y < 0.0f)
                rb.velocity.y = -rb.velocity.y * rb.restitution;
        }
    }
    ASSERT_TRUE(isfinite(rb.position.x));
    ASSERT_TRUE(isfinite(rb.position.y));
    ASSERT_TRUE(isfinite(rb.position.z));
    ASSERT_TRUE(isfinite(rb.velocity.x));
    ASSERT_TRUE(isfinite(rb.velocity.y));
    ASSERT_TRUE(isfinite(rb.velocity.z));
    ASSERT_TRUE(isfinite(rb.angular_velocity.x));
    ASSERT_TRUE(isfinite(rb.angular_velocity.y));
    ASSERT_TRUE(isfinite(rb.angular_velocity.z));
    ASSERT_TRUE(isfinite(rb.orientation.w));
    float q_len = quat_length(rb.orientation);
    ASSERT_NEAR(q_len, 1.0f, FG_TOL);
    END_TEST();
}

static void test_rb_force_generators_determinism(void)
{
    TEST("combined — deterministic with all generators");
    ForgePhysicsRigidBody rb1 = forge_physics_rigid_body_create(
        vec3_create(0, FG_INIT_Y, 0), FG_MASS, FG_COMB_DAMPING, FG_COMB_DAMPING, FG_RESTIT);
    forge_physics_rigid_body_set_inertia_box(&rb1,
        vec3_create(FG_BOX_EXT_X, FG_BOX_EXT_Y, FG_BOX_EXT_Z));
    rb1.velocity = vec3_create(FG_COMB_VEL_X, 0.0f, FG_COMB_VEL_Z);
    rb1.angular_velocity = vec3_create(FG_COMB_ANGVEL_X, FG_COMB_ANGVEL_Y, FG_COMB_ANGVEL_Z);
    ForgePhysicsRigidBody rb2 = rb1;
    vec3 ground_normal = vec3_create(0, 1, 0);
    for (int i = 0; i < FG_DETERM_STEPS; i++) {
        forge_physics_rigid_body_apply_gravity(&rb1,
            vec3_create(0, FG_GRAVITY_Y, 0));
        forge_physics_rigid_body_apply_linear_drag(&rb1, FG_COMB_LDRAG);
        forge_physics_rigid_body_apply_angular_drag(&rb1, FG_COMB_ADRAG);
        if (rb1.position.y < FG_GROUND_PROX) {
            vec3 cp1 = rb1.position; cp1.y = 0.0f;
            forge_physics_rigid_body_apply_friction(&rb1, ground_normal,
                cp1, FG_COMB_FRICTION);
        }
        forge_physics_rigid_body_integrate(&rb1, FG_DT);

        forge_physics_rigid_body_apply_gravity(&rb2,
            vec3_create(0, FG_GRAVITY_Y, 0));
        forge_physics_rigid_body_apply_linear_drag(&rb2, FG_COMB_LDRAG);
        forge_physics_rigid_body_apply_angular_drag(&rb2, FG_COMB_ADRAG);
        if (rb2.position.y < FG_GROUND_PROX) {
            vec3 cp2 = rb2.position; cp2.y = 0.0f;
            forge_physics_rigid_body_apply_friction(&rb2, ground_normal,
                cp2, FG_COMB_FRICTION);
        }
        forge_physics_rigid_body_integrate(&rb2, FG_DT);
    }
    /* Compare full rigid body state */
    ASSERT_NEAR(rb1.position.x, rb2.position.x, EPSILON);
    ASSERT_NEAR(rb1.position.y, rb2.position.y, EPSILON);
    ASSERT_NEAR(rb1.position.z, rb2.position.z, EPSILON);
    ASSERT_NEAR(rb1.velocity.x, rb2.velocity.x, EPSILON);
    ASSERT_NEAR(rb1.velocity.y, rb2.velocity.y, EPSILON);
    ASSERT_NEAR(rb1.velocity.z, rb2.velocity.z, EPSILON);
    ASSERT_NEAR(rb1.angular_velocity.x, rb2.angular_velocity.x, EPSILON);
    ASSERT_NEAR(rb1.angular_velocity.y, rb2.angular_velocity.y, EPSILON);
    ASSERT_NEAR(rb1.angular_velocity.z, rb2.angular_velocity.z, EPSILON);
    ASSERT_NEAR(rb1.orientation.w, rb2.orientation.w, EPSILON);
    ASSERT_NEAR(rb1.orientation.x, rb2.orientation.x, EPSILON);
    ASSERT_NEAR(rb1.orientation.y, rb2.orientation.y, EPSILON);
    ASSERT_NEAR(rb1.orientation.z, rb2.orientation.z, EPSILON);
    END_TEST();
}

/* Rigid body contact tests are in test_physics_rbc.c — called via run_rbc_tests() */

/* ══════════════════════════════════════════════════════════════════════════
 * Main — run all tests
 * ══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_Log("=== Physics Library Tests ===\n");

    /* 1. forge_physics_particle_create */
    SDL_Log("--- particle_create ---");
    test_particle_create_normal();
    test_particle_create_static();
    test_particle_create_clamping();
    test_particle_create_all_fields_initialized();

    /* 2. forge_physics_apply_gravity */
    SDL_Log("--- apply_gravity ---");
    test_apply_gravity_normal();
    test_apply_gravity_static();
    test_apply_gravity_custom_direction();

    /* 3. forge_physics_apply_drag */
    SDL_Log("--- apply_drag ---");
    test_apply_drag_normal();
    test_apply_drag_static();
    test_apply_drag_zero_velocity();
    test_apply_drag_negative_coefficient();

    /* 4. forge_physics_integrate */
    SDL_Log("--- integrate ---");
    test_integrate_gravity_one_second();
    test_integrate_zero_dt();
    test_integrate_static_particle();
    test_integrate_velocity_clamping();

    /* 5. forge_physics_collide_plane */
    SDL_Log("--- collide_plane ---");
    test_collide_plane_no_collision();
    test_collide_plane_penetrating();
    test_collide_plane_restitution();
    test_collide_plane_static_particle();
    test_collide_plane_zero_normal();
    test_collide_plane_static_unchanged();

    /* 6. Determinism */
    SDL_Log("--- determinism ---");
    test_determinism();

    /* 7. Energy stability */
    SDL_Log("--- energy stability ---");
    test_energy_stability_no_nan();
    test_energy_stability_bounded();

    /* 8. forge_physics_clear_forces */
    SDL_Log("--- clear_forces ---");
    test_clear_forces();

    /* 9. forge_physics_apply_force (direct) */
    SDL_Log("--- apply_force ---");
    test_apply_force_direct();

    /* 10. integrate — negative dt */
    SDL_Log("--- integrate (negative dt) ---");
    test_integrate_negative_dt();

    /* 11. collide_plane — zero restitution */
    SDL_Log("--- collide_plane (zero restitution) ---");
    test_collide_plane_zero_restitution();

    /* 12. particle_create — negative mass */
    SDL_Log("--- particle_create (negative mass) ---");
    test_particle_create_negative_mass();

    /* 13. Very heavy particle must not be treated as static */
    SDL_Log("--- very heavy particle (inv_mass < EPSILON) ---");
    test_very_heavy_particle_not_static();

    /* 14. forge_physics_spring_create */
    SDL_Log("--- spring_create ---");
    test_spring_create_normal();
    test_spring_create_zero_stiffness();
    test_spring_create_zero_damping();
    test_spring_create_zero_rest_length();

    /* 15. Spring force correctness */
    SDL_Log("--- spring_apply (force correctness) ---");
    test_spring_force_extension();
    test_spring_force_compression();
    test_spring_force_diagonal();

    /* 16. Spring force symmetry */
    SDL_Log("--- spring_apply (Newton's third law) ---");
    test_spring_force_symmetry();

    /* 17. Damped spring behavior */
    SDL_Log("--- spring (damped oscillation) ---");
    test_spring_damped_amplitude_decreases();
    test_spring_damped_energy_decreases();

    /* 18. Spring + static particle */
    SDL_Log("--- spring_apply (static particle) ---");
    test_spring_static_particle_no_force();
    test_spring_static_position_unchanged();

    /* 19. Spring degenerate cases */
    SDL_Log("--- spring_apply (degenerate cases) ---");
    test_spring_coincident_particles();
    test_spring_out_of_bounds_indices();

    /* 20. forge_physics_constraint_distance_create */
    SDL_Log("--- constraint_distance_create ---");
    test_constraint_create_normal();
    test_constraint_create_zero_distance();
    test_constraint_create_stiffness_clamped();

    /* 21. Constraint projection */
    SDL_Log("--- constraint_solve_distance (projection) ---");
    test_constraint_projection_pull_in();
    test_constraint_projection_push_apart();

    /* 22. Constraint mass weighting */
    SDL_Log("--- constraint_solve_distance (mass weighting) ---");
    test_constraint_mass_weighting();

    /* 23. Constraint + static particle */
    SDL_Log("--- constraint_solve_distance (static particle) ---");
    test_constraint_static_particle();

    /* 24. Multi-constraint solver */
    SDL_Log("--- constraints_solve (Gauss-Seidel) ---");
    test_constraints_solve_chain_converges();
    test_constraints_solve_more_iterations_better();

    /* 25. Constraint stability */
    SDL_Log("--- constraint (stability) ---");
    test_constraint_stability_no_nan();

    /* 26. Spring-constraint comparison */
    SDL_Log("--- spring vs constraint comparison ---");
    test_spring_constraint_comparison();

    /* 27. Spring + constraint determinism */
    SDL_Log("--- spring+constraint determinism ---");
    test_spring_constraint_determinism();

    /* 28. NULL pointer safety */
    SDL_Log("--- NULL pointer safety ---");
    test_spring_apply_null_spring();
    test_spring_apply_null_particles();
    test_spring_apply_zero_count();
    test_constraint_solve_null_constraint();
    test_constraint_solve_null_particles();
    test_constraints_solve_null_constraints();
    test_constraints_solve_null_particles();

    /* 29. Both-static early returns */
    SDL_Log("--- both-static early returns ---");
    test_spring_both_static_no_force();
    test_constraint_both_static_no_movement();

    /* 30. NaN guard — constraint_distance_create */
    SDL_Log("--- NaN guard (constraint_distance_create) ---");
    test_constraint_create_nan_stiffness();
    test_constraint_create_nan_distance();

    /* 31. Empty batch + iteration clamping */
    SDL_Log("--- empty batch + iteration clamping ---");
    test_constraints_solve_zero_constraints();
    test_constraints_solve_zero_particles();
    test_constraints_solve_negative_iterations();

    /* 32. Sphere-sphere detection */
    SDL_Log("--- collide_sphere_sphere ---");
    test_collide_sphere_overlapping();
    test_collide_sphere_touching();
    test_collide_sphere_separated();
    test_collide_sphere_coincident();
    test_collide_sphere_zero_radius();
    test_collide_sphere_both_static();
    test_collide_sphere_one_static();
    test_collide_sphere_asymmetric_radii();

    /* 33. Impulse response */
    SDL_Log("--- resolve_contact ---");
    test_resolve_equal_mass_elastic();
    test_resolve_unequal_mass();
    test_resolve_one_static();
    test_resolve_zero_restitution();
    test_resolve_separating_pair();

    /* 34. Conservation laws */
    SDL_Log("--- conservation laws ---");
    test_collision_momentum_conservation();
    test_collision_energy_elastic();
    test_collision_energy_inelastic();

    /* 35. All-pairs detection */
    SDL_Log("--- collide_particles_all ---");
    test_all_pairs_correct_count();

    /* 36. Determinism */
    SDL_Log("--- collision determinism ---");
    test_collision_determinism();

    /* 37. All-pairs boundary conditions */
    SDL_Log("--- all-pairs boundary conditions ---");
    test_all_pairs_zero_particles();
    test_all_pairs_single_particle();
    test_all_pairs_no_overlaps();
    test_all_pairs_all_overlap();
    test_all_pairs_exceeds_max_contacts();
    test_all_pairs_contact_ordering();

    /* 38. 3D geometry (non-axis-aligned) */
    SDL_Log("--- 3D geometry ---");
    test_collide_sphere_diagonal();
    test_collide_sphere_z_axis();
    test_resolve_contact_3d();
    test_contact_point_midpoint_3d();

    /* 39. Positional correction */
    SDL_Log("--- positional correction ---");
    test_resolve_position_correction_equal_mass();
    test_resolve_position_correction_one_static();
    test_resolve_position_correction_unequal_mass();
    test_resolve_no_overlap_no_correction();

    /* 40. Resting threshold */
    SDL_Log("--- resting threshold ---");
    test_resolve_low_velocity_no_bounce();
    test_resolve_above_threshold_bounces();

    /* 41. NULL safety and bounds checking for collision functions */
    SDL_Log("--- NULL safety (collision functions) ---");
    test_resolve_contact_out_of_bounds();
    test_resolve_contact_null_contact();
    test_resolve_contact_null_particles();
    test_resolve_contacts_null_contacts();
    test_resolve_contacts_zero_count();
    test_all_pairs_null_particles();
    test_all_pairs_null_contacts();
    test_step_null_particles();
    test_step_null_contacts();

    /* 42. Convenience function (collide_particles_step) */
    SDL_Log("--- collide_particles_step ---");
    test_step_detects_and_resolves();
    test_step_no_collisions();
    test_step_multiple_contacts();

    /* ── Lesson 04: Rigid Body State and Orientation ──────────────────── */

    /* 43. Rigid body create */
    SDL_Log("--- rigid body create ---");
    test_rb_create_defaults();
    test_rb_create_static();
    test_rb_create_negative_mass();
    test_rb_create_clamping();
    test_rb_create_prev_fields();
    test_rb_create_large_mass();

    /* 44. Inertia tensor */
    SDL_Log("--- inertia tensor ---");
    test_rb_inertia_box_unit_cube();
    test_rb_inertia_box_nonuniform();
    test_rb_inertia_sphere();
    test_rb_inertia_sphere_r2();
    test_rb_inertia_cylinder();
    test_rb_inertia_static_body();
    test_rb_inertia_symmetry();

    /* 45. Force / torque */
    SDL_Log("--- force / torque ---");
    test_rb_force_at_center();
    test_rb_force_accumulates();
    test_rb_force_at_point_torque();
    test_rb_force_at_point_nonorigin_com();
    test_rb_torque_angular_acceleration_magnitude();
    test_rb_apply_torque_direct();
    test_rb_force_static_noop();
    test_rb_force_zero();
    test_rb_clear_forces();

    /* 46. Integration */
    SDL_Log("--- integration ---");
    test_rb_integrate_pure_linear();
    test_rb_integrate_pure_rotation();
    test_rb_integrate_quat_stays_unit();
    test_rb_integrate_static_noop();
    test_rb_integrate_zero_dt();
    test_rb_integrate_velocity_clamp();
    test_rb_integrate_angular_velocity_clamp();
    test_rb_integrate_prev_saved();
    test_rb_integrate_clears_accumulators();

    /* 47. Damping */
    SDL_Log("--- damping ---");
    test_rb_damping_linear();
    test_rb_damping_angular();
    test_rb_damping_one_preserves();
    test_rb_damping_zero_zeroes();

    /* 48. World-space inertia */
    SDL_Log("--- world-space inertia ---");
    test_rb_world_inertia_identity_orient();
    test_rb_world_inertia_sphere_invariant();
    test_rb_world_inertia_90_rotation();
    test_rb_world_inertia_after_integration();

    /* 49. Transform */
    SDL_Log("--- transform ---");
    test_rb_transform_identity();
    test_rb_transform_translation_only();
    test_rb_transform_combined();

    /* 50. Conservation and stability */
    SDL_Log("--- conservation and stability ---");
    test_rb_no_nan_after_many_steps();
    test_rb_determinism();

    /* 51. Gyroscopic term */
    SDL_Log("--- gyroscopic term ---");
    test_rb_gyro_sphere_no_effect();
    test_rb_gyro_box_coupling();
    test_rb_gyro_angular_momentum_conservation();
    test_rb_gyro_inertia_world_field();
    test_rb_gyro_stability_asymmetric();

    /* 52. NULL safety for rigid body functions */
    SDL_Log("--- NULL safety (rigid body) ---");
    test_rb_get_transform_null();
    test_rb_clear_forces_null();

    /* 53. Inertia setter edge cases */
    SDL_Log("--- inertia edge cases ---");
    test_rb_inertia_box_nan_rejected();
    test_rb_inertia_sphere_inf_rejected();
    test_rb_inertia_cylinder_nan_rejected();

    /* 54. Rigid body integrator edge cases */
    SDL_Log("--- rigid body integrate edge cases ---");
    test_rb_integrate_negative_dt();
    test_rb_integrate_nan_dt();
    test_rb_integrate_inf_dt();

    /* ── Lesson 05: Force Generators ───────────────────────────────────── */

    /* 55. Gravity on rigid body */
    SDL_Log("--- rigid body gravity ---");
    test_rb_gravity_basic();
    test_rb_gravity_static_noop();
    test_rb_gravity_direction();
    test_rb_gravity_null_noop();

    /* 56. Linear drag */
    SDL_Log("--- rigid body linear drag ---");
    test_rb_linear_drag_basic();
    test_rb_linear_drag_zero_coeff_noop();
    test_rb_linear_drag_static_noop();
    test_rb_linear_drag_negative_coeff_noop();
    test_rb_linear_drag_nan_coeff_noop();
    test_rb_linear_drag_null_noop();

    /* 57. Angular drag */
    SDL_Log("--- rigid body angular drag ---");
    test_rb_angular_drag_basic();
    test_rb_angular_drag_zero_coeff_noop();
    test_rb_angular_drag_static_noop();
    test_rb_angular_drag_negative_coeff_noop();
    test_rb_angular_drag_nan_coeff_noop();
    test_rb_angular_drag_null_noop();

    /* 58. Friction */
    SDL_Log("--- rigid body friction ---");
    test_rb_friction_opposes_sliding();
    test_rb_friction_normal_aligned_noop();
    test_rb_friction_zero_coeff_noop();
    test_rb_friction_static_noop();
    test_rb_friction_generates_torque();
    test_rb_friction_spinning_body_gets_friction();
    test_rb_friction_nan_coeff_noop();
    test_rb_friction_negative_coeff_noop();
    test_rb_friction_null_noop();

    /* 59. Combined force generator effects */
    SDL_Log("--- combined force generators ---");
    test_rb_gravity_drag_terminal_velocity();
    test_rb_all_generators_no_nan();
    test_rb_force_generators_determinism();

    /* ── Rigid Body Contacts (test_physics_rbc.c) ────────────────────────── */
    run_rbc_tests();

    /* ── Collision Shapes (test_physics_shapes.c) ─────────────────────── */
    run_collision_shape_tests();

    /* ── SAP Broadphase (test_physics_sap.c) ──────────────────────────── */
    run_sap_tests();

    /* Report results */
    SDL_Log("\n=== Results: %d/%d passed, %d failed ===",
            pass_count, test_count, fail_count);

    return (fail_count > 0) ? 1 : 0;
}
