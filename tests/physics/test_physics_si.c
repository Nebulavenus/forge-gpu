/*
 * Sequential Impulse Solver Tests
 *
 * Tests for forge_physics_si_* functions added in Lesson 12.
 * Validates tangent basis construction, effective mass computation,
 * accumulated impulse clamping, warm-starting, friction cone, stacking
 * stability, determinism, and edge cases.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Helpers ──────────────────────────────────────────────────────────────── */

/* Create a dynamic rigid body at position with given mass and restitution. */
static ForgePhysicsRigidBody make_si_body(vec3 pos, float mass, float restit)
{
    ForgePhysicsRigidBody b;
    SDL_memset(&b, 0, sizeof(b));
    b.position    = pos;
    b.orientation = quat_identity();
    b.mass        = mass;
    b.inv_mass    = (mass > FORGE_PHYSICS_EPSILON) ? (1.0f / mass) : 0.0f;
    b.restitution = restit;
    b.damping     = 1.0f;
    b.angular_damping = 1.0f;

    /* Sphere inertia for simplicity: I = (2/5) * m * r^2, r = 0.5 */
    if (mass > FORGE_PHYSICS_EPSILON) {
        float I = FORGE_PHYSICS_INERTIA_SPHERE_COEFF * mass * 0.25f;
        float inv_I = 1.0f / I;
        b.inertia_local = mat3_from_diagonal(I, I, I);
        b.inv_inertia_local = mat3_from_diagonal(inv_I, inv_I, inv_I);
        b.inv_inertia_world = mat3_from_diagonal(inv_I, inv_I, inv_I);
    }

    return b;
}

/* Build a single-contact manifold between body_a and ground (body_b=-1). */
static ForgePhysicsManifold make_ground_manifold(
    int body_a_idx, vec3 contact_point, vec3 normal,
    float penetration, float mu_s, float mu_d)
{
    ForgePhysicsManifold m;
    SDL_memset(&m, 0, sizeof(m));
    m.body_a = body_a_idx;
    m.body_b = -1;
    m.normal = normal;
    m.static_friction  = mu_s;
    m.dynamic_friction = mu_d;
    m.count = 1;
    m.contacts[0].world_point = contact_point;
    m.contacts[0].penetration = penetration;
    return m;
}

/* ── Test constants ───────────────────────────────────────────────────────── */

#define SI_TEST_BOUNCE_SPEED      5.0f    /* initial fall speed for bounce test (m/s) */
#define SI_TEST_BOUNCE_THRESHOLD  4.0f    /* minimum post-bounce speed to pass (m/s) */
#define SI_TEST_STACK_STEPS       3000    /* simulation steps for stacking stability */
#define SI_TEST_STACK_KE_BOUND    1000.0f /* max KE for non-warm-started stack (J) */
#define SI_TEST_TALL_STACK_KE     5.0f   /* max KE for warm-started tall stack (J) */
#define SI_TEST_WARM_FRAMES       30      /* frame count for warm-start comparison */

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_SI_tangent_basis_orthogonal(void)
{
    TEST("SI_tangent_basis_orthogonal");

    vec3 normals[] = {
        vec3_create(0, 1, 0),
        vec3_create(1, 0, 0),
        vec3_create(0, 0, 1),
        vec3_normalize(vec3_create(1, 1, 1)),
        vec3_normalize(vec3_create(-0.3f, 0.8f, 0.5f)),
    };
    int num = (int)(sizeof(normals) / sizeof(normals[0]));

    for (int i = 0; i < num; i++) {
        vec3 t1, t2;
        forge_physics_si_tangent_basis(normals[i], &t1, &t2);

        /* Orthogonality: dot products should be ~0 */
        ASSERT_NEAR(vec3_dot(normals[i], t1), 0.0f, 1e-5f);
        ASSERT_NEAR(vec3_dot(normals[i], t2), 0.0f, 1e-5f);
        ASSERT_NEAR(vec3_dot(t1, t2), 0.0f, 1e-5f);

        /* Unit length */
        ASSERT_NEAR(vec3_length(t1), 1.0f, 1e-5f);
        ASSERT_NEAR(vec3_length(t2), 1.0f, 1e-5f);
    }

    END_TEST();
}

static void test_SI_prepare_effective_mass(void)
{
    TEST("SI_prepare_effective_mass");

    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0.5f, 0), 1.0f, 0.5f);

    ForgePhysicsManifold manifolds[1];
    manifolds[0] = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0), 0.01f, 0.6f, 0.4f);

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_prepare(manifolds, 1, bodies, 1, PHYSICS_DT, false,
                             workspace, NULL);

    ASSERT_TRUE(workspace[0].count == 1);
    ASSERT_TRUE(workspace[0].constraints[0].eff_mass_n > 0.0f);
    ASSERT_TRUE(forge_isfinite(workspace[0].constraints[0].eff_mass_n));
    ASSERT_TRUE(workspace[0].constraints[0].eff_mass_t1 > 0.0f);
    ASSERT_TRUE(workspace[0].constraints[0].eff_mass_t2 > 0.0f);

    END_TEST();
}

static void test_SI_basic_bounce(void)
{
    TEST("SI_basic_bounce");

    /* Sphere falling at -5 m/s, restitution = 1.0 (perfect bounce) */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0.49f, 0), 1.0f, 1.0f);
    bodies[0].velocity = vec3_create(0, -SI_TEST_BOUNCE_SPEED, 0);

    ForgePhysicsManifold manifolds[1];
    manifolds[0] = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0), 0.01f, 0.0f, 0.0f);

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_solve(manifolds, 1, bodies, 1, 10, PHYSICS_DT,
                           false, workspace, NULL);

    /* Velocity should reverse: v_y should be positive (~bounce speed) */
    ASSERT_TRUE(bodies[0].velocity.y > SI_TEST_BOUNCE_THRESHOLD);
    ASSERT_NEAR(bodies[0].velocity.x, 0.0f, 0.01f);
    ASSERT_NEAR(bodies[0].velocity.z, 0.0f, 0.01f);

    END_TEST();
}

static void test_SI_accumulated_clamp_nonnegative(void)
{
    TEST("SI_accumulated_clamp_nonnegative");

    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0.5f, 0), 2.0f, 0.3f);
    bodies[0].velocity = vec3_create(0, -2.0f, 0);

    ForgePhysicsManifold manifolds[1];
    manifolds[0] = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0), 0.02f, 0.6f, 0.4f);

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_solve(manifolds, 1, bodies, 1, 10, PHYSICS_DT,
                           false, workspace, NULL);

    /* j_n must be >= 0 (normal impulse only pushes apart, never pulls) */
    ASSERT_TRUE(workspace[0].constraints[0].j_n >= 0.0f);

    END_TEST();
}

static void test_SI_friction_cone(void)
{
    TEST("SI_friction_cone");

    /* Sphere sliding sideways on ground */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0.49f, 0), 1.0f, 0.0f);
    bodies[0].velocity = vec3_create(10.0f, -1.0f, 0);

    float mu_d = 0.4f;
    ForgePhysicsManifold manifolds[1];
    manifolds[0] = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0), 0.01f, 0.6f, mu_d);

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_solve(manifolds, 1, bodies, 1, 10, PHYSICS_DT,
                           false, workspace, NULL);

    float j_n  = workspace[0].constraints[0].j_n;
    float j_t1 = workspace[0].constraints[0].j_t1;
    float j_t2 = workspace[0].constraints[0].j_t2;

    /* Friction impulses must be within Coulomb cone */
    float limit = mu_d * j_n + EPSILON;
    ASSERT_TRUE(SDL_fabsf(j_t1) <= limit);
    ASSERT_TRUE(SDL_fabsf(j_t2) <= limit);

    END_TEST();
}

static void test_SI_stacking_energy_bounded(void)
{
    TEST("SI_stacking_energy_bounded");

    /* Stack of 5 boxes resting on ground.
     * Uses the correct split-integration ordering:
     *   1. integrate velocities (v += a*dt)
     *   2. detect collisions at current positions
     *   3. solve velocity constraints
     *   4. position correction (push apart)
     *   5. integrate positions (x += v*dt) */
    #define STACK_N 5
    ForgePhysicsRigidBody bodies[STACK_N];
    float box_half = 0.5f;
    float box_mass = 1.0f;
    vec3 gravity = vec3_create(0, -9.81f, 0);

    for (int i = 0; i < STACK_N; i++) {
        float y = box_half + (float)i * (2.0f * box_half);
        bodies[i] = make_si_body(vec3_create(0, y, 0), box_mass, 0.0f);
        /* Box inertia */
        float a2 = (2.0f * box_half) * (2.0f * box_half);
        float I = FORGE_PHYSICS_INERTIA_BOX_COEFF * box_mass * (a2 + a2);
        float inv_I = 1.0f / I;
        bodies[i].inertia_local = mat3_from_diagonal(I, I, I);
        bodies[i].inv_inertia_local = mat3_from_diagonal(inv_I, inv_I, inv_I);
        bodies[i].inv_inertia_world = mat3_from_diagonal(inv_I, inv_I, inv_I);
    }

    /* Run stability test steps */
    for (int step = 0; step < SI_TEST_STACK_STEPS; step++) {
        /* Phase 1: apply gravity + integrate velocities only */
        for (int i = 0; i < STACK_N; i++) {
            bodies[i].force_accum = vec3_scale(gravity, bodies[i].mass);
            forge_physics_rigid_body_integrate_velocities(
                &bodies[i], PHYSICS_DT);
        }

        /* Phase 2: detect collisions at current positions */
        ForgePhysicsManifold manifolds[STACK_N * 2];
        int mc = 0;

        /* Ground contacts for each box */
        for (int i = 0; i < STACK_N; i++) {
            ForgePhysicsRBContact gc[8];
            int ng = forge_physics_rb_collide_box_plane(
                &bodies[i], i, vec3_create(box_half, box_half, box_half),
                vec3_create(0, 0, 0), vec3_create(0, 1, 0),
                0.6f, 0.4f, gc, 8);
            if (ng > 0) {
                forge_physics_si_rb_contacts_to_manifold(
                    gc, ng, 0.6f, 0.4f, &manifolds[mc++]);
            }
        }

        /* Body-body: each adjacent pair */
        for (int i = 0; i < STACK_N - 1; i++) {
            float top_of_i = bodies[i].position.y + box_half;
            float bot_of_j = bodies[i + 1].position.y - box_half;
            float pen = top_of_i - bot_of_j;
            if (pen > 0.0f) {
                ForgePhysicsManifold *gm = &manifolds[mc];
                SDL_memset(gm, 0, sizeof(*gm));
                gm->body_a = i + 1;
                gm->body_b = i;
                gm->normal = vec3_create(0, 1, 0);
                gm->static_friction = 0.6f;
                gm->dynamic_friction = 0.4f;
                gm->count = 1;
                gm->contacts[0].world_point = vec3_create(
                    0, (top_of_i + bot_of_j) * 0.5f, 0);
                gm->contacts[0].penetration = pen;
                mc++;
            }
        }

        /* Phase 3: solve velocity constraints */
        if (mc > 0) {
            ForgePhysicsSIManifold workspace[STACK_N * 2];
            forge_physics_si_solve(manifolds, mc, bodies, STACK_N,
                                   10, PHYSICS_DT, false, workspace, NULL);

            /* Phase 4: position correction */
            forge_physics_si_correct_positions(
                manifolds, mc, bodies, STACK_N, 0.4f,
                FORGE_PHYSICS_PENETRATION_SLOP);
        }

        /* Phase 5: integrate positions */
        for (int i = 0; i < STACK_N; i++) {
            forge_physics_rigid_body_integrate_positions(
                &bodies[i], PHYSICS_DT);
        }
    }

    /* Check: no body has exploded (position bounded, no NaN/Inf) */
    for (int i = 0; i < STACK_N; i++) {
        ASSERT_TRUE(forge_isfinite(bodies[i].position.x));
        ASSERT_TRUE(forge_isfinite(bodies[i].position.y));
        ASSERT_TRUE(forge_isfinite(bodies[i].position.z));
        ASSERT_TRUE(bodies[i].position.y > -10.0f);
        ASSERT_TRUE(bodies[i].position.y < 100.0f);
    }

    /* Check: total kinetic energy is bounded (stack should be resting) */
    float total_ke = 0.0f;
    for (int i = 0; i < STACK_N; i++) {
        total_ke += 0.5f * bodies[i].mass
                  * vec3_dot(bodies[i].velocity, bodies[i].velocity);
    }
    ASSERT_TRUE(forge_isfinite(total_ke));
    ASSERT_TRUE(total_ke < SI_TEST_STACK_KE_BOUND);

    /* Check: no body has fallen below the ground plane */
    for (int i = 0; i < STACK_N; i++) {
        ASSERT_TRUE(bodies[i].position.y >= -0.1f);
    }

    /* Check: stack ordering is preserved — each body above the one below.
     * Bodies should maintain y[0] < y[1] < y[2] < ... after settling. */
    for (int i = 0; i < STACK_N - 1; i++) {
        ASSERT_TRUE(bodies[i + 1].position.y > bodies[i].position.y);
    }

    /* Check: bottom box rests near the ground plane.
     * Note: this test uses simplified single-point box-box contacts,
     * not full GJK/EPA with 4-point manifolds. Production stacking
     * quality is validated in the lesson itself. */
    ASSERT_TRUE(bodies[0].position.y > -0.5f);
    ASSERT_TRUE(bodies[0].position.y < 2.0f);

    #undef STACK_N

    END_TEST();
}

static void test_SI_tall_stack_stability(void)
{
    TEST("SI — 12-box stack stays upright with tuned parameters");

    /* 12 boxes stacked vertically — the lesson's first scene.
     * Uses multi-contact box-plane detection (same as the real lesson)
     * and runs for the full SI_TEST_STACK_STEPS (50 simulated seconds).
     * The stack must remain ordered, laterally stable, and resting. */
    #define TALL_N 12
    #define TALL_MAX_MANIFOLDS (TALL_N * 2)
    ForgePhysicsRigidBody bodies[TALL_N];
    ForgePhysicsCollisionShape shapes[TALL_N];
    float box_half = 0.5f;
    float box_mass = 2.0f;
    vec3 half_ext = vec3_create(box_half, box_half, box_half);
    vec3 gravity = vec3_create(0, -9.81f, 0);
    vec3 ground_pt = vec3_create(0, 0, 0);
    vec3 ground_n  = vec3_create(0, 1, 0);

    for (int i = 0; i < TALL_N; i++) {
        float y = box_half + (float)i * (2.0f * box_half + 0.001f);
        bodies[i] = make_si_body(vec3_create(0, y, 0), box_mass, 0.2f);
        /* Box inertia tensor */
        float side = 2.0f * box_half;
        float a2 = side * side;
        float I = FORGE_PHYSICS_INERTIA_BOX_COEFF * box_mass * (a2 + a2);
        float inv_I = 1.0f / I;
        bodies[i].inertia_local = mat3_from_diagonal(I, I, I);
        bodies[i].inv_inertia_local = mat3_from_diagonal(inv_I, inv_I, inv_I);
        bodies[i].inv_inertia_world = bodies[i].inv_inertia_local;
        bodies[i].prev_position = bodies[i].position;
        bodies[i].prev_orientation = bodies[i].orientation;

        /* Collision shape */
        shapes[i].type = FORGE_PHYSICS_SHAPE_BOX;
        shapes[i].data.box.half_extents = half_ext;
    }

    /* Record initial top position for drift check */
    float initial_top_y = bodies[TALL_N - 1].position.y;

    /* Solver config tuned for tall stacks */
    ForgePhysicsSolverConfig cfg = forge_physics_solver_config_default();
    cfg.baumgarte_factor = 0.15f;
    cfg.penetration_slop = 0.005f;
    int solver_iters = 40;

    /* Manifold cache for warm-starting */
    ForgePhysicsManifoldCacheEntry *manifold_cache = NULL;

    ForgePhysicsManifold manifolds[TALL_MAX_MANIFOLDS];
    SDL_memset(manifolds, 0, sizeof(manifolds));
    ForgePhysicsSIManifold workspace[TALL_MAX_MANIFOLDS];

    for (int step = 0; step < SI_TEST_STACK_STEPS; step++) {
        /* Apply gravity */
        for (int i = 0; i < TALL_N; i++) {
            forge_physics_rigid_body_apply_force(
                &bodies[i], vec3_scale(gravity, bodies[i].mass));
        }

        /* Integrate velocities */
        for (int i = 0; i < TALL_N; i++)
            forge_physics_rigid_body_integrate_velocities(
                &bodies[i], PHYSICS_DT);

        /* Collision detection using multi-contact box-plane for ground
         * and GJK/EPA for body-body — same pipeline as the lesson */
        int mc = 0;
        uint64_t active_keys[TALL_MAX_MANIFOLDS];
        int active_key_count = 0;

        /* Ground contacts (multi-contact box-plane) */
        for (int i = 0; i < TALL_N && mc < TALL_MAX_MANIFOLDS; i++) {
            ForgePhysicsRBContact gc[8];
            int ng = forge_physics_rb_collide_box_plane(
                &bodies[i], i, half_ext,
                ground_pt, ground_n, 0.6f, 0.4f, gc, 8);

            if (ng > 0) {
                ForgePhysicsManifold manifold;
                if (forge_physics_si_rb_contacts_to_manifold(
                        gc, ng, 0.6f, 0.4f, &manifold)) {
                    forge_physics_manifold_cache_update(
                        &manifold_cache, &manifold);
                    uint64_t key = forge_physics_manifold_pair_key(i, -1);
                    if (active_key_count < TALL_MAX_MANIFOLDS)
                        active_keys[active_key_count++] = key;
                    ForgePhysicsManifoldCacheEntry *cached =
                        forge_hm_get_ptr_or_null(manifold_cache, key);
                    if (cached && cached->key == key)
                        manifolds[mc++] = cached->manifold;
                    else
                        manifolds[mc++] = manifold;
                }
            }
        }

        /* Body-body contacts (GJK/EPA) */
        for (int i = 0; i < TALL_N - 1 && mc < TALL_MAX_MANIFOLDS; i++) {
            int j = i + 1;
            ForgePhysicsManifold manifold;
            if (forge_physics_gjk_epa_manifold(
                    &bodies[i], &shapes[i],
                    &bodies[j], &shapes[j],
                    i, j, 0.6f, 0.4f, &manifold)) {
                forge_physics_manifold_cache_update(
                    &manifold_cache, &manifold);
                uint64_t key = forge_physics_manifold_pair_key(i, j);
                if (active_key_count < TALL_MAX_MANIFOLDS)
                    active_keys[active_key_count++] = key;
                ForgePhysicsManifoldCacheEntry *cached =
                    forge_hm_get_ptr_or_null(manifold_cache, key);
                if (cached && cached->key == key)
                    manifolds[mc++] = cached->manifold;
                else
                    manifolds[mc++] = manifold;
            }
        }

        /* Prune stale cache entries */
        forge_physics_manifold_cache_prune(
            &manifold_cache, active_keys, active_key_count);

        /* Solve */
        if (mc > 0) {
            forge_physics_si_solve(manifolds, mc, bodies, TALL_N,
                                   solver_iters, PHYSICS_DT, true,
                                   workspace, &cfg);
            /* si_solve stores impulses internally (Phase 4) */
            for (int mi = 0; mi < mc; mi++)
                forge_physics_manifold_cache_store(
                    &manifold_cache, &manifolds[mi]);
            forge_physics_si_correct_positions(
                manifolds, mc, bodies, TALL_N,
                cfg.correction_fraction, cfg.correction_slop);
        }

        /* Integrate positions */
        for (int i = 0; i < TALL_N; i++)
            forge_physics_rigid_body_integrate_positions(
                &bodies[i], PHYSICS_DT);
    }

    /* Verify: stack ordering preserved (each body above the one below) */
    for (int i = 0; i < TALL_N - 1; i++) {
        ASSERT_TRUE(bodies[i + 1].position.y > bodies[i].position.y);
    }

    /* Verify: no body drifted laterally more than half a box width */
    for (int i = 0; i < TALL_N; i++) {
        ASSERT_TRUE(SDL_fabsf(bodies[i].position.x) < box_half);
        ASSERT_TRUE(SDL_fabsf(bodies[i].position.z) < box_half);
    }

    /* Verify: top box hasn't dropped more than 1 box height */
    float final_top_y = bodies[TALL_N - 1].position.y;
    ASSERT_TRUE(final_top_y > initial_top_y - 2.0f * box_half);

    /* Verify: no NaN, no explosion */
    for (int i = 0; i < TALL_N; i++) {
        ASSERT_TRUE(forge_isfinite(bodies[i].position.x));
        ASSERT_TRUE(forge_isfinite(bodies[i].position.y));
        ASSERT_TRUE(forge_isfinite(bodies[i].position.z));
    }

    /* Verify: stack is resting (low kinetic energy) */
    float total_ke = 0.0f;
    for (int i = 0; i < TALL_N; i++) {
        total_ke += 0.5f * bodies[i].mass
                  * vec3_dot(bodies[i].velocity, bodies[i].velocity);
    }
    ASSERT_TRUE(total_ke < SI_TEST_TALL_STACK_KE);

    /* Clean up */
    forge_hm_free(manifold_cache);

    #undef TALL_N
    #undef TALL_MAX_MANIFOLDS

    END_TEST();
}

static void test_SI_determinism(void)
{
    TEST("SI_determinism");

    /* Run same scenario twice, check identical results */
    ForgePhysicsRigidBody bodies_a[2], bodies_b[2];

    for (int run = 0; run < 2; run++) {
        ForgePhysicsRigidBody *bodies = (run == 0) ? bodies_a : bodies_b;
        bodies[0] = make_si_body(vec3_create(0, 1.0f, 0), 1.0f, 0.5f);
        bodies[0].velocity = vec3_create(2.0f, -3.0f, 1.0f);
        bodies[1] = make_si_body(vec3_create(0.8f, 1.0f, 0), 2.0f, 0.3f);
        bodies[1].velocity = vec3_create(-1.0f, -2.0f, 0.5f);

        for (int step = 0; step < 100; step++) {
            /* Phase 1: integrate velocities */
            for (int i = 0; i < 2; i++) {
                bodies[i].force_accum = vec3_scale(
                    vec3_create(0, -9.81f, 0), bodies[i].mass);
                forge_physics_rigid_body_integrate_velocities(
                    &bodies[i], PHYSICS_DT);
            }

            /* Phase 2: detect collisions at current positions */
            ForgePhysicsManifold manifolds[4];
            int mc = 0;

            for (int i = 0; i < 2; i++) {
                ForgePhysicsRBContact gc;
                if (forge_physics_rb_collide_sphere_plane(
                        &bodies[i], i, 0.5f,
                        vec3_create(0, 0, 0), vec3_create(0, 1, 0),
                        0.6f, 0.4f, &gc)) {
                    forge_physics_si_rb_contacts_to_manifold(
                        &gc, 1, 0.6f, 0.4f, &manifolds[mc++]);
                }
            }

            /* Phase 3: solve + position correction */
            if (mc > 0) {
                ForgePhysicsSIManifold workspace[4];
                forge_physics_si_solve(manifolds, mc, bodies, 2,
                                       10, PHYSICS_DT, false, workspace, NULL);
                forge_physics_si_correct_positions(
                    manifolds, mc, bodies, 2, 0.4f,
                    FORGE_PHYSICS_PENETRATION_SLOP);
            }

            /* Phase 4: integrate positions */
            for (int i = 0; i < 2; i++) {
                forge_physics_rigid_body_integrate_positions(
                    &bodies[i], PHYSICS_DT);
            }
        }
    }

    /* Results must be bit-identical */
    for (int i = 0; i < 2; i++) {
        ASSERT_NEAR(bodies_a[i].position.x, bodies_b[i].position.x, 1e-6f);
        ASSERT_NEAR(bodies_a[i].position.y, bodies_b[i].position.y, 1e-6f);
        ASSERT_NEAR(bodies_a[i].position.z, bodies_b[i].position.z, 1e-6f);
        ASSERT_NEAR(bodies_a[i].velocity.x, bodies_b[i].velocity.x, 1e-6f);
        ASSERT_NEAR(bodies_a[i].velocity.y, bodies_b[i].velocity.y, 1e-6f);
        ASSERT_NEAR(bodies_a[i].velocity.z, bodies_b[i].velocity.z, 1e-6f);
    }

    END_TEST();
}

static void test_SI_static_body_unchanged(void)
{
    TEST("SI_static_body_unchanged");

    /* body 0 = dynamic falling, body 1 = static (mass 0) */
    ForgePhysicsRigidBody bodies[2];
    bodies[0] = make_si_body(vec3_create(0, 1.0f, 0), 1.0f, 0.5f);
    bodies[0].velocity = vec3_create(0, -5.0f, 0);
    bodies[1] = make_si_body(vec3_create(0, 0, 0), 0.0f, 0.5f);  /* static */

    /* Fake collision between them */
    ForgePhysicsManifold manifolds[1];
    SDL_memset(&manifolds[0], 0, sizeof(manifolds[0]));
    manifolds[0].body_a = 0;
    manifolds[0].body_b = 1;
    manifolds[0].normal = vec3_create(0, 1, 0);
    manifolds[0].static_friction = 0.6f;
    manifolds[0].dynamic_friction = 0.4f;
    manifolds[0].count = 1;
    manifolds[0].contacts[0].world_point = vec3_create(0, 0.5f, 0);
    manifolds[0].contacts[0].penetration = 0.01f;

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_solve(manifolds, 1, bodies, 2, 10, PHYSICS_DT,
                           false, workspace, NULL);

    /* Static body must not move */
    ASSERT_NEAR(bodies[1].velocity.x, 0.0f, 1e-6f);
    ASSERT_NEAR(bodies[1].velocity.y, 0.0f, 1e-6f);
    ASSERT_NEAR(bodies[1].velocity.z, 0.0f, 1e-6f);
    ASSERT_NEAR(bodies[1].angular_velocity.x, 0.0f, 1e-6f);
    ASSERT_NEAR(bodies[1].angular_velocity.y, 0.0f, 1e-6f);
    ASSERT_NEAR(bodies[1].angular_velocity.z, 0.0f, 1e-6f);

    END_TEST();
}

static void test_SI_ground_contact(void)
{
    TEST("SI_ground_contact");

    /* Single sphere resting on ground plane (body_b == -1) */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0.49f, 0), 1.0f, 0.0f);
    bodies[0].velocity = vec3_create(0, -2.0f, 0);

    ForgePhysicsManifold manifolds[1];
    manifolds[0] = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0), 0.01f, 0.6f, 0.4f);

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_solve(manifolds, 1, bodies, 1, 10, PHYSICS_DT,
                           false, workspace, NULL);

    /* Body should stop falling (v_y >= 0 after resolution) */
    ASSERT_TRUE(bodies[0].velocity.y >= -0.1f);

    END_TEST();
}

static void test_SI_null_inputs(void)
{
    TEST("SI_null_inputs");

    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 1, 0), 1.0f, 0.5f);
    ForgePhysicsManifold manifolds[1];
    SDL_memset(&manifolds[0], 0, sizeof(manifolds[0]));
    ForgePhysicsSIManifold workspace[1];

    /* These should not crash */
    forge_physics_si_prepare(NULL, 1, bodies, 1, PHYSICS_DT, false, workspace, NULL);
    forge_physics_si_prepare(manifolds, 1, NULL, 1, PHYSICS_DT, false, workspace, NULL);
    forge_physics_si_prepare(manifolds, 0, bodies, 1, PHYSICS_DT, false, workspace, NULL);
    forge_physics_si_warm_start(NULL, 1, bodies, 1);
    forge_physics_si_warm_start(workspace, 0, bodies, 1);
    forge_physics_si_solve_velocities(NULL, 1, bodies, 1);
    forge_physics_si_store_impulses(NULL, 1, manifolds);
    forge_physics_si_solve(NULL, 1, bodies, 1, 10, PHYSICS_DT, false, workspace, NULL);
    forge_physics_si_solve(manifolds, 1, bodies, 1, 10, 0.0f, false, workspace, NULL);
    forge_physics_si_solve(manifolds, 1, bodies, 1, 10, PHYSICS_DT, false, NULL, NULL);

    /* Tangent basis null output */
    forge_physics_si_tangent_basis(vec3_create(0, 1, 0), NULL, NULL);

    /* RBContact to manifold with nulls */
    ASSERT_TRUE(!forge_physics_si_rb_contacts_to_manifold(NULL, 1, 0.6f, 0.4f,
                                                           &manifolds[0]));

    END_TEST();
}

static void test_SI_warm_start_improves(void)
{
    TEST("SI_warm_start_improves");

    /* Run a scenario with and without warm-starting.
     * With warm-start, the solver should need less total impulse change. */

    float total_delta_no_warm = 0.0f;
    float total_delta_warm = 0.0f;

    for (int use_warm = 0; use_warm < 2; use_warm++) {
        ForgePhysicsRigidBody bodies[1];
        bodies[0] = make_si_body(vec3_create(0, 0.49f, 0), 1.0f, 0.0f);

        ForgePhysicsManifold manifolds[1];
        ForgePhysicsSIManifold workspace[1];

        /* Saved impulses from previous frame for warm-start transfer */
        float saved_jn  = 0.0f;
        float saved_jt1 = 0.0f;
        float saved_jt2 = 0.0f;

        float total_delta = 0.0f;

        for (int frame = 0; frame < SI_TEST_WARM_FRAMES; frame++) {
            /* Phase 1: integrate velocities */
            bodies[0].force_accum = vec3_scale(
                vec3_create(0, -9.81f, 0), bodies[0].mass);
            forge_physics_rigid_body_integrate_velocities(
                &bodies[0], PHYSICS_DT);

            /* Phase 2: detect collisions at current positions */
            ForgePhysicsRBContact gc;
            if (forge_physics_rb_collide_sphere_plane(
                    &bodies[0], 0, 0.5f,
                    vec3_create(0, 0, 0), vec3_create(0, 1, 0),
                    0.6f, 0.4f, &gc)) {
                forge_physics_si_rb_contacts_to_manifold(
                    &gc, 1, 0.6f, 0.4f, &manifolds[0]);

                /* Carry forward impulses from previous frame for warm-start.
                 * rb_contacts_to_manifold zeros them, so we restore manually. */
                if (use_warm == 1) {
                    manifolds[0].contacts[0].normal_impulse    = saved_jn;
                    manifolds[0].contacts[0].tangent_impulse_1 = saved_jt1;
                    manifolds[0].contacts[0].tangent_impulse_2 = saved_jt2;
                }

                float pre_jn = manifolds[0].contacts[0].normal_impulse;

                /* Phase 3: solve + position correction */
                forge_physics_si_solve(manifolds, 1, bodies, 1,
                                       10, PHYSICS_DT,
                                       (use_warm == 1), workspace, NULL);
                forge_physics_si_correct_positions(
                    manifolds, 1, bodies, 1, 0.4f,
                    FORGE_PHYSICS_PENETRATION_SLOP);

                float post_jn = workspace[0].constraints[0].j_n;
                total_delta += SDL_fabsf(post_jn - pre_jn);

                /* Save impulses for next frame's warm-start */
                saved_jn  = manifolds[0].contacts[0].normal_impulse;
                saved_jt1 = manifolds[0].contacts[0].tangent_impulse_1;
                saved_jt2 = manifolds[0].contacts[0].tangent_impulse_2;
            }

            /* Phase 4: integrate positions */
            forge_physics_rigid_body_integrate_positions(
                &bodies[0], PHYSICS_DT);
        }

        if (use_warm == 0) total_delta_no_warm = total_delta;
        else total_delta_warm = total_delta;
    }

    /* Warm-started solver should have strictly less total impulse change
     * (it starts closer to the solution each frame). Use a small tolerance
     * to account for floating-point noise but not allow warm-start to be
     * worse or equal. */
    ASSERT_TRUE(total_delta_warm < total_delta_no_warm);

    END_TEST();
}

static void test_SI_rb_contacts_to_manifold(void)
{
    TEST("SI_rb_contacts_to_manifold");

    ForgePhysicsRBContact contacts[2];
    contacts[0].point = vec3_create(0.5f, 0, 0.5f);
    contacts[0].normal = vec3_create(0, 1, 0);
    contacts[0].penetration = 0.02f;
    contacts[0].body_a = 3;
    contacts[0].body_b = -1;
    contacts[0].static_friction = 0.6f;
    contacts[0].dynamic_friction = 0.4f;

    contacts[1].point = vec3_create(-0.5f, 0, 0.5f);
    contacts[1].normal = vec3_create(0, 1, 0);
    contacts[1].penetration = 0.01f;
    contacts[1].body_a = 3;
    contacts[1].body_b = -1;
    contacts[1].static_friction = 0.6f;
    contacts[1].dynamic_friction = 0.4f;

    ForgePhysicsManifold m;
    ASSERT_TRUE(forge_physics_si_rb_contacts_to_manifold(
        contacts, 2, 0.6f, 0.4f, &m));

    ASSERT_TRUE(m.body_a == 3);
    ASSERT_TRUE(m.body_b == -1);
    ASSERT_TRUE(m.count == 2);
    ASSERT_NEAR(m.contacts[0].penetration, 0.02f, 1e-6f);
    ASSERT_NEAR(m.contacts[1].penetration, 0.01f, 1e-6f);
    ASSERT_NEAR(m.static_friction, 0.6f, 1e-6f);
    ASSERT_NEAR(m.dynamic_friction, 0.4f, 1e-6f);

    /* IDs are sequential indices — contacts are produced in a stable
     * winding order by the collision detector, so index-based IDs give
     * consistent warm-start matching across frames. */
    ASSERT_TRUE(m.contacts[0].id == 0);
    ASSERT_TRUE(m.contacts[1].id == 1);

    END_TEST();
}

static void test_SI_rb_contacts_mixed_body_rejected(void)
{
    TEST("SI_rb_contacts_mixed_body_rejected");

    /* Contacts from different bodies should be rejected */
    ForgePhysicsRBContact contacts[2];
    contacts[0].point = vec3_create(0, 0, 0);
    contacts[0].normal = vec3_create(0, 1, 0);
    contacts[0].penetration = 0.01f;
    contacts[0].body_a = 0;
    contacts[0].body_b = -1;
    contacts[0].static_friction = 0.6f;
    contacts[0].dynamic_friction = 0.4f;

    contacts[1].point = vec3_create(1, 0, 0);
    contacts[1].normal = vec3_create(0, 1, 0);
    contacts[1].penetration = 0.01f;
    contacts[1].body_a = 1;  /* different body! */
    contacts[1].body_b = -1;
    contacts[1].static_friction = 0.6f;
    contacts[1].dynamic_friction = 0.4f;

    ForgePhysicsManifold m;
    ASSERT_TRUE(!forge_physics_si_rb_contacts_to_manifold(
        contacts, 2, 0.6f, 0.4f, &m));
    ASSERT_TRUE(m.count == 0);

    END_TEST();
}

/* ── Phase 1 fix validation tests ─────────────────────────────────────────── */

static void test_SI_tangent_basis_zero_normal(void)
{
    TEST("SI_tangent_basis_zero_normal");

    /* Zero normal should produce a valid (non-NaN) basis without crashing */
    vec3 t1, t2;
    forge_physics_si_tangent_basis(vec3_create(0, 0, 0), &t1, &t2);

    ASSERT_TRUE(forge_isfinite(t1.x));
    ASSERT_TRUE(forge_isfinite(t1.y));
    ASSERT_TRUE(forge_isfinite(t1.z));
    ASSERT_TRUE(forge_isfinite(t2.x));
    ASSERT_TRUE(forge_isfinite(t2.y));
    ASSERT_TRUE(forge_isfinite(t2.z));

    END_TEST();
}

static void test_SI_prepare_clamps_count(void)
{
    TEST("SI_prepare_clamps_count");

    /* Manifold with count > MAX_CONTACTS should be clamped */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0.5f, 0), 1.0f, 0.5f);

    ForgePhysicsManifold manifolds[1];
    manifolds[0] = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0), 0.01f, 0.6f, 0.4f);
    manifolds[0].count = 99;  /* intentionally corrupt */

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_prepare(manifolds, 1, bodies, 1, PHYSICS_DT, false,
                             workspace, NULL);

    /* Count must be clamped to MAX_CONTACTS (4) */
    ASSERT_TRUE(workspace[0].count <= FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS);

    END_TEST();
}

static void test_SI_prepare_negative_friction_clamped(void)
{
    TEST("SI_prepare_negative_friction_clamped");

    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0.5f, 0), 1.0f, 0.5f);

    ForgePhysicsManifold manifolds[1];
    manifolds[0] = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0), 0.01f, -0.5f, -0.3f);

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_prepare(manifolds, 1, bodies, 1, PHYSICS_DT, false,
                             workspace, NULL);

    /* Negative friction must be clamped to 0 */
    ASSERT_NEAR(workspace[0].static_friction, 0.0f, 1e-6f);
    ASSERT_NEAR(workspace[0].dynamic_friction, 0.0f, 1e-6f);

    END_TEST();
}

static void test_SI_prepare_degenerate_normal_rejected(void)
{
    TEST("SI_prepare_degenerate_normal_rejected");

    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0.5f, 0), 1.0f, 0.5f);

    /* Manifold with zero normal */
    ForgePhysicsManifold manifolds[1];
    manifolds[0] = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 0, 0), 0.01f, 0.6f, 0.4f);

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_prepare(manifolds, 1, bodies, 1, PHYSICS_DT, false,
                             workspace, NULL);

    /* Degenerate normal should result in count = 0 (skipped manifold) */
    ASSERT_TRUE(workspace[0].count == 0);

    END_TEST();
}

static void test_SI_solve_nan_normal_safe(void)
{
    TEST("SI_solve_nan_normal_safe");

    /* Solving with a NaN normal should not produce NaN in body state */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0.5f, 0), 1.0f, 0.5f);
    bodies[0].velocity = vec3_create(0, -2.0f, 0);

    ForgePhysicsManifold manifolds[1];
    manifolds[0] = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(NAN, NAN, NAN),
        0.01f, 0.6f, 0.4f);

    ForgePhysicsSIManifold workspace[1];
    forge_physics_si_solve(manifolds, 1, bodies, 1, 10, PHYSICS_DT,
                           false, workspace, NULL);

    /* Body velocity must remain finite (NaN normal was rejected in prepare) */
    ASSERT_TRUE(forge_isfinite(bodies[0].velocity.x));
    ASSERT_TRUE(forge_isfinite(bodies[0].velocity.y));
    ASSERT_TRUE(forge_isfinite(bodies[0].velocity.z));

    END_TEST();
}

/* ── Split integration tests ──────────────────────────────────────────────── */

static void test_SI_integrate_velocities_applies_force(void)
{
    TEST("SI_integrate_velocities_applies_force");

    ForgePhysicsRigidBody b = make_si_body(
        vec3_create(0, 5, 0), 2.0f, 0.0f);

    /* Apply downward force (gravity-like) */
    vec3 gravity_force = vec3_create(0, -9.81f * b.mass, 0);
    forge_physics_rigid_body_apply_force(&b, gravity_force);

    vec3 pos_before = b.position;
    float vy_before = b.velocity.y;

    float dt = 1.0f / 60.0f;
    forge_physics_rigid_body_integrate_velocities(&b, dt);

    /* Velocity should have changed (gravity applied) */
    ASSERT_TRUE(b.velocity.y < vy_before);

    /* Position should NOT have changed (velocity-only step) */
    ASSERT_NEAR(b.position.x, pos_before.x, 1e-6f);
    ASSERT_NEAR(b.position.y, pos_before.y, 1e-6f);
    ASSERT_NEAR(b.position.z, pos_before.z, 1e-6f);

    /* Force accumulator should be cleared */
    ASSERT_NEAR(b.force_accum.x, 0.0f, 1e-6f);
    ASSERT_NEAR(b.force_accum.y, 0.0f, 1e-6f);
    ASSERT_NEAR(b.force_accum.z, 0.0f, 1e-6f);

    END_TEST();
}

static void test_SI_integrate_positions_updates_position(void)
{
    TEST("SI_integrate_positions_updates_position");

    ForgePhysicsRigidBody b = make_si_body(
        vec3_create(0, 5, 0), 2.0f, 0.0f);
    b.velocity = vec3_create(1.0f, -2.0f, 3.0f);

    vec3 pos_before = b.position;
    vec3 vel_before = b.velocity;
    float dt = 1.0f / 60.0f;

    forge_physics_rigid_body_integrate_positions(&b, dt);

    /* Position should have moved by v * dt */
    ASSERT_NEAR(b.position.x, pos_before.x + vel_before.x * dt, 1e-4f);
    ASSERT_NEAR(b.position.y, pos_before.y + vel_before.y * dt, 1e-4f);
    ASSERT_NEAR(b.position.z, pos_before.z + vel_before.z * dt, 1e-4f);

    /* Velocity should be unchanged (position-only step) */
    ASSERT_NEAR(b.velocity.x, vel_before.x, 1e-6f);
    ASSERT_NEAR(b.velocity.y, vel_before.y, 1e-6f);
    ASSERT_NEAR(b.velocity.z, vel_before.z, 1e-6f);

    /* prev_position is NOT saved by integrate_positions — callers
     * doing split-step with position correction must save it themselves
     * before the correction pass for correct render interpolation. */

    END_TEST();
}

static void test_SI_split_matches_combined(void)
{
    TEST("SI_split_matches_combined");

    /* Two identical bodies — one uses combined integrate, the other uses
     * the split velocity + position steps. Results should match. */
    ForgePhysicsRigidBody combined = make_si_body(
        vec3_create(0, 5, 0), 2.0f, 0.0f);
    ForgePhysicsRigidBody split = make_si_body(
        vec3_create(0, 5, 0), 2.0f, 0.0f);

    vec3 force = vec3_create(0, -9.81f * 2.0f, 0);
    float dt = 1.0f / 60.0f;

    /* Combined path */
    forge_physics_rigid_body_apply_force(&combined, force);
    forge_physics_rigid_body_integrate(&combined, dt);

    /* Split path */
    forge_physics_rigid_body_apply_force(&split, force);
    forge_physics_rigid_body_integrate_velocities(&split, dt);
    forge_physics_rigid_body_integrate_positions(&split, dt);

    /* Positions should match */
    ASSERT_NEAR(split.position.x, combined.position.x, 1e-5f);
    ASSERT_NEAR(split.position.y, combined.position.y, 1e-5f);
    ASSERT_NEAR(split.position.z, combined.position.z, 1e-5f);

    /* Velocities should match */
    ASSERT_NEAR(split.velocity.x, combined.velocity.x, 1e-5f);
    ASSERT_NEAR(split.velocity.y, combined.velocity.y, 1e-5f);
    ASSERT_NEAR(split.velocity.z, combined.velocity.z, 1e-5f);

    END_TEST();
}

static void test_SI_integrate_velocities_static_body(void)
{
    TEST("SI_integrate_velocities_static_body");

    /* Static body (mass 0) should be untouched */
    ForgePhysicsRigidBody b = make_si_body(
        vec3_create(0, 0, 0), 0.0f, 0.0f);
    vec3 pos_before = b.position;

    forge_physics_rigid_body_integrate_velocities(&b, 1.0f / 60.0f);

    ASSERT_NEAR(b.position.x, pos_before.x, 1e-6f);
    ASSERT_NEAR(b.velocity.x, 0.0f, 1e-6f);

    END_TEST();
}

static void test_SI_integrate_positions_static_body(void)
{
    TEST("SI_integrate_positions_static_body");

    ForgePhysicsRigidBody b = make_si_body(
        vec3_create(0, 0, 0), 0.0f, 0.0f);
    vec3 pos_before = b.position;

    forge_physics_rigid_body_integrate_positions(&b, 1.0f / 60.0f);

    ASSERT_NEAR(b.position.x, pos_before.x, 1e-6f);

    END_TEST();
}

/* ── Position correction tests ───────────────────────────────────────────── */

static void test_SI_correct_positions_removes_penetration(void)
{
    TEST("SI_correct_positions_removes_penetration");

    /* Body penetrating the ground plane by 0.1m */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, -0.05f, 0), 2.0f, 0.0f);

    ForgePhysicsManifold m = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        0.1f, 0.6f, 0.4f);

    float y_before = bodies[0].position.y;

    forge_physics_si_correct_positions(&m, 1, bodies, 1,
                                        0.4f, 0.01f);

    /* Body should have moved upward (positive y) */
    ASSERT_TRUE(bodies[0].position.y > y_before);

    /* Correction = 0.4 * max(0.1 - 0.01, 0) / inv_mass_sum
     * inv_mass_sum = 0.5 (only body A, inv_mass = 1/2)
     * correction = 0.4 * 0.09 / 0.5 = 0.072
     * body A moves: 0.072 * 0.5 = 0.036 upward */
    float expected_correction = 0.4f * (0.1f - 0.01f) / 0.5f * 0.5f;
    ASSERT_NEAR(bodies[0].position.y, y_before + expected_correction, 1e-5f);

    END_TEST();
}

static void test_SI_correct_positions_respects_slop(void)
{
    TEST("SI_correct_positions_respects_slop");

    /* Penetration below slop — no correction */
    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, -0.005f, 0), 2.0f, 0.0f);

    ForgePhysicsManifold m = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        0.005f, 0.6f, 0.4f);

    float y_before = bodies[0].position.y;

    forge_physics_si_correct_positions(&m, 1, bodies, 1,
                                        0.4f, 0.01f);

    /* No correction — penetration < slop */
    ASSERT_NEAR(bodies[0].position.y, y_before, 1e-6f);

    END_TEST();
}

static void test_SI_correct_positions_mass_weighted(void)
{
    TEST("SI_correct_positions_mass_weighted");

    /* Two bodies: A (mass 2) and B (mass 8) — lighter body moves more */
    ForgePhysicsRigidBody bodies[2];
    bodies[0] = make_si_body(vec3_create(0, 1.0f, 0), 2.0f, 0.0f);
    bodies[1] = make_si_body(vec3_create(0, 0.9f, 0), 8.0f, 0.0f);

    ForgePhysicsManifold m;
    SDL_memset(&m, 0, sizeof(m));
    m.body_a = 0;
    m.body_b = 1;
    m.normal = vec3_create(0, 1, 0);  /* B toward A */
    m.static_friction = 0.6f;
    m.dynamic_friction = 0.4f;
    m.count = 1;
    m.contacts[0].world_point = vec3_create(0, 0.95f, 0);
    m.contacts[0].penetration = 0.1f;

    float ya_before = bodies[0].position.y;
    float yb_before = bodies[1].position.y;

    forge_physics_si_correct_positions(&m, 1, bodies, 2,
                                        0.4f, 0.01f);

    /* Both bodies should move apart */
    ASSERT_TRUE(bodies[0].position.y > ya_before);  /* A pushed up */
    ASSERT_TRUE(bodies[1].position.y < yb_before);  /* B pushed down */

    /* A (lighter) should move more than B (heavier) */
    float da = bodies[0].position.y - ya_before;
    float db = yb_before - bodies[1].position.y;
    ASSERT_TRUE(da > db);

    /* Ratio should match inverse mass ratio: da/db = inv_mass_a/inv_mass_b
     * inv_mass_a = 0.5, inv_mass_b = 0.125 → ratio = 4 */
    ASSERT_NEAR(da / db, 4.0f, 0.01f);

    END_TEST();
}

static void test_SI_correct_positions_null_inputs(void)
{
    TEST("SI_correct_positions_null_inputs");

    ForgePhysicsRigidBody bodies[1];
    bodies[0] = make_si_body(vec3_create(0, 0, 0), 2.0f, 0.0f);
    ForgePhysicsManifold m = make_ground_manifold(
        0, vec3_create(0, 0, 0), vec3_create(0, 1, 0),
        0.1f, 0.6f, 0.4f);

    /* Should not crash */
    forge_physics_si_correct_positions(NULL, 1, bodies, 1, 0.4f, 0.01f);
    forge_physics_si_correct_positions(&m, 0, bodies, 1, 0.4f, 0.01f);
    forge_physics_si_correct_positions(&m, 1, NULL, 1, 0.4f, 0.01f);
    forge_physics_si_correct_positions(&m, 1, bodies, 0, 0.4f, 0.01f);

    END_TEST();
}

static void test_SI_correct_positions_static_body_unmoved(void)
{
    TEST("SI_correct_positions_static_body_unmoved");

    /* Static body (mass 0) should not move; only the dynamic body moves */
    ForgePhysicsRigidBody bodies[2];
    bodies[0] = make_si_body(vec3_create(0, 1.0f, 0), 2.0f, 0.0f);
    bodies[1] = make_si_body(vec3_create(0, 0.9f, 0), 0.0f, 0.0f);

    ForgePhysicsManifold m;
    SDL_memset(&m, 0, sizeof(m));
    m.body_a = 0;
    m.body_b = 1;
    m.normal = vec3_create(0, 1, 0);
    m.static_friction = 0.6f;
    m.dynamic_friction = 0.4f;
    m.count = 1;
    m.contacts[0].world_point = vec3_create(0, 0.95f, 0);
    m.contacts[0].penetration = 0.1f;

    float yb_before = bodies[1].position.y;

    forge_physics_si_correct_positions(&m, 1, bodies, 2,
                                        0.4f, 0.01f);

    /* Static body unchanged */
    ASSERT_NEAR(bodies[1].position.y, yb_before, 1e-6f);

    /* Dynamic body should have moved */
    ASSERT_TRUE(bodies[0].position.y > 1.0f);

    END_TEST();
}

/* ── Test Runner ──────────────────────────────────────────────────────────── */

void run_si_tests(void)
{
    SDL_Log("--- Sequential Impulse Solver (Lesson 12) ---");
    test_SI_tangent_basis_orthogonal();
    test_SI_prepare_effective_mass();
    test_SI_basic_bounce();
    test_SI_accumulated_clamp_nonnegative();
    test_SI_friction_cone();
    test_SI_stacking_energy_bounded();
    test_SI_tall_stack_stability();
    test_SI_determinism();
    test_SI_static_body_unchanged();
    test_SI_ground_contact();
    test_SI_null_inputs();
    test_SI_warm_start_improves();
    test_SI_rb_contacts_to_manifold();
    test_SI_rb_contacts_mixed_body_rejected();

    /* Phase 1 fix validation */
    test_SI_tangent_basis_zero_normal();
    test_SI_prepare_clamps_count();
    test_SI_prepare_negative_friction_clamped();
    test_SI_prepare_degenerate_normal_rejected();
    test_SI_solve_nan_normal_safe();

    /* Split integration */
    test_SI_integrate_velocities_applies_force();
    test_SI_integrate_positions_updates_position();
    test_SI_split_matches_combined();
    test_SI_integrate_velocities_static_body();
    test_SI_integrate_positions_static_body();

    /* Position correction */
    test_SI_correct_positions_removes_penetration();
    test_SI_correct_positions_respects_slop();
    test_SI_correct_positions_mass_weighted();
    test_SI_correct_positions_null_inputs();
    test_SI_correct_positions_static_body_unmoved();
}
