/*
 * Arena Allocator Tests
 *
 * Automated tests for common/arena/forge_arena.h
 * Tests creation, allocation, alignment, growth, reset, and destruction.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include "arena/forge_arena.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

static int test_count  = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_BEGIN(name)                                                     \
    do {                                                                     \
        test_count++;                                                        \
        const char *test_name_ = (name);                                     \
        int local_fail_ = 0

#define CHECK(cond, msg)                                                     \
        if (!(cond)) {                                                       \
            SDL_Log("  FAIL [%s]: %s", test_name_, (msg));                   \
            local_fail_ = 1;                                                 \
        }

/* REQUIRE is like CHECK but aborts the current test on failure.
 * Use for preconditions where continuing would dereference NULL. */
#define REQUIRE(cond, msg)                                                   \
        if (!(cond)) {                                                       \
            SDL_Log("  FAIL [%s]: %s (aborting test)", test_name_, (msg));   \
            local_fail_ = 1;                                                 \
            break;  /* exits the do { ... } while(0) in TEST_BEGIN */        \
        }

#define TEST_END()                                                           \
        if (local_fail_) { test_failed++; }                                  \
        else { test_passed++; SDL_Log("  PASS [%s]", test_name_); }          \
    } while (0)

/* ── Tests ───────────────────────────────────────────────────────────────── */

static void test_create_default(void)
{
    TEST_BEGIN("create_default");
    ForgeArena arena = forge_arena_create(0);
    CHECK(arena.first != NULL, "first block should be allocated");
    CHECK(arena.current != NULL, "current block should be set");
    CHECK(arena.default_block_size == FORGE_ARENA_DEFAULT_BLOCK_SIZE,
          "default block size should match constant");
    CHECK(forge_arena_capacity(&arena) >= FORGE_ARENA_DEFAULT_BLOCK_SIZE,
          "capacity should be at least default block size");
    CHECK(forge_arena_used(&arena) == 0, "used should start at 0");
    forge_arena_destroy(&arena);
    CHECK(arena.first == NULL, "first should be NULL after destroy");
    CHECK(arena.current == NULL, "current should be NULL after destroy");
    TEST_END();
}

static void test_create_custom_size(void)
{
    TEST_BEGIN("create_custom_size");
    ForgeArena arena = forge_arena_create(1024);
    CHECK(arena.default_block_size == 1024,
          "should use custom block size");
    CHECK(forge_arena_capacity(&arena) >= 1024,
          "capacity should be at least 1024");
    forge_arena_destroy(&arena);
    TEST_END();
}

static void test_basic_alloc(void)
{
    ForgeArena arena = forge_arena_create(4096);
    TEST_BEGIN("basic_alloc");

    int *a = (int *)forge_arena_alloc(&arena, sizeof(int));
    REQUIRE(a != NULL, "allocation should succeed");
    CHECK(*a == 0, "memory should be zero-initialized");
    *a = 42;
    CHECK(*a == 42, "write/read should work");

    float *b = (float *)forge_arena_alloc(&arena, sizeof(float));
    REQUIRE(b != NULL, "second allocation should succeed");
    CHECK(*b == 0.0f, "second allocation zero-initialized");

    CHECK(forge_arena_used(&arena) > 0, "used should increase after alloc");

    TEST_END();
    forge_arena_destroy(&arena);
}

static void test_array_alloc(void)
{
    ForgeArena arena = forge_arena_create(4096);
    TEST_BEGIN("array_alloc");

    int count = 100;
    int *arr = (int *)forge_arena_alloc(&arena, (size_t)count * sizeof(int));
    REQUIRE(arr != NULL, "array allocation should succeed");

    /* Verify zero-init and write/read */
    int all_zero = 1;
    for (int i = 0; i < count; i++) {
        if (arr[i] != 0) { all_zero = 0; break; }
    }
    CHECK(all_zero, "array should be zero-initialized");

    for (int i = 0; i < count; i++) arr[i] = i * i;
    CHECK(arr[50] == 2500, "array write/read should work");

    TEST_END();
    forge_arena_destroy(&arena);
}

static void test_alignment(void)
{
    ForgeArena arena = forge_arena_create(4096);
    TEST_BEGIN("alignment");

    /* Default alignment is 8 bytes (covers int, float, double, pointers) */
    void *p1 = forge_arena_alloc(&arena, 1);
    REQUIRE(p1 != NULL, "default alloc should succeed");
    CHECK(((uintptr_t)p1 % FORGE_ARENA_DEFAULT_ALIGN) == 0,
          "default alloc should be aligned to FORGE_ARENA_DEFAULT_ALIGN");

    /* Explicit 16-byte alignment */
    void *p2 = forge_arena_alloc_aligned(&arena, 32, 16);
    REQUIRE(p2 != NULL, "aligned alloc should succeed");
    CHECK(((uintptr_t)p2 % 16) == 0,
          "16-byte aligned alloc should be 16-byte aligned");

    /* Explicit 64-byte alignment */
    void *p3 = forge_arena_alloc_aligned(&arena, 64, 64);
    REQUIRE(p3 != NULL, "64-aligned alloc should succeed");
    CHECK(((uintptr_t)p3 % 64) == 0,
          "64-byte aligned alloc should be 64-byte aligned");

    TEST_END();
    forge_arena_destroy(&arena);
}

static void test_growth(void)
{
    /* Use a small block size to force growth */
    ForgeArena arena = forge_arena_create(256);
    TEST_BEGIN("growth");

    /* Allocate more than one block's worth */
    void *a = forge_arena_alloc(&arena, 200);
    REQUIRE(a != NULL, "first alloc should succeed");

    void *b = forge_arena_alloc(&arena, 200);
    REQUIRE(b != NULL, "second alloc should succeed (triggers new block)");

    /* Verify first allocation is still valid */
    int *ia = (int *)a;
    *ia = 12345;
    CHECK(*ia == 12345, "first alloc data should survive growth");

    CHECK(forge_arena_capacity(&arena) > 256,
          "capacity should have grown beyond initial block");

    TEST_END();
    forge_arena_destroy(&arena);
}

static void test_large_single_alloc(void)
{
    ForgeArena arena = forge_arena_create(256);
    TEST_BEGIN("large_single_alloc");

    /* Allocate more than the default block size in one call */
    size_t big_size = 1024;
    char *big = (char *)forge_arena_alloc(&arena, big_size);
    REQUIRE(big != NULL, "large allocation should succeed");

    /* Write pattern to verify it's usable */
    for (size_t i = 0; i < big_size; i++) big[i] = (char)(i & 0xFF);
    CHECK(big[500] == (char)(500 & 0xFF), "large alloc data should be correct");

    TEST_END();
    forge_arena_destroy(&arena);
}

static void test_reset(void)
{
    TEST_BEGIN("reset");
    ForgeArena arena = forge_arena_create(4096);

    /* Fill pre-reset allocations with non-zero pattern */
    void *a1 = forge_arena_alloc(&arena, 100);
    void *a2 = forge_arena_alloc(&arena, 200);
    CHECK(a1 != NULL, "first alloc should succeed");
    CHECK(a2 != NULL, "second alloc should succeed");
    if (a1) SDL_memset(a1, 0xFF, 100);
    if (a2) SDL_memset(a2, 0xFF, 200);

    size_t used_before = forge_arena_used(&arena);
    CHECK(used_before > 0, "should have used memory before reset");

    size_t capacity_before = forge_arena_capacity(&arena);

    forge_arena_reset(&arena);
    CHECK(forge_arena_used(&arena) == 0, "used should be 0 after reset");
    CHECK(forge_arena_capacity(&arena) == capacity_before,
          "capacity should not change after reset");

    /* Allocations after reset should return zeroed memory */
    void *p = forge_arena_alloc(&arena, 50);
    CHECK(p != NULL, "alloc after reset should succeed");
    if (p) {
        const unsigned char *bytes = (const unsigned char *)p;
        int all_zero = 1;
        size_t i;
        for (i = 0; i < 50; i++) {
            if (bytes[i] != 0) { all_zero = 0; break; }
        }
        CHECK(all_zero, "memory after reset should be zeroed");
    }

    forge_arena_destroy(&arena);
    TEST_END();
}

static void test_zero_size_alloc(void)
{
    TEST_BEGIN("zero_size_alloc");
    ForgeArena arena = forge_arena_create(4096);

    void *p = forge_arena_alloc(&arena, 0);
    CHECK(p == NULL, "zero-size alloc should return NULL");

    forge_arena_destroy(&arena);
    TEST_END();
}

static void test_null_arena(void)
{
    TEST_BEGIN("null_arena");
    void *p = forge_arena_alloc(NULL, 100);
    CHECK(p == NULL, "alloc with NULL arena should return NULL");

    /* These should not crash */
    forge_arena_destroy(NULL);
    forge_arena_reset(NULL);
    CHECK(forge_arena_used(NULL) == 0, "used of NULL should be 0");
    CHECK(forge_arena_capacity(NULL) == 0, "capacity of NULL should be 0");
    TEST_END();
}

static void test_many_small_allocs(void)
{
    TEST_BEGIN("many_small_allocs");
    ForgeArena arena = forge_arena_create(1024);

    /* Many small allocations — stress the bump allocator */
    int success = 1;
    for (int i = 0; i < 500; i++) {
        int *p = (int *)forge_arena_alloc(&arena, sizeof(int));
        if (!p) { success = 0; break; }
        *p = i;
    }
    CHECK(success, "500 small allocs should all succeed");

    forge_arena_destroy(&arena);
    TEST_END();
}

static void test_struct_alloc(void)
{
    typedef struct {
        float x, y, z;
        int   id;
        char  name[32];
    } TestVertex;

    ForgeArena arena = forge_arena_create(4096);
    TEST_BEGIN("struct_alloc");

    int count = 10;
    TestVertex *verts = (TestVertex *)forge_arena_alloc(
        &arena, (size_t)count * sizeof(TestVertex));
    REQUIRE(verts != NULL, "struct array alloc should succeed");

    for (int i = 0; i < count; i++) {
        verts[i].x = (float)i;
        verts[i].y = (float)(i * 2);
        verts[i].z = (float)(i * 3);
        verts[i].id = i;
        SDL_snprintf(verts[i].name, sizeof(verts[i].name), "vert_%d", i);
    }

    CHECK(verts[5].id == 5, "struct data should be correct");
    CHECK(verts[5].x == 5.0f, "struct float data should be correct");

    TEST_END();
    forge_arena_destroy(&arena);
}

static void test_multiple_arenas(void)
{
    ForgeArena arena_a = forge_arena_create(1024);
    ForgeArena arena_b = forge_arena_create(2048);
    TEST_BEGIN("multiple_arenas");

    int *a = (int *)forge_arena_alloc(&arena_a, sizeof(int));
    int *b = (int *)forge_arena_alloc(&arena_b, sizeof(int));
    REQUIRE(a != NULL && b != NULL, "both allocs should succeed");

    *a = 111;
    *b = 222;

    /* Destroy one, the other should still work */
    forge_arena_destroy(&arena_a);
    CHECK(*b == 222, "arena_b data should survive arena_a destroy");

    TEST_END();
    /* Safe to call on an already-destroyed arena (fields are zeroed). */
    forge_arena_destroy(&arena_a);
    forge_arena_destroy(&arena_b);
}

static void test_reset_then_grow(void)
{
    TEST_BEGIN("reset_then_grow");
    ForgeArena arena = forge_arena_create(256);

    /* Fill, grow, reset, fill again */
    for (int i = 0; i < 5; i++) {
        forge_arena_alloc(&arena, 200);
    }
    size_t cap_after_grow = forge_arena_capacity(&arena);

    forge_arena_reset(&arena);

    /* Should reuse existing blocks */
    for (int i = 0; i < 5; i++) {
        void *p = forge_arena_alloc(&arena, 200);
        CHECK(p != NULL, "alloc after reset should succeed");
    }

    CHECK(forge_arena_capacity(&arena) == cap_after_grow,
          "capacity should be exactly reused after reset");

    forge_arena_destroy(&arena);
    TEST_END();
}

static void test_invalid_alignment(void)
{
    TEST_BEGIN("invalid_alignment");
    ForgeArena arena = forge_arena_create(4096);

    /* align = 0 should be rejected */
    void *p1 = forge_arena_alloc_aligned(&arena, 16, 0);
    CHECK(p1 == NULL, "align=0 should return NULL");

    /* Non-power-of-2 alignment should be rejected */
    void *p2 = forge_arena_alloc_aligned(&arena, 16, 3);
    CHECK(p2 == NULL, "align=3 should return NULL");

    void *p3 = forge_arena_alloc_aligned(&arena, 16, 7);
    CHECK(p3 == NULL, "align=7 should return NULL");

    /* Valid power-of-2 should still work after rejections */
    void *p4 = forge_arena_alloc_aligned(&arena, 16, 16);
    CHECK(p4 != NULL, "align=16 should succeed after rejections");

    forge_arena_destroy(&arena);
    TEST_END();
}

static void test_huge_alloc_rejected(void)
{
    TEST_BEGIN("huge_alloc_rejected");
    ForgeArena arena = forge_arena_create(4096);

    /* SIZE_MAX should fail (cannot allocate that much memory) */
    void *p = forge_arena_alloc(&arena, SIZE_MAX);
    CHECK(p == NULL, "SIZE_MAX alloc should return NULL");

    /* Arena should still be usable after the failed request */
    int *ok = (int *)forge_arena_alloc(&arena, sizeof(int));
    CHECK(ok != NULL, "normal alloc should succeed after huge failure");

    forge_arena_destroy(&arena);
    TEST_END();
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== Arena Allocator Tests ===");

    test_create_default();
    test_create_custom_size();
    test_basic_alloc();
    test_array_alloc();
    test_alignment();
    test_growth();
    test_large_single_alloc();
    test_reset();
    test_zero_size_alloc();
    test_null_arena();
    test_many_small_allocs();
    test_struct_alloc();
    test_multiple_arenas();
    test_reset_then_grow();
    test_invalid_alignment();
    test_huge_alloc_rejected();

    SDL_Log("=== Results: %d/%d passed, %d failed ===",
            test_passed, test_count, test_failed);

    SDL_Quit();
    return test_failed > 0 ? 1 : 0;
}
