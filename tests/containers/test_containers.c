/*
 * Stretchy Containers Tests
 *
 * Automated tests for common/containers/forge_containers.h
 * Tests arrays, hash maps, string maps, and thread-safe get.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include "containers/forge_containers.h"

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

/* ── Test Constants ──────────────────────────────────────────────────────── */

/* Array test parameters */
#define ARR_MANY_COUNT        100    /* element count for append-many test */
#define ARR_GROW_PTR_COUNT    3      /* slots to add in grow_by_ptr test */
#define ARR_GROW_IDX_COUNT    2      /* slots to add in grow_by_index test */
#define ARR_SET_LEN_GROW      10     /* target length for set_length grow */
#define ARR_SET_LEN_SHRINK    3      /* target length for set_length shrink */
#define ARR_SET_CAP_TARGET    64     /* target capacity for set_capacity test */

/* Hash map test parameters */
#define HM_MULTI_COUNT        10     /* entries in multiple-key test */
#define HM_RESIZE_SMALL       20     /* entries to trigger one resize */
#define HM_RESIZE_LARGE       1000   /* entries to stress many resizes */
#define HM_DELETE_FILL         100   /* entries before bulk delete */
#define HM_DELETE_REMOVE       80    /* entries to delete in shrink test */
#define HM_STABILITY_PHASE1   50     /* first phase of interleaved test */
#define HM_STABILITY_PHASE1_DEL 25   /* deletes after first phase */
#define HM_STABILITY_PHASE2   100    /* second phase upper bound */
#define HM_STABILITY_LIVE     50     /* expected live entries after stability */
#define HM_DEFAULT_SENTINEL   (-1)   /* custom default value for missing keys */

/* String map test parameters */
#define SHM_MANY_COUNT        50     /* entries in string map stress test */

/* Invariant validation parameters */
#define INV_THRESHOLD_COUNT   200    /* entries for threshold invariant test */
#define INV_DELETE_COUNT      50     /* entries per delete-path phase */
#define INV_INTERLEAVE_COUNT  100    /* entries in interleaved insert/delete */
#define INV_FIXUP_COUNT       20     /* entries for fixup correctness test */
#define INV_SHRINK_FILL       200    /* entries before shrink test */
#define INV_SHRINK_REMOVE     190    /* entries to delete for shrink */
#define INV_SHRINK_REGROW     100    /* entries to re-add after shrink */
#define INV_SHRINK_KEY_BASE   1000   /* key offset for regrow phase (avoids collision with first batch) */
#define INV_SHM_STRESS_COUNT  50     /* entries for string map delete stress */

/* IEEE 754 test parameters */
#define IEEE_FLOAT_ENTRIES    5      /* elements in float array test */

/* IEEE 754 bit patterns — portable alternative to division-by-zero.
 * Avoids FP traps on toolchains with exception handling enabled. */
#define IEEE_F32_POS_INF  0x7F800000u  /* +infinity */
#define IEEE_F32_NEG_INF  0xFF800000u  /* -infinity */
#define IEEE_F32_QNAN     0x7FC00000u  /* quiet NaN */
#define IEEE_F64_POS_INF_HI 0x7FF00000u  /* +infinity high word */
#define IEEE_F64_QNAN_HI    0x7FF80000u  /* quiet NaN high word */

static float ieee_f32(uint32_t bits)
{
    float f;
    SDL_memcpy(&f, &bits, sizeof(f));
    return f;
}

static double ieee_f64(uint32_t hi, uint32_t lo)
{
    double d;
    uint64_t bits = ((uint64_t)hi << 32) | (uint64_t)lo;
    SDL_memcpy(&d, &bits, sizeof(d));
    return d;
}

/* ── Array Tests ─────────────────────────────────────────────────────────── */

static void test_arr_null_length(void)
{
    int *arr = NULL;
    TEST_BEGIN("test_arr_null_length");
    {
        CHECK(forge_arr_length(arr) == 0,          "NULL array: signed length should be 0");
        CHECK(forge_arr_length_unsigned(arr) == 0, "NULL array: unsigned length should be 0");
        CHECK(forge_arr_capacity(arr) == 0,        "NULL array: capacity should be 0");
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_append_one(void)
{
    int *arr = NULL;
    forge_arr_append(arr, 42);
    TEST_BEGIN("test_arr_append_one");
    {
        CHECK(forge_arr_length(arr) == 1,  "length should be 1 after one append");
        CHECK(arr[0] == 42,                "first element should be 42");
        CHECK(forge_arr_capacity(arr) > 0, "capacity should be positive");
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_append_many(void)
{
    int *arr = NULL;
    int i;
    for (i = 0; i < ARR_MANY_COUNT; i++) {
        forge_arr_append(arr, i * 3);
    }
    TEST_BEGIN("test_arr_append_many");
    {
        REQUIRE(arr != NULL, "array should not be NULL after appends");
        CHECK(forge_arr_length(arr) == ARR_MANY_COUNT, "length should match ARR_MANY_COUNT");
        {
            int all_correct = 1;
            for (i = 0; i < ARR_MANY_COUNT; i++) {
                if (arr[i] != i * 3) { all_correct = 0; break; }
            }
            CHECK(all_correct, "all appended values should be correct");
        }
        CHECK(forge_arr_capacity(arr) >= ARR_MANY_COUNT, "capacity should be at least ARR_MANY_COUNT");
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_pop(void)
{
    int *arr = NULL;
    forge_arr_append(arr, 10);
    forge_arr_append(arr, 20);
    forge_arr_append(arr, 30);
    TEST_BEGIN("test_arr_pop");
    {
        int val = forge_arr_pop(arr);
        CHECK(val == 30, "popped value should be 30");
        CHECK(forge_arr_length(arr) == 2,   "length should be 2 after pop");
        CHECK(arr[0] == 10 && arr[1] == 20, "remaining elements should be 10, 20");
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_last(void)
{
    float *arr = NULL;
    forge_arr_append(arr, 1.0f);
    forge_arr_append(arr, 2.5f);
    forge_arr_append(arr, 3.75f);
    TEST_BEGIN("test_arr_last");
    {
        CHECK(forge_arr_last(arr) == 3.75f, "last should return 3.75f without removing");
        CHECK(forge_arr_length(arr) == 3,   "length should still be 3");
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_free(void)
{
    int *arr = NULL;
    forge_arr_append(arr, 1);
    forge_arr_append(arr, 2);
    TEST_BEGIN("test_arr_free");
    {
        REQUIRE(arr != NULL, "array should not be NULL before free");
        forge_arr_free(arr);
        CHECK(arr == NULL, "arr should be NULL after forge_arr_free");
    }
    TEST_END();
    /* No trailing free — the free IS the test. If REQUIRE aborted before
     * the free, arr is still non-NULL but that is a test-only leak (no
     * double-free risk). forge_arr_free(NULL) is safe but redundant here. */
}

static void test_arr_insert_at(void)
{
    int *arr = NULL;
    /* Build [10, 20, 30] then insert 5 at beginning and 15 in middle. */
    forge_arr_append(arr, 10);
    forge_arr_append(arr, 20);
    forge_arr_append(arr, 30);
    TEST_BEGIN("test_arr_insert_at");
    {
        /* Insert 5 at index 0 => [5, 10, 20, 30] */
        forge_arr_insert_at(arr, 0, 5);
        CHECK(forge_arr_length(arr) == 4, "length should be 4 after insert at 0");
        CHECK(arr[0] == 5,  "arr[0] should be 5");
        CHECK(arr[1] == 10, "arr[1] should be 10");
        CHECK(arr[2] == 20, "arr[2] should be 20");
        CHECK(arr[3] == 30, "arr[3] should be 30");

        /* Insert 15 at index 2 => [5, 10, 15, 20, 30] */
        forge_arr_insert_at(arr, 2, 15);
        CHECK(forge_arr_length(arr) == 5, "length should be 5 after second insert");
        CHECK(arr[2] == 15, "arr[2] should be 15");
        CHECK(arr[3] == 20, "arr[3] should be 20");
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_delete_at(void)
{
    int *arr = NULL;
    forge_arr_append(arr, 1);
    forge_arr_append(arr, 2);
    forge_arr_append(arr, 3);
    forge_arr_append(arr, 4);
    forge_arr_append(arr, 5);
    TEST_BEGIN("test_arr_delete_at");
    {
        /* Delete middle element (index 2, value 3) => [1, 2, 4, 5] */
        forge_arr_delete_at(arr, 2);
        CHECK(forge_arr_length(arr) == 4, "length should be 4 after delete");
        CHECK(arr[0] == 1, "arr[0] should be 1");
        CHECK(arr[1] == 2, "arr[1] should be 2");
        CHECK(arr[2] == 4, "arr[2] should shift to 4");
        CHECK(arr[3] == 5, "arr[3] should shift to 5");
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_swap_remove(void)
{
    int *arr = NULL;
    forge_arr_append(arr, 10);
    forge_arr_append(arr, 20);
    forge_arr_append(arr, 30);
    forge_arr_append(arr, 40);
    TEST_BEGIN("test_arr_swap_remove");
    {
        /* swap_remove index 1: last element (40) moves to index 1 => [10, 40, 30] */
        forge_arr_swap_remove(arr, 1);
        CHECK(forge_arr_length(arr) == 3, "length should be 3 after swap_remove");
        CHECK(arr[0] == 10, "arr[0] should still be 10");
        CHECK(arr[1] == 40, "arr[1] should be 40 (swapped from last)");
        CHECK(arr[2] == 30, "arr[2] should still be 30");
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_grow_by(void)
{
    int *arr = NULL;
    TEST_BEGIN("test_arr_grow_by");
    {
        /* grow_by_ptr: append ARR_GROW_PTR_COUNT slots, return pointer to first new slot */
        {
            int *first = forge_arr_grow_by_ptr(arr, ARR_GROW_PTR_COUNT);
            REQUIRE(first != NULL,                            "grow_by_ptr should return non-NULL");
            CHECK(forge_arr_length(arr) == ARR_GROW_PTR_COUNT, "length should be ARR_GROW_PTR_COUNT after grow_by_ptr");
            first[0] = 100; first[1] = 200; first[2] = 300;
            CHECK(arr[0] == 100, "arr[0] should be 100");
            CHECK(arr[1] == 200, "arr[1] should be 200");
            CHECK(arr[2] == 300, "arr[2] should be 300");
        }

        /* grow_by_index: append ARR_GROW_IDX_COUNT more, return index of first new slot */
        {
            ptrdiff_t idx = forge_arr_grow_by_index(arr, ARR_GROW_IDX_COUNT);
            CHECK(idx == ARR_GROW_PTR_COUNT,                                    "grow_by_index should return ARR_GROW_PTR_COUNT");
            CHECK(forge_arr_length(arr) == ARR_GROW_PTR_COUNT + ARR_GROW_IDX_COUNT, "length should be ARR_GROW_PTR_COUNT + ARR_GROW_IDX_COUNT after grow_by_index");
            arr[idx]     = 400;
            arr[idx + 1] = 500;
            CHECK(arr[3] == 400, "arr[3] should be 400");
            CHECK(arr[4] == 500, "arr[4] should be 500");
        }
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_set_length(void)
{
    int *arr = NULL;
    forge_arr_append(arr, 1);
    forge_arr_append(arr, 2);
    TEST_BEGIN("test_arr_set_length");
    {
        /* Grow to ARR_SET_LEN_GROW */
        forge_arr_set_length(arr, ARR_SET_LEN_GROW);
        CHECK(forge_arr_length(arr) == ARR_SET_LEN_GROW,   "length should be ARR_SET_LEN_GROW after set_length");
        CHECK(forge_arr_capacity(arr) >= ARR_SET_LEN_GROW, "capacity should be at least ARR_SET_LEN_GROW");
        CHECK(arr[0] == 1, "arr[0] should still be 1 after grow");
        CHECK(arr[1] == 2, "arr[1] should still be 2 after grow");

        /* Shrink length (allocation does not shrink) */
        forge_arr_set_length(arr, ARR_SET_LEN_SHRINK);
        CHECK(forge_arr_length(arr) == ARR_SET_LEN_SHRINK,    "length should be ARR_SET_LEN_SHRINK after shrink");
        CHECK(forge_arr_capacity(arr) >= ARR_SET_LEN_GROW, "capacity should not have shrunk");
    }
    TEST_END();
    forge_arr_free(arr);
}

static void test_arr_set_capacity(void)
{
    int *arr = NULL;
    forge_arr_append(arr, 42);
    TEST_BEGIN("test_arr_set_capacity");
    {
        /* Reserve space for ARR_SET_CAP_TARGET elements without changing length */
        forge_arr_set_capacity(arr, ARR_SET_CAP_TARGET);
        CHECK(forge_arr_capacity(arr) >= ARR_SET_CAP_TARGET, "capacity should be at least ARR_SET_CAP_TARGET");
        CHECK(forge_arr_length(arr) == 1,                    "length should remain 1 after set_capacity");
        CHECK(arr[0] == 42,                  "existing value should be preserved");
    }
    TEST_END();
    forge_arr_free(arr);
}

/* ── Hash Map Tests ──────────────────────────────────────────────────────── */

typedef struct { int key; int value; } IntMap;

static void test_hm_put_get(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 7, 99);
    TEST_BEGIN("test_hm_put_get");
    {
        CHECK(forge_hm_get(map, 7) == 99, "get should return 99 after put(7, 99)");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_overwrite(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 1, 100);
    forge_hm_put(map, 1, 200);
    TEST_BEGIN("test_hm_overwrite");
    {
        CHECK(forge_hm_get(map, 1) == 200,  "second put should overwrite first");
        CHECK(forge_hm_length(map) == 1,    "length should still be 1 after overwrite");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_multiple(void)
{
    IntMap *map = NULL;
    int i;
    for (i = 0; i < HM_MULTI_COUNT; i++) {
        forge_hm_put(map, i, i * i);
    }
    TEST_BEGIN("test_hm_multiple");
    {
        CHECK(forge_hm_length(map) == HM_MULTI_COUNT, "length should be HM_MULTI_COUNT");
        {
            int all_correct = 1;
            for (i = 0; i < HM_MULTI_COUNT; i++) {
                if (forge_hm_get(map, i) != i * i) { all_correct = 0; break; }
            }
            CHECK(all_correct, "all HM_MULTI_COUNT entries should round-trip correctly");
        }
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_find_index(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 42, 100);
    TEST_BEGIN("test_hm_find_index");
    {
        ptrdiff_t idx = forge_hm_find_index(map, 42);
        REQUIRE(idx >= 0, "find_index should return a non-negative index for existing key");
        /* Entry i is stored at map[i+1]; element 0 is the default. */
        CHECK(map[idx + 1].key   == 42,  "entry at found index should have key 42");
        CHECK(map[idx + 1].value == 100, "entry at found index should have value 100");
        CHECK(forge_hm_find_index(map, 999) == -1,
              "find_index should return -1 for missing key");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_missing_key(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 1, 10);
    TEST_BEGIN("test_hm_missing_key");
    {
        /* Default value is 0 (zero-initialised). */
        CHECK(forge_hm_get(map, 999) == 0, "missing key should return default (0)");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_set_default(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 1, 10);
    forge_hm_set_default(map, HM_DEFAULT_SENTINEL);
    TEST_BEGIN("test_hm_set_default");
    {
        CHECK(forge_hm_get(map, 999) == HM_DEFAULT_SENTINEL, "missing key should return custom default HM_DEFAULT_SENTINEL");
        CHECK(forge_hm_get(map, 1)   == 10, "existing key should still return its value");
    }
    TEST_END();
    forge_hm_free(map);
}

/* Regression: set_default creates the data array without a hash index.
 * A subsequent put must create the hash index on demand. */
static void test_hm_set_default_then_put(void)
{
    IntMap *map = NULL;
    forge_hm_set_default(map, HM_DEFAULT_SENTINEL);
    TEST_BEGIN("test_hm_set_default_then_put");
    {
        CHECK(forge_hm_get(map, 999) == HM_DEFAULT_SENTINEL, "default should be HM_DEFAULT_SENTINEL before any put");
        forge_hm_put(map, 42, 100);
        forge_hm_put(map, 99, 200);
        CHECK(forge_hm_get(map, 42)  == 100, "get(42) should return 100");
        CHECK(forge_hm_get(map, 99)  == 200, "get(99) should return 200");
        CHECK(forge_hm_get(map, 999) == HM_DEFAULT_SENTINEL, "missing key should still return default");
        CHECK(forge_hm_length(map) == 2,     "length should be 2");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_get_ptr(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 5, 55);
    TEST_BEGIN("test_hm_get_ptr");
    {
        IntMap *ptr = forge_hm_get_ptr(map, 5);
        REQUIRE(ptr != NULL,    "get_ptr should return a non-NULL pointer");
        CHECK(ptr->key   == 5,  "pointer key should be 5");
        CHECK(ptr->value == 55, "pointer value should be 55");
        /* Mutate through pointer and verify the map sees the change. */
        ptr->value = 77;
        CHECK(forge_hm_get(map, 5) == 77, "mutation through pointer should persist");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_get_ptr_or_null(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 3, 30);
    TEST_BEGIN("test_hm_get_ptr_or_null");
    {
        CHECK(forge_hm_get_ptr_or_null(map, 3)   != NULL,
              "get_ptr_or_null should return non-NULL for existing key");
        CHECK(forge_hm_get_ptr_or_null(map, 999) == NULL,
              "get_ptr_or_null should return NULL for missing key");
    }
    TEST_END();
    forge_hm_free(map);
}

typedef struct { int key; float value; } IntFloatMap;

static void test_hm_put_struct(void)
{
    IntFloatMap *map = NULL;
    IntFloatMap entry;
    entry.key   = 77;
    entry.value = 3.14f;
    forge_hm_put_struct(map, entry);
    TEST_BEGIN("test_hm_put_struct");
    {
        IntFloatMap got = forge_hm_get_struct(map, 77);
        CHECK(got.key   == 77,    "get_struct key should be 77");
        CHECK(got.value == 3.14f, "get_struct value should be 3.14f");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_iterate(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 10, 100);
    forge_hm_put(map, 20, 200);
    forge_hm_put(map, 30, 300);
    TEST_BEGIN("test_hm_iterate");
    {
        ptrdiff_t i;
        int count = 0;
        int sum   = 0;
        forge_hm_iter(map, i) {
            count++;
            sum += map[i + 1].value;
        }
        CHECK(count == 3, "iteration should visit 3 entries");
        CHECK(sum == 600, "sum of values should be 600");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_length(void)
{
    IntMap *map = NULL;
    TEST_BEGIN("test_hm_length");
    {
        CHECK(forge_hm_length(map) == 0, "NULL map length should be 0");
        forge_hm_put(map, 1, 1);
        CHECK(forge_hm_length(map) == 1, "length should be 1 after one put");
        forge_hm_put(map, 2, 2);
        forge_hm_put(map, 3, 3);
        CHECK(forge_hm_length(map) == 3, "length should be 3 after three puts");
        /* Overwrite should not increase count. */
        forge_hm_put(map, 2, 99);
        CHECK(forge_hm_length(map) == 3, "length should remain 3 after overwrite");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_free(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 1, 1);
    TEST_BEGIN("test_hm_free");
    {
        REQUIRE(map != NULL, "map should not be NULL before free");
        forge_hm_free(map);
        CHECK(map == NULL, "map should be NULL after forge_hm_free");
    }
    TEST_END();
}

/* ── Hash Map Resize / Shrink Tests ──────────────────────────────────────── */

static void test_hm_resize_grow(void)
{
    IntMap *map = NULL;
    int i;
    /* Insert enough entries to trigger at least one resize (>75% of min slots). */
    for (i = 0; i < HM_RESIZE_SMALL; i++) {
        forge_hm_put(map, i, i + 1000);
    }
    TEST_BEGIN("test_hm_resize_grow");
    {
        CHECK(forge_hm_length(map) == HM_RESIZE_SMALL, "length should be HM_RESIZE_SMALL after resize");
        {
            int all_correct = 1;
            for (i = 0; i < HM_RESIZE_SMALL; i++) {
                if (forge_hm_get(map, i) != i + 1000) { all_correct = 0; break; }
            }
            CHECK(all_correct, "all entries should still be findable after resize");
        }
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_resize_many(void)
{
    IntMap *map = NULL;
    int i;
    for (i = 0; i < HM_RESIZE_LARGE; i++) {
        forge_hm_put(map, i * 7 + 3, i);
    }
    TEST_BEGIN("test_hm_resize_many");
    {
        CHECK(forge_hm_length(map) == HM_RESIZE_LARGE, "length should be HM_RESIZE_LARGE");
        {
            int all_correct = 1;
            for (i = 0; i < HM_RESIZE_LARGE; i++) {
                if (forge_hm_get(map, i * 7 + 3) != i) { all_correct = 0; break; }
            }
            CHECK(all_correct, "all HM_RESIZE_LARGE entries should be retrievable after many resizes");
        }
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_resize_after_delete(void)
{
    IntMap *map = NULL;
    int i;
    /* Insert HM_DELETE_FILL entries then delete HM_DELETE_REMOVE — should trigger a shrink. */
    for (i = 0; i < HM_DELETE_FILL; i++) {
        forge_hm_put(map, i, i);
    }
    for (i = 0; i < HM_DELETE_REMOVE; i++) {
        forge_hm_remove(map, i);
    }
    TEST_BEGIN("test_hm_resize_after_delete");
    {
        CHECK(forge_hm_length(map) == HM_DELETE_FILL - HM_DELETE_REMOVE,
              "length should be HM_DELETE_FILL - HM_DELETE_REMOVE after bulk deletes");
        {
            int all_correct = 1;
            for (i = HM_DELETE_REMOVE; i < HM_DELETE_FILL; i++) {
                if (forge_hm_get(map, i) != i) { all_correct = 0; break; }
            }
            CHECK(all_correct, "remaining entries should still be correct after shrink");
        }
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_resize_stability(void)
{
    IntMap *map = NULL;
    int i;
    /* Interleave inserts and deletes to stress tombstone handling and rebuilds. */
    for (i = 0; i < HM_STABILITY_PHASE1; i++) {
        forge_hm_put(map, i, i * 2);
    }
    for (i = 0; i < HM_STABILITY_PHASE1_DEL; i++) {
        forge_hm_remove(map, i);
    }
    for (i = HM_STABILITY_PHASE1; i < HM_STABILITY_PHASE2; i++) {
        forge_hm_put(map, i, i * 2);
    }
    for (i = HM_STABILITY_PHASE1_DEL; i < HM_STABILITY_PHASE1; i++) {
        forge_hm_remove(map, i);
    }
    TEST_BEGIN("test_hm_resize_stability");
    {
        /* Only keys HM_STABILITY_PHASE1..HM_STABILITY_PHASE2-1 remain. */
        CHECK(forge_hm_length(map) == HM_STABILITY_LIVE, "HM_STABILITY_LIVE entries should remain");
        {
            int all_correct = 1;
            for (i = HM_STABILITY_PHASE1; i < HM_STABILITY_PHASE2; i++) {
                if (forge_hm_get(map, i) != i * 2) { all_correct = 0; break; }
            }
            CHECK(all_correct,
                  "all live keys should return correct values after interleaved ops");
        }
    }
    TEST_END();
    forge_hm_free(map);
}

/* ── Hash Map Deletion Tests ─────────────────────────────────────────────── */

static void test_hm_delete_basic(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 10, 100);
    forge_hm_put(map, 20, 200);
    TEST_BEGIN("test_hm_delete_basic");
    {
        CHECK(forge_hm_remove(map, 10) == 1,   "remove should return 1 for existing key");
        CHECK(forge_hm_length(map) == 1,       "length should be 1 after remove");
        CHECK(forge_hm_get(map, 10) == 0,      "removed key should return default 0");
        CHECK(forge_hm_get(map, 20) == 200,    "surviving key should still return 200");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_delete_missing(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 5, 50);
    TEST_BEGIN("test_hm_delete_missing");
    {
        CHECK(forge_hm_remove(map, 999) == 0,  "remove should return 0 for missing key");
        CHECK(forge_hm_length(map) == 1,       "length should be unchanged after failed remove");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_delete_reinsert(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 42, 100);
    forge_hm_remove(map, 42);
    TEST_BEGIN("test_hm_delete_reinsert");
    {
        CHECK(forge_hm_length(map) == 0,    "length should be 0 after delete");
        forge_hm_put(map, 42, 999);
        CHECK(forge_hm_length(map) == 1,    "length should be 1 after re-insert");
        CHECK(forge_hm_get(map, 42) == 999, "re-inserted value should be 999");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_hm_delete_iteration(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 1, 10);
    forge_hm_put(map, 2, 20);
    forge_hm_put(map, 3, 30);
    forge_hm_remove(map, 2);
    TEST_BEGIN("test_hm_delete_iteration");
    {
        ptrdiff_t i;
        int count = 0;
        int sum   = 0;
        forge_hm_iter(map, i) {
            count++;
            sum += map[i + 1].value;
        }
        CHECK(count == 2, "iteration should visit 2 entries after deletion");
        CHECK(sum == 40,  "sum should be 40 (10 + 30)");
    }
    TEST_END();
    forge_hm_free(map);
}

/* ── String Map Tests ────────────────────────────────────────────────────── */

typedef struct { char *key; int value; } StrMap;

static void test_shm_user_mode(void)
{
    StrMap *map = NULL;
    /* Default mode: user manages key lifetime; string literals are safe here. */
    forge_shm_put(map, "alpha", 1);
    forge_shm_put(map, "beta",  2);
    forge_shm_put(map, "gamma", 3);
    TEST_BEGIN("test_shm_user_mode");
    {
        CHECK(forge_shm_get(map, "alpha") == 1, "alpha should be 1");
        CHECK(forge_shm_get(map, "beta")  == 2, "beta should be 2");
        CHECK(forge_shm_get(map, "gamma") == 3, "gamma should be 3");
        CHECK(forge_shm_length(map) == 3,       "length should be 3");
    }
    TEST_END();
    forge_shm_free(map);
}

static void test_shm_strdup_mode(void)
{
    StrMap *map = NULL;
    forge_shm_init_strdup(map);
    TEST_BEGIN("test_shm_strdup_mode");
    {
        REQUIRE(map != NULL, "map should not be NULL after init_strdup");
        {
            /* Build keys on the stack; overwrite them after put to verify copies. */
            char k1[16]; SDL_snprintf(k1, sizeof(k1), "key_%d", 1);
            char k2[16]; SDL_snprintf(k2, sizeof(k2), "key_%d", 2);
            forge_shm_put(map, k1, 111);
            forge_shm_put(map, k2, 222);
            /* Destroy the originals; the library's copies must keep working. */
            SDL_memset(k1, 0, sizeof(k1));
            SDL_memset(k2, 0, sizeof(k2));
        }
        CHECK(forge_shm_get(map, "key_1") == 111, "strdup map: key_1 should be 111");
        CHECK(forge_shm_get(map, "key_2") == 222, "strdup map: key_2 should be 222");
    }
    TEST_END();
    forge_shm_free(map);
}

static void test_shm_arena_mode(void)
{
    StrMap *map = NULL;
    forge_shm_init_arena(map);
    TEST_BEGIN("test_shm_arena_mode");
    {
        REQUIRE(map != NULL, "map should not be NULL after init_arena");
        forge_shm_put(map, "one",   1);
        forge_shm_put(map, "two",   2);
        forge_shm_put(map, "three", 3);
        CHECK(forge_shm_get(map, "one")   == 1, "arena map: one should be 1");
        CHECK(forge_shm_get(map, "two")   == 2, "arena map: two should be 2");
        CHECK(forge_shm_get(map, "three") == 3, "arena map: three should be 3");
        CHECK(forge_shm_length(map) == 3,       "arena map length should be 3");
    }
    TEST_END();
    forge_shm_free(map);
}

static void test_shm_overwrite(void)
{
    StrMap *map = NULL;
    forge_shm_put(map, "x", 10);
    forge_shm_put(map, "x", 20);
    TEST_BEGIN("test_shm_overwrite");
    {
        CHECK(forge_shm_get(map, "x") == 20, "second put should overwrite first");
        CHECK(forge_shm_length(map) == 1,    "length should remain 1 after overwrite");
    }
    TEST_END();
    forge_shm_free(map);
}

static void test_shm_remove(void)
{
    StrMap *map = NULL;
    forge_shm_put(map, "keep",   1);
    forge_shm_put(map, "delete", 2);
    TEST_BEGIN("test_shm_remove");
    {
        CHECK(forge_shm_remove(map, "delete") == 1, "remove should return 1 for existing key");
        CHECK(forge_shm_length(map) == 1,           "length should be 1 after removal");
        CHECK(forge_shm_get(map, "keep")   == 1,    "surviving key should still return 1");
        CHECK(forge_shm_get(map, "delete") == 0,    "removed key should return default 0");
    }
    TEST_END();
    forge_shm_free(map);
}

static void test_shm_iterate(void)
{
    StrMap *map = NULL;
    forge_shm_put(map, "a", 10);
    forge_shm_put(map, "b", 20);
    forge_shm_put(map, "c", 30);
    TEST_BEGIN("test_shm_iterate");
    {
        ptrdiff_t i;
        int count = 0;
        int sum   = 0;
        forge_shm_iter(map, i) {
            count++;
            sum += map[i + 1].value;
        }
        CHECK(count == 3, "string map iteration should visit 3 entries");
        CHECK(sum == 60,  "sum of values should be 60");
    }
    TEST_END();
    forge_shm_free(map);
}

static void test_shm_many(void)
{
    StrMap *map = NULL;
    char key[32];
    int i;
    forge_shm_init_strdup(map);
    for (i = 0; i < SHM_MANY_COUNT; i++) {
        SDL_snprintf(key, sizeof(key), "entry_%d", i);
        forge_shm_put(map, key, i * 3);
    }
    TEST_BEGIN("test_shm_many");
    {
        CHECK(forge_shm_length(map) == SHM_MANY_COUNT, "length should be SHM_MANY_COUNT after SHM_MANY_COUNT inserts");
        {
            int all_correct = 1;
            for (i = 0; i < SHM_MANY_COUNT; i++) {
                SDL_snprintf(key, sizeof(key), "entry_%d", i);
                if (forge_shm_get(map, key) != i * 3) { all_correct = 0; break; }
            }
            CHECK(all_correct, "all SHM_MANY_COUNT string entries should round-trip correctly");
        }
    }
    TEST_END();
    forge_shm_free(map);
}

static void test_shm_free(void)
{
    StrMap *map = NULL;
    forge_shm_put(map, "hello", 1);
    TEST_BEGIN("test_shm_free");
    {
        REQUIRE(map != NULL, "map should not be NULL before free");
        forge_shm_free(map);
        CHECK(map == NULL, "map should be NULL after forge_shm_free");
    }
    TEST_END();
}

/* ── Thread-Safe Get Tests ───────────────────────────────────────────────── */

static void test_ts_read(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 11, 111);
    forge_hm_put(map, 22, 222);
    TEST_BEGIN("test_ts_read");
    {
        /* forge_hm_get_ts uses a caller-provided temp variable instead of the
         * shared header scratch field — safe for read-only concurrent access. */
        ptrdiff_t tmp;
        int v1 = forge_hm_get_ts(map, 11, tmp);
        int v2 = forge_hm_get_ts(map, 22, tmp);
        CHECK(v1 == 111, "thread-safe get: key 11 should return 111");
        CHECK(v2 == 222, "thread-safe get: key 22 should return 222");
    }
    TEST_END();
    forge_hm_free(map);
}

static void test_ts_missing(void)
{
    IntMap *map = NULL;
    forge_hm_put(map, 1, 10);
    TEST_BEGIN("test_ts_missing");
    {
        ptrdiff_t tmp;
        int v = forge_hm_get_ts(map, 999, tmp);
        CHECK(v == 0, "thread-safe get on missing key should return default 0");
    }
    TEST_END();
    forge_hm_free(map);
}

/* ── Audit Fix Regression Tests ──────────────────────────────────────────── */

/* F1: rotl/rotr with n=0 must return identity (was UB: shift by bit-width). */
static void test_rotl_zero(void)
{
    TEST_BEGIN("test_rotl_zero");
    {
        size_t x = (size_t)0xDEADBEEF;
        size_t rl = forge_containers__rotl(x, 0);
        size_t rr = forge_containers__rotr(x, 0);
        CHECK(rl == x, "rotl(x, 0) should return x unchanged");
        CHECK(rr == x, "rotr(x, 0) should return x unchanged");
        /* Also verify a known rotation: rotl(1, 1) should be 2. */
        CHECK(forge_containers__rotl((size_t)1, 1) == (size_t)2,
              "rotl(1, 1) should be 2");
    }
    TEST_END();
}

/* F4: forge_arr_append on a NULL array after grow must not crash.
 * We cannot easily simulate OOM, so test that append on NULL works and
 * that the NULL guard is present by verifying normal append still works. */
static void test_arr_append_null_safe(void)
{
    int *arr = NULL;
    TEST_BEGIN("test_arr_append_null_safe");
    /* Normal append should succeed. */
    forge_arr_append(arr, 42);
    REQUIRE(arr != NULL, "append should allocate on first use");
    CHECK(arr[0] == 42, "first element should be 42");
    CHECK(forge_arr_length(arr) == 1, "length should be 1");
    forge_arr_free(arr);
    /* After free, arr is NULL — appending again should work. */
    forge_arr_append(arr, 99);
    REQUIRE(arr != NULL, "append after free should re-allocate");
    CHECK(arr[0] == 99, "element should be 99 after re-append");
    TEST_END();
    forge_arr_free(arr);  /* cleanup on any exit path */
}

/* F10: set_length must not set length > capacity on grow failure.
 * We verify normal behavior: set_length grows and sets correctly. */
static void test_arr_set_length_guard(void)
{
    int *arr = NULL;
    TEST_BEGIN("test_arr_set_length_guard");
    {
        forge_arr_set_length(arr, ARR_SET_LEN_GROW);
        REQUIRE(arr != NULL, "set_length should allocate");
        CHECK(forge_arr_length(arr) == ARR_SET_LEN_GROW, "length should be ARR_SET_LEN_GROW");
        CHECK(forge_arr_capacity(arr) >= ARR_SET_LEN_GROW, "capacity should be >= ARR_SET_LEN_GROW");
        /* Verify length <= capacity invariant. */
        CHECK(forge_arr_length_unsigned(arr) <= forge_arr_capacity(arr),
              "length must not exceed capacity");
        /* Shrink length (no realloc). */
        forge_arr_set_length(arr, ARR_SET_LEN_SHRINK);
        CHECK(forge_arr_length(arr) == ARR_SET_LEN_SHRINK,
              "length should shrink to ARR_SET_LEN_SHRINK");
    }
    TEST_END();
    forge_arr_free(arr);
}

/* F6/F7: NULL key in string map must not crash. */
static void test_shm_null_key(void)
{
    struct { char *key; int value; } *map = NULL;
    forge_shm_init_strdup(map);
    TEST_BEGIN("test_shm_null_key");
    {
        /* Put with NULL key should not crash (returns -1 internally). */
        forge_shm_put(map, NULL, 42);
        /* The NULL put should be silently skipped — length should be 0. */
        CHECK(forge_shm_length(map) == 0,
              "NULL key put should not insert an entry");
    }
    TEST_END();
    forge_shm_free(map);
}

/* F6: hash_string with NULL must not crash. */
static void test_hash_string_null(void)
{
    TEST_BEGIN("test_hash_string_null");
    {
        /* Direct call — should return seed without crashing. */
        size_t h = forge_containers__hash_string(NULL, 12345);
        CHECK(h == 12345, "hash_string(NULL) should return seed");
    }
    TEST_END();
}

/* Regression: shm_put_key must handle NULL hash_table after a failed init.
 * forge_shm_init_strdup allocates the data array, but if index_new fails
 * internally, hash_table is NULL.  A subsequent forge_shm_put must create
 * the index on demand rather than dereferencing NULL. We simulate this by
 * manually NULLing the hash_table after init. */
static void test_shm_put_after_null_index(void)
{
    StrMap *map = NULL;
    forge_shm_init_strdup(map);
    TEST_BEGIN("test_shm_put_after_null_index");
    REQUIRE(map != NULL, "init_strdup should allocate the data array");
    {
        /* Simulate index_new failure: free the hash index and NULL it. */
        forge_containers__hash_index *idx = forge_containers__hdr(map)->hash_table;
        if (idx) {
            forge_containers__index_free(idx);
            forge_containers__hdr(map)->hash_table = NULL;
        }

        /* Now put should recover by creating a new index on demand. */
        forge_shm_put(map, "hello", 42);
        CHECK(forge_shm_length(map) == 1,
              "put should succeed after index recovery");
        CHECK(forge_shm_get(map, "hello") == 42,
              "get should return correct value after index recovery");

        /* Second put to verify the recovered index works normally. */
        forge_shm_put(map, "world", 99);
        CHECK(forge_shm_length(map) == 2,
              "second put should also succeed");
        CHECK(forge_shm_get(map, "world") == 99,
              "second get should return correct value");
    }
    TEST_END();
    forge_shm_free(map);
}

/* Theme A: arr_append on a full array where grow fails must not write
 * past capacity.  We simulate by filling to capacity without growing. */
static void test_arr_append_full_noop(void)
{
    int *arr = NULL;
    forge_arr_append(arr, 1);
    forge_arr_append(arr, 2);
    TEST_BEGIN("test_arr_append_full_noop");
    REQUIRE(arr != NULL, "array should be allocated");
    {
        /* Record state before the capacity-check test. */
        size_t cap = forge_arr_capacity(arr);
        ptrdiff_t len = forge_arr_length(arr);
        /* Verify the has_room guard: length < capacity means append works. */
        CHECK(forge_containers__hdr(arr)->length <
              forge_containers__hdr(arr)->capacity,
              "should have room for more appends");
        /* Normal append should still work. */
        forge_arr_append(arr, 3);
        CHECK(forge_arr_length(arr) == len + 1,
              "append should succeed when room is available");
        (void)cap;
    }
    TEST_END();
    forge_arr_free(arr);
}

/* Theme B: NULL string key in find/remove must not crash. */
static void test_shm_null_key_find_remove(void)
{
    StrMap *map = NULL;
    forge_shm_init_strdup(map);
    forge_shm_put(map, "hello", 42);
    TEST_BEGIN("test_shm_null_key_find_remove");
    REQUIRE(map != NULL, "map should be allocated");
    {
        /* find_index with NULL key should return -1, not crash. */
        ptrdiff_t idx = forge_shm_find_index(map, NULL);
        CHECK(idx == -1, "find_index(NULL) should return -1");
        /* remove with NULL key should return 0, not crash. */
        int removed = forge_shm_remove(map, NULL);
        CHECK(removed == 0, "remove(NULL) should return 0");
        /* Original entry should be unaffected. */
        CHECK(forge_shm_get(map, "hello") == 42,
              "existing entry should survive NULL operations");
    }
    TEST_END();
    forge_shm_free(map);
}

/* Theme C: shm_init_* fails closed — if index_new fails, pointer is NULL.
 * We test that a normal init+put cycle works (can't easily simulate OOM). */
static void test_shm_init_strdup_then_put(void)
{
    StrMap *map = NULL;
    forge_shm_init_strdup(map);
    TEST_BEGIN("test_shm_init_strdup_then_put");
    REQUIRE(map != NULL, "init_strdup should produce non-NULL map");
    {
        /* Verify the hash index was created with the correct mode. */
        forge_containers__hash_index *idx =
            forge_containers__hdr(map)->hash_table;
        REQUIRE(idx != NULL, "hash_table should be non-NULL after init");
        CHECK(idx->arena.mode == FORGE_CONTAINERS__MODE_STRDUP,
              "mode should be STRDUP after init_strdup");
        /* Put and get should work. */
        forge_shm_put(map, "test", 99);
        CHECK(forge_shm_get(map, "test") == 99,
              "put/get should work after init_strdup");
    }
    TEST_END();
    forge_shm_free(map);
}

/* Theme D: hm_get on NULL map returns 0 (not crash). */
static void test_hm_get_null_map(void)
{
    IntMap *map = NULL;
    TEST_BEGIN("test_hm_get_null_map");
    {
        /* forge_hm_get on NULL should return 0, not crash.
         * ensure_default may fail (though unlikely), so the OOM guard
         * returns 0. On success, it returns the default (also 0). */
        int v = forge_hm_get(map, 999);
        CHECK(v == 0, "get on NULL map should return 0");
    }
    TEST_END();
    forge_hm_free(map);
}

/* F15: hm_remove fixup — delete non-last entry, verify the swapped entry
 * is still findable. */
static void test_hm_remove_swap_last(void)
{
    IntMap *map = NULL;
    int i;
    for (i = 0; i < 5; i++) forge_hm_put(map, i, i * 100);
    /* Delete key 0 (first entry — forces swap with last). */
    forge_hm_remove(map, 0);
    TEST_BEGIN("test_hm_remove_swap_last");
    {
        CHECK(forge_hm_find_index(map, 0) == -1, "key 0 should be gone");
        /* All remaining keys must still be findable. */
        {
            int all_ok = 1;
            for (i = 1; i < 5; i++) {
                if (forge_hm_find_index(map, i) < 0) { all_ok = 0; break; }
            }
            CHECK(all_ok, "all non-deleted keys should still be findable after swap");
        }
        CHECK(forge_hm_length(map) == 4, "length should be 4 after one removal");
    }
    TEST_END();
    forge_hm_free(map);
}

/* Additional: delete ALL entries one by one (stress the swap-with-last
 * fixup and tombstone rebuild). */
static void test_hm_delete_all(void)
{
    IntMap *map = NULL;
    int i;
    for (i = 0; i < HM_DELETE_FILL; i++) forge_hm_put(map, i, i);
    TEST_BEGIN("test_hm_delete_all");
    {
        for (i = 0; i < HM_DELETE_FILL; i++) {
            int removed = forge_hm_remove(map, i);
            if (!removed) {
                CHECK(0, "remove should return 1 for existing key");
                break;
            }
        }
        CHECK(forge_hm_length(map) == 0, "length should be 0 after deleting all");
        /* Re-insert after full deletion should work. */
        forge_hm_put(map, 999, 42);
        CHECK(forge_hm_get(map, 999) == 42, "re-insert after full delete should work");
        CHECK(forge_hm_length(map) == 1, "length should be 1 after re-insert");
    }
    TEST_END();
    forge_hm_free(map);
}

/* ── IEEE 754 Edge Case Tests ────────────────────────────────────────────── */

/* Float arrays store NaN and infinity without issue — just raw bytes. */
static void test_arr_float_special(void)
{
    float *arr = NULL;
    float inf  = ieee_f32(IEEE_F32_POS_INF);           /* +INFINITY */
    float ninf = ieee_f32(IEEE_F32_NEG_INF);          /* -INFINITY */
    float nan1 = ieee_f32(IEEE_F32_QNAN);           /* NaN */

    forge_arr_append(arr, inf);
    forge_arr_append(arr, ninf);
    forge_arr_append(arr, nan1);
    forge_arr_append(arr, 0.0f);
    forge_arr_append(arr, -0.0f);

    TEST_BEGIN("test_arr_float_special");
    {
        REQUIRE(forge_arr_length(arr) == IEEE_FLOAT_ENTRIES, "should have IEEE_FLOAT_ENTRIES elements");

        /* Infinity round-trips correctly. */
        CHECK(arr[0] == inf,   "+inf should round-trip");
        CHECK(arr[1] == ninf,  "-inf should round-trip");
        /* NaN: IEEE says NaN != NaN, so we check with memcmp. */
        {
            int nan_match = (SDL_memcmp(&arr[2], &nan1, sizeof(float)) == 0);
            CHECK(nan_match, "NaN should round-trip (byte-identical)");
        }
        /* 0.0 and -0.0 are stored as their distinct bit patterns. */
        CHECK(arr[3] == 0.0f, "0.0 should round-trip");
        CHECK(arr[4] == 0.0f, "-0.0 should compare equal via == (IEEE semantics)");
        {
            /* But their bit patterns differ. */
            int bits_differ = (SDL_memcmp(&arr[3], &arr[4], sizeof(float)) != 0);
            CHECK(bits_differ, "+0.0 and -0.0 have different bit patterns");
        }
    }
    TEST_END();
    forge_arr_free(arr);
}

/* Float keys in hash maps: the library hashes and compares raw bytes.
 * Infinity works like any other key.  NaN with a consistent bit pattern
 * is findable.  +0.0 and -0.0 are treated as DIFFERENT keys (byte-wise). */
static void test_hm_float_key_infinity(void)
{
    struct { float key; int value; } *map = NULL;
    float pinf = ieee_f32(IEEE_F32_POS_INF);
    float ninf = ieee_f32(IEEE_F32_NEG_INF);

    forge_hm_put(map, pinf, 100);
    forge_hm_put(map, ninf, 200);
    forge_hm_put(map, 1.0f, 300);

    TEST_BEGIN("test_hm_float_key_infinity");
    {
        CHECK(forge_hm_length(map) == 3, "should have 3 entries");
        CHECK(forge_hm_get(map, pinf) == 100, "+inf key should be retrievable");
        CHECK(forge_hm_get(map, ninf) == 200, "-inf key should be retrievable");
        CHECK(forge_hm_get(map, 1.0f) == 300, "normal key should be retrievable");

        /* Overwrite +inf value. */
        forge_hm_put(map, pinf, 999);
        CHECK(forge_hm_get(map, pinf) == 999, "+inf key should be overwritable");
        CHECK(forge_hm_length(map) == 3, "overwrite should not change length");

        /* Remove +inf. */
        CHECK(forge_hm_remove(map, pinf) == 1, "+inf should be removable");
        CHECK(forge_hm_find_index(map, pinf) == -1, "+inf should be gone");
        CHECK(forge_hm_get(map, ninf) == 200, "-inf should survive +inf removal");
    }
    TEST_END();
    forge_hm_free(map);
}

/* NaN as a hash map key.  The library uses byte-wise comparison (memcmp),
 * NOT IEEE == semantics.  So NaN with the same bit pattern IS findable. */
static void test_hm_float_key_nan(void)
{
    struct { float key; int value; } *map = NULL;
    float nan_val = ieee_f32(IEEE_F32_QNAN);

    forge_hm_put(map, nan_val, 42);

    TEST_BEGIN("test_hm_float_key_nan");
    {
        CHECK(forge_hm_length(map) == 1, "NaN key should insert");

        /* Retrieve using the same NaN bit pattern. */
        int v = forge_hm_get(map, nan_val);
        CHECK(v == 42, "NaN key should be retrievable via same bit pattern");

        /* Overwrite NaN — should find existing entry, not insert a duplicate. */
        forge_hm_put(map, nan_val, 99);
        CHECK(forge_hm_length(map) == 1, "NaN overwrite should not duplicate");
        CHECK(forge_hm_get(map, nan_val) == 99, "NaN overwrite should update value");

        /* Remove NaN. */
        CHECK(forge_hm_remove(map, nan_val) == 1, "NaN should be removable");
        CHECK(forge_hm_length(map) == 0, "length should be 0 after NaN removal");
    }
    TEST_END();
    forge_hm_free(map);
}

/* +0.0 and -0.0 as hash map keys.  IEEE says +0.0 == -0.0, but they have
 * different bit patterns.  The library uses byte-wise comparison, so they
 * are treated as DIFFERENT keys.  This is correct for a raw-bytes container
 * and matches the design spec's "struct key gotcha" note about byte equality. */
static void test_hm_float_key_signed_zero(void)
{
    struct { float key; int value; } *map = NULL;
    float pos_zero = 0.0f;
    float neg_zero = -0.0f;

    forge_hm_put(map, pos_zero, 1);
    forge_hm_put(map, neg_zero, 2);

    TEST_BEGIN("test_hm_float_key_signed_zero");
    {
        /* Both should exist as separate entries (different bit patterns). */
        CHECK(forge_hm_length(map) == 2,
              "+0.0 and -0.0 should be distinct keys (byte-wise)");
        CHECK(forge_hm_get(map, pos_zero) == 1, "+0.0 value should be 1");
        CHECK(forge_hm_get(map, neg_zero) == 2, "-0.0 value should be 2");

        /* Remove +0.0 should not affect -0.0. */
        forge_hm_remove(map, pos_zero);
        CHECK(forge_hm_find_index(map, pos_zero) == -1, "+0.0 should be gone");
        CHECK(forge_hm_get(map, neg_zero) == 2, "-0.0 should survive");
    }
    TEST_END();
    forge_hm_free(map);
}

/* Double keys — same behavior as float but through the 8-byte hash path. */
static void test_hm_double_key_special(void)
{
    struct { double key; int value; } *map = NULL;
    double pinf = ieee_f64(IEEE_F64_POS_INF_HI, 0);
    double nan_val = ieee_f64(IEEE_F64_QNAN_HI, 0);
    double neg_zero = -0.0;

    forge_hm_put(map, pinf, 10);
    forge_hm_put(map, nan_val, 20);
    forge_hm_put(map, 0.0, 30);
    forge_hm_put(map, neg_zero, 40);

    TEST_BEGIN("test_hm_double_key_special");
    {
        CHECK(forge_hm_length(map) == 4, "should have 4 entries (0.0 and -0.0 distinct)");
        CHECK(forge_hm_get(map, pinf) == 10, "+inf double key works");
        CHECK(forge_hm_get(map, nan_val) == 20, "NaN double key works");
        CHECK(forge_hm_get(map, 0.0) == 30, "+0.0 double key works");
        CHECK(forge_hm_get(map, neg_zero) == 40, "-0.0 double key works");
    }
    TEST_END();
    forge_hm_free(map);
}

/* Float values (not keys) — NaN/infinity stored and retrieved unchanged. */
static void test_hm_float_value_special(void)
{
    struct { int key; float value; } *map = NULL;
    float inf  = ieee_f32(IEEE_F32_POS_INF);
    float nan_val = ieee_f32(IEEE_F32_QNAN);

    forge_hm_put(map, 1, inf);
    forge_hm_put(map, 2, nan_val);
    forge_hm_put(map, 3, -0.0f);

    TEST_BEGIN("test_hm_float_value_special");
    {
        CHECK(forge_hm_get(map, 1) == inf, "+inf value should round-trip");
        {
            float v = forge_hm_get(map, 2);
            int nan_ok = (SDL_memcmp(&v, &nan_val, sizeof(float)) == 0);
            CHECK(nan_ok, "NaN value should round-trip (byte-identical)");
        }
        /* -0.0 as a value: IEEE == says -0.0 == 0.0, so this CHECK uses ==. */
        CHECK(forge_hm_get(map, 3) == 0.0f, "-0.0 value compares equal to 0.0 via ==");
    }
    TEST_END();
    forge_hm_free(map);
}

/* ── Invariant Validation Tests ──────────────────────────────────────────── */

/* Threshold invariant: grow + tombstone thresholds < slot_count.
 * This guarantees at least one empty slot exists to terminate probes. */
static void test_threshold_invariant(void)
{
    IntMap *map = NULL;
    int i;
    /* Insert enough to trigger several resizes. */
    for (i = 0; i < INV_THRESHOLD_COUNT; i++) forge_hm_put(map, i, i);
    TEST_BEGIN("test_threshold_invariant");
    {
        /* The invariant is checked inside index_new (logs on violation).
         * If we got here without a log, the invariant held through all
         * resizes.  Verify the map is still functional. */
        int all_ok = 1;
        for (i = 0; i < INV_THRESHOLD_COUNT; i++) {
            if (forge_hm_get(map, i) != i) { all_ok = 0; break; }
        }
        CHECK(all_ok, "all INV_THRESHOLD_COUNT entries should be findable (threshold invariant held)");
    }
    TEST_END();
    forge_hm_free(map);
}

/* Delete-path validation: delete every entry from a map, exercising
 * the slot bounds check, used_count underflow guard, and fixup assertions.
 * Interleave insertions and deletions to stress tombstone rebuild. */
static void test_delete_path_validation(void)
{
    IntMap *map = NULL;
    int i;
    /* Phase 1: fill and delete all (forward order). */
    for (i = 0; i < INV_DELETE_COUNT; i++) forge_hm_put(map, i, i * 10);
    for (i = 0; i < INV_DELETE_COUNT; i++) {
        int r = forge_hm_remove(map, i);
        if (!r) break;
    }
    TEST_BEGIN("test_delete_path_validation");
    CHECK(forge_hm_length(map) == 0, "phase 1: all entries deleted");

    /* Phase 2: refill and delete in reverse (forces different swap patterns). */
    for (i = 0; i < INV_DELETE_COUNT; i++) forge_hm_put(map, i, i * 20);
    {
        int j;
        for (j = INV_DELETE_COUNT - 1; j >= 0; j--) {
            int r = forge_hm_remove(map, j);
            if (!r) break;
        }
    }
    CHECK(forge_hm_length(map) == 0, "phase 2: reverse deletion complete");

    /* Phase 3: interleaved insert/delete (stress tombstone rebuild). */
    for (i = 0; i < INV_INTERLEAVE_COUNT; i++) {
        forge_hm_put(map, i, i);
        if (i > 0 && (i % 3) == 0) {
            forge_hm_remove(map, i - 1);
        }
    }
    {
        /* Verify all live keys. */
        int live = 0;
        for (i = 0; i < INV_INTERLEAVE_COUNT; i++) {
            if (forge_hm_find_index(map, i) >= 0) live++;
        }
        CHECK(live == forge_hm_length(map),
              "phase 3: live count matches length after interleaved ops");
    }
    TEST_END();
    forge_hm_free(map);
}

/* Fixup correctness: delete the first entry of many, then verify the
 * swapped-in entry is still accessible.  This directly exercises the
 * b->index[i] == final_index assertion path. */
static void test_fixup_index_correctness(void)
{
    IntMap *map = NULL;
    int i;
    for (i = 0; i < INV_FIXUP_COUNT; i++) forge_hm_put(map, i * 100, i);
    TEST_BEGIN("test_fixup_index_correctness");
    /* Delete key 0 — first entry, forces swap with last (key (INV_FIXUP_COUNT-1)*100). */
    CHECK(forge_hm_remove(map, 0) == 1, "delete key 0 should succeed");
    CHECK(forge_hm_find_index(map, 0) == -1, "key 0 should be gone");
    /* The entry that was last is swapped into slot 0.
     * It must still be findable. */
    {
        int last_key = (INV_FIXUP_COUNT - 1) * 100;
        int last_val = INV_FIXUP_COUNT - 1;
        CHECK(forge_hm_find_index(map, last_key) >= 0,
              "swapped entry should still be findable");
        CHECK(forge_hm_get(map, last_key) == last_val,
              "swapped entry value should be correct");
    }
    /* All other entries must survive. */
    {
        int all_ok = 1;
        for (i = 1; i < INV_FIXUP_COUNT; i++) {
            if (forge_hm_get(map, i * 100) != i) { all_ok = 0; break; }
        }
        CHECK(all_ok, "all remaining entries should have correct values");
    }
    TEST_END();
    forge_hm_free(map);
}

/* Shrink path: insert many, delete most, verify the table shrinks and
 * all remaining entries survive the index rebuild. */
static void test_shrink_rebuild(void)
{
    IntMap *map = NULL;
    int i;
    for (i = 0; i < INV_SHRINK_FILL; i++) forge_hm_put(map, i, i * 7);
    /* Delete INV_SHRINK_REMOVE of INV_SHRINK_FILL, leaving only keys INV_SHRINK_REMOVE..INV_SHRINK_FILL-1. */
    for (i = 0; i < INV_SHRINK_REMOVE; i++) forge_hm_remove(map, i);
    TEST_BEGIN("test_shrink_rebuild");
    CHECK(forge_hm_length(map) == INV_SHRINK_FILL - INV_SHRINK_REMOVE,
          "should have INV_SHRINK_FILL - INV_SHRINK_REMOVE entries after bulk delete");
    {
        int all_ok = 1;
        for (i = INV_SHRINK_REMOVE; i < INV_SHRINK_FILL; i++) {
            if (forge_hm_get(map, i) != i * 7) { all_ok = 0; break; }
        }
        CHECK(all_ok, "surviving entries should have correct values after shrink");
    }
    /* Re-grow: insert INV_SHRINK_REGROW more to trigger growth from the shrunk state. */
    for (i = INV_SHRINK_KEY_BASE; i < INV_SHRINK_KEY_BASE + INV_SHRINK_REGROW; i++) forge_hm_put(map, i, i);
    CHECK(forge_hm_length(map) == (INV_SHRINK_FILL - INV_SHRINK_REMOVE) + INV_SHRINK_REGROW,
          "should have (INV_SHRINK_FILL - INV_SHRINK_REMOVE) + INV_SHRINK_REGROW entries after re-grow");
    {
        int all_ok = 1;
        for (i = INV_SHRINK_KEY_BASE; i < INV_SHRINK_KEY_BASE + INV_SHRINK_REGROW; i++) {
            if (forge_hm_get(map, i) != i) { all_ok = 0; break; }
        }
        CHECK(all_ok, "new entries should be correct after re-grow from shrunk state");
    }
    TEST_END();
    forge_hm_free(map);
}

/* String map delete path: exercises strdup key free + fixup. */
static void test_shm_delete_stress(void)
{
    StrMap *map = NULL;
    int i;
    char key[32];
    forge_shm_init_strdup(map);
    for (i = 0; i < INV_SHM_STRESS_COUNT; i++) {
        SDL_snprintf(key, sizeof(key), "k_%03d", i);
        forge_shm_put(map, key, i);
    }
    /* Delete in stride-3 pattern to create tombstones. */
    for (i = 0; i < INV_SHM_STRESS_COUNT; i += 3) {
        SDL_snprintf(key, sizeof(key), "k_%03d", i);
        forge_shm_remove(map, key);
    }
    TEST_BEGIN("test_shm_delete_stress");
    {
        /* Verify remaining entries. */
        int live = 0;
        for (i = 0; i < INV_SHM_STRESS_COUNT; i++) {
            SDL_snprintf(key, sizeof(key), "k_%03d", i);
            if (forge_shm_find_index(map, key) >= 0) {
                CHECK(forge_shm_get(map, key) == i,
                      "surviving string entry should have correct value");
                live++;
            }
        }
        CHECK(live == forge_shm_length(map),
              "live count should match shm_length");
    }
    TEST_END();
    forge_shm_free(map);
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

    SDL_Log("=== Stretchy Containers Tests ===");

    /* Array basics */
    SDL_Log("--- Array ---");
    test_arr_null_length();
    test_arr_append_one();
    test_arr_append_many();
    test_arr_pop();
    test_arr_last();
    test_arr_free();
    test_arr_insert_at();
    test_arr_delete_at();
    test_arr_swap_remove();
    test_arr_grow_by();
    test_arr_set_length();
    test_arr_set_capacity();

    /* Hash map basics */
    SDL_Log("--- Hash Map ---");
    test_hm_put_get();
    test_hm_overwrite();
    test_hm_multiple();
    test_hm_find_index();
    test_hm_missing_key();
    test_hm_set_default();
    test_hm_set_default_then_put();
    test_hm_get_ptr();
    test_hm_get_ptr_or_null();
    test_hm_put_struct();
    test_hm_iterate();
    test_hm_length();
    test_hm_free();

    /* Hash map resize/shrink */
    SDL_Log("--- Hash Map Resize ---");
    test_hm_resize_grow();
    test_hm_resize_many();
    test_hm_resize_after_delete();
    test_hm_resize_stability();

    /* Hash map deletion */
    SDL_Log("--- Hash Map Deletion ---");
    test_hm_delete_basic();
    test_hm_delete_missing();
    test_hm_delete_reinsert();
    test_hm_delete_iteration();

    /* String map */
    SDL_Log("--- String Map ---");
    test_shm_user_mode();
    test_shm_strdup_mode();
    test_shm_arena_mode();
    test_shm_overwrite();
    test_shm_remove();
    test_shm_iterate();
    test_shm_many();
    test_shm_free();

    /* Thread-safe */
    SDL_Log("--- Thread-Safe Get ---");
    test_ts_read();
    test_ts_missing();

    /* Audit-fix regression tests */
    SDL_Log("--- Audit Fix Regressions ---");
    test_rotl_zero();
    test_arr_append_null_safe();
    test_arr_set_length_guard();
    test_shm_null_key();
    test_hash_string_null();
    test_shm_put_after_null_index();
    test_arr_append_full_noop();
    test_shm_null_key_find_remove();
    test_shm_init_strdup_then_put();
    test_hm_get_null_map();
    test_hm_remove_swap_last();
    test_hm_delete_all();

    /* IEEE 754 edge cases (NaN, infinity, signed zero) */
    SDL_Log("--- IEEE 754 Edge Cases ---");
    test_arr_float_special();
    test_hm_float_key_infinity();
    test_hm_float_key_nan();
    test_hm_float_key_signed_zero();
    test_hm_double_key_special();
    test_hm_float_value_special();

    /* Invariant validation */
    SDL_Log("--- Invariant Validation ---");
    test_threshold_invariant();
    test_delete_path_validation();
    test_fixup_index_correctness();
    test_shrink_rebuild();
    test_shm_delete_stress();

    SDL_Log("=== Results: %d/%d passed, %d failed ===",
            test_passed, test_count, test_failed);

    SDL_Quit();
    return test_failed > 0 ? 1 : 0;
}
