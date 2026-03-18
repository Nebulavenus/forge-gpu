/*
 * Sweep-and-Prune Broadphase Tests
 *
 * Tests for forge_physics_sap_* functions in common/physics/forge_physics.h
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Helper: create AABB from center and half-extents ─────────────────── */

#define SAP_HALF 0.5f

static ForgePhysicsAABB make_aabb(float cx, float cy, float cz,
                                  float hx, float hy, float hz)
{
    ForgePhysicsAABB a;
    a.min = vec3_create(cx - hx, cy - hy, cz - hz);
    a.max = vec3_create(cx + hx, cy + hy, cz + hz);
    return a;
}

/* ── Helper: check if pair (a,b) exists in SAP world ─────────────────── */

static bool sap_has_pair(const ForgePhysicsSAPWorld *w, int a, int b)
{
    uint16_t pa = (uint16_t)(a < b ? a : b);
    uint16_t pb = (uint16_t)(a < b ? b : a);
    for (int i = 0; i < w->pair_count; i++) {
        if (w->pairs[i].a == pa && w->pairs[i].b == pb) return true;
    }
    return false;
}

/* ── Tests ─────────────────────────────────────────────────────────────── */

static void test_sap_init(void)
{
    TEST("SAP_init — zeroes all fields");
    ForgePhysicsSAPWorld w;
    SDL_memset(&w, 0xFF, sizeof(w));
    forge_physics_sap_init(&w);
    ASSERT_TRUE(w.endpoint_count == 0);
    ASSERT_TRUE(w.pair_count == 0);
    ASSERT_TRUE(w.sweep_axis == 0);
    ASSERT_TRUE(w.sort_ops == 0);
    ASSERT_TRUE(!w.pair_overflow);
    END_TEST();
}

static void test_sap_init_null(void)
{
    TEST("SAP_init — NULL is safe");
    forge_physics_sap_init(NULL);  /* must not crash */
    ASSERT_TRUE(true);
    END_TEST();
}

static void test_sap_zero_bodies(void)
{
    TEST("SAP_update — zero bodies produces zero pairs");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    forge_physics_sap_update(&w, NULL, 0);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    ASSERT_TRUE(forge_physics_sap_get_pairs(&w) != NULL);
    END_TEST();
}

static void test_sap_one_body(void)
{
    TEST("SAP_update — one body produces zero pairs");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[1] = { make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF) };
    forge_physics_sap_update(&w, aabbs, 1);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    END_TEST();
}

static void test_sap_two_overlapping(void)
{
    TEST("SAP_update — two overlapping bodies produce one pair");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0.5f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);
    ASSERT_TRUE(sap_has_pair(&w, 0, 1));
    END_TEST();
}

static void test_sap_two_separated(void)
{
    TEST("SAP_update — two separated bodies produce zero pairs");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(10, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    END_TEST();
}

static void test_sap_pair_ordering(void)
{
    TEST("SAP_update — pairs always have a < b");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    /* Body 1 at left, body 0 at right — pair should still be (0, 1) */
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(1, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);
    ASSERT_TRUE(w.pairs[0].a < w.pairs[0].b);
    END_TEST();
}

static void test_sap_chain_overlaps(void)
{
    TEST("SAP_update — chain of 3 overlapping bodies: AB, BC, not AC");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    /* A=[0..1], B=[0.8..1.8], C=[1.6..2.6] — A overlaps B, B overlaps C, A does not overlap C */
    ForgePhysicsAABB aabbs[3] = {
        make_aabb(0.5f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(1.3f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(2.1f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 3);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 2);
    ASSERT_TRUE(sap_has_pair(&w, 0, 1));
    ASSERT_TRUE(sap_has_pair(&w, 1, 2));
    ASSERT_TRUE(!sap_has_pair(&w, 0, 2));
    END_TEST();
}

static void test_sap_all_same_position(void)
{
    TEST("SAP_update — all bodies at same position, all pairs found");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[4];
    for (int i = 0; i < 4; i++)
        aabbs[i] = make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs, 4);
    /* 4 bodies all overlapping: C(4,2) = 6 pairs */
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 6);
    END_TEST();
}

static void test_sap_axis_select_x(void)
{
    TEST("SAP_select_axis — spread on X selects axis 0");
    ForgePhysicsAABB aabbs[3] = {
        make_aabb(-10, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(10, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    int axis = forge_physics_sap_select_axis(aabbs, 3);
    ASSERT_TRUE(axis == 0);
    END_TEST();
}

static void test_sap_axis_select_y(void)
{
    TEST("SAP_select_axis — spread on Y selects axis 1");
    ForgePhysicsAABB aabbs[3] = {
        make_aabb(0, -10, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0, 10, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    int axis = forge_physics_sap_select_axis(aabbs, 3);
    ASSERT_TRUE(axis == 1);
    END_TEST();
}

static void test_sap_axis_select_z(void)
{
    TEST("SAP_select_axis — spread on Z selects axis 2");
    ForgePhysicsAABB aabbs[3] = {
        make_aabb(0, 0, -10, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0, 0, 10, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    int axis = forge_physics_sap_select_axis(aabbs, 3);
    ASSERT_TRUE(axis == 2);
    END_TEST();
}

static void test_sap_incremental_move(void)
{
    TEST("SAP_update — moving body apart removes the pair");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0.5f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);

    /* Move body 1 far away */
    aabbs[1] = make_aabb(100, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    END_TEST();
}

static void test_sap_no_duplicates(void)
{
    TEST("SAP_update — no duplicate pairs");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    /* Two bodies that clearly overlap */
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0, 0, 0, 1.0f, 1.0f, 1.0f),
        make_aabb(0, 0, 0, 1.0f, 1.0f, 1.0f),
    };
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);
    END_TEST();
}

static void test_sap_max_capacity(void)
{
    TEST("SAP_update — handles SAP_MAX_BODIES bodies");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[FORGE_PHYSICS_SAP_MAX_BODIES];
    /* Spread bodies far apart so no pairs */
    for (int i = 0; i < FORGE_PHYSICS_SAP_MAX_BODIES; i++)
        aabbs[i] = make_aabb((float)i * 10.0f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs, FORGE_PHYSICS_SAP_MAX_BODIES);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    ASSERT_TRUE(w.endpoint_count == FORGE_PHYSICS_SAP_MAX_BODIES * 2);
    END_TEST();
}

static void test_sap_over_capacity_clamp(void)
{
    TEST("SAP_update — count > SAP_MAX_BODIES is clamped");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    /* Allocate a full-size array so the clamped read is in bounds */
    ForgePhysicsAABB aabbs[FORGE_PHYSICS_SAP_MAX_BODIES];
    for (int i = 0; i < FORGE_PHYSICS_SAP_MAX_BODIES; i++)
        aabbs[i] = make_aabb((float)i * 10.0f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs, FORGE_PHYSICS_SAP_MAX_BODIES + 10);
    /* Clamped to max bodies, endpoints should be 2 * max */
    ASSERT_TRUE(w.endpoint_count == FORGE_PHYSICS_SAP_MAX_BODIES * 2);
    END_TEST();
}

static void test_sap_brute_force_comparison(void)
{
    TEST("SAP_update — matches brute-force pair count for grid layout");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    /* Create a grid of bodies — adjacent ones overlap with 0.8 spacing */
    #define BF_COUNT 16
    #define BF_SPACING 0.8f
    ForgePhysicsAABB aabbs[BF_COUNT];
    for (int i = 0; i < BF_COUNT; i++) {
        float x = (float)(i % 4) * BF_SPACING;
        float z = (float)(i / 4) * BF_SPACING;
        aabbs[i] = make_aabb(x, 0, z, SAP_HALF, SAP_HALF, SAP_HALF);
    }

    /* Brute-force count */
    int bf_pairs = 0;
    for (int i = 0; i < BF_COUNT; i++) {
        for (int j = i + 1; j < BF_COUNT; j++) {
            if (forge_physics_aabb_overlap(aabbs[i], aabbs[j]))
                bf_pairs++;
        }
    }

    w.sweep_axis = forge_physics_sap_select_axis(aabbs, BF_COUNT);
    forge_physics_sap_update(&w, aabbs, BF_COUNT);
    ASSERT_TRUE(bf_pairs > 0);  /* spacing ensures some overlaps */
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == bf_pairs);

    /* Pair-by-pair verification: every brute-force pair must appear in SAP
     * and vice versa. Count-only comparison can hide false positive/negative
     * cancellation. */
    for (int i = 0; i < BF_COUNT; i++) {
        for (int j = i + 1; j < BF_COUNT; j++) {
            bool expected = forge_physics_aabb_overlap(aabbs[i], aabbs[j]);
            ASSERT_TRUE(sap_has_pair(&w, i, j) == expected);
        }
    }

    #undef BF_SPACING
    #undef BF_COUNT
    END_TEST();
}

static void test_sap_vec3_axis_invalid(void)
{
    TEST("SAP_vec3_axis — invalid axis returns 0");
    vec3 v = vec3_create(1.0f, 2.0f, 3.0f);
    ASSERT_NEAR(forge_physics_vec3_axis(v, -1), 0.0f, EPSILON);
    ASSERT_NEAR(forge_physics_vec3_axis(v, 3), 0.0f, EPSILON);
    /* Also verify valid axes */
    ASSERT_NEAR(forge_physics_vec3_axis(v, 0), 1.0f, EPSILON);
    ASSERT_NEAR(forge_physics_vec3_axis(v, 1), 2.0f, EPSILON);
    ASSERT_NEAR(forge_physics_vec3_axis(v, 2), 3.0f, EPSILON);
    END_TEST();
}

static void test_sap_touching_counts_as_overlap(void)
{
    TEST("SAP_update — touching endpoints count as overlap");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0.0f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF), /* x in [-0.5, 0.5] */
        make_aabb(1.0f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF), /* x in [ 0.5, 1.5] */
    };
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);
    ASSERT_TRUE(sap_has_pair(&w, 0, 1));
    END_TEST();
}

static void test_sap_sweep_axis_clamped(void)
{
    TEST("SAP_update — invalid sweep_axis clamped to 0");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    w.sweep_axis = 5;  /* positive out-of-range */
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0.0f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0.5f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(w.sweep_axis == 0);
    /* Bodies overlap on x — should still detect the pair */
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);

    /* Negative out-of-range */
    w.sweep_axis = -1;
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(w.sweep_axis == 0);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);
    END_TEST();
}

static void test_sap_temporal_coherence(void)
{
    TEST("SAP_update — preserves sort order across frames (temporal coherence)");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    /* Frame 1: bodies out of spatial order so the initial sort needs swaps.
     * Body 0 at x=10, body 1 at x=0, body 2 at x=5 — endpoints arrive
     * in body-index order which is NOT sorted by value. */
    ForgePhysicsAABB aabbs[3] = {
        make_aabb(10.0f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0.0f,  0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(5.0f,  0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    w.sweep_axis = 0;
    forge_physics_sap_update(&w, aabbs, 3);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    int first_sort_ops = w.sort_ops;
    ASSERT_TRUE(first_sort_ops > 0);  /* must require swaps on first frame */

    /* Frame 2: same count, tiny position change — should need fewer sort ops
     * than the initial sort because the prior order is preserved */
    aabbs[0] = make_aabb(10.01f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF);
    aabbs[1] = make_aabb(0.01f,  0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF);
    aabbs[2] = make_aabb(5.01f,  0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs, 3);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    /* Tiny movement preserves sort order — should need zero or very few swaps */
    ASSERT_TRUE(w.sort_ops < first_sort_ops);

    /* Frame 3: change count — forces rebuild, but must still be correct */
    ForgePhysicsAABB aabbs2[2] = {
        make_aabb(0.0f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0.5f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs2, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);
    ASSERT_TRUE(sap_has_pair(&w, 0, 1));
    END_TEST();
}

static void test_sap_pair_overflow(void)
{
    TEST("SAP_update — sets pair_overflow when buffer is exceeded");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    /* No overflow for a simple scene */
    ForgePhysicsAABB simple[2] = {
        make_aabb(0.0f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(0.5f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, simple, 2);
    ASSERT_TRUE(!w.pair_overflow);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);

    /* Trigger overflow: place all MAX_BODIES at the same position.
     * Every body overlaps every other: C(N,2) = N*(N-1)/2 = 32,640 pairs,
     * which exceeds FORGE_PHYSICS_SAP_MAX_PAIRS (4096). */
    ForgePhysicsAABB dense[FORGE_PHYSICS_SAP_MAX_BODIES];
    for (int i = 0; i < FORGE_PHYSICS_SAP_MAX_BODIES; i++)
        dense[i] = make_aabb(0.0f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, dense, FORGE_PHYSICS_SAP_MAX_BODIES);
    ASSERT_TRUE(w.pair_overflow);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == FORGE_PHYSICS_SAP_MAX_PAIRS);
    END_TEST();
}

/* ── Runner ────────────────────────────────────────────────────────────── */

void run_sap_tests(void)
{
    SDL_Log("--- SAP broadphase ---");
    test_sap_init();
    test_sap_init_null();
    test_sap_zero_bodies();
    test_sap_one_body();
    test_sap_two_overlapping();
    test_sap_two_separated();
    test_sap_pair_ordering();
    test_sap_chain_overlaps();
    test_sap_all_same_position();
    test_sap_axis_select_x();
    test_sap_axis_select_y();
    test_sap_axis_select_z();
    test_sap_incremental_move();
    test_sap_no_duplicates();
    test_sap_max_capacity();
    test_sap_over_capacity_clamp();
    test_sap_brute_force_comparison();
    test_sap_touching_counts_as_overlap();
    test_sap_sweep_axis_clamped();
    test_sap_temporal_coherence();
    test_sap_pair_overflow();
    test_sap_vec3_axis_invalid();
}
