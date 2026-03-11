/*
 * glTF Parser Overflow & Bounds Tests
 *
 * Tests for overflow protection, negative value guards, and out-of-range
 * index handling in common/gltf/forge_gltf.h.
 *
 * Split from test_gltf.c to keep the main test file focused on parsing
 * correctness and this file focused on adversarial/malformed input.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stddef.h>
#include "gltf/forge_gltf.h"

/* ── Test Framework (MSVC C99 compatible) ────────────────────────────────── */

static int test_count  = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST(name) do { test_count++; SDL_Log("  Testing: %s", (name)); } while (0)

/* Plain macros — abort test via return (use for tests without cleanup). */
#define ASSERT_TRUE(cond) \
    do { if (!(cond)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "    FAIL: %s (line %d)", #cond, __LINE__); \
        test_failed++; return; \
    } } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_INT_EQ(a, b) \
    do { if ((a) != (b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "    FAIL: %d != %d (line %d)", \
                     (int)(a), (int)(b), __LINE__); \
        test_failed++; return; \
    } } while (0)

/* Cleanup-aware macros — jump to a 'cleanup' label instead of returning,
 * so arena/heap resources are always released on assertion failure. */
#define ASSERT_TRUE_C(cond) \
    do { if (!(cond)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "    FAIL: %s (line %d)", #cond, __LINE__); \
        test_failed++; goto cleanup; \
    } } while (0)

#define ASSERT_FALSE_C(cond) ASSERT_TRUE_C(!(cond))

#define ASSERT_INT_EQ_C(a, b) \
    do { if ((a) != (b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "    FAIL: %d != %d (line %d)", \
                     (int)(a), (int)(b), __LINE__); \
        test_failed++; goto cleanup; \
    } } while (0)

#define END_TEST() do { SDL_Log("    PASS"); test_passed++; } while (0)

/* ── Helper: write temp files for glTF tests ─────────────────────────────── */

typedef struct TempGltf {
    char gltf_path[FORGE_GLTF_PATH_SIZE];
    char bin_path[FORGE_GLTF_PATH_SIZE];
} TempGltf;

/* Cached writable path — prefer SDL_GetPrefPath (always writable) over
 * SDL_GetBasePath (may be read-only in CI or app-bundle layouts). */
static char *g_temp_path;
static bool  g_temp_path_owned;  /* true if g_temp_path needs SDL_free */

static void init_temp_path(void)
{
    if (g_temp_path) return;
    g_temp_path = SDL_GetPrefPath("forge-gpu", "test_gltf_overflow");
    if (g_temp_path) {
        g_temp_path_owned = true;
    } else {
        g_temp_path = (char *)SDL_GetBasePath();
        g_temp_path_owned = false;
    }
}

static void cleanup_temp_path(void)
{
    if (g_temp_path_owned) {
        SDL_free(g_temp_path);
    }
    g_temp_path = NULL;
    g_temp_path_owned = false;
}

/* Write a .gltf JSON file and .bin binary file to a writable temp directory. */
static bool write_temp_gltf(const char *json_text,
                             const void *bin_data, size_t bin_size,
                             const char *name, TempGltf *out)
{
    SDL_IOStream *io;
    size_t json_len;

    init_temp_path();
    if (!g_temp_path) return false;

    SDL_snprintf(out->gltf_path, sizeof(out->gltf_path),
                 "%s%s.gltf", g_temp_path, name);
    SDL_snprintf(out->bin_path, sizeof(out->bin_path),
                 "%s%s.bin", g_temp_path, name);

    /* Write JSON (.gltf). */
    io = SDL_IOFromFile(out->gltf_path, "w");
    if (!io) return false;
    json_len = SDL_strlen(json_text);
    if (SDL_WriteIO(io, json_text, json_len) != json_len) {
        if (!SDL_CloseIO(io)) {
            SDL_LogError(SDL_LOG_CATEGORY_TEST,
                         "SDL_CloseIO failed for '%s': %s",
                         out->gltf_path, SDL_GetError());
        }
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_LogError(SDL_LOG_CATEGORY_TEST,
                     "SDL_CloseIO failed for '%s': %s",
                     out->gltf_path, SDL_GetError());
        return false;
    }

    /* Write binary data (.bin) — must use "wb" on Windows. */
    if (bin_data && bin_size > 0) {
        io = SDL_IOFromFile(out->bin_path, "wb");
        if (!io) return false;
        if (SDL_WriteIO(io, bin_data, bin_size) != bin_size) {
            if (!SDL_CloseIO(io)) {
                SDL_LogError(SDL_LOG_CATEGORY_TEST,
                             "SDL_CloseIO failed for '%s': %s",
                             out->bin_path, SDL_GetError());
            }
            return false;
        }
        if (!SDL_CloseIO(io)) {
            SDL_LogError(SDL_LOG_CATEGORY_TEST,
                         "SDL_CloseIO failed for '%s': %s",
                         out->bin_path, SDL_GetError());
            return false;
        }
    }

    return true;
}

static bool remove_temp_gltf(TempGltf *tg)
{
    bool ok = true;
    if (tg->gltf_path[0]) {
        if (!SDL_RemovePath(tg->gltf_path)) {
            SDL_LogError(SDL_LOG_CATEGORY_TEST,
                         "SDL_RemovePath failed for '%s': %s",
                         tg->gltf_path, SDL_GetError());
            ok = false;
        }
    }
    if (tg->bin_path[0]) {
        if (!SDL_RemovePath(tg->bin_path)) {
            SDL_LogError(SDL_LOG_CATEGORY_TEST,
                         "SDL_RemovePath failed for '%s': %s",
                         tg->bin_path, SDL_GetError());
            ok = false;
        }
    }
    return ok;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Test Cases
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── safe_mul unit tests ──────────────────────────────────────────────────── */

static void test_safe_mul_basic(void)
{
    size_t result;

    TEST("safe_mul: basic multiplication");

    ASSERT_TRUE(forge_gltf__safe_mul(10, 20, &result));
    ASSERT_TRUE(result == 200);

    ASSERT_TRUE(forge_gltf__safe_mul(0, 100, &result));
    ASSERT_TRUE(result == 0);

    ASSERT_TRUE(forge_gltf__safe_mul(100, 0, &result));
    ASSERT_TRUE(result == 0);

    ASSERT_TRUE(forge_gltf__safe_mul(1, SIZE_MAX, &result));
    ASSERT_TRUE(result == SIZE_MAX);

    END_TEST();
}

static void test_safe_mul_overflow(void)
{
    size_t result;

    TEST("safe_mul: overflow rejection");

    /* SIZE_MAX * 2 would overflow */
    ASSERT_FALSE(forge_gltf__safe_mul(SIZE_MAX, 2, &result));

    /* Two large values that overflow when multiplied */
    ASSERT_FALSE(forge_gltf__safe_mul(SIZE_MAX / 2 + 1, 3, &result));

    /* Just under the limit should succeed */
    ASSERT_TRUE(forge_gltf__safe_mul(SIZE_MAX / 4, 4, &result));

    END_TEST();
}

/* ── Buffer view bounds overflow (subtraction form) ──────────────────────── */
/* Craft a glTF where bv_offset + bv_byte_length would overflow Uint32 if
 * added naively, but the subtraction form catches it correctly. */

static void test_buffer_view_offset_overflow(void)
{
    /* Minimal valid binary: 3 floats for a VEC3 position. */
    float positions[3] = {0.0f, 1.0f, 0.0f};
    Uint8 bin[12];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("buffer view with offset+length overflow is rejected");

    SDL_memcpy(bin, positions, 12);

    /* byteOffset=4294967200, byteLength=200 would wrap to ~104 if added
     * as Uint32.  The buffer is only 12 bytes, so this must be rejected. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0}"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 1, \"type\": \"VEC3\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 4294967200, \"byteLength\": 200}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_bvovf.bin\", \"byteLength\": 12}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_bvovf", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    if (!gltf_arena.first) {
        SDL_Log("  FAIL: forge_arena_create returned empty arena");
        test_failed++;
        goto cleanup;
    }
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must fail — POSITION accessor references invalid bufferView. */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Accessor stride overflow ────────────────────────────────────────────── */
/* Craft a glTF where (count-1)*byte_stride overflows. */

static void test_accessor_stride_overflow(void)
{
    float positions[3] = {0.0f, 1.0f, 0.0f};
    Uint8 bin[12];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("accessor with stride causing overflow is rejected");

    SDL_memcpy(bin, positions, 12);

    /* count=1000000, byteStride=1000000 — (count-1)*stride overflows Uint32.
     * The buffer is only 12 bytes so this must be rejected. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0}"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 1000000, \"type\": \"VEC3\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 12,"
        "     \"byteStride\": 1000000}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_strideovf.bin\", \"byteLength\": 12}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_strideovf", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    /* Ensure the arena is usable so failures come from validation,
     * not from an OOM arena turning this into a false positive. */
    ASSERT_TRUE_C(gltf_arena.first != NULL);

    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must fail — POSITION accessor stride overflows bounds. */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Interleaved accessor stride ─────────────────────────────────────────── */
/* byteStride != element_size means interleaved data — must be rejected
 * because consumers assume tightly packed accessors. */

static void test_accessor_interleaved_stride(void)
{
    /* 48 bytes: room for 2 vertices at stride 24 (VEC3 float = 12 bytes,
     * but stride is 24 — interleaved with another attribute). */
    Uint8 bin[48];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("accessor with interleaved byteStride is rejected");

    SDL_memset(bin, 0, sizeof(bin));

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0}"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 2, \"type\": \"VEC3\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 48,"
        "     \"byteStride\": 24}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_interleaved.bin\", \"byteLength\": 48}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_interleaved", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must fail — byteStride 24 != element_size 12 (interleaved). */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Out-of-range material index ─────────────────────────────────────────── */
/* A primitive referencing material_index=999 (only 1 material exists)
 * should get material_index = -1 (default). */

static void test_material_index_out_of_range(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3] = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("out-of-range material index falls back to -1");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1,"
        "    \"material\": 999"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"materials\": [{\"name\": \"mat0\"}],"
        "  \"buffers\": [{\"uri\": \"test_matidx.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_matidx", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_TRUE_C(ok);
    ASSERT_INT_EQ_C(scene->primitive_count, 1);
    /* Out-of-range material index should fall back to -1. */
    ASSERT_INT_EQ_C(scene->primitives[0].material_index, -1);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Negative material index ─────────────────────────────────────────────── */

static void test_material_index_negative(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3] = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("negative material index falls back to -1");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1,"
        "    \"material\": -5"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"materials\": [{\"name\": \"mat0\"}],"
        "  \"buffers\": [{\"uri\": \"test_matneg.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_matneg", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_TRUE_C(ok);
    ASSERT_INT_EQ_C(scene->primitive_count, 1);
    ASSERT_INT_EQ_C(scene->primitives[0].material_index, -1);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Out-of-range mesh index on node ─────────────────────────────────────── */

static void test_mesh_index_out_of_range(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3] = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("out-of-range mesh index on node is ignored");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    /* Node references mesh index 999, but only 1 mesh exists. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 999}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_meshidx.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_meshidx", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_TRUE_C(ok);
    /* Node should have mesh_index = -1 (out-of-range rejected). */
    ASSERT_TRUE_C(scene->node_count >= 1);
    ASSERT_INT_EQ_C(scene->nodes[0].mesh_index, -1);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Negative accessor byteOffset is rejected ────────────────────────────── */

static void test_negative_accessor_offset(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3] = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("negative accessor byteOffset is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    /* accessor 0 has byteOffset: -4, which the parser must reject. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"byteOffset\": -4, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_negoff.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_negoff", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must fail — negative accessor byteOffset is rejected. */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Negative bufferView byteOffset is rejected ──────────────────────────── */

static void test_negative_buffer_view_offset(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint8 bin[36];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("negative bufferView byteOffset is rejected");

    SDL_memcpy(bin, positions, 36);

    /* bufferView has byteOffset: -100, should be ignored (treated as 0). */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0}"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": -100, \"byteLength\": 36}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_negbv.bin\", \"byteLength\": 36}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_negbv", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must fail — negative bufferView byteOffset is rejected. */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Index buffer overflow is a hard failure ──────────────────────────────── */

static void test_index_buffer_overflow_fails(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3] = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("index buffer with huge count fails the load");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    /* Index accessor claims 2 billion elements — the multiplication
     * 2000000000 * 2 overflows Uint32.  Should fail the load. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 2000000000, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_idxovf.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_idxovf", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must fail — index accessor overflow causes bounds check failure. */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Circular node hierarchy does not crash ───────────────────────────────
 * Found by fuzz_gltf: node 0 lists itself as its own child.
 * Before the fix, this caused infinite recursion in
 * forge_gltf_compute_world_transforms() → stack overflow. */

static void test_circular_node_hierarchy(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3] = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("circular node hierarchy (self-child) does not crash");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    /* Node 0 lists itself as a child — creates a cycle. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0, \"children\": [0]}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_cycle.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_cycle", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Depth limit now causes forge_gltf_load to return false (the cycle
     * in compute_world_transforms propagates upward as a load failure). */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Invalid child indices are filtered out ──────────────────────────────
 * Found by fuzz_gltf: negative or out-of-range child indices were stored
 * in the children array, causing invariant violations. */

static void test_invalid_child_indices_filtered(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3] = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("negative and out-of-range child indices are filtered out");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    /* Node 0 has children [-1, 9999] — both invalid (only 1 node exists). */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0, \"children\": [-1, 9999]}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_badchild.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_badchild", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_TRUE_C(ok);
    ASSERT_TRUE_C(scene->node_count >= 1);
    /* Both invalid children should be filtered out. */
    ASSERT_INT_EQ_C(scene->nodes[0].child_count, 0);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── POSITION accessor with wrong type (SCALAR instead of VEC3) ──────────
 * The parser must reject a POSITION accessor whose type is not VEC3,
 * because reading 1 float as 3 would cause out-of-bounds reads. */

static void test_position_accessor_wrong_type(void)
{
    float positions[3] = {0.0f, 1.0f, 0.0f};
    Uint8 bin[12];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("POSITION accessor with type SCALAR is rejected");

    SDL_memcpy(bin, positions, 12);

    /* POSITION accessor has type "SCALAR" instead of "VEC3".
     * componentType 5126 (FLOAT) is correct, but type is wrong. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0}"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 1, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 12}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_postype.bin\", \"byteLength\": 12}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_postype", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must fail — POSITION accessor type is SCALAR, not VEC3. */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Index accessor with wrong type (VEC2 instead of SCALAR) ─────────────
 * The parser must reject an index accessor whose type is not SCALAR,
 * because index data must be a single component per element. */

static void test_index_accessor_wrong_type(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[6] = {0, 1, 2, 0, 0, 0};
    Uint8 bin[48];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("index accessor with type VEC2 is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 12);

    /* Index accessor has type "VEC2" instead of "SCALAR".
     * componentType 5123 (UNSIGNED_SHORT) is correct, but type is wrong. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"VEC2\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 12}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_idxtype.bin\", \"byteLength\": 48}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_idxtype", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must fail — index accessor type is VEC2, not SCALAR. */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Index accessor with unsupported component type (FLOAT) ──────────────
 * glTF indices must be UNSIGNED_BYTE (5121), UNSIGNED_SHORT (5123), or
 * UNSIGNED_INT (5125).  FLOAT (5126) is not a valid index component type
 * and the parser must reject it rather than silently dropping geometry. */

static void test_index_unsupported_component_type(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    float bad_indices[3] = {0.0f, 1.0f, 2.0f};
    Uint8 bin[48];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("index accessor with FLOAT component type is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, bad_indices, 12);

    /* Index accessor has componentType 5126 (FLOAT) — invalid for indices. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 12}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_idxfloat.bin\", \"byteLength\": 48}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_idxfloat", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must fail — FLOAT is not a valid index component type. */
    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Index accessor with UNSIGNED_BYTE component type (happy path) ────────
 * UNSIGNED_BYTE (5121) is a valid index component type per the glTF spec.
 * After verifying that FLOAT is rejected, confirm that UNSIGNED_BYTE is
 * accepted so we don't over-reject valid files. */

static void test_index_unsigned_byte_accepted(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint8 indices[3] = {0, 1, 2};
    Uint8 bin[39];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("index accessor with UNSIGNED_BYTE component type is accepted");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 3);

    /* Index accessor has componentType 5121 (UNSIGNED_BYTE) — valid. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5121,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 3}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_idxu8.bin\", \"byteLength\": 39}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_idxu8", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    /* Load must succeed — UNSIGNED_BYTE is a valid index component type. */
    ASSERT_TRUE_C(ok);

    /* Verify geometry was actually created with 8-bit index widening. */
    ASSERT_TRUE_C(scene->mesh_count > 0);
    ASSERT_TRUE_C(scene->primitive_count > 0);
    ASSERT_TRUE_C(scene->primitives[0].index_count > 0);
    /* UNSIGNED_BYTE is widened to Uint16 (stride 2). */
    ASSERT_TRUE_C(scene->primitives[0].index_stride == 2);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Index out-of-range: UNSIGNED_SHORT ───────────────────────────────── */

/* An index value >= vertex_count must cause forge_gltf_load to fail.
 * This tests the Uint16 (UNSIGNED_SHORT / 5123) code path. */

static void test_index_out_of_range_uint16(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};  /* 3 vertices */
    Uint16 indices[3]  = {0, 1, 3};  /* index 3 >= vertex_count 3 */
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("UNSIGNED_SHORT index >= vertex_count is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_idx16oor.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_idx16oor", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Index out-of-range: UNSIGNED_INT ────────────────────────────────── */

/* Same validation for Uint32 (UNSIGNED_INT / 5125) indices. */

static void test_index_out_of_range_uint32(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};  /* 3 vertices */
    Uint32 indices[3]  = {0, 1, 5};  /* index 5 >= vertex_count 3 */
    Uint8 bin[48];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("UNSIGNED_INT index >= vertex_count is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 12);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5125,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 12}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_idx32oor.bin\", \"byteLength\": 48}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_idx32oor", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Index out-of-range: UNSIGNED_BYTE ───────────────────────────────── */

/* Same validation for Uint8 (UNSIGNED_BYTE / 5121) indices, which are
 * widened to Uint16 during loading. */

static void test_index_out_of_range_uint8(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};  /* 3 vertices */
    Uint8 indices[3]   = {0, 1, 4};  /* index 4 >= vertex_count 3 */
    Uint8 bin[39];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("UNSIGNED_BYTE index >= vertex_count is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 3);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5121,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 3}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_idx8oor.bin\", \"byteLength\": 39}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_idx8oor", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Index in-range: boundary value ──────────────────────────────────── */

/* Index equal to vertex_count-1 (the maximum valid value) must succeed. */

static void test_index_at_boundary_accepted(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};  /* 3 vertices */
    Uint16 indices[3]  = {0, 1, 2};  /* all indices < vertex_count 3 */
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("index at vertex_count-1 boundary is accepted");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_idxbnd.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_idxbnd", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_TRUE_C(ok);
    ASSERT_TRUE_C(scene->primitive_count > 0);
    ASSERT_TRUE_C(scene->primitives[0].index_count == 3);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Negative byteStride in bufferView ────────────────────────────────── */

/* forge_gltf__get_accessor must reject a bufferView with a negative
 * byteStride.  Previously, negative values silently fell through to the
 * tightly-packed default. */

static void test_negative_byte_stride(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3]  = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("negative byteStride in bufferView is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36,"
        "     \"byteStride\": -4},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_negstride.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_negstride", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ── Non-numeric accessor fields ─────────────────────────────────────── */

/* Accessor componentType, bufferView, and count must be numbers.
 * If any is a string or object, forge_gltf_load must reject the file. */

static void test_accessor_non_numeric_component_type(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3]  = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("accessor with string componentType is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": \"FLOAT\","
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_strcomp.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_strcomp", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

static void test_accessor_non_numeric_buffer_view(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3]  = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("accessor with string bufferView is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": \"zero\", \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_strbv.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_strbv", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

static void test_accessor_non_numeric_count(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3]  = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg = {0};
    ForgeGltfScene *scene = NULL;
    ForgeArena gltf_arena = {0};
    bool wrote, ok;

    TEST("accessor with string count is rejected");

    SDL_memcpy(bin, positions, 36);
    SDL_memcpy(bin + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": \"three\", \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_strcnt.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) { test_failed++; return; }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_strcnt", &tg);
    ASSERT_TRUE_C(wrote);

    gltf_arena = forge_arena_create(0);
    ok = forge_gltf_load(tg.gltf_path, scene, &gltf_arena);

    ASSERT_FALSE_C(ok);

    END_TEST();
cleanup:
    remove_temp_gltf(&tg);
    forge_arena_destroy(&gltf_arena);
    SDL_free(scene);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== glTF Overflow & Bounds Tests ===\n");

    /* safe_mul */
    test_safe_mul_basic();
    test_safe_mul_overflow();

    /* Buffer view / accessor overflow */
    test_buffer_view_offset_overflow();
    test_accessor_stride_overflow();
    test_accessor_interleaved_stride();

    /* Out-of-range and negative indices */
    test_material_index_out_of_range();
    test_material_index_negative();
    test_mesh_index_out_of_range();

    /* Negative offsets */
    test_negative_accessor_offset();
    test_negative_buffer_view_offset();

    /* Index buffer overflow */
    test_index_buffer_overflow_fails();

    /* Accessor arity validation */
    test_position_accessor_wrong_type();
    test_index_accessor_wrong_type();
    test_index_unsupported_component_type();
    test_index_unsigned_byte_accepted();

    /* Index out-of-range validation */
    test_index_out_of_range_uint16();
    test_index_out_of_range_uint32();
    test_index_out_of_range_uint8();
    test_index_at_boundary_accepted();

    /* Stride validation */
    test_negative_byte_stride();

    /* Non-numeric accessor fields (cJSON_IsNumber checks) */
    test_accessor_non_numeric_component_type();
    test_accessor_non_numeric_buffer_view();
    test_accessor_non_numeric_count();

    /* Fuzzer-found bugs */
    test_circular_node_hierarchy();
    test_invalid_child_indices_filtered();

    /* Summary */
    SDL_Log("\n=== Test Summary ===");
    SDL_Log("Total:  %d", test_count);
    SDL_Log("Passed: %d", test_passed);
    SDL_Log("Failed: %d", test_failed);

    cleanup_temp_path();

    if (test_failed > 0) {
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "\nSome tests FAILED!");
        SDL_Quit();
        return 1;
    }

    SDL_Log("\nAll tests PASSED!");
    SDL_Quit();
    return 0;
}
