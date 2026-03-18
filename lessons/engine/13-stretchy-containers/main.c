/*
 * Engine Lesson 13 -- Stretchy Containers
 *
 * Demonstrates: Fat-pointer dynamic arrays and hash maps -- the per-element
 * allocation pattern complementing arenas (batch-lifetime allocation).
 *
 * This program shows six container patterns:
 *   1. Dynamic arrays     -- append, index, pop, length
 *   2. Array operations   -- insert_at, delete_at, swap_remove
 *   3. Hash map basics    -- put, get, iterate, remove
 *   4. Default values     -- set_default, get on missing key
 *   5. String hash maps   -- strdup mode, put, get, iterate
 *   6. Comparison         -- array vs hash map access patterns
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stddef.h>
#include "containers/forge_containers.h"

/* ── Demo 1: Dynamic Arrays ─────────────────────────────────────────────── */

static void demo_dynamic_arrays(void)
{
    SDL_Log("--- Demo 1: Dynamic Arrays ---");

    int *nums = NULL;
    SDL_Log("  Start: nums is NULL, length = %td", forge_arr_length(nums));

    forge_arr_append(nums, 10);
    forge_arr_append(nums, 20);
    forge_arr_append(nums, 30);
    forge_arr_append(nums, 40);
    forge_arr_append(nums, 50);

    SDL_Log("  After 5 appends: length = %td, capacity = %zu",
            forge_arr_length(nums), forge_arr_capacity(nums));

    SDL_Log("  Contents: [%d, %d, %d, %d, %d]",
            nums[0], nums[1], nums[2], nums[3], nums[4]);

    int last = forge_arr_last(nums);
    SDL_Log("  Last element: %d", last);

    int popped = forge_arr_pop(nums);
    SDL_Log("  Popped: %d, length now = %td", popped, forge_arr_length(nums));

    forge_arr_free(nums);
    SDL_Log("  After free: nums is %s, length = %td",
            nums ? "valid" : "NULL", forge_arr_length(nums));
    SDL_Log("");
}

/* ── Demo 2: Array Operations ────────────────────────────────────────────── */

static void print_int_array(const char *label, const int *arr)
{
    ptrdiff_t len = forge_arr_length(arr);
    ptrdiff_t i;
    SDL_Log("  %s (len=%td): ", label, len);
    for (i = 0; i < len; i++) {
        SDL_Log("    [%td] = %d", i, arr[i]);
    }
}

static void demo_array_operations(void)
{
    SDL_Log("--- Demo 2: Array Operations ---");

    int *arr = NULL;
    int i;
    for (i = 0; i < 5; i++) forge_arr_append(arr, (i + 1) * 10);

    print_int_array("Initial", arr);

    forge_arr_insert_at(arr, 2, 999);
    print_int_array("After insert_at(2, 999)", arr);

    forge_arr_delete_at(arr, 2);
    print_int_array("After delete_at(2)", arr);

    forge_arr_swap_remove(arr, 1);
    print_int_array("After swap_remove(1)", arr);

    forge_arr_free(arr);
    SDL_Log("");
}

/* ── Demo 3: Hash Map Basics ─────────────────────────────────────────────── */

static void demo_hash_map_basics(void)
{
    SDL_Log("--- Demo 3: Hash Map Basics ---");

    struct { int key; float value; } *scores = NULL;

    forge_hm_put(scores, 1, 95.5f);
    forge_hm_put(scores, 2, 87.0f);
    forge_hm_put(scores, 3, 92.3f);
    forge_hm_put(scores, 4, 78.8f);

    SDL_Log("  Inserted 4 entries, length = %td", forge_hm_length(scores));

    float s1 = forge_hm_get(scores, 1);
    float s3 = forge_hm_get(scores, 3);
    SDL_Log("  get(1) = %.1f, get(3) = %.1f", s1, s3);

    SDL_Log("  Iterating all entries:");
    {
        ptrdiff_t idx;
        forge_hm_iter(scores, idx) {
            SDL_Log("    key=%d  value=%.1f",
                    scores[idx + 1].key, scores[idx + 1].value);
        }
    }

    int removed = forge_hm_remove(scores, 2);
    SDL_Log("  remove(2) returned %d, length = %td",
            removed, forge_hm_length(scores));

    ptrdiff_t idx = forge_hm_find_index(scores, 2);
    SDL_Log("  find_index(2) = %td (deleted)", idx);

    forge_hm_free(scores);
    SDL_Log("");
}

/* ── Demo 4: Hash Map Default Values ─────────────────────────────────────── */

static void demo_default_values(void)
{
    SDL_Log("--- Demo 4: Hash Map Default Values ---");

    struct { int key; int value; } *lookup = NULL;

    forge_hm_set_default(lookup, -1);
    SDL_Log("  Set default value to -1");

    forge_hm_put(lookup, 100, 42);
    forge_hm_put(lookup, 200, 84);

    int v100 = forge_hm_get(lookup, 100);
    int v200 = forge_hm_get(lookup, 200);
    int v999 = forge_hm_get(lookup, 999);

    SDL_Log("  get(100) = %d", v100);
    SDL_Log("  get(200) = %d", v200);
    SDL_Log("  get(999) = %d  (missing key returns default)", v999);

    forge_hm_free(lookup);
    SDL_Log("");
}

/* ── Demo 5: String Hash Map ─────────────────────────────────────────────── */

static void demo_string_map(void)
{
    SDL_Log("--- Demo 5: String Hash Map ---");

    struct { char *key; int value; } *config = NULL;
    forge_shm_init_strdup(config);
    SDL_Log("  Initialized string map in strdup mode");

    forge_shm_put(config, "width",      1920);
    forge_shm_put(config, "height",     1080);
    forge_shm_put(config, "fullscreen", 1);
    forge_shm_put(config, "vsync",      1);
    forge_shm_put(config, "fov",        90);

    SDL_Log("  Inserted 5 config entries, length = %td",
            forge_shm_length(config));

    int w = forge_shm_get(config, "width");
    int h = forge_shm_get(config, "height");
    SDL_Log("  get(\"width\") = %d, get(\"height\") = %d", w, h);

    SDL_Log("  Iterating:");
    {
        ptrdiff_t i;
        forge_shm_iter(config, i) {
            SDL_Log("    \"%s\" = %d",
                    config[i + 1].key, config[i + 1].value);
        }
    }

    forge_shm_free(config);
    SDL_Log("");
}

/* ── Demo 6: Comparison ──────────────────────────────────────────────────── */

static void demo_comparison(void)
{
    SDL_Log("--- Demo 6: Arrays vs Hash Maps ---");
    SDL_Log("");
    SDL_Log("  Dynamic Array (forge_arr_*):");
    SDL_Log("    int *scores = NULL;");
    SDL_Log("    forge_arr_append(scores, 95);");
    SDL_Log("    forge_arr_append(scores, 87);");
    SDL_Log("    score = scores[0];          // O(1) by index");
    SDL_Log("    forge_arr_free(scores);");
    SDL_Log("");
    SDL_Log("  Hash Map (forge_hm_*):");
    SDL_Log("    struct { int key; int value; } *lookup = NULL;");
    SDL_Log("    forge_hm_put(lookup, 42, 95);");
    SDL_Log("    forge_hm_put(lookup, 99, 87);");
    SDL_Log("    score = forge_hm_get(lookup, 42);  // O(1) by key");
    SDL_Log("    forge_hm_free(lookup);");
    SDL_Log("");
    SDL_Log("  When to use which:");
    SDL_Log("    - Array: ordered data, indexed access, iteration");
    SDL_Log("    - Hash map: keyed lookup, existence checks, deduplication");
    SDL_Log("    - Both start as NULL and grow automatically");
    SDL_Log("    - Both use typed pointers (no void* casting)");
    SDL_Log("");
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

    SDL_Log("=== Engine Lesson 13: Stretchy Containers ===");
    SDL_Log("");

    demo_dynamic_arrays();
    demo_array_operations();
    demo_hash_map_basics();
    demo_default_values();
    demo_string_map();
    demo_comparison();

    SDL_Log("=== Done ===");

    SDL_Quit();
    return 0;
}
