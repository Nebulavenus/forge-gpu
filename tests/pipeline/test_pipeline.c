/*
 * Pipeline Library Tests
 *
 * Automated tests for common/pipeline/forge_pipeline.h
 * Verifies correctness of the .fmesh binary loader, free functions,
 * and utility accessors.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define EPSILON 0.0001f

static bool float_eq(float a, float b)
{
    return SDL_fabsf(a - b) < EPSILON;
}

#define TEST(name) \
    do { \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_TRUE(expr) \
    if (!(expr)) { \
        SDL_Log("    FAIL: Assertion failed: %s", #expr); \
        fail_count++; \
        return; \
    }

#define ASSERT_INT_EQ(a, b) \
    if ((a) != (b)) { \
        SDL_Log("    FAIL: Expected %d, got %d", (int)(b), (int)(a)); \
        fail_count++; \
        return; \
    }

#define ASSERT_UINT_EQ(a, b) \
    if ((a) != (b)) { \
        SDL_Log("    FAIL: Expected %u, got %u", (unsigned)(b), (unsigned)(a)); \
        fail_count++; \
        return; \
    }

#define ASSERT_FLOAT_EQ(a, b) \
    if (!float_eq(a, b)) { \
        SDL_Log("    FAIL: Expected %.6f, got %.6f", (double)(b), (double)(a)); \
        fail_count++; \
        return; \
    }

#define ASSERT_NULL(ptr) \
    if ((ptr) != NULL) { \
        SDL_Log("    FAIL: Expected NULL"); \
        fail_count++; \
        return; \
    }

#define ASSERT_NOT_NULL(ptr) \
    if ((ptr) == NULL) { \
        SDL_Log("    FAIL: Expected non-NULL"); \
        fail_count++; \
        return; \
    }

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

/* ── Platform-neutral temp directory ──────────────────────────────────────
 *
 * SDL_GetPrefPath returns a writable per-app directory on all platforms.
 * We use it to avoid hard-coded /tmp/ paths that fail on Windows. */
static char temp_dir[512];

static void init_temp_dir(void)
{
    /* SDL_GetPrefPath returns a path with trailing separator.
     * Use a test-specific org/app so we don't collide with real data. */
    char *pref = SDL_GetPrefPath("forge-gpu-test", "pipeline");
    if (pref) {
        SDL_snprintf(temp_dir, sizeof(temp_dir), "%s", pref);
        SDL_free(pref);
    } else {
        /* Fallback for headless environments without a home directory */
        SDL_snprintf(temp_dir, sizeof(temp_dir), "%s",
                     SDL_getenv("TMPDIR") ? SDL_getenv("TMPDIR") :
                     SDL_getenv("TMP")    ? SDL_getenv("TMP")    :
                     SDL_getenv("TEMP")   ? SDL_getenv("TEMP")   : "/tmp/");
    }
}

/* Build a full temp file path from a filename.
 * Caller provides the output buffer so multiple paths can coexist. */
static void temp_path(char *out, size_t out_size, const char *filename)
{
    SDL_snprintf(out, out_size, "%s%s", temp_dir, filename);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Helper: write a minimal .fmesh file for testing
 *
 * Creates a simple triangle (3 vertices, variable LOD count).
 * All values are little-endian (native on x86/ARM).
 * ══════════════════════════════════════════════════════════════════════════ */

/* Write a uint32 to a buffer at the given offset */
static void write_u32(uint8_t *buf, size_t offset, uint32_t value)
{
    memcpy(buf + offset, &value, sizeof(value));
}

/* Write a float to a buffer at the given offset */
static void write_f32(uint8_t *buf, size_t offset, float value)
{
    memcpy(buf + offset, &value, sizeof(value));
}

/* Write a minimal .fmesh file to disk.
 * Returns true on success. Caller provides the path.
 *
 * with_tangents: if true, stride = 48 and tangent data is written
 * lod_count:     number of LOD levels (each gets 3 indices for the triangle)
 */
static bool write_test_fmesh(const char *path, bool with_tangents, int lod_count)
{
    uint32_t vertex_count = 3;
    uint32_t stride = with_tangents ? 48 : 32;
    uint32_t flags  = with_tangents ? FORGE_PIPELINE_FLAG_TANGENTS : 0;
    uint32_t indices_per_lod = 3;
    uint32_t total_indices = (uint32_t)lod_count * indices_per_lod;

    /* Compute sizes — use the library's own constants */
    size_t header_size     = FORGE_PIPELINE_HEADER_SIZE;
    size_t lod_section     = (size_t)lod_count * FORGE_PIPELINE_LOD_ENTRY_SIZE;
    size_t vertex_data_size = (size_t)vertex_count * stride;
    size_t index_data_size  = (size_t)total_indices * sizeof(uint32_t);
    size_t total_size       = header_size + lod_section + vertex_data_size + index_data_size;

    uint8_t *buf = (uint8_t *)SDL_calloc(1, total_size);
    if (!buf) return false;

    /* Header (32 bytes) */
    memcpy(buf, "FMSH", 4);                  /* magic */
    write_u32(buf, 4,  1);                    /* version */
    write_u32(buf, 8,  vertex_count);         /* vertex_count */
    write_u32(buf, 12, stride);               /* vertex_stride */
    write_u32(buf, 16, (uint32_t)lod_count);  /* lod_count */
    write_u32(buf, 20, flags);                /* flags */
    /* bytes 24-31: reserved (already zero from calloc) */

    /* LOD entries */
    for (int i = 0; i < lod_count; i++) {
        size_t off = header_size + (size_t)i * FORGE_PIPELINE_LOD_ENTRY_SIZE;
        write_u32(buf, off + 0, indices_per_lod);                         /* index_count */
        write_u32(buf, off + 4, (uint32_t)(i * indices_per_lod * 4));     /* index_offset (bytes) */
        write_f32(buf, off + 8, (float)i * 0.01f);                        /* target_error */
    }

    /* Vertex data */
    size_t vdata_off = header_size + lod_section;

    /* Vertex 0: pos(0,0,0) norm(0,0,1) uv(0,0) */
    write_f32(buf, vdata_off + 0,  0.0f);  /* pos.x */
    write_f32(buf, vdata_off + 4,  0.0f);  /* pos.y */
    write_f32(buf, vdata_off + 8,  0.0f);  /* pos.z */
    write_f32(buf, vdata_off + 12, 0.0f);  /* norm.x */
    write_f32(buf, vdata_off + 16, 0.0f);  /* norm.y */
    write_f32(buf, vdata_off + 20, 1.0f);  /* norm.z */
    write_f32(buf, vdata_off + 24, 0.0f);  /* uv.u */
    write_f32(buf, vdata_off + 28, 0.0f);  /* uv.v */
    if (with_tangents) {
        write_f32(buf, vdata_off + 32, 1.0f);  /* tan.x */
        write_f32(buf, vdata_off + 36, 0.0f);  /* tan.y */
        write_f32(buf, vdata_off + 40, 0.0f);  /* tan.z */
        write_f32(buf, vdata_off + 44, 1.0f);  /* tan.w */
    }

    /* Vertex 1: pos(1,0,0) norm(0,0,1) uv(1,0) */
    size_t v1 = vdata_off + stride;
    write_f32(buf, v1 + 0,  1.0f);
    write_f32(buf, v1 + 4,  0.0f);
    write_f32(buf, v1 + 8,  0.0f);
    write_f32(buf, v1 + 12, 0.0f);
    write_f32(buf, v1 + 16, 0.0f);
    write_f32(buf, v1 + 20, 1.0f);
    write_f32(buf, v1 + 24, 1.0f);
    write_f32(buf, v1 + 28, 0.0f);
    if (with_tangents) {
        write_f32(buf, v1 + 32, 1.0f);
        write_f32(buf, v1 + 36, 0.0f);
        write_f32(buf, v1 + 40, 0.0f);
        write_f32(buf, v1 + 44, 1.0f);
    }

    /* Vertex 2: pos(0,1,0) norm(0,0,1) uv(0,1) */
    size_t v2 = vdata_off + 2 * stride;
    write_f32(buf, v2 + 0,  0.0f);
    write_f32(buf, v2 + 4,  1.0f);
    write_f32(buf, v2 + 8,  0.0f);
    write_f32(buf, v2 + 12, 0.0f);
    write_f32(buf, v2 + 16, 0.0f);
    write_f32(buf, v2 + 20, 1.0f);
    write_f32(buf, v2 + 24, 0.0f);
    write_f32(buf, v2 + 28, 1.0f);
    if (with_tangents) {
        write_f32(buf, v2 + 32, 1.0f);
        write_f32(buf, v2 + 36, 0.0f);
        write_f32(buf, v2 + 40, 0.0f);
        write_f32(buf, v2 + 44, 1.0f);
    }

    /* Index data: (0, 1, 2) repeated for each LOD */
    size_t idata_off = vdata_off + vertex_data_size;
    for (int i = 0; i < lod_count; i++) {
        size_t base = idata_off + (size_t)i * indices_per_lod * sizeof(uint32_t);
        write_u32(buf, base + 0,  0);
        write_u32(buf, base + 4,  1);
        write_u32(buf, base + 8,  2);
    }

    /* Write to file */
    bool ok = SDL_SaveFile(path, buf, total_size);
    if (!ok) {
        SDL_Log("write_test_fmesh: SDL_SaveFile failed: %s", SDL_GetError());
    }
    SDL_free(buf);
    return ok;
}

/* Write a broken .fmesh with custom header fields for error testing */
static bool write_broken_fmesh(const char *path,
                                const char *magic, uint32_t version,
                                uint32_t vertex_count, uint32_t stride,
                                uint32_t lod_count, uint32_t flags,
                                size_t file_size)
{
    uint8_t *buf = (uint8_t *)SDL_calloc(1, file_size);
    if (!buf) return false;

    /* Write whatever fits into the buffer */
    size_t magic_len = strlen(magic);
    if (magic_len > 4) magic_len = 4;
    if (file_size >= 4)  memcpy(buf, magic, magic_len);
    if (file_size >= 8)  write_u32(buf, 4,  version);
    if (file_size >= 12) write_u32(buf, 8,  vertex_count);
    if (file_size >= 16) write_u32(buf, 12, stride);
    if (file_size >= 20) write_u32(buf, 16, lod_count);
    if (file_size >= 24) write_u32(buf, 20, flags);

    bool ok = SDL_SaveFile(path, buf, file_size);
    if (!ok) {
        SDL_Log("write_broken_fmesh: SDL_SaveFile failed for '%s': %s",
                path, SDL_GetError());
    }
    SDL_free(buf);
    return ok;
}

/* Remove a temp file (best-effort cleanup) */
static void cleanup_file(const char *path)
{
    /* SDL doesn't have a remove function; use stdio */
    remove(path);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Mesh Loading — Valid Files (tests 1-6)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_load_mesh_no_tangents(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_no_tan.fmesh");
    TEST("load_mesh_no_tangents");

    ASSERT_TRUE(write_test_fmesh(path, false, 1));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    ASSERT_UINT_EQ(mesh.vertex_count, 3);
    ASSERT_UINT_EQ(mesh.vertex_stride, 32);
    ASSERT_UINT_EQ(mesh.lod_count, 1);
    ASSERT_TRUE((mesh.flags & FORGE_PIPELINE_FLAG_TANGENTS) == 0);
    ASSERT_NOT_NULL(mesh.vertices);
    ASSERT_NOT_NULL(mesh.indices);

    /* Verify index values */
    ASSERT_UINT_EQ(mesh.indices[0], 0);
    ASSERT_UINT_EQ(mesh.indices[1], 1);
    ASSERT_UINT_EQ(mesh.indices[2], 2);

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_with_tangents(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_tan.fmesh");
    TEST("load_mesh_with_tangents");

    ASSERT_TRUE(write_test_fmesh(path, true, 1));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    ASSERT_UINT_EQ(mesh.vertex_count, 3);
    ASSERT_UINT_EQ(mesh.vertex_stride, 48);
    ASSERT_UINT_EQ(mesh.lod_count, 1);
    ASSERT_TRUE(forge_pipeline_has_tangents(&mesh));

    /* Verify tangent data for vertex 0: tangent(1,0,0,1) */
    const float *v0 = (const float *)mesh.vertices;
    /* Tangent starts at float index 8 (after pos[3]+norm[3]+uv[2]) */
    ASSERT_FLOAT_EQ(v0[8],  1.0f);  /* tan.x */
    ASSERT_FLOAT_EQ(v0[9],  0.0f);  /* tan.y */
    ASSERT_FLOAT_EQ(v0[10], 0.0f);  /* tan.z */
    ASSERT_FLOAT_EQ(v0[11], 1.0f);  /* tan.w */

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_multiple_lods(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_multi_lod.fmesh");
    TEST("load_mesh_multiple_lods");

    ASSERT_TRUE(write_test_fmesh(path, false, 3));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    ASSERT_UINT_EQ(mesh.lod_count, 3);
    ASSERT_NOT_NULL(mesh.lods);

    /* Each LOD has 3 indices */
    for (uint32_t i = 0; i < mesh.lod_count; i++) {
        ASSERT_UINT_EQ(mesh.lods[i].index_count, 3);
    }

    /* Verify target_error values */
    ASSERT_FLOAT_EQ(mesh.lods[0].target_error, 0.0f);
    ASSERT_FLOAT_EQ(mesh.lods[1].target_error, 0.01f);
    ASSERT_FLOAT_EQ(mesh.lods[2].target_error, 0.02f);

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_mesh_lod_accessor(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_lod_acc.fmesh");
    TEST("mesh_lod_accessor");

    ASSERT_TRUE(write_test_fmesh(path, false, 3));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    /* forge_pipeline_lod_index_count should return 3 for each LOD */
    ASSERT_UINT_EQ(forge_pipeline_lod_index_count(&mesh, 0), 3);
    ASSERT_UINT_EQ(forge_pipeline_lod_index_count(&mesh, 1), 3);
    ASSERT_UINT_EQ(forge_pipeline_lod_index_count(&mesh, 2), 3);

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_mesh_lod_indices(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_lod_idx.fmesh");
    TEST("mesh_lod_indices");

    ASSERT_TRUE(write_test_fmesh(path, false, 3));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    /* Each LOD's indices should point to (0, 1, 2) */
    for (uint32_t lod = 0; lod < mesh.lod_count; lod++) {
        const uint32_t *idx = forge_pipeline_lod_indices(&mesh, lod);
        ASSERT_NOT_NULL(idx);
        ASSERT_UINT_EQ(idx[0], 0);
        ASSERT_UINT_EQ(idx[1], 1);
        ASSERT_UINT_EQ(idx[2], 2);
    }

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_mesh_vertex_data_integrity(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_vdata.fmesh");
    TEST("mesh_vertex_data_integrity");

    ASSERT_TRUE(write_test_fmesh(path, false, 1));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    const float *v = (const float *)mesh.vertices;
    /* floats_per_vertex = stride / sizeof(float) = 32 / 4 = 8 */
    int fpv = (int)(mesh.vertex_stride / sizeof(float));

    /* Vertex 0: pos(0,0,0) norm(0,0,1) uv(0,0) */
    ASSERT_FLOAT_EQ(v[0 * fpv + 0], 0.0f);  /* pos.x */
    ASSERT_FLOAT_EQ(v[0 * fpv + 1], 0.0f);  /* pos.y */
    ASSERT_FLOAT_EQ(v[0 * fpv + 2], 0.0f);  /* pos.z */
    ASSERT_FLOAT_EQ(v[0 * fpv + 3], 0.0f);  /* norm.x */
    ASSERT_FLOAT_EQ(v[0 * fpv + 4], 0.0f);  /* norm.y */
    ASSERT_FLOAT_EQ(v[0 * fpv + 5], 1.0f);  /* norm.z */
    ASSERT_FLOAT_EQ(v[0 * fpv + 6], 0.0f);  /* uv.u */
    ASSERT_FLOAT_EQ(v[0 * fpv + 7], 0.0f);  /* uv.v */

    /* Vertex 1: pos(1,0,0) norm(0,0,1) uv(1,0) */
    ASSERT_FLOAT_EQ(v[1 * fpv + 0], 1.0f);
    ASSERT_FLOAT_EQ(v[1 * fpv + 1], 0.0f);
    ASSERT_FLOAT_EQ(v[1 * fpv + 2], 0.0f);
    ASSERT_FLOAT_EQ(v[1 * fpv + 6], 1.0f);
    ASSERT_FLOAT_EQ(v[1 * fpv + 7], 0.0f);

    /* Vertex 2: pos(0,1,0) norm(0,0,1) uv(0,1) */
    ASSERT_FLOAT_EQ(v[2 * fpv + 0], 0.0f);
    ASSERT_FLOAT_EQ(v[2 * fpv + 1], 1.0f);
    ASSERT_FLOAT_EQ(v[2 * fpv + 2], 0.0f);
    ASSERT_FLOAT_EQ(v[2 * fpv + 6], 0.0f);
    ASSERT_FLOAT_EQ(v[2 * fpv + 7], 1.0f);

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Mesh Loading — Error Cases (tests 7-15)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_load_mesh_null_path(void)
{
    TEST("load_mesh_null_path");
    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(NULL, &mesh));
    END_TEST();
}

static void test_load_mesh_null_mesh(void)
{
    char path[512];
    temp_path(path, sizeof(path), "whatever.fmesh");
    TEST("load_mesh_null_mesh");
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, NULL));
    END_TEST();
}

static void test_load_mesh_nonexistent(void)
{
    char path[512];
    temp_path(path, sizeof(path), "nonexistent_xyz_42.fmesh");
    TEST("load_mesh_nonexistent");
    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));
    END_TEST();
}

static void test_load_mesh_invalid_magic(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_bad_magic.fmesh");
    TEST("load_mesh_invalid_magic");

    ASSERT_TRUE(write_broken_fmesh(path, "NOPE", 1, 3, 32, 1, 0, 128));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_invalid_version(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_bad_ver.fmesh");
    TEST("load_mesh_invalid_version");

    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 99, 3, 32, 1, 0, 128));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_invalid_stride(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_bad_stride.fmesh");
    TEST("load_mesh_invalid_stride");

    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 1, 3, 64, 1, 0, 128));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_truncated_header(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_trunc_hdr.fmesh");
    TEST("load_mesh_truncated_header");

    /* Write only 16 bytes — less than the 32-byte header */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 1, 3, 32, 1, 0, 16));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_truncated_data(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_trunc_data.fmesh");
    TEST("load_mesh_truncated_data");

    /* Write a valid header with 3 vertices, stride 32, 1 LOD,
     * but only 48 bytes total — not enough for LOD entry + vertex data.
     * Need: 32 (header) + 12 (1 LOD) + 96 (3*32 vertices) + 12 (3 indices)
     *     = 152 bytes minimum.  We provide only 48. */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 1, 3, 32, 1, 0, 48));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_too_many_lods(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_too_many_lods.fmesh");
    TEST("load_mesh_too_many_lods");

    /* lod_count = 99, exceeds FORGE_PIPELINE_MAX_LODS (8) */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 1, 3, 32, 99, 0, 128));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_stride_tan_without_flag(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_stride_noflag.fmesh");
    TEST("load_mesh_stride_tan_without_flag");

    /* stride = 48 (tangent stride) but flags = 0 (no TANGENTS flag) */
    ASSERT_TRUE(write_broken_fmesh(path, "FMSH", 1, 3, 48, 1, 0, 256));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

static void test_load_mesh_invalid_lod_offset(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_bad_lod_off.fmesh");
    TEST("load_mesh_invalid_lod_offset");

    /* Write a valid-looking file but with a bad LOD index_offset.
     * We build a 1-LOD file manually with index_offset = 99 instead of 0. */
    uint32_t vertex_count = 3;
    uint32_t stride = 32;
    uint32_t lod_count = 1;
    uint32_t indices_per_lod = 3;

    size_t header_size     = FORGE_PIPELINE_HEADER_SIZE;
    size_t lod_section     = (size_t)lod_count * FORGE_PIPELINE_LOD_ENTRY_SIZE;
    size_t vertex_data_size = (size_t)vertex_count * stride;
    size_t index_data_size  = (size_t)indices_per_lod * sizeof(uint32_t);
    size_t total_size       = header_size + lod_section + vertex_data_size + index_data_size;

    uint8_t *buf = (uint8_t *)SDL_calloc(1, total_size);
    ASSERT_NOT_NULL(buf);

    /* Header */
    memcpy(buf, "FMSH", 4);
    write_u32(buf, 4,  1);             /* version */
    write_u32(buf, 8,  vertex_count);
    write_u32(buf, 12, stride);
    write_u32(buf, 16, lod_count);
    write_u32(buf, 20, 0);             /* flags */

    /* LOD entry with bad offset (should be 0, set to 99) */
    write_u32(buf, header_size + 0, indices_per_lod);  /* index_count */
    write_u32(buf, header_size + 4, 99);               /* index_offset (INVALID) */
    write_f32(buf, header_size + 8, 0.0f);             /* target_error */

    ASSERT_TRUE(SDL_SaveFile(path, buf, total_size));
    SDL_free(buf);

    ForgePipelineMesh mesh;
    ASSERT_TRUE(!forge_pipeline_load_mesh(path, &mesh));

    cleanup_file(path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Mesh Free
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_free_mesh_null(void)
{
    TEST("free_mesh_null");
    /* Should not crash when passed NULL */
    forge_pipeline_free_mesh(NULL);
    /* If we reach here, the test passes */
    END_TEST();
}

static void test_free_mesh_zeroes(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_free_zero.fmesh");
    TEST("free_mesh_zeroes");

    ASSERT_TRUE(write_test_fmesh(path, false, 1));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    /* Verify data exists before free */
    ASSERT_NOT_NULL(mesh.vertices);
    ASSERT_NOT_NULL(mesh.indices);
    ASSERT_NOT_NULL(mesh.lods);
    ASSERT_TRUE(mesh.vertex_count > 0);

    forge_pipeline_free_mesh(&mesh);

    /* After free, all fields should be zeroed */
    ASSERT_NULL(mesh.vertices);
    ASSERT_NULL(mesh.indices);
    ASSERT_NULL(mesh.lods);
    ASSERT_UINT_EQ(mesh.vertex_count, 0);
    ASSERT_UINT_EQ(mesh.vertex_stride, 0);
    ASSERT_UINT_EQ(mesh.lod_count, 0);
    ASSERT_UINT_EQ(mesh.flags, 0);

    cleanup_file(path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Helper Functions (tests 18-20)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_has_tangents(void)
{
    char path_tan[512];
    char path_no_tan[512];
    temp_path(path_tan, sizeof(path_tan), "test_pipeline_has_tan.fmesh");
    temp_path(path_no_tan, sizeof(path_no_tan), "test_pipeline_no_tan2.fmesh");
    TEST("has_tangents");

    /* With tangents */
    ASSERT_TRUE(write_test_fmesh(path_tan, true, 1));
    ForgePipelineMesh mesh_tan;
    ASSERT_TRUE(forge_pipeline_load_mesh(path_tan, &mesh_tan));
    ASSERT_TRUE(forge_pipeline_has_tangents(&mesh_tan));

    /* Without tangents */
    ASSERT_TRUE(write_test_fmesh(path_no_tan, false, 1));
    ForgePipelineMesh mesh_no_tan;
    ASSERT_TRUE(forge_pipeline_load_mesh(path_no_tan, &mesh_no_tan));
    ASSERT_TRUE(!forge_pipeline_has_tangents(&mesh_no_tan));

    /* NULL mesh */
    ASSERT_TRUE(!forge_pipeline_has_tangents(NULL));

    forge_pipeline_free_mesh(&mesh_tan);
    forge_pipeline_free_mesh(&mesh_no_tan);
    cleanup_file(path_tan);
    cleanup_file(path_no_tan);
    END_TEST();
}

static void test_lod_index_count_out_of_range(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_lod_oor.fmesh");
    TEST("lod_index_count_out_of_range");

    ASSERT_TRUE(write_test_fmesh(path, false, 2));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    /* Valid LOD indices return 3 */
    ASSERT_UINT_EQ(forge_pipeline_lod_index_count(&mesh, 0), 3);
    ASSERT_UINT_EQ(forge_pipeline_lod_index_count(&mesh, 1), 3);

    /* Out-of-range returns 0 */
    ASSERT_UINT_EQ(forge_pipeline_lod_index_count(&mesh, 2), 0);
    ASSERT_UINT_EQ(forge_pipeline_lod_index_count(&mesh, 99), 0);

    /* NULL mesh returns 0 */
    ASSERT_UINT_EQ(forge_pipeline_lod_index_count(NULL, 0), 0);

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

static void test_lod_indices_out_of_range(void)
{
    char path[512];
    temp_path(path, sizeof(path), "test_pipeline_lod_ptr_oor.fmesh");
    TEST("lod_indices_out_of_range");

    ASSERT_TRUE(write_test_fmesh(path, false, 2));

    ForgePipelineMesh mesh;
    ASSERT_TRUE(forge_pipeline_load_mesh(path, &mesh));

    /* Valid LOD indices return non-NULL */
    ASSERT_NOT_NULL(forge_pipeline_lod_indices(&mesh, 0));
    ASSERT_NOT_NULL(forge_pipeline_lod_indices(&mesh, 1));

    /* Out-of-range returns NULL */
    ASSERT_NULL(forge_pipeline_lod_indices(&mesh, 2));
    ASSERT_NULL(forge_pipeline_lod_indices(&mesh, 99));

    /* NULL mesh returns NULL */
    ASSERT_NULL(forge_pipeline_lod_indices(NULL, 0));

    forge_pipeline_free_mesh(&mesh);
    cleanup_file(path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Texture Loading Helpers
 *
 * Write a fake image file (just some bytes) and a .meta.json sidecar
 * so the texture loader can exercise its JSON parsing and
 * multi-mip file loading.
 * ══════════════════════════════════════════════════════════════════════════ */

/* Dimensions used by texture test fixtures */
#define TEX_TEST_BASE_WIDTH  64
#define TEX_TEST_BASE_HEIGHT 64
#define TEX_TEST_MIP1_WIDTH  32
#define TEX_TEST_MIP1_HEIGHT 32
#define TEX_TEST_MIP2_WIDTH  16
#define TEX_TEST_MIP2_HEIGHT 16

/* Size of fake image file data (arbitrary, just needs to be loadable) */
#define TEX_TEST_FAKE_SIZE   128

/* Write a fake image file — just TEX_TEST_FAKE_SIZE zero bytes.
 * The texture loader reads raw file bytes without decoding, so any
 * content works for testing the loader plumbing. */
static bool write_fake_image(const char *path)
{
    uint8_t buf[TEX_TEST_FAKE_SIZE];
    SDL_memset(buf, 0xAB, sizeof(buf));
    bool ok = SDL_SaveFile(path, buf, sizeof(buf));
    if (!ok) {
        SDL_Log("write_fake_image: SDL_SaveFile failed for '%s': %s",
                path, SDL_GetError());
    }
    return ok;
}

/* Write a .meta.json sidecar using the texture plugin's format:
 *   { "output_width": W, "output_height": H, "mip_levels": [...] }
 *
 * mip_count == 0 means omit the mip_levels array entirely. */
static bool write_meta_json(const char *path, uint32_t width, uint32_t height,
                            int mip_count)
{
    char buf[1024];
    int len = 0;

    if (mip_count <= 0) {
        /* No mip array — loader should treat the base file as mip 0 */
        len = SDL_snprintf(buf, sizeof(buf),
            "{\n"
            "  \"source\": \"test.png\",\n"
            "  \"output\": \"test.png\",\n"
            "  \"output_width\": %u,\n"
            "  \"output_height\": %u\n"
            "}\n",
            width, height);
    } else {
        /* Build mip_levels array */
        len = SDL_snprintf(buf, sizeof(buf),
            "{\n"
            "  \"source\": \"test.png\",\n"
            "  \"output\": \"test.png\",\n"
            "  \"output_width\": %u,\n"
            "  \"output_height\": %u,\n"
            "  \"mip_levels\": [\n",
            width, height);

        for (int i = 0; i < mip_count && len < (int)sizeof(buf) - 64; i++) {
            uint32_t mw = width >> i;
            uint32_t mh = height >> i;
            if (mw == 0) mw = 1;
            if (mh == 0) mh = 1;
            len += SDL_snprintf(buf + len, sizeof(buf) - (size_t)len,
                "    { \"level\": %d, \"width\": %u, \"height\": %u }%s\n",
                i, mw, mh, (i < mip_count - 1) ? "," : "");
        }
        len += SDL_snprintf(buf + len, sizeof(buf) - (size_t)len,
            "  ]\n}\n");
    }

    bool ok = SDL_SaveFile(path, buf, (size_t)len);
    if (!ok) {
        SDL_Log("write_meta_json: SDL_SaveFile failed for '%s': %s",
                path, SDL_GetError());
    }
    return ok;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Texture Loading — Valid Files
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_load_texture_single_mip(void)
{
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex.meta.json");
    TEST("load_texture_single_mip");

    /* Write a fake image and a meta.json with no mip array */
    ASSERT_TRUE(write_fake_image(img_path));
    ASSERT_TRUE(write_meta_json(meta_path,
                                TEX_TEST_BASE_WIDTH, TEX_TEST_BASE_HEIGHT, 0));

    ForgePipelineTexture tex;
    ASSERT_TRUE(forge_pipeline_load_texture(img_path, &tex));

    ASSERT_UINT_EQ(tex.width,  TEX_TEST_BASE_WIDTH);
    ASSERT_UINT_EQ(tex.height, TEX_TEST_BASE_HEIGHT);
    ASSERT_UINT_EQ(tex.mip_count, 1);
    ASSERT_NOT_NULL(tex.mips);
    ASSERT_NOT_NULL(tex.mips[0].data);
    ASSERT_UINT_EQ(tex.mips[0].size, TEX_TEST_FAKE_SIZE);
    ASSERT_UINT_EQ(tex.mips[0].width,  TEX_TEST_BASE_WIDTH);
    ASSERT_UINT_EQ(tex.mips[0].height, TEX_TEST_BASE_HEIGHT);

    forge_pipeline_free_texture(&tex);
    cleanup_file(img_path);
    cleanup_file(meta_path);
    END_TEST();
}

static void test_load_texture_with_mip_levels(void)
{
    char img_path[512];
    char meta_path[512];
    char mip1_path[512];
    char mip2_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_texm.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_texm.meta.json");
    temp_path(mip1_path, sizeof(mip1_path), "test_pipeline_texm_mip1.png");
    temp_path(mip2_path, sizeof(mip2_path), "test_pipeline_texm_mip2.png");
    TEST("load_texture_with_mip_levels");

    /* Write fake images for mip 0, 1, and 2 */
    ASSERT_TRUE(write_fake_image(img_path));
    ASSERT_TRUE(write_fake_image(mip1_path));
    ASSERT_TRUE(write_fake_image(mip2_path));
    ASSERT_TRUE(write_meta_json(meta_path,
                                TEX_TEST_BASE_WIDTH, TEX_TEST_BASE_HEIGHT, 3));

    ForgePipelineTexture tex;
    ASSERT_TRUE(forge_pipeline_load_texture(img_path, &tex));

    ASSERT_UINT_EQ(tex.width,  TEX_TEST_BASE_WIDTH);
    ASSERT_UINT_EQ(tex.height, TEX_TEST_BASE_HEIGHT);
    ASSERT_UINT_EQ(tex.mip_count, 3);
    ASSERT_NOT_NULL(tex.mips);

    /* All 3 mip levels should have data loaded */
    for (uint32_t i = 0; i < tex.mip_count; i++) {
        ASSERT_NOT_NULL(tex.mips[i].data);
        ASSERT_TRUE(tex.mips[i].size > 0);
    }

    /* Verify dimensions halve per level */
    ASSERT_UINT_EQ(tex.mips[0].width,  TEX_TEST_BASE_WIDTH);
    ASSERT_UINT_EQ(tex.mips[0].height, TEX_TEST_BASE_HEIGHT);
    ASSERT_UINT_EQ(tex.mips[1].width,  TEX_TEST_MIP1_WIDTH);
    ASSERT_UINT_EQ(tex.mips[1].height, TEX_TEST_MIP1_HEIGHT);
    ASSERT_UINT_EQ(tex.mips[2].width,  TEX_TEST_MIP2_WIDTH);
    ASSERT_UINT_EQ(tex.mips[2].height, TEX_TEST_MIP2_HEIGHT);

    forge_pipeline_free_texture(&tex);
    cleanup_file(img_path);
    cleanup_file(meta_path);
    cleanup_file(mip1_path);
    cleanup_file(mip2_path);
    END_TEST();
}

static void test_load_texture_format_default(void)
{
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_texfmt.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_texfmt.meta.json");
    TEST("load_texture_format_default");

    ASSERT_TRUE(write_fake_image(img_path));
    ASSERT_TRUE(write_meta_json(meta_path,
                                TEX_TEST_BASE_WIDTH, TEX_TEST_BASE_HEIGHT, 0));

    ForgePipelineTexture tex;
    ASSERT_TRUE(forge_pipeline_load_texture(img_path, &tex));

    /* Default format should be RGBA8 when not specified */
    ASSERT_INT_EQ(tex.format, FORGE_PIPELINE_TEX_RGBA8);

    forge_pipeline_free_texture(&tex);
    cleanup_file(img_path);
    cleanup_file(meta_path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Texture Loading — Error Cases
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_load_texture_null_path(void)
{
    TEST("load_texture_null_path");
    ForgePipelineTexture tex;
    ASSERT_TRUE(!forge_pipeline_load_texture(NULL, &tex));
    END_TEST();
}

static void test_load_texture_null_tex(void)
{
    char path[512];
    temp_path(path, sizeof(path), "whatever.png");
    TEST("load_texture_null_tex");
    ASSERT_TRUE(!forge_pipeline_load_texture(path, NULL));
    END_TEST();
}

static void test_load_texture_no_meta(void)
{
    char img_path[512];
    char meta_cleanup_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_nometa.png");
    temp_path(meta_cleanup_path, sizeof(meta_cleanup_path), "test_pipeline_tex_nometa.meta.json");
    TEST("load_texture_no_meta");

    /* Image exists but no .meta.json sidecar */
    ASSERT_TRUE(write_fake_image(img_path));

    /* Remove any leftover meta file */
    cleanup_file(meta_cleanup_path);

    ForgePipelineTexture tex;
    ASSERT_TRUE(!forge_pipeline_load_texture(img_path, &tex));

    cleanup_file(img_path);
    END_TEST();
}

static void test_load_texture_invalid_json(void)
{
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_badjson.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_badjson.meta.json");
    TEST("load_texture_invalid_json");

    ASSERT_TRUE(write_fake_image(img_path));

    /* Write invalid JSON */
    const char *bad_json = "{ this is not valid json !!!";
    ASSERT_TRUE(SDL_SaveFile(meta_path, bad_json, SDL_strlen(bad_json)));

    ForgePipelineTexture tex;
    ASSERT_TRUE(!forge_pipeline_load_texture(img_path, &tex));

    cleanup_file(img_path);
    cleanup_file(meta_path);
    END_TEST();
}

static void test_load_texture_missing_dimensions(void)
{
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_nodim.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_nodim.meta.json");
    TEST("load_texture_missing_dimensions");

    ASSERT_TRUE(write_fake_image(img_path));

    /* Write JSON missing width/height */
    const char *json = "{ \"source\": \"test.png\" }";
    ASSERT_TRUE(SDL_SaveFile(meta_path, json, SDL_strlen(json)));

    ForgePipelineTexture tex;
    ASSERT_TRUE(!forge_pipeline_load_texture(img_path, &tex));

    cleanup_file(img_path);
    cleanup_file(meta_path);
    END_TEST();
}

static void test_load_texture_missing_mip_file(void)
{
    char img_path[512];
    char meta_path[512];
    char mip1_cleanup_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_mipmiss.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_mipmiss.meta.json");
    temp_path(mip1_cleanup_path, sizeof(mip1_cleanup_path), "test_pipeline_tex_mipmiss_mip1.png");
    TEST("load_texture_missing_mip_file");

    /* Write mip 0 but not mip 1 */
    ASSERT_TRUE(write_fake_image(img_path));
    cleanup_file(mip1_cleanup_path);
    ASSERT_TRUE(write_meta_json(meta_path,
                                TEX_TEST_BASE_WIDTH, TEX_TEST_BASE_HEIGHT, 2));

    ForgePipelineTexture tex;
    ASSERT_TRUE(!forge_pipeline_load_texture(img_path, &tex));

    cleanup_file(img_path);
    cleanup_file(meta_path);
    END_TEST();
}

static void test_load_texture_zero_dimensions(void)
{
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_zerodim.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_zerodim.meta.json");
    TEST("load_texture_zero_dimensions");

    ASSERT_TRUE(write_fake_image(img_path));

    /* Write meta JSON with zero width */
    const char *json = "{ \"output_width\": 0, \"output_height\": 64 }";
    ASSERT_TRUE(SDL_SaveFile(meta_path, json, SDL_strlen(json)));

    ForgePipelineTexture tex;
    ASSERT_TRUE(!forge_pipeline_load_texture(img_path, &tex));

    cleanup_file(img_path);
    cleanup_file(meta_path);
    END_TEST();
}

static void test_load_texture_negative_dimensions(void)
{
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_tex_negdim.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_tex_negdim.meta.json");
    TEST("load_texture_negative_dimensions");

    ASSERT_TRUE(write_fake_image(img_path));

    /* Write meta JSON with negative height */
    const char *json = "{ \"output_width\": 64, \"output_height\": -1 }";
    ASSERT_TRUE(SDL_SaveFile(meta_path, json, SDL_strlen(json)));

    ForgePipelineTexture tex;
    ASSERT_TRUE(!forge_pipeline_load_texture(img_path, &tex));

    cleanup_file(img_path);
    cleanup_file(meta_path);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Texture Free
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_free_texture_null(void)
{
    TEST("free_texture_null");
    /* Should not crash when passed NULL */
    forge_pipeline_free_texture(NULL);
    END_TEST();
}

static void test_free_texture_zeroes(void)
{
    char img_path[512];
    char meta_path[512];
    temp_path(img_path, sizeof(img_path), "test_pipeline_texfree.png");
    temp_path(meta_path, sizeof(meta_path), "test_pipeline_texfree.meta.json");
    TEST("free_texture_zeroes");

    ASSERT_TRUE(write_fake_image(img_path));
    ASSERT_TRUE(write_meta_json(meta_path,
                                TEX_TEST_BASE_WIDTH, TEX_TEST_BASE_HEIGHT, 0));

    ForgePipelineTexture tex;
    ASSERT_TRUE(forge_pipeline_load_texture(img_path, &tex));
    ASSERT_NOT_NULL(tex.mips);

    forge_pipeline_free_texture(&tex);

    /* After free, all fields should be zeroed */
    ASSERT_NULL(tex.mips);
    ASSERT_UINT_EQ(tex.mip_count, 0);
    ASSERT_UINT_EQ(tex.width, 0);
    ASSERT_UINT_EQ(tex.height, 0);

    cleanup_file(img_path);
    cleanup_file(meta_path);
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

    init_temp_dir();
    SDL_Log("\n=== forge-gpu Pipeline Library Tests ===\n");
    SDL_Log("Temp directory: %s\n", temp_dir);

    /* ── Mesh loading: valid files (6 tests) ── */
    SDL_Log("Mesh loading — valid files:");
    test_load_mesh_no_tangents();
    test_load_mesh_with_tangents();
    test_load_mesh_multiple_lods();
    test_mesh_lod_accessor();
    test_mesh_lod_indices();
    test_mesh_vertex_data_integrity();

    /* ── Mesh loading: error cases (11 tests) ── */
    SDL_Log("\nMesh loading — error cases:");
    test_load_mesh_null_path();
    test_load_mesh_null_mesh();
    test_load_mesh_nonexistent();
    test_load_mesh_invalid_magic();
    test_load_mesh_invalid_version();
    test_load_mesh_invalid_stride();
    test_load_mesh_truncated_header();
    test_load_mesh_truncated_data();
    test_load_mesh_too_many_lods();
    test_load_mesh_stride_tan_without_flag();
    test_load_mesh_invalid_lod_offset();

    /* ── Mesh free (2 tests) ── */
    SDL_Log("\nMesh free:");
    test_free_mesh_null();
    test_free_mesh_zeroes();

    /* ── Helper functions (3 tests) ── */
    SDL_Log("\nHelper functions:");
    test_has_tangents();
    test_lod_index_count_out_of_range();
    test_lod_indices_out_of_range();

    /* ── Texture loading: valid files (3 tests) ── */
    SDL_Log("\nTexture loading — valid files:");
    test_load_texture_single_mip();
    test_load_texture_with_mip_levels();
    test_load_texture_format_default();

    /* ── Texture loading: error cases (8 tests) ── */
    SDL_Log("\nTexture loading — error cases:");
    test_load_texture_null_path();
    test_load_texture_null_tex();
    test_load_texture_no_meta();
    test_load_texture_invalid_json();
    test_load_texture_missing_dimensions();
    test_load_texture_missing_mip_file();
    test_load_texture_zero_dimensions();
    test_load_texture_negative_dimensions();

    /* ── Texture free (2 tests) ── */
    SDL_Log("\nTexture free:");
    test_free_texture_null();
    test_free_texture_zeroes();

    /* ── Summary ── */
    SDL_Log("\n=== Results: %d/%d passed, %d failed ===",
            pass_count, test_count, fail_count);

    SDL_Quit();
    return fail_count > 0 ? 1 : 0;
}
