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

        float disp = fabsf(particles[1].position.x - SP_REST_TEST);
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
    ASSERT_TRUE(fabsf(particles[1].force_accum.x) > EPSILON);
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
        float d_few = fabsf(vec3_length(
            vec3_sub(p_few[i + 1].position, p_few[i].position)) - CHAIN_TARGET);
        float d_many = fabsf(vec3_length(
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
    ASSERT_TRUE(fabsf(cp_dist - CMP_REST_LEN) < CMP_CONSTRAINT_TOL);
    ASSERT_TRUE(fabsf(sp_dist - CMP_REST_LEN) < CMP_SPRING_TOL); /* spring has gravity sag */
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
    float dist = fabsf(particles[1].position.x - particles[0].position.x);
    ASSERT_NEAR(dist, GUARD_DC_DIST, EPSILON);
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

    /* Report results */
    SDL_Log("\n=== Results: %d/%d passed, %d failed ===",
            pass_count, test_count, fail_count);

    return (fail_count > 0) ? 1 : 0;
}
