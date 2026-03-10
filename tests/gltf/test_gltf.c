/*
 * glTF Parser Tests
 *
 * Automated tests for common/gltf/forge_gltf.h
 * Writes small glTF + binary files to a temp directory, parses them, and
 * verifies the output (vertices, indices, materials, nodes, transforms).
 *
 * Also tests against the CesiumMilkTruck model if available.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stddef.h>
#include "math/forge_math.h"
#include "gltf/forge_gltf.h"
#include "gltf/forge_gltf_anim.h"

/* ── Test Framework (MSVC C99 compatible) ────────────────────────────────── */

static int test_count  = 0;
static int test_passed = 0;
static int test_failed = 0;

#define EPSILON 0.001f

static bool float_eq(float a, float b)
{
    return SDL_fabsf(a - b) < EPSILON;
}

static bool vec2_eq(vec2 a, vec2 b)
{
    return float_eq(a.x, b.x) && float_eq(a.y, b.y);
}

static bool vec3_eq(vec3 a, vec3 b)
{
    return float_eq(a.x, b.x) && float_eq(a.y, b.y) && float_eq(a.z, b.z);
}

static bool quat_eq(quat a, quat b)
{
    /* q and -q represent the same rotation — check both signs. */
    bool same = float_eq(a.w, b.w) && float_eq(a.x, b.x)
             && float_eq(a.y, b.y) && float_eq(a.z, b.z);
    bool flip = float_eq(a.w, -b.w) && float_eq(a.x, -b.x)
             && float_eq(a.y, -b.y) && float_eq(a.z, -b.z);
    return same || flip;
}

#define TEST(name) do { test_count++; SDL_Log("  Testing: %s", (name)); } while (0)

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

#define ASSERT_UINT_EQ(a, b) \
    do { if ((a) != (b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "    FAIL: %u != %u (line %d)", \
                     (unsigned)(a), (unsigned)(b), __LINE__); \
        test_failed++; return; \
    } } while (0)

#define ASSERT_FLOAT_EQ(a, b) \
    do { if (!float_eq((a), (b))) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "    FAIL: %.6f != %.6f (line %d)", \
                     (double)(a), (double)(b), __LINE__); \
        test_failed++; return; \
    } } while (0)

#define ASSERT_VEC2_EQ(a, b) \
    do { if (!vec2_eq((a), (b))) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
            "    FAIL: (%.3f,%.3f) != (%.3f,%.3f) (line %d)", \
            (double)(a).x, (double)(a).y, (double)(b).x, (double)(b).y, __LINE__); \
        test_failed++; return; \
    } } while (0)

#define ASSERT_VEC3_EQ(a, b) \
    do { if (!vec3_eq((a), (b))) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
            "    FAIL: (%.3f,%.3f,%.3f) != (%.3f,%.3f,%.3f) (line %d)", \
            (double)(a).x, (double)(a).y, (double)(a).z, \
            (double)(b).x, (double)(b).y, (double)(b).z, __LINE__); \
        test_failed++; return; \
    } } while (0)

#define END_TEST() do { SDL_Log("    PASS"); test_passed++; } while (0)

/* ── Helper: write temp files for glTF tests ─────────────────────────────── */

typedef struct TempGltf {
    char gltf_path[FORGE_GLTF_PATH_SIZE];
    char bin_path[FORGE_GLTF_PATH_SIZE];
} TempGltf;

/* Write a .gltf JSON file and .bin binary file next to the executable. */
static bool write_temp_gltf(const char *json_text,
                             const void *bin_data, size_t bin_size,
                             const char *name, TempGltf *out)
{
    const char *base;
    SDL_IOStream *io;
    size_t json_len;

    base = SDL_GetBasePath();
    if (!base) return false;

    SDL_snprintf(out->gltf_path, sizeof(out->gltf_path),
                 "%s%s.gltf", base, name);
    SDL_snprintf(out->bin_path, sizeof(out->bin_path),
                 "%s%s.bin", base, name);

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

/* ── Nonexistent file ─────────────────────────────────────────────────────── */

static void test_nonexistent_file(void)
{
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool ok;

    TEST("nonexistent file returns false");

    ok = forge_gltf_load("this_file_does_not_exist_12345.gltf", scene);
    ASSERT_FALSE(ok);

    SDL_free(scene);
    END_TEST();
}

/* ── Invalid JSON ─────────────────────────────────────────────────────────── */

static void test_invalid_json(void)
{
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("invalid JSON returns false");

    wrote = write_temp_gltf("{ this is not valid json !!!",
                             NULL, 0, "test_invalid", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_FALSE(ok);
    SDL_free(scene);
    END_TEST();
}

/* ── forge_gltf_free on zeroed scene ──────────────────────────────────────── */

static void test_free_zeroed_scene(void)
{
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }

    TEST("forge_gltf_free on zeroed scene is safe");

    SDL_memset(scene, 0, sizeof(*scene));
    forge_gltf_free(scene);

    ASSERT_INT_EQ(scene->primitive_count, 0);
    ASSERT_INT_EQ(scene->node_count, 0);

    SDL_free(scene);
    END_TEST();
}

/* ── Invalid componentType ─────────────────────────────────────────────────── */
/* Accessor with an invalid componentType (not one of the six glTF values)
 * should be rejected — the primitive is skipped. */

static void test_invalid_component_type(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("invalid componentType (9999) rejects accessor");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* componentType 9999 is not one of the six allowed values. */
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
        "    {\"bufferView\": 0, \"componentType\": 9999,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_badcomp.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_badcomp", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    /* Scene loads but primitive is skipped due to bad componentType. */
    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->primitive_count, 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Misaligned float accessor is rejected ────────────────────────────────── */
/* A float (4-byte) accessor at a 1-byte-aligned offset should be rejected.
 * The glTF spec requires data to be aligned to the component size. */

static void test_accessor_misaligned(void)
{
    /* Binary layout: 1 padding byte, then 3 floats (VEC3) at offset 1,
     * then 3 uint16 indices at offset 13.  Total = 19 bytes. */
    Uint8 bin_data[19];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("float accessor at misaligned offset is rejected");

    SDL_memset(bin_data, 0, sizeof(bin_data));

    /* bufferView 0 starts at byte 1 — not 4-byte aligned.
     * The accessor references this view for POSITION (VEC3 float). */
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
        "     \"count\": 1, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 1, \"byteLength\": 12},"
        "    {\"buffer\": 0, \"byteOffset\": 13, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_misalign.bin\","
        "                 \"byteLength\": 19}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_misalign", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    /* Scene loads but the misaligned POSITION accessor is rejected,
     * so no primitive is created. */
    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->primitive_count, 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Accessor exceeds bufferView bounds ───────────────────────────────────── */
/* An accessor claiming 3 VEC3 floats (36 bytes) in a bufferView of only
 * 12 bytes should be rejected. */

static void test_accessor_exceeds_buffer_view(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("accessor exceeding bufferView.byteLength is rejected");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* bufferView 0 only claims 12 bytes, but accessor wants 3 VEC3 = 36. */
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
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 12},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_bvsmall.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_bvsmall", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    /* Scene loads but primitive is skipped — accessor overflows bufferView. */
    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->primitive_count, 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── bufferView exceeds buffer bounds ─────────────────────────────────────── */
/* A bufferView whose offset + length exceeds the binary buffer should be
 * rejected. */

static void test_buffer_view_exceeds_buffer(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("bufferView exceeding buffer size is rejected");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* bufferView 0: offset=20 + length=36 = 56 > buffer size (42). */
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
        "    {\"buffer\": 0, \"byteOffset\": 20, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_bvover.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_bvover", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    /* Primitive skipped — bufferView overflows the binary buffer. */
    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->primitive_count, 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Missing bufferView.byteLength ────────────────────────────────────────── */
/* bufferView.byteLength is required by the glTF spec.  A view missing it
 * should be rejected. */

static void test_missing_buffer_view_byte_length(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("missing bufferView.byteLength rejects accessor");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* bufferView 0 is missing byteLength entirely. */
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
        "    {\"buffer\": 0, \"byteOffset\": 0},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_nobvlen.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_nobvlen", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    /* Primitive skipped — bufferView has no byteLength. */
    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->primitive_count, 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Minimal triangle (positions + indices) ───────────────────────────────── */

static void test_minimal_triangle(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;
    const Uint16 *idx;

    TEST("minimal triangle (positions + uint16 indices)");

    /* 3 positions (float3) + 3 indices (uint16) = 42 bytes. */
    positions[0] = 0.0f; positions[1] = 0.0f; positions[2] = 0.0f;
    positions[3] = 1.0f; positions[4] = 0.0f; positions[5] = 0.0f;
    positions[6] = 0.0f; positions[7] = 1.0f; positions[8] = 0.0f;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

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
        "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_tri.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data), "test_tri", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->primitive_count, 1);
    ASSERT_INT_EQ(scene->node_count, 1);
    ASSERT_INT_EQ(scene->mesh_count, 1);
    ASSERT_UINT_EQ(scene->primitives[0].vertex_count, 3);

    /* Check positions. */
    ASSERT_VEC3_EQ(scene->primitives[0].vertices[0].position,
                   vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(scene->primitives[0].vertices[1].position,
                   vec3_create(1.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(scene->primitives[0].vertices[2].position,
                   vec3_create(0.0f, 1.0f, 0.0f));

    /* Check indices (uint16). */
    ASSERT_UINT_EQ(scene->primitives[0].index_count, 3);
    ASSERT_UINT_EQ(scene->primitives[0].index_stride, 2);
    idx = (const Uint16 *)scene->primitives[0].indices;
    ASSERT_TRUE(idx != NULL);
    ASSERT_UINT_EQ(idx[0], 0);
    ASSERT_UINT_EQ(idx[1], 1);
    ASSERT_UINT_EQ(idx[2], 2);

    /* Normals/UVs should be zero (not in file). */
    ASSERT_VEC3_EQ(scene->primitives[0].vertices[0].normal,
                   vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC2_EQ(scene->primitives[0].vertices[0].uv,
                   vec2_create(0.0f, 0.0f));

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Triangle with normals and UVs ────────────────────────────────────────── */

static void test_normals_and_uvs(void)
{
    float positions[9];
    float normals[9];
    float uvs[6];
    Uint16 indices[3];
    Uint8 bin_data[102];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("triangle with normals and UVs");

    /* Binary: positions(36) + normals(36) + UVs(24) + indices(6) = 102 */
    positions[0] = 0.0f; positions[1] = 0.0f; positions[2] = 0.0f;
    positions[3] = 1.0f; positions[4] = 0.0f; positions[5] = 0.0f;
    positions[6] = 0.0f; positions[7] = 1.0f; positions[8] = 0.0f;

    normals[0] = 0.0f; normals[1] = 0.0f; normals[2] = 1.0f;
    normals[3] = 0.0f; normals[4] = 0.0f; normals[5] = 1.0f;
    normals[6] = 0.0f; normals[7] = 0.0f; normals[8] = 1.0f;

    uvs[0] = 0.0f; uvs[1] = 0.0f;
    uvs[2] = 1.0f; uvs[3] = 0.0f;
    uvs[4] = 0.0f; uvs[5] = 1.0f;

    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data,      positions, 36);
    SDL_memcpy(bin_data + 36, normals,   36);
    SDL_memcpy(bin_data + 72, uvs,       24);
    SDL_memcpy(bin_data + 96, indices,    6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {"
        "      \"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2"
        "    }, \"indices\": 3"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 2, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC2\"},"
        "    {\"bufferView\": 3, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 72, \"byteLength\": 24},"
        "    {\"buffer\": 0, \"byteOffset\": 96, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_nrmuv.bin\", \"byteLength\": 102}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_nrmuv", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(scene->primitives[0].vertex_count, 3);

    ASSERT_VEC3_EQ(scene->primitives[0].vertices[0].normal,
                   vec3_create(0.0f, 0.0f, 1.0f));
    ASSERT_VEC3_EQ(scene->primitives[0].vertices[1].normal,
                   vec3_create(0.0f, 0.0f, 1.0f));

    ASSERT_VEC2_EQ(scene->primitives[0].vertices[0].uv,
                   vec2_create(0.0f, 0.0f));
    ASSERT_VEC2_EQ(scene->primitives[0].vertices[1].uv,
                   vec2_create(1.0f, 0.0f));
    ASSERT_VEC2_EQ(scene->primitives[0].vertices[2].uv,
                   vec2_create(0.0f, 1.0f));

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Normal count mismatch → normals treated as missing ───────────────────── */

static void test_normal_count_mismatch(void)
{
    float positions[9];
    float normals[6];   /* only 2 normals, not 3 */
    Uint16 indices[3];
    Uint8 bin_data[66]; /* 36 + 24 + 6 */
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("NORMAL accessor count != POSITION count → normals ignored");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;

    normals[0] = 0; normals[1] = 0; normals[2] = 1;
    normals[3] = 0; normals[4] = 0; normals[5] = 1;

    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data,      positions, 36);
    SDL_memcpy(bin_data + 36, normals,   24);
    SDL_memcpy(bin_data + 60, indices,    6);

    /* NORMAL accessor count=2 but POSITION count=3. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0, \"NORMAL\": 1},"
        "    \"indices\": 2"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5126,"
        "     \"count\": 2, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 2, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 24},"
        "    {\"buffer\": 0, \"byteOffset\": 60, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_normmis.bin\", \"byteLength\": 66}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_normmis", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(scene->primitives[0].vertex_count, 3);

    /* Normals should be zero — treated as missing due to count mismatch. */
    ASSERT_VEC3_EQ(scene->primitives[0].vertices[0].normal,
                   vec3_create(0.0f, 0.0f, 0.0f));
    ASSERT_VEC3_EQ(scene->primitives[0].vertices[2].normal,
                   vec3_create(0.0f, 0.0f, 0.0f));

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── UV wrong componentType → UVs treated as missing ─────────────────────── */

static void test_uv_wrong_component_type(void)
{
    float positions[9];
    Uint16 fake_uvs[6]; /* uint16 instead of float */
    Uint16 indices[3];
    Uint8 bin_data[54]; /* 36 + 12 + 6 */
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("TEXCOORD_0 with wrong componentType (USHORT) → UVs ignored");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;

    fake_uvs[0] = 0; fake_uvs[1] = 0;
    fake_uvs[2] = 1; fake_uvs[3] = 0;
    fake_uvs[4] = 0; fake_uvs[5] = 1;

    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data,      positions, 36);
    SDL_memcpy(bin_data + 36, fake_uvs,  12);
    SDL_memcpy(bin_data + 48, indices,    6);

    /* TEXCOORD_0 componentType=5123 (UNSIGNED_SHORT) instead of 5126 (FLOAT). */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0, \"TEXCOORD_0\": 1},"
        "    \"indices\": 2"
        "  }]}],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"VEC2\"},"
        "    {\"bufferView\": 2, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 12},"
        "    {\"buffer\": 0, \"byteOffset\": 48, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_uvbad.bin\", \"byteLength\": 54}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_uvbad", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_UINT_EQ(scene->primitives[0].vertex_count, 3);

    /* UVs should be zero — wrong componentType means they're skipped. */
    ASSERT_FALSE(scene->primitives[0].has_uvs);
    ASSERT_VEC2_EQ(scene->primitives[0].vertices[0].uv,
                   vec2_create(0.0f, 0.0f));

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: base color factor ──────────────────────────────────────────── */

static void test_material_base_color(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("material with base color factor");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"name\": \"RedMat\","
        "    \"pbrMetallicRoughness\": {"
        "      \"baseColorFactor\": [0.8, 0.2, 0.1, 1.0]"
        "    }"
        "  }],"
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
        "  \"buffers\": [{\"uri\": \"test_mat.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data), "test_mat", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);
    ASSERT_FLOAT_EQ(scene->materials[0].base_color[0], 0.8f);
    ASSERT_FLOAT_EQ(scene->materials[0].base_color[1], 0.2f);
    ASSERT_FLOAT_EQ(scene->materials[0].base_color[2], 0.1f);
    ASSERT_FLOAT_EQ(scene->materials[0].base_color[3], 1.0f);
    ASSERT_FALSE(scene->materials[0].has_texture);
    ASSERT_TRUE(SDL_strcmp(scene->materials[0].name, "RedMat") == 0);
    ASSERT_INT_EQ(scene->primitives[0].material_index, 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: texture path resolution ────────────────────────────────────── */

static void test_material_texture_path(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;
    const char *path;
    size_t len;

    TEST("material texture path resolution");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"pbrMetallicRoughness\": {"
        "      \"baseColorTexture\": {\"index\": 0},"
        "      \"baseColorFactor\": [1.0, 1.0, 1.0, 1.0]"
        "    }"
        "  }],"
        "  \"textures\": [{\"source\": 0}],"
        "  \"images\": [{\"uri\": \"diffuse.png\"}],"
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
        "  \"buffers\": [{\"uri\": \"test_texpath.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_texpath", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);
    ASSERT_TRUE(scene->materials[0].has_texture);

    /* texture_path should end with "diffuse.png". */
    path = scene->materials[0].texture_path;
    len = SDL_strlen(path);
    ASSERT_TRUE(len >= 11);
    ASSERT_TRUE(SDL_strcmp(path + len - 11, "diffuse.png") == 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: PBR metallic-roughness fields ──────────────────────────────── */

static void test_material_pbr_fields(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("material PBR metallic-roughness fields");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"pbrMetallicRoughness\": {"
        "      \"metallicFactor\": 0.3,"
        "      \"roughnessFactor\": 0.7"
        "    }"
        "  }],"
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
        "  \"buffers\": [{\"uri\": \"test_pbr.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data), "test_pbr", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);
    ASSERT_FLOAT_EQ(scene->materials[0].metallic_factor, 0.3f);
    ASSERT_FLOAT_EQ(scene->materials[0].roughness_factor, 0.7f);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: normal scale, occlusion, emissive ──────────────────────────── */

static void test_material_extended_fields(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("material normal scale, occlusion strength, emissive factor");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"pbrMetallicRoughness\": {},"
        "    \"normalTexture\": {\"index\": 0, \"scale\": 0.5},"
        "    \"occlusionTexture\": {\"index\": 1, \"strength\": 0.8},"
        "    \"emissiveFactor\": [1.0, 0.5, 0.2],"
        "    \"emissiveTexture\": {\"index\": 2}"
        "  }],"
        "  \"textures\": [{\"source\": 0}, {\"source\": 1}, {\"source\": 2}],"
        "  \"images\": ["
        "    {\"uri\": \"normal.png\"},"
        "    {\"uri\": \"occlusion.png\"},"
        "    {\"uri\": \"emissive.png\"}"
        "  ],"
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
        "  \"buffers\": [{\"uri\": \"test_ext.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data), "test_ext", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);

    /* Normal scale */
    ASSERT_TRUE(scene->materials[0].has_normal_map);
    ASSERT_FLOAT_EQ(scene->materials[0].normal_scale, 0.5f);

    /* Occlusion */
    ASSERT_TRUE(scene->materials[0].has_occlusion);
    ASSERT_FLOAT_EQ(scene->materials[0].occlusion_strength, 0.8f);

    /* Emissive */
    ASSERT_TRUE(scene->materials[0].has_emissive);
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[0], 1.0f);
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[1], 0.5f);
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[2], 0.2f);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: defaults when no PBR block ─────────────────────────────────── */

static void test_material_defaults(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("material defaults (no pbrMetallicRoughness block)");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* Material with no pbrMetallicRoughness — all fields should get
     * spec defaults.  Previously this caused a `continue` that skipped
     * parsing the rest of the material. */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"name\": \"NoPBR\","
        "    \"normalTexture\": {\"index\": 0, \"scale\": 2.0},"
        "    \"emissiveFactor\": [0.5, 0.5, 0.5]"
        "  }],"
        "  \"textures\": [{\"source\": 0}],"
        "  \"images\": [{\"uri\": \"normal.png\"}],"
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
        "  \"buffers\": [{\"uri\": \"test_def.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data), "test_def", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);

    /* PBR defaults (no pbrMetallicRoughness block) */
    ASSERT_FLOAT_EQ(scene->materials[0].metallic_factor, 1.0f);
    ASSERT_FLOAT_EQ(scene->materials[0].roughness_factor, 1.0f);
    ASSERT_FALSE(scene->materials[0].has_texture);
    ASSERT_FALSE(scene->materials[0].has_metallic_roughness);

    /* normalTexture should still be parsed even without PBR block */
    ASSERT_TRUE(scene->materials[0].has_normal_map);
    ASSERT_FLOAT_EQ(scene->materials[0].normal_scale, 2.0f);

    /* emissiveFactor should still be parsed */
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[0], 0.5f);
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[1], 0.5f);
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[2], 0.5f);

    /* Occlusion defaults */
    ASSERT_FALSE(scene->materials[0].has_occlusion);
    ASSERT_FLOAT_EQ(scene->materials[0].occlusion_strength, 1.0f);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: metallic-roughness texture resolution ──────────────────────── */

static void test_material_mr_texture(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;
    const char *path;
    size_t len;

    TEST("material metallic-roughness texture path resolution");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"pbrMetallicRoughness\": {"
        "      \"metallicRoughnessTexture\": {\"index\": 0}"
        "    }"
        "  }],"
        "  \"textures\": [{\"source\": 0}],"
        "  \"images\": [{\"uri\": \"metal_rough.png\"}],"
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
        "  \"buffers\": [{\"uri\": \"test_mr.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data), "test_mr", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);
    ASSERT_TRUE(scene->materials[0].has_metallic_roughness);

    path = scene->materials[0].metallic_roughness_path;
    len = SDL_strlen(path);
    ASSERT_TRUE(len >= 15);
    ASSERT_TRUE(SDL_strcmp(path + len - 15, "metal_rough.png") == 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Node hierarchy: accumulated translation ──────────────────────────────── */

static void test_node_hierarchy(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("node hierarchy (parent + child translations accumulate)");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": ["
        "    {\"mesh\": 0, \"translation\": [1.0, 0.0, 0.0],"
        "     \"children\": [1]},"
        "    {\"mesh\": 0, \"translation\": [0.0, 2.0, 0.0]}"
        "  ],"
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
        "  \"buffers\": [{\"uri\": \"test_hier.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_hier", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->node_count, 2);

    /* Node 0 world translation = (1,0,0). */
    ASSERT_FLOAT_EQ(scene->nodes[0].world_transform.m[12], 1.0f);
    ASSERT_FLOAT_EQ(scene->nodes[0].world_transform.m[13], 0.0f);
    ASSERT_FLOAT_EQ(scene->nodes[0].world_transform.m[14], 0.0f);

    /* Node 1 world translation = parent(1,0,0) + child(0,2,0) = (1,2,0). */
    ASSERT_FLOAT_EQ(scene->nodes[1].world_transform.m[12], 1.0f);
    ASSERT_FLOAT_EQ(scene->nodes[1].world_transform.m[13], 2.0f);
    ASSERT_FLOAT_EQ(scene->nodes[1].world_transform.m[14], 0.0f);

    /* Decomposed TRS fields — both nodes use TRS (translation only). */
    ASSERT_TRUE(scene->nodes[0].has_trs);
    ASSERT_TRUE(vec3_eq(scene->nodes[0].translation,
                        vec3_create(1.0f, 0.0f, 0.0f)));
    ASSERT_TRUE(quat_eq(scene->nodes[0].rotation, quat_identity()));
    ASSERT_TRUE(vec3_eq(scene->nodes[0].scale_xyz,
                        vec3_create(1.0f, 1.0f, 1.0f)));

    ASSERT_TRUE(scene->nodes[1].has_trs);
    ASSERT_TRUE(vec3_eq(scene->nodes[1].translation,
                        vec3_create(0.0f, 2.0f, 0.0f)));

    /* Parent reference. */
    ASSERT_INT_EQ(scene->nodes[1].parent, 0);
    ASSERT_INT_EQ(scene->root_node_count, 1);
    ASSERT_INT_EQ(scene->root_nodes[0], 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Quaternion rotation (glTF [x,y,z,w] order) ──────────────────────────── */

static void test_quaternion_rotation(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;
    const mat4 *m;

    TEST("quaternion rotation (90 deg Y, glTF [x,y,z,w] order)");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": ["
        "    {\"mesh\": 0, \"rotation\": [0.0, 0.7071068, 0.0, 0.7071068]}"
        "  ],"
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
        "  \"buffers\": [{\"uri\": \"test_quat.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_quat", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->node_count, 1);

    /* 90 deg Y rotation (column-major):
     *   col0=(0,0,-1,0)  col1=(0,1,0,0)  col2=(1,0,0,0) */
    m = &scene->nodes[0].world_transform;
    ASSERT_FLOAT_EQ(m->m[0],   0.0f);   /* col0.x */
    ASSERT_FLOAT_EQ(m->m[2],  -1.0f);   /* col0.z */
    ASSERT_FLOAT_EQ(m->m[5],   1.0f);   /* col1.y */
    ASSERT_FLOAT_EQ(m->m[8],   1.0f);   /* col2.x */
    ASSERT_FLOAT_EQ(m->m[10],  0.0f);   /* col2.z */

    /* Decomposed TRS — rotation stored as quaternion (w,x,y,z). */
    ASSERT_TRUE(scene->nodes[0].has_trs);
    ASSERT_TRUE(quat_eq(scene->nodes[0].rotation,
                        quat_create(0.7071068f, 0.0f, 0.7071068f, 0.0f)));
    ASSERT_TRUE(vec3_eq(scene->nodes[0].translation,
                        vec3_create(0.0f, 0.0f, 0.0f)));
    ASSERT_TRUE(vec3_eq(scene->nodes[0].scale_xyz,
                        vec3_create(1.0f, 1.0f, 1.0f)));

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Scale transform ──────────────────────────────────────────────────────── */

static void test_scale_transform(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("scale transform (2x uniform)");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0, \"scale\": [2.0, 2.0, 2.0]}],"
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
        "  \"buffers\": [{\"uri\": \"test_scale.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_scale", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);

    /* Diagonal should be 2.0 (uniform scale). */
    ASSERT_FLOAT_EQ(scene->nodes[0].world_transform.m[0],  2.0f);
    ASSERT_FLOAT_EQ(scene->nodes[0].world_transform.m[5],  2.0f);
    ASSERT_FLOAT_EQ(scene->nodes[0].world_transform.m[10], 2.0f);

    /* Decomposed TRS — scale stored as vec3. */
    ASSERT_TRUE(scene->nodes[0].has_trs);
    ASSERT_TRUE(vec3_eq(scene->nodes[0].scale_xyz,
                        vec3_create(2.0f, 2.0f, 2.0f)));
    ASSERT_TRUE(vec3_eq(scene->nodes[0].translation,
                        vec3_create(0.0f, 0.0f, 0.0f)));
    ASSERT_TRUE(quat_eq(scene->nodes[0].rotation, quat_identity()));

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Explicit matrix transform ─────────────────────────────────────────────── */

static void test_node_explicit_matrix(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;
    const mat4 *m;

    TEST("node with explicit 4x4 matrix transform");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* Column-major 4x4: 2x scale on X, 3x on Y, 1x on Z, translate (4,5,6).
     *   col0=(2,0,0,0) col1=(0,3,0,0) col2=(0,0,1,0) col3=(4,5,6,1) */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0, \"matrix\": ["
        "    2.0, 0.0, 0.0, 0.0,"
        "    0.0, 3.0, 0.0, 0.0,"
        "    0.0, 0.0, 1.0, 0.0,"
        "    4.0, 5.0, 6.0, 1.0"
        "  ]}],"
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
        "  \"buffers\": [{\"uri\": \"test_matrix.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_matrix", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->node_count, 1);

    m = &scene->nodes[0].world_transform;
    /* Scale: X=2, Y=3, Z=1 */
    ASSERT_FLOAT_EQ(m->m[0],  2.0f);   /* col0.x */
    ASSERT_FLOAT_EQ(m->m[5],  3.0f);   /* col1.y */
    ASSERT_FLOAT_EQ(m->m[10], 1.0f);   /* col2.z */
    /* Translation: (4, 5, 6) */
    ASSERT_FLOAT_EQ(m->m[12], 4.0f);
    ASSERT_FLOAT_EQ(m->m[13], 5.0f);
    ASSERT_FLOAT_EQ(m->m[14], 6.0f);
    /* Homogeneous w=1 */
    ASSERT_FLOAT_EQ(m->m[15], 1.0f);

    /* Explicit matrix nodes do NOT have decomposed TRS. */
    ASSERT_FALSE(scene->nodes[0].has_trs);
    /* Defaults should be preserved for non-TRS nodes. */
    ASSERT_TRUE(vec3_eq(scene->nodes[0].translation,
                        vec3_create(0.0f, 0.0f, 0.0f)));
    ASSERT_TRUE(quat_eq(scene->nodes[0].rotation, quat_identity()));
    ASSERT_TRUE(vec3_eq(scene->nodes[0].scale_xyz,
                        vec3_create(1.0f, 1.0f, 1.0f)));

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Combined TRS (translation + rotation + scale) ────────────────────────── */

static void test_combined_trs(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;
    const mat4 *m;

    TEST("combined TRS (translation + 90-deg-Y rotation + non-uniform scale)");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* translation=(3,4,5), rotation=90-deg-Y, scale=(2,1,3) */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{"
        "    \"mesh\": 0,"
        "    \"translation\": [3.0, 4.0, 5.0],"
        "    \"rotation\": [0.0, 0.7071068, 0.0, 0.7071068],"
        "    \"scale\": [2.0, 1.0, 3.0]"
        "  }],"
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
        "  \"buffers\": [{\"uri\": \"test_trs.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_trs", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->node_count, 1);

    /* Verify decomposed TRS fields are stored correctly. */
    ASSERT_TRUE(scene->nodes[0].has_trs);
    ASSERT_TRUE(vec3_eq(scene->nodes[0].translation,
                        vec3_create(3.0f, 4.0f, 5.0f)));
    ASSERT_TRUE(quat_eq(scene->nodes[0].rotation,
                        quat_create(0.7071068f, 0.0f, 0.7071068f, 0.0f)));
    ASSERT_TRUE(vec3_eq(scene->nodes[0].scale_xyz,
                        vec3_create(2.0f, 1.0f, 3.0f)));

    /* Verify local_transform = T * R * S matches the decomposed fields.
     * 90-deg-Y rotation: R swaps X↔Z with sign flip.
     *   col0 = R * scale_x = (0,0,-2,0)
     *   col1 = R * scale_y = (0,1, 0,0)
     *   col2 = R * scale_z = (3,0, 0,0)
     *   col3 = translation = (3,4, 5,1) */
    m = &scene->nodes[0].world_transform;
    ASSERT_FLOAT_EQ(m->m[0],   0.0f);   /* col0.x */
    ASSERT_FLOAT_EQ(m->m[1],   0.0f);   /* col0.y */
    ASSERT_FLOAT_EQ(m->m[2],  -2.0f);   /* col0.z */
    ASSERT_FLOAT_EQ(m->m[5],   1.0f);   /* col1.y */
    ASSERT_FLOAT_EQ(m->m[8],   3.0f);   /* col2.x */
    ASSERT_FLOAT_EQ(m->m[10],  0.0f);   /* col2.z */
    ASSERT_FLOAT_EQ(m->m[12],  3.0f);   /* translate x */
    ASSERT_FLOAT_EQ(m->m[13],  4.0f);   /* translate y */
    ASSERT_FLOAT_EQ(m->m[14],  5.0f);   /* translate z */

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Multiple primitives per mesh ─────────────────────────────────────────── */

static void test_multiple_primitives(void)
{
    float pos1[9];
    float pos2[9];
    Uint16 idx1[3];
    Uint16 idx2[3];
    Uint8 bin_data[84];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("mesh with two primitives (multi-material)");

    pos1[0] = 0; pos1[1] = 0; pos1[2] = 0;
    pos1[3] = 1; pos1[4] = 0; pos1[5] = 0;
    pos1[6] = 0; pos1[7] = 1; pos1[8] = 0;

    pos2[0] = 2; pos2[1] = 0; pos2[2] = 0;
    pos2[3] = 3; pos2[4] = 0; pos2[5] = 0;
    pos2[6] = 2; pos2[7] = 1; pos2[8] = 0;

    idx1[0] = 0; idx1[1] = 1; idx1[2] = 2;
    idx2[0] = 0; idx2[1] = 1; idx2[2] = 2;

    SDL_memcpy(bin_data,      pos1, 36);
    SDL_memcpy(bin_data + 36, pos2, 36);
    SDL_memcpy(bin_data + 72, idx1,  6);
    SDL_memcpy(bin_data + 78, idx2,  6);

    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": ["
        "    {\"attributes\": {\"POSITION\": 0}, \"indices\": 2,"
        "     \"material\": 0},"
        "    {\"attributes\": {\"POSITION\": 1}, \"indices\": 3,"
        "     \"material\": 1}"
        "  ]}],"
        "  \"materials\": ["
        "    {\"name\": \"Mat0\", \"pbrMetallicRoughness\": {"
        "      \"baseColorFactor\": [1.0, 0.0, 0.0, 1.0]}},"
        "    {\"name\": \"Mat1\", \"pbrMetallicRoughness\": {"
        "      \"baseColorFactor\": [0.0, 0.0, 1.0, 1.0]}}"
        "  ],"
        "  \"accessors\": ["
        "    {\"bufferView\": 0, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 1, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"},"
        "    {\"bufferView\": 2, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"},"
        "    {\"bufferView\": 3, \"componentType\": 5123,"
        "     \"count\": 3, \"type\": \"SCALAR\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 72, \"byteLength\": 6},"
        "    {\"buffer\": 0, \"byteOffset\": 78, \"byteLength\": 6}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_multi.bin\", \"byteLength\": 84}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_multi", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->mesh_count, 1);
    ASSERT_INT_EQ(scene->primitive_count, 2);
    ASSERT_INT_EQ(scene->material_count, 2);

    /* First primitive = material 0 (red). */
    ASSERT_INT_EQ(scene->primitives[0].material_index, 0);
    ASSERT_FLOAT_EQ(scene->materials[0].base_color[0], 1.0f);
    ASSERT_FLOAT_EQ(scene->materials[0].base_color[2], 0.0f);

    /* Second primitive = material 1 (blue). */
    ASSERT_INT_EQ(scene->primitives[1].material_index, 1);
    ASSERT_FLOAT_EQ(scene->materials[1].base_color[0], 0.0f);
    ASSERT_FLOAT_EQ(scene->materials[1].base_color[2], 1.0f);

    /* Second primitive positions differ from first. */
    ASSERT_VEC3_EQ(scene->primitives[1].vertices[0].position,
                   vec3_create(2.0f, 0.0f, 0.0f));

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: factor clamping ─────────────────────────────────────────────── */

static void test_material_factor_clamping(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("material factor clamping (out-of-range values)");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* metallicFactor > 1, roughnessFactor < 0,
     * occlusion strength > 1, alphaCutoff < 0,
     * negative emissive component */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"pbrMetallicRoughness\": {"
        "      \"metallicFactor\": 5.0,"
        "      \"roughnessFactor\": -2.0"
        "    },"
        "    \"occlusionTexture\": {\"index\": 0, \"strength\": 99.0},"
        "    \"alphaCutoff\": -1.0,"
        "    \"alphaMode\": \"MASK\","
        "    \"emissiveFactor\": [-0.5, 1.0, 0.0]"
        "  }],"
        "  \"textures\": [{\"source\": 0}],"
        "  \"images\": [{\"uri\": \"dummy.png\"}],"
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
        "  \"buffers\": [{\"uri\": \"test_clamp.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_clamp", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);

    /* metallicFactor 5.0 → clamped to 1.0 */
    ASSERT_FLOAT_EQ(scene->materials[0].metallic_factor, 1.0f);
    /* roughnessFactor -2.0 → clamped to 0.0 */
    ASSERT_FLOAT_EQ(scene->materials[0].roughness_factor, 0.0f);
    /* occlusion_strength 99.0 → clamped to 1.0 */
    ASSERT_FLOAT_EQ(scene->materials[0].occlusion_strength, 1.0f);
    /* alphaCutoff -1.0 → clamped to 0.0 */
    ASSERT_FLOAT_EQ(scene->materials[0].alpha_cutoff, 0.0f);
    /* emissiveFactor[0] -0.5 → clamped to 0.0 */
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[0], 0.0f);
    /* emissiveFactor[1] 1.0 → unchanged */
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[1], 1.0f);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: negative texture index ─────────────────────────────────────── */

static void test_material_negative_texture_index(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("material with negative texture index (should not crash)");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* baseColorTexture.index = -1 (malformed), normalTexture.index = -5 */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"pbrMetallicRoughness\": {"
        "      \"baseColorTexture\": {\"index\": -1}"
        "    },"
        "    \"normalTexture\": {\"index\": -5},"
        "    \"occlusionTexture\": {\"index\": -1},"
        "    \"emissiveTexture\": {\"index\": -1}"
        "  }],"
        "  \"textures\": [{\"source\": 0}],"
        "  \"images\": [{\"uri\": \"dummy.png\"}],"
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
        "  \"buffers\": [{\"uri\": \"test_negidx.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_negidx", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);

    /* Negative indices should result in has_* = false (no crash) */
    ASSERT_FALSE(scene->materials[0].has_texture);
    ASSERT_FALSE(scene->materials[0].has_normal_map);
    ASSERT_FALSE(scene->materials[0].has_occlusion);
    ASSERT_FALSE(scene->materials[0].has_emissive);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: negative texture source index ──────────────────────────────── */

static void test_material_negative_source_index(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("texture with negative source index (should not crash)");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* texture[0].source = -1 (malformed) */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"pbrMetallicRoughness\": {"
        "      \"baseColorTexture\": {\"index\": 0}"
        "    }"
        "  }],"
        "  \"textures\": [{\"source\": -1}],"
        "  \"images\": [{\"uri\": \"dummy.png\"}],"
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
        "  \"buffers\": [{\"uri\": \"test_negsrc.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_negsrc", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);

    /* Negative source index → has_texture = false */
    ASSERT_FALSE(scene->materials[0].has_texture);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Material: emissiveFactor with wrong element count ────────────────────── */

static void test_material_emissive_wrong_count(void)
{
    float positions[9];
    Uint16 indices[3];
    Uint8 bin_data[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }
    bool wrote;
    bool ok;

    TEST("material emissiveFactor with wrong element count");

    positions[0] = 0; positions[1] = 0; positions[2] = 0;
    positions[3] = 1; positions[4] = 0; positions[5] = 0;
    positions[6] = 0; positions[7] = 1; positions[8] = 0;
    indices[0] = 0; indices[1] = 1; indices[2] = 2;

    SDL_memcpy(bin_data, positions, 36);
    SDL_memcpy(bin_data + 36, indices, 6);

    /* emissiveFactor has only 2 elements instead of 3,
     * baseColorFactor has only 2 elements instead of 4 */
    json =
        "{"
        "  \"asset\": {\"version\": \"2.0\"},"
        "  \"scene\": 0,"
        "  \"scenes\": [{\"nodes\": [0]}],"
        "  \"nodes\": [{\"mesh\": 0}],"
        "  \"meshes\": [{\"primitives\": [{"
        "    \"attributes\": {\"POSITION\": 0},"
        "    \"indices\": 1, \"material\": 0"
        "  }]}],"
        "  \"materials\": [{"
        "    \"pbrMetallicRoughness\": {"
        "      \"baseColorFactor\": [0.5, 0.5]"
        "    },"
        "    \"emissiveFactor\": [1.0, 0.5]"
        "  }],"
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
        "  \"buffers\": [{\"uri\": \"test_emcnt.bin\", \"byteLength\": 42}]"
        "}";

    wrote = write_temp_gltf(json, bin_data, sizeof(bin_data),
                             "test_emcnt", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);

    ASSERT_TRUE(ok);
    ASSERT_INT_EQ(scene->material_count, 1);

    /* Wrong-sized arrays should be ignored; defaults preserved.
     * baseColorFactor[2] → not 4 elements, so default white (1,1,1,1) */
    ASSERT_FLOAT_EQ(scene->materials[0].base_color[0], 1.0f);
    ASSERT_FLOAT_EQ(scene->materials[0].base_color[3], 1.0f);

    /* emissiveFactor[2] → not 3 elements, so default black (0,0,0) */
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[0], 0.0f);
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[1], 0.0f);
    ASSERT_FLOAT_EQ(scene->materials[0].emissive_factor[2], 0.0f);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── CesiumMilkTruck (real-world model) ───────────────────────────────────── */

static void test_cesium_milk_truck(void)
{
    const char *base;
    char path[512];
    ForgeGltfScene *scene;
    bool ok;
    bool found_texture;
    int i;

    TEST("CesiumMilkTruck model (real-world glTF)");

    base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("    SKIP (SDL_GetBasePath failed)");
        test_passed++;
        return;
    }

    SDL_snprintf(path, sizeof(path),
                 "%sassets/CesiumMilkTruck/CesiumMilkTruck.gltf", base);

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }

    ok = forge_gltf_load(path, scene);
    if (!ok) {
        SDL_Log("    SKIP (model not found at %s)", path);
        SDL_free(scene);
        test_passed++;
        return;
    }

    ASSERT_INT_EQ(scene->node_count, 6);
    ASSERT_INT_EQ(scene->mesh_count, 2);
    ASSERT_INT_EQ(scene->material_count, 4);
    ASSERT_TRUE(scene->primitive_count >= 4);

    /* All primitives should have vertex + index data. */
    for (i = 0; i < scene->primitive_count; i++) {
        ASSERT_TRUE(scene->primitives[i].vertices != NULL);
        ASSERT_TRUE(scene->primitives[i].vertex_count > 0);
        ASSERT_TRUE(scene->primitives[i].indices != NULL);
        ASSERT_TRUE(scene->primitives[i].index_count > 0);
    }

    /* At least one material should have a texture. */
    found_texture = false;
    for (i = 0; i < scene->material_count; i++) {
        if (scene->materials[i].has_texture) {
            found_texture = true;
            break;
        }
    }
    ASSERT_TRUE(found_texture);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── CesiumMan: skin parsing ───────────────────────────────────────────────── */

static void test_cesiumman_skin(void)
{
    const char *base;
    char path[FORGE_GLTF_PATH_SIZE];
    ForgeGltfScene *scene;
    bool ok;
    mat4 identity;

    TEST("CesiumMan skin parsing (19 joints, skin data on mesh)");

    base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("    SKIP (SDL_GetBasePath failed)");
        test_passed++;
        return;
    }

    SDL_snprintf(path, sizeof(path),
                 "%sassets/CesiumMan/CesiumMan.gltf", base);

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }

    ok = forge_gltf_load(path, scene);
    if (!ok) {
        SDL_Log("    SKIP (model not found at %s)", path);
        SDL_free(scene);
        test_passed++;
        return;
    }

    /* Skin count and joint count. */
    ASSERT_INT_EQ(scene->skin_count, 1);
    ASSERT_INT_EQ(scene->skins[0].joint_count, 19);

    /* First joint is node 3 (Skeleton_torso_joint_1). */
    ASSERT_INT_EQ(scene->skins[0].joints[0], 3);

    /* Skeleton root is node 3. */
    ASSERT_INT_EQ(scene->skins[0].skeleton, 3);

    /* Inverse bind matrices should be loaded (first matrix != identity). */
    identity = mat4_identity();
    ASSERT_FALSE(SDL_memcmp(scene->skins[0].inverse_bind_matrices[0].m,
                            identity.m, sizeof(identity.m)) == 0);

    /* Node 2 ("Cesium_Man") has skin_index == 0. */
    ASSERT_TRUE(scene->node_count > 2);
    ASSERT_INT_EQ(scene->nodes[2].skin_index, 0);

    /* The mesh primitive should have skin data. */
    ASSERT_TRUE(scene->primitive_count >= 1);
    ASSERT_TRUE(scene->primitives[0].has_skin_data);
    ASSERT_TRUE(scene->primitives[0].joint_indices != NULL);
    ASSERT_TRUE(scene->primitives[0].weights != NULL);

    /* CesiumMan has 3273 skinned vertices. */
    ASSERT_INT_EQ((int)scene->primitives[0].vertex_count, 3273);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Animation tests
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Synthetic animation parsing ─────────────────────────────────────────── */
/* A minimal glTF with one animation: 3 translation keyframes on node 0.
 *
 * Binary layout:
 *   Bytes 0-35:   3 positions (VEC3 float × 3 verts)
 *   Bytes 36-41:  3 indices (uint16 × 3)
 *   Bytes 44-55:  3 timestamps (float × 3): 0.0, 0.5, 1.0
 *   Bytes 56-91:  3 translations (VEC3 float × 3): (0,0,0), (1,2,3), (2,4,6)
 *
 * 2 bytes padding after indices to align floats to 4 bytes. */

static void test_anim_parse_synthetic(void)
{
    /* Build binary data. */
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3]  = {0, 1, 2};
    Uint16 pad = 0;
    float timestamps[3] = {0.0f, 0.5f, 1.0f};
    float translations[9] = {0,0,0, 1,2,3, 2,4,6};

    /* Total: 36 + 6 + 2 + 12 + 36 = 92 bytes */
    Uint8 bin[92];
    SDL_memcpy(bin,      positions,    36);
    SDL_memcpy(bin + 36, indices,       6);
    SDL_memcpy(bin + 42, &pad,          2);
    SDL_memcpy(bin + 44, timestamps,   12);
    SDL_memcpy(bin + 56, translations, 36);

    const char *json =
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
        "     \"count\": 3, \"type\": \"SCALAR\"},"
        "    {\"bufferView\": 2, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"SCALAR\"},"
        "    {\"bufferView\": 3, \"componentType\": 5126,"
        "     \"count\": 3, \"type\": \"VEC3\"}"
        "  ],"
        "  \"bufferViews\": ["
        "    {\"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36},"
        "    {\"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6},"
        "    {\"buffer\": 0, \"byteOffset\": 44, \"byteLength\": 12},"
        "    {\"buffer\": 0, \"byteOffset\": 56, \"byteLength\": 36}"
        "  ],"
        "  \"buffers\": [{\"uri\": \"test_anim.bin\", \"byteLength\": 92}],"
        "  \"animations\": [{"
        "    \"name\": \"Move\","
        "    \"samplers\": [{"
        "      \"input\": 2, \"output\": 3,"
        "      \"interpolation\": \"LINEAR\""
        "    }],"
        "    \"channels\": [{"
        "      \"sampler\": 0,"
        "      \"target\": {\"node\": 0, \"path\": \"translation\"}"
        "    }]"
        "  }]"
        "}";

    TempGltf tg;
    ForgeGltfScene *scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }

    TEST("synthetic animation: parse count and metadata");

    bool wrote = write_temp_gltf(json, bin, sizeof(bin), "test_anim", &tg);
    ASSERT_TRUE(wrote);

    bool ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);
    ASSERT_TRUE(ok);

    ASSERT_INT_EQ(scene->animation_count, 1);
    ASSERT_TRUE(SDL_strcmp(scene->animations[0].name, "Move") == 0);
    ASSERT_INT_EQ(scene->animations[0].sampler_count, 1);
    ASSERT_INT_EQ(scene->animations[0].channel_count, 1);
    ASSERT_INT_EQ(scene->animations[0].samplers[0].keyframe_count, 3);
    ASSERT_INT_EQ(scene->animations[0].samplers[0].value_components, 3);
    ASSERT_INT_EQ((int)scene->animations[0].samplers[0].interpolation,
                  (int)FORGE_GLTF_INTERP_LINEAR);
    ASSERT_INT_EQ(scene->animations[0].channels[0].target_node, 0);
    ASSERT_INT_EQ((int)scene->animations[0].channels[0].target_path,
                  (int)FORGE_GLTF_ANIM_TRANSLATION);
    ASSERT_FLOAT_EQ(scene->animations[0].duration, 1.0f);

    /* Verify data pointers reference the correct values. */
    ASSERT_FLOAT_EQ(scene->animations[0].samplers[0].timestamps[0], 0.0f);
    ASSERT_FLOAT_EQ(scene->animations[0].samplers[0].timestamps[1], 0.5f);
    ASSERT_FLOAT_EQ(scene->animations[0].samplers[0].timestamps[2], 1.0f);
    ASSERT_FLOAT_EQ(scene->animations[0].samplers[0].values[0], 0.0f);
    ASSERT_FLOAT_EQ(scene->animations[0].samplers[0].values[3], 1.0f);
    ASSERT_FLOAT_EQ(scene->animations[0].samplers[0].values[4], 2.0f);
    ASSERT_FLOAT_EQ(scene->animations[0].samplers[0].values[5], 3.0f);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── CesiumMilkTruck animation parsing ───────────────────────────────────── */

static void test_truck_animation(void)
{
    const char *base;
    char path[FORGE_GLTF_PATH_SIZE];
    ForgeGltfScene *scene;
    bool ok;

    TEST("CesiumMilkTruck animation (2 rotation channels, 31 keyframes)");

    base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("    SKIP (SDL_GetBasePath failed)");
        test_passed++;
        return;
    }

    SDL_snprintf(path, sizeof(path),
                 "%sassets/CesiumMilkTruck/CesiumMilkTruck.gltf", base);

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }

    ok = forge_gltf_load(path, scene);
    if (!ok) {
        SDL_Log("    SKIP (model not found at %s)", path);
        SDL_free(scene);
        test_passed++;
        return;
    }

    ASSERT_INT_EQ(scene->animation_count, 1);
    ASSERT_TRUE(SDL_strcmp(scene->animations[0].name, "Wheels") == 0);
    ASSERT_INT_EQ(scene->animations[0].channel_count, 2);

    /* Both channels target rotation. */
    ASSERT_INT_EQ((int)scene->animations[0].channels[0].target_path,
                  (int)FORGE_GLTF_ANIM_ROTATION);
    ASSERT_INT_EQ((int)scene->animations[0].channels[1].target_path,
                  (int)FORGE_GLTF_ANIM_ROTATION);

    /* Both samplers should have 31 keyframes. */
    int si0 = scene->animations[0].channels[0].sampler_index;
    int si1 = scene->animations[0].channels[1].sampler_index;
    ASSERT_INT_EQ(scene->animations[0].samplers[si0].keyframe_count, 31);
    ASSERT_INT_EQ(scene->animations[0].samplers[si1].keyframe_count, 31);

    /* Wheel animation duration is 1.25 seconds (the 3.708s in L31 is the
     * path-following duration, not the glTF animation clip duration). */
    ASSERT_FLOAT_EQ(scene->animations[0].duration, 1.25f);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── CesiumMan animation parsing ─────────────────────────────────────────── */

static void test_cesiumman_animation(void)
{
    const char *base;
    char path[FORGE_GLTF_PATH_SIZE];
    ForgeGltfScene *scene;
    bool ok;

    TEST("CesiumMan animation (57 channels)");

    base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("    SKIP (SDL_GetBasePath failed)");
        test_passed++;
        return;
    }

    SDL_snprintf(path, sizeof(path),
                 "%sassets/CesiumMan/CesiumMan.gltf", base);

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }

    ok = forge_gltf_load(path, scene);
    if (!ok) {
        SDL_Log("    SKIP (model not found at %s)", path);
        SDL_free(scene);
        test_passed++;
        return;
    }

    ASSERT_INT_EQ(scene->animation_count, 1);
    ASSERT_INT_EQ(scene->animations[0].channel_count, 57);
    ASSERT_TRUE(scene->animations[0].duration > 0.0f);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
}

/* ── Evaluation: vec3 linear interpolation ───────────────────────────────── */

static void test_eval_vec3_linear(void)
{
    float ts[3]  = {0.0f, 0.5f, 1.0f};
    float vals[9] = {0,0,0, 2,4,6, 4,8,12};
    ForgeGltfAnimSampler samp;
    vec3 result;

    TEST("eval vec3 linear: start, midpoint, end, and clamp");

    SDL_memset(&samp, 0, sizeof(samp));
    samp.timestamps      = ts;
    samp.values           = vals;
    samp.keyframe_count   = 3;
    samp.value_components = 3;
    samp.interpolation    = FORGE_GLTF_INTERP_LINEAR;

    /* At start (t=0). */
    result = forge_gltf_anim_eval_vec3(&samp, 0.0f);
    ASSERT_VEC3_EQ(result, vec3_create(0, 0, 0));

    /* At end (t=1). */
    result = forge_gltf_anim_eval_vec3(&samp, 1.0f);
    ASSERT_VEC3_EQ(result, vec3_create(4, 8, 12));

    /* Midpoint of first interval (t=0.25). */
    result = forge_gltf_anim_eval_vec3(&samp, 0.25f);
    ASSERT_VEC3_EQ(result, vec3_create(1, 2, 3));

    /* Clamp before start. */
    result = forge_gltf_anim_eval_vec3(&samp, -1.0f);
    ASSERT_VEC3_EQ(result, vec3_create(0, 0, 0));

    /* Clamp after end. */
    result = forge_gltf_anim_eval_vec3(&samp, 5.0f);
    ASSERT_VEC3_EQ(result, vec3_create(4, 8, 12));

    END_TEST();
}

/* ── Evaluation: vec3 step interpolation ─────────────────────────────────── */

static void test_eval_vec3_step(void)
{
    float ts[3]  = {0.0f, 0.5f, 1.0f};
    float vals[9] = {1,1,1, 2,2,2, 3,3,3};
    ForgeGltfAnimSampler samp;
    vec3 result;

    TEST("eval vec3 step: holds previous value until next keyframe");

    SDL_memset(&samp, 0, sizeof(samp));
    samp.timestamps      = ts;
    samp.values           = vals;
    samp.keyframe_count   = 3;
    samp.value_components = 3;
    samp.interpolation    = FORGE_GLTF_INTERP_STEP;

    /* Between first and second keyframe — holds first value. */
    result = forge_gltf_anim_eval_vec3(&samp, 0.25f);
    ASSERT_VEC3_EQ(result, vec3_create(1, 1, 1));

    /* Between second and third keyframe — holds second value. */
    result = forge_gltf_anim_eval_vec3(&samp, 0.75f);
    ASSERT_VEC3_EQ(result, vec3_create(2, 2, 2));

    END_TEST();
}

/* ── Evaluation: quaternion linear (slerp) ───────────────────────────────── */

static void test_eval_quat_linear(void)
{
    /* glTF quaternion order: [x, y, z, w].
     * Two keyframes: identity (0,0,0,1) and 90° around Y (0,sin45,0,cos45). */
    float ts[2] = {0.0f, 1.0f};
    float sin45 = 0.70710678f;
    float cos45 = 0.70710678f;
    float vals[8] = {
        0.0f, 0.0f, 0.0f, 1.0f,       /* identity */
        0.0f, sin45, 0.0f, cos45       /* 90° around Y */
    };
    ForgeGltfAnimSampler samp;
    quat result;
    float len;

    TEST("eval quat slerp: midpoint produces 45-degree rotation, normalized");

    SDL_memset(&samp, 0, sizeof(samp));
    samp.timestamps      = ts;
    samp.values           = vals;
    samp.keyframe_count   = 2;
    samp.value_components = 4;
    samp.interpolation    = FORGE_GLTF_INTERP_LINEAR;

    /* At start — identity quaternion. */
    result = forge_gltf_anim_eval_quat(&samp, 0.0f);
    ASSERT_TRUE(quat_eq(result, quat_create(1, 0, 0, 0)));

    /* At end — 90° around Y. */
    result = forge_gltf_anim_eval_quat(&samp, 1.0f);
    ASSERT_TRUE(quat_eq(result, quat_create(cos45, 0, sin45, 0)));

    /* Midpoint — should be 45° around Y.
     * sin(22.5°) ≈ 0.3827, cos(22.5°) ≈ 0.9239. */
    result = forge_gltf_anim_eval_quat(&samp, 0.5f);
    len = SDL_sqrtf(result.w*result.w + result.x*result.x
                  + result.y*result.y + result.z*result.z);
    ASSERT_FLOAT_EQ(len, 1.0f);  /* normalized */
    ASSERT_TRUE(SDL_fabsf(result.y - 0.3827f) < 0.01f);
    ASSERT_TRUE(SDL_fabsf(result.w - 0.9239f) < 0.01f);

    END_TEST();
}

/* ── Evaluation: quaternion component order ──────────────────────────────── */

static void test_eval_quat_component_order(void)
{
    /* Verify glTF [x,y,z,w] → forge_math quat(w,x,y,z) conversion.
     * Store a quaternion (0.1, 0.2, 0.3, 0.9) in glTF order. */
    float ts[1] = {0.0f};
    float vals[4] = {0.1f, 0.2f, 0.3f, 0.9f}; /* glTF: x,y,z,w */
    ForgeGltfAnimSampler samp;
    quat result;

    TEST("eval quat component order: glTF [x,y,z,w] → forge_math (w,x,y,z)");

    SDL_memset(&samp, 0, sizeof(samp));
    samp.timestamps      = ts;
    samp.values           = vals;
    samp.keyframe_count   = 1;
    samp.value_components = 4;
    samp.interpolation    = FORGE_GLTF_INTERP_LINEAR;

    result = forge_gltf_anim_eval_quat(&samp, 0.0f);
    ASSERT_FLOAT_EQ(result.w, 0.9f);
    ASSERT_FLOAT_EQ(result.x, 0.1f);
    ASSERT_FLOAT_EQ(result.y, 0.2f);
    ASSERT_FLOAT_EQ(result.z, 0.3f);

    END_TEST();
}

/* ── Apply: translation channel updates node ─────────────────────────────── */

static void test_apply_translation(void)
{
    float ts[2]  = {0.0f, 1.0f};
    float vals[6] = {0,0,0, 3,6,9};
    ForgeGltfAnimation anim;
    ForgeGltfNode nodes[1];

    TEST("apply animation: translation channel updates node TRS");

    SDL_memset(&anim, 0, sizeof(anim));
    SDL_memset(nodes, 0, sizeof(nodes));

    /* Set up node with identity TRS. */
    nodes[0].translation = vec3_create(0, 0, 0);
    nodes[0].rotation    = quat_create(1, 0, 0, 0);
    nodes[0].scale_xyz   = vec3_create(1, 1, 1);

    /* Set up a single translation sampler + channel. */
    anim.samplers[0].timestamps      = ts;
    anim.samplers[0].values           = vals;
    anim.samplers[0].keyframe_count   = 2;
    anim.samplers[0].value_components = 3;
    anim.samplers[0].interpolation    = FORGE_GLTF_INTERP_LINEAR;
    anim.sampler_count = 1;

    anim.channels[0].target_node  = 0;
    anim.channels[0].target_path  = FORGE_GLTF_ANIM_TRANSLATION;
    anim.channels[0].sampler_index = 0;
    anim.channel_count = 1;

    anim.duration = 1.0f;
    SDL_strlcpy(anim.name, "Test", sizeof(anim.name));

    /* Apply at t=0.5 (midpoint). */
    forge_gltf_anim_apply(&anim, nodes, 1, 0.5f, false);

    ASSERT_VEC3_EQ(nodes[0].translation, vec3_create(1.5f, 3.0f, 4.5f));

    END_TEST();
}

/* ── Apply: loop wraps time ──────────────────────────────────────────────── */

static void test_apply_loop(void)
{
    float ts[2]  = {0.0f, 1.0f};
    float vals[6] = {0,0,0, 10,0,0};
    ForgeGltfAnimation anim;
    ForgeGltfNode nodes[1];

    TEST("apply animation: loop wraps time past duration");

    SDL_memset(&anim, 0, sizeof(anim));
    SDL_memset(nodes, 0, sizeof(nodes));

    nodes[0].translation = vec3_create(0, 0, 0);
    nodes[0].rotation    = quat_create(1, 0, 0, 0);
    nodes[0].scale_xyz   = vec3_create(1, 1, 1);

    anim.samplers[0].timestamps      = ts;
    anim.samplers[0].values           = vals;
    anim.samplers[0].keyframe_count   = 2;
    anim.samplers[0].value_components = 3;
    anim.samplers[0].interpolation    = FORGE_GLTF_INTERP_LINEAR;
    anim.sampler_count = 1;

    anim.channels[0].target_node  = 0;
    anim.channels[0].target_path  = FORGE_GLTF_ANIM_TRANSLATION;
    anim.channels[0].sampler_index = 0;
    anim.channel_count = 1;

    anim.duration = 1.0f;
    SDL_strlcpy(anim.name, "Loop", sizeof(anim.name));

    /* t=1.5 with loop → wraps to 0.5 → translation = (5,0,0). */
    forge_gltf_anim_apply(&anim, nodes, 1, 1.5f, true);

    ASSERT_VEC3_EQ(nodes[0].translation, vec3_create(5.0f, 0.0f, 0.0f));

    END_TEST();
}

/* ── Apply: clamp at duration ────────────────────────────────────────────── */

static void test_apply_clamp(void)
{
    float ts[2]  = {0.0f, 1.0f};
    float vals[6] = {0,0,0, 10,0,0};
    ForgeGltfAnimation anim;
    ForgeGltfNode nodes[1];

    TEST("apply animation: clamp holds last value past duration");

    SDL_memset(&anim, 0, sizeof(anim));
    SDL_memset(nodes, 0, sizeof(nodes));

    nodes[0].translation = vec3_create(0, 0, 0);
    nodes[0].rotation    = quat_create(1, 0, 0, 0);
    nodes[0].scale_xyz   = vec3_create(1, 1, 1);

    anim.samplers[0].timestamps      = ts;
    anim.samplers[0].values           = vals;
    anim.samplers[0].keyframe_count   = 2;
    anim.samplers[0].value_components = 3;
    anim.samplers[0].interpolation    = FORGE_GLTF_INTERP_LINEAR;
    anim.sampler_count = 1;

    anim.channels[0].target_node  = 0;
    anim.channels[0].target_path  = FORGE_GLTF_ANIM_TRANSLATION;
    anim.channels[0].sampler_index = 0;
    anim.channel_count = 1;

    anim.duration = 1.0f;
    SDL_strlcpy(anim.name, "Clamp", sizeof(anim.name));

    /* t=5.0 without loop → clamps to 1.0 → translation = (10,0,0). */
    forge_gltf_anim_apply(&anim, nodes, 1, 5.0f, false);

    ASSERT_VEC3_EQ(nodes[0].translation, vec3_create(10.0f, 0.0f, 0.0f));

    END_TEST();
}

/* ── Evaluation: quaternion step interpolation ────────────────────────────── */

static void test_eval_quat_step(void)
{
    /* Two keyframes: identity and 90° around Y.
     * glTF order: [x, y, z, w]. */
    float ts[2] = {0.0f, 1.0f};
    float sin45 = 0.70710678f;
    float cos45 = 0.70710678f;
    float vals[8] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, sin45, 0.0f, cos45
    };
    ForgeGltfAnimSampler samp;
    quat result;

    TEST("eval quat step: holds previous quaternion until next keyframe");

    SDL_memset(&samp, 0, sizeof(samp));
    samp.timestamps      = ts;
    samp.values           = vals;
    samp.keyframe_count   = 2;
    samp.value_components = 4;
    samp.interpolation    = FORGE_GLTF_INTERP_STEP;

    /* At t=0.5 (between keyframes) — should hold the identity quaternion. */
    result = forge_gltf_anim_eval_quat(&samp, 0.5f);
    ASSERT_TRUE(quat_eq(result, quat_create(1, 0, 0, 0)));

    END_TEST();
}

/* ── Apply: rotation channel updates node ────────────────────────────────── */

static void test_apply_rotation(void)
{
    /* Single rotation keyframe at t=0: 90° around Y.
     * glTF order: [x, y, z, w]. */
    float ts[1]  = {0.0f};
    float sin45 = 0.70710678f;
    float cos45 = 0.70710678f;
    float vals[4] = {0.0f, sin45, 0.0f, cos45};
    ForgeGltfAnimation anim;
    ForgeGltfNode nodes[1];

    TEST("apply animation: rotation channel updates node quaternion");

    SDL_memset(&anim, 0, sizeof(anim));
    SDL_memset(nodes, 0, sizeof(nodes));

    nodes[0].translation = vec3_create(0, 0, 0);
    nodes[0].rotation    = quat_create(1, 0, 0, 0);
    nodes[0].scale_xyz   = vec3_create(1, 1, 1);

    anim.samplers[0].timestamps      = ts;
    anim.samplers[0].values           = vals;
    anim.samplers[0].keyframe_count   = 1;
    anim.samplers[0].value_components = 4;
    anim.samplers[0].interpolation    = FORGE_GLTF_INTERP_LINEAR;
    anim.sampler_count = 1;

    anim.channels[0].target_node  = 0;
    anim.channels[0].target_path  = FORGE_GLTF_ANIM_ROTATION;
    anim.channels[0].sampler_index = 0;
    anim.channel_count = 1;

    anim.duration = 0.0f;
    SDL_strlcpy(anim.name, "Rot", sizeof(anim.name));

    forge_gltf_anim_apply(&anim, nodes, 1, 0.0f, false);

    /* quat_create(w, x, y, z) — expect 90° Y rotation. */
    ASSERT_TRUE(quat_eq(nodes[0].rotation,
                         quat_create(cos45, 0, sin45, 0)));

    END_TEST();
}

/* ── Apply: scale channel updates node ───────────────────────────────────── */

static void test_apply_scale(void)
{
    float ts[2]  = {0.0f, 1.0f};
    float vals[6] = {1,1,1, 2,3,4};
    ForgeGltfAnimation anim;
    ForgeGltfNode nodes[1];

    TEST("apply animation: scale channel updates node scale_xyz");

    SDL_memset(&anim, 0, sizeof(anim));
    SDL_memset(nodes, 0, sizeof(nodes));

    nodes[0].translation = vec3_create(0, 0, 0);
    nodes[0].rotation    = quat_create(1, 0, 0, 0);
    nodes[0].scale_xyz   = vec3_create(1, 1, 1);

    anim.samplers[0].timestamps      = ts;
    anim.samplers[0].values           = vals;
    anim.samplers[0].keyframe_count   = 2;
    anim.samplers[0].value_components = 3;
    anim.samplers[0].interpolation    = FORGE_GLTF_INTERP_LINEAR;
    anim.sampler_count = 1;

    anim.channels[0].target_node  = 0;
    anim.channels[0].target_path  = FORGE_GLTF_ANIM_SCALE;
    anim.channels[0].sampler_index = 0;
    anim.channel_count = 1;

    anim.duration = 1.0f;
    SDL_strlcpy(anim.name, "Scale", sizeof(anim.name));

    /* At t=1.0 — scale should be (2,3,4). */
    forge_gltf_anim_apply(&anim, nodes, 1, 1.0f, false);

    ASSERT_VEC3_EQ(nodes[0].scale_xyz, vec3_create(2.0f, 3.0f, 4.0f));

    END_TEST();
}

/* ── Apply: out-of-range target_node is skipped gracefully ───────────────── */

static void test_apply_bad_target(void)
{
    float ts[1]  = {0.0f};
    float vals[3] = {5, 5, 5};
    ForgeGltfAnimation anim;
    ForgeGltfNode nodes[1];

    TEST("apply animation: out-of-range target_node is skipped");

    SDL_memset(&anim, 0, sizeof(anim));
    SDL_memset(nodes, 0, sizeof(nodes));

    nodes[0].translation = vec3_create(0, 0, 0);
    nodes[0].rotation    = quat_create(1, 0, 0, 0);
    nodes[0].scale_xyz   = vec3_create(1, 1, 1);

    anim.samplers[0].timestamps      = ts;
    anim.samplers[0].values           = vals;
    anim.samplers[0].keyframe_count   = 1;
    anim.samplers[0].value_components = 3;
    anim.samplers[0].interpolation    = FORGE_GLTF_INTERP_LINEAR;
    anim.sampler_count = 1;

    /* Channel targets node 99, but we only have 1 node. */
    anim.channels[0].target_node  = 99;
    anim.channels[0].target_path  = FORGE_GLTF_ANIM_TRANSLATION;
    anim.channels[0].sampler_index = 0;
    anim.channel_count = 1;

    anim.duration = 0.0f;
    SDL_strlcpy(anim.name, "Bad", sizeof(anim.name));

    /* Should not crash, and node should be unmodified. */
    forge_gltf_anim_apply(&anim, nodes, 1, 0.0f, false);

    ASSERT_VEC3_EQ(nodes[0].translation, vec3_create(0, 0, 0));

    END_TEST();
}

/* ── No animations in file ───────────────────────────────────────────────── */

static void test_no_animations(void)
{
    float positions[9] = {0,0,0, 1,0,0, 0,1,0};
    Uint16 indices[3]  = {0, 1, 2};
    Uint8 bin[42];
    const char *json;
    TempGltf tg;
    ForgeGltfScene *scene;
    bool wrote;
    bool ok;

    TEST("glTF without animations: animation_count == 0");

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
        "  \"buffers\": [{\"uri\": \"test_noanim.bin\", \"byteLength\": 42}]"
        "}";

    scene = SDL_calloc(1, sizeof(*scene));
    if (!scene) {
        SDL_Log("  FAIL: SDL_calloc failed for ForgeGltfScene");
        test_failed++;
        return;
    }

    wrote = write_temp_gltf(json, bin, sizeof(bin), "test_noanim", &tg);
    ASSERT_TRUE(wrote);

    ok = forge_gltf_load(tg.gltf_path, scene);
    remove_temp_gltf(&tg);
    ASSERT_TRUE(ok);

    ASSERT_INT_EQ(scene->animation_count, 0);

    forge_gltf_free(scene);
    SDL_free(scene);
    END_TEST();
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

    SDL_Log("=== glTF Parser Tests ===\n");

    /* Error handling */
    test_nonexistent_file();
    test_invalid_json();
    test_free_zeroed_scene();

    /* Accessor validation */
    test_invalid_component_type();
    test_accessor_misaligned();
    test_accessor_exceeds_buffer_view();
    test_buffer_view_exceeds_buffer();
    test_missing_buffer_view_byte_length();

    /* Basic parsing */
    test_minimal_triangle();
    test_normals_and_uvs();

    /* Accessor validation (normals/UVs) */
    test_normal_count_mismatch();
    test_uv_wrong_component_type();

    /* Materials */
    test_material_base_color();
    test_material_texture_path();
    test_material_pbr_fields();
    test_material_extended_fields();
    test_material_defaults();
    test_material_mr_texture();
    test_material_factor_clamping();
    test_material_negative_texture_index();
    test_material_negative_source_index();
    test_material_emissive_wrong_count();

    /* Transforms */
    test_node_hierarchy();
    test_quaternion_rotation();
    test_scale_transform();
    test_node_explicit_matrix();
    test_combined_trs();

    /* Multi-primitive */
    test_multiple_primitives();

    /* Real model */
    test_cesium_milk_truck();

    /* Skin parsing */
    test_cesiumman_skin();

    /* Animation parsing */
    test_no_animations();
    test_anim_parse_synthetic();
    test_truck_animation();
    test_cesiumman_animation();

    /* Animation evaluation */
    test_eval_vec3_linear();
    test_eval_vec3_step();
    test_eval_quat_linear();
    test_eval_quat_component_order();
    test_eval_quat_step();

    /* Animation apply */
    test_apply_translation();
    test_apply_rotation();
    test_apply_scale();
    test_apply_loop();
    test_apply_clamp();
    test_apply_bad_target();

    /* Summary */
    SDL_Log("\n=== Test Summary ===");
    SDL_Log("Total:  %d", test_count);
    SDL_Log("Passed: %d", test_passed);
    SDL_Log("Failed: %d", test_failed);

    if (test_failed > 0) {
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "\nSome tests FAILED!");
        SDL_Quit();
        return 1;
    }

    SDL_Log("\nAll tests PASSED!");
    SDL_Quit();
    return 0;
}
