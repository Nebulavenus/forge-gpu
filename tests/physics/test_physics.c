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

#include <SDL3/SDL.h>
#include <math.h>
#include "math/forge_math.h"
#include "physics/forge_physics.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

/* ── Shared test constants ─────────────────────────────────────────────── */

#define EPSILON           0.001f
#define PHYSICS_DT        (1.0f / 60.0f)

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

#define TEST(name) \
    do { \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_NEAR(a, b, eps) \
    if (fabsf((a) - (b)) > (eps)) { \
        SDL_Log("    FAIL: Expected %.6f, got %.6f (eps=%.6f)", \
                (double)(b), (double)(a), (double)(eps)); \
        fail_count++; \
        return; \
    }

#define ASSERT_TRUE(cond) \
    if (!(cond)) { \
        SDL_Log("    FAIL: Condition false: %s", #cond); \
        fail_count++; \
        return; \
    }

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

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
    ASSERT_TRUE(fabsf(p.position.x) < NRG_POS_BOUND);
    ASSERT_TRUE(fabsf(p.position.y) < NRG_POS_BOUND);
    ASSERT_TRUE(fabsf(p.position.z) < NRG_POS_BOUND);
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
     * less than FORGE_PHYSICS_EPSILON. Under the old fabsf(inv_mass) < EPSILON
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
    ASSERT_NEAR(p.force_accum.y, fy, fabsf(fy) * 0.001f);

    /* Integrate with dt = 1/60 — particle must move */
    float orig_y = p.position.y;
    forge_physics_integrate(&p, PHYSICS_DT);
    ASSERT_TRUE(fabsf(p.position.y - orig_y) > EPSILON);

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

    /* Report results */
    SDL_Log("\n=== Results: %d/%d passed, %d failed ===",
            pass_count, test_count, fail_count);

    return (fail_count > 0) ? 1 : 0;
}
