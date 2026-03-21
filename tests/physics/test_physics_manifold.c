/*
 * Contact Manifold Tests
 *
 * Tests for the contact manifold system (Physics Lesson 11):
 * pair keys, polygon clipping, manifold generation, cache operations,
 * and contact point reduction.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "test_physics_common.h"

/* ── Constants ────────────────────────────────────────────────────── */

#define MAN_BOX_HALF        0.5f
#define MAN_SPHERE_R        0.5f
#define MAN_MU_S            0.6f
#define MAN_MU_D            0.4f
#define MAN_BODY_MASS       1.0f
#define MAN_DAMPING         0.99f
#define MAN_RESTITUTION     0.3f
#define MAN_BODY_RESTIT     0.8f
#define MAN_SPHERE_SEP      0.8f    /* sphere separation for overlap tests  */
#define MAN_WARM_IMPULSE    10.0f   /* test impulse for warm-start tests    */

/* ── Helpers ──────────────────────────────────────────────────────── */

static ForgePhysicsRigidBody make_body(vec3 pos, float mass)
{
    ForgePhysicsRigidBody b = forge_physics_rigid_body_create(
        pos, mass, MAN_DAMPING, MAN_RESTITUTION, MAN_BODY_RESTIT);
    return b;
}

static ForgePhysicsCollisionShape make_box(float half)
{
    ForgePhysicsCollisionShape s;
    s.type = FORGE_PHYSICS_SHAPE_BOX;
    s.data.box.half_extents = vec3_create(half, half, half);
    return s;
}

static ForgePhysicsCollisionShape make_sphere(float radius)
{
    ForgePhysicsCollisionShape s;
    s.type = FORGE_PHYSICS_SHAPE_SPHERE;
    s.data.sphere.radius = radius;
    return s;
}

/* ── Pair Key Tests ───────────────────────────────────────────────── */

static void test_MAN_pair_key_symmetric(void)
{
    TEST("MAN_pair_key_symmetric")
    uint64_t k1 = forge_physics_manifold_pair_key(3, 7);
    uint64_t k2 = forge_physics_manifold_pair_key(7, 3);
    ASSERT_TRUE(k1 == k2);
    END_TEST();
}

static void test_MAN_pair_key_distinct(void)
{
    TEST("MAN_pair_key_distinct")
    uint64_t k1 = forge_physics_manifold_pair_key(1, 2);
    uint64_t k2 = forge_physics_manifold_pair_key(1, 3);
    uint64_t k3 = forge_physics_manifold_pair_key(2, 3);
    ASSERT_TRUE(k1 != k2);
    ASSERT_TRUE(k1 != k3);
    ASSERT_TRUE(k2 != k3);
    END_TEST();
}

static void test_MAN_pair_key_same_body(void)
{
    TEST("MAN_pair_key_same_body")
    uint64_t k = forge_physics_manifold_pair_key(5, 5);
    /* Should still produce a valid key */
    ASSERT_TRUE(k == ((5ULL << 32) | 5ULL));
    END_TEST();
}

/* ── Polygon Clipping Tests ───────────────────────────────────────── */

static void test_MAN_clip_polygon_no_clip(void)
{
    TEST("MAN_clip_polygon_no_clip")
    /* Square fully inside the half-plane (all dot products <= plane_d) */
    vec3 in[4] = {
        {-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1}
    };
    vec3 out[8];
    vec3 plane_n = {1, 0, 0};   /* clip plane: x <= 5 */
    float plane_d = 5.0f;
    int count = forge_physics_clip_polygon(in, 4, out, plane_n, plane_d);
    ASSERT_TRUE(count == 4);
    END_TEST();
}

static void test_MAN_clip_polygon_full_clip(void)
{
    TEST("MAN_clip_polygon_full_clip")
    /* Square fully outside the half-plane */
    vec3 in[4] = {
        {10, 0, -1}, {12, 0, -1}, {12, 0, 1}, {10, 0, 1}
    };
    vec3 out[8];
    vec3 plane_n = {1, 0, 0};   /* clip plane: x <= 5 */
    float plane_d = 5.0f;
    int count = forge_physics_clip_polygon(in, 4, out, plane_n, plane_d);
    ASSERT_TRUE(count == 0);
    END_TEST();
}

static void test_MAN_clip_polygon_partial(void)
{
    TEST("MAN_clip_polygon_partial")
    /* Square from x=-1 to x=3, clip at x <= 1 → should produce 4 verts */
    vec3 in[4] = {
        {-1, 0, -1}, {3, 0, -1}, {3, 0, 1}, {-1, 0, 1}
    };
    vec3 out[8];
    vec3 plane_n = {1, 0, 0};   /* clip plane: x <= 1 */
    float plane_d = 1.0f;
    int count = forge_physics_clip_polygon(in, 4, out, plane_n, plane_d);
    ASSERT_TRUE(count == 4);
    /* All output vertices should have x <= 1 + epsilon */
    for (int i = 0; i < count; i++) {
        ASSERT_TRUE(out[i].x <= 1.0f + 0.01f);
    }
    END_TEST();
}

static void test_MAN_clip_null_inputs(void)
{
    TEST("MAN_clip_null_inputs")
    vec3 out[4];
    vec3 n = {1, 0, 0};
    ASSERT_TRUE(forge_physics_clip_polygon(NULL, 4, out, n, 1.0f) == 0);
    vec3 in[1] = {{0, 0, 0}};
    ASSERT_TRUE(forge_physics_clip_polygon(in, 4, NULL, n, 1.0f) == 0);
    ASSERT_TRUE(forge_physics_clip_polygon(in, 0, out, n, 1.0f) == 0);
    END_TEST();
}

/* ── Manifold Generation Tests ────────────────────────────────────── */

static void test_MAN_generate_sphere_sphere(void)
{
    TEST("MAN_generate_sphere_sphere")
    /* Two overlapping spheres — should produce exactly 1 contact */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0, 0), MAN_BODY_MASS);
    ForgePhysicsRigidBody b = make_body(vec3_create(MAN_SPHERE_SEP, 0, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_sphere(MAN_SPHERE_R);
    ForgePhysicsCollisionShape sb = make_sphere(MAN_SPHERE_R);

    ForgePhysicsManifold m;
    bool hit = forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m);
    ASSERT_TRUE(hit);
    ASSERT_TRUE(m.count == 1);
    ASSERT_TRUE(m.contacts[0].penetration > 0.0f);
    ASSERT_TRUE(m.body_a == 0);
    ASSERT_TRUE(m.body_b == 1);
    END_TEST();
}

static void test_MAN_generate_box_box_face(void)
{
    TEST("MAN_generate_box_box_face")
    /* Two boxes with flat face-face contact — should produce 4 contacts */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0.5f, 0), MAN_BODY_MASS);
    ForgePhysicsRigidBody b = make_body(vec3_create(0, -0.3f, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);

    ForgePhysicsManifold m;
    bool hit = forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m);
    ASSERT_TRUE(hit);
    /* Axis-aligned box-box face contact should produce exactly 4 contacts */
    ASSERT_TRUE(m.count == 4);
    ASSERT_TRUE(m.count <= FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS);
    /* All contacts should have positive penetration */
    for (int i = 0; i < m.count; i++) {
        ASSERT_TRUE(m.contacts[i].penetration >= 0.0f);
    }
    END_TEST();
}

static void test_MAN_generate_box_box_edge(void)
{
    TEST("MAN_generate_box_box_edge")
    /* Two boxes at 45 degrees — edge-edge contact, fewer contacts */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0.6f, 0), MAN_BODY_MASS);
    a.orientation = quat_from_euler(0, 0, 0.785f); /* 45 deg around Z */
    forge_physics_rigid_body_update_derived(&a);
    ForgePhysicsRigidBody b = make_body(vec3_create(0, -0.3f, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);

    ForgePhysicsManifold m;
    bool hit = forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m);
    ASSERT_TRUE(hit);
    ASSERT_TRUE(m.count >= 1);
    ASSERT_TRUE(m.count <= FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS);
    END_TEST();
}

static void test_MAN_generate_separated(void)
{
    TEST("MAN_generate_separated")
    /* Two boxes far apart — no contact */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0, 0), MAN_BODY_MASS);
    ForgePhysicsRigidBody b = make_body(vec3_create(10, 0, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);

    ForgePhysicsManifold m;
    bool hit = forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m);
    ASSERT_TRUE(!hit);
    END_TEST();
}

static void test_MAN_generate_null_inputs(void)
{
    TEST("MAN_generate_null_inputs")
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsManifold m;
    ASSERT_TRUE(!forge_physics_gjk_epa_manifold(
        NULL, &sa, &a, &sa, 0, 1, MAN_MU_S, MAN_MU_D, &m));
    ASSERT_TRUE(!forge_physics_gjk_epa_manifold(
        &a, &sa, &a, &sa, 0, 1, 0.6f, 0.4f, NULL));
    END_TEST();
}

/* ── Contact ID Tests ─────────────────────────────────────────────── */

static void test_MAN_contact_ids_stable(void)
{
    TEST("MAN_contact_ids_stable")
    /* Same configuration produces same IDs */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0.5f, 0), MAN_BODY_MASS);
    ForgePhysicsRigidBody b = make_body(vec3_create(0, -0.3f, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);

    ForgePhysicsManifold m1, m2;
    ASSERT_TRUE(forge_physics_gjk_epa_manifold(&a, &sa, &b, &sb, 0, 1,
        MAN_MU_S, MAN_MU_D, &m1));
    ASSERT_TRUE(forge_physics_gjk_epa_manifold(&a, &sa, &b, &sb, 0, 1,
        MAN_MU_S, MAN_MU_D, &m2));

    ASSERT_TRUE(m1.count == m2.count);
    for (int i = 0; i < m1.count; i++) {
        ASSERT_TRUE(m1.contacts[i].id == m2.contacts[i].id);
    }
    END_TEST();
}

/* ── Cache Tests ──────────────────────────────────────────────────── */

static void test_MAN_cache_insert(void)
{
    TEST("MAN_cache_insert")
    ForgePhysicsManifoldCacheEntry *cache = NULL;

    ForgePhysicsManifold m;
    SDL_memset(&m, 0, sizeof(m));
    m.body_a = 0;
    m.body_b = 1;
    m.normal = vec3_create(0, 1, 0);
    m.count = 1;
    m.contacts[0].world_point = vec3_create(0, 0, 0);
    m.contacts[0].penetration = 0.1f;
    m.contacts[0].id = 42;

    forge_physics_manifold_cache_update(&cache, &m);
    ASSERT_TRUE(forge_hm_length(cache) == 1);

    uint64_t key = forge_physics_manifold_pair_key(0, 1);
    ForgePhysicsManifoldCacheEntry e = forge_hm_get_struct(cache, key);
    ASSERT_TRUE(e.manifold.count == 1);
    ASSERT_TRUE(e.manifold.contacts[0].id == 42);

    forge_physics_manifold_cache_free(&cache);
    END_TEST();
}

static void test_MAN_cache_warmstart(void)
{
    TEST("MAN_cache_warmstart")
    ForgePhysicsManifoldCacheEntry *cache = NULL;

    /* Insert manifold with accumulated impulse */
    ForgePhysicsManifold m1;
    SDL_memset(&m1, 0, sizeof(m1));
    m1.body_a = 0;
    m1.body_b = 1;
    m1.normal = vec3_create(0, 1, 0);
    m1.count = 1;
    m1.contacts[0].id = 100;
    m1.contacts[0].normal_impulse = MAN_WARM_IMPULSE;
    m1.contacts[0].penetration = 0.1f;
    m1.contacts[0].world_point = vec3_create(0, 0, 0);
    forge_physics_manifold_cache_update(&cache, &m1);

    /* Update with same contact ID — should carry impulse */
    ForgePhysicsManifold m2;
    SDL_memset(&m2, 0, sizeof(m2));
    m2.body_a = 0;
    m2.body_b = 1;
    m2.normal = vec3_create(0, 1, 0);
    m2.count = 1;
    m2.contacts[0].id = 100;
    m2.contacts[0].normal_impulse = 0.0f;
    m2.contacts[0].penetration = 0.05f;
    m2.contacts[0].world_point = vec3_create(0, 0, 0);
    forge_physics_manifold_cache_update(&cache, &m2);

    uint64_t key = forge_physics_manifold_pair_key(0, 1);
    ForgePhysicsManifoldCacheEntry e = forge_hm_get_struct(cache, key);
    /* Impulse should be warm-started (10.0 * WARM_SCALE) */
    ASSERT_NEAR(e.manifold.contacts[0].normal_impulse,
                MAN_WARM_IMPULSE * FORGE_PHYSICS_MANIFOLD_WARM_SCALE, EPSILON);

    forge_physics_manifold_cache_free(&cache);
    END_TEST();
}

static void test_MAN_cache_prune(void)
{
    TEST("MAN_cache_prune")
    ForgePhysicsManifoldCacheEntry *cache = NULL;

    /* Insert two manifolds */
    ForgePhysicsManifold m1;
    SDL_memset(&m1, 0, sizeof(m1));
    m1.body_a = 0; m1.body_b = 1;
    m1.normal = vec3_create(0, 1, 0);
    m1.count = 1;
    m1.contacts[0].penetration = 0.1f;
    m1.contacts[0].world_point = vec3_create(0, 0, 0);
    forge_physics_manifold_cache_update(&cache, &m1);

    ForgePhysicsManifold m2;
    SDL_memset(&m2, 0, sizeof(m2));
    m2.body_a = 2; m2.body_b = 3;
    m2.normal = vec3_create(0, 1, 0);
    m2.count = 1;
    m2.contacts[0].penetration = 0.1f;
    m2.contacts[0].world_point = vec3_create(0, 0, 0);
    forge_physics_manifold_cache_update(&cache, &m2);

    ASSERT_TRUE(forge_hm_length(cache) == 2);

    /* Prune with only pair (0,1) active */
    uint64_t active[1];
    active[0] = forge_physics_manifold_pair_key(0, 1);
    forge_physics_manifold_cache_prune(&cache, active, 1);

    ASSERT_TRUE(forge_hm_length(cache) == 1);

    forge_physics_manifold_cache_free(&cache);
    END_TEST();
}

/* ── Reduction Tests ──────────────────────────────────────────────── */

static void test_MAN_reduce_to_four(void)
{
    TEST("MAN_reduce_to_four")
    /* 6 points — reduce should pick 4 maximizing area */
    vec3 points[6] = {
        {0, 0, 0}, {1, 0, 0}, {2, 0, 0},
        {0, 0, 1}, {1, 0, 1}, {2, 0, 1}
    };
    float depths[6] = {0.1f, 0.2f, 0.15f, 0.1f, 0.05f, 0.1f};
    int indices[4];
    int n = forge_physics_manifold_reduce(points, depths, 6, indices);
    ASSERT_TRUE(n == 4);
    /* Deepest (index 1, depth 0.2) must be selected */
    bool has_deepest = false;
    for (int i = 0; i < 4; i++) {
        if (indices[i] == 1) has_deepest = true;
    }
    ASSERT_TRUE(has_deepest);
    END_TEST();
}

/* ── Conversion Tests ─────────────────────────────────────────────── */

static void test_MAN_to_rb_contacts(void)
{
    TEST("MAN_to_rb_contacts")
    ForgePhysicsManifold m;
    SDL_memset(&m, 0, sizeof(m));
    m.body_a = 0; m.body_b = 1;
    m.normal = vec3_create(0, 1, 0);
    m.static_friction = MAN_MU_S;
    m.dynamic_friction = MAN_MU_D;
    m.count = 2;
    m.contacts[0].world_point = vec3_create(1, 0, 0);
    m.contacts[0].penetration = 0.1f;
    m.contacts[1].world_point = vec3_create(-1, 0, 0);
    m.contacts[1].penetration = 0.2f;

    ForgePhysicsRBContact out[4];
    int n = forge_physics_manifold_to_rb_contacts(&m, out, 4);
    ASSERT_TRUE(n == 2);
    ASSERT_NEAR(out[0].penetration, 0.1f, EPSILON);
    ASSERT_NEAR(out[1].penetration, 0.2f, EPSILON);
    ASSERT_NEAR(out[0].normal.y, 1.0f, EPSILON);
    ASSERT_TRUE(out[0].body_a == 0);
    ASSERT_TRUE(out[1].body_b == 1);
    END_TEST();
}

/* ── Numerical Safety Tests ────────────────────────────────────────── */

static void test_MAN_cache_prune_null_keys(void)
{
    TEST("MAN_cache_prune_null_keys")
    ForgePhysicsManifoldCacheEntry *cache = NULL;
    ForgePhysicsManifold m;
    SDL_memset(&m, 0, sizeof(m));
    m.body_a = 0; m.body_b = 1;
    m.normal = vec3_create(0, 1, 0);
    m.count = 1;
    m.contacts[0].penetration = 0.1f;
    m.contacts[0].world_point = vec3_create(0, 0, 0);
    forge_physics_manifold_cache_update(&cache, &m);
    /* NULL active_keys with count > 0 should not crash */
    forge_physics_manifold_cache_prune(&cache, NULL, 1);
    /* Cache should be unchanged since prune returns early */
    ASSERT_TRUE(forge_hm_length(cache) == 1);
    forge_physics_manifold_cache_free(&cache);
    END_TEST();
}

static void test_MAN_cache_update_overflow_count(void)
{
    TEST("MAN_cache_update_overflow_count")
    ForgePhysicsManifoldCacheEntry *cache = NULL;
    ForgePhysicsManifold m;
    SDL_memset(&m, 0, sizeof(m));
    m.body_a = 0; m.body_b = 1;
    m.normal = vec3_create(0, 1, 0);
    m.count = 100; /* corrupted count — exceeds MAX_CONTACTS */
    /* Should be rejected silently, not crash */
    forge_physics_manifold_cache_update(&cache, &m);
    ASSERT_TRUE(forge_hm_length(cache) == 0);
    forge_physics_manifold_cache_free(&cache);
    END_TEST();
}

static void test_MAN_reduce_collinear(void)
{
    TEST("MAN_reduce_collinear")
    /* 5 collinear points — reduction should still produce 4 distinct indices */
    vec3 points[5] = {
        {0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}
    };
    float depths[5] = {0.1f, 0.2f, 0.15f, 0.1f, 0.05f};
    int indices[4];
    int n = forge_physics_manifold_reduce(points, depths, 5, indices);
    ASSERT_TRUE(n == 4);
    /* All 4 indices should be distinct */
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            ASSERT_TRUE(indices[i] != indices[j]);
        }
    }
    END_TEST();
}

static void test_MAN_generate_static_static(void)
{
    TEST("MAN_generate_static_static")
    /* Two zero-mass (static) bodies — GJK should still detect overlap */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0.5f, 0), 0.0f);
    ForgePhysicsRigidBody b = make_body(vec3_create(0, -0.3f, 0), 0.0f);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);
    ForgePhysicsManifold m;
    bool hit = forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m);
    /* GJK is a geometry query — mass does not affect it. Overlapping
     * shapes must produce a manifold regardless of mass. */
    ASSERT_TRUE(hit);
    ASSERT_TRUE(m.count >= 1);
    ASSERT_TRUE(m.count <= FORGE_PHYSICS_MANIFOLD_MAX_CONTACTS);
    END_TEST();
}

static void test_MAN_determinism(void)
{
    TEST("MAN_determinism")
    /* Two identical runs must produce bit-identical manifolds */
    ForgePhysicsRigidBody a1 = make_body(vec3_create(0.1f, 0.5f, 0.2f), MAN_BODY_MASS);
    ForgePhysicsRigidBody b1 = make_body(vec3_create(0.0f, -0.2f, 0.1f), MAN_BODY_MASS);
    ForgePhysicsRigidBody a2 = make_body(vec3_create(0.1f, 0.5f, 0.2f), MAN_BODY_MASS);
    ForgePhysicsRigidBody b2 = make_body(vec3_create(0.0f, -0.2f, 0.1f), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);
    ForgePhysicsManifold m1, m2;
    ASSERT_TRUE(forge_physics_gjk_epa_manifold(
        &a1, &sa, &b1, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m1));
    ASSERT_TRUE(forge_physics_gjk_epa_manifold(
        &a2, &sa, &b2, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m2));
    ASSERT_TRUE(m1.count == m2.count);
    for (int i = 0; i < m1.count; i++) {
        ASSERT_NEAR(m1.contacts[i].penetration, m2.contacts[i].penetration, 0.0f);
        ASSERT_NEAR(m1.contacts[i].world_point.x, m2.contacts[i].world_point.x, 0.0f);
        ASSERT_NEAR(m1.contacts[i].world_point.y, m2.contacts[i].world_point.y, 0.0f);
        ASSERT_NEAR(m1.contacts[i].world_point.z, m2.contacts[i].world_point.z, 0.0f);
    }
    END_TEST();
}

/* ── Tests for PR feedback fixes ───────────────────────────────────── */

static void test_MAN_clip_contacts_inside_ref_face(void)
{
    TEST("MAN_clip_contacts_inside_ref_face")
    /* After the clipping normal fix, box-box contacts must lie within
     * the reference face bounds — not be clipped away to nothing or
     * placed outside the face. */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0.5f, 0), MAN_BODY_MASS);
    ForgePhysicsRigidBody b = make_body(vec3_create(0, -0.3f, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);
    ForgePhysicsManifold m;
    ASSERT_TRUE(forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m));
    ASSERT_TRUE(m.count >= 1);
    /* All contact world_points should be within the overlap region.
     * For axis-aligned boxes, x and z should be within half-extents. */
    for (int i = 0; i < m.count; i++) {
        ASSERT_TRUE(SDL_fabsf(m.contacts[i].world_point.x) <=
                    MAN_BOX_HALF + EPSILON);
        ASSERT_TRUE(SDL_fabsf(m.contacts[i].world_point.z) <=
                    MAN_BOX_HALF + EPSILON);
    }
    END_TEST();
}

static void test_MAN_box_box_penetration_depth(void)
{
    TEST("MAN_box_box_penetration_depth")
    /* Boxes at y=0.5 (half=0.5, bottom at y=0) and y=-0.3 (half=0.5,
     * top at y=0.2). Overlap is 0.2 units. Penetration depth for each
     * contact should be close to 0.2, not ~1.8 (which would indicate
     * the face selection used the far faces instead of the touching faces). */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0.5f, 0), MAN_BODY_MASS);
    ForgePhysicsRigidBody b = make_body(vec3_create(0, -0.3f, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);
    ForgePhysicsManifold m;
    ASSERT_TRUE(forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m));
    ASSERT_TRUE(m.count >= 1);
    for (int i = 0; i < m.count; i++) {
        /* Penetration should be approximately 0.2 (the overlap distance) */
        ASSERT_NEAR(m.contacts[i].penetration, 0.2f, 0.05f);
    }
    END_TEST();
}

static void test_MAN_epa_fallback_distinct_locals(void)
{
    TEST("MAN_epa_fallback_distinct_locals")
    /* Sphere-sphere produces a single EPA contact. After the fix,
     * local_a should be relative to body A and local_b to body B,
     * using epa->point_a and point_b respectively (not the midpoint). */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0, 0), MAN_BODY_MASS);
    ForgePhysicsRigidBody b = make_body(vec3_create(MAN_SPHERE_SEP, 0, 0),
                                        MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_sphere(MAN_SPHERE_R);
    ForgePhysicsCollisionShape sb = make_sphere(MAN_SPHERE_R);
    ForgePhysicsManifold m;
    ASSERT_TRUE(forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m));
    ASSERT_TRUE(m.count == 1);
    /* local_a and local_b should differ — they are on opposite sides
     * of the contact in their respective body spaces. Since body A is
     * at origin, local_a.x should be positive (toward B). Since body B
     * is at MAN_SPHERE_SEP, local_b.x should be negative (toward A). */
    ASSERT_TRUE(m.contacts[0].local_a.x > 0.0f);
    ASSERT_TRUE(m.contacts[0].local_b.x < 0.0f);
    END_TEST();
}

static void test_MAN_prune_zero_contact_pairs(void)
{
    TEST("MAN_prune_zero_contact_pairs")
    /* Insert a manifold, then prune with an active key list that only
     * includes keys from pairs that produced manifolds. A broadphase
     * pair that did NOT produce a manifold should not keep stale entries. */
    ForgePhysicsManifoldCacheEntry *cache = NULL;

    /* Insert manifold for pair (0,1) */
    ForgePhysicsManifold m1;
    SDL_memset(&m1, 0, sizeof(m1));
    m1.body_a = 0; m1.body_b = 1;
    m1.normal = vec3_create(0, 1, 0);
    m1.count = 1;
    m1.contacts[0].penetration = 0.1f;
    m1.contacts[0].world_point = vec3_create(0, 0, 0);
    forge_physics_manifold_cache_update(&cache, &m1);

    /* Insert manifold for pair (2,3) */
    ForgePhysicsManifold m2;
    SDL_memset(&m2, 0, sizeof(m2));
    m2.body_a = 2; m2.body_b = 3;
    m2.normal = vec3_create(0, 1, 0);
    m2.count = 1;
    m2.contacts[0].penetration = 0.1f;
    m2.contacts[0].world_point = vec3_create(0, 0, 0);
    forge_physics_manifold_cache_update(&cache, &m2);

    ASSERT_TRUE(forge_hm_length(cache) == 2);

    /* Prune: only pair (0,1) produced a manifold this frame.
     * Pair (2,3) was in broadphase but had no manifold → should be pruned. */
    uint64_t active[1];
    active[0] = forge_physics_manifold_pair_key(0, 1);
    forge_physics_manifold_cache_prune(&cache, active, 1);

    ASSERT_TRUE(forge_hm_length(cache) == 1);

    /* Verify the surviving entry is pair (0,1) */
    uint64_t key01 = forge_physics_manifold_pair_key(0, 1);
    ForgePhysicsManifoldCacheEntry e = forge_hm_get_struct(cache, key01);
    ASSERT_TRUE(e.manifold.body_a == 0);
    ASSERT_TRUE(e.manifold.body_b == 1);

    forge_physics_manifold_cache_free(&cache);
    END_TEST();
}

/* ── Reduction fast-path and anchor tests (PR #354 feedback) ──────── */

static void test_MAN_reduce_small_count_writes_indices(void)
{
    TEST("MAN_reduce_small_count_writes_indices")
    /* count <= 4 should write identity indices and return count */
    vec3 points[3] = {{0,0,0}, {1,0,0}, {0,0,1}};
    float depths[3] = {0.1f, 0.2f, 0.15f};
    int indices[4] = {-1, -1, -1, -1}; /* sentinel values */
    int n = forge_physics_manifold_reduce(points, depths, 3, indices);
    ASSERT_TRUE(n == 3);
    ASSERT_TRUE(indices[0] == 0);
    ASSERT_TRUE(indices[1] == 1);
    ASSERT_TRUE(indices[2] == 2);
    /* Fourth index should be untouched */
    ASSERT_TRUE(indices[3] == -1);
    END_TEST();
}

static void test_MAN_reduce_single_point(void)
{
    TEST("MAN_reduce_single_point")
    vec3 points[1] = {{0,0,0}};
    float depths[1] = {0.1f};
    int indices[4] = {-1, -1, -1, -1};
    int n = forge_physics_manifold_reduce(points, depths, 1, indices);
    ASSERT_TRUE(n == 1);
    ASSERT_TRUE(indices[0] == 0);
    END_TEST();
}

static void test_MAN_reduce_exactly_four(void)
{
    TEST("MAN_reduce_exactly_four")
    vec3 points[4] = {{0,0,0}, {1,0,0}, {0,0,1}, {1,0,1}};
    float depths[4] = {0.1f, 0.2f, 0.15f, 0.05f};
    int indices[4] = {-1, -1, -1, -1};
    int n = forge_physics_manifold_reduce(points, depths, 4, indices);
    ASSERT_TRUE(n == 4);
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(indices[i] == i);
    }
    END_TEST();
}

static void test_MAN_reduce_zero_count(void)
{
    TEST("MAN_reduce_zero_count")
    int indices[4] = {-1, -1, -1, -1};
    int n = forge_physics_manifold_reduce(NULL, NULL, 0, indices);
    ASSERT_TRUE(n == 0);
    END_TEST();
}

static void test_MAN_reduce_null_out_indices(void)
{
    TEST("MAN_reduce_null_out_indices")
    vec3 points[2] = {{0,0,0}, {1,0,0}};
    float depths[2] = {0.1f, 0.2f};
    int n = forge_physics_manifold_reduce(points, depths, 2, NULL);
    ASSERT_TRUE(n == 0);
    END_TEST();
}

static void test_MAN_box_box_distinct_local_anchors(void)
{
    TEST("MAN_box_box_distinct_local_anchors")
    /* Axis-aligned face-face box contact: local_a and local_b should
     * differ because the reference-face anchor and incident-surface
     * anchor are at different world positions. */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0.5f, 0), MAN_BODY_MASS);
    ForgePhysicsRigidBody b = make_body(vec3_create(0, -0.3f, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);
    ForgePhysicsManifold m;
    ASSERT_TRUE(forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m));
    ASSERT_TRUE(m.count >= 1);
    for (int i = 0; i < m.count; i++) {
        /* The y-component of local_a and local_b should differ —
         * one is on the reference face, the other on the incident surface.
         * With penetration ~0.2, the gap should be measurable. */
        float dy = SDL_fabsf(m.contacts[i].local_a.y - m.contacts[i].local_b.y);
        ASSERT_TRUE(dy > 0.01f);
    }
    END_TEST();
}

static void test_MAN_box_box_world_point_midpoint(void)
{
    TEST("MAN_box_box_world_point_midpoint")
    /* For axis-aligned face-face contact, world_point should be the
     * midpoint between the reference face projection and the incident
     * surface point. With boxes at y=0.5 and y=-0.3 (half=0.5), the
     * overlap region spans y=0 to y=0.2. The reference face is at y=0
     * (bottom of A) and incident points are at y=0.2 (top of B), so
     * the midpoint y should be approximately 0.1. */
    ForgePhysicsRigidBody a = make_body(vec3_create(0, 0.5f, 0), MAN_BODY_MASS);
    ForgePhysicsRigidBody b = make_body(vec3_create(0, -0.3f, 0), MAN_BODY_MASS);
    ForgePhysicsCollisionShape sa = make_box(MAN_BOX_HALF);
    ForgePhysicsCollisionShape sb = make_box(MAN_BOX_HALF);
    ForgePhysicsManifold m;
    ASSERT_TRUE(forge_physics_gjk_epa_manifold(
        &a, &sa, &b, &sb, 0, 1, MAN_MU_S, MAN_MU_D, &m));
    ASSERT_TRUE(m.count >= 1);
    for (int i = 0; i < m.count; i++) {
        /* Y should be near the overlap midpoint (~0.1), not pinned
         * to the reference face (0.0) or incident surface (0.2). */
        ASSERT_NEAR(m.contacts[i].world_point.y, 0.1f, 0.05f);
    }
    END_TEST();
}

/* ── Runner ───────────────────────────────────────────────────────── */

void run_manifold_tests(void)
{
    SDL_Log("=== Contact Manifold Tests ===");

    /* Pair key */
    test_MAN_pair_key_symmetric();
    test_MAN_pair_key_distinct();
    test_MAN_pair_key_same_body();

    /* Polygon clipping */
    test_MAN_clip_polygon_no_clip();
    test_MAN_clip_polygon_full_clip();
    test_MAN_clip_polygon_partial();
    test_MAN_clip_null_inputs();

    /* Manifold generation */
    test_MAN_generate_sphere_sphere();
    test_MAN_generate_box_box_face();
    test_MAN_generate_box_box_edge();
    test_MAN_generate_separated();
    test_MAN_generate_null_inputs();

    /* Contact IDs */
    test_MAN_contact_ids_stable();

    /* Cache */
    test_MAN_cache_insert();
    test_MAN_cache_warmstart();
    test_MAN_cache_prune();

    /* Reduction */
    test_MAN_reduce_to_four();

    /* Conversion */
    test_MAN_to_rb_contacts();

    /* Numerical safety */
    test_MAN_cache_prune_null_keys();
    test_MAN_cache_update_overflow_count();
    test_MAN_reduce_collinear();
    test_MAN_generate_static_static();
    test_MAN_determinism();

    /* PR feedback fixes */
    test_MAN_clip_contacts_inside_ref_face();
    test_MAN_box_box_penetration_depth();
    test_MAN_epa_fallback_distinct_locals();
    test_MAN_prune_zero_contact_pairs();

    /* Reduction fast-path and anchor tests (PR #354) */
    test_MAN_reduce_small_count_writes_indices();
    test_MAN_reduce_single_point();
    test_MAN_reduce_exactly_four();
    test_MAN_reduce_zero_count();
    test_MAN_reduce_null_out_indices();
    test_MAN_box_box_distinct_local_anchors();
    test_MAN_box_box_world_point_midpoint();
}
