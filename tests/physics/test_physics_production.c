/*
 * Physics Library Production Hardening Tests
 *
 * Tests for dynamic growth, memory lifecycle, per-frame reuse, edge cases,
 * stress, and numerical safety with the dynamic container-backed physics
 * library.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Constants ─────────────────────────────────────────────────────────── */

/* Shared particle properties */
#define PROD_MASS              1.0f
#define PROD_RADIUS            0.5f
#define PROD_RESTIT            0.8f
#define PROD_DAMPING           0.0f

/* AABB half-extent for unit-sized test boxes */
#define PROD_AABB_HALF         0.5f

/* Spacing multipliers */
#define PROD_WIDE_SPACING      10.0f   /* bodies far apart — no overlaps */
#define PROD_CHAIN_SPACING     0.8f    /* adjacent bodies overlap */
#define PROD_PARTICLE_SPACING  0.3f    /* particles within collision range */
#define PROD_STRESS_SPACING    0.6f    /* stress test initial positions */

/* Stress test physics */
#define PROD_STRESS_DAMPING    0.01f
#define PROD_GRAVITY           9.81f
#define PROD_DT                (1.0f / 60.0f)
#define PROD_STRESS_START_Y    5.0f

/* Per-frame reuse thresholds */
#define PROD_WARMUP_FRAMES     5       /* frames before checking capacity stability */
#define PROD_SAP_WARMUP_FRAMES 2

/* Numerical safety test magnitudes */
#define PROD_LARGE_POS         1e6f
#define PROD_TINY_SEPARATION   1e-7f

/* ── Dynamic growth tests ─────────────────────────────────────────────── */

static void test_prod_particles_all_contacts(void)
{
    TEST("PROD: 20 overlapping particles — all 190 contacts detected");
    /* 20 particles at the same spot: C(20,2) = 190 contacts. */
    #define OVERLAP_COUNT 20
    ForgePhysicsParticle particles[OVERLAP_COUNT];
    for (int i = 0; i < OVERLAP_COUNT; i++) {
        particles[i] = forge_physics_particle_create(
            vec3_create(0.0f, 0.0f, 0.0f),
            PROD_MASS, PROD_DAMPING, PROD_RESTIT, PROD_RADIUS);
    }

    ForgePhysicsContact *contacts = NULL;
    int n = forge_physics_collide_particles_all(
        particles, OVERLAP_COUNT, &contacts);

    int expected = OVERLAP_COUNT * (OVERLAP_COUNT - 1) / 2;
    ASSERT_TRUE(n == expected);
    forge_arr_free(contacts);
    #undef OVERLAP_COUNT
    END_TEST();
}

static void test_prod_sap_all_overlapping(void)
{
    TEST("PROD: 10 SAP bodies all overlapping — all pairs detected");
    /* 10 bodies all overlapping: C(10,2) = 45 pairs */
    #define SAP_OVERLAP_COUNT 10
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[SAP_OVERLAP_COUNT];
    for (int i = 0; i < SAP_OVERLAP_COUNT; i++) {
        aabbs[i].min = vec3_create(-PROD_AABB_HALF, -PROD_AABB_HALF, -PROD_AABB_HALF);
        aabbs[i].max = vec3_create(PROD_AABB_HALF, PROD_AABB_HALF, PROD_AABB_HALF);
    }
    forge_physics_sap_update(&w, aabbs, SAP_OVERLAP_COUNT);
    int expected = SAP_OVERLAP_COUNT * (SAP_OVERLAP_COUNT - 1) / 2;
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == expected);
    forge_physics_sap_destroy(&w);
    #undef SAP_OVERLAP_COUNT
    END_TEST();
}

static void test_prod_500_sap_bodies_separated(void)
{
    TEST("PROD: 500 separated SAP bodies — arrays scale, zero pairs");
    #define P500S_COUNT 500
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB *aabbs = (ForgePhysicsAABB *)SDL_calloc(
        P500S_COUNT, sizeof(ForgePhysicsAABB));
    ASSERT_TRUE(aabbs != NULL);
    for (int i = 0; i < P500S_COUNT; i++) {
        float x = (float)i * PROD_WIDE_SPACING;
        aabbs[i].min = vec3_create(x - PROD_AABB_HALF, -PROD_AABB_HALF, -PROD_AABB_HALF);
        aabbs[i].max = vec3_create(x + PROD_AABB_HALF, PROD_AABB_HALF, PROD_AABB_HALF);
    }
    forge_physics_sap_update(&w, aabbs, P500S_COUNT);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    ASSERT_TRUE((int)forge_arr_length(w.endpoints) == P500S_COUNT * 2);
    SDL_free(aabbs);
    forge_physics_sap_destroy(&w);
    #undef P500S_COUNT
    END_TEST();
}

/* ── Memory lifecycle tests ───────────────────────────────────────────── */

static void test_prod_sap_init_destroy_empty(void)
{
    TEST("PROD: SAP init → destroy with zero bodies");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    forge_physics_sap_destroy(&w);
    ASSERT_TRUE(w.endpoints == NULL);
    ASSERT_TRUE(w.pairs == NULL);
    END_TEST();
}

static void test_prod_sap_init_populate_destroy_reinit(void)
{
    TEST("PROD: SAP init → populate → destroy → re-init → populate");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    ForgePhysicsAABB aabbs[3];
    for (int i = 0; i < 3; i++) {
        aabbs[i].min = vec3_create(-PROD_AABB_HALF, -PROD_AABB_HALF, -PROD_AABB_HALF);
        aabbs[i].max = vec3_create(PROD_AABB_HALF, PROD_AABB_HALF, PROD_AABB_HALF);
    }
    forge_physics_sap_update(&w, aabbs, 3);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 3);

    forge_physics_sap_destroy(&w);
    forge_physics_sap_init(&w);

    /* Reuse after re-init */
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_prod_sap_double_destroy(void)
{
    TEST("PROD: SAP double-destroy safety");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    ForgePhysicsAABB aabbs[2];
    aabbs[0].min = vec3_create(-PROD_AABB_HALF, -PROD_AABB_HALF, -PROD_AABB_HALF);
    aabbs[0].max = vec3_create(PROD_AABB_HALF, PROD_AABB_HALF, PROD_AABB_HALF);
    aabbs[1] = aabbs[0];
    forge_physics_sap_update(&w, aabbs, 2);

    forge_physics_sap_destroy(&w);
    forge_physics_sap_destroy(&w);  /* must not crash */
    ASSERT_TRUE(w.endpoints == NULL);
    END_TEST();
}

static void test_prod_sap_lifecycle_loop(void)
{
    TEST("PROD: 1000 cycles of init → add 10 bodies → update → destroy");
    #define LIFECYCLE_CYCLES 1000
    #define LIFECYCLE_BODIES 10
    for (int cycle = 0; cycle < LIFECYCLE_CYCLES; cycle++) {
        ForgePhysicsSAPWorld w;
        forge_physics_sap_init(&w);

        ForgePhysicsAABB aabbs[LIFECYCLE_BODIES];
        for (int i = 0; i < LIFECYCLE_BODIES; i++) {
            float x = (float)i * PROD_CHAIN_SPACING;
            aabbs[i].min = vec3_create(x - PROD_AABB_HALF, -PROD_AABB_HALF, -PROD_AABB_HALF);
            aabbs[i].max = vec3_create(x + PROD_AABB_HALF, PROD_AABB_HALF, PROD_AABB_HALF);
        }
        forge_physics_sap_update(&w, aabbs, LIFECYCLE_BODIES);
        forge_physics_sap_destroy(&w);
    }
    ASSERT_TRUE(true);  /* no crash = pass */
    #undef LIFECYCLE_BODIES
    #undef LIFECYCLE_CYCLES
    END_TEST();
}

/* ── Per-frame reuse tests ────────────────────────────────────────────── */

static void test_prod_contacts_capacity_stabilizes(void)
{
    TEST("PROD: contact array capacity stabilizes after warmup");
    #define REUSE_PARTICLES 10
    #define REUSE_FRAMES    100
    ForgePhysicsParticle particles[REUSE_PARTICLES];
    for (int i = 0; i < REUSE_PARTICLES; i++) {
        particles[i] = forge_physics_particle_create(
            vec3_create((float)i * PROD_PARTICLE_SPACING, 0.0f, 0.0f),
            PROD_MASS, PROD_DAMPING, PROD_RESTIT, PROD_RADIUS);
    }

    ForgePhysicsContact *contacts = NULL;
    ptrdiff_t prev_cap = 0;
    int stable_count = 0;

    for (int frame = 0; frame < REUSE_FRAMES; frame++) {
        forge_arr_set_length(contacts, 0);
        int n = forge_physics_collide_particles_all(
            particles, REUSE_PARTICLES, &contacts);
        ASSERT_TRUE(n >= 0);

        ptrdiff_t cap = forge_arr_capacity(contacts);
        if (cap == prev_cap && frame > PROD_WARMUP_FRAMES) {
            stable_count++;
        }
        prev_cap = cap;
    }

    /* After warmup, capacity should stop growing */
    ASSERT_TRUE(stable_count > REUSE_FRAMES / 2);
    forge_arr_free(contacts);
    #undef REUSE_FRAMES
    #undef REUSE_PARTICLES
    END_TEST();
}

static void test_prod_sap_pairs_capacity_stabilizes(void)
{
    TEST("PROD: SAP pair capacity stabilizes across frames");
    #define SAP_REUSE_BODIES  8
    #define SAP_REUSE_FRAMES  100
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    ForgePhysicsAABB aabbs[SAP_REUSE_BODIES];
    for (int i = 0; i < SAP_REUSE_BODIES; i++) {
        aabbs[i].min = vec3_create(-PROD_AABB_HALF, -PROD_AABB_HALF, -PROD_AABB_HALF);
        aabbs[i].max = vec3_create(PROD_AABB_HALF, PROD_AABB_HALF, PROD_AABB_HALF);
    }

    ptrdiff_t prev_cap = 0;
    int stable_count = 0;
    for (int frame = 0; frame < SAP_REUSE_FRAMES; frame++) {
        forge_physics_sap_update(&w, aabbs, SAP_REUSE_BODIES);
        ptrdiff_t cap = forge_arr_capacity(w.pairs);
        if (cap == prev_cap && frame > PROD_SAP_WARMUP_FRAMES) stable_count++;
        prev_cap = cap;
    }

    ASSERT_TRUE(stable_count > SAP_REUSE_FRAMES / 2);
    forge_physics_sap_destroy(&w);
    #undef SAP_REUSE_FRAMES
    #undef SAP_REUSE_BODIES
    END_TEST();
}

/* ── Edge case tests ──────────────────────────────────────────────────── */

static void test_prod_single_particle_no_contacts(void)
{
    TEST("PROD: single particle — no contacts possible");
    ForgePhysicsParticle p = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        PROD_MASS, PROD_DAMPING, PROD_RESTIT, PROD_RADIUS);

    ForgePhysicsContact *contacts = NULL;
    int n = forge_physics_collide_particles_all(&p, 1, &contacts);
    ASSERT_TRUE(n == 0);
    forge_arr_free(contacts);
    END_TEST();
}

static void test_prod_two_coincident_particles(void)
{
    TEST("PROD: two particles at exact same position");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        PROD_MASS, PROD_DAMPING, PROD_RESTIT, PROD_RADIUS);
    particles[1] = particles[0];

    ForgePhysicsContact *contacts = NULL;
    int n = forge_physics_collide_particles_all(particles, 2, &contacts);
    ASSERT_TRUE(n == 1);
    /* Should have a valid normal (arbitrary up for coincident) */
    ASSERT_TRUE(forge_isfinite(contacts[0].normal.x));
    ASSERT_TRUE(forge_isfinite(contacts[0].normal.y));
    ASSERT_TRUE(forge_isfinite(contacts[0].normal.z));
    forge_arr_free(contacts);
    END_TEST();
}

static void test_prod_null_contacts_to_resolve(void)
{
    TEST("PROD: NULL contact array to resolve_contacts is safe");
    ForgePhysicsParticle p = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        PROD_MASS, PROD_DAMPING, PROD_RESTIT, PROD_RADIUS);
    /* resolve_contacts with NULL/0 should be a no-op */
    forge_physics_resolve_contacts(NULL, 0, &p, 1);
    ASSERT_TRUE(true);
    END_TEST();
}

/* ── Stress tests ─────────────────────────────────────────────────────── */

static void test_prod_contacts_detect_resolve_loop(void)
{
    TEST("PROD: 100 frames of detect + resolve — no crash or NaN");
    #define STRESS_NUM     15
    #define STRESS_FRAMES  100
    ForgePhysicsParticle particles[STRESS_NUM];
    for (int i = 0; i < STRESS_NUM; i++) {
        particles[i] = forge_physics_particle_create(
            vec3_create((float)i * PROD_STRESS_SPACING, PROD_STRESS_START_Y, 0.0f),
            PROD_MASS, PROD_STRESS_DAMPING, PROD_RESTIT, PROD_RADIUS);
    }

    ForgePhysicsContact *contacts = NULL;
    for (int frame = 0; frame < STRESS_FRAMES; frame++) {
        for (int i = 0; i < STRESS_NUM; i++) {
            forge_physics_apply_gravity(&particles[i],
                vec3_create(0.0f, -PROD_GRAVITY, 0.0f));
            forge_physics_integrate(&particles[i], PROD_DT);
        }
        int n = forge_physics_collide_particles_step(
            particles, STRESS_NUM, &contacts);
        (void)n;

        for (int i = 0; i < STRESS_NUM; i++) {
            (void)forge_physics_collide_plane(&particles[i],
                vec3_create(0.0f, 1.0f, 0.0f), 0.0f);
        }
    }

    for (int i = 0; i < STRESS_NUM; i++) {
        ASSERT_TRUE(forge_isfinite(particles[i].position.x));
        ASSERT_TRUE(forge_isfinite(particles[i].position.y));
        ASSERT_TRUE(forge_isfinite(particles[i].position.z));
    }
    forge_arr_free(contacts);
    #undef STRESS_FRAMES
    #undef STRESS_NUM
    END_TEST();
}

static void test_prod_sap_varying_body_count(void)
{
    TEST("PROD: SAP with body count varying each frame");
    #define VAR_MAX_BODIES 50
    #define VAR_FRAMES     200
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    ForgePhysicsAABB aabbs[VAR_MAX_BODIES];
    for (int i = 0; i < VAR_MAX_BODIES; i++) {
        float x = (float)i * PROD_CHAIN_SPACING;
        aabbs[i].min = vec3_create(x - PROD_AABB_HALF, -PROD_AABB_HALF, -PROD_AABB_HALF);
        aabbs[i].max = vec3_create(x + PROD_AABB_HALF, PROD_AABB_HALF, PROD_AABB_HALF);
    }

    for (int frame = 0; frame < VAR_FRAMES; frame++) {
        /* Vary body count: 1 to VAR_MAX_BODIES and back */
        int count = (frame % VAR_MAX_BODIES) + 1;
        forge_physics_sap_update(&w, aabbs, count);
        /* Chain spacing ensures adjacent bodies overlap: expect count-1 pairs
         * for count >= 2, and 0 pairs for count == 1. */
        int expected_pairs = (count > 1) ? count - 1 : 0;
        ASSERT_TRUE(forge_physics_sap_pair_count(&w) == expected_pairs);
    }

    forge_physics_sap_destroy(&w);
    #undef VAR_FRAMES
    #undef VAR_MAX_BODIES
    END_TEST();
}

/* ── Numerical safety tests ───────────────────────────────────────────── */

static void test_prod_large_positions(void)
{
    TEST("PROD: particles at large positions (1e6) — no AABB overflow");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(PROD_LARGE_POS, PROD_LARGE_POS, PROD_LARGE_POS),
        PROD_MASS, PROD_DAMPING, PROD_RESTIT, PROD_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(PROD_LARGE_POS + PROD_RADIUS, PROD_LARGE_POS, PROD_LARGE_POS),
        PROD_MASS, PROD_DAMPING, PROD_RESTIT, PROD_RADIUS);

    ForgePhysicsContact *contacts = NULL;
    int n = forge_physics_collide_particles_all(particles, 2, &contacts);
    ASSERT_TRUE(n == 1);
    ASSERT_TRUE(forge_isfinite(contacts[0].normal.x));
    ASSERT_TRUE(forge_isfinite(contacts[0].penetration));
    forge_arr_free(contacts);
    END_TEST();
}

static void test_prod_tiny_separation(void)
{
    TEST("PROD: particles with tiny separation (1e-7) — stable resolution");
    ForgePhysicsParticle particles[2];
    particles[0] = forge_physics_particle_create(
        vec3_create(0.0f, 0.0f, 0.0f),
        PROD_MASS, PROD_DAMPING, PROD_RESTIT, PROD_RADIUS);
    particles[1] = forge_physics_particle_create(
        vec3_create(PROD_TINY_SEPARATION, 0.0f, 0.0f),
        PROD_MASS, PROD_DAMPING, PROD_RESTIT, PROD_RADIUS);

    ForgePhysicsContact *contacts = NULL;
    int n = forge_physics_collide_particles_step(particles, 2, &contacts);
    ASSERT_TRUE(n == 1);
    /* Velocities should be finite after resolution */
    ASSERT_TRUE(forge_isfinite(particles[0].velocity.x));
    ASSERT_TRUE(forge_isfinite(particles[1].velocity.x));
    forge_arr_free(contacts);
    END_TEST();
}

/* ── Runner ────────────────────────────────────────────────────────────── */

void run_production_tests(void)
{
    SDL_Log("--- Production hardening ---");

    /* Dynamic growth */
    test_prod_particles_all_contacts();
    test_prod_sap_all_overlapping();
    test_prod_500_sap_bodies_separated();

    /* Memory lifecycle */
    test_prod_sap_init_destroy_empty();
    test_prod_sap_init_populate_destroy_reinit();
    test_prod_sap_double_destroy();
    test_prod_sap_lifecycle_loop();

    /* Per-frame reuse */
    test_prod_contacts_capacity_stabilizes();
    test_prod_sap_pairs_capacity_stabilizes();

    /* Edge cases */
    test_prod_single_particle_no_contacts();
    test_prod_two_coincident_particles();
    test_prod_null_contacts_to_resolve();

    /* Stress */
    test_prod_contacts_detect_resolve_loop();
    test_prod_sap_varying_body_count();

    /* Numerical safety */
    test_prod_large_positions();
    test_prod_tiny_separation();
}
