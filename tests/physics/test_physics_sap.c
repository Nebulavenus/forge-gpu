/*
 * Sweep-and-Prune Broadphase Tests
 *
 * Tests for forge_physics_sap_* functions in common/physics/forge_physics.h
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Helper: create AABB from center and half-extents ─────────────────── */

#define SAP_HALF            0.5f
#define SAP_DIRTY_SENTINEL  99    /* non-zero sentinel to verify init zeroes fields */

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
    int n = forge_physics_sap_pair_count(w);
    for (int i = 0; i < n; i++) {
        if (w->pairs[i].a == pa && w->pairs[i].b == pb) return true;
    }
    return false;
}

/* ── Tests ─────────────────────────────────────────────────────────────── */

static void test_sap_init(void)
{
    TEST("SAP_init — zeroes all fields including pointers");
    ForgePhysicsSAPWorld w;
    /* Dirty all fields so init must explicitly zero them */
    w.endpoints  = (ForgePhysicsSAPEndpoint *)(uintptr_t)0xDEADBEEF;
    w.pairs      = (ForgePhysicsSAPPair *)(uintptr_t)0xDEADBEEF;
    w.sweep_axis = SAP_DIRTY_SENTINEL;
    w.sort_ops   = SAP_DIRTY_SENTINEL;
    forge_physics_sap_init(&w);
    ASSERT_TRUE(w.endpoints == NULL);
    ASSERT_TRUE(w.pairs == NULL);
    ASSERT_TRUE(w.sweep_axis == 0);
    ASSERT_TRUE(w.sort_ops == 0);
    ASSERT_TRUE(w.sweep_arena.first != NULL);  /* arena backing block allocated */
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_init_null(void)
{
    TEST("SAP_init — NULL is safe");
    forge_physics_sap_init(NULL);  /* must not crash */
    ASSERT_TRUE(true);
    END_TEST();
}

static void test_sap_destroy_null(void)
{
    TEST("SAP_destroy — NULL is safe");
    forge_physics_sap_destroy(NULL);  /* must not crash */
    ASSERT_TRUE(true);
    END_TEST();
}

static void test_sap_destroy_empty(void)
{
    TEST("SAP_destroy — empty world is safe");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    forge_physics_sap_destroy(&w);
    ASSERT_TRUE(w.endpoints == NULL);
    ASSERT_TRUE(w.pairs == NULL);
    END_TEST();
}

static void test_sap_destroy_double(void)
{
    TEST("SAP_destroy — double destroy is safe");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(SAP_HALF, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 2);
    forge_physics_sap_destroy(&w);
    forge_physics_sap_destroy(&w);  /* second destroy must not crash */
    ASSERT_TRUE(w.endpoints == NULL);
    END_TEST();
}

static void test_sap_zero_bodies(void)
{
    TEST("SAP_update — zero bodies produces zero pairs");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    forge_physics_sap_update(&w, NULL, 0);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    forge_physics_sap_destroy(&w);
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
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_two_overlapping(void)
{
    TEST("SAP_update — two overlapping bodies produce one pair");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(SAP_HALF, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);
    ASSERT_TRUE(sap_has_pair(&w, 0, 1));
    forge_physics_sap_destroy(&w);
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
    forge_physics_sap_destroy(&w);
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
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_chain_overlaps(void)
{
    TEST("SAP_update — chain of 3 overlapping bodies: AB, BC, not AC");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    /* A=[0..1], B=[0.8..1.8], C=[1.6..2.6] — A overlaps B, B overlaps C, A does not overlap C */
    ForgePhysicsAABB aabbs[3] = {
        make_aabb(SAP_HALF, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(1.3f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(2.1f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 3);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 2);
    ASSERT_TRUE(sap_has_pair(&w, 0, 1));
    ASSERT_TRUE(sap_has_pair(&w, 1, 2));
    ASSERT_TRUE(!sap_has_pair(&w, 0, 2));
    forge_physics_sap_destroy(&w);
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
    forge_physics_sap_destroy(&w);
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
        make_aabb(SAP_HALF, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);

    /* Move body 1 far away */
    aabbs[1] = make_aabb(100, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    forge_physics_sap_destroy(&w);
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
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_large_body_count(void)
{
    TEST("SAP_update — handles 500 bodies (beyond old 256 limit)");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    #define SAP_LARGE_COUNT 500
    ForgePhysicsAABB *aabbs = (ForgePhysicsAABB *)SDL_calloc(
        SAP_LARGE_COUNT, sizeof(ForgePhysicsAABB));
    ASSERT_TRUE(aabbs != NULL);
    /* Spread bodies far apart so no pairs */
    for (int i = 0; i < SAP_LARGE_COUNT; i++)
        aabbs[i] = make_aabb((float)i * 10.0f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs, SAP_LARGE_COUNT);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    ASSERT_TRUE((int)forge_arr_length(w.endpoints) == SAP_LARGE_COUNT * 2);
    SDL_free(aabbs);
    forge_physics_sap_destroy(&w);
    #undef SAP_LARGE_COUNT
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

    forge_physics_sap_destroy(&w);
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
    forge_physics_sap_destroy(&w);
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
        make_aabb(SAP_HALF, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
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
    forge_physics_sap_destroy(&w);
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
        make_aabb(SAP_HALF, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs2, 2);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);
    ASSERT_TRUE(sap_has_pair(&w, 0, 1));
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_all_pairs_no_overflow(void)
{
    TEST("SAP_update — dynamic arrays grow without overflow");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    /* Place 256 bodies at the same position. C(256,2) = 32,640 pairs.
     * With dynamic arrays this must succeed — no overflow flag, no cap. */
    #define SAP_DENSE_COUNT 256
    ForgePhysicsAABB *dense = (ForgePhysicsAABB *)SDL_calloc(
        SAP_DENSE_COUNT, sizeof(ForgePhysicsAABB));
    ASSERT_TRUE(dense != NULL);
    for (int i = 0; i < SAP_DENSE_COUNT; i++)
        dense[i] = make_aabb(0.0f, 0.0f, 0.0f, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, dense, SAP_DENSE_COUNT);
    int expected_pairs = SAP_DENSE_COUNT * (SAP_DENSE_COUNT - 1) / 2;
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == expected_pairs);
    SDL_free(dense);
    forge_physics_sap_destroy(&w);
    #undef SAP_DENSE_COUNT
    END_TEST();
}

static void test_sap_no_duplicates_3_coincident(void)
{
    TEST("SAP_update — 3 coincident bodies, no duplicate pairs");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsAABB aabbs[3];
    for (int i = 0; i < 3; i++)
        aabbs[i] = make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs, 3);
    /* C(3,2) = 3 unique pairs — verify no duplicates */
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 3);
    ASSERT_TRUE(sap_has_pair(&w, 0, 1));
    ASSERT_TRUE(sap_has_pair(&w, 0, 2));
    ASSERT_TRUE(sap_has_pair(&w, 1, 2));
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_body_count_change_rebuilds_endpoints(void)
{
    TEST("SAP_update — changing body count rebuilds endpoints correctly");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    /* Frame 1: 4 separated bodies */
    ForgePhysicsAABB aabbs4[4];
    for (int i = 0; i < 4; i++)
        aabbs4[i] = make_aabb((float)i * 5.0f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs4, 4);
    ASSERT_TRUE((int)forge_arr_length(w.endpoints) == 8);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);

    /* Frame 2: shrink to 2 overlapping bodies */
    ForgePhysicsAABB aabbs2[2] = {
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        make_aabb(SAP_HALF, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
    };
    forge_physics_sap_update(&w, aabbs2, 2);
    ASSERT_TRUE((int)forge_arr_length(w.endpoints) == 4);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 1);

    /* Frame 3: grow to 3 separated bodies */
    ForgePhysicsAABB aabbs3[3];
    for (int i = 0; i < 3; i++)
        aabbs3[i] = make_aabb((float)i * 10.0f, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs3, 3);
    ASSERT_TRUE((int)forge_arr_length(w.endpoints) == 6);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);

    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_destroy_clears_populated_world(void)
{
    TEST("SAP_destroy — clears populated world, fields reset");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    /* Populate with 5 overlapping bodies */
    ForgePhysicsAABB aabbs[5];
    for (int i = 0; i < 5; i++)
        aabbs[i] = make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF);
    forge_physics_sap_update(&w, aabbs, 5);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 10);  /* C(5,2) */
    ASSERT_TRUE(w.endpoints != NULL);
    ASSERT_TRUE(w.pairs != NULL);

    forge_physics_sap_destroy(&w);
    ASSERT_TRUE(w.endpoints == NULL);
    ASSERT_TRUE(w.pairs == NULL);
    END_TEST();
}

static void test_sap_update_negative_count(void)
{
    TEST("SAP_update — negative count treated as zero");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    forge_physics_sap_update(&w, NULL, -1);
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_pair_count_null(void)
{
    TEST("SAP_pair_count — NULL world returns 0");
    ASSERT_TRUE(forge_physics_sap_pair_count(NULL) == 0);
    END_TEST();
}

static void test_sap_nan_aabb_no_pairs(void)
{
    TEST("SAP_update — NaN AABB endpoints clamped to 0, body excluded from pairs");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    /* Body 0: valid AABB at origin.
     * Body 1: NaN AABB — should be clamped to a zero-size point at 0,
     *         so its min and max endpoints coincide and it overlaps body 0
     *         only because both collapse to the same point.  The important
     *         invariant: no NaN in the output and no crash. */
    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        { .min = vec3_create(NAN, NAN, NAN),
          .max = vec3_create(NAN, NAN, NAN) },
    };
    forge_physics_sap_update(&w, aabbs, 2);

    /* Verify no endpoint value is NaN */
    int ep_count = (int)forge_arr_length(w.endpoints);
    for (int i = 0; i < ep_count; i++) {
        ASSERT_TRUE(w.endpoints[i].value == w.endpoints[i].value);  /* not NaN */
    }

    /* NaN AABB body must produce zero pairs — the sweep skips bodies
     * with non-finite AABBs to prevent bogus overlap results. */
    int n = forge_physics_sap_pair_count(&w);
    ASSERT_TRUE(n == 0);

    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_inf_aabb_no_crash(void)
{
    TEST("SAP_update — +inf AABB body excluded from pairs, no crash");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    ForgePhysicsAABB aabbs[2] = {
        make_aabb(0, 0, 0, SAP_HALF, SAP_HALF, SAP_HALF),
        { .min = vec3_create(-INFINITY, -INFINITY, -INFINITY),
          .max = vec3_create(INFINITY, INFINITY, INFINITY) },
    };
    forge_physics_sap_update(&w, aabbs, 2);

    /* All endpoint values must be finite */
    int ep_count = (int)forge_arr_length(w.endpoints);
    for (int i = 0; i < ep_count; i++) {
        ASSERT_TRUE(forge_isfinite(w.endpoints[i].value));
    }

    /* Inf AABB body must not produce pairs */
    ASSERT_TRUE(forge_physics_sap_pair_count(&w) == 0);

    forge_physics_sap_destroy(&w);
    END_TEST();
}

/* ── SAP Particle Collision Tests ──────────────────────────────────────── */

static ForgePhysicsParticle make_particle(float x, float y, float z,
                                          float radius)
{
    ForgePhysicsParticle p;
    SDL_memset(&p, 0, sizeof(p));
    p.position = vec3_create(x, y, z);
    p.prev_position = p.position;
    p.mass = 1.0f;
    p.inv_mass = 1.0f;
    p.radius = radius;
    p.restitution = 0.5f;
    return p;
}

static void test_sap_particles_two_overlapping(void)
{
    TEST("SAP_particles — 2 overlapping particles detect 1 contact");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    ForgePhysicsParticle parts[2];
    parts[0] = make_particle(0.0f, 0.0f, 0.0f, 1.0f);
    parts[1] = make_particle(1.5f, 0.0f, 0.0f, 1.0f);

    ForgePhysicsContact *contacts = NULL;
    int sap_pairs = 0;
    int n = forge_physics_collide_particles_sap(
        parts, 2, &w, &contacts, &sap_pairs);

    ASSERT_TRUE(n == 1);
    ASSERT_TRUE(sap_pairs >= 1);

    forge_arr_free(contacts);
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_particles_two_separated(void)
{
    TEST("SAP_particles — 2 separated particles detect 0 contacts");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    ForgePhysicsParticle parts[2];
    parts[0] = make_particle(0.0f, 0.0f, 0.0f, 0.5f);
    parts[1] = make_particle(10.0f, 0.0f, 0.0f, 0.5f);

    ForgePhysicsContact *contacts = NULL;
    int n = forge_physics_collide_particles_sap(
        parts, 2, &w, &contacts, NULL);

    ASSERT_TRUE(n == 0);

    forge_arr_free(contacts);
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_particles_matches_brute_force(void)
{
    TEST("SAP_particles — results match brute-force for 8 particles");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    /* Place 8 particles in a line with varying spacing — some overlap */
    ForgePhysicsParticle parts[8];
    for (int i = 0; i < 8; i++) {
        parts[i] = make_particle((float)i * 1.5f, 0.0f, 0.0f, 1.0f);
    }

    /* Brute force */
    ForgePhysicsContact *bf_contacts = NULL;
    int bf_n = forge_physics_collide_particles_all(parts, 8, &bf_contacts);

    /* SAP */
    ForgePhysicsContact *sap_contacts = NULL;
    int sap_n = forge_physics_collide_particles_sap(
        parts, 8, &w, &sap_contacts, NULL);

    ASSERT_TRUE(sap_n == bf_n);

    forge_arr_free(bf_contacts);
    forge_arr_free(sap_contacts);
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_particles_null_args(void)
{
    TEST("SAP_particles — NULL arguments return 0");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsContact *contacts = NULL;
    ForgePhysicsParticle p = make_particle(0.0f, 0.0f, 0.0f, 1.0f);

    ASSERT_TRUE(forge_physics_collide_particles_sap(NULL, 2, &w, &contacts, NULL) == 0);
    ASSERT_TRUE(forge_physics_collide_particles_sap(&p, 2, NULL, &contacts, NULL) == 0);
    ASSERT_TRUE(forge_physics_collide_particles_sap(&p, 2, &w, NULL, NULL) == 0);
    ASSERT_TRUE(forge_physics_collide_particles_sap(&p, 0, &w, &contacts, NULL) == 0);

    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_particles_step_null_args(void)
{
    TEST("SAP_particles_step — NULL arguments return 0");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);
    ForgePhysicsContact *contacts = NULL;
    ForgePhysicsParticle p = make_particle(0.0f, 0.0f, 0.0f, 1.0f);
    int pair_count = 99;

    /* NULL out_contacts also zeroes pair count */
    ASSERT_TRUE(forge_physics_collide_particles_sap_step(&p, 2, &w, NULL, &pair_count) == 0);
    ASSERT_TRUE(pair_count == 0);

    /* NULL particles zeroes pair count via inner _sap call */
    pair_count = 99;
    ASSERT_TRUE(forge_physics_collide_particles_sap_step(NULL, 2, &w, &contacts, &pair_count) == 0);
    ASSERT_TRUE(pair_count == 0);

    /* NULL sap zeroes pair count via inner _sap call */
    pair_count = 99;
    ASSERT_TRUE(forge_physics_collide_particles_sap_step(&p, 2, NULL, &contacts, &pair_count) == 0);
    ASSERT_TRUE(pair_count == 0);

    forge_arr_free(contacts);
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_particles_step(void)
{
    TEST("SAP_particles_step — detects and resolves collisions");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    ForgePhysicsParticle parts[2];
    parts[0] = make_particle(0.0f, 0.0f, 0.0f, 1.0f);
    parts[0].velocity = vec3_create(1.0f, 0.0f, 0.0f);
    parts[1] = make_particle(1.5f, 0.0f, 0.0f, 1.0f);
    parts[1].velocity = vec3_create(-1.0f, 0.0f, 0.0f);

    ForgePhysicsContact *contacts = NULL;
    int n = forge_physics_collide_particles_sap_step(
        parts, 2, &w, &contacts, NULL);

    ASSERT_TRUE(n == 1);
    /* Velocities should have changed after resolution */
    ASSERT_TRUE(parts[0].velocity.x < 1.0f);
    ASSERT_TRUE(parts[1].velocity.x > -1.0f);

    forge_arr_free(contacts);
    forge_physics_sap_destroy(&w);
    END_TEST();
}

static void test_sap_particles_temporal_coherence(void)
{
    TEST("SAP_particles — temporal coherence reduces sort ops");
    ForgePhysicsSAPWorld w;
    forge_physics_sap_init(&w);

    ForgePhysicsParticle parts[20];
    for (int i = 0; i < 20; i++) {
        parts[i] = make_particle((float)i * 2.0f, 0.0f, 0.0f, 0.8f);
    }

    /* First call builds from scratch */
    ForgePhysicsContact *contacts = NULL;
    forge_physics_collide_particles_sap(parts, 20, &w, &contacts, NULL);
    forge_arr_set_length(contacts, 0);

    /* Move particles slightly */
    for (int i = 0; i < 20; i++) {
        parts[i].position.x += 0.01f;
    }

    /* Second call should benefit from temporal coherence */
    forge_physics_collide_particles_sap(parts, 20, &w, &contacts, NULL);
    ASSERT_TRUE(w.sort_ops <= 5);  /* near-sorted → few swaps */

    forge_arr_free(contacts);
    forge_physics_sap_destroy(&w);
    END_TEST();
}

/* ── Runner ────────────────────────────────────────────────────────────── */

void run_sap_tests(void)
{
    SDL_Log("--- SAP broadphase ---");
    test_sap_init();
    test_sap_init_null();
    test_sap_destroy_null();
    test_sap_destroy_empty();
    test_sap_destroy_double();
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
    test_sap_large_body_count();
    test_sap_brute_force_comparison();
    test_sap_touching_counts_as_overlap();
    test_sap_sweep_axis_clamped();
    test_sap_temporal_coherence();
    test_sap_all_pairs_no_overflow();
    test_sap_vec3_axis_invalid();
    test_sap_no_duplicates_3_coincident();
    test_sap_body_count_change_rebuilds_endpoints();
    test_sap_destroy_clears_populated_world();
    test_sap_update_negative_count();
    test_sap_pair_count_null();
    test_sap_nan_aabb_no_pairs();
    test_sap_inf_aabb_no_crash();

    /* SAP particle collision */
    SDL_Log("--- SAP particle collision ---");
    test_sap_particles_two_overlapping();
    test_sap_particles_two_separated();
    test_sap_particles_matches_brute_force();
    test_sap_particles_null_args();
    test_sap_particles_step_null_args();
    test_sap_particles_step();
    test_sap_particles_temporal_coherence();
}
